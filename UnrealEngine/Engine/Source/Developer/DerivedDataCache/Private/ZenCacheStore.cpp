// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataLegacyCacheStore.h"
#include "Experimental/ZenServerInterface.h"

#if UE_WITH_ZEN

#include "Async/UniqueLock.h"
#include "BatchView.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataChunk.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "Experimental/ZenStatistics.h"
#include "HAL/FileManager.h"
#include "Http/HttpClient.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Optional.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Templates/Function.h"
#include "ZenBackendUtils.h"
#include "ZenSerialization.h"

TRACE_DECLARE_INT_COUNTER(ZenDDC_Get, TEXT("ZenDDC Get"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_GetHit, TEXT("ZenDDC Get Hit"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_Put, TEXT("ZenDDC Put"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_PutHit, TEXT("ZenDDC Put Hit"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_BytesReceived, TEXT("ZenDDC Bytes Received"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_BytesSent, TEXT("ZenDDC Bytes Sent"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_CacheRecordRequestCountInFlight, TEXT("ZenDDC CacheRecord Request Count"));
TRACE_DECLARE_INT_COUNTER(ZenDDC_ChunkRequestCountInFlight, TEXT("ZenDDC Chunk Request Count"));

namespace UE::DerivedData
{

template<typename T>
void ForEachBatch(const int32 BatchSize, const int32 TotalCount, T&& Fn)
{
	check(BatchSize > 0);

	if (TotalCount > 0)
	{
		const int32 BatchCount = FMath::DivideAndRoundUp(TotalCount, BatchSize);
		const int32 Last = TotalCount - 1;

		for (int32 BatchIndex = 0; BatchIndex < BatchCount; BatchIndex++)
		{
			const int32 BatchFirstIndex	= BatchIndex * BatchSize;
			const int32 BatchLastIndex	= FMath::Min(BatchFirstIndex + BatchSize - 1, Last);

			Fn(BatchFirstIndex, BatchLastIndex);
		}
	}
}

/**
 * Backend for a HTTP based caching service (Zen)
 */
class FZenCacheStore final : public ILegacyCacheStore
{
public:
	
	/**
	 * Creates the cache store client, checks health status and attempts to acquire an access token.
	 *
	 * @param ServiceUrl	Base url to the service including scheme.
	 * @param Namespace		Namespace to use.
	 */
	FZenCacheStore(
		const TCHAR* ServiceUrl,
		const TCHAR* Namespace,
		const TCHAR* Name,
		const TCHAR* Config,
		ICacheStoreOwner* Owner);

	FZenCacheStore(
		UE::Zen::FServiceSettings&& Settings,
		const TCHAR* Namespace,
		const TCHAR* Name,
		const TCHAR* Config,
		ICacheStoreOwner* Owner);

	~FZenCacheStore() final;

	inline FString GetName() const { return ZenService.GetInstance().GetURL(); }

	/**
	 * Checks if cache service is usable (reachable and accessible).
	 * @return true if usable
	 */
	inline bool IsUsable() const { return bIsUsable; }

	// ICacheStore

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete = FOnCachePutComplete()) final;

	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete = FOnCachePutValueComplete()) final;

	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete = FOnCacheGetValueComplete()) final;

	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	// ILegacyCacheStore

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final;
	bool LegacyDebugOptions(FBackendDebugOptions& Options) final;

	const Zen::FZenServiceInstance& GetServiceInstance() const { return ZenService.GetInstance(); }

private:
	void Initialize(const TCHAR* Namespace, const TCHAR* Name, const TCHAR* Config);

	bool IsServiceReady();

	static FCompositeBuffer SaveRpcPackage(const FCbPackage& Package);
	THttpUniquePtr<IHttpRequest> CreateRpcRequest();
	using FOnRpcComplete = TUniqueFunction<void(THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)>;
	void EnqueueAsyncRpc(IRequestOwner& Owner, FCbObject RequestObject, FOnRpcComplete&& OnComplete);
	void EnqueueAsyncRpc(IRequestOwner& Owner, const FCbPackage& RequestPackage, FOnRpcComplete&& OnComplete);

	template <typename T, typename... ArgTypes>
	static TRefCountPtr<T> MakeAsyncOp(ArgTypes&&... Args)
	{
		// TODO: This should in-place construct from a pre-allocated memory pool
		return TRefCountPtr<T>(new T(Forward<ArgTypes>(Args)...));
	}

private:
	template <typename RequestType>
	struct TRequestWithStats;

	template <typename OutContainerType, typename InContainerType, typename BucketAccessorType, typename RequestTypeAccessorType>
	static void StartRequests(
		OutContainerType& Out,
		const InContainerType& In,
		BucketAccessorType BucketAccessor,
		RequestTypeAccessorType TypeAccessor,
		const ERequestOp Op);

	class FPutOp;
	class FGetOp;
	class FPutValueOp;
	class FGetValueOp;
	class FGetChunksOp;

	class FCbPackageReceiver;
	class FAsyncCbPackageReceiver;

	FString Namespace;
	UE::Zen::FScopeZenService ZenService;
	ICacheStoreOwner* StoreOwner = nullptr;
	ICacheStoreStats* StoreStats = nullptr;
	THttpUniquePtr<IHttpConnectionPool> ConnectionPool;
	FHttpRequestQueue RequestQueue;
	bool bIsUsable = false;
	bool bIsLocalConnection = false;
	int32 BatchPutMaxBytes = 1024*1024;
	int32 CacheRecordBatchSize = 8;
	int32 CacheChunksBatchSize = 8;
	FBackendDebugOptions DebugOptions;
	TAnsiStringBuilder<256> RpcUri;
};

template <typename RequestType>
struct FZenCacheStore::TRequestWithStats
{
	RequestType Request;
	mutable FRequestStats Stats;

	explicit TRequestWithStats(const RequestType& InRequest)
		: Request(InRequest)
	{
	}

	void EndRequest(FZenCacheStore& Outer, const EStatus Status) const
	{
		{
			TUniqueLock Lock(Stats.Mutex);
			Stats.EndTime = FMonotonicTimePoint::Now();
			Stats.Status = Status;
		}
		if (Outer.StoreStats)
		{
			Outer.StoreStats->AddRequest(Stats);
		}
	}
};

template <typename OutContainerType, typename InContainerType, typename BucketAccessorType, typename RequestTypeAccessorType>
void FZenCacheStore::StartRequests(
	OutContainerType& Out,
	const InContainerType& In,
	BucketAccessorType BucketAccessor,
	RequestTypeAccessorType TypeAccessor,
	const ERequestOp Op)
{
	const FMonotonicTimePoint Now = FMonotonicTimePoint::Now();
	Out.Reserve(In.Num());
	for (const auto& Request : In)
	{
		auto& RequestWithStats = Out[Out.Emplace(Request)];
		RequestWithStats.Stats.Name = Request.Name;
		RequestWithStats.Stats.Bucket = BucketAccessor(Request);
		RequestWithStats.Stats.Type = TypeAccessor(Request);
		RequestWithStats.Stats.Op = Op;
		RequestWithStats.Stats.StartTime = Now;
	}
}

class FZenCacheStore::FPutOp final : public FThreadSafeRefCountedObject
{
public:
	FPutOp(FZenCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const TConstArrayView<FCachePutRequest> InRequests,
		FOnCachePutComplete&& InOnComplete)
		: CacheStore(InCacheStore)
		, Owner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
	{
		StartRequests(Requests, InRequests, [](const FCachePutRequest& Request) { return Request.Record.GetKey().Bucket; },
			[](auto&) { return ERequestType::Record; }, ERequestOp::Put);
		Batches = TBatchView<const TRequestWithStats<FCachePutRequest>>(Requests,
			[this](const TRequestWithStats<FCachePutRequest>& NextRequest) { return BatchGroupingFilter(NextRequest.Request); });
		TRACE_COUNTER_ADD(ZenDDC_Put, int64(Requests.Num()));
	}

	void IssueRequests()
	{
		FRequestBarrier Barrier(Owner);
		for (TArrayView<const TRequestWithStats<FCachePutRequest>> Batch : Batches)
		{
			FCbPackage BatchPackage;
			FCbWriter BatchWriter;
			BatchWriter.BeginObject();
			{
				BatchWriter << ANSITEXTVIEW("Method") << "PutCacheRecords";
				BatchWriter.AddInteger(ANSITEXTVIEW("Accept"), Zen::Http::kCbPkgMagic);

				BatchWriter.BeginObject(ANSITEXTVIEW("Params"));
				{
					ECachePolicy BatchDefaultPolicy = Batch[0].Request.Policy.GetRecordPolicy();
					BatchWriter << ANSITEXTVIEW("DefaultPolicy") << *WriteToString<128>(BatchDefaultPolicy);
					BatchWriter.AddString(ANSITEXTVIEW("Namespace"), CacheStore.Namespace);

					BatchWriter.BeginArray(ANSITEXTVIEW("Requests"));
					for (const TRequestWithStats<FCachePutRequest>& RequestWithStats : Batch)
					{
						const FCachePutRequest& Request = RequestWithStats.Request;
						const FCacheRecord& Record = Request.Record;

						BatchWriter.BeginObject();
						{
							BatchWriter.SetName(ANSITEXTVIEW("Record"));
							Record.Save(BatchPackage, BatchWriter);
							if ((!Request.Policy.IsUniform()) || (Request.Policy.GetRecordPolicy() != BatchDefaultPolicy))
							{
								BatchWriter << ANSITEXTVIEW("Policy") << Request.Policy;
							}
						}
						BatchWriter.EndObject();
					}
					BatchWriter.EndArray();
				}
				BatchWriter.EndObject();
			}
			BatchWriter.EndObject();
			BatchPackage.SetObject(BatchWriter.Save().AsObject());

			auto OnRpcComplete = [this, OpRef = TRefCountPtr<FPutOp>(this), Batch](THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)
			{
				int32 RequestIndex = 0;
				if (HttpResponse->GetErrorCode() == EHttpErrorCode::None && HttpResponse->GetStatusCode() >= 200 && HttpResponse->GetStatusCode() <= 299)
				{
					const FCbObject& ResponseObj = Response.GetObject();
					for (FCbField ResponseField : ResponseObj[ANSITEXTVIEW("Result")])
					{
						if (RequestIndex >= Batch.Num())
						{
							++RequestIndex;
							continue;
						}

						const TRequestWithStats<FCachePutRequest>& RequestWithStats = Batch[RequestIndex++];
						RequestWithStats.Stats.AddLatency(FMonotonicTimeSpan::FromSeconds(HttpResponse->GetStats().StartTransferTime));

						const FCacheKey& Key = RequestWithStats.Request.Record.GetKey();
						bool bPutSucceeded = ResponseField.AsBool();
						if (CacheStore.DebugOptions.ShouldSimulatePutMiss(Key))
						{
							UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
								*CacheStore.GetName(), *WriteToString<96>(Key), *RequestWithStats.Request.Name);
							bPutSucceeded = false;
						}
						bPutSucceeded ? OnHit(RequestWithStats) : OnMiss(RequestWithStats);
					}
					if (RequestIndex != Batch.Num())
					{
						UE_LOG(LogDerivedDataCache, Warning,
							TEXT("%s: Invalid response received from PutCacheRecords RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if (HttpResponse->GetStatusCode() != 404)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Error response received from PutCacheRecords RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCachePutRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					OnMiss(RequestWithStats);
				}
			};
			CacheStore.EnqueueAsyncRpc(Owner, BatchPackage, MoveTemp(OnRpcComplete));
		}
	}

private:
	EBatchView BatchGroupingFilter(const FCachePutRequest& NextRequest)
	{
		const FCacheRecord& Record = NextRequest.Record;
		uint64 RecordSize = sizeof(FCacheKey) + Record.GetMeta().GetSize();
		for (const FValueWithId& Value : Record.GetValues())
		{
			RecordSize += Value.GetData().GetCompressedSize();
		}
		BatchSize += RecordSize;
		if (BatchSize > CacheStore.BatchPutMaxBytes)
		{
			BatchSize = RecordSize;
			return EBatchView::NewBatch;
		}
		return EBatchView::Continue;
	}

	void OnHit(const TRequestWithStats<FCachePutRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache Put complete for %s from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Record.GetKey()), *RequestWithStats.Request.Name);

		if (const FCbObject& Meta = RequestWithStats.Request.Record.GetMeta())
		{
			RequestWithStats.Stats.PhysicalWriteSize += Meta.GetSize();
		}
		for (const FValueWithId& Value : RequestWithStats.Request.Record.GetValues())
		{
			RequestWithStats.Stats.AddLogicalWrite(Value);
			RequestWithStats.Stats.PhysicalWriteSize += Value.GetData().GetCompressedSize();
		}
		RequestWithStats.EndRequest(CacheStore, EStatus::Ok);

		TRACE_COUNTER_ADD(ZenDDC_BytesSent, int64(RequestWithStats.Stats.PhysicalWriteSize));
		TRACE_COUNTER_INCREMENT(ZenDDC_PutHit);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Ok));
	}

	void OnMiss(const TRequestWithStats<FCachePutRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache Put miss for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Record.GetKey()), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	}

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCachePutRequest>, TInlineAllocator<1>> Requests;
	uint64 BatchSize = 0;
	TBatchView<const TRequestWithStats<FCachePutRequest>> Batches;
	FOnCachePutComplete OnComplete;
};

class FZenCacheStore::FGetOp final : public FThreadSafeRefCountedObject
{
public:
	FGetOp(FZenCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const TConstArrayView<FCacheGetRequest> InRequests,
		FOnCacheGetComplete&& InOnComplete)
		: CacheStore(InCacheStore)
		, Owner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
	{
		StartRequests(Requests, InRequests, [](const FCacheGetRequest& Request) { return Request.Key.Bucket; },
			[](auto&) { return ERequestType::Record; }, ERequestOp::Get);
		TRACE_COUNTER_ADD(ZenDDC_Get, int64(Requests.Num()));
		TRACE_COUNTER_ADD(ZenDDC_CacheRecordRequestCountInFlight, int64(Requests.Num()));
	}

	virtual ~FGetOp()
	{
		TRACE_COUNTER_SUBTRACT(ZenDDC_CacheRecordRequestCountInFlight, int64(Requests.Num()));
	}

	void IssueRequests()
	{
		FRequestBarrier Barrier(Owner);
		ForEachBatch(CacheStore.CacheRecordBatchSize, Requests.Num(),
			[this](int32 BatchFirst, int32 BatchLast)
		{
			TConstArrayView<TRequestWithStats<FCacheGetRequest>> Batch(Requests.GetData() + BatchFirst, BatchLast - BatchFirst + 1);

			FCbWriter BatchRequest;
			BatchRequest.BeginObject();
			{
				BatchRequest << ANSITEXTVIEW("Method") << ANSITEXTVIEW("GetCacheRecords");
				BatchRequest.AddInteger(ANSITEXTVIEW("Accept"), Zen::Http::kCbPkgMagic);
				if (CacheStore.bIsLocalConnection)
				{
					BatchRequest.AddInteger(ANSITEXTVIEW("AcceptFlags"), static_cast<uint32_t>(Zen::Http::RpcAcceptOptions::kAllowLocalReferences));
					BatchRequest.AddInteger(ANSITEXTVIEW("Pid"), FPlatformProcess::GetCurrentProcessId());
				}

				BatchRequest.BeginObject(ANSITEXTVIEW("Params"));
				{
					ECachePolicy BatchDefaultPolicy = Batch[0].Request.Policy.GetRecordPolicy();
					BatchRequest << ANSITEXTVIEW("DefaultPolicy") << *WriteToString<128>(BatchDefaultPolicy);
					BatchRequest.AddString(ANSITEXTVIEW("Namespace"), CacheStore.Namespace);

					BatchRequest.BeginArray(ANSITEXTVIEW("Requests"));
					for (const TRequestWithStats<FCacheGetRequest>& RequestWithStats : Batch)
					{
						BatchRequest.BeginObject();
						{
							BatchRequest << ANSITEXTVIEW("Key") << RequestWithStats.Request.Key;

							if ((!RequestWithStats.Request.Policy.IsUniform()) || (RequestWithStats.Request.Policy.GetRecordPolicy() != BatchDefaultPolicy))
							{
								BatchRequest << ANSITEXTVIEW("Policy") << RequestWithStats.Request.Policy;
							}
						}
						BatchRequest.EndObject();
					}
					BatchRequest.EndArray();
				}
				BatchRequest.EndObject();
			}
			BatchRequest.EndObject();

			FGetOp* OriginalOp = this;
			auto OnRpcComplete = [this, OpRef = TRefCountPtr<FGetOp>(OriginalOp), Batch](THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)
			{
				int32 RequestIndex = 0;
				if (HttpResponse->GetErrorCode() == EHttpErrorCode::None && HttpResponse->GetStatusCode() >= 200 && HttpResponse->GetStatusCode() <= 299)
				{
					const FCbObject& ResponseObj = Response.GetObject();
						
					for (FCbField RecordField : ResponseObj[ANSITEXTVIEW("Result")])
					{
						if (RequestIndex >= Batch.Num())
						{
							++RequestIndex;
							continue;
						}

						const TRequestWithStats<FCacheGetRequest>& RequestWithStats = Batch[RequestIndex++];
						RequestWithStats.Stats.AddLatency(FMonotonicTimeSpan::FromSeconds(HttpResponse->GetStats().StartTransferTime));

						const FCacheKey& Key = RequestWithStats.Request.Key;
						FOptionalCacheRecord Record;

						if (CacheStore.DebugOptions.ShouldSimulateGetMiss(Key))
						{
							UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of '%s' from '%s'"),
								*CacheStore.GetName(), *WriteToString<96>(Key), *RequestWithStats.Request.Name);
						}
						else if (!RecordField.IsNull())
						{
							Record = FCacheRecord::Load(Response, RecordField.AsObject());
						}
						Record ? OnHit(RequestWithStats, MoveTemp(Record).Get()) : OnMiss(RequestWithStats);
					}
					if (RequestIndex != Batch.Num())
					{
						UE_LOG(LogDerivedDataCache, Warning,
							TEXT("%s: Invalid response received from GetCacheRecords RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if (HttpResponse->GetStatusCode() != 404)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Error response received from GetCacheRecords RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCacheGetRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					OnMiss(RequestWithStats);
				}
			};

			CacheStore.EnqueueAsyncRpc(Owner, BatchRequest.Save().AsObject(), MoveTemp(OnRpcComplete));
		});
	}

private:
	void OnHit(const TRequestWithStats<FCacheGetRequest>& RequestWithStats, FCacheRecord&& Record)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);

		bool bInComplete = false;
		if (const FCbObject& Meta = Record.GetMeta())
		{
			RequestWithStats.Stats.PhysicalReadSize += Meta.GetSize();
		}
		for (const FValueWithId& Value : Record.GetValues())
		{
			RequestWithStats.Stats.AddLogicalRead(Value);
			RequestWithStats.Stats.PhysicalReadSize += Value.GetData().GetCompressedSize();
			ECachePolicy ValuePolicy = RequestWithStats.Request.Policy.GetValuePolicy(Value.GetId());
			if (EnumHasAnyFlags(ValuePolicy, ECachePolicy::SkipData))
			{
				continue;
			}
			if (Value.HasData())
			{
				continue;
			}
			if (EnumHasAnyFlags(ValuePolicy, ECachePolicy::Query))
			{
				bInComplete = true;
		}
		}
		EStatus Status = bInComplete ? EStatus::Error : EStatus::Ok;
		RequestWithStats.EndRequest(CacheStore, Status);

		TRACE_COUNTER_INCREMENT(ZenDDC_GetHit);
		TRACE_COUNTER_ADD(ZenDDC_BytesReceived, int64(RequestWithStats.Stats.PhysicalReadSize));
		OnComplete({RequestWithStats.Request.Name, MoveTemp(Record), RequestWithStats.Request.UserData, Status});
	}

	void OnMiss(const TRequestWithStats<FCacheGetRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	}

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCacheGetRequest>, TInlineAllocator<1>> Requests;
	FOnCacheGetComplete OnComplete;
};

class FZenCacheStore::FPutValueOp final : public FThreadSafeRefCountedObject
{
public:
	FPutValueOp(FZenCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const TConstArrayView<FCachePutValueRequest> InRequests,
		FOnCachePutValueComplete&& InOnComplete)
		: CacheStore(InCacheStore)
		, Owner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
	{
		StartRequests(Requests, InRequests, [](const FCachePutValueRequest& Request) { return Request.Key.Bucket; },
			[](auto&) { return ERequestType::Value; }, ERequestOp::Put);
		TRACE_COUNTER_ADD(ZenDDC_Put, int64(Requests.Num()));
		Batches = TBatchView<const TRequestWithStats<FCachePutValueRequest>>(Requests,
			[this](const TRequestWithStats<FCachePutValueRequest>& NextRequest) { return BatchGroupingFilter(NextRequest.Request); });
	}

	void IssueRequests()
	{
		FRequestBarrier Barrier(Owner);
		for (TArrayView<const TRequestWithStats<FCachePutValueRequest>> Batch : Batches)
		{
			FCbPackage BatchPackage;
			FCbWriter BatchWriter;
			BatchWriter.BeginObject();
			{
				BatchWriter << ANSITEXTVIEW("Method") << ANSITEXTVIEW("PutCacheValues");
				BatchWriter.AddInteger(ANSITEXTVIEW("Accept"), Zen::Http::kCbPkgMagic);

				BatchWriter.BeginObject(ANSITEXTVIEW("Params"));
				{
					ECachePolicy BatchDefaultPolicy = Batch[0].Request.Policy;
					BatchWriter << ANSITEXTVIEW("DefaultPolicy") << *WriteToString<128>(BatchDefaultPolicy);
					BatchWriter.AddString(ANSITEXTVIEW("Namespace"), CacheStore.Namespace);

					BatchWriter.BeginArray("Requests");
					for (const TRequestWithStats<FCachePutValueRequest>& RequestWithStats : Batch)
					{
						BatchWriter.BeginObject();
						{
							BatchWriter << ANSITEXTVIEW("Key") << RequestWithStats.Request.Key;
							const FValue& Value = RequestWithStats.Request.Value;
							BatchWriter.AddBinaryAttachment("RawHash", Value.GetRawHash());
							if (Value.HasData())
							{
								BatchPackage.AddAttachment(FCbAttachment(Value.GetData()));
							}
							if (RequestWithStats.Request.Policy != BatchDefaultPolicy)
							{
								BatchWriter << ANSITEXTVIEW("Policy") << WriteToString<128>(RequestWithStats.Request.Policy);
							}
						}
						BatchWriter.EndObject();
					}
					BatchWriter.EndArray();
				}
				BatchWriter.EndObject();
			}
			BatchWriter.EndObject();
			BatchPackage.SetObject(BatchWriter.Save().AsObject());

			auto OnRpcComplete = [this, OpRef = TRefCountPtr<FPutValueOp>(this), Batch](THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)
			{
				int32 RequestIndex = 0;
				if (HttpResponse->GetErrorCode() == EHttpErrorCode::None && HttpResponse->GetStatusCode() >= 200 && HttpResponse->GetStatusCode() <= 299)
				{
					const FCbObject& ResponseObj = Response.GetObject();
					for (FCbField ResponseField : ResponseObj[ANSITEXTVIEW("Result")])
					{
						if (RequestIndex >= Batch.Num())
						{
							++RequestIndex;
							continue;
						}

						const TRequestWithStats<FCachePutValueRequest>& RequestWithStats = Batch[RequestIndex++];
						RequestWithStats.Stats.AddLatency(FMonotonicTimeSpan::FromSeconds(HttpResponse->GetStats().StartTransferTime));

						bool bPutSucceeded = ResponseField.AsBool();
						if (CacheStore.DebugOptions.ShouldSimulatePutMiss(RequestWithStats.Request.Key))
						{
							UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for PutValue of %s from '%s'"),
								*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
							bPutSucceeded = false;
						}
						bPutSucceeded ? OnHit(RequestWithStats) : OnMiss(RequestWithStats);
					}
					if (RequestIndex != Batch.Num())
					{
						UE_LOG(LogDerivedDataCache, Warning,
							TEXT("%s: Invalid response received from PutCacheValues RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if (HttpResponse->GetStatusCode() != 404)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Error response received from PutCacheValues RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCachePutValueRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					OnMiss(RequestWithStats);
				}
			};

			CacheStore.EnqueueAsyncRpc(Owner, BatchPackage, MoveTemp(OnRpcComplete));
		}
	}

private:
	EBatchView BatchGroupingFilter(const FCachePutValueRequest& NextRequest)
	{
		uint64 ValueSize = sizeof(FCacheKey) + NextRequest.Value.GetData().GetCompressedSize();
		BatchSize += ValueSize;
		if (BatchSize > CacheStore.BatchPutMaxBytes)
		{
			BatchSize = ValueSize;
			return EBatchView::NewBatch;
		}
		return EBatchView::Continue;
	}

	void OnHit(const TRequestWithStats<FCachePutValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache PutValue complete for %s from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);

		RequestWithStats.Stats.AddLogicalWrite(RequestWithStats.Request.Value);
		RequestWithStats.Stats.PhysicalWriteSize += RequestWithStats.Request.Value.GetData().GetCompressedSize();
		RequestWithStats.EndRequest(CacheStore, EStatus::Ok);

		TRACE_COUNTER_INCREMENT(ZenDDC_PutHit);
		TRACE_COUNTER_ADD(ZenDDC_BytesSent, RequestWithStats.Stats.PhysicalWriteSize);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Ok));
	}

	void OnMiss(const TRequestWithStats<FCachePutValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache PutValue miss for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	}

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCachePutValueRequest>, TInlineAllocator<1>> Requests;
	uint64 BatchSize = 0;
	TBatchView<const TRequestWithStats<FCachePutValueRequest>> Batches;
	FOnCachePutValueComplete OnComplete;
};

class FZenCacheStore::FGetValueOp final : public FThreadSafeRefCountedObject
{
public:
	FGetValueOp(FZenCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const TConstArrayView<FCacheGetValueRequest> InRequests,
		FOnCacheGetValueComplete&& InOnComplete)
		: CacheStore(InCacheStore)
		, Owner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
	{
		StartRequests(Requests, InRequests, [](const FCacheGetValueRequest& Request) { return Request.Key.Bucket; },
			[](auto&) { return ERequestType::Value; }, ERequestOp::Get);
		TRACE_COUNTER_ADD(ZenDDC_Get, (int64)Requests.Num());
		TRACE_COUNTER_ADD(ZenDDC_CacheRecordRequestCountInFlight, int64(Requests.Num()));
	}

	virtual ~FGetValueOp()
	{
		TRACE_COUNTER_SUBTRACT(ZenDDC_CacheRecordRequestCountInFlight, int64(Requests.Num()));
	}

	void IssueRequests()
	{
		FRequestBarrier Barrier(Owner);
		ForEachBatch(CacheStore.CacheRecordBatchSize, Requests.Num(),
			[this](int32 BatchFirst, int32 BatchLast)
		{
			TConstArrayView<TRequestWithStats<FCacheGetValueRequest>> Batch(Requests.GetData() + BatchFirst, BatchLast - BatchFirst + 1);

			FCbWriter BatchRequest;
			BatchRequest.BeginObject();
			{
				BatchRequest << ANSITEXTVIEW("Method") << ANSITEXTVIEW("GetCacheValues");
				BatchRequest.AddInteger(ANSITEXTVIEW("Accept"), Zen::Http::kCbPkgMagic);
				if (CacheStore.bIsLocalConnection)
				{
					BatchRequest.AddInteger(ANSITEXTVIEW("AcceptFlags"), static_cast<uint32_t>(Zen::Http::RpcAcceptOptions::kAllowLocalReferences));
					BatchRequest.AddInteger(ANSITEXTVIEW("Pid"), FPlatformProcess::GetCurrentProcessId());
				}

				BatchRequest.BeginObject(ANSITEXTVIEW("Params"));
				{
					ECachePolicy BatchDefaultPolicy = Batch[0].Request.Policy;
					BatchRequest << ANSITEXTVIEW("DefaultPolicy") << *WriteToString<128>(BatchDefaultPolicy);
					BatchRequest.AddString(ANSITEXTVIEW("Namespace"), CacheStore.Namespace);

					BatchRequest.BeginArray("Requests");
					for (const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats : Batch)
					{
						BatchRequest.BeginObject();
						{
							BatchRequest << ANSITEXTVIEW("Key") << RequestWithStats.Request.Key;
							if (RequestWithStats.Request.Policy != BatchDefaultPolicy)
							{
								BatchRequest << ANSITEXTVIEW("Policy") << WriteToString<128>(RequestWithStats.Request.Policy);
							}
						}
						BatchRequest.EndObject();
					}
					BatchRequest.EndArray();
				}
				BatchRequest.EndObject();
			}
			BatchRequest.EndObject();

			FGetValueOp* OriginalOp = this;
			auto OnRpcComplete = [this, OpRef = TRefCountPtr<FGetValueOp>(OriginalOp), Batch](THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)
			{
				int32 RequestIndex = 0;
				if (HttpResponse->GetErrorCode() == EHttpErrorCode::None && HttpResponse->GetStatusCode() >= 200 && HttpResponse->GetStatusCode() <= 299)
				{
					const FCbObject& ResponseObj = Response.GetObject();

					for (FCbFieldView ResultField : ResponseObj[ANSITEXTVIEW("Result")])
					{
						if (RequestIndex >= Batch.Num())
						{
							++RequestIndex;
							continue;
						}

						const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats = Batch[RequestIndex++];
						RequestWithStats.Stats.AddLatency(FMonotonicTimeSpan::FromSeconds(HttpResponse->GetStats().StartTransferTime));

						FCbObjectView ResultObj = ResultField.AsObjectView();
						TOptional<FValue> Value;
						if (CacheStore.DebugOptions.ShouldSimulateGetMiss(RequestWithStats.Request.Key))
						{
							UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for GetValue of '%s' from '%s'"),
								*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
						}
						else
						{
							FCbFieldView RawHashField = ResultObj["RawHash"];
							FIoHash RawHash = RawHashField.AsHash();
							if (const FCbAttachment* Attachment = Response.FindAttachment(RawHash))
							{
								Value.Emplace(Attachment->AsCompressedBinary());
							}
							else
							{
								FCbFieldView RawSizeField = ResultObj["RawSize"];
								uint64 RawSize = RawSizeField.AsUInt64();
								if (!RawSizeField.HasError() && !RawHashField.HasError())
								{
									Value.Emplace(RawHash, RawSize);
								}
							}
						}
						(bool)Value ? OnHit(RequestWithStats, MoveTemp(*Value)) : OnMiss(RequestWithStats);
					}
					if (RequestIndex != Batch.Num())
					{
						UE_LOG(LogDerivedDataCache, Warning,
							TEXT("%s: Invalid response received from GetCacheValues RPC: %d results expected, received %d from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if (HttpResponse->GetStatusCode() != 404)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Error response received from GetCacheValues RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					OnMiss(RequestWithStats);
				}
			};

			CacheStore.EnqueueAsyncRpc(Owner, BatchRequest.Save().AsObject(), MoveTemp(OnRpcComplete));
		});
	}

private:
	void OnHit(const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats, FValue&& Value)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);

		RequestWithStats.Stats.AddLogicalRead(Value);
		RequestWithStats.Stats.PhysicalReadSize += Value.GetData().GetCompressedSize();
		RequestWithStats.EndRequest(CacheStore, EStatus::Ok);

		TRACE_COUNTER_INCREMENT(ZenDDC_GetHit);
		TRACE_COUNTER_ADD(ZenDDC_BytesReceived, int64(RequestWithStats.Stats.PhysicalReadSize));
		OnComplete({RequestWithStats.Request.Name, RequestWithStats.Request.Key, MoveTemp(Value), RequestWithStats.Request.UserData, EStatus::Ok});
	};

	void OnMiss(const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	};

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCacheGetValueRequest>, TInlineAllocator<1>> Requests;
	FOnCacheGetValueComplete OnComplete;
};

class FZenCacheStore::FGetChunksOp final : public FThreadSafeRefCountedObject
{
public:
	FGetChunksOp(FZenCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const TConstArrayView<FCacheGetChunkRequest> InRequests,
		FOnCacheGetChunkComplete&& InOnComplete)
		: CacheStore(InCacheStore)
		, Owner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
	{
		StartRequests(Requests, InRequests, [](const FCacheGetChunkRequest& Request) { return Request.Key.Bucket; },
			[](const FCacheGetChunkRequest& Request) { return Request.Id.IsNull() ? ERequestType::Value : ERequestType::Record; },
			ERequestOp::GetChunk);
		Algo::StableSortBy(Requests, &TRequestWithStats<FCacheGetChunkRequest>::Request, TChunkLess());
		TRACE_COUNTER_ADD(ZenDDC_Get, int64(Requests.Num()));
		TRACE_COUNTER_ADD(ZenDDC_ChunkRequestCountInFlight, int64(Requests.Num()));
	}

	virtual ~FGetChunksOp()
	{
		TRACE_COUNTER_SUBTRACT(ZenDDC_ChunkRequestCountInFlight, int64(Requests.Num()));
	}

	void IssueRequests()
	{
		FRequestBarrier Barrier(Owner);
		ForEachBatch(CacheStore.CacheChunksBatchSize, Requests.Num(),
			[this](int32 BatchFirst, int32 BatchLast)
		{
			TConstArrayView<TRequestWithStats<FCacheGetChunkRequest>> Batch(Requests.GetData() + BatchFirst, BatchLast - BatchFirst + 1);

			FCbWriter BatchRequest;
			BatchRequest.BeginObject();
			{
				BatchRequest << ANSITEXTVIEW("Method") << "GetCacheChunks";
				BatchRequest.AddInteger(ANSITEXTVIEW("Accept"), Zen::Http::kCbPkgMagic);
				if (CacheStore.bIsLocalConnection)
				{
					BatchRequest.AddInteger(ANSITEXTVIEW("AcceptFlags"), static_cast<uint32_t>(Zen::Http::RpcAcceptOptions::kAllowLocalReferences));
					BatchRequest.AddInteger(ANSITEXTVIEW("Pid"), FPlatformProcess::GetCurrentProcessId());
				}

				BatchRequest.BeginObject(ANSITEXTVIEW("Params"));
				{
					ECachePolicy DefaultPolicy = Batch[0].Request.Policy;
					BatchRequest << ANSITEXTVIEW("DefaultPolicy") << WriteToString<128>(DefaultPolicy);
					BatchRequest.AddString(ANSITEXTVIEW("Namespace"), CacheStore.Namespace);

					BatchRequest.BeginArray(ANSITEXTVIEW("ChunkRequests"));
					for (const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats : Batch)
					{
						const FCacheGetChunkRequest& Request = RequestWithStats.Request;
						BatchRequest.BeginObject();
						{
							BatchRequest << ANSITEXTVIEW("Key") << Request.Key;

							if (Request.Id.IsValid())
							{
								BatchRequest.AddObjectId(ANSITEXTVIEW("ValueId"), Request.Id);
							}
							if (Request.RawOffset != 0)
							{
								BatchRequest << ANSITEXTVIEW("RawOffset") << Request.RawOffset;
							}
							if (Request.RawSize != MAX_uint64)
							{
								BatchRequest << ANSITEXTVIEW("RawSize") << Request.RawSize;
							}
							if (!Request.RawHash.IsZero())
							{
								BatchRequest << ANSITEXTVIEW("ChunkId") << Request.RawHash;
							}
							if (Request.Policy != DefaultPolicy)
							{
								BatchRequest << ANSITEXTVIEW("Policy") << WriteToString<128>(Request.Policy);
							}
						}
						BatchRequest.EndObject();
					}
					BatchRequest.EndArray();
				}
				BatchRequest.EndObject();
			}
			BatchRequest.EndObject();

			FGetChunksOp* OriginalOp = this;
			auto OnRpcComplete = [this, OpRef = TRefCountPtr<FGetChunksOp>(OriginalOp), Batch](THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)
			{
				int32 RequestIndex = 0;
				if (HttpResponse->GetErrorCode() == EHttpErrorCode::None && HttpResponse->GetStatusCode() >= 200 && HttpResponse->GetStatusCode() <= 299)
				{
					const FCbObject& ResponseObj = Response.GetObject();

					for (FCbFieldView ResultView : ResponseObj[ANSITEXTVIEW("Result")])
					{
						if (RequestIndex >= Batch.Num())
						{
							++RequestIndex;
							continue;
						}
						const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats = Batch[RequestIndex++];
						RequestWithStats.Stats.AddLatency(FMonotonicTimeSpan::FromSeconds(HttpResponse->GetStats().StartTransferTime));

						FIoHash RawHash;
						bool Succeeded = false;
						uint64 RawSize = 0;
						FCbObjectView ResultObject = ResultView.AsObjectView();
						FSharedBuffer RequestedBytes;
						const FCacheGetChunkRequest& Request = RequestWithStats.Request;
						if (CacheStore.DebugOptions.ShouldSimulateGetMiss(Request.Key))
						{
							UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of '%s' from '%s'"),
								*CacheStore.GetName(), *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
						}
						else
						{
							FCbFieldView HashView = ResultObject[ANSITEXTVIEW("RawHash")];
							RawHash = HashView.AsHash();
							if (!HashView.HasError())
							{
								if (const FCbAttachment* Attachment = Response.FindAttachment(HashView.AsHash()))
								{
									FCompressedBuffer CompressedBuffer = Attachment->AsCompressedBinary();
									if (CompressedBuffer)
									{
										TRACE_COUNTER_ADD(ZenDDC_BytesReceived, CompressedBuffer.GetCompressedSize());
										RequestedBytes = FCompressedBufferReader(CompressedBuffer).Decompress(Request.RawOffset, Request.RawSize);
										RawSize = RequestedBytes.GetSize();
										Succeeded = true;
									}
								}
								else
								{
									FCbFieldView RawSizeField = ResultObject[ANSITEXTVIEW("RawSize")];
									uint64 TotalSize = RawSizeField.AsUInt64();
									Succeeded = !RawSizeField.HasError();
									if (Succeeded)
									{
										RawSize = FMath::Min(Request.RawSize, TotalSize - FMath::Min(Request.RawOffset, TotalSize));
									}
								}
							}
						}
						Succeeded ? OnHit(RequestWithStats, MoveTemp(RawHash), RawSize, MoveTemp(RequestedBytes)) : OnMiss(RequestWithStats);
					}
				}
				else if (HttpResponse->GetStatusCode() != 404)
				{
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Error response received from GetChunks RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					OnMiss(RequestWithStats);
				}
			};
			CacheStore.EnqueueAsyncRpc(Owner, BatchRequest.Save().AsObject(), MoveTemp(OnRpcComplete));
		});
	}

private:
	void OnHit(const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats, FIoHash&& RawHash, uint64 RawSize, FSharedBuffer&& RequestedBytes)
	{
		const FCacheGetChunkRequest& Request = RequestWithStats.Request;
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: CacheChunk hit for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);

		// This is a rough estimate of physical read size until Zen communicates stats with each response.
		RequestWithStats.Stats.LogicalReadSize += RequestedBytes.GetSize();
		RequestWithStats.Stats.PhysicalReadSize += RequestedBytes.GetSize();
		RequestWithStats.EndRequest(CacheStore, EStatus::Ok);

		TRACE_COUNTER_INCREMENT(ZenDDC_GetHit);
		TRACE_COUNTER_ADD(ZenDDC_BytesReceived, int64(RequestWithStats.Stats.PhysicalReadSize));
		OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
			RawSize, MoveTemp(RawHash), MoveTemp(RequestedBytes), Request.UserData, EStatus::Ok});
	};

	void OnMiss(const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats)
	{
		const FCacheGetChunkRequest& Request = RequestWithStats.Request;
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: CacheChunk miss with missing value '%s' for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<16>(Request.Id), *WriteToString<96>(Request.Key), *Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(Request.MakeResponse(EStatus::Error));
	};

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCacheGetChunkRequest>, TInlineAllocator<1>> Requests;
	FOnCacheGetChunkComplete OnComplete;
};

class FZenCacheStore::FCbPackageReceiver final : public IHttpReceiver
{
public:
	FCbPackageReceiver(const FCbPackageReceiver&) = delete;
	FCbPackageReceiver& operator=(const FCbPackageReceiver&) = delete;

	explicit FCbPackageReceiver(FCbPackage& OutPackage, IHttpReceiver* InNext = nullptr)
		: Package(OutPackage)
		, Next(InNext)
	{
	}

private:
	IHttpReceiver* OnCreate(IHttpResponse& Response) final
	{
		return &BodyReceiver;
	}

	IHttpReceiver* OnComplete(IHttpResponse& Response) final
	{
		FMemoryView MemoryView = MakeMemoryView(BodyArray);
		{
			FMemoryReaderView Ar(MemoryView);
			if (Zen::Http::TryLoadCbPackage(Package, Ar))
			{
				return Next;
			}
		}
		FMemoryReaderView Ar(MemoryView);
		Package.TryLoad(Ar);
		return Next;
	}

private:
	FCbPackage& Package;
	IHttpReceiver* Next;
	TArray64<uint8> BodyArray;
	FHttpByteArrayReceiver BodyReceiver{BodyArray, this};
};

class FZenCacheStore::FAsyncCbPackageReceiver final : public FRequestBase, public IHttpReceiver
{
public:
	FAsyncCbPackageReceiver(const FAsyncCbPackageReceiver&) = delete;
	FAsyncCbPackageReceiver& operator=(const FAsyncCbPackageReceiver&) = delete;

	FAsyncCbPackageReceiver(
		THttpUniquePtr<IHttpRequest>&& InRequest,
		IRequestOwner* InOwner,
		Zen::FZenServiceInstance& InZenServiceInstance,
		FOnRpcComplete&& InOnRpcComplete)
		: Request(MoveTemp(InRequest))
		, Owner(InOwner)
		, ZenServiceInstance(InZenServiceInstance)
		, BaseReceiver(Package, this)
		, OnRpcComplete(MoveTemp(InOnRpcComplete))
	{
		Request->SendAsync(this, Response);
	}

private:
	// IRequest Interface

	void SetPriority(EPriority Priority) final {}
	void Cancel() final { Monitor->Cancel(); }
	void Wait() final { Monitor->Wait(); }

	// IHttpReceiver Interface

	IHttpReceiver* OnCreate(IHttpResponse& LocalResponse) final
	{
		Monitor = LocalResponse.GetMonitor();
		Owner->Begin(this);
		return &BaseReceiver;
	}

	bool ShouldRecoverAndRetry(IHttpResponse& LocalResponse)
	{
		if (!ZenServiceInstance.IsServiceRunningLocally())
		{
			return false;
		}

		if ((LocalResponse.GetErrorCode() == EHttpErrorCode::Connect) ||
			(LocalResponse.GetErrorCode() == EHttpErrorCode::TlsConnect) ||
			(LocalResponse.GetErrorCode() == EHttpErrorCode::TimedOut))
		{
			return true;
		}

		return false;
	}

	IHttpReceiver* OnComplete(IHttpResponse& LocalResponse) final
	{
		Owner->End(this, [Self = this, &LocalResponse]
		{
			if (Self->ShouldRecoverAndRetry(LocalResponse) && Self->ZenServiceInstance.TryRecovery())
			{
				new FAsyncCbPackageReceiver(MoveTemp(Self->Request), Self->Owner, Self->ZenServiceInstance, MoveTemp(Self->OnRpcComplete));
				return;
			}

			Self->Request.Reset();
			if (Self->OnRpcComplete)
			{
				// Launch a task for the completion function since it can execute arbitrary code.
				Self->Owner->LaunchTask(TEXT("ZenHttpComplete"), [Self = TRefCountPtr(Self)]
				{
					Self->OnRpcComplete(Self->Response, Self->Package);
				});
			}
		});
		return nullptr;
	}

private:
	THttpUniquePtr<IHttpRequest> Request;
	THttpUniquePtr<IHttpResponse> Response;
	TRefCountPtr<IHttpResponseMonitor> Monitor;
	IRequestOwner* Owner;
	Zen::FZenServiceInstance& ZenServiceInstance;
	FCbPackage Package;
	FCbPackageReceiver BaseReceiver;
	FOnRpcComplete OnRpcComplete;
};

FZenCacheStore::FZenCacheStore(
	const TCHAR* InServiceUrl,
	const TCHAR* InNamespace,
	const TCHAR* InName,
	const TCHAR* InConfig,
	ICacheStoreOwner* InStoreOwner)
	: ZenService(InServiceUrl)
	, StoreOwner(InStoreOwner)
{
	Initialize(InNamespace, InName, InConfig);
}

FZenCacheStore::FZenCacheStore(
	UE::Zen::FServiceSettings&& InSettings,
	const TCHAR* InNamespace,
	const TCHAR* InName,
	const TCHAR* InConfig,
	ICacheStoreOwner* InStoreOwner)
	: ZenService(MoveTemp(InSettings))
	, StoreOwner(InStoreOwner)
{
	Initialize(InNamespace, InName, InConfig);
}

FZenCacheStore::~FZenCacheStore()
{
	if (StoreStats)
	{
		StoreOwner->DestroyStats(StoreStats);
	}
}

void FZenCacheStore::Initialize(
	const TCHAR* InNamespace,
	const TCHAR* InName,
	const TCHAR* InConfig)
{
	Namespace = InNamespace;
	if (IsServiceReady())
	{
		RpcUri << ZenService.GetInstance().GetURL() << ANSITEXTVIEW("/z$/$rpc");

		const uint32 MaxConnections = uint32(FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads(), 8, 64));
		constexpr uint32 RequestPoolSize = 128;
		constexpr uint32 RequestPoolOverflowSize = 128;

		FHttpConnectionPoolParams ConnectionPoolParams;
		ConnectionPoolParams.MaxConnections = MaxConnections;
		ConnectionPoolParams.MinConnections = MaxConnections;
		ConnectionPool = IHttpManager::Get().CreateConnectionPool(ConnectionPoolParams);

		FHttpClientParams ClientParams;
		ClientParams.Version = EHttpVersion::V2;
		ClientParams.MaxRequests = RequestPoolSize + RequestPoolOverflowSize;
		ClientParams.MinRequests = RequestPoolSize;
		RequestQueue = FHttpRequestQueue(*ConnectionPool, ClientParams);

		bIsUsable = true;
		bIsLocalConnection = ZenService.GetInstance().IsServiceRunningLocally();

		if (StoreOwner)
		{
			bool bReadOnly = false;
			FParse::Bool(InConfig, TEXT("ReadOnly="), bReadOnly);

			// Default to locally launched service getting the Local cache store flag.  Can be overridden by explicit value in config.
			bool bLocal = bIsLocalConnection;
			FParse::Bool(InConfig, TEXT("Local="), bLocal);

			// Default to non-locally launched service getting the Remote cache store flag.  Can be overridden by explicit value in config.
			// In the future this could be extended to allow the Remote flag by default (even for locally launched instances) if they have upstreams configured.
			bool bRemote = !bIsLocalConnection;
			FParse::Bool(InConfig, TEXT("Remote="), bRemote);

			ECacheStoreFlags Flags = ECacheStoreFlags::Query;
			Flags |= bReadOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Store;
			Flags |= bLocal ? ECacheStoreFlags::Local : ECacheStoreFlags::None;
			Flags |= bRemote ? ECacheStoreFlags::Remote : ECacheStoreFlags::None;

			StoreOwner->Add(this, Flags);
			StoreStats = StoreOwner->CreateStats(this, Flags, TEXT("Zen"), InName, ZenService.GetInstance().GetURL());
		}

		// Issue a request for stats as it will be fetched asynchronously and issuing now makes them available sooner for future callers.
		Zen::FZenStats ZenStats;
		ZenService.GetInstance().GetStats(ZenStats);
	}

	GConfig->GetInt(TEXT("Zen"), TEXT("BatchPutMaxBytes"), BatchPutMaxBytes, GEngineIni);
	GConfig->GetInt(TEXT("Zen"), TEXT("CacheRecordBatchSize"), CacheRecordBatchSize, GEngineIni);
	GConfig->GetInt(TEXT("Zen"), TEXT("CacheChunksBatchSize"), CacheChunksBatchSize, GEngineIni);
}

bool FZenCacheStore::IsServiceReady()
{
	return ZenService.GetInstance().IsServiceReady();
}

FCompositeBuffer FZenCacheStore::SaveRpcPackage(const FCbPackage& Package)
{
	FLargeMemoryWriter Memory;
	Zen::Http::SaveCbPackage(Package, Memory);
	uint64 PackageMemorySize = Memory.TotalSize();
	return FCompositeBuffer(FSharedBuffer::TakeOwnership(Memory.ReleaseOwnership(), PackageMemorySize, FMemory::Free));
}

THttpUniquePtr<IHttpRequest> FZenCacheStore::CreateRpcRequest()
{
	THttpUniquePtr<IHttpRequest> Request = RequestQueue.CreateRequest({});
	Request->SetUri(RpcUri);
	Request->SetMethod(EHttpMethod::Post);
	Request->AddAcceptType(EHttpMediaType::CbPackage);
	return Request;
}

void FZenCacheStore::EnqueueAsyncRpc(IRequestOwner& Owner, FCbObject RequestObject, FOnRpcComplete&& OnComplete)
{
	THttpUniquePtr<IHttpRequest> Request = CreateRpcRequest();
	Request->SetContentType(EHttpMediaType::CbObject);
	Request->SetBody(RequestObject.GetBuffer().MakeOwned());
	new FAsyncCbPackageReceiver(MoveTemp(Request), &Owner, ZenService.GetInstance(), MoveTemp(OnComplete));
}

void FZenCacheStore::EnqueueAsyncRpc(IRequestOwner& Owner, const FCbPackage& RequestPackage, FOnRpcComplete&& OnComplete)
{
	THttpUniquePtr<IHttpRequest> Request = CreateRpcRequest();
	Request->SetContentType(EHttpMediaType::CbPackage);
	Request->SetBody(SaveRpcPackage(RequestPackage));
	new FAsyncCbPackageReceiver(MoveTemp(Request), &Owner, ZenService.GetInstance(), MoveTemp(OnComplete));
}

void FZenCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	checkNoEntry();
}

bool FZenCacheStore::LegacyDebugOptions(FBackendDebugOptions& InOptions)
{
	DebugOptions = InOptions;
	return true;
}

void FZenCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenDDC::PutCachedRecord);
	TRefCountPtr<FPutOp> PutOp = MakeAsyncOp<FPutOp>(*this, Owner, Requests, MoveTemp(OnComplete));
	PutOp->IssueRequests();
}

void FZenCacheStore::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenDDC::GetCacheRecord);
	TRefCountPtr<FGetOp> GetOp = MakeAsyncOp<FGetOp>(*this, Owner, Requests, MoveTemp(OnComplete));
	GetOp->IssueRequests();
}

void FZenCacheStore::PutValue(
	TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenDDC::PutValue);
	TRefCountPtr<FPutValueOp> PutValueOp = MakeAsyncOp<FPutValueOp>(*this, Owner, Requests, MoveTemp(OnComplete));
	PutValueOp->IssueRequests();
}

void FZenCacheStore::GetValue(
	TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenDDC::GetValue);
	TRefCountPtr<FGetValueOp> GetValueOp = MakeAsyncOp<FGetValueOp>(*this, Owner, Requests, MoveTemp(OnComplete));
	GetValueOp->IssueRequests();
}

void FZenCacheStore::GetChunks(
	TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenDDC::GetChunks);
	TRefCountPtr<FGetChunksOp> GetChunksOp = MakeAsyncOp<FGetChunksOp>(*this, Owner, Requests, MoveTemp(OnComplete));
	GetChunksOp->IssueRequests();
}

ILegacyCacheStore* CreateZenCacheStore(const TCHAR* NodeName, const TCHAR* Config, ICacheStoreOwner* Owner)
{
	FString ServiceUrl;
	FParse::Value(Config, TEXT("Host="), ServiceUrl);

	FString OverrideName;
	if (FParse::Value(Config, TEXT("EnvHostOverride="), OverrideName))
	{
		FString ServiceUrlEnv = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!ServiceUrlEnv.IsEmpty())
		{
			ServiceUrl = ServiceUrlEnv;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment override for Host %s=%s"), NodeName, *OverrideName, *ServiceUrl);
		}
	}

	if (FParse::Value(Config, TEXT("CommandLineHostOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), ServiceUrl))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for Host %s=%s"), NodeName, *OverrideName, *ServiceUrl);
		}
	}

	if (ServiceUrl == TEXT("None"))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("Disabling %s data cache - host set to 'None'."), NodeName);
		return nullptr;
	}

	FString Namespace;
	if (!FParse::Value(Config, TEXT("StructuredNamespace="), Namespace) && !FParse::Value(Config, TEXT("Namespace="), Namespace))
	{
		Namespace = FApp::GetProjectName();
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Missing required parameter 'Namespace', falling back to '%s'"), NodeName, *Namespace);
	}

	FString Sandbox;
	FParse::Value(Config, TEXT("Sandbox="), Sandbox);
	bool bHasSandbox = !Sandbox.IsEmpty();
	bool bUseLocalDataCachePathOverrides = !bHasSandbox;

	FString CachePathOverride;
	if (bUseLocalDataCachePathOverrides && UE::Zen::Private::IsLocalAutoLaunched(ServiceUrl) && UE::Zen::Private::GetLocalDataCachePathOverride(CachePathOverride))
	{
		if (CachePathOverride == TEXT("None"))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Disabling %s data cache - path set to 'None'."), NodeName);
			return nullptr;
		}
	}

	TUniquePtr<FZenCacheStore> Backend;

	bool bFlush = false;
	FParse::Bool(Config, TEXT("Flush="), bFlush);

	if (bHasSandbox)
	{
		Zen::FServiceSettings DefaultServiceSettings;
		DefaultServiceSettings.ReadFromConfig();

		if (!DefaultServiceSettings.IsAutoLaunch())
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Attempting to use a sandbox when there is no default autolaunch configured to interhit settings from.  Cache will be disabled."), NodeName);
			return nullptr;
		}

		// Make a unique local instance (not the default local instance) of ZenServer
		Zen::FServiceSettings ServiceSettings;
		ServiceSettings.SettingsVariant.Emplace<Zen::FServiceAutoLaunchSettings>();
		Zen::FServiceAutoLaunchSettings& AutoLaunchSettings = ServiceSettings.SettingsVariant.Get<Zen::FServiceAutoLaunchSettings>();

		const Zen::FServiceAutoLaunchSettings& DefaultAutoLaunchSettings = DefaultServiceSettings.SettingsVariant.Get<Zen::FServiceAutoLaunchSettings>();
		AutoLaunchSettings = DefaultAutoLaunchSettings;
		// Default as one more than the default port to not collide.  Multiple sandboxes will share a desired port, but will get differing effective ports.
		AutoLaunchSettings.DesiredPort++;

		FPaths::NormalizeDirectoryName(AutoLaunchSettings.DataPath);
		AutoLaunchSettings.DataPath += TEXT("_");
		AutoLaunchSettings.DataPath += Sandbox;
		AutoLaunchSettings.bIsDefaultSharedRunContext = false;

		// The unique local instances will always limit process lifetime for now to avoid accumulating many of them
		AutoLaunchSettings.bLimitProcessLifetime = true;

		// Flush the cache if requested.
		uint32 MultiprocessId = 0;
		FParse::Value(FCommandLine::Get(), TEXT("-MultiprocessId="), MultiprocessId);
		if (bFlush && (MultiprocessId == 0))
		{
			bool bStopped = true;
			if (UE::Zen::IsLocalServiceRunning(*AutoLaunchSettings.DataPath))
			{
				bStopped = UE::Zen::StopLocalService(*AutoLaunchSettings.DataPath);
			}

			if (bStopped)
			{
				IFileManager::Get().DeleteDirectory(*(AutoLaunchSettings.DataPath / TEXT("")), /*bRequireExists*/ false, /*bTree*/ true);
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Zen DDC could not be flushed due to an existing instance not shutting down when requested."), NodeName);
			}
		}

		Backend = MakeUnique<FZenCacheStore>(MoveTemp(ServiceSettings), *Namespace, NodeName, Config, Owner);
	}
	else
	{
		Backend = MakeUnique<FZenCacheStore>(*ServiceUrl, *Namespace, NodeName, Config, Owner);
	}

	if (!Backend->IsUsable())
	{
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to contact the service (%s), will not use it."), NodeName, *Backend->GetName());
		Backend.Reset();
		return nullptr;
	}

	return Backend.Release();
}

} // namespace UE::DerivedData

#else

namespace UE::DerivedData
{

ILegacyCacheStore* CreateZenCacheStore(const TCHAR* NodeName, const TCHAR* Config, ICacheStoreOwner* Owner)
{
	UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Zen cache is not yet supported in the current build configuration."), NodeName);
	return nullptr;
}

} // UE::DerivedData

#endif // UE_WITH_ZEN