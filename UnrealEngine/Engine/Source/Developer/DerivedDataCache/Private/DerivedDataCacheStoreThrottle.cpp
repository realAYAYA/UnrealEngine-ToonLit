// Copyright Epic Games, Inc. All Rights Reserved.

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataLegacyCacheStore.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include <atomic>

namespace UE::DerivedData
{

/**
 * A cache store that increases the latency and reduces the throughput of another cache store.
 * 1. Reproduce timings for a remote cache with a local cache, to reduce both network usage and measurement noise.
 * 2. Reproduce HDD latency and throughput even when data is stored on SSD.
 */
class FCacheStoreThrottle final : public ILegacyCacheStore
{
public:
	FCacheStoreThrottle(ILegacyCacheStore* InInnerCache, uint32 InLatencyMS, uint32 InMaxBytesPerSecond)
		: InnerCache(InInnerCache)
		, Latency(float(InLatencyMS) / 1000.0f)
		, MaxBytesPerSecond(InMaxBytesPerSecond)
	{
		check(InnerCache);
	}

	void Put(
		const TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final
	{
		struct FRecordSize
		{
			FCacheKey Key;
			uint64 Size;
		};
		TArray<FRecordSize, TInlineAllocator<1>> RecordSizes;
		RecordSizes.Reserve(Requests.Num());
		Algo::Transform(Requests, RecordSizes, [](const FCachePutRequest& Request) -> FRecordSize
		{
			return {Request.Record.GetKey(), Private::GetCacheRecordCompressedSize(Request.Record)};
		});

		InnerCache->Put(Requests, Owner,
			[this, RecordSizes = MoveTemp(RecordSizes), State = EnterThrottlingScope(), OnComplete = MoveTemp(OnComplete)](FCachePutResponse&& Response)
			{
				const FRecordSize* Size = Algo::FindBy(RecordSizes, Response.Key, &FRecordSize::Key);
				CloseThrottlingScope(State, FThrottlingState(this, Size ? Size->Size : 0));
				OnComplete(MoveTemp(Response));
			});
	}

	void Get(
		const TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final
	{
		InnerCache->Get(Requests, Owner,
			[this, State = EnterThrottlingScope(), OnComplete = MoveTemp(OnComplete)](FCacheGetResponse&& Response)
			{
				CloseThrottlingScope(State, FThrottlingState(this, Private::GetCacheRecordCompressedSize(Response.Record)));
				OnComplete(MoveTemp(Response));
			});
	}

	void PutValue(
		const TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final
	{
		struct FValueSize
		{
			FCacheKey Key;
			uint64 Size;
		};
		TArray<FValueSize, TInlineAllocator<1>> ValueSizes;
		ValueSizes.Reserve(Requests.Num());
		Algo::Transform(Requests, ValueSizes, [](const FCachePutValueRequest& Request) -> FValueSize
		{
			return {Request.Key, Request.Value.GetData().GetCompressedSize()};
		});

		InnerCache->PutValue(Requests, Owner,
			[this, ValueSizes = MoveTemp(ValueSizes), State = EnterThrottlingScope(), OnComplete = MoveTemp(OnComplete)](FCachePutValueResponse&& Response)
			{
				const FValueSize* Size = Algo::FindBy(ValueSizes, Response.Key, &FValueSize::Key);
				CloseThrottlingScope(State, FThrottlingState(this, Size ? Size->Size : 0));
				OnComplete(MoveTemp(Response));
			});
	}

	void GetValue(
		const TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final
	{
		InnerCache->GetValue(Requests, Owner,
			[this, State = EnterThrottlingScope(), OnComplete = MoveTemp(OnComplete)](FCacheGetValueResponse&& Response)
			{
				CloseThrottlingScope(State, FThrottlingState(this, Response.Value.GetData().GetCompressedSize()));
				OnComplete(MoveTemp(Response));
			});
	}

	void GetChunks(
		const TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final
	{
		InnerCache->GetChunks(Requests, Owner,
			[this, State = EnterThrottlingScope(), OnComplete = MoveTemp(OnComplete)](FCacheGetChunkResponse&& Response)
			{
				CloseThrottlingScope(State, FThrottlingState(this, Response.RawData.GetSize()));
				OnComplete(MoveTemp(Response));
			});

	}

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final
	{
		InnerCache->LegacyStats(OutNode);
	}

	bool LegacyDebugOptions(FBackendDebugOptions& Options) final
	{
		return InnerCache->LegacyDebugOptions(Options);
	}

private:
	struct FThrottlingState
	{
		double Time;
		uint64 TotalBytesTransferred;

		explicit FThrottlingState(FCacheStoreThrottle* ThrottleWrapper)
		: Time(FPlatformTime::Seconds())
		, TotalBytesTransferred(ThrottleWrapper->TotalBytesTransferred.load(std::memory_order_relaxed))
		{
		}

		explicit FThrottlingState(FCacheStoreThrottle* ThrottleWrapper, uint64 BytesTransferred)
		: Time(FPlatformTime::Seconds())
		, TotalBytesTransferred(ThrottleWrapper->TotalBytesTransferred.fetch_add(BytesTransferred, std::memory_order_relaxed) + BytesTransferred)
		{
		}
	};

	FThrottlingState EnterThrottlingScope()
	{
		if (Latency > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ThrottlingLatency);
			FPlatformProcess::Sleep(Latency);
		}
		return FThrottlingState(this);
	}

	void CloseThrottlingScope(FThrottlingState PreviousState, FThrottlingState CurrentState)
	{
		if (MaxBytesPerSecond)
		{
			// Take into account any other transfer that might have happened during that time from any other thread so we have a global limit
			const double ExpectedTime = double(CurrentState.TotalBytesTransferred - PreviousState.TotalBytesTransferred) / MaxBytesPerSecond;
			const double ActualTime = CurrentState.Time - PreviousState.Time;
			if (ExpectedTime > ActualTime)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ThrottlingBandwidth);
				FPlatformProcess::Sleep(float(ExpectedTime - ActualTime));
			}
		}
	}

	/** Backend to use for storage, my responsibilities are about throttling **/
	ILegacyCacheStore* InnerCache;
	float Latency;
	uint32 MaxBytesPerSecond;
	std::atomic<uint64> TotalBytesTransferred{0};
};

ILegacyCacheStore* CreateCacheStoreThrottle(ILegacyCacheStore* InnerCache, uint32 LatencyMS, uint32 MaxBytesPerSecond)
{
	return new FCacheStoreThrottle(InnerCache, LatencyMS, MaxBytesPerSecond);
}

} // UE::DerivedData
