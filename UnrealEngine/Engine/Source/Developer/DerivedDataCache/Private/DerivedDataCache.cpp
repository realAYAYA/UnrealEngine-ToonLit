// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCache.h"
#include "DerivedDataCacheInterface.h"

#include "Algo/AllOf.h"
#include "Async/AsyncWork.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Map.h"
#include "DDCCleanup.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheMaintainer.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataPluginInterface.h"
#include "DerivedDataPrivate.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "Experimental/Async/LazyEvent.h"
#include "Features/IModularFeatures.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "ZenServerInterface.h"
#include <atomic>

DEFINE_STAT(STAT_DDC_NumGets);
DEFINE_STAT(STAT_DDC_NumPuts);
DEFINE_STAT(STAT_DDC_NumBuilds);
DEFINE_STAT(STAT_DDC_NumExist);
DEFINE_STAT(STAT_DDC_SyncGetTime);
DEFINE_STAT(STAT_DDC_ASyncWaitTime);
DEFINE_STAT(STAT_DDC_PutTime);
DEFINE_STAT(STAT_DDC_SyncBuildTime);
DEFINE_STAT(STAT_DDC_ExistTime);

//#define DDC_SCOPE_CYCLE_COUNTER(x) QUICK_SCOPE_CYCLE_COUNTER(STAT_ ## x)
#define DDC_SCOPE_CYCLE_COUNTER(x) TRACE_CPUPROFILER_EVENT_SCOPE(x);

#if ENABLE_COOK_STATS
#include "DerivedDataCacheUsageStats.h"
namespace UE::DerivedData::CookStats
{
	// Use to prevent potential divide by zero issues
	inline double SafeDivide(const int64 Numerator, const int64 Denominator)
	{
		return Denominator != 0 ? (double)Numerator / (double)Denominator : 0.0;
	}

	// AddCookStats cannot be a lambda because of false positives in static analysis.
	// See https://developercommunity.visualstudio.com/content/problem/576913/c6244-regression-in-new-lambda-processorpermissive.html
	static void AddCookStats(FCookStatsManager::AddStatFuncRef AddStat)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS;
		TSharedRef<FDerivedDataCacheStatsNode> RootNode = GetDerivedDataCacheRef().GatherUsageStats();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS;

		{
			const FString StatName(TEXT("DDC.Usage"));
			for (const auto& UsageStatPair : RootNode->ToLegacyUsageMap())
			{
				UsageStatPair.Value.LogStats(AddStat, StatName, UsageStatPair.Key);
			}
		}

		TArray<TSharedRef<const FDerivedDataCacheStatsNode>> Nodes;
		RootNode->ForEachDescendant([&Nodes](TSharedRef<const FDerivedDataCacheStatsNode> Node)
		{
			if (Node->Children.IsEmpty())
			{
				Nodes.Add(Node);
			}
		});

		// Now lets add some summary data to that applies some crazy knowledge of how we set up our DDC. The goal 
		// is to print out the global hit rate, and the hit rate of the local and shared DDC.
		// This is done by adding up the total get/miss calls the root node receives.
		// Then we find the FileSystem nodes that correspond to the local and shared cache using some hacky logic to detect a "network drive".
		// If the DDC graph ever contains more than one local or remote filesystem, this will only find one of them.
		{
			const TSharedRef<const FDerivedDataCacheStatsNode>* LocalNode = Nodes.FindByPredicate([](TSharedRef<const FDerivedDataCacheStatsNode> Node) { return Node->GetCacheType() == TEXT("File System") && Node->IsLocal(); });
			const TSharedRef<const FDerivedDataCacheStatsNode>* SharedNode = Nodes.FindByPredicate([](TSharedRef<const FDerivedDataCacheStatsNode> Node) { return Node->GetCacheType() == TEXT("File System") && !Node->IsLocal(); });
			const TSharedRef<const FDerivedDataCacheStatsNode>* CloudNode = Nodes.FindByPredicate([](TSharedRef<const FDerivedDataCacheStatsNode> Node) { return Node->GetCacheType() == TEXT("Unreal Cloud DDC"); });
			const TSharedRef<const FDerivedDataCacheStatsNode>* ZenLocalNode = Nodes.FindByPredicate([](TSharedRef<const FDerivedDataCacheStatsNode> Node) { return Node->GetCacheType() == TEXT("Zen") && Node->IsLocal(); });
			const TSharedRef<const FDerivedDataCacheStatsNode>* ZenRemoteNode = Nodes.FindByPredicate([](TSharedRef<const FDerivedDataCacheStatsNode> Node) { return (Node->GetCacheType() == TEXT("Zen") || Node->GetCacheType() == TEXT("Horde")) && !Node->IsLocal(); });

			const FDerivedDataCacheUsageStats& RootStats = RootNode->UsageStats.CreateConstIterator().Value();
			const int64 TotalGetHits = RootStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
			const int64 TotalGetMisses = RootStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
			const int64 TotalGets = TotalGetHits + TotalGetMisses;

			int64 LocalHits = 0;
			FDerivedDataCacheSpeedStats LocalSpeedStats;
			if (LocalNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*LocalNode)->UsageStats.CreateConstIterator().Value();
				LocalHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				LocalSpeedStats = (*LocalNode)->SpeedStats;
			}
			if (ZenLocalNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*ZenLocalNode)->UsageStats.CreateConstIterator().Value();
				LocalHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				LocalSpeedStats = (*ZenLocalNode)->SpeedStats;
			}
			int64 SharedHits = 0;
			FDerivedDataCacheSpeedStats SharedSpeedStats;
			if (SharedNode)
			{
				// The shared DDC is only queried if the local one misses (or there isn't one). So it's hit rate is technically 
				const FDerivedDataCacheUsageStats& UsageStats = (*SharedNode)->UsageStats.CreateConstIterator().Value();
				SharedHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				SharedSpeedStats = (*SharedNode)->SpeedStats;
			}
			if (ZenRemoteNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*ZenRemoteNode)->UsageStats.CreateConstIterator().Value();
				SharedHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				SharedSpeedStats = (*ZenRemoteNode)->SpeedStats;
			}
			int64 CloudHits = 0;
			FDerivedDataCacheSpeedStats CloudSpeedStats;
			if (CloudNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*CloudNode)->UsageStats.CreateConstIterator().Value();
				CloudHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				CloudSpeedStats = (*CloudNode)->SpeedStats;
			}

			const int64 TotalPutHits = RootStats.PutStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
			const int64 TotalPutMisses = RootStats.PutStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
			const int64 TotalPuts = TotalPutHits + TotalPutMisses;

			AddStat(TEXT("DDC.Summary"), FCookStatsManager::CreateKeyValueArray(
				TEXT("BackEnd"), FDerivedDataBackend::Get().GetGraphName(),
				TEXT("HasLocalCache"), LocalNode || ZenLocalNode,
				TEXT("HasSharedCache"), SharedNode || ZenRemoteNode,
				TEXT("HasCloudCache"), !!CloudNode,
				TEXT("HasZenCache"), ZenLocalNode || ZenRemoteNode,
				TEXT("TotalGetHits"), TotalGetHits,
				TEXT("TotalGets"), TotalGets,
				TEXT("TotalGetHitPct"), SafeDivide(TotalGetHits, TotalGets),
				TEXT("LocalGetHitPct"), SafeDivide(LocalHits, TotalGets),
				TEXT("SharedGetHitPct"), SafeDivide(SharedHits, TotalGets),
				TEXT("CloudGetHitPct"), SafeDivide(CloudHits, TotalGets),
				TEXT("OtherGetHitPct"), SafeDivide((TotalGetHits - LocalHits - SharedHits - CloudHits), TotalGets),
				TEXT("GetMissPct"), SafeDivide(TotalGetMisses, TotalGets),
				TEXT("TotalPutHits"), TotalPutHits,
				TEXT("TotalPuts"), TotalPuts,
				TEXT("TotalPutHitPct"), SafeDivide(TotalPutHits, TotalPuts),
				TEXT("PutMissPct"), SafeDivide(TotalPutMisses, TotalPuts),
				TEXT("LocalLatency"), LocalSpeedStats.LatencyMS,
				TEXT("LocalReadSpeed"), LocalSpeedStats.ReadSpeedMBs,
				TEXT("LocalWriteSpeed"), LocalSpeedStats.WriteSpeedMBs,
				TEXT("SharedLatency"), SharedSpeedStats.LatencyMS,
				TEXT("SharedReadSpeed"), SharedSpeedStats.ReadSpeedMBs,
				TEXT("SharedWriteSpeed"), SharedSpeedStats.WriteSpeedMBs,
				TEXT("CloudLatency"), CloudSpeedStats.LatencyMS,
				TEXT("CloudReadSpeed"), CloudSpeedStats.ReadSpeedMBs,
				TEXT("CloudWriteSpeed"), CloudSpeedStats.WriteSpeedMBs
			));
		}
	}

	FCookStatsManager::FAutoRegisterCallback RegisterCookStats(AddCookStats);
}
#endif

void GatherDerivedDataCacheResourceStats(TArray<FDerivedDataCacheResourceStat>& DDCResourceStats);
void GatherDerivedDataCacheSummaryStats(FDerivedDataCacheSummaryStats& DDCSummaryStats);

/** Whether we want to verify the DDC (pass in -VerifyDDC on the command line)*/
bool GVerifyDDC = false;

namespace UE::DerivedData
{

FCachePutResponse FCachePutRequest::MakeResponse(const EStatus Status) const
{
	return {Name, Record.GetKey(), UserData, Status};
}

FCacheGetResponse FCacheGetRequest::MakeResponse(const EStatus Status) const
{
	return {Name, FCacheRecordBuilder(Key).Build(), UserData, Status};
}

FCachePutValueResponse FCachePutValueRequest::MakeResponse(const EStatus Status) const
{
	return {Name, Key, UserData, Status};
}

FCacheGetValueResponse FCacheGetValueRequest::MakeResponse(const EStatus Status) const
{
	return {Name, Key, {}, UserData, Status};
}

FCacheGetChunkResponse FCacheGetChunkRequest::MakeResponse(const EStatus Status) const
{
	return {Name, Key, Id, RawOffset, 0, {}, {}, UserData, Status};
}

FCbWriter& operator<<(FCbWriter& Writer, const FCacheGetRequest& Request)
{
	Writer.BeginObject();
	if (!Request.Name.IsEmpty())
	{
		Writer << ANSITEXTVIEW("Name") << Request.Name;
	}
	Writer << ANSITEXTVIEW("Key") << Request.Key;
	if (!Request.Policy.IsDefault())
	{
		Writer << ANSITEXTVIEW("Policy") << Request.Policy;
	}
	if (Request.UserData != 0)
	{
		Writer << ANSITEXTVIEW("UserData") << Request.UserData;
	}
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FCacheGetRequest& Request)
{
	bool bOk = Field.IsObject();
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Name")], Request.Name);
	bOk &= LoadFromCompactBinary(Field[ANSITEXTVIEW("Key")], Request.Key);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Policy")], Request.Policy);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("UserData")], Request.UserData);
	return bOk;
}

FCbWriter& operator<<(FCbWriter& Writer, const FCacheGetValueRequest& Request)
{
	Writer.BeginObject();
	if (!Request.Name.IsEmpty())
	{
		Writer << ANSITEXTVIEW("Name") << MakeStringView(Request.Name);
	}
	Writer << ANSITEXTVIEW("Key") << Request.Key;
	if (Request.Policy != ECachePolicy::Default)
	{
		Writer << ANSITEXTVIEW("Policy") << Request.Policy;
	}
	if (Request.UserData != 0)
	{
		Writer << ANSITEXTVIEW("UserData") << Request.UserData;
	}
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FCacheGetValueRequest& Request)
{
	bool bOk = Field.IsObject();
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Name")], Request.Name);
	bOk &= LoadFromCompactBinary(Field[ANSITEXTVIEW("Key")], Request.Key);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Policy")], Request.Policy);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("UserData")], Request.UserData);
	return bOk;
}

FCbWriter& operator<<(FCbWriter& Writer, const FCacheGetChunkRequest& Request)
{
	Writer.BeginObject();
	if (!Request.Name.IsEmpty())
	{
		Writer << ANSITEXTVIEW("Name") << MakeStringView(Request.Name);
	}
	Writer << ANSITEXTVIEW("Key") << Request.Key;
	if (Request.Id.IsValid())
	{
		Writer << ANSITEXTVIEW("Id") << Request.Id;
	}
	if (Request.RawOffset != 0)
	{
		Writer << ANSITEXTVIEW("RawOffset") << Request.RawOffset;
	}
	if (Request.RawSize != MAX_uint64)
	{
		Writer << ANSITEXTVIEW("RawSize") << Request.RawSize;
	}
	if (!Request.RawHash.IsZero())
	{
		Writer << ANSITEXTVIEW("RawHash") << Request.RawHash;
	}
	if (Request.Policy != ECachePolicy::Default)
	{
		Writer << ANSITEXTVIEW("Policy") << Request.Policy;
	}
	if (Request.UserData)
	{
		Writer << ANSITEXTVIEW("UserData") << Request.UserData;
	}
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FCacheGetChunkRequest& OutRequest)
{
	bool bOk = Field.IsObject();
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Name")], OutRequest.Name);
	bOk &= LoadFromCompactBinary(Field[ANSITEXTVIEW("Key")], OutRequest.Key);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Id")], OutRequest.Id);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("RawOffset")], OutRequest.RawOffset, 0);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("RawSize")], OutRequest.RawSize, MAX_uint64);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("RawHash")], OutRequest.RawHash);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("Policy")], OutRequest.Policy);
	LoadFromCompactBinary(Field[ANSITEXTVIEW("UserData")], OutRequest.UserData);
	return bOk;
}

} // UE::DerivedData

namespace UE::DerivedData::Private
{

FQueuedThreadPool* GCacheThreadPool;

class FCacheThreadPoolTaskRequest final : public FRequestBase, private IQueuedWork
{
public:
	inline FCacheThreadPoolTaskRequest(IRequestOwner& InOwner, TUniqueFunction<void ()>&& InTaskBody)
		: Owner(InOwner)
		, TaskBody(MoveTemp(InTaskBody))
	{
		LLM_IF_ENABLED(MemTag = FLowLevelMemTracker::Get().GetActiveTagData(ELLMTracker::Default));
		Owner.Begin(this);
		DoneEvent.Reset();
		GCacheThreadPool->AddQueuedWork(this, ConvertToQueuedWorkPriority(Owner.GetPriority()));
	}

private:
	inline void Execute()
	{
		LLM_SCOPE(MemTag);
		FScopeCycleCounter Scope(GetStatId(), /*bAlways*/ true);
		Owner.End(this, [this]
		{
			TaskBody();
			DoneEvent.Trigger();
		});
		// DO NOT ACCESS ANY MEMBERS PAST THIS POINT!
	}

	// IRequest Interface

	inline void SetPriority(EPriority Priority) final
	{
		if (GCacheThreadPool->RetractQueuedWork(this))
		{
			GCacheThreadPool->AddQueuedWork(this, ConvertToQueuedWorkPriority(Priority));
		}
	}

	inline void Cancel() final
	{
		if (!DoneEvent.Wait(0))
		{
			if (GCacheThreadPool->RetractQueuedWork(this))
			{
				Abandon();
			}
			else
			{
				FScopeCycleCounter Scope(GetStatId());
				DoneEvent.Wait();
			}
		}
	}

	inline void Wait() final
	{
		if (!DoneEvent.Wait(0))
		{
			if (GCacheThreadPool->RetractQueuedWork(this))
			{
				DoThreadedWork();
			}
			else
			{
				FScopeCycleCounter Scope(GetStatId());
				DoneEvent.Wait();
			}
		}
	}

	// IQueuedWork Interface

	inline void DoThreadedWork() final { Execute(); }
	inline void Abandon() final { Execute(); }

	inline TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCacheThreadPoolTaskRequest, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	IRequestOwner& Owner;
	TUniqueFunction<void ()> TaskBody;
	FLazyEvent DoneEvent{EEventMode::ManualReset};
	LLM(const UE::LLMPrivate::FTagData* MemTag = nullptr);
};

void LaunchTaskInCacheThreadPool(IRequestOwner& Owner, TUniqueFunction<void ()>&& TaskBody)
{
	if (GCacheThreadPool)
	{
		new FCacheThreadPoolTaskRequest(Owner, MoveTemp(TaskBody));
	}
	else
	{
		TaskBody();
	}
}

/**
 * Implementation of the derived data cache
 * This API is fully threadsafe
**/
class FDerivedDataCache final
	: public FDerivedDataCacheInterface
	, public ICache
	, public ICacheStoreMaintainer
	, public IDDCCleanup
{

	/** 
	 * Async worker that checks the cache backend and if that fails, calls the deriver to build the data and then puts the results to the cache
	**/
	friend class FBuildAsyncWorker;
	class FBuildAsyncWorker : public FNonAbandonableTask
	{
	public:
		enum EWorkerState : uint32
		{
			WorkerStateNone			= 0,
			WorkerStateRunning		= 1 << 0,
			WorkerStateFinished		= 1 << 1,
			WorkerStateDestroyed	= 1 << 2,
		};

		/** 
		 * Constructor for async task 
		 * @param	InDataDeriver	plugin to produce cache key and in the event of a miss, return the data.
		 * @param	InCacheKey		Complete cache key for this data.
		**/
		FBuildAsyncWorker(FDerivedDataBackend* InBackend, FDerivedDataPluginInterface* InDataDeriver, const TCHAR* InCacheKey, FStringView InDebugContext, bool bInSynchronousForStats)
		: bSuccess(false)
		, bSynchronousForStats(bInSynchronousForStats)
		, bDataWasBuilt(false)
		, Backend(InBackend)
		, DataDeriver(InDataDeriver)
		, CacheKey(InCacheKey)
		, DebugContext(InDebugContext)
		{
		}

		virtual ~FBuildAsyncWorker()
		{
			// Record that the task is destroyed and check that it was not running or destroyed previously.
			{
				const uint32 PreviousState = WorkerState.fetch_or(WorkerStateDestroyed, std::memory_order_relaxed);
				checkf(!(PreviousState & WorkerStateRunning), TEXT("Destroying DDC worker that is still running! Key: %s"), *CacheKey);
				checkf(!(PreviousState & WorkerStateDestroyed), TEXT("Destroying DDC worker that has been destroyed previously! Key: %s"), *CacheKey);
			}
		}

		/** Async worker that checks the cache backend and if that fails, calls the deriver to build the data and then puts the results to the cache **/
		void DoWork()
		{
			// Record that the task is running and check that it was not running, finished, or destroyed previously.
			{
				const uint32 PreviousState = WorkerState.fetch_or(WorkerStateRunning, std::memory_order_relaxed);
				checkf(!(PreviousState & WorkerStateRunning), TEXT("Starting DDC worker that is already running! Key: %s"), *CacheKey);
				checkf(!(PreviousState & WorkerStateFinished), TEXT("Starting DDC worker that is already finished! Key: %s"), *CacheKey);
				checkf(!(PreviousState & WorkerStateDestroyed), TEXT("Starting DDC worker that has been destroyed! Key: %s"), *CacheKey);
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(DDC_DoWork);

			const int64 NumBeforeDDC = Data.Num();
			bool bGetResult;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DDC_Get);

				INC_DWORD_STAT(STAT_DDC_NumGets);
				STAT(double ThisTime = 0);
				{
					SCOPE_SECONDS_COUNTER(ThisTime);
					FLegacyCacheGetRequest LegacyRequest;
					LegacyRequest.Name = DebugContext;
					LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
					FRequestOwner BlockingOwner(EPriority::Blocking);
					Backend->GetRoot().LegacyGet({LegacyRequest}, BlockingOwner,
						[this, &bGetResult](FLegacyCacheGetResponse&& Response)
						{
							const uint64 RawSize = Response.Value.GetRawSize();
							bGetResult = Response.Status == EStatus::Ok && RawSize > 0 && RawSize < MAX_int64;
							if (bGetResult)
							{
								const FCompositeBuffer& RawData = Response.Value.GetRawData();
								Data.Reset(int64(RawSize));
								for (const FSharedBuffer& Segment : RawData.GetSegments())
								{
									Data.Append(static_cast<const uint8*>(Segment.GetData()), int64(Segment.GetSize()));
								}
							}
						});
					BlockingOwner.Wait();
				}
				INC_FLOAT_STAT_BY(STAT_DDC_SyncGetTime, bSynchronousForStats ? (float)ThisTime : 0.0f);
			}
			if (bGetResult)
			{
				
				if (GVerifyDDC && DataDeriver && DataDeriver->IsDeterministic())
				{
					TArray<uint8> CmpData;
					DataDeriver->Build(CmpData);
					const int64 NumInDDC = Data.Num() - NumBeforeDDC;
					const int64 NumGenerated = CmpData.Num();
					
					bool bMatchesInSize = NumGenerated == NumInDDC;
					bool bDifferentMemory = true;
					int32 DifferentOffset = 0;
					if (bMatchesInSize)
					{
						bDifferentMemory = false;
						for (int32 i = 0; i < NumGenerated; i++)
						{
							if (CmpData[i] != Data[i])
							{
								bDifferentMemory = true;
								DifferentOffset = i;
								break;
							}
						}
					}

					if (!bMatchesInSize || bDifferentMemory)
					{
						FString ErrMsg = FString::Printf(TEXT("There is a mismatch between the DDC data and the generated data for plugin (%s) for asset (%s). BytesInDDC:%d, BytesGenerated:%d, bDifferentMemory:%d, offset:%d"), DataDeriver->GetPluginName(), *DataDeriver->GetDebugContextString(), NumInDDC, NumGenerated, bDifferentMemory, DifferentOffset);
						ensureMsgf(false, TEXT("%s"), *ErrMsg);
						UE_LOG(LogDerivedDataCache, Error, TEXT("%s"), *ErrMsg );
					}
				}

				check(Data.Num());
				bSuccess = true;
				delete DataDeriver;
				DataDeriver = NULL;
			}
			else if (DataDeriver)
			{
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(DDC_Build);

					INC_DWORD_STAT(STAT_DDC_NumBuilds);
					STAT(double ThisTime = 0);
					{
						SCOPE_SECONDS_COUNTER(ThisTime);
						TArray<uint8> Data32;
						bSuccess = DataDeriver->Build(Data32);
						Data = TArray64<uint8>(MoveTemp(Data32));
						bDataWasBuilt = true;
					}
					INC_FLOAT_STAT_BY(STAT_DDC_SyncBuildTime, bSynchronousForStats ? (float)ThisTime : 0.0f);
				}
				delete DataDeriver;
				DataDeriver = NULL;
				if (bSuccess)
				{
					check(Data.Num());

					TRACE_CPUPROFILER_EVENT_SCOPE(DDC_Put);

					INC_DWORD_STAT(STAT_DDC_NumPuts);
					STAT(double ThisTime = 0);
					{
						SCOPE_SECONDS_COUNTER(ThisTime);
						FLegacyCachePutRequest LegacyRequest;
						LegacyRequest.Name = DebugContext;
						LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
						LegacyRequest.Value = FLegacyCacheValue(FCompositeBuffer(FSharedBuffer::Clone(MakeMemoryView(Data))));
						FRequestOwner AsyncOwner(EPriority::Normal);
						Backend->GetRoot().LegacyPut({LegacyRequest}, AsyncOwner, [](auto&&){});
						AsyncOwner.KeepAlive();
					}
					INC_FLOAT_STAT_BY(STAT_DDC_PutTime, bSynchronousForStats ? (float)ThisTime : 0.0f);
				}
			}
			if (!bSuccess)
			{
				Data.Empty();
			}
			Backend->AddToAsyncCompletionCounter(-1);

			// Record that the task is finished and check that it was running and not finished or destroyed previously.
			{
				const uint32 PreviousState = WorkerState.fetch_xor(WorkerStateRunning | WorkerStateFinished, std::memory_order_relaxed);
				checkf((PreviousState & WorkerStateRunning), TEXT("Finishing DDC worker that was not running! Key: %s"), *CacheKey);
				checkf(!(PreviousState & WorkerStateFinished), TEXT("Finishing DDC worker that is already finished! Key: %s"), *CacheKey);
				checkf(!(PreviousState & WorkerStateDestroyed), TEXT("Finishing DDC worker that has been destroyed! Key: %s"), *CacheKey);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FBuildAsyncWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

		std::atomic<uint32>				WorkerState{WorkerStateNone};
		/** true in the case of a cache hit, otherwise the result of the deriver build call **/
		bool							bSuccess;
		/** true if we should record the timing **/
		bool							bSynchronousForStats;
		/** true if we had to build the data */
		bool							bDataWasBuilt;
		/** Backend graph to execute against. */
		FDerivedDataBackend*			Backend;
		/** Data dervier we are operating on **/
		FDerivedDataPluginInterface*	DataDeriver;
		/** Cache key associated with this build **/
		FString							CacheKey;
		/** Context from the caller */
		FSharedString					DebugContext;
		/** Data to return to caller, later **/
		TArray64<uint8>					Data;
	};

public:

	/** Constructor, called once to cereate a singleton **/
	FDerivedDataCache()
		: CurrentHandle(19248) // we will skip some potential handles to catch errors
	{
		if (FPlatformProcess::SupportsMultithreading())
		{
			GCacheThreadPool = FQueuedThreadPool::Allocate();
			const int32 ThreadCount = FPlatformMisc::NumberOfIOWorkerThreadsToSpawn();
#if WITH_EDITOR
			// Use normal priority to avoid preempting GT/RT/RHI and other more important threads with CPU processing (i.e. compression) happening on the IO Threads in editor.
			verify(GCacheThreadPool->Create(ThreadCount, 96 * 1024, TPri_Normal, TEXT("DDC IO ThreadPool")));
#else
			verify(GCacheThreadPool->Create(ThreadCount, 96 * 1024, TPri_AboveNormal, TEXT("DDC IO ThreadPool")));
#endif
		}

		Backend = FDerivedDataBackend::Create();

		CacheStoreMaintainers = IModularFeatures::Get().GetModularFeatureImplementations<ICacheStoreMaintainer>(FeatureName);

		GVerifyDDC = FParse::Param(FCommandLine::Get(), TEXT("VerifyDDC"));

		UE_CLOG(GVerifyDDC, LogDerivedDataCache, Display, TEXT("Items retrieved from the DDC will be verified (-VerifyDDC)"));
	}

	/** Destructor, flushes all sync tasks **/
	~FDerivedDataCache()
	{
		WaitForQuiescence(true);
		FScopeLock ScopeLock(&SynchronizationObject);
		for (TMap<uint32,FAsyncTask<FBuildAsyncWorker>*>::TIterator It(PendingTasks); It; ++It)
		{
			It.Value()->EnsureCompletion();
			delete It.Value();
		}
		PendingTasks.Empty();
		delete Backend;
	}

	virtual bool GetSynchronous(FDerivedDataPluginInterface* DataDeriver, TArray<uint8>& OutData, bool* bDataWasBuilt) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetSynchronous);
		check(DataDeriver);
		FString CacheKey = FDerivedDataCache::BuildCacheKey(DataDeriver);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("GetSynchronous %s from '%s'"), *CacheKey, *DataDeriver->GetDebugContextString());
		FAsyncTask<FBuildAsyncWorker> PendingTask(Backend, DataDeriver, *CacheKey, DataDeriver->GetDebugContextString(), true);
		AddToAsyncCompletionCounter(1);
		PendingTask.StartSynchronousTask();
		OutData = TArray<uint8>(MoveTemp(PendingTask.GetTask().Data));
		if (bDataWasBuilt)
		{
			*bDataWasBuilt = PendingTask.GetTask().bDataWasBuilt;
		}
		return PendingTask.GetTask().bSuccess;
	}

	virtual uint32 GetAsynchronous(FDerivedDataPluginInterface* DataDeriver) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetAsynchronous);
		FScopeLock ScopeLock(&SynchronizationObject);
		const uint32 Handle = NextHandle();
		FString CacheKey = FDerivedDataCache::BuildCacheKey(DataDeriver);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("GetAsynchronous %s from '%s', Handle %d"), *CacheKey, *DataDeriver->GetDebugContextString(), Handle);
		const bool bSync = !DataDeriver->IsBuildThreadsafe();
		FAsyncTask<FBuildAsyncWorker>* AsyncTask = new FAsyncTask<FBuildAsyncWorker>(Backend, DataDeriver, *CacheKey, DataDeriver->GetDebugContextString(), bSync);
		check(!PendingTasks.Contains(Handle));
		PendingTasks.Add(Handle,AsyncTask);
		AddToAsyncCompletionCounter(1);
		if (!bSync)
		{
			AsyncTask->StartBackgroundTask(DataDeriver->GetCustomThreadPool());
		}
		else
		{
			AsyncTask->StartSynchronousTask();
		}
		// Must return a valid handle
		check(Handle != 0);
		return Handle;
	}

	virtual bool PollAsynchronousCompletion(uint32 Handle) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_PollAsynchronousCompletion);
		FAsyncTask<FBuildAsyncWorker>* AsyncTask = NULL;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			AsyncTask = PendingTasks.FindRef(Handle);
		}
		check(AsyncTask);
		return AsyncTask->IsDone();
	}

	virtual void WaitAsynchronousCompletion(uint32 Handle) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_WaitAsynchronousCompletion);
		STAT(double ThisTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ThisTime);
			FAsyncTask<FBuildAsyncWorker>* AsyncTask = NULL;
			{
				FScopeLock ScopeLock(&SynchronizationObject);
				AsyncTask = PendingTasks.FindRef(Handle);
			}
			check(AsyncTask);
			AsyncTask->EnsureCompletion();
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("WaitAsynchronousCompletion, Handle %d"), Handle);
		}
		INC_FLOAT_STAT_BY(STAT_DDC_ASyncWaitTime,(float)ThisTime);
	}

	template <typename DataType>
	bool GetAsynchronousResultsByHandle(uint32 Handle, DataType& OutData, bool* bOutDataWasBuilt)
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetAsynchronousResults);
		FAsyncTask<FBuildAsyncWorker>* AsyncTask = NULL;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			PendingTasks.RemoveAndCopyValue(Handle,AsyncTask);
		}
		check(AsyncTask);
		const bool bDataWasBuilt = AsyncTask->GetTask().bDataWasBuilt;
		if (bOutDataWasBuilt)
		{
			*bOutDataWasBuilt = bDataWasBuilt;
		}
		if (!AsyncTask->GetTask().bSuccess)
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("GetAsynchronousResults, bDataWasBuilt: %d, Handle %d, FAILED"), (int32)bDataWasBuilt, Handle);
			delete AsyncTask;
			return false;
		}

		UE_LOG(LogDerivedDataCache, Verbose, TEXT("GetAsynchronousResults, bDataWasBuilt: %d, Handle %d, SUCCESS"), (int32)bDataWasBuilt, Handle);
		OutData = DataType(MoveTemp(AsyncTask->GetTask().Data));
		delete AsyncTask;
		check(OutData.Num());
		return true;
	}

	virtual bool GetAsynchronousResults(uint32 Handle, TArray<uint8>& OutData, bool* bOutDataWasBuilt) override
	{
		return GetAsynchronousResultsByHandle(Handle, OutData, bOutDataWasBuilt);
	}

	virtual bool GetAsynchronousResults(uint32 Handle, TArray64<uint8>& OutData, bool* bOutDataWasBuilt) override
	{
		return GetAsynchronousResultsByHandle(Handle, OutData, bOutDataWasBuilt);
	}

	template <typename DataType>
	bool GetSynchronousByKey(const TCHAR* CacheKey, DataType& OutData, FStringView DebugContext)
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetSynchronous_Data);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("GetSynchronous %s from '%.*s'"), CacheKey, DebugContext.Len(), DebugContext.GetData());
		FAsyncTask<FBuildAsyncWorker> PendingTask(Backend, nullptr, CacheKey, DebugContext, true);
		AddToAsyncCompletionCounter(1);
		PendingTask.StartSynchronousTask();
		OutData = DataType(MoveTemp(PendingTask.GetTask().Data));
		return PendingTask.GetTask().bSuccess;
	}

	virtual bool GetSynchronous(const TCHAR* CacheKey, TArray<uint8>& OutData, FStringView DebugContext) override
	{
		return GetSynchronousByKey(CacheKey, OutData, DebugContext);
	}

	virtual bool GetSynchronous(const TCHAR* CacheKey, TArray64<uint8>& OutData, FStringView DebugContext) override
	{
		return GetSynchronousByKey(CacheKey, OutData, DebugContext);
	}

	virtual uint32 GetAsynchronous(const TCHAR* CacheKey, FStringView DebugContext) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetAsynchronous_Handle);
		FScopeLock ScopeLock(&SynchronizationObject);
		const uint32 Handle = NextHandle();
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("GetAsynchronous %s from '%.*s', Handle %d"), CacheKey, DebugContext.Len(), DebugContext.GetData(), Handle);
		FAsyncTask<FBuildAsyncWorker>* AsyncTask = new FAsyncTask<FBuildAsyncWorker>(Backend, nullptr, CacheKey, DebugContext, false);
		check(!PendingTasks.Contains(Handle));
		PendingTasks.Add(Handle, AsyncTask);
		AddToAsyncCompletionCounter(1);
		// This request is I/O only, doesn't do any processing, send it to the I/O only thread-pool to avoid wasting worker threads on long I/O waits.
		AsyncTask->StartBackgroundTask(GCacheThreadPool);
		return Handle;
	}

	virtual void Put(const TCHAR* CacheKey, TArrayView64<const uint8> Data, FStringView DebugContext, bool bPutEvenIfExists = false) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_Put);
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("Put %s from '%.*s'"), CacheKey, DebugContext.Len(), DebugContext.GetData());
		STAT(double ThisTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ThisTime);
			FLegacyCachePutRequest LegacyRequest;
			LegacyRequest.Name = DebugContext;
			LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
			LegacyRequest.Value = FLegacyCacheValue(FCompositeBuffer(FSharedBuffer::Clone(MakeMemoryView(Data))));
			FRequestOwner AsyncOwner(EPriority::Normal);
			Backend->GetRoot().LegacyPut({LegacyRequest}, AsyncOwner, [](auto&&){});
			AsyncOwner.KeepAlive();
		}
		INC_FLOAT_STAT_BY(STAT_DDC_PutTime,(float)ThisTime);
		INC_DWORD_STAT(STAT_DDC_NumPuts);
	}

	virtual void MarkTransient(const TCHAR* CacheKey) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_MarkTransient);
		FLegacyCacheDeleteRequest LegacyRequest;
		LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
		LegacyRequest.Name = LegacyRequest.Key.GetFullKey();
		LegacyRequest.bTransient = true;
		FRequestOwner BlockingOwner(EPriority::Blocking);
		Backend->GetRoot().LegacyDelete({LegacyRequest}, BlockingOwner, [](auto&&){});
		BlockingOwner.Wait();
	}

	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_CachedDataProbablyExists);
		bool bResult;
		INC_DWORD_STAT(STAT_DDC_NumExist);
		STAT(double ThisTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ThisTime);
			FLegacyCacheGetRequest LegacyRequest;
			LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
			LegacyRequest.Name = LegacyRequest.Key.GetFullKey();
			LegacyRequest.Policy = ECachePolicy::Query | ECachePolicy::SkipData;
			FRequestOwner BlockingOwner(EPriority::Blocking);
			Backend->GetRoot().LegacyGet({LegacyRequest}, BlockingOwner,
				[&bResult](FLegacyCacheGetResponse&& Response) { bResult = Response.Status == EStatus::Ok; });
			BlockingOwner.Wait();
		}
		INC_FLOAT_STAT_BY(STAT_DDC_ExistTime, (float)ThisTime);
		return bResult;
	}

	virtual TBitArray<> CachedDataProbablyExistsBatch(TConstArrayView<FString> CacheKeys) override
	{
		TBitArray<> Result(false, CacheKeys.Num());
		if (!CacheKeys.IsEmpty())
		{
			DDC_SCOPE_CYCLE_COUNTER(DDC_CachedDataProbablyExistsBatch);
			INC_DWORD_STAT(STAT_DDC_NumExist);
			STAT(double ThisTime = 0);
			{
				SCOPE_SECONDS_COUNTER(ThisTime);
				TArray<FLegacyCacheGetRequest, TInlineAllocator<8>> LegacyRequests;
				int32 Index = 0;
				for (const FString& CacheKey : CacheKeys)
				{
					FLegacyCacheGetRequest& LegacyRequest = LegacyRequests.AddDefaulted_GetRef();
					LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
					LegacyRequest.Name = LegacyRequest.Key.GetFullKey();
					LegacyRequest.Policy = ECachePolicy::Query | ECachePolicy::SkipData;
					LegacyRequest.UserData = uint64(Index);
					++Index;
				}
				FRequestOwner BlockingOwner(EPriority::Blocking);
				Backend->GetRoot().LegacyGet(LegacyRequests, BlockingOwner,
					[&Result](FLegacyCacheGetResponse&& Response)
					{
						Result[int32(Response.UserData)] = Response.Status == EStatus::Ok;
					});
				BlockingOwner.Wait();
			}
			INC_FLOAT_STAT_BY(STAT_DDC_ExistTime, (float)ThisTime);
		}
		return Result;
	}

	virtual bool AllCachedDataProbablyExists(TConstArrayView<FString> CacheKeys) override
	{
		return CacheKeys.Num() == 0 || CachedDataProbablyExistsBatch(CacheKeys).CountSetBits() == CacheKeys.Num();
	}

	virtual bool TryToPrefetch(TConstArrayView<FString> CacheKeys, FStringView DebugContext) override
	{
		if (!CacheKeys.IsEmpty())
		{
			DDC_SCOPE_CYCLE_COUNTER(DDC_TryToPrefetch);
			UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("TryToPrefetch %d keys including %s from '%.*s'"),
			CacheKeys.Num(), *CacheKeys[0], DebugContext.Len(), DebugContext.GetData());
			TArray<FLegacyCacheGetRequest, TInlineAllocator<8>> LegacyRequests;
			int32 Index = 0;
			const FSharedString Name = DebugContext;
			for (const FString& CacheKey : CacheKeys)
			{
				FLegacyCacheGetRequest& LegacyRequest = LegacyRequests.AddDefaulted_GetRef();
				LegacyRequest.Name = Name;
				LegacyRequest.Key = FLegacyCacheKey(CacheKey, Backend->GetMaxKeyLength());
				LegacyRequest.Policy = ECachePolicy::Default | ECachePolicy::SkipData;
				LegacyRequest.UserData = uint64(Index);
				++Index;
			}
			bool bOk = true;
			FRequestOwner BlockingOwner(EPriority::Blocking);
			Backend->GetRoot().LegacyGet(LegacyRequests, BlockingOwner,
				[&bOk](FLegacyCacheGetResponse&& Response)
				{
					bOk &= Response.Status == EStatus::Ok;
				});
			BlockingOwner.Wait();
			return bOk;
		}
		return true;
	}

	void NotifyBootComplete() override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_NotifyBootComplete);
		Backend->NotifyBootComplete();
	}

	void AddToAsyncCompletionCounter(int32 Addend) override
	{
		Backend->AddToAsyncCompletionCounter(Addend);
	}

	bool AnyAsyncRequestsRemaining() const override
	{
		return Backend->AnyAsyncRequestsRemaining();
	}

	void WaitForQuiescence(bool bShutdown) override
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_WaitForQuiescence);
		Backend->WaitForQuiescence(bShutdown);
	}

	/** Get whether a Shared Data Cache is in use */
	virtual bool GetUsingSharedDDC() const override
	{		
		return Backend->GetUsingSharedDDC();
	}

	virtual const TCHAR* GetGraphName() const override
	{
		return Backend->GetGraphName();
	}

	virtual const TCHAR* GetDefaultGraphName() const override
	{
		return Backend->GetDefaultGraphName();
	}

	void GetDirectories(TArray<FString>& OutResults) override
	{
		Backend->GetDirectories(OutResults);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual IDDCCleanup* GetCleanup() const override
	{
		return const_cast<IDDCCleanup*>(static_cast<const IDDCCleanup*>(this));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual bool IsFinished() const override
	{
		return IsIdle();
	}

	virtual void WaitBetweenDeletes(bool bWait) override
	{
		if (!bWait)
		{
			BoostPriority();
		}
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStats) override
	{
		GatherUsageStats()->GatherLegacyUsageStats(UsageStats, TEXT(" 0"));
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual TSharedRef<FDerivedDataCacheStatsNode> GatherUsageStats() const override
	{
		return Backend->GatherUsageStats();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual void GatherResourceStats(TArray<FDerivedDataCacheResourceStat>& DDCResourceStats) const override
	{
		GatherDerivedDataCacheResourceStats(DDCResourceStats);
	}

	virtual void GatherSummaryStats(FDerivedDataCacheSummaryStats& DDCSummaryStats) const override
	{
		GatherDerivedDataCacheSummaryStats(DDCSummaryStats);
	}

	/** Get event delegate for data cache notifications */
	virtual FOnDDCNotification& GetDDCNotificationEvent()
	{
		return DDCNotificationEvent;
	}

protected:
	uint32 NextHandle()
	{
		return (uint32)CurrentHandle.Increment();
	}


private:

	/** 
	 * Internal function to build a cache key out of the plugin name, versions and plugin specific info
	 * @param	DataDeriver	plugin to produce the elements of the cache key.
	 * @return				Assembled cache key
	**/
	static FString BuildCacheKey(FDerivedDataPluginInterface* DataDeriver)
	{
		FString Result = FDerivedDataCacheInterface::BuildCacheKey(DataDeriver->GetPluginName(), DataDeriver->GetVersionString(), *DataDeriver->GetPluginSpecificCacheKeySuffix());
		return Result;
	}

	FDerivedDataBackend*		Backend;
	/** Counter used to produce unique handles **/
	FThreadSafeCounter			CurrentHandle;
	/** Object used for synchronization via a scoped lock **/
	FCriticalSection			SynchronizationObject;
	/** Map of handle to pending task **/
	TMap<uint32,FAsyncTask<FBuildAsyncWorker>*>	PendingTasks;

	/** Cache notification delegate */
	FOnDDCNotification DDCNotificationEvent;

public:
	// ICache Interface

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_Put);
		return Backend->GetRoot().Put(Requests, Owner, OnComplete ? MoveTemp(OnComplete) : FOnCachePutComplete([](auto&&) {}));
	}

	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_Get);
		return Backend->GetRoot().Get(Requests, Owner, OnComplete ? MoveTemp(OnComplete) : FOnCacheGetComplete([](auto&&) {}));
	}

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_PutValue);
		return Backend->GetRoot().PutValue(Requests, Owner, OnComplete ? MoveTemp(OnComplete) : FOnCachePutValueComplete([](auto&&) {}));
	}

	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetValue);
		return Backend->GetRoot().GetValue(Requests, Owner, OnComplete ? MoveTemp(OnComplete) : FOnCacheGetValueComplete([](auto&&) {}));
	}

	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final
	{
		DDC_SCOPE_CYCLE_COUNTER(DDC_GetChunks);
		return Backend->GetRoot().GetChunks(Requests, Owner, OnComplete ? MoveTemp(OnComplete) : FOnCacheGetChunkComplete([](auto&&) {}));
	}

	ICacheStoreMaintainer& GetMaintainer() final
	{
		return *this;
	}

	// ICacheStoreMaintainer Interface

	bool IsIdle() const final
	{
		return Algo::AllOf(CacheStoreMaintainers, &ICacheStoreMaintainer::IsIdle);
	}

	void BoostPriority() final
	{
		for (ICacheStoreMaintainer* Maintainer : CacheStoreMaintainers)
		{
			Maintainer->BoostPriority();
		}
	}

private:
	TArray<ICacheStoreMaintainer*> CacheStoreMaintainers;
};

ICache* CreateCache(FDerivedDataCacheInterface** OutLegacyCache)
{
	LLM_SCOPE_BYTAG(DerivedDataCache);
	FDerivedDataCache* Cache = new FDerivedDataCache;
	if (OutLegacyCache)
	{
		*OutLegacyCache = Cache;
	}
	return Cache;
}

} // UE::DerivedData::Private
