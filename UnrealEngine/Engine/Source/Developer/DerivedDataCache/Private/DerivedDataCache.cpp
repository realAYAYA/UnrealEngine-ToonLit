// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCache.h"
#include "DerivedDataCacheInterface.h"

#include "Algo/AllOf.h"
#include "AnalyticsEventAttribute.h"
#include "Async/AsyncWork.h"
#include "Async/InheritedContext.h"
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
#include "DerivedDataThreadPoolTask.h"
#include "Experimental/ZenServerInterface.h"
#include "Features/IModularFeatures.h"
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
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TSharedRef<FDerivedDataCacheStatsNode> RootNode = GetDerivedDataCacheRef().GatherUsageStats();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

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
			
			int64 LocalGetHits = 0;
			int64 LocalGetMisses = 0;
			FDerivedDataCacheSpeedStats LocalSpeedStats;
			if (LocalNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*LocalNode)->UsageStats.CreateConstIterator().Value();
				LocalGetHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				LocalGetMisses += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
				LocalSpeedStats = (*LocalNode)->SpeedStats;
			}
			const int64 LocalGetTotal = LocalGetHits + LocalGetMisses;

			int64 ZenLocalGetHits = 0;
			int64 ZenLocalGetMisses = 0;
			FDerivedDataCacheSpeedStats ZenLocalSpeedStats;
			if (ZenLocalNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*ZenLocalNode)->UsageStats.CreateConstIterator().Value();
				ZenLocalGetHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				ZenLocalGetMisses += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
				ZenLocalSpeedStats = (*ZenLocalNode)->SpeedStats;
			}			
			const int64 ZenLocalGetTotal = ZenLocalGetHits + ZenLocalGetMisses;
			
			int64 SharedGetHits = 0;
			int64 SharedGetMisses = 0;
			FDerivedDataCacheSpeedStats SharedSpeedStats;
			if (SharedNode)
			{
				// The shared DDC is only queried if the local one misses (or there isn't one). So it's hit rate is technically 
				const FDerivedDataCacheUsageStats& UsageStats = (*SharedNode)->UsageStats.CreateConstIterator().Value();
				SharedGetHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				SharedGetMisses += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
				SharedSpeedStats = (*SharedNode)->SpeedStats;
			}
			const int64 SharedGetTotal = SharedGetHits + SharedGetMisses;

			int64 ZenRemoteGetHits = 0;
			int64 ZenRemoteGetMisses = 0;
			FDerivedDataCacheSpeedStats ZenRemoteSpeedStats;
			if (ZenRemoteNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*ZenRemoteNode)->UsageStats.CreateConstIterator().Value();
				ZenRemoteGetHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				ZenRemoteGetMisses += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
				ZenRemoteSpeedStats = (*ZenRemoteNode)->SpeedStats;
			}
			const int64 ZenRemoteGetTotal = ZenRemoteGetHits + ZenRemoteGetMisses;

			int64 CloudGetHits = 0;
			int64 CloudGetMisses = 0;
			FDerivedDataCacheSpeedStats CloudSpeedStats;
			if (CloudNode)
			{
				const FDerivedDataCacheUsageStats& UsageStats = (*CloudNode)->UsageStats.CreateConstIterator().Value();
				CloudGetHits += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
				CloudGetMisses += UsageStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
				CloudSpeedStats = (*CloudNode)->SpeedStats;
			}
			const int64 CloudGetTotal = CloudGetHits + CloudGetMisses;

			const int64 RootGetHits = RootStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
			const int64 RootGetMisses = RootStats.GetStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
			const int64 RootGetTotal = RootGetHits + RootGetMisses;

			const int64 RootPutHits = RootStats.PutStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter);
			const int64 RootPutMisses = RootStats.PutStats.GetAccumulatedValueAnyThread(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter);
			const int64 RootPutTotal = RootPutHits + RootPutMisses;

			AddStat(TEXT("DDC.Summary"), FCookStatsManager::CreateKeyValueArray(
				TEXT("BackEnd"), FDerivedDataBackend::Get().GetGraphName(),
				TEXT("HasLocalCache"), LocalNode || ZenLocalNode,
				TEXT("HasSharedCache"), SharedNode || ZenRemoteNode,
				TEXT("HasCloudCache"), !!CloudNode,
				TEXT("HasZenCache"), ZenLocalNode || ZenRemoteNode,
				TEXT("TotalGetHits"), RootGetHits,
				TEXT("TotalGetMisses"), RootGetMisses,
				TEXT("TotalGets"), RootGetTotal,
				TEXT("TotalGetHitPct"), SafeDivide(RootGetHits, RootGetTotal),
				TEXT("GetMissPct"), SafeDivide(RootGetMisses, RootGetTotal),
				TEXT("TotalPutHits"), RootPutHits,
				TEXT("TotalPutMisses"), RootPutMisses,
				TEXT("TotalPuts"), RootPutTotal,	
				TEXT("TotalPutHitPct"), SafeDivide(RootPutHits, RootPutTotal),	
				TEXT("PutMissPct"), SafeDivide(RootPutMisses, RootPutTotal),
				TEXT("LocalGetHits"), LocalGetHits,
				TEXT("LocalGetTotal"), LocalGetTotal,
				TEXT("LocalGetHitPct"), SafeDivide(LocalGetHits, LocalGetTotal),
				TEXT("SharedGetHits"), SharedGetHits,
				TEXT("SharedGetTotal"), SharedGetTotal,
				TEXT("SharedGetHitPct"), SafeDivide(SharedGetHits, SharedGetTotal),
				TEXT("ZenLocalGetHits"), ZenLocalGetHits,
				TEXT("ZenLocalGetTotal"), ZenLocalGetTotal,
				TEXT("ZenLocalGetHitPct"), SafeDivide(ZenLocalGetHits, ZenLocalGetTotal),
				TEXT("ZenRemoteGetHits"), ZenRemoteGetHits,
				TEXT("ZenRemoteGetTotal"), ZenRemoteGetTotal,
				TEXT("ZenRemoteGetHitPct"), SafeDivide(ZenRemoteGetHits, ZenRemoteGetTotal),
				TEXT("CloudGetHits"), CloudGetHits,
				TEXT("CloudGetTotal"), CloudGetTotal,
				TEXT("CloudGetHitPct"), SafeDivide(CloudGetHits, CloudGetTotal),
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

void LaunchTaskInCacheThreadPool(IRequestOwner& Owner, TUniqueFunction<void ()>&& TaskBody)
{
	LaunchTaskInThreadPool(Owner, GCacheThreadPool, MoveTemp(TaskBody));
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

		/** Async worker that checks the cache backend and if that fails, calls the deriver to build the data and then puts the results to the cache **/
		void DoWork()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DDC_DoWork);

			const int64 NumBeforeDDC = Data.Num();
			bool bGetResult = false;
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
								UE_CLOG(Data.Num() != int64(RawSize), LogDerivedDataCache, Display,
									TEXT("Copied %" INT64_FMT " bytes when %" INT64_FMT " bytes were expected for %s from '%s'"),
									Data.Num(), int64(RawSize), *CacheKey, *Response.Name);
							}
						});
					BlockingOwner.Wait();
				}
				INC_FLOAT_STAT_BY(STAT_DDC_SyncGetTime, bSynchronousForStats ? (float)ThisTime : 0.0f);
			}

			if (bGetResult && Data.Num())
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
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FBuildAsyncWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

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
		PendingTask.StartSynchronousTask(EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
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
			AsyncTask->StartBackgroundTask(DataDeriver->GetCustomThreadPool(), EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
		}
		else
		{
			AsyncTask->StartSynchronousTask(EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
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
		PendingTask.StartSynchronousTask(EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
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
		AsyncTask->StartBackgroundTask(GCacheThreadPool, EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
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
		Backend->GatherResourceStats(DDCResourceStats);
	}

	virtual void GatherSummaryStats(FDerivedDataCacheSummaryStats& DDCSummaryStats) const override
	{
		GatherDerivedDataCacheSummaryStats(DDCSummaryStats);
	}

	virtual void GatherAnalytics(TArray<FAnalyticsEventAttribute>& Attributes) const override
	{
#if ENABLE_COOK_STATS

		// Gather the latest resource stats
		TArray<FDerivedDataCacheResourceStat> ResourceStats;

		GatherResourceStats(ResourceStats);

		// Append to the attributes
		for (const FDerivedDataCacheResourceStat& Stat : ResourceStats)
		{
			FString BaseName = TEXT("DDC_Resource_") + Stat.AssetType;

			BaseName = BaseName.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));

			{
				FString AttrName = BaseName + TEXT("_BuildCount");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildCount);
			}

			{
				FString AttrName = BaseName + TEXT("_BuildTimeSec");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildTimeSec);
			}

			{
				FString AttrName = BaseName + TEXT("_BuildSizeMB");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildSizeMB);
			}

			{
				FString AttrName = BaseName + TEXT("_LoadCount");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadCount);
			}

			{
				FString AttrName = BaseName + TEXT("_LoadTimeSecLoadTimeSec");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadTimeSec);
			}

			{
				FString AttrName = BaseName + TEXT("_LoadSizeMB");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadSizeMB);
			}
		}

		// Gather the summary stats
		FDerivedDataCacheSummaryStats SummaryStats;

		GatherDerivedDataCacheSummaryStats(SummaryStats);

		// Append to the attributes
		for (const FDerivedDataCacheSummaryStat& Stat : SummaryStats.Stats)
		{
			FString FormattedAttrName = "DDC_Summary_" + Stat.Key.Replace(TEXT("."), TEXT("_"));

			if (Stat.Value.IsNumeric())
			{
				Attributes.Emplace(FormattedAttrName, FCString::Atof(*Stat.Value));
			}
			else
			{
				Attributes.Emplace(FormattedAttrName, Stat.Value);
			}
		}

		// Gather the backend stats
		TSharedRef<FDerivedDataCacheStatsNode> RootNode = Backend->GatherUsageStats();
		RootNode->ForEachDescendant([&Attributes](TSharedRef<const FDerivedDataCacheStatsNode> Node)
			{
				const FString& CacheName = Node->GetCacheName();

				for (const FCookStatsManager::StringKeyValue& Stat : Node->CustomStats)
				{
					FString FormattedAttrName = CacheName + TEXT("_") + Stat.Key.Replace(TEXT("."), TEXT("_"));

					if (Stat.Value.IsNumeric())
					{
						Attributes.Emplace(FormattedAttrName, FCString::Atof(*Stat.Value));
					}
					else
					{
						Attributes.Emplace(FormattedAttrName, Stat.Value);
					}
				}
			});
#endif
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
