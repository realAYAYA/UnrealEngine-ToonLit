// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataLegacyCacheStore.h"
#include "Experimental/ZenServerInterface.h"
#include "Experimental/ZenStatistics.h"

#if UE_WITH_ZEN

#include "Async/ManualResetEvent.h"
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
#include "HAL/Thread.h"
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

TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_Get, TEXT("ZenDDC Get"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_GetHit, TEXT("ZenDDC Get Hit"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_Put, TEXT("ZenDDC Put"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_PutHit, TEXT("ZenDDC Put Hit"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_BytesReceived, TEXT("ZenDDC Bytes Received"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_BytesSent, TEXT("ZenDDC Bytes Sent"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_CacheRecordRequestCountInFlight, TEXT("ZenDDC CacheRecord Request Count"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(ZenDDC_ChunkRequestCountInFlight, TEXT("ZenDDC Chunk Request Count"));

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

struct FZenCacheStoreParams
{
	FString Name;
	FString Host;
	FString Namespace;
	FString Sandbox;
	int32 MaxBatchPutKB = 1024;
	int32 RecordBatchSize = 8;
	int32 ChunksBatchSize = 8;
	float DeactivateAtMs = -1.0f;
	TOptional<bool> bLocal;
	TOptional<bool> bRemote;
	bool bFlush = false;
	bool bReadOnly = false;
	bool bBypassProxy = true;

	void Parse(const TCHAR* NodeName, const TCHAR* Config);
};

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
		const FZenCacheStoreParams& InParams,
		ICacheStoreOwner* Owner);

	FZenCacheStore(
		UE::Zen::FServiceSettings&& Settings,
		const FZenCacheStoreParams& InParams,
		ICacheStoreOwner* Owner);

	~FZenCacheStore() final;

	inline const FString& GetName() const { return NodeName; }

	/**
	 * Checks if cache service is usable (reachable and accessible).
	 * @return true if usable
	 */
	inline bool IsUsable() const { return bIsUsable; }

	/**
	 * Checks if cache service is on the local machine.
	 * @return true if it is local
	 */
	inline bool IsLocalConnection() const { return bIsLocalConnection; }

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
	void Initialize(const FZenCacheStoreParams& Params);

	bool IsServiceReady();

	static FCompositeBuffer SaveRpcPackage(const FCbPackage& Package);
	THttpUniquePtr<IHttpRequest> CreateRpcRequest();
	using FOnRpcComplete = TUniqueFunction<void(THttpUniquePtr<IHttpResponse>& HttpResponse, FCbPackage& Response)>;
	void EnqueueAsyncRpc(IRequestOwner& Owner, FCbObject RequestObject, FOnRpcComplete&& OnComplete);
	void EnqueueAsyncRpc(IRequestOwner& Owner, const FCbPackage& RequestPackage, FOnRpcComplete&& OnComplete);

	void ActivatePerformanceEvaluationThread();
	void ConditionalEvaluatePerformance();
	void ConditionalUpdateStorageSize();
	void UpdateStatus();

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

	enum class EHealth
	{
		Unknown,
		Ok,
		Error,
	};

	using FOnHealthComplete = TUniqueFunction<void(THttpUniquePtr<IHttpResponse>& HttpResponse, EHealth Health)>;
	class FHealthReceiver;
	class FAsyncHealthReceiver;

	class FCbPackageReceiver;
	class FAsyncCbPackageReceiver;

	FString NodeName;
	FString Namespace;
	UE::Zen::FScopeZenService ZenService;
	ICacheStoreOwner* StoreOwner = nullptr;
	ICacheStoreStats* StoreStats = nullptr;
	THttpUniquePtr<IHttpConnectionPool> ConnectionPool;
	FHttpRequestQueue RequestQueue;
	bool bIsUsable = false;
	bool bIsLocalConnection = false;
	bool bTryEvaluatePerformance = false;
	std::atomic<bool> bDeactivatedForPerformance = false;
	int32 MaxBatchPutKB = 1024;
	int32 CacheRecordBatchSize = 8;
	int32 CacheChunksBatchSize = 8;
	FBackendDebugOptions DebugOptions;
	TAnsiStringBuilder<256> RpcUri;
	std::atomic<int64> LastPerformanceEvaluationTicks;
	TOptional<FThread> PerformanceEvaluationThread;
	FManualResetEvent PerformanceEvaluationThreadShutdownEvent;
	std::atomic<int64> LastStorageSizeUpdateTicks;
	float DeactivateAtMs = -1.0f;
	ECacheStoreFlags OperationalFlags;
	FRequestOwner PerformanceEvaluationRequestOwner;
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

	virtual ~FPutOp()
	{
		FMonotonicTimeSpan AverageMainThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.MainThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageOtherThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.OtherThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageLatency = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.Latency.ToSeconds() / Requests.Num());

		for (TRequestWithStats<FCachePutRequest>& Request : Requests)
		{
			Request.Stats.MainThreadTime = AverageMainThreadTime;
			Request.Stats.OtherThreadTime = AverageOtherThreadTime;
			Request.Stats.Latency = AverageLatency;
		}
		CacheStore.ConditionalEvaluatePerformance();
		CacheStore.ConditionalUpdateStorageSize();
	}

	void IssueRequests()
	{
		FRequestTimer RequestTimer(Requests[0].Stats);

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
				FRequestTimer RequestTimer(Requests[0].Stats);
				// Latency can't be measured for Put operations because it is intertwined with upload time.
				Requests[0].Stats.Latency = FMonotonicTimeSpan::Infinity();

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
						UE_LOG(LogDerivedDataCache, Display,
							TEXT("%s: Invalid response received from PutCacheRecords RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if ((HttpResponse->GetErrorCode() != EHttpErrorCode::Canceled) && (HttpResponse->GetStatusCode() != 404))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Error response received from PutCacheRecords RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCachePutRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					if (HttpResponse->GetErrorCode() == EHttpErrorCode::Canceled)
					{
						OnCanceled(RequestWithStats);
					}
					else
					{
						OnMiss(RequestWithStats);
					}
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
		if (BatchSize > CacheStore.MaxBatchPutKB*1024)
		{
			BatchSize = RecordSize;
			return EBatchView::NewBatch;
		}
		return EBatchView::Continue;
	}

	void OnHit(const TRequestWithStats<FCachePutRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
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
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put failed for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Record.GetKey()), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	}

	void OnCanceled(const TRequestWithStats<FCachePutRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put failed with canceled request for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Record.GetKey()), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Canceled);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Canceled));
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
		FMonotonicTimeSpan AverageMainThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.MainThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageOtherThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.OtherThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageLatency = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.Latency.ToSeconds() / Requests.Num());

		for (TRequestWithStats<FCacheGetRequest>& Request : Requests)
		{
			Request.Stats.MainThreadTime = AverageMainThreadTime;
			Request.Stats.OtherThreadTime = AverageOtherThreadTime;
			Request.Stats.Latency = AverageLatency;
		}
		CacheStore.ConditionalEvaluatePerformance();
		CacheStore.ConditionalUpdateStorageSize();
	}

	void IssueRequests()
	{
		FRequestTimer RequestTimer(Requests[0].Stats);

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
				FRequestTimer RequestTimer(Requests[0].Stats);
				const FHttpResponseStats& ResponseStats = HttpResponse->GetStats();
				Requests[0].Stats.Latency = FMonotonicTimeSpan::FromSeconds(ResponseStats.StartTransferTime - ResponseStats.ConnectTime);

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
						UE_LOG(LogDerivedDataCache, Display,
							TEXT("%s: Invalid response received from GetCacheRecords RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if ((HttpResponse->GetErrorCode() != EHttpErrorCode::Canceled) && (HttpResponse->GetStatusCode() != 404))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Error response received from GetCacheRecords RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}
					
				for (const TRequestWithStats<FCacheGetRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					if (HttpResponse->GetErrorCode() == EHttpErrorCode::Canceled)
					{
						OnCanceled(RequestWithStats);
					}
					else
					{
						OnMiss(RequestWithStats);
					}
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

	void OnCanceled(const TRequestWithStats<FCacheGetRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with canceled request for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Canceled);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Canceled));
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

	virtual ~FPutValueOp()
	{
		FMonotonicTimeSpan AverageMainThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.MainThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageOtherThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.OtherThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageLatency = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.Latency.ToSeconds() / Requests.Num());

		for (TRequestWithStats<FCachePutValueRequest>& Request : Requests)
		{
			Request.Stats.MainThreadTime = AverageMainThreadTime;
			Request.Stats.OtherThreadTime = AverageOtherThreadTime;
			Request.Stats.Latency = AverageLatency;
		}
		CacheStore.ConditionalEvaluatePerformance();
		CacheStore.ConditionalUpdateStorageSize();
	}

	void IssueRequests()
	{
		FRequestTimer RequestTimer(Requests[0].Stats);

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
				FRequestTimer RequestTimer(Requests[0].Stats);
				// Latency can't be measured for Put operations because it is intertwined with upload time.
				Requests[0].Stats.Latency = FMonotonicTimeSpan::Infinity();

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
						UE_LOG(LogDerivedDataCache, Display,
							TEXT("%s: Invalid response received from PutCacheValues RPC: %d results expected, received %d, from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if ((HttpResponse->GetErrorCode() != EHttpErrorCode::Canceled) && (HttpResponse->GetStatusCode() != 404))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Error response received from PutCacheValues RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCachePutValueRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					if (HttpResponse->GetErrorCode() == EHttpErrorCode::Canceled)
					{
						OnCanceled(RequestWithStats);
					}
					else
					{
						OnMiss(RequestWithStats);
					}
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
		if (BatchSize > CacheStore.MaxBatchPutKB*1024)
		{
			BatchSize = ValueSize;
			return EBatchView::NewBatch;
		}
		return EBatchView::Continue;
	}

	void OnHit(const TRequestWithStats<FCachePutValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
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
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put failed for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Error));
	}

	void OnCanceled(const TRequestWithStats<FCachePutValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put failed with canceled request for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Canceled);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Canceled));
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
		FMonotonicTimeSpan AverageMainThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.MainThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageOtherThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.OtherThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageLatency = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.Latency.ToSeconds() / Requests.Num());

		for (TRequestWithStats<FCacheGetValueRequest>& Request : Requests)
		{
			Request.Stats.MainThreadTime = AverageMainThreadTime;
			Request.Stats.OtherThreadTime = AverageOtherThreadTime;
			Request.Stats.Latency = AverageLatency;
		}
		CacheStore.ConditionalEvaluatePerformance();
		CacheStore.ConditionalUpdateStorageSize();
	}

	void IssueRequests()
	{
		FRequestTimer RequestTimer(Requests[0].Stats);

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
				FRequestTimer RequestTimer(Requests[0].Stats);
				const FHttpResponseStats& ResponseStats = HttpResponse->GetStats();
				Requests[0].Stats.Latency = FMonotonicTimeSpan::FromSeconds(ResponseStats.StartTransferTime - ResponseStats.ConnectTime);

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
							const FCbAttachment* Attachment = EnumHasAnyFlags(RequestWithStats.Request.Policy, ECachePolicy::SkipData) ? nullptr : Response.FindAttachment(RawHash);
							if (Attachment)
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
						UE_LOG(LogDerivedDataCache, Display,
							TEXT("%s: Invalid response received from GetCacheValues RPC: %d results expected, received %d from %s"),
							*CacheStore.GetName(), Batch.Num(), RequestIndex, *WriteToString<256>(*HttpResponse));
					}
				}
				else if ((HttpResponse->GetErrorCode() != EHttpErrorCode::Canceled) && (HttpResponse->GetStatusCode() != 404))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Error response received from GetCacheValues RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					if (HttpResponse->GetErrorCode() == EHttpErrorCode::Canceled)
					{
						OnCanceled(RequestWithStats);
					}
					else
					{
						OnMiss(RequestWithStats);
					}
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

	void OnCanceled(const TRequestWithStats<FCacheGetValueRequest>& RequestWithStats)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with canceled request for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(RequestWithStats.Request.Key), *RequestWithStats.Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Canceled);
		OnComplete(RequestWithStats.Request.MakeResponse(EStatus::Canceled));
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
		FMonotonicTimeSpan AverageMainThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.MainThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageOtherThreadTime = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.OtherThreadTime.ToSeconds() / Requests.Num());
		FMonotonicTimeSpan AverageLatency = FMonotonicTimeSpan::FromSeconds(Requests[0].Stats.Latency.ToSeconds() / Requests.Num());

		for (TRequestWithStats<FCacheGetChunkRequest>& Request : Requests)
		{
			Request.Stats.MainThreadTime = AverageMainThreadTime;
			Request.Stats.OtherThreadTime = AverageOtherThreadTime;
			Request.Stats.Latency = AverageLatency;
		}
		CacheStore.ConditionalEvaluatePerformance();
		CacheStore.ConditionalUpdateStorageSize();
	}

	void IssueRequests()
	{
		FRequestTimer RequestTimer(Requests[0].Stats);

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
				uint32_t AcceptFlags = static_cast<uint32_t>(Zen::Http::RpcAcceptOptions::kAllowPartialCacheChunks);
				if (CacheStore.bIsLocalConnection)
				{
					AcceptFlags |= static_cast<uint32_t>(Zen::Http::RpcAcceptOptions::kAllowLocalReferences);
					BatchRequest.AddInteger(ANSITEXTVIEW("Pid"), FPlatformProcess::GetCurrentProcessId());
				}
				BatchRequest.AddInteger(ANSITEXTVIEW("AcceptFlags"), AcceptFlags);

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
				FRequestTimer RequestTimer(Requests[0].Stats);
				const FHttpResponseStats& ResponseStats = HttpResponse->GetStats();
				Requests[0].Stats.Latency = FMonotonicTimeSpan::FromSeconds(ResponseStats.StartTransferTime - ResponseStats.ConnectTime);

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
								FIoHash AttachmentHash = RawHash;
								const FCbAttachment* Attachment = nullptr;
								if (EnumHasAnyFlags(RequestWithStats.Request.Policy, ECachePolicy::SkipData))
								{
									FCbFieldView RawSizeField = ResultObject[ANSITEXTVIEW("RawSize")];
									uint64 TotalSize = RawSizeField.AsUInt64();
									Succeeded = !RawSizeField.HasError();
									if (Succeeded)
									{
										RawSize = FMath::Min(Request.RawSize, TotalSize - FMath::Min(Request.RawOffset, TotalSize));
									}
								}
								else
								{
									FCbFieldView FragmentOffsetField = ResultObject[ANSITEXTVIEW("FragmentOffset")];
									uint64 FragmentOffset = FragmentOffsetField.AsUInt64();

									FCbFieldView FragmentHashView = ResultObject[ANSITEXTVIEW("FragmentHash")];
									if (FragmentHashView.IsHash())
									{
										FIoHash FragmentHash = FragmentHashView.AsHash();
										AttachmentHash = FragmentHash;
									}

									if (Attachment = Response.FindAttachment(AttachmentHash); Attachment != nullptr)
									{
										if (FCompressedBuffer CompressedBuffer = Attachment->AsCompressedBinary(); CompressedBuffer)
										{
											RequestedBytes = FCompressedBufferReader(CompressedBuffer).Decompress(Request.RawOffset - FragmentOffset, Request.RawSize);
											RawSize = RequestedBytes.GetSize();
											Succeeded = true;
										}
									}
								}
							}
						}
						Succeeded ? OnHit(RequestWithStats, MoveTemp(RawHash), RawSize, MoveTemp(RequestedBytes)) : OnMiss(RequestWithStats);
					}
				}
				else if ((HttpResponse->GetErrorCode() != EHttpErrorCode::Canceled) && (HttpResponse->GetStatusCode() != 404))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Error response received from GetChunks RPC: from %s"),
						*CacheStore.GetName(), *WriteToString<256>(*HttpResponse));
				}

				for (const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats : Batch.RightChop(RequestIndex))
				{
					if (HttpResponse->GetErrorCode() == EHttpErrorCode::Canceled)
					{
						OnCanceled(RequestWithStats);
					}
					else
					{
						OnMiss(RequestWithStats);
					}
				}
			};
			CacheStore.EnqueueAsyncRpc(Owner, BatchRequest.Save().AsObject(), MoveTemp(OnRpcComplete));
		});
	}

private:
	void OnHit(const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats, FIoHash&& RawHash, uint64 RawSize, FSharedBuffer&& RequestedBytes)
	{
		const FCacheGetChunkRequest& Request = RequestWithStats.Request;
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);

		// This is a rough estimate of physical read size until Zen communicates stats with each response.
		RequestWithStats.Stats.LogicalReadSize += RawSize;
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
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing value '%s' for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<16>(Request.Id), *WriteToString<96>(Request.Key), *Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Error);
		OnComplete(Request.MakeResponse(EStatus::Error));
	};

	void OnCanceled(const TRequestWithStats<FCacheGetChunkRequest>& RequestWithStats)
	{
		const FCacheGetChunkRequest& Request = RequestWithStats.Request;
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with canceled request for '%s' from '%s'"),
			*CacheStore.GetName(), *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
		RequestWithStats.EndRequest(CacheStore, EStatus::Canceled);
		OnComplete(Request.MakeResponse(EStatus::Canceled));
	};

	FZenCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<TRequestWithStats<FCacheGetChunkRequest>, TInlineAllocator<1>> Requests;
	FOnCacheGetChunkComplete OnComplete;
};

class FZenCacheStore::FHealthReceiver final : public IHttpReceiver
{
public:
	FHealthReceiver(const FHealthReceiver&) = delete;
	FHealthReceiver& operator=(const FHealthReceiver&) = delete;

	explicit FHealthReceiver(EHealth& OutHealth, FString* OutResponseBody = nullptr, IHttpReceiver* InNext = nullptr)
		: Health(OutHealth)
		, ResponseBody(OutResponseBody)
		, Next(InNext)
	{
		Health = EHealth::Unknown;
	}

private:
	IHttpReceiver* OnCreate(IHttpResponse& Response) final
	{
		return &BodyReceiver;
	}

	IHttpReceiver* OnComplete(IHttpResponse& Response) final
	{
		FUtf8StringView ResponseStringView(reinterpret_cast<const UTF8CHAR*>(BodyArray.GetData()), IntCastChecked<int32>(BodyArray.Num()));
		if (ResponseStringView == UTF8TEXTVIEW("OK!"))
		{
			Health = EHealth::Ok;
		}
		else
		{
			Health = EHealth::Error;
		}

		if (ResponseBody)
		{
			*ResponseBody = *WriteToString<64>(ResponseStringView);
		}
		return Next;
	}

private:
	EHealth& Health;
	FString* ResponseBody;
	IHttpReceiver* Next;
	TArray64<uint8> BodyArray;
	FHttpByteArrayReceiver BodyReceiver{ BodyArray, this };
};

class FZenCacheStore::FAsyncHealthReceiver final : public FRequestBase, public IHttpReceiver
{
public:
	FAsyncHealthReceiver(const FAsyncHealthReceiver&) = delete;
	FAsyncHealthReceiver& operator=(const FAsyncHealthReceiver&) = delete;

	FAsyncHealthReceiver(
		THttpUniquePtr<IHttpRequest>&& InRequest,
		IRequestOwner* InOwner,
		Zen::FZenServiceInstance& InZenServiceInstance,
		FOnHealthComplete&& InOnHealthComplete)
		: Request(MoveTemp(InRequest))
		, Owner(InOwner)
		, ZenServiceInstance(InZenServiceInstance)
		, BaseReceiver(Health, nullptr, this)
		, OnHealthComplete(MoveTemp(InOnHealthComplete))
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
				new FAsyncHealthReceiver(MoveTemp(Self->Request), Self->Owner, Self->ZenServiceInstance, MoveTemp(Self->OnHealthComplete));
				return;
			}

			Self->Request.Reset();
			if (Self->OnHealthComplete)
			{
				// Launch a task for the completion function since it can execute arbitrary code.
				Self->Owner->LaunchTask(TEXT("ZenHealthComplete"), [Self = TRefCountPtr(Self)]
				{
					// Ensuring that the OnRpcComplete method is destroyed by the time we exit this method by moving it to a local scope variable
					FOnHealthComplete LocalOnComplete = MoveTemp(Self->OnHealthComplete);
					LocalOnComplete(Self->Response, Self->Health);
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
	EHealth Health;
	FHealthReceiver BaseReceiver;
	FOnHealthComplete OnHealthComplete;
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
					// Ensuring that the OnRpcComplete method is destroyed by the time we exit this method by moving it to a local scope variable
					FOnRpcComplete LocalOnComplete = MoveTemp(Self->OnRpcComplete);
					LocalOnComplete(Self->Response, Self->Package);
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
	const FZenCacheStoreParams& InParams,
	ICacheStoreOwner* InStoreOwner)
	: ZenService(*InParams.Host)
	, StoreOwner(InStoreOwner)
	, PerformanceEvaluationRequestOwner(EPriority::Low)
{
	Initialize(InParams);
}

FZenCacheStore::FZenCacheStore(
	UE::Zen::FServiceSettings&& InSettings,
	const FZenCacheStoreParams& InParams,
	ICacheStoreOwner* InStoreOwner)
	: ZenService(MoveTemp(InSettings))
	, StoreOwner(InStoreOwner)
	, PerformanceEvaluationRequestOwner(EPriority::Low)
{
	Initialize(InParams);
}

FZenCacheStore::~FZenCacheStore()
{
	PerformanceEvaluationRequestOwner.Cancel();
	if (PerformanceEvaluationThread.IsSet())
	{
		PerformanceEvaluationThreadShutdownEvent.Notify();
		PerformanceEvaluationThread->Join();
		PerformanceEvaluationThread.Reset();
		PerformanceEvaluationThreadShutdownEvent.Reset();
	}

	if (StoreStats)
	{
		StoreOwner->DestroyStats(StoreStats);
	}
}

void FZenCacheStore::Initialize(const FZenCacheStoreParams& Params)
{
	NodeName = Params.Name;
	LastPerformanceEvaluationTicks.store(FDateTime::UtcNow().GetTicks(), std::memory_order_relaxed);
	LastStorageSizeUpdateTicks.store(FDateTime::UtcNow().GetTicks(), std::memory_order_relaxed);
	Namespace = Params.Namespace;

	RpcUri << ZenService.GetInstance().GetURL() << ANSITEXTVIEW("/z$/$rpc");

	const uint32 MaxConnections = uint32(FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads(), 8, 64));
	constexpr uint32 RequestPoolSize = 128;
	constexpr uint32 RequestPoolOverflowSize = 128;

	FHttpConnectionPoolParams ConnectionPoolParams;
	ConnectionPoolParams.MaxConnections = MaxConnections;
	ConnectionPoolParams.MinConnections = MaxConnections;
	ConnectionPool = IHttpManager::Get().CreateConnectionPool(ConnectionPoolParams);

	bool bReady = false;
	{
		// Issue a synchronous health/ready request with a limited idle time
		FHttpClientParams ReadinessClientParams;
		ReadinessClientParams.Version = EHttpVersion::V2;
		ReadinessClientParams.MaxRequests = 1;
		ReadinessClientParams.MinRequests = 1;
		ReadinessClientParams.LowSpeedLimit = 1;
		ReadinessClientParams.LowSpeedTime = 5; // 5 second idle time limit for the initial readiness check
		ReadinessClientParams.bBypassProxy = Params.bBypassProxy;
		THttpUniquePtr<IHttpClient> ReadinessClient = ConnectionPool->CreateClient(ReadinessClientParams);
		THttpUniquePtr<IHttpRequest> ReadinessRequest = ReadinessClient->TryCreateRequest({});
		TAnsiStringBuilder<256> StatusUri;
		StatusUri << ZenService.GetInstance().GetURL() << ANSITEXTVIEW("/health/ready");
		ReadinessRequest->SetUri(StatusUri);
		ReadinessRequest->SetMethod(EHttpMethod::Get);
		ReadinessRequest->AddAcceptType(EHttpMediaType::Text);
		EHealth Health = EHealth::Unknown;
		FString ResponseString;
		FHealthReceiver HealthReceiver(Health, &ResponseString);
		THttpUniquePtr<IHttpResponse> ReadinessResponse;
		ReadinessRequest->Send(&HealthReceiver, ReadinessResponse);
		bReady = Health == EHealth::Ok; // -V547

		if (ReadinessResponse->GetErrorCode() == EHttpErrorCode::None &&
			(ReadinessResponse->GetStatusCode() >= 200 && ReadinessResponse->GetStatusCode() <= 299))
		{
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Using ZenServer HTTP service at %s with namespace %s status: %s."),
				*GetName(), ZenService.GetInstance().GetURL(), *Namespace, *ResponseString);
		}
		else
		{
			if (ZenService.GetInstance().IsServiceRunningLocally())
			{
				UE_LOG(LogDerivedDataCache, Warning,
					TEXT("%s: Unable to reach ZenServer HTTP service at %s with namespace %s. Status: %d . Response: %s"),
					*GetName(), ZenService.GetInstance().GetURL(), *Namespace, ReadinessResponse->GetStatusCode(),
					*ResponseString);
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("%s: Unable to reach ZenServer HTTP service at %s with namespace %s. Status: %d . Response: %s"),
					*GetName(), ZenService.GetInstance().GetURL(), *Namespace, ReadinessResponse->GetStatusCode(),
					*ResponseString);
			}
		}
	}

	FHttpClientParams ClientParams;
	ClientParams.MaxRequests = RequestPoolSize + RequestPoolOverflowSize;
	ClientParams.MinRequests = RequestPoolSize;
	ClientParams.LowSpeedLimit = 1;
	ClientParams.LowSpeedTime = 25;
	ClientParams.bBypassProxy = Params.bBypassProxy;
	RequestQueue = FHttpRequestQueue(*ConnectionPool, ClientParams);

	bIsLocalConnection = ZenService.GetInstance().IsServiceRunningLocally() || ZenService.GetInstance().GetServiceSettings().IsAutoLaunch();
	bIsUsable = true;


	if (StoreOwner)
	{
		// Default to locally launched service getting the Local cache store flag.  Can be overridden by explicit value in config.
		bool bLocal = Params.bLocal.Get(bIsLocalConnection);

		// Default to non-locally launched service getting the Remote cache store flag.  Can be overridden by explicit value in config.
		// In the future this could be extended to allow the Remote flag by default (even for locally launched instances) if they have upstreams configured.
		bool bRemote = Params.bRemote.Get(!bIsLocalConnection);

		DeactivateAtMs = Params.DeactivateAtMs;

		ECacheStoreFlags Flags = ECacheStoreFlags::Query;
		Flags |= Params.bReadOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Store;
		Flags |= bLocal ? ECacheStoreFlags::Local : ECacheStoreFlags::None;
		Flags |= bRemote ? ECacheStoreFlags::Remote : ECacheStoreFlags::None;

		OperationalFlags = Flags;

		if (!bReady)
		{
			Flags = OperationalFlags & ~(ECacheStoreFlags::Store | ECacheStoreFlags::Query);
			if (bIsLocalConnection)
			{
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("%s: Readiness check failed. "
						"It will be deactivated until responsiveness improves. "
						"If this is consistent, consider disabling this cache store through "
						"the use of the '-ddc=NoZenLocalFallback' or '-ddc=InstalledNoZenLocalFallback' "
						"commandline arguments."),
					*GetName());
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("%s: Readiness check failed. "
						"It will be deactivated until responsiveness improves. "
						"If this is consistent, consider disabling this cache store through "
						"environment variables or other configuration."),
					*GetName());
			}
			bDeactivatedForPerformance.store(true, std::memory_order_relaxed);
		}

		StoreOwner->Add(this, Flags);
		TStringBuilder<256> Path(InPlace, ZenService.GetInstance().GetPath(), TEXTVIEW(" ("), Namespace, TEXTVIEW(")"));
		StoreStats = StoreOwner->CreateStats(this, Flags, TEXT("Zen"), *Params.Name, Path);
		bTryEvaluatePerformance = !GIsBuildMachine && (StoreStats != nullptr) && (DeactivateAtMs > 0.0f);

		StoreStats->SetAttribute(TEXTVIEW("Namespace"), Namespace);

		if (!bReady)
		{
			UpdateStatus();
			ActivatePerformanceEvaluationThread();
		}
	}

	// Issue a request for stats as it will be fetched asynchronously and issuing now makes them available sooner for future callers.
	Zen::FZenCacheStats ZenStats;
	ZenService.GetInstance().GetCacheStats(ZenStats);

	MaxBatchPutKB = Params.MaxBatchPutKB;
	CacheRecordBatchSize = Params.RecordBatchSize;
	CacheChunksBatchSize = Params.ChunksBatchSize;
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

void FZenCacheStore::ActivatePerformanceEvaluationThread()
{
	if (!PerformanceEvaluationThread.IsSet())
	{
		PerformanceEvaluationThread.Emplace(TEXT("ZenCacheStore Performance Evaluation"), [this]
		{
			while (!PerformanceEvaluationThreadShutdownEvent.WaitFor(FMonotonicTimeSpan::FromSeconds(30.0)))
			{
				IRequestOwner& Owner(PerformanceEvaluationRequestOwner);
				FRequestBarrier Barrier(Owner);
				THttpUniquePtr<IHttpRequest> Request = RequestQueue.CreateRequest({});
				TAnsiStringBuilder<256> StatusUri;
				StatusUri << ZenService.GetInstance().GetURL() << ANSITEXTVIEW("/health/ready");
				Request->SetUri(StatusUri);
				Request->SetMethod(EHttpMethod::Get);
				Request->AddAcceptType(EHttpMediaType::Text);
				new FAsyncHealthReceiver(MoveTemp(Request), &Owner, ZenService.GetInstance(), [this, StartTime = FMonotonicTimePoint::Now()](THttpUniquePtr<IHttpResponse>& HttpResponse, EHealth Health)
					{
						if (Health != EHealth::Ok)
						{
							// Any non-ok health means we hold off any possibility of reactivating and we don't add the lagency to the stats
							// as the failure may be client-side and instantaneous which will create an artificially low latency measurement.
							return;
						}

						double LatencySec = (HttpResponse->GetStats().StartTransferTime - HttpResponse->GetStats().ConnectTime);
						StoreStats->AddLatency(StartTime, FMonotonicTimePoint::Now(), FMonotonicTimeSpan::FromSeconds(LatencySec));
						if (!bTryEvaluatePerformance || (StoreStats->GetAverageLatency() * 1000 <= DeactivateAtMs))
						{
							if (PerformanceEvaluationThread.IsSet())
							{
								PerformanceEvaluationThreadShutdownEvent.Notify();
							}

							if (bDeactivatedForPerformance.load(std::memory_order_relaxed))
							{
								StoreOwner->SetFlags(this, OperationalFlags);
								UE_LOG(LogDerivedDataCache, Display,
									TEXT("%s: Performance has improved and meets minimum performance criteria. "
										"It will be reactivated now."),
									*GetName());
								bDeactivatedForPerformance.store(false, std::memory_order_relaxed);
								UpdateStatus();
							}
						}
					});
			}
		});
	}
}

void FZenCacheStore::ConditionalUpdateStorageSize()
{
	if (!StoreStats)
	{
		return;
	}
		
	// Look for an opportunity to measure and evaluate if storage size is acceptable.
	int64 LocalStorageSizeUpdateTicks = LastStorageSizeUpdateTicks.load(std::memory_order_relaxed);
	FTimespan TimespanSinceLastStorageSizeUpdate = FDateTime::UtcNow() - FDateTime(LocalStorageSizeUpdateTicks);

	if (TimespanSinceLastStorageSizeUpdate < FTimespan::FromSeconds(30))
	{
		return;
	}

	if (!LastStorageSizeUpdateTicks.compare_exchange_strong(LocalStorageSizeUpdateTicks, FDateTime::UtcNow().GetTicks()))
	{
		return;
	}

	bool PhysicalSizeIsValid = false;
	double PhysicalSize = 0.0;
	Zen::FZenCacheStats ZenCacheStats;
	if (ZenService.GetInstance().GetCacheStats(ZenCacheStats))
	{
		PhysicalSize += ZenCacheStats.General.Size.Disk + static_cast<double>(ZenCacheStats.CID.Size.Total);
		PhysicalSizeIsValid = true;
	}
	Zen::FZenProjectStats ZenProjectStats;
	if (ZenService.GetInstance().GetProjectStats(ZenProjectStats))
	{
		PhysicalSize += ZenProjectStats.General.Size.Disk;
		PhysicalSizeIsValid = true;
	}
	if (PhysicalSizeIsValid)
	{
		StoreStats->SetTotalPhysicalSize(static_cast<uint64>(PhysicalSize));
	}
}

void FZenCacheStore::ConditionalEvaluatePerformance()
{
	if (!bTryEvaluatePerformance)
	{
		return;
	}

	// Look for an opportunity to measure and evaluate if performance is acceptable.
	int64 LocalLastPerfEvaluationTicks = LastPerformanceEvaluationTicks.load(std::memory_order_relaxed);
	FTimespan TimespanSinceLastPerfEval = FDateTime::UtcNow() - FDateTime(LocalLastPerfEvaluationTicks);

	if (TimespanSinceLastPerfEval < FTimespan::FromSeconds(30))
	{
		return;
	}

	if (!LastPerformanceEvaluationTicks.compare_exchange_strong(LocalLastPerfEvaluationTicks, FDateTime::UtcNow().GetTicks()))
	{
		return;
	}

	// We won the race and get to do the performance check

	if (PerformanceEvaluationThread.IsSet() && PerformanceEvaluationThreadShutdownEvent.IsNotified())
	{
		// Join and cleanup old thread before we consider whether we need to start a new one
		PerformanceEvaluationThread->Join();
		PerformanceEvaluationThread.Reset();
		PerformanceEvaluationThreadShutdownEvent.Reset();
	}

	if (StoreStats->GetAverageLatency() * 1000 > DeactivateAtMs)
	{
		if (!bDeactivatedForPerformance.load(std::memory_order_relaxed))
		{
			StoreOwner->SetFlags(this, OperationalFlags & ~(ECacheStoreFlags::Store | ECacheStoreFlags::Query));
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Performance does not meet minimum criteria. "
					"It will be deactivated until performance measurements improve. "
					"If this is consistent, consider disabling this cache store through "
					"environment variables or other configuration."),
				*GetName());
			bDeactivatedForPerformance.store(true, std::memory_order_relaxed);
			UpdateStatus();
		}

		ActivatePerformanceEvaluationThread();
	}
}

void FZenCacheStore::UpdateStatus()
{
	if (StoreStats)
	{
		if (bDeactivatedForPerformance.load(std::memory_order_relaxed))
		{
			StoreStats->SetStatus(ECacheStoreStatusCode::Warning, NSLOCTEXT("DerivedDataCache", "DeactivatedForPerformanceOrReadiness", "Deactivated for performance or readiness"));
		}
		else
		{
			StoreStats->SetStatus(ECacheStoreStatusCode::None, {});
		}
	}
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

void FZenCacheStoreParams::Parse(const TCHAR* NodeName, const TCHAR* Config)
{
	Name = NodeName;

	if (FString ServerId; FParse::Value(Config, TEXT("ServerID="), ServerId))
	{
		FString ServerEntry;
		const TCHAR* ServerSection = TEXT("StorageServers");
		if (GConfig->GetString(ServerSection, *ServerId, ServerEntry, GEngineIni))
		{
			Parse(NodeName, *ServerEntry);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Using ServerID=%s which was not found in [%s]"), NodeName, *ServerId, ServerSection);
		}
	}

	FParse::Value(Config, TEXT("Host="), Host);

	FString OverrideName;
	if (FParse::Value(Config, TEXT("EnvHostOverride="), OverrideName))
	{
		FString HostEnv = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!HostEnv.IsEmpty())
		{
			Host = HostEnv;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment override for Host %s=%s"), NodeName, *OverrideName, *Host);
		}
	}

	if (FParse::Value(Config, TEXT("CommandLineHostOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), Host))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for Host %s=%s"), NodeName, *OverrideName, *Host);
		}
	}

	FParse::Value(Config, TEXT("Namespace="), Namespace);
	FParse::Value(Config, TEXT("StructuredNamespace="), Namespace);

	FParse::Bool(Config, TEXT("ReadOnly="), bReadOnly);

	// Sandbox and flush configuration for use in Cold/Warm type use cases
	FParse::Value(Config, TEXT("Sandbox="), Sandbox);
	FParse::Bool(Config, TEXT("Flush="), bFlush);
	FParse::Bool(Config, TEXT("BypassProxy="), bBypassProxy);

	// Performance deactivation
	FParse::Value(Config, TEXT("DeactivateAt="), DeactivateAtMs);

	// Explicit local and remote configuration
	if (bool bExplicitLocal = false; FParse::Bool(Config, TEXT("Local="), bExplicitLocal))
	{
		bLocal = bExplicitLocal;
	}

	if (bool bExplicitRemote = false; FParse::Bool(Config, TEXT("Remote="), bExplicitRemote))
	{
		bRemote = bExplicitRemote;
	}

	// Request batch fracturing configuration
	FParse::Value(Config, TEXT("MaxBatchPutKB="), MaxBatchPutKB);
	FParse::Value(Config, TEXT("RecordBatchSize="), RecordBatchSize);
	FParse::Value(Config, TEXT("ChunksBatchSize="), ChunksBatchSize);
}

ILegacyCacheStore* CreateZenCacheStore(const TCHAR* NodeName, const TCHAR* Config, ICacheStoreOwner* Owner)
{
	FZenCacheStoreParams Params;
	Params.Parse(NodeName, Config);

	if (Params.Host == TEXTVIEW("None"))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Disabled because Host is set to 'None'"), NodeName);
		return nullptr;
	}

	if (Params.Namespace.IsEmpty())
	{
		Params.Namespace = FApp::GetProjectName();
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Missing required parameter 'Namespace', falling back to '%s'"), NodeName, *Params.Namespace);
	}

	bool bHasSandbox = !Params.Sandbox.IsEmpty();
	bool bUseLocalDataCachePathOverrides = !bHasSandbox;

	FString CachePathOverride;
	if (bUseLocalDataCachePathOverrides && UE::Zen::Private::IsLocalAutoLaunched(Params.Host) && UE::Zen::Private::GetLocalDataCachePathOverride(CachePathOverride))
	{
		if (CachePathOverride == TEXT("None"))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Disabled because path is set to 'None'"), NodeName);
			return nullptr;
		}
	}

	TUniquePtr<FZenCacheStore> Backend;

	if (bHasSandbox)
	{
		Zen::FServiceSettings DefaultServiceSettings;
		DefaultServiceSettings.ReadFromConfig();

		if (!DefaultServiceSettings.IsAutoLaunch())
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Attempting to use a sandbox when there is no default autolaunch configured to inherit settings from.  Cache will be disabled."), NodeName);
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
		AutoLaunchSettings.DataPath += Params.Sandbox;
		AutoLaunchSettings.bIsDefaultSharedRunContext = false;

		// The unique local instances will always limit process lifetime for now to avoid accumulating many of them
		AutoLaunchSettings.bLimitProcessLifetime = true;

		// Flush the cache if requested.
		uint32 MultiprocessId = UE::GetMultiprocessId();
		if (Params.bFlush && (MultiprocessId == 0))
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

		Backend = MakeUnique<FZenCacheStore>(MoveTemp(ServiceSettings), Params, Owner);
	}
	else
	{
		Backend = MakeUnique<FZenCacheStore>(Params, Owner);
	}

	if (!Backend->IsUsable())
	{
		if (Backend->IsLocalConnection())
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to contact the service (%s), will not use it."), NodeName, *Backend->GetName());
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Failed to contact the service (%s), will not use it."), NodeName, *Backend->GetName());
		}
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