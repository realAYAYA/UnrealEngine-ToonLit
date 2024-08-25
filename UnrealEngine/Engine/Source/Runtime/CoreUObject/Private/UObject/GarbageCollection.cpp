// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GarbageCollection.cpp: Unreal object garbage collection code.
=============================================================================*/

#include "UObject/GarbageCollection.h"
#include "Algo/Find.h"
#include "Containers/Deque.h"
#include "Containers/StaticBitArray.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/TimeGuard.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Tasks/Task.h"
#include "UObject/ScriptInterface.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectHandle.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/LinkerLoad.h"
#include "UObject/GCObject.h"
#include "UObject/GCScopeLock.h"
#include "HAL/ExceptionHandling.h"
#include "UObject/UObjectClusters.h"
#include "HAL/LowLevelMemTracker.h"
#include "UObject/GarbageCollectionVerification.h"
#include "UObject/Package.h"
#include "Async/ParallelFor.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/GarbageCollectionHistory.h"
#include "UObject/GarbageCollectionTesting.h"
#include "UObject/PropertyOptional.h"
#include "UObject/ExpandingChunkedList.h"
#include "UObject/ReachabilityAnalysisState.h"
#include "UObject/ReachabilityAnalysis.h"

#include <atomic>

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMCollectionCycleRequest.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMMarkStack.h"
#include "VerseVM/VVMValue.h"
#endif

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

// FastReferenceCollector uses PERF_DETAILED_PER_CLASS_GC_STATS
#include "UObject/FastReferenceCollector.h"

DEFINE_LOG_CATEGORY(LogGarbage);
CSV_DEFINE_CATEGORY_MODULE(COREUOBJECT_API, GC, true);

#define PERF_DETAILED_PER_CLASS_GC_STATS				(LOOKING_FOR_PERF_ISSUES || 0) 

/** Allows release builds to override not verifying GC assumptions. Useful for profiling as it's hitchy. */
extern COREUOBJECT_API bool GShouldVerifyGCAssumptionsOnFullPurge;
extern COREUOBJECT_API float GVerifyGCAssumptionsChance;

/** Object count during last mark phase																				*/
FThreadSafeCounter		GObjectCountDuringLastMarkPhase;
/** Whether UObject hash tables are locked by GC */
bool GIsGarbageCollectingAndLockingUObjectHashTables = false;
/** Whether incremental object purge is in progress										*/
bool GObjIncrementalPurgeIsInProgress = false;
/** Whether GC is currently routing BeginDestroy to objects										*/
bool GObjUnhashUnreachableIsInProgress = false;
/** Time the GC started, needs to be reset on return from being in the background on some OSs */
double GCStartTime = 0.;
/** Whether FinishDestroy has already been routed to all unreachable objects. */
static bool GObjFinishDestroyHasBeenRoutedToAllObjects	= false;
/** 
 * Array that we'll fill with indices to objects that are still pending destruction after
 * the first GC sweep (because they weren't ready to be destroyed yet.) 
 */
static TArray<UObject *> GGCObjectsPendingDestruction;
/** Number of objects actually still pending destruction */
static int32 GGCObjectsPendingDestructionCount = 0;
/** Whether we need to purge objects or not.											*/
static bool GObjPurgeIsRequired = false;
/** Current object index for incremental purge.											*/
static int32 GObjCurrentPurgeObjectIndex = 0;
/** Current object index for incremental purge.											*/
static bool GObjCurrentPurgeObjectIndexNeedsReset = true;

/** Contains a list of objects that stayed marked as unreachable after the last reachability analysis */
static TArray<FUObjectItem*> GUnreachableObjects;
static FCriticalSection GUnreachableObjectsCritical;
static int32 GUnrechableObjectIndex = 0;

static UE::GC::Private::FStats GGCStats;

struct FGCTimingInfo
{
	double LastGCTime{};
	double LastGCDuration = -1;
};
static FGCTimingInfo GTimingInfo;

bool GIsGarbageCollecting = false;

/**
* Call back into the async loading code to inform of the destruction of serialized objects
*/
void NotifyUnreachableObjects(const TArrayView<FUObjectItem*>& UnreachableObjects);


/** Locks all UObject hash tables for performing GC reachability analysis.
 * Can be queried with IsGarbageCollectingAndLockingUObjectHashTables().
 * */
class FGCHashTableScopeLock
{
public:

	FORCEINLINE FGCHashTableScopeLock()
	{
		GIsGarbageCollectingAndLockingUObjectHashTables = true;
		LockUObjectHashTables();
	}
	FORCEINLINE ~FGCHashTableScopeLock()
	{
		UnlockUObjectHashTables();
		GIsGarbageCollectingAndLockingUObjectHashTables = false;
	}
};

FGCCSyncObject::FGCCSyncObject()
{
	GCUnlockedEvent = FPlatformProcess::GetSynchEventFromPool(true);
}
FGCCSyncObject::~FGCCSyncObject()
{
	FPlatformProcess::ReturnSynchEventToPool(GCUnlockedEvent);
	GCUnlockedEvent = nullptr;
}

FGCCSyncObject* GGCSingleton;

void FGCCSyncObject::Create()
{
	struct FSingletonOwner
	{
		FGCCSyncObject Singleton;

		FSingletonOwner()	{ GGCSingleton = &Singleton; }
		~FSingletonOwner()	{ GGCSingleton = nullptr;	}
	};
	static const FSingletonOwner MagicStaticSingleton;
}

FGCCSyncObject& FGCCSyncObject::Get()
{
	FGCCSyncObject* Singleton = GGCSingleton;
	check(Singleton);
	return *Singleton;
}

#define UE_LOG_FGCScopeGuard_LockAsync_Time 0

FGCScopeGuard::FGCScopeGuard()
{
#if UE_LOG_FGCScopeGuard_LockAsync_Time
	const double StartTime = FPlatformTime::Seconds();
#endif
	FGCCSyncObject::Get().LockAsync();
#if UE_LOG_FGCScopeGuard_LockAsync_Time
	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	if (FPlatformProperties::RequiresCookedData() && ElapsedTime > 0.001)
	{
		// Note this is expected to take roughly the time it takes to collect garbage and verify GC assumptions, so up to 300ms in development
		UE_LOG(LogGarbage, Warning, TEXT("%f ms for acquiring ASYNC lock"), ElapsedTime * 1000);
	}
#endif
}

FGCScopeGuard::~FGCScopeGuard()
{
	FGCCSyncObject::Get().UnlockAsync();
}

bool IsGarbageCollectionLocked()
{
	return FGCCSyncObject::Get().IsAsyncLocked();
}

static int32 GIncrementalBeginDestroyEnabled = 1;
static FAutoConsoleVariableRef CIncrementalBeginDestroyEnabled(
	TEXT("gc.IncrementalBeginDestroyEnabled"),
	GIncrementalBeginDestroyEnabled,
	TEXT("If true, the engine will destroy objects incrementally using time limit each frame"),
	ECVF_Default
);

int32 GMultithreadedDestructionEnabled = 0;
static FAutoConsoleVariableRef CMultithreadedDestructionEnabled(
	TEXT("gc.MultithreadedDestructionEnabled"),
	GMultithreadedDestructionEnabled,
	TEXT("If true, the engine will free objects' memory from a worker thread"),
	ECVF_Default
);

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
bool GEnableFrankenGC = true;
static FAutoConsoleVariableRef CEnableFrankenGC(
	TEXT("gc.EnableFrankenGC"),
	GEnableFrankenGC,
	TEXT("If true, the engine will run Verse GC concurrently with UE GC"),
	ECVF_Default
);
#endif

static TArray<UObjectReachabilityStressData*> GReachabilityStressData;
static void AllocateReachabilityStressData(FOutputDevice&)
{
	GenerateReachabilityStressData(GReachabilityStressData);
}

static void UnlinkReachabilityStressData(FOutputDevice&)
{
	UnlinkReachabilityStressData(GReachabilityStressData);
	GReachabilityStressData.Empty();
}

static FAutoConsoleCommandWithOutputDevice GGenerateReachabilityStressDataCmd(TEXT("gc.GenerateReachabilityStressData"),
		        						      TEXT("Allocate deeply-nested UObject tree to "
						        			   "stress test reachability analysis."),
FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&AllocateReachabilityStressData));

static FAutoConsoleCommandWithOutputDevice GUnlinkReachabilityStressDataCmd(TEXT("gc.UnlinkReachabilityStressData"),
                                 					    TEXT("Unlink previously-generated reachability analysis "
										 "stress test data for collection in the next cycle."),
FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&UnlinkReachabilityStressData));

#if UE_BUILD_SHIPPING
static constexpr int32 GGarbageReferenceTrackingEnabled = 0;
static constexpr int32 GDelayReachabilityIterations = 0;
#else
int32 GGarbageReferenceTrackingEnabled = 0;
static FAutoConsoleVariableRef CGarbageReferenceTrackingEnabled(
	TEXT("gc.GarbageReferenceTrackingEnabled"),
	GGarbageReferenceTrackingEnabled,
	TEXT("Causes the Garbage Collector to track and log unreleased garbage objects. If 1, will dump every reference. If 2, will dump a sample of the references to highlight problematic properties."),
	ECVF_Default
);

int32 GDelayReachabilityIterations = 0;
static FAutoConsoleVariableRef CDelayReachabilityIterations(
	TEXT("gc.DelayReachabilityIterations"),
	GDelayReachabilityIterations,
	TEXT("Causes the Garbage Collector to delay incremental reachability iterations by the provided number of frames."),
	ECVF_Default
);
#endif // UE_BUILD_SHIPPING

static int32 GAllowIncrementalReachability = 0;
static FAutoConsoleVariableRef CVarAllowIncrementalGC(
	TEXT("gc.AllowIncrementalReachability"),
	GAllowIncrementalReachability,
	TEXT("Set to control incremental Reachability Analysis (experimental)"),
	ECVF_Default
);

static float GIncrementalReachabilityTimeLimit = 0.005f;
static FAutoConsoleVariableRef CVarIncrementalReachabilityTimeLimit(
	TEXT("gc.IncrementalReachabilityTimeLimit"),
	GIncrementalReachabilityTimeLimit,
	TEXT("Time in seconds (game time) we should allow for incremental GC (experimental)."),
	ECVF_Default
);

static int32 GAllowIncrementalGather = 0;
static FAutoConsoleVariableRef CVarAllowIncrementalGather(
	TEXT("gc.AllowIncrementalGather"),
	GAllowIncrementalGather,
	TEXT("Set to control incremental Gather Unreachable Objects (experimental)"),
	ECVF_Default
);

#if UE_BUILD_SHIPPING
static constexpr float GIncrementalGatherTimeLimit = 0.0f;
#else
static float GIncrementalGatherTimeLimit = 0.0f;
static FAutoConsoleVariableRef CVarIncrementalGatherTimeLimit(
	TEXT("gc.IncrementalGatherTimeLimit"),
	GIncrementalGatherTimeLimit,
	TEXT("Override Incremental Gather Time Limit (in seconds)."),
	ECVF_Default
);
#endif

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
static int32 GVerifyNoUnreachableObjects = 0;
static FAutoConsoleVariableRef CVarVerifyNoUnreachableObjects(
	TEXT("gc.VerifyNoUnreachableObjects"),
	GVerifyNoUnreachableObjects,
	TEXT("Enables or disables no unreachable objects are reachable verification"),
	ECVF_Default
);
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS

static bool GForceEnableDebugGCProcessor = 0;
static FAutoConsoleVariableRef CVarForceEnableDebugGCProcessor(
	TEXT("gc.ForceEnableGCProcessor"),
	GForceEnableDebugGCProcessor,
	TEXT("Force garbage collection to use the debug processor which may provide additional information during GC crashes."),
	ECVF_Default
);

static int32 GMaxFinishDestroyTimeoutObjectsToLog = 10;
static FAutoConsoleVariableRef CVarMaxFinishDestroyTimeoutObjectsToLog (
	TEXT("gc.MaxFinishDestroyTimeoutObjectsToLog"),
	GMaxFinishDestroyTimeoutObjectsToLog ,
	TEXT("Maximum number of objects to log out when object destruction takes longer than expected"),
	ECVF_Default
);

#if UE_BUILD_SHIPPING
static constexpr int32 GDumpGCAnalyticsToLog = 0;
#else
static int32 GDumpGCAnalyticsToLog = 0;
static FAutoConsoleVariableRef CVarDumpGCAnalyticsToLog(
	TEXT("gc.DumpAnalyticsToLog"),
	GDumpGCAnalyticsToLog,
	TEXT("Dumps Garbage Collection analytics to log at the end of each GC."),
	ECVF_Default
);
#endif

namespace UE::GC
{

	struct FDetailedClassStats
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		/** Map from a UClass' FName to the number of objects that were purged during the last purge phase of this class.	*/
		TMap<const FName,uint64> ClassToPurgeCountMap;
		/** Map from a UClass' FName to the number of "Disregard For GC" object references followed for all instances.		*/
		TMap<const FName,uint64> ClassToDisregardedObjectRefsMap;
		/** Map from a UClass' FName to the number of regular object references followed for all instances.					*/
		TMap<const FName,uint64> ClassToRegularObjectRefsMap;
		/** Map from a UClass' FName to the number of clsuter references followed for all instances.						*/
		TMap<const FName,uint64> ClassToClustersRefsMap;
		/** Map from a UClass' FName to the number of cycles spent with GC.													*/
		TMap<const FName,uint64> ClassToCyclesMap;

		/** Number of objects traversed by GC in total.			 															*/
		uint32 NumObjectsTraversed = 0;
		/** Number of clusters traversed by GC in total.			 														*/
		uint32 NumClustersTraversed = 0;
		/** Number of references from cluster to cluster checked.	 														*/
		uint32 NumClusterToClusterRefs = 0;
		/** Number of references from cluster to mutable object checked.													*/
		uint32 NumClusterToObjectRefs = 0;
		/** Number of references from clusters to mutable objects which were later clustered. 								*/
		uint32 NumClusterToLateClusterRefs = 0;

		/** Number of disregarded object refs for current object.															*/
		uint32 CurrentObjectDisregardedObjectRefs;
		/** Number of regular object refs for current object.																*/
		uint32 CurrentObjectRegularObjectRefs;
		/** Number of references to clusters from the current object */
		uint32 CurrentObjectClusterRefs;

		// Timestamp for starting to profile the current object's outgoing references
		uint64 CurrentObjectStartCyles;
#endif
		
		void LogClassCountInfo(const TCHAR* LogText, TMap<const FName, uint64>& ClassToCountMap, int32 NumItemsToLog, uint64 TotalCount);
		void IncClusterToObjectRefs(FUObjectItem* Item);
		void IncClusterToClusterRefs(int32 Count);
		void IncNumClustersTraversed();

		void LogDetailedStatsSummary();
		void BeginTimingObject(UObject* CurrentObject);
		void UpdateDetailedStats(UObject* CurrentObject);
		void IncreaseObjectRefStats(UObject* RefToObject);

		void IncPurgeCount(UObject* Object);
		void LogPurgeStats(int32 Total);
	};
	static FDetailedClassStats GDetailedStats;

	/**
	 * Helper function to log the various class to count info maps.
	 *
	 * @param	LogText				Text to emit between number and class 
	 * @param	ClassToCountMap		TMap from a class' FName to "count"
	 * @param	NumItemsToList		Number of items to log
	 * @param	TotalCount			Total count, if 0 will be calculated
	 */
	void FDetailedClassStats::LogClassCountInfo( const TCHAR* LogText, TMap<const FName, uint64>& ClassToCountMap, int32 NumItemsToLog, uint64 TotalCount )
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		// Figure out whether we need to calculate the total count.
		bool bNeedToCalculateCount = false;
		if( TotalCount == 0 )
		{
			for (const TPair<FName, uint64>& Pair : ClassToCountMap) 
			{
				TotalCount += Pair.Value;
			}
		}
		ClassToCountMap.ValueSort(TGreater<uint64>{});

		// Log top NumItemsToLog class counts
		for (const TPair<FName, uint64>& Pair : ClassToCountMap)
		{
			const float Percent = 100.f * Pair.Value / TotalCount;
			const FString PercentString = (TotalCount > 0) ? FString::Printf(TEXT("%6.2f%%"), Percent) : FString(TEXT("  N/A  "));
			UE_LOG(LogGarbage, Log, TEXT("%5" UINT64_FMT "[% s] % s Class % s"), Pair.Value, *PercentString, LogText, *Pair.Key.ToString() ); 
		}

		// Empty the map for the next run.
		ClassToCountMap.Empty();
#endif
	};

	FORCEINLINE void FDetailedClassStats::IncClusterToObjectRefs(FUObjectItem* ObjectItem)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		NumClusterToObjectRefs++;
		if (ObjectItem->GetOwnerIndex() != 0)
		{
			NumClusterToLateClusterRefs++;
		}
#endif
	}

	FORCEINLINE void FDetailedClassStats::IncClusterToClusterRefs(int32 Count)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		NumClusterToClusterRefs += Count;
#endif
	}

	FORCEINLINE void FDetailedClassStats::IncNumClustersTraversed()
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		NumClusterToObjectRefs++;
#endif
	}

	void FDetailedClassStats::BeginTimingObject(UObject* CurrentObject)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		CurrentObjectStartCyles = FPlatformTime::Cycles64();
#endif
	}

	void FDetailedClassStats::UpdateDetailedStats(UObject* CurrentObject)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		uint64 DeltaCycles = FPlatformTime::Cycles64() - CurrentObjectStartCyles;
		// Keep track of how many refs we encountered for the object's class.
		const FName& ClassName = CurrentObject->GetClass()->GetFName();
		// Refs to objects that reside in permanent object pool.
		ClassToDisregardedObjectRefsMap.FindOrAdd(ClassName) += CurrentObjectDisregardedObjectRefs;
		// Refs to regular objects.
		ClassToRegularObjectRefsMap.FindOrAdd(ClassName) += CurrentObjectRegularObjectRefs;
		// Refs to clusters
		ClassToClustersRefsMap.FindOrAdd(ClassName) += CurrentObjectClusterRefs;
		// Track per class cycle count spent in GC.
		uint64  ClassCycles = ClassToCyclesMap.FindRef(ClassName);
		ClassToCyclesMap.Add(ClassName, ClassCycles + DeltaCycles);
		++NumObjectsTraversed;
		// Reset current counts.
		CurrentObjectDisregardedObjectRefs = 0;
		CurrentObjectRegularObjectRefs = 0;
		CurrentObjectClusterRefs = 0;
#endif
	}

	void FDetailedClassStats::LogDetailedStatsSummary()
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		LogClassCountInfo(TEXT("references to regular objects from"), ClassToRegularObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("references to permanent objects from"), ClassToDisregardedObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("references to clusters from"), ClassToClustersRefsMap, 20, 0);
		LogClassCountInfo(TEXT("cycles for GC"), ClassToCyclesMap, 20, 0);

		UE_LOG(LogGarbage, Log, TEXT("%d objects traversed"), NumObjectsTraversed);
		UE_LOG(LogGarbage, Log, TEXT("%d clusters traversed"), NumClustersTraversed);
		UE_LOG(LogGarbage, Log, TEXT("%d cluster to cluster refs followed"), NumClusterToClusterRefs);
		UE_LOG(LogGarbage, Log, TEXT("%d cluster to object refs followed"), NumClusterToObjectRefs);
		UE_LOG(LogGarbage, Log, TEXT("%d cluster to clustered mutable object refs followed"), NumClusterToLateClusterRefs);

		NumObjectsTraversed = 0;
		NumClustersTraversed = 0;
		NumClusterToClusterRefs = 0;
		NumClusterToObjectRefs = 0;
		NumClusterToLateClusterRefs = 0;
#endif
	}

	FORCEINLINE void FDetailedClassStats::IncreaseObjectRefStats(UObject* RefToObject)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		if (!RefToObject)
		{
			return;
		}

		FPermanentObjectPoolExtents Extents;
		if (Extents.Contains(RefToObject))
		{
			++CurrentObjectDisregardedObjectRefs;
		}
		else 
		{
			FUObjectItem* Item = GUObjectArray.ObjectToObjectItem(RefToObject);
			if (Item->HasAnyFlags(EInternalObjectFlags::ClusterRoot) || Item->GetOwnerIndex() > 0)
			{
				++CurrentObjectClusterRefs;
			}
			else
			{
				++CurrentObjectRegularObjectRefs;
			}
		}
#endif
	}

	FORCEINLINE void FDetailedClassStats::IncPurgeCount(UObject* Object)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		// Keep track of how many objects of a certain class we're purging.
		const FName& ClassName = Object->GetClass()->GetFName();
		int32 InstanceCount = ClassToPurgeCountMap.FindRef( ClassName );
		ClassToPurgeCountMap.Add( ClassName, ++InstanceCount );
#endif
	}

	FORCEINLINE void FDetailedClassStats::LogPurgeStats(int32 Total)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		LogClassCountInfo(TEXT("objects of"), ClassToPurgeCountMap, 10, Total);
#endif
	}

	void MarkAsReachable(const UObject* Obj)
	{
		Obj->MarkAsReachable();
	}

	EGCOptions GetReferenceCollectorOptions(bool bPerformFullPurge);
	EGatherOptions GetObjectGatherOptions();

	/** EInternalObjectFlags value representing a reachable object */
	EInternalObjectFlags GReachableObjectFlag = EInternalObjectFlags::ReachabilityFlag0;

	/** EInternalObjectFlag value representing an unreachable object */
	EInternalObjectFlags GUnreachableObjectFlag = EInternalObjectFlags::ReachabilityFlag1;

	/** EInternalObjectFlag value representing a maybe unreachable object */
	EInternalObjectFlags GMaybeUnreachableObjectFlag = EInternalObjectFlags::ReachabilityFlag2;

	bool GIsIncrementalReachabilityPending = false;
} // namespace UE::GC

namespace UE::GC::Private
{
	/** List of objects marked as reachable by GC barrier (see UObject::MarkAsReachable()) */
	static TExpandingChunkedList<UObject*> GReachableObjects;
	/** List of FUObjectItems representing cluster root objects marker as reachable by GC barrier (see UObject::MarkAsReachable()) */
	static TExpandingChunkedList<FUObjectItem*> GReachableClusters;

	using FGatherUnreachableObjectsState = TThreadedGather<TArray<FUObjectItem*>>;
	static FGatherUnreachableObjectsState GGatherUnreachableObjectsState;

	static TSet<int32> GRoots;
	static FCriticalSection GRootsCritical;
}

static bool GatherUnreachableObjects(UE::GC::EGatherOptions Options, double TimeLimit = 0.0);

bool FUObjectItem::SetRootFlags(EInternalObjectFlags FlagsToSet)
{
	using namespace UE::GC;
	using namespace UE::GC::Private;

	constexpr int32 RootFlags = (int32)EInternalObjectFlags_RootFlags;
	bool bIChangedIt = false;
	{
		FScopeLock RootsLock(&GRootsCritical);
		const int32 OldFlags = GetFlagsInternal();
		if ((OldFlags & RootFlags) == 0)
		{
			if (!(GUObjectArray.IsOpenForDisregardForGC() & GUObjectArray.DisregardForGCEnabled())) //-V792
			{
				GRoots.Add(GUObjectArray.ObjectToIndex(Object));
			}
		}
		bIChangedIt = ThisThreadAtomicallySetFlag_ForGC(FlagsToSet);
	}
	if (bIChangedIt & GIsIncrementalReachabilityPending) //-V792
	{
		// Setting any of the root flags on an object during incremental reachability requires a GC barrier
		// to make sure an object with root flags does not get Garbage Collected
		checkf(Object, TEXT("Setting an internal object flag on a null object entry"));
		Object->MarkAsReachable();
	}
	return bIChangedIt;
}

bool FUObjectItem::ClearRootFlags(EInternalObjectFlags FlagsToClear)
{
	using namespace UE::GC::Private;

	constexpr int32 RootFlags = (int32)EInternalObjectFlags_RootFlags;
	FScopeLock RootsLock(&GRootsCritical);
	const int32 OldFlags = GetFlagsInternal();
	if ((OldFlags & RootFlags) != 0 && ((OldFlags & ~(int32)FlagsToClear) & RootFlags) == 0)
	{
		GRoots.Remove(GUObjectArray.ObjectToIndex(Object));
	}
	return ThisThreadAtomicallyClearedFlag_ForGC((EInternalObjectFlags)FlagsToClear);

}

void OnDisregardForGCSetDisabled(int32 NumObjects)
{
	using namespace UE::GC;
	using namespace UE::GC::Private;
	using FMarkMarkDisregardState = TThreadedGather<TArray<UObject*>>;

	FMarkMarkDisregardState MarkDisregardState;
	
	MarkDisregardState.Start(EGatherOptions::Parallel, NumObjects);
	FMarkMarkDisregardState::FThreadIterators& ThreadIterators = MarkDisregardState.GetThreadIterators();

	// Objects in the disregard for GC set do not have any of the reachability flags set (this way they never become (Maybe)Unreachable) nor are they added to the GRoots array 
	FScopeLock RootsLock(&GRootsCritical);
	ParallelFor(TEXT("GC.OnDisregardForGCSetDisabled"), MarkDisregardState.NumWorkerThreads(), 1, [&ThreadIterators](int32 ThreadIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OnDisregardForGCSetDisabledTask);
		FMarkMarkDisregardState::FIterator& ThreadState = ThreadIterators[ThreadIndex];

		while (ThreadState.Index <= ThreadState.LastIndex)
		{
			int32 ObjectIndex = ThreadState.Index++;
			FUObjectItem* RootItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];	
			if (RootItem->Object)
			{
				RootItem->ThisThreadAtomicallySetFlag_ForGC(GReachableObjectFlag);
				if (RootItem->HasAnyFlags(EInternalObjectFlags_RootFlags))
				{
					GRoots.Add(ObjectIndex);
				}
			}
		}
	}, (MarkDisregardState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
}

/**
 * Helper class for destroying UObjects on a worker thread
 */
class FAsyncPurge : public FRunnable
{
	/** Thread to run the worker FRunnable on. Destroys objects. */
	volatile FRunnableThread* Thread;
	/** Id of the worker thread */
	uint32 AsyncPurgeThreadId;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;
	/** Event that triggers the UObject destruction */
	FEvent* BeginPurgeEvent;
	/** Event that signales the UObject destruction is finished */
	FEvent* FinishedPurgeEvent;
	/** Current index into the global unreachable objects array (GUnreachableObjects) of the object being destroyed */
	int32 ObjCurrentPurgeObjectIndex;
	/** Number of objects deferred to the game thread to destroy */
	std::atomic<int32> NumObjectsToDestroyOnGameThread;
	/** Number of objectsalready destroyed on the game thread */
	int32 NumObjectsDestroyedOnGameThread;
	/** Current index into the global unreachable objects array (GUnreachableObjects) of the object being destroyed on the game thread */
	int32 ObjCurrentPurgeObjectIndexOnGameThread;
	/** Number of unreachable objects the last time single-threaded tick was called */
	int32 LastUnreachableObjectsCount;
	/** Stats for the number of objects destroyed */
	int32 ObjectsDestroyedSinceLastMarkPhase;

	/** [PURGE/GAME THREAD] Destroys objects that are unreachable */
	template <bool bMultithreaded> // Having this template argument lets the compiler strip unnecessary checks
	bool TickDestroyObjects(bool bUseTimeLimit, double TimeLimit, double StartTime)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPurge::TickDestroyObjects);
		const int32 TimeLimitEnforcementGranularityForDeletion = 100;
		int32 ProcessedObjectsCount = 0;
		bool bFinishedDestroyingObjects = true;

		// Avoid fetch_add synchronization for each object sent to the game-thread and instead load the value
		// once into a local variable and replace by a simple store to publish the value to the other thread.
		// This is safe because we know only this thread is going to modify it.
		int32 LocalNumObjectsToDestroyOnGameThread = NumObjectsToDestroyOnGameThread.load(std::memory_order_acquire);
		
		while (ObjCurrentPurgeObjectIndex < GUnreachableObjects.Num())
		{
			FUObjectItem* ObjectItem = GUnreachableObjects[ObjCurrentPurgeObjectIndex];
			check(ObjectItem->IsUnreachable());

			UObject* Object = (UObject*)ObjectItem->Object;
			check(Object->HasAllFlags(RF_FinishDestroyed | RF_BeginDestroyed));
			if (!bMultithreaded || Object->IsDestructionThreadSafe())
			{
				// Can't lock once for the entire batch here as it could hold the lock for too long
				GUObjectArray.LockInternalArray();
				Object->~UObject();
				GUObjectArray.UnlockInternalArray();
				GUObjectAllocator.FreeUObject(Object);
				GUnreachableObjects[ObjCurrentPurgeObjectIndex] = nullptr;
			}
			else
			{
				NumObjectsToDestroyOnGameThread.store(++LocalNumObjectsToDestroyOnGameThread, std::memory_order_release);
			}
			++ProcessedObjectsCount;
			++ObjectsDestroyedSinceLastMarkPhase;
			++ObjCurrentPurgeObjectIndex;

			// Time slicing when running on the game thread
			if (!bMultithreaded && bUseTimeLimit && (ProcessedObjectsCount == TimeLimitEnforcementGranularityForDeletion) && (ObjCurrentPurgeObjectIndex < GUnreachableObjects.Num()))
			{
				ProcessedObjectsCount = 0;
				if ((FPlatformTime::Seconds() - StartTime) > TimeLimit)
				{
					bFinishedDestroyingObjects = false;
					break;
				}				
			}
		}
		return bFinishedDestroyingObjects;
	}

	/** [GAME THREAD] Destroys objects that are unreachable and couldn't be destroyed on the worker thread */
	bool TickDestroyGameThreadObjects(bool bUseTimeLimit, double TimeLimit, double StartTime)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPurge::TickDestroyGameThreadObjects);
		const int32 TimeLimitEnforcementGranularityForDeletion = 100;
		int32 ProcessedObjectsCount = 0;
		bool bFinishedDestroyingObjects = true;

		// Lock once for the entire batch
		GUObjectArray.LockInternalArray();

		// Cache the number of objects to destroy locally. The number may grow later but that's ok, we'll catch up to it in the next tick
		const int32 LocalNumObjectsToDestroyOnGameThread = NumObjectsToDestroyOnGameThread.load(std::memory_order_acquire);

		while (NumObjectsDestroyedOnGameThread < LocalNumObjectsToDestroyOnGameThread && ObjCurrentPurgeObjectIndexOnGameThread < GUnreachableObjects.Num())
		{
			FUObjectItem* ObjectItem = GUnreachableObjects[ObjCurrentPurgeObjectIndexOnGameThread];			
			if (ObjectItem)
			{
				GUnreachableObjects[ObjCurrentPurgeObjectIndexOnGameThread] = nullptr;
				UObject* Object = (UObject*)ObjectItem->Object;
				Object->~UObject();
				GUObjectAllocator.FreeUObject(Object);
				++ProcessedObjectsCount;
				++NumObjectsDestroyedOnGameThread;

				if (bUseTimeLimit && (ProcessedObjectsCount == TimeLimitEnforcementGranularityForDeletion) && NumObjectsDestroyedOnGameThread < LocalNumObjectsToDestroyOnGameThread)
				{
					ProcessedObjectsCount = 0;
					if ((FPlatformTime::Seconds() - StartTime) > TimeLimit)
					{
						bFinishedDestroyingObjects = false;
						break;
					}
				}
			}
			++ObjCurrentPurgeObjectIndexOnGameThread;
		}

		GUObjectArray.UnlockInternalArray();

		// Make sure that when we reach the end of GUnreachableObjects array, there's no objects to destroy left
		check(!bFinishedDestroyingObjects || NumObjectsDestroyedOnGameThread == LocalNumObjectsToDestroyOnGameThread);

		// Note that even though NumObjectsToDestroyOnGameThread may have been incremented by now or still hasn't but it will be 
		// after we report we're done with all objects, it doesn't matter since we don't care about the result of this function in MT mode
		return bFinishedDestroyingObjects;
	}

	/** Waits for the worker thread to finish destroying objects */
	void WaitForAsyncDestructionToFinish()
	{
		FinishedPurgeEvent->Wait();
	}

public:

	/** 
	 * Constructor
	 * @param bMultithreaded if true, the destruction of objects will happen on a worker thread
	 */
	FAsyncPurge(bool bMultithreaded)
		: Thread(nullptr)
		, AsyncPurgeThreadId(0)
		, BeginPurgeEvent(nullptr)
		, FinishedPurgeEvent(nullptr)
		, ObjCurrentPurgeObjectIndex(0)
		, NumObjectsToDestroyOnGameThread(0)
		, NumObjectsDestroyedOnGameThread(0)
		, ObjCurrentPurgeObjectIndexOnGameThread(0)
		, LastUnreachableObjectsCount(0)
		, ObjectsDestroyedSinceLastMarkPhase(0)
	{
		BeginPurgeEvent = FPlatformProcess::GetSynchEventFromPool(true);
		FinishedPurgeEvent = FPlatformProcess::GetSynchEventFromPool(true);
		FinishedPurgeEvent->Trigger();
		if (bMultithreaded)
		{
			check(FPlatformProcess::SupportsMultithreading());
			FPlatformAtomics::InterlockedExchangePtr((void**)&Thread, FRunnableThread::Create(this, TEXT("FAsyncPurge"), 0, TPri_BelowNormal));			
		}
		else
		{
			AsyncPurgeThreadId = GGameThreadId;
		}
	}

	virtual ~FAsyncPurge()
	{
		check(IsFinished());
		delete Thread;
		Thread = nullptr;
		FPlatformProcess::ReturnSynchEventToPool(BeginPurgeEvent);
		FPlatformProcess::ReturnSynchEventToPool(FinishedPurgeEvent);
		BeginPurgeEvent = nullptr;
		FinishedPurgeEvent = nullptr;
	}

	/** Returns true if the destruction process is finished */
	FORCEINLINE bool IsFinished() const
	{
		if (Thread)
		{
			return FinishedPurgeEvent->Wait(0, true) && NumObjectsToDestroyOnGameThread == NumObjectsDestroyedOnGameThread;
		}
		else
		{
			return (ObjCurrentPurgeObjectIndex >= LastUnreachableObjectsCount && NumObjectsToDestroyOnGameThread == NumObjectsDestroyedOnGameThread);
		}
	}

	/** [MAIN THREAD] Adds objects to the purge queue */
	void BeginPurge()
	{
		check(IsFinished()); // In single-threaded mode we need to be finished or the condition below will hang
		if (FinishedPurgeEvent->Wait())
		{
			FinishedPurgeEvent->Reset();

			ObjCurrentPurgeObjectIndex = 0;
			ObjectsDestroyedSinceLastMarkPhase = 0;
			NumObjectsToDestroyOnGameThread = 0;
			NumObjectsDestroyedOnGameThread = 0;
			ObjCurrentPurgeObjectIndexOnGameThread = 0;

			BeginPurgeEvent->Trigger();
		}
	}

	/** [GAME THREAD] Ticks the purge process on the game thread */
	void TickPurge(bool bUseTimeLimit, double TimeLimit, double StartTime)
	{
		bool bCanStartDestroyingGameThreadObjects = true;
		if (!Thread)
		{
			// If we're running single-threaded we need to tick the main loop here too
			LastUnreachableObjectsCount = GUnreachableObjects.Num();
			bCanStartDestroyingGameThreadObjects = TickDestroyObjects<false>(bUseTimeLimit, TimeLimit, StartTime);
		}
		if (bCanStartDestroyingGameThreadObjects)
		{
			do
			{
				// Deal with objects that couldn't be destroyed on the worker thread. This will do nothing when running single-threaded
				bool bFinishedDestroyingObjectsOnGameThread = TickDestroyGameThreadObjects(bUseTimeLimit, TimeLimit, StartTime);
				if (!Thread && bFinishedDestroyingObjectsOnGameThread)
				{
					// This only gets triggered here in single-threaded mode
					FinishedPurgeEvent->Trigger();
				}
			} while (!bUseTimeLimit && !IsFinished());
		}
	}

	/** Returns the number of objects already destroyed */
	int32 GetObjectsDestroyedSinceLastMarkPhase() const
	{
		return ObjectsDestroyedSinceLastMarkPhase - NumObjectsToDestroyOnGameThread + NumObjectsDestroyedOnGameThread;
	}

	/** Resets the number of objects already destroyed */
	void ResetObjectsDestroyedSinceLastMarkPhase()
	{
		ObjectsDestroyedSinceLastMarkPhase = 0;
	}

	/** 
	  * Returns true if this function is called from the async destruction thread. 
	  * It will also return true if we're running single-threaded and this function is called on the game thread
	  */
	bool IsInAsyncPurgeThread() const
	{
		return AsyncPurgeThreadId == FPlatformTLS::GetCurrentThreadId();
	}

	/* Returns true if it can run multi-threaded destruction */
	bool IsMultithreaded() const
	{
		return !!Thread;
	}

	//~ Begin FRunnable Interface.
	virtual bool Init()
	{
		return true;
	}

	virtual uint32 Run()
	{
		AsyncPurgeThreadId = FPlatformTLS::GetCurrentThreadId();
		
		while (StopTaskCounter.GetValue() == 0)
		{
			if (BeginPurgeEvent->Wait(15, true))
			{
				BeginPurgeEvent->Reset();
				TickDestroyObjects<true>(/* bUseTimeLimit = */ false, /* TimeLimit = */ 0.0f, /* StartTime = */ 0.0);
				FinishedPurgeEvent->Trigger();
			}
		}
		FinishedPurgeEvent->Trigger();
		return 0;
	}

	virtual void Stop()
	{
		StopTaskCounter.Increment();
	}
	//~ End FRunnable Interface

	void VerifyAllObjectsDestroyed()
	{
		for (FUObjectItem* ObjectItem : GUnreachableObjects)
		{
			UE_CLOG(ObjectItem, LogGarbage, Fatal, TEXT("Object 0x%016llx has not been destroyed during async purge"), (int64)(PTRINT)ObjectItem->Object);
		}
	}
};
static FAsyncPurge* GAsyncPurge = nullptr;

/**
  * Returns true if this function is called from the async destruction thread.
  * It will also return true if we're running single-threaded and this function is called on the game thread
  */
bool IsInGarbageCollectorThread()
{
	return GAsyncPurge ? GAsyncPurge->IsInAsyncPurgeThread() : IsInGameThread();
}

//////////////////////////////////////////////////////////////////////////

namespace UE::GC {

/** Pool for reusing contexts between CollectReferences calls */
class FContextPool
{
	friend class FContextPoolScope;	
	TArray<TUniquePtr<FWorkerContext>> Reusable;
	int32 NumAllocated = 0;
};

// Helps validate single-threaded access and check for leaks
//
// Bit of defensive coding during a large refactor, can be removed in the long run
class FContextPoolScope
{
	UE_NONCOPYABLE(FContextPoolScope);

	FContextPool& Pool;
	bool bNested;
	static FContextPool& Get();

	void CheckGameThread()
	{
		checkf(IsInGameThread(), TEXT("Context pool use restricted to game thread"));
	}
public:
	FContextPoolScope() : Pool(Get()) { CheckGameThread(); }

	FWorkerContext* AllocateFromPool();
	void ReturnToPool(FWorkerContext* Context);
	TConstArrayView<TUniquePtr<FWorkerContext>> PeekFree() { return Pool.Reusable; }
	
	void Cleanup();
	int32 NumAllocated() const { return Pool.NumAllocated; }
};

FContextPool& FContextPoolScope::Get()
{
	static FContextPool Singleton;
	return Singleton;
}

FWorkerContext* FContextPoolScope::AllocateFromPool()
{
	CheckGameThread();

	++Pool.NumAllocated;
	if (Pool.Reusable.IsEmpty())
	{
		return new FWorkerContext();
	}
	
	FWorkerContext* Out = Pool.Reusable.Pop().Release();
	Out->AllocateWorkerIndex();
	return Out;
}

void FContextPoolScope::ReturnToPool(FWorkerContext* Context)
{
	CheckGameThread();
	check(Pool.NumAllocated >= 1);
	--Pool.NumAllocated;
	Context->FreeWorkerIndex();
	Context->Stats = {};
	Context->IncrementalStructs = {};
	Context->bIsSuspended = false;
	Pool.Reusable.Emplace(Context);
}

void FContextPoolScope::Cleanup()
{
	SIZE_T FreedMemory = 0;
	for (const TUniquePtr<FWorkerContext>& Context : Pool.Reusable)
	{
		checkf(Context->WeakReferences.Num() == 0, TEXT("Cleaning up with active weak references"));
		FreedMemory += Context->GetAllocatedSize();
	}
	UE_LOG(LogGarbage, Log, TEXT("Freed %" SIZE_T_FMT "b from %d GC contexts"), FreedMemory, Pool.Reusable.Num());

	Pool.Reusable.Empty();
}

//////////////////////////////////////////////////////////////////////////

template<EGCOptions Options, class ContainerType>
static void MarkReferencedClustersAsReachable(int32 ClusterIndex, ContainerType& ObjectsToSerialize);

// Let MarkReferencedClustersAsReachable generically Add() to both TArray<UObject*> and FWorkBlockifier
template<EGCOptions Options>
struct TAddAdapter
{
	FWorkBlockifier& Queue;
	FORCEINLINE void Add(UObject* Object) { Queue.Add<Options>(Object); }
};

template<EGCOptions Options>
FORCEINLINE static void MarkReferencedClustersAsReachableThunk(int32 ClusterIndex, FWorkBlockifier& ObjectsToSerialize)
{
	TAddAdapter<Options> Adapter{ObjectsToSerialize};
	MarkReferencedClustersAsReachable<Options>(ClusterIndex, Adapter);
}

/** Marks all objects that can't be directly in a cluster but are referenced by it as reachable */
template<EGCOptions Options, class ContainerType>
static bool MarkClusterMutableObjectsAsReachable(FUObjectCluster& Cluster, ContainerType& ObjectsToSerialize)
{
	// This is going to be the return value and basically means that we ran across some Garbage objects
	bool bAddClusterObjectsToSerialize = false;
	for (int32& ReferencedMutableObjectIndex : Cluster.MutableObjects)
	{
		if (ReferencedMutableObjectIndex >= 0) // Garbage Elimination support
		{
			FUObjectItem* ReferencedMutableObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectIndex);
			UE::GC::GDetailedStats.IncClusterToObjectRefs(ReferencedMutableObjectItem);
			if (!ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::Garbage))
			{
				if (ReferencedMutableObjectItem->IsMaybeUnreachable())
				{
					if (ReferencedMutableObjectItem->MarkAsReachableInterlocked_ForGC())
					{
						// Needs doing because this is either a normal unclustered object (clustered objects are never unreachable) or a cluster root
						ObjectsToSerialize.Add(static_cast<UObject*>(ReferencedMutableObjectItem->Object));

						// So is this a cluster root maybe?
						if (ReferencedMutableObjectItem->GetOwnerIndex() < 0)
						{
							MarkReferencedClustersAsReachable<Options>(ReferencedMutableObjectItem->GetClusterIndex(), ObjectsToSerialize);
						}
					}
				}
				else if (ReferencedMutableObjectItem->GetOwnerIndex() > 0 && !ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
				{
					// This is a clustered object that maybe hasn't been processed yet
					if (ReferencedMutableObjectItem->ThisThreadAtomicallySetFlag_ForGC(EInternalObjectFlags::ReachableInCluster))
					{
						// Needs doing, we need to get its cluster root and process it too
						FUObjectItem* ReferencedMutableObjectsClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectItem->GetOwnerIndex());
						if (ReferencedMutableObjectsClusterRootItem->IsMaybeUnreachable())
						{
							// The root is also maybe unreachable so process it and all the referenced clusters
							if (ReferencedMutableObjectsClusterRootItem->MarkAsReachableInterlocked_ForGC())
							{
								MarkReferencedClustersAsReachable<Options>(ReferencedMutableObjectsClusterRootItem->GetClusterIndex(), ObjectsToSerialize);
							}
						}
					}
				}
			}
			else
			{
				// Garbage Elimination support for clusters
				ReferencedMutableObjectIndex = -1;
				bAddClusterObjectsToSerialize = true;
			}
		}
	}
	return bAddClusterObjectsToSerialize;
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
static void MarkClusterMutableCellsAsReachable(FUObjectCluster& Cluster)
{
	if (GIsFrankenGCCollecting && Cluster.MutableCells.Num() > 0)
	{
		Verse::FMarkStack MarkStack;
		for (Verse::VCell* Cell : Cluster.MutableCells)
		{
			MarkStack.MarkNonNull(Cell);
		}
		Verse::FHeap::AddExternalMarkStack(MoveTemp(MarkStack));
	}
}
#else
FORCEINLINE static void MarkClusterMutableCellsAsReachable(FUObjectCluster& Cluster)
{
}
#endif

/** Marks all clusters referenced by another cluster as reachable */
template<EGCOptions Options, class ContainerType>
static FORCENOINLINE void MarkReferencedClustersAsReachable(int32 ClusterIndex, ContainerType& ObjectsToSerialize)
{
	UE::GC::GDetailedStats.IncNumClustersTraversed();

	// If we run across some Garbage objects we need to add all objects from this cluster
	// to ObjectsToSerialize so that we can properly null out all the references.
	// It also means this cluster will have to be dissolved because we may no longer guarantee all cross-cluster references are correct.

	bool bAddClusterObjectsToSerialize = false;
	FUObjectCluster& Cluster = GUObjectClusters[ClusterIndex];
	UE::GC::GDetailedStats.IncClusterToClusterRefs(Cluster.ReferencedClusters.Num());
	// Also mark all referenced objects from outside of the cluster as reachable
	for (int32& ReferncedClusterIndex : Cluster.ReferencedClusters)
	{
		if (ReferncedClusterIndex >= 0) // Garbag Elimination support
		{
			FUObjectItem* ReferencedClusterRootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferncedClusterIndex);
			if (!ReferencedClusterRootObjectItem->HasAnyFlags(EInternalObjectFlags::Garbage))
			{
				ReferencedClusterRootObjectItem->FastMarkAsReachableInterlocked_ForGC();
			}
			else
			{
				// Garbage Elimination support for clusters
				ReferncedClusterIndex = -1;
				bAddClusterObjectsToSerialize = true;
			}
		}
	}
	if (MarkClusterMutableObjectsAsReachable<Options>(Cluster, ObjectsToSerialize))
	{
		bAddClusterObjectsToSerialize = true;
	}
	MarkClusterMutableCellsAsReachable(Cluster);
	if (bAddClusterObjectsToSerialize)
	{
		// We need to process all cluster objects to handle Garbage objects we nulled out (-1) from the cluster.
		for (int32 ClusterObjectIndex : Cluster.Objects)
		{
			FUObjectItem* ClusterObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ClusterObjectIndex);
			UObject* ClusterObject = static_cast<UObject*>(ClusterObjectItem->Object);
			ObjectsToSerialize.Add(ClusterObject);
		}
		Cluster.bNeedsDissolving = true;
		GUObjectClusters.SetClustersNeedDissolving();
	}
}

//////////////////////////////////////////////////////////////////////////

enum class EKillable { No, Yes };

// Retains address of reference to allow killing it
struct FMutableReference { UObject** Object; };
struct FImmutableReference { UObject* Object; };
struct FReferenceArray { TArray<UObject*>* Array; };

// Cached immutable reference to avoid rereading it
struct FResolvedMutableReference : FImmutableReference
{
	UObject** Mutable;
};

FORCEINLINE_DEBUGGABLE static bool ValidateReference(UObject* Object, FPermanentObjectPoolExtents PermanentPool, FReferenceToken Referencer, FMemberId MemberId)
{
	bool bOk = (!PermanentPool.Contains(Object)) & (!!Object) & IsObjectHandleResolved(reinterpret_cast<FObjectHandle&>(Object)); //-V792

#if ENABLE_GC_OBJECT_CHECKS
	if (bOk)
	{
		if (
#if DO_POINTER_CHECKS_ON_GC
			!IsPossiblyAllocatedUObjectPointer(Object) ||
#endif
			!Object->IsValidLowLevelFast())
		{
			UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, Referencer: %s, MemberId %s (%d)"),
				(int64)(PTRINT)Object,
				*Referencer.GetDescription(),
				*Referencer.GetMemberName(MemberId), MemberId.AsPrintableIndex());
		}
	}
#endif // ENABLE_GC_OBJECT_CHECKS

	return bOk;
}

struct FReferenceMetadata 
{
	FReferenceMetadata() {} // Uninitialized

	explicit FReferenceMetadata(int32 ObjectIndex)
	{
		ObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ObjectIndex);
		Flags = ObjectItem->GetFlags();
	}

	EInternalObjectFlags Flags;
	FUObjectItem* ObjectItem;

	FORCEINLINE bool Has(EInternalObjectFlags InFlags) const { return !!(int32(Flags) & int32(InFlags)); }
};

//////////////////////////////////////////////////////////////////////////

static constexpr int32 MaxWorkers = FReachabilityAnalysisState::MaxWorkers;

int32 GetNumSlowAROs();

// Allocates and caches 4K pages used for temporary allocations during GC
//
// Currently doesn't free pages until shutdown
class FPageAllocator
{
public:
	// We need one extra worker cache to be able to perform non-async reference scans when incremental reachability is suspended
	static constexpr int32 NumWorkerCaches = MaxWorkers + 1;

private:
	struct alignas(PLATFORM_CACHE_LINE_SIZE) FWorkerCache
	{
		UE_NONCOPYABLE(FWorkerCache);
		FWorkerCache() = default;
		~FWorkerCache()
		{
			for (void* Page : MakeArrayView(Pages, Num))
			{
				FreePage(Page);
			}
		}

		bool Push(void* Page)
		{
			check(Page);
			if (Num < Capacity)
			{
				Pages[Num++] = Page;
				return true;	
			}
			return false;
		}

		void* Pop()
		{
			return Num > 0 ? Pages[--Num] : nullptr;
		}

		static constexpr uint32 Capacity = 512 / MaxWorkers;
		uint32 Num = 0;
		void* Pages[Capacity] = {};
	};	

	struct FSharedCache
	{
		UE_NONCOPYABLE(FSharedCache);
		FSharedCache() = default;
		~FSharedCache()
		{
			for (void* Page : Pages)
			{
				FreePage(Page);
			}
		}

		void Push(void* Page)
		{
			FScopeLock Scope(&Lock);
			Pages.Add(Page);
		}

		void PushSurplus(FWorkerCache& Worker)
		{
			// Each worker context also needs a page for FWorkBlockifier and FStructBlockifer
			const uint32 NumWorkerInitialPages = 2 + GetNumSlowAROs();

			if (Worker.Num > NumWorkerInitialPages)
			{
				FScopeLock Scope(&Lock);
				Pages.Append(Worker.Pages + NumWorkerInitialPages, Worker.Num - NumWorkerInitialPages);
				Worker.Num = NumWorkerInitialPages;
			}
		}

		void* Pop()
		{
			FScopeLock Scope(&Lock);
			return Pages.IsEmpty() ? nullptr : Pages.Pop(EAllowShrinking::No);
		}

		FCriticalSection Lock;
		TArray<void*> Pages;
	};	

	FSharedCache SharedCache;
	FWorkerCache WorkerCaches[NumWorkerCaches];

public:
	static constexpr uint64 PageSize = 4096;

	void* AllocatePage(int32 WorkerIdx)
	{
		check(WorkerIdx >=0 && WorkerIdx < NumWorkerCaches);
		if (void* WorkerPage = WorkerCaches[WorkerIdx].Pop())
		{
			check(IsValidPage(WorkerPage));
			return WorkerPage;
		}
		else if (void* SharedPage = SharedCache.Pop())
		{
			check(IsValidPage(SharedPage));
			return SharedPage;
		}

		void* NewPage = FMemory::Malloc(PageSize, PageSize);
		return NewPage;
	}
	
	void ReturnWorkerPage(int32 WorkerIdx, void* Page)
	{
		check(IsValidPage(Page));
		if (!WorkerCaches[WorkerIdx].Push(Page))
		{
			FreePage(Page);
		}
	}

	void ReturnSharedPage(void* Page)
	{
		check(IsValidPage(Page));
		SharedCache.Push(Page);
	}

	static void FreePage(void* Page)
	{
		check(IsValidPage(Page));
		FMemory::Free(Page);
	}

	uint64 CountBytes() const
	{
		uint64 NumPages = SharedCache.Pages.Num();
		for (const FWorkerCache& WorkerCache : WorkerCaches)
		{
			NumPages += WorkerCache.Num;
		}
		return NumPages * PageSize;
	}

	static bool IsValidPage(void* Page)
	{
		return Page && UPTRINT(Page) % PageSize == 0;
	}
};

static FPageAllocator GScratchPages;

////////////// TReferenceBatcher and FStructBatcher helpers //////////////

class FBatcherBase
{
protected:
	template<class EntryType, uint32 Capacity, uint32 PrefetchCapacity = 0>
	struct TBatchQueue
	{
		uint32 Num = 0;
		EntryType Entries[Capacity + PrefetchCapacity];

		FORCEINLINE static constexpr uint32 Max() { return Capacity; }
		FORCEINLINE bool IsFull() const { return Num == Capacity; }
		FORCEINLINE void Push(EntryType Entry) { Entries[Num++] = Entry; }
		FORCEINLINE uint32 Slack() const { return Capacity - Num; }
		FORCEINLINE EntryType& operator[](uint32 Idx) { return Entries[Idx]; }

		TBatchQueue() { FMemory::Memzero(Entries); } // Zero out so prefetching ahead fetches same (near) null addresses
		~TBatchQueue() { checkf(Num == 0, TEXT("Failed to flush")); }
	};

	static constexpr uint32 ArrayBatchSize = 32;
	static constexpr uint32 UnvalidatedBatchSize = 32;
	static constexpr uint32 ValidatedBatchSize = 1024;
	static constexpr uint32 ValidatedPrefetchAhead = 64;

	struct FValidatedBitmask
	{
		FORCEINLINE_DEBUGGABLE void Set(uint32 Idx, bool Value) { Words[Idx/64] |= (uint64(Value) << (Idx%64)); }
		FORCEINLINE_DEBUGGABLE uint64 Get(uint32 Idx) { return (uint64(1) << (Idx%64)) & Words[Idx/64]; }
		FORCEINLINE_DEBUGGABLE uint32 CountBits()
		{
			uint32 Out = 0;
			for (uint64 Word : Words)
			{
				Out += FPlatformMath::CountBits(Word);
			}
			return Out;
		}

		FORCEINLINE_DEBUGGABLE static FValidatedBitmask Or(const FValidatedBitmask& A, const FValidatedBitmask& B)
		{
			FValidatedBitmask Out;
			for (uint32 WordIdx = 0; WordIdx < NumWords; ++WordIdx)
			{
				Out.Words[WordIdx] = A.Words[WordIdx] | B.Words[WordIdx];
			}
			return Out;
		}

		FORCEINLINE_DEBUGGABLE static FValidatedBitmask And(const FValidatedBitmask& A, const FValidatedBitmask& B)
		{
			FValidatedBitmask Out;
			for (uint32 WordIdx = 0; WordIdx < NumWords; ++WordIdx)
			{
				Out.Words[WordIdx] = A.Words[WordIdx] & B.Words[WordIdx];
			}
			return Out;
		}


		static const uint32 NumWords = 1 + (UnvalidatedBatchSize - 1) / 64; //-V1064
		uint64 Words[NumWords] = {};
	};
};

FORCEINLINE UObject*						GetObject(FImmutableReference In) { return In.Object; }
FORCEINLINE UObject*						GetObject(FMutableReference In) { return *In.Object; }
FORCEINLINE void							PrefetchObjectPointer(FImmutableReference) {}
FORCEINLINE void							PrefetchObjectPointer(FMutableReference In) { FPlatformMisc::Prefetch(In.Object); }
FORCEINLINE FImmutableReference				ToImmutableReference(FMutableReference In) { return {*In.Object}; }
FORCEINLINE FImmutableReference				ToResolvedReference(FImmutableReference In) { return In; }
FORCEINLINE FResolvedMutableReference		ToResolvedReference(FMutableReference In) { return { ToImmutableReference(In), In.Object }; }
template<class RefType> RefType				ToReference(FMutableReference In);
template<> FORCEINLINE FImmutableReference	ToReference(FMutableReference In) { return ToImmutableReference(In); }
template<> FORCEINLINE FMutableReference	ToReference(FMutableReference In) { return In; }
template<class RefType> RefType				MakeReference(UObject*& Object);
template<> FORCEINLINE FImmutableReference	MakeReference(UObject*& Object) { return {Object}; }
template<> FORCEINLINE FMutableReference	MakeReference(UObject*& Object) { return {&Object}; }

using FStridedReferenceArray = Private::FStridedReferenceArray;
using FStridedReferenceView = Private::FStridedReferenceView;

FORCEINLINE_DEBUGGABLE static TArrayView<UObject*> Split(/* in-out */ TArrayView<UObject*>& Body, int32 Idx)
{
	check(Idx <= Body.Num());
	TArrayView<UObject*> Head(Body.GetData(), Idx);
	Body = TArrayView<UObject*>(Body.GetData() + Idx, Body.Num() - Idx);
	return Head;
}

FORCEINLINE_DEBUGGABLE static FStridedReferenceView Split(/* in-out */ FStridedReferenceView& Body, int32 Idx)
{
	check(Idx <= Body.Num);
	check(Body.Stride > 0);
	FStridedReferenceView Head = Body;
	Head.Num = Idx;
	Body.Data += Idx * Body.Stride;
	Body.Num -= Idx;
	return Head;
}

/////////////////////////// TReferenceBatcher ///////////////////////////

/**
 * Queues up memory indirection to avoid synchronous cache misses by prefetching data just in time.
 *
 * Uses bounded queues to avoid branching, control temp memory consumption, allow compiler to generate vectorized code
 * and make life easier for the branch predictor.
 * 
 * Array queue feed into "unvalidated" references queue, which feed into validated references queue, which gets processed when full.
 * 
 * Unvalidated means references that might be nullptr or part of the permanent object pool, neither of which require processing.
 */
template <class UnvalidatedReferenceType, class ValidatedReferenceType, class ProcessorType>
class TReferenceBatcher : public FBatcherBase
{
	FWorkerContext& Context;
	const FPermanentObjectPoolExtents PermanentPool;

	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<FReferenceArray, ArrayBatchSize>									UnvalidatedArrays;			// Drains to UnvalidatedReferences
	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<FStridedReferenceArray, ArrayBatchSize>								UnvalidatedStridedArrays;	// Drains to UnvalidatedReferences
	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<UnvalidatedReferenceType, UnvalidatedBatchSize>						UnvalidatedReferences;		// Drains to ValidatedReferences
	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<ValidatedReferenceType, ValidatedBatchSize, ValidatedPrefetchAhead>	ValidatedReferences;		// Drains to ProcessorType::HandleBatchedReference

public:
	TReferenceBatcher(FWorkerContext& InContext) : Context(InContext) {}
	
	FORCEINLINE_DEBUGGABLE void PushArray(TArray<UObject*>& Array)
	{
		FPlatformMisc::Prefetch(&Array);
		UnvalidatedArrays.Push({&Array});
		if (UnvalidatedArrays.IsFull())
		{
			DrainArraysFull();
		}
	}

	FORCEINLINE_DEBUGGABLE void PushArray(FStridedReferenceArray StridedArray)
	{
		FPlatformMisc::Prefetch(StridedArray.Array);
		UnvalidatedStridedArrays.Push(StridedArray);
		if (UnvalidatedStridedArrays.IsFull())
		{
			DrainStridedArraysFull();
		}
	}

	template<class ViewType>
	FORCEINLINE_DEBUGGABLE void PushReferenceView(ViewType View)
	{
		if (GetNum(View))
		{
			// Fill up UnvalidatedReferences slack and drain it to avoid inserting an element at a time and testing if full
			for (uint32 Slack = UnvalidatedReferences.Slack(); (uint32)GetNum(View) >= Slack; Slack = UnvalidatedBatchSize) //-V1021
			{
				QueueUnvalidated(Split(/* in-out */ View, Slack));
				check(UnvalidatedReferences.IsFull());
				DrainUnvalidatedFull();
			}

			QueueUnvalidated(View);
			check(!UnvalidatedReferences.IsFull());
		}
	}

	FORCEINLINE_DEBUGGABLE void PushReference(UnvalidatedReferenceType UnvalidatedReference)
	{
		UnvalidatedReferences.Push(UnvalidatedReference);
		if (UnvalidatedReferences.IsFull())
		{
			DrainUnvalidatedFull();
		}
	}

	FORCEINLINE_DEBUGGABLE void FlushQueues()
	{
		// Slower path with dynamic Num that the compiler won't unroll or vectorize, unlike DrainXyzFull methods
		DrainArrays(UnvalidatedArrays.Num);
		DrainStridedArrays(UnvalidatedStridedArrays.Num);
		DrainUnvalidated(UnvalidatedReferences.Num);
		DrainValidated(ValidatedReferences.Num);
	}

private:

	// Process queued arrays of references and feed contents to unvalidated queue
	FORCEINLINE_DEBUGGABLE void DrainArrays(uint32 Num)
	{
		check(Num <= ArrayBatchSize);

		for (FReferenceArray Entry : MakeArrayView(UnvalidatedArrays.Entries, Num))
		{
			FPlatformMisc::Prefetch(Entry.Array->GetData());
		}

		for (FReferenceArray Entry : MakeArrayView(UnvalidatedArrays.Entries, Num))
		{
			PushReferenceView(MakeArrayView(*Entry.Array));
		}

		UnvalidatedArrays.Num = 0;
	}

	// Process queued strided arrays of references and feed contents to unvalidated queue
	FORCEINLINE_DEBUGGABLE void DrainStridedArrays(uint32 Num)
	{
		check(Num <= ArrayBatchSize);

		FStridedReferenceView Views[ArrayBatchSize];
		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			Views[Idx] = ToView(UnvalidatedStridedArrays.Entries[Idx]);
		}

		for (FStridedReferenceView View : MakeArrayView(Views, Num))
		{
			FPlatformMisc::Prefetch(View.Data);
		}
		
		for (FStridedReferenceView View : MakeArrayView(Views, Num))
		{
			PushReferenceView(View);
		}

		UnvalidatedStridedArrays.Num = 0;
	}


	template<class ViewType>
	FORCEINLINE_DEBUGGABLE void QueueUnvalidated(ViewType View)
	{
		check((uint32)GetNum(View) <= UnvalidatedReferences.Slack());

		UnvalidatedReferenceType* UnvalidatedIt = UnvalidatedReferences.Entries + UnvalidatedReferences.Num;
		for (UObject*& Object : View)
		{
			*UnvalidatedIt++ = MakeReference<UnvalidatedReferenceType>(Object);
		}

		UnvalidatedReferences.Num += GetNum(View);
	}

	// Validate all unvalidated references and feed referencing in need of processing to validated queue
	FORCEINLINE_DEBUGGABLE void DrainUnvalidated(const uint32 Num)
	{
		check(Num <= UnvalidatedBatchSize);
		Context.Stats.AddReferences(Num);

		FPermanentObjectPoolExtents Permanent(PermanentPool);
		FValidatedBitmask ValidsA, ValidsB;
		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			UObject* Object = GetObject(UnvalidatedReferences[Idx]);
			uint64 ObjectAddress = reinterpret_cast<uint64>(Object);
			ValidsA.Set(Idx, !Permanent.Contains(Object));
		}

		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			UObject* Object = GetObject(UnvalidatedReferences[Idx]);
			ValidsB.Set(Idx, (!!Object) & IsObjectHandleResolved(reinterpret_cast<FObjectHandle&>(Object))); //-V792
		}
		
		FValidatedBitmask Validations = FValidatedBitmask::And(ValidsA, ValidsB);
		uint32 NumValid = Validations.CountBits();
		uint32 UnvalidatedIdx = 0;
		for (uint32 Slack = ValidatedReferences.Slack(); NumValid >= Slack; Slack = ValidatedBatchSize) //-V1021
		{
			QueueValidReferences(Slack, Validations, /* in-out */  UnvalidatedIdx);
			check(ValidatedReferences.IsFull());
			DrainValidatedFull();
			NumValid -= Slack;
		}

		QueueValidReferences(NumValid, Validations, UnvalidatedIdx);
		check(!ValidatedReferences.IsFull());

		UnvalidatedReferences.Num = 0;
	}

	FORCEINLINE_DEBUGGABLE void QueueValidReferences(uint32 NumToAppend, FValidatedBitmask Validations, uint32& InOutIdx)
	{
		checkSlow(NumToAppend <= Validations.CountBits());
		checkSlow(ValidatedReferences.Num + NumToAppend <= ValidatedReferences.Max());
		uint32 NewQueueNum = ValidatedReferences.Num + NumToAppend;
		for (uint32 QueueIdx = ValidatedReferences.Num; QueueIdx < NewQueueNum; ++InOutIdx)
		{
			bool bIsValid = !!Validations.Get(InOutIdx);
			ValidatedReferences[QueueIdx] = ToResolvedReference(UnvalidatedReferences[InOutIdx]);
			QueueIdx += bIsValid;
		}

		ValidatedReferences.Num += NumToAppend;
	}

	// Process all queued validated references
	FORCEINLINE_DEBUGGABLE void DrainValidated(const uint32 Num)
	{
		check(Num <= ValidatedBatchSize);

		// Prefetch metadata
		// 
		// UObjectBase::InternalIndex is 4B at offset 12. We allocate UObjects with 16B alignment so InternalIndex always
		// shares cacheline with the raw UObject*. Not adding an offset leads to a bit faster and more compact code.
		static constexpr uint32 InternalIndexPrefetchOffset = 0;
		static constexpr uint32 PrefetchAhead = ValidatedPrefetchAhead;
		static constexpr ::size_t OffsetOfFlags = FUObjectItem::OffsetOfFlags();

		uint32 ObjectIndices[ValidatedBatchSize];
		if (Num > PrefetchAhead)
		{
			for (uint32 Idx = 0; Idx < PrefetchAhead; ++Idx)
			{
				FPlatformMisc::Prefetch(ValidatedReferences[Idx].Object, InternalIndexPrefetchOffset);
			}
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				ObjectIndices[Idx] = GUObjectArray.ObjectToIndex(ValidatedReferences[Idx].Object);
				FPlatformMisc::Prefetch(ValidatedReferences[Idx + PrefetchAhead].Object, InternalIndexPrefetchOffset);
			}
		}
		else
		{
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				FPlatformMisc::Prefetch(ValidatedReferences[Idx].Object, InternalIndexPrefetchOffset);
			}
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				ObjectIndices[Idx] = GUObjectArray.ObjectToIndex(ValidatedReferences[Idx].Object);
			}
		}

		FChunkedFixedUObjectArray& ObjectArray = GUObjectArray.GetObjectItemArrayUnsafe();
		FReferenceMetadata Metadatas[ValidatedBatchSize + PrefetchAhead];
		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			Metadatas[Idx].ObjectItem = ObjectArray.GetObjectPtr(ObjectIndices[Idx]);
		}

		if (Num > PrefetchAhead)
		{
			FMemory::Memzero(Metadatas + ValidatedBatchSize, PrefetchAhead * sizeof(FReferenceMetadata));

			for (uint32 Idx = 0; Idx < PrefetchAhead; ++Idx)
			{
				FPlatformMisc::Prefetch(Metadatas[Idx].ObjectItem, OffsetOfFlags);
			}
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				Metadatas[Idx].Flags = Metadatas[Idx].ObjectItem->GetFlags();
				FPlatformMisc::Prefetch(Metadatas[Idx + PrefetchAhead].ObjectItem, OffsetOfFlags);
			}
		}
		else
		{
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				FPlatformMisc::Prefetch(Metadatas[Idx].ObjectItem, OffsetOfFlags);
			}
			for (uint32 Idx = 0; Idx < Num; ++Idx)
			{
				Metadatas[Idx].Flags = Metadatas[Idx].ObjectItem->GetFlags();
			}
		}

		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			ProcessorType::HandleBatchedReference(Context, ValidatedReferences[Idx], Metadatas[Idx]);
		}

		ValidatedReferences.Num = 0;
	}

	// Helps generate vectorized code with static Num

	FORCENOINLINE void DrainArraysFull()			{ DrainArrays(UnvalidatedArrays.Max()); }
	FORCENOINLINE void DrainStridedArraysFull()		{ DrainStridedArrays(UnvalidatedStridedArrays.Max()); }
	FORCENOINLINE void DrainUnvalidatedFull()		{ DrainUnvalidated(UnvalidatedReferences.Max()); }
	FORCENOINLINE void DrainValidatedFull()			{ DrainValidated(ValidatedReferences.Max()); }
};


///////////////////////// FStructBatcher + helpers ///////////////////////

// Fixed block of array-of-structs waiting to be processed
struct FStructArrayBlock
{
	static constexpr uint32 Lookahead = 2;
	static constexpr uint32 Capacity = (FPageAllocator::PageSize - sizeof(FStructArrayBlock*)) / sizeof(FStructArray) - Lookahead;

	FStructArray Data[Capacity + Lookahead];
	FStructArrayBlock* NextFull;

	FStructArray* GetPadEnd() { return Data + Capacity; }
};

// Unbounded queue of validated / non-empty AoS
class FStructBlockifier
{
	UE_NONCOPYABLE(FStructBlockifier);
public:
	explicit FStructBlockifier(FWorkerContext& Context)
	: WorkerIndex(Context.GetWorkerIndex())
	{
		if (Context.IncrementalStructs.ContainsBatchData())
		{
			Wip = Context.IncrementalStructs.Wip;
			WipIt = Context.IncrementalStructs.WipIt;
			Context.IncrementalStructs = {};
		}
		else
		{
			AllocateWipBlock(nullptr);
		}
	}

	~FStructBlockifier()
	{
		if (Wip)
		{
			check(!CanPop());
			FreeBlock(Wip);
		}
	}
	
	void Push(FStructArray AoS)
	{
		*WipIt = AoS;
		if (++WipIt == Wip->Data+ FStructArrayBlock::Capacity)
		{
			AllocateWipBlock(Wip);
		}
	}

	FStructArray* PushUninitialized(int32 Num)
	{
		check(Num > 0 && Num <= GetSlack());

		FStructArray* Out = WipIt;

		WipIt += Num;
		if (WipIt == Wip->Data + FStructArrayBlock::Capacity)
		{
			AllocateWipBlock(Wip);
		}
		return Out;
	}

	int32 GetSlack() const
	{
		return IntCastChecked<int32>(Wip->Data + FStructArrayBlock::Capacity - WipIt);
	}

	bool CanPop() const
	{
		return (PartialNum() > 0) | !!(Wip->NextFull);
	}

	FStructArrayBlock* PopBlock(int32& OutNum)
	{
		if (FStructArrayBlock* Full = Wip->NextFull)
		{
			FPlatformMisc::Prefetch(Full->NextFull);
			FPlatformMisc::Prefetch(Full->Data);
			PadBlock(Full, FStructArrayBlock::Capacity);
			Wip->NextFull = Full->NextFull;
			OutNum = FStructArrayBlock::Capacity;
			return Full;
		}
		
		OutNum = PartialNum();
		check(OutNum > 0);
		PadBlock(Wip, OutNum);
		FStructArrayBlock* Out = Wip;
		AllocateWipBlock(nullptr);
		return Out;
	}

	void FreeBlock(FStructArrayBlock* Block)
	{
		GScratchPages.ReturnWorkerPage(WorkerIndex, Block);
	}

	FSuspendedStructBatch Suspend()
	{
		FSuspendedStructBatch SuspendData = { Wip, WipIt };
		Wip = nullptr;
		WipIt = nullptr;
		return SuspendData;
	}

private:
	FStructArray* WipIt; // Wip->Data cursor
	FStructArrayBlock* Wip;
	const int32 WorkerIndex;
	
	int32 PartialNum() const
	{
		check(WipIt >= Wip->Data && WipIt < Wip->Data + FStructArrayBlock::Capacity);
		return static_cast<int32>(WipIt - Wip->Data);
	}

	static void PadBlock(FStructArrayBlock* Block, int32 PadIdx)
	{
		check(PadIdx <= FStructArrayBlock::Capacity);
		FMemory::Memzero(Block->Data + PadIdx, sizeof(FStructArray) * FStructArrayBlock::Lookahead);
	}

	void AllocateWipBlock(FStructArrayBlock* NextFull)
	{	
		static_assert(sizeof(FStructArrayBlock) <= FPageAllocator::PageSize, "Block must fit in page");
	
		Wip = reinterpret_cast<FStructArrayBlock*>(GScratchPages.AllocatePage(WorkerIndex));
		//Wip = new (GScratchPages.AllocatePage(WorkerIndex)) FStructArrayBlock;
		Wip->NextFull = NextFull;
		WipIt = Wip->Data;
	}
};

struct FSparseStructArray
{
	FSchemaView Schema{NoInit};
	FScriptSparseArray* Array;
	int32 Num;
	uint32 Stride;
};

struct FStructArrayWithoutStride
{
	FSchemaView Schema{NoInit};
	uint8* Data;
	int32 Num;
};

FORCEINLINE_DEBUGGABLE void CheckValid(FSchemaView Schema)
{
	check(!Schema.IsEmpty());
	check(Schema.GetHeader().RefCount.load(std::memory_order_relaxed) > 0); // Memory stomp or using destroyed schema
	check(Schema.GetHeader().StructStride % 8 == 0); // Memory stomp or using destroyed schema
}

static FStructArray CopyAndLoadStride(FStructArrayWithoutStride In)
{
	CheckValid(In.Schema);
	return {In.Schema, In.Data, In.Num, In.Schema.GetStructStride() };
}

// Queues up arrays-of-structs and maps/sets as sparse array-of-structs for later processing by ProcessStructs
class FStructBatcher : public FBatcherBase
{
	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<FSparseStructArray, ArrayBatchSize>			UnvalidatedSparseStructArrays;	// Drains to ValidatedStructArrays
	alignas (PLATFORM_CACHE_LINE_SIZE)	TBatchQueue<FStructArrayWithoutStride, ArrayBatchSize>	UnvalidatedStructArrays;		// Drains to ValidatedStructArrays
	alignas (PLATFORM_CACHE_LINE_SIZE)	FStructBlockifier										ValidatedStructArrays;			// Drained by ProcessStructs, which feed back into TReferenceBatcher

public:
	explicit FStructBatcher(FWorkerContext& Context) : ValidatedStructArrays(Context) {}

	FORCEINLINE_DEBUGGABLE void PushSparseStructArray(FSchemaView Schema, FScriptSparseArray& Array)
	{
		CheckValid(Schema);

		UnvalidatedSparseStructArrays.Push({ Schema, &Array, Array.Num(), Schema.GetStructStride() });
		if (UnvalidatedSparseStructArrays.IsFull())
		{
			DrainSparseStructArraysFull();
		}
	}
	
	FORCEINLINE_DEBUGGABLE void PushStructArray(FSchemaView Schema, uint8* Data, int32 Num)
	{
		CheckValid(Schema);

		UnvalidatedStructArrays.Push({Schema, Data, Num});
		if (UnvalidatedStructArrays.IsFull())
		{
			DrainStructArraysFull();
		}
	}

	FORCEINLINE_DEBUGGABLE void FlushBoundedQueues()
	{
		DrainSparseStructArrays(UnvalidatedSparseStructArrays.Num);
		DrainStructArrays(UnvalidatedStructArrays.Num);
	}

	FStructBlockifier& GetUnboundedQueue() { return ValidatedStructArrays;	}

	FSuspendedStructBatch Suspend()
	{
		FlushBoundedQueues();
		return ValidatedStructArrays.Suspend();
	}

private:
	FORCEINLINE_DEBUGGABLE void DrainSparseStructArrays(uint32 Num)
	{
		check(Num <= ArrayBatchSize);

		if (Num == 0)
		{
			return;
		}

		// This can likely be optimized further, e.g. prefetching, intermediate queues, FValidatedBitmask for IsCompact() etc
		for (FSparseStructArray Entry : MakeArrayView(UnvalidatedSparseStructArrays.Entries, Num))
		{
			if (Entry.Num)
			{
				FScriptSparseArray& Array = *Entry.Array;
				FStructArray Slice = { Entry.Schema, Private::GetSparseData(Array), Entry.Num, Entry.Stride };
				if (Array.IsCompact())
				{
					ValidatedStructArrays.Push(Slice);
				}
				else
				{
					Slice.Num = 0;
					for (uint32 Idx = 0, NumIndices = Array.GetMaxIndex(); Idx < NumIndices; ++Idx)
					{
						if (Array.IsAllocated(Idx))
						{
							++Slice.Num;
						}
						else
						{
							if (Slice.Num > 0)
							{
								ValidatedStructArrays.Push(Slice);
								Slice.Data += Slice.Stride * Slice.Num;
								Slice.Num = 0;
							}

							Slice.Data += Slice.Stride;
						}
					}

					if (Slice.Num > 0)
					{
						ValidatedStructArrays.Push(Slice);
					}
				}
			}
		}

		UnvalidatedSparseStructArrays.Num = 0;
	}
	
	FORCEINLINE_DEBUGGABLE void DrainStructArrays(uint32 Num)
	{
		check(Num <= UnvalidatedStructArrays.Max());
		
		if (Num == 0)
		{
			return;
		}
		
		uint32 ValidIndices[UnvalidatedStructArrays.Max()];
		uint32* ValidIt = ValidIndices;
		for (uint32 Idx = 0; Idx < Num; ++Idx)
		{
			bool bIsValid = UnvalidatedStructArrays.Entries[Idx].Num > 0;
			*ValidIt = Idx;
			ValidIt += bIsValid;
		}

		for (uint32* IdxIt = ValidIndices, *IdxEnd = ValidIt; IdxIt != IdxEnd; )
		{
			uint32* NextIdxEnd = FMath::Min(IdxIt + ValidatedStructArrays.GetSlack(), IdxEnd);
			FStructArray* OutIt = ValidatedStructArrays.PushUninitialized(static_cast<int32>(NextIdxEnd - IdxIt));
			do
			{
				*OutIt++ = CopyAndLoadStride(UnvalidatedStructArrays.Entries[*IdxIt++]);
			}
			while (IdxIt != NextIdxEnd);
		}

		UnvalidatedStructArrays.Num = 0;
	}

	// Helps generate vectorized code with static Num

	FORCENOINLINE void DrainStructArraysFull()			{ DrainStructArrays(UnvalidatedStructArrays.Max());	}
	FORCENOINLINE void DrainSparseStructArraysFull()	{ DrainSparseStructArrays(UnvalidatedSparseStructArrays.Max());	}
};

//////////////////////////////////////////////////////////////////////////

FORCEINLINE_DEBUGGABLE static void PadObjects(UObject* LastObject, TArrayView<UObject*> Padding)
{
	check(LastObject->IsValidLowLevel());

	for (UObject*& ObjectPastEnd : Padding)
	{
		ObjectPastEnd = LastObject;
	}
}

void PadObjectArray(TArray<UObject*>& Objects)
{
	if (int32 Num = Objects.Num())
	{
		Objects.Reserve(Num + ObjectLookahead);
		PadObjects(Objects.Last(), MakeArrayView(Objects.GetData() + Num, ObjectLookahead));
	}
}

FORCEINLINE_DEBUGGABLE void PadBlock(FWorkBlock& Block)
{
	PadObjects(Block.GetObjects().Last(), Block.GetPadding());
}

//////////////////////////////////////////////////////////////////////////

class FWorkerIndexAllocator
{
	std::atomic<uint64> Used{0};
	static_assert(FPageAllocator::NumWorkerCaches <= 64, "Currently supports single uint64 word");
public:
	int32 AllocateChecked()
	{
		while (true)
		{
			uint64 UsedNow = Used.load(std::memory_order_relaxed);
			uint64 FreeIndex = FPlatformMath::CountTrailingZeros64(~UsedNow);
			checkf(FreeIndex < FPageAllocator::NumWorkerCaches, TEXT("Exceeded max active GC worker contexts"));

			uint64 Mask = uint64(1) << FreeIndex;
			if ((Used.fetch_or(Mask) & Mask) == 0)
			{
				return static_cast<int32>(FreeIndex);
			}
		}
	}

	void FreeChecked(int32 Index)
	{
		check(Index >= 0 && Index < FPageAllocator::NumWorkerCaches);
		uint64 Mask = uint64(1) << Index;
		uint64 Old = Used.fetch_and(~Mask);
		checkf(Old & Mask, TEXT("Index already freed"));
	}
};
static FWorkerIndexAllocator GWorkerIndices;

FWorkerContext::FWorkerContext()
{
	AllocateWorkerIndex();
	ObjectsToSerialize.Init();
}
	
FWorkerContext::~FWorkerContext()
{
	FreeWorkerIndex();
}

void FWorkerContext::AllocateWorkerIndex()
{
	check(GetWorkerIndex() == INDEX_NONE);
	ObjectsToSerialize.SetWorkerIndex(GWorkerIndices.AllocateChecked());
}

void FWorkerContext::FreeWorkerIndex()
{
	if (GetWorkerIndex() != INDEX_NONE)
	{
		GWorkerIndices.FreeChecked(GetWorkerIndex());
		ObjectsToSerialize.SetWorkerIndex(INDEX_NONE);
	}
}

// Bounded SPMC work-stealing queue
//
// Derived from task graph TWorkStealingQueueBase2 and added CheckEmpty(),
// reduced alignment and simplified some code
class FBoundedWorkstealingQueue
{
	static constexpr uint32 Capacity = 16;
	static constexpr uint64 FreeSlot = 0;
	static constexpr uint64 TakenSlot = 1;

public:
	// Called by the single producer
	bool Push(FWorkBlock* Block)
	{
		uint64 Item = reinterpret_cast<uint64>(Block);
		checkSlow(Item != FreeSlot);
		checkSlow(Item != TakenSlot);

		uint32 Idx = (Head + 1) % Capacity;
		uint64 Slot = Slots[Idx].load(std::memory_order_acquire);

		if (Slot == FreeSlot)
		{
			Slots[Idx].store(Item, std::memory_order_release);
			Head++;
			checkSlow(Head % Capacity == Idx);
			return true;		
		}

		return false;
	}

	// Called by single producer, pops in LIFO order
	FWorkBlock* Pop()
	{
		uint32 Idx = Head % Capacity;
		uint64 Slot = Slots[Idx].load(std::memory_order_acquire);

		if (Slot > TakenSlot && Slots[Idx].compare_exchange_strong(Slot, FreeSlot, std::memory_order_acq_rel))
		{
			Head--;
			checkSlow((Head + 1) % Capacity == Idx);
			return reinterpret_cast<FWorkBlock*>(Slot);
		}

		return nullptr;
	}

	// Called by other consumers, pops in FIFO order
	FWorkBlock* Steal()
	{
		while (true)
		{
			uint32 IdxVer = Tail.load(std::memory_order_acquire);
			uint32 Idx = IdxVer % Capacity;
			uint64 Slot = Slots[Idx].load(std::memory_order_acquire);

			if (Slot == FreeSlot)
			{
				return nullptr;
			}
			else if (Slot != TakenSlot && Slots[Idx].compare_exchange_weak(Slot, TakenSlot, std::memory_order_acq_rel))
			{
				if (IdxVer == Tail.load(std::memory_order_acquire))
				{
					uint32 Prev = Tail.fetch_add(1, std::memory_order_release);
					checkSlow(Prev % Capacity == Idx);
					Slots[Idx].store(FreeSlot, std::memory_order_release);
					return reinterpret_cast<FWorkBlock*>(Slot);
				}
				Slots[Idx].store(Slot, std::memory_order_release);
			}
		}
	}

	// Called after all producers and consumers have stopped working
	void CheckEmpty()
	{
		for (const AlignedAtomicUint64& Slot : Slots)
		{
			check(Slot.load() == FreeSlot);
		}
	}

private:
	struct alignas(PLATFORM_CACHE_LINE_SIZE) AlignedAtomicUint64 : public std::atomic<uint64> {};

	uint32 Head { ~0u };
	alignas(PLATFORM_CACHE_LINE_SIZE) std::atomic_uint32_t Tail { 0 };
	AlignedAtomicUint64 Slots[Capacity] = {};
};

// Unbounded SPMC queue where only a bounded number of items can be stolen
class FWorkstealingQueue
{
public:
	void Push(FWorkBlock* Block)
	{
		if (!SharedBlocks.Push(Block))
		{
			LocalBlocks.Push(Block);
		}
	}

	FWorkBlock* Pop()
	{
		int32 LocalNum = LocalBlocks.Num();
		if (LocalNum == 0)
		{
			return SharedBlocks.Pop();	
		}

		// Pop last block and try moving remaining blocks to shared queue
		FWorkBlock* Out = LocalBlocks[--LocalNum];
		while (LocalNum > 0 && SharedBlocks.Push(LocalBlocks[LocalNum - 1]))
		{
			--LocalNum;
		}
		LocalBlocks.SetNum(LocalNum, EAllowShrinking::No);

		return Out;
	}

	FWorkBlock* Steal()
	{
		return SharedBlocks.Steal();
	}

	void CheckEmpty()
	{
		SharedBlocks.CheckEmpty();
		check(LocalBlocks.IsEmpty());
	}

private:
	FBoundedWorkstealingQueue SharedBlocks;
	TArray<FWorkBlock*> LocalBlocks;
};

struct FWorkstealingManager
{
	FWorkstealingQueue Queues[MaxWorkers];

	FWorkBlock* Steal(FWorkstealingQueue* Workless)
	{
		PTRINT WorkerPtrDiff = Workless - Queues;
		checkf(WorkerPtrDiff >= 0 && WorkerPtrDiff < MaxWorkers, TEXT("FWorkstealingQueue not owned by manager"));
		int32 WorklessIdx = static_cast<int32>(WorkerPtrDiff);

		return TrySteal(WorklessIdx);
	}

	FWorkBlock* TrySteal(int32 WorklessIdx)
	{
		for (int32 Idx = WorklessIdx + 1; Idx < MaxWorkers; ++Idx)
		{
			if (FWorkBlock* Stolen = Queues[Idx].Steal())
			{
				return Stolen;
			}
		}

		for (int32 Idx = 0; Idx < WorklessIdx; ++Idx)
		{
			if (FWorkBlock* Stolen = Queues[Idx].Steal())
			{
				return Stolen;
			}
		}

		return nullptr;
	}
};

static FWorkstealingManager GWorkstealingManager;

//////////////////////////////////////////////////////////////////////////

FORCEINLINE_DEBUGGABLE void FWorkBlockifier::AllocateWipBlock()
{
	static_assert(sizeof(FWorkBlock) <= FPageAllocator::PageSize, "Block must fit in page");
	static_assert(sizeof(FWorkBlock) >= FPageAllocator::PageSize, "Implement thread local blocks-in-page allocator");
	
	Wip = new (GScratchPages.AllocatePage(WorkerIndex)) FWorkBlock;
	Wip->Previous = nullptr;
	WipIt = Wip->Objects;
}

void FWorkBlockifier::FreeOwningBlock(UObject*const* BlockObjects)
{
	if (BlockObjects)
	{
		FWorkBlock* Block = reinterpret_cast<FWorkBlock*>(UPTRINT(BlockObjects) - offsetof(FWorkBlock, Objects));
		check(UPTRINT(Block) % FPageAllocator::PageSize == 0);

		Block->~FWorkBlock();
		GScratchPages.ReturnWorkerPage(WorkerIndex, Block);
	}
}

FWorkBlockifier::~FWorkBlockifier()
{
	check(IsUnused());
	Wip->~FWorkBlock();
	GScratchPages.FreePage(Wip);
}

void FWorkBlockifier::ResetAsyncQueue()
{
	checkf(PartialNum() == 0, TEXT("Queue not empty"));
	SyncQueue = nullptr;
}

void FWorkBlockifier::PushFullBlockSync()
{
	PadBlock(*Wip);
	Wip->Previous = SyncQueue;
	SyncQueue = Wip;
	AllocateWipBlock();
}
void FWorkBlockifier::PushFullBlockAsync()
{
	PadBlock(*Wip);
	AsyncQueue->Push(Wip);
	AllocateWipBlock();
}

FWorkBlock* FWorkBlockifier::PopFullBlockSync()
{
	FWorkBlock* Out = SyncQueue;
	SyncQueue = Out ? Out->Previous : nullptr;
	return Out;
}

FWorkBlock* FWorkBlockifier::PopFullBlockAsync()
{
	return AsyncQueue->Pop();
}
	
FWorkBlock* FWorkBlockifier::PopWipBlock()
{
	PadObjects(WipIt[-1], MakeArrayView(WipIt, ObjectLookahead));
	FWorkBlock* Out = Wip;
	AllocateWipBlock();
	return Out;
}

FWorkBlock* FWorkBlockifier::StealAsyncBlock() const
{
	return GWorkstealingManager.Steal(AsyncQueue);
}

//////////////////////////////////////////////////////////////////////////

struct FAROBlock
{
	static constexpr uint32 NumWords = FPageAllocator::PageSize / sizeof(UObject*);
	static constexpr uint32 Capacity = NumWords - 1;

	uint32 FirstIndexInNextBlock;
	UObject* Objects[Capacity];
};

// Simple bounded lock-free SPMC LIFO queue used to schedule AddReferencedObjects calls
//
// * Producer steals in same order as consumer, simpler but less efficient
// * Popping is batched, in part to reduce producer-consumer tail popping contention
// * Producer consumes all entries before shutting down, so stealing can fail
// * ARO queues and blocks outlive producer and consumers to avoid lifetime problems
// * Bounded by fix number of pages shared between all queues
// * Last two points allow using ever increasing indices to side-step the ABA problem
class FAROQueue
{
public:
	struct FFew{};
	struct FMany{};
	template<class Quantity> static constexpr int NumPop = std::is_same_v<Quantity, FMany> ? 64 : 4;

	UE_NONCOPYABLE(FAROQueue);
	explicit FAROQueue(int32 WorkerIdx);
	~FAROQueue();

	[[nodiscard]] bool TryPush(UObject* Object);								// Called by the single producer thread
	template<class Quantity> [[nodiscard]] TArrayView<UObject*> Pop();		// Called by the single producer thread
	template<class Quantity> [[nodiscard]] TArrayView<UObject*> Steal();	// Called by other work-stealing threads 
	
private:
	alignas(PLATFORM_CACHE_LINE_SIZE) std::atomic<uint32> Head;
	FAROBlock* HeadBlock;
	const int32 WorkerIndex;
	alignas(PLATFORM_CACHE_LINE_SIZE) std::atomic<uint32> Tail;

	template<void(*Fence)(), uint32 NumWanted> TArrayView<UObject*> PopImpl();
};

class FAROQueueStore
{
public:
	[[nodiscard]] FAROBlock* AllocateBlock(int32 WorkerIdx, uint32& OutIdx)
	{
		static_assert(sizeof(FAROBlock) == FPageAllocator::PageSize);

		if (bFull.load(std::memory_order_relaxed))
		{
			return nullptr;
		}

		uint32 BlockIdx = NumBlocks.fetch_add(1, std::memory_order_acq_rel);
		if (BlockIdx >= MaxBlocks)
		{
			bFull.store(true, std::memory_order_relaxed);
			NumBlocks.fetch_sub(1, std::memory_order_acq_rel);
			return nullptr;
		}

		check(Blocks[BlockIdx] == nullptr);

		OutIdx = BlockIdx * FAROBlock::NumWords;
		FAROBlock* Block = new (GScratchPages.AllocatePage(WorkerIdx)) FAROBlock;
		Blocks[BlockIdx] = Block;
		return Block;
	}

	UObject*& operator[](uint32 Idx)
	{
		check(Idx / FAROBlock::NumWords < MaxBlocks);
		check(Idx % FAROBlock::NumWords < FAROBlock::Capacity);
		check(Blocks[Idx / FAROBlock::NumWords]);

		return Blocks[Idx / FAROBlock::NumWords]->Objects[Idx % FAROBlock::NumWords];
	}

	uint32 GetFirstIndexInNextBlock(uint32 Idx)
	{
		check(Idx / FAROBlock::NumWords < MaxBlocks);
		check(Idx % FAROBlock::NumWords < FAROBlock::Capacity);
		check(Blocks[Idx / FAROBlock::NumWords]);
		check(Blocks[Idx / FAROBlock::NumWords]->FirstIndexInNextBlock % FAROBlock::NumWords == 0);
		check(Blocks[Idx / FAROBlock::NumWords]->FirstIndexInNextBlock / FAROBlock::NumWords < MaxBlocks);

		return Blocks[Idx / FAROBlock::NumWords]->FirstIndexInNextBlock;
	}

	void ReturnAllBlocks()
	{
		check(NumBlocks.load() <= MaxBlocks);

		for (FAROBlock*& Block : MakeArrayView(Blocks, NumBlocks.load()))
		{
			Block->~FAROBlock();
			GScratchPages.ReturnSharedPage(Block);
			Block = nullptr;
		}

		NumBlocks.store(0);
		bFull.store(false);
	}

	static constexpr uint32 MaxBlocks = 256;
private:
	std::atomic<uint32> NumBlocks{0};
	std::atomic_bool bFull{false};
	alignas(PLATFORM_CACHE_LINE_SIZE) FAROBlock* Blocks[MaxBlocks] = {};
};

static FAROQueueStore GAROBlocks;

FAROQueue::FAROQueue(int32 WorkerIdx)
: WorkerIndex(WorkerIdx)
{
	uint32 HeadIdx;
	HeadBlock = GAROBlocks.AllocateBlock(WorkerIdx, /* out */ HeadIdx);
	checkf(HeadBlock, TEXT("One head block per worker must exist during worker setup, "
			"the assumption MaxWorkers * slow AROs <= MaxBlocks %s with %d slow AROs registered."),
			(MaxWorkers * GetNumSlowAROs() <= FAROQueueStore::MaxBlocks) ? TEXT("held") : TEXT("failed"), GetNumSlowAROs());
	HeadBlock->FirstIndexInNextBlock = ~0u;
	Head.store(HeadIdx, std::memory_order_relaxed);
	Tail.store(HeadIdx, std::memory_order_relaxed);
}

FAROQueue::~FAROQueue()
{
	checkf(Head.load() == Tail.load(), TEXT("Failed to flush ARO calls"));
	// HeadBlock is returned by GAROBlocks.FreeAllBlocks()
}

FORCEINLINE_DEBUGGABLE bool FAROQueue::TryPush(UObject* Object)
{
	uint32 HeadIdx = Head.load(std::memory_order_relaxed);
	uint32 ObjectsIdx = HeadIdx % FAROBlock::NumWords;
	CA_ASSUME(ObjectsIdx < FAROBlock::Capacity);
	HeadBlock->Objects[ObjectsIdx] = Object;
	++HeadIdx;

	// Must store OldBlock->FirstIndexInNextBlock before Head.store
	if (HeadIdx % FAROBlock::NumWords == FAROBlock::Capacity)
	{
		FAROBlock* OldBlock = HeadBlock;
		FAROBlock* NewBlock = GAROBlocks.AllocateBlock(WorkerIndex, /* out */ HeadIdx);
		if (!NewBlock)
		{
			// Head won't move so HeadBlock->Objects[HeadIdx % FAROBlock::NumWords] won't be read
			return false;
		}
		
		HeadBlock = NewBlock;
		OldBlock->FirstIndexInNextBlock = HeadIdx;
	}

	// Synchronizes non-atomic Blocks, Object and FirstIndexInNextBlock stores with work-stealing AcquireFence
	Head.store(HeadIdx, std::memory_order_release);

	return true;
}

static FORCEINLINE void AcquireFence() { std::atomic_thread_fence(std::memory_order_acquire); }
static FORCEINLINE void NoFence() {}

template<void(*Fence)(), uint32 NumWanted>
FORCEINLINE_DEBUGGABLE
TArrayView<UObject*> FAROQueue::PopImpl()
{
	check(NumWanted < FAROBlock::Capacity);

	while (true)
	{
		const uint32 HeadNow = Head.load(std::memory_order_relaxed);
		const uint32 TailNow = Tail.load(std::memory_order_relaxed);

		if (TailNow >= HeadNow)
		{
			return {};
		}

		// Can only pop up to end of tail block
		static constexpr uint32 PageIndexMask = ~(FAROBlock::NumWords - 1);
		const uint32 LastInTailBlock = (TailNow & PageIndexMask) + FAROBlock::Capacity - 1;
		const uint32 WantedTail = FMath::Min(HeadNow, TailNow + NumWanted);

		// Atomic-fence synchronization of non-atomic stores via relaxed Head load
		Fence();

		// This is a temporary solution to make TSAN see the fence until the global fence
		// can be removed the next time we do a proper profiling pass of these bits on all platforms.
		if constexpr (USING_THREAD_SANITISER && Fence == AcquireFence)
		{
			Head.load(std::memory_order_acquire);
		}

		UObject** TailData = &GAROBlocks[TailNow];
		uint32 ExpectedTail = TailNow;
		if (WantedTail <= LastInTailBlock)
		{
			if (Tail.compare_exchange_weak(ExpectedTail, WantedTail))
			{
				return MakeArrayView(TailData, WantedTail - TailNow);
			}
		}
		else
		{
			if (Tail.compare_exchange_weak(ExpectedTail, GAROBlocks.GetFirstIndexInNextBlock(TailNow)))
			{
				return MakeArrayView(TailData, FAROBlock::Capacity - (TailNow % FAROBlock::NumWords));
			}			
		}
	}
}

template<class Quantity> FORCEINLINE TArrayView<UObject*> FAROQueue::Pop()		{ return PopImpl<NoFence,		NumPop<Quantity>>(); }
template<class Quantity> FORCEINLINE TArrayView<UObject*> FAROQueue::Steal()	{ return PopImpl<AcquireFence,	NumPop<Quantity>>(); }

//////////////////////////////////////////////////////////////////////////

class FSlowAROManager
{
public:
	void RegisterImplementation(ObjectAROFn ARO, EAROFlags Flags)
	{
		check(IsInGameThread());
		check(AROs.Find(ARO) == INDEX_NONE);
		checkf(AROs.Num() < Capacity, TEXT("Don't register this many slow AROs, memory consumption and work-stealing time increases linearly"));

		int32 Idx = AROs.Num();
		AROs.Add(ARO);
		if (EnumHasAllFlags(Flags, EAROFlags::Unbalanced))
		{
			UnbalancedAROIndices.Add(Idx);
		}

		static_assert(Capacity <= sizeof(ExtraSlowAROs) * 8);
		ExtraSlowAROs |= decltype(ExtraSlowAROs)(EnumHasAllFlags(Flags, EAROFlags::ExtraSlow)) << Idx;
	}

	int32 FindImplementation(ObjectAROFn ARO) const
	{
		return AROs.Find(ARO);
	}
	
	void SetupWorkerQueues(int32 InNumWorkers)
	{
		check(AllQueues.IsEmpty());
		NumWorkers = InNumWorkers;
		AllQueues.SetNumUninitialized(NumWorkers * AROs.Num());
		for (int32 AROIdx = 0, NumAROs = AROs.Num(); AROIdx < NumAROs; ++AROIdx)
		{
			for (int32 WorkerIdx = 0; WorkerIdx < NumWorkers; ++WorkerIdx)
			{
				new (&GetQueue(AROIdx, WorkerIdx)) FAROQueue(WorkerIdx);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void CallSync(int32 AROIdx, UObject* Object, FReferenceCollector& Collector)
	{
		AROs[AROIdx](Object, Collector);
	}

	[[nodiscard]] FORCEINLINE_DEBUGGABLE bool QueueCall(int32 AROIdx, int32 WorkerIdx, UObject* Object)
	{
		return GetQueue(AROIdx, WorkerIdx).TryPush(Object);
	}

	FORCEINLINE_DEBUGGABLE void DrainLocalUnbalancedQueues(FWorkerContext& Context, FReferenceCollector& Collector)
	{
		const int32 WorkerIdx = Context.GetWorkerIndex();
		for (uint32 AROIdx : UnbalancedAROIndices)
		{
			ObjectAROFn ARO = AROs[AROIdx];
			FAROQueue& Queue = GetQueue(AROIdx, WorkerIdx);
			for (TArrayView<UObject*> Objects = Queue.Pop<FAROQueue::FMany>(); Objects.Num(); Objects = Queue.Pop<FAROQueue::FMany>())
			{
				for (UObject* Object : Objects)
				{
					Context.ReferencingObject = Object;
					ARO(Object, Collector);
				}
			}
		}
	}

	bool ProcessAllQueues(FWorkerContext& Context, FReferenceCollector& Collector)
	{
		// Drain all queues for one specific slow ARO type at a time and stop if any ARO calls were made
		// Workers start with different ARO types to reach the slowest ARO type sooner and reduce stealing contention
		
		bool bStop = false;
		uint32 NumCalls = 0;
		const uint32 NumAROs = (uint32)AROs.Num();
		
		const double StopTime = FPlatformTime::Seconds() + 0.1/1000;
		for (uint32 OffsetIdx = (uint32)Context.GetWorkerIndex() % NumAROs, EndOffsetIdx = OffsetIdx + NumAROs; OffsetIdx < EndOffsetIdx && !bStop; ++OffsetIdx)
		{
			int32 AROIdx = OffsetIdx - (OffsetIdx >= NumAROs ? NumAROs : 0);
			TArrayView<FAROQueue> WorkerQueues = MakeArrayView(&GetQueue(AROIdx, 0), NumWorkers);
			bStop = IsExtraSlow(AROIdx)	? ProcessQueues<FAROQueue::FFew >(Context, WorkerQueues, AROs[AROIdx], Collector, NumCalls)
										: ProcessQueues<FAROQueue::FMany>(Context, WorkerQueues, AROs[AROIdx], Collector, NumCalls);

			// Don't proceed to next ARO type if we did some work that took a considerable amount of time
			bStop = bStop || (NumCalls > 0 && FPlatformTime::Seconds() > StopTime);
		}

		return NumCalls > 0;
	}

	void ResetWorkerQueues()
	{
		AllQueues.Reset();
		GAROBlocks.ReturnAllBlocks();
		NumWorkers = 0;
	}

	int32 NumAROs() const { return AROs.Num(); }

private:
	static constexpr int32 Capacity = 8;
	uint32 ExtraSlowAROs = 0;
	int32 NumWorkers = 0;
	TArray<FAROQueue> AllQueues;
	TArray<int32, TFixedAllocator<2>> UnbalancedAROIndices;
	TArray<ObjectAROFn, TFixedAllocator<Capacity>> AROs;

	FAROQueue& GetQueue(int32 AROIdx, int32 WorkerIdx)
	{
		check(WorkerIdx < NumWorkers);	
		check(AROIdx < AROs.Num());
		return AllQueues[AROIdx * NumWorkers + WorkerIdx];
	}

	// @return if processing should stop since NumCalls reached CallLimit
	template<class Quantity>
	static bool ProcessQueues(FWorkerContext& Context, TArrayView<FAROQueue> WorkerQueues, ObjectAROFn ARO, FReferenceCollector& Collector, uint32& NumCalls)
	{
		static constexpr uint32 CallLimit = FAROQueue::NumPop<Quantity>;
		const int32 WorkerIdx = Context.GetWorkerIndex();

		return	ProcessQueuesUsing<&FAROQueue::Pop<Quantity>,	CallLimit>(Context, WorkerQueues.Slice(WorkerIdx, 1),		ARO, Collector, NumCalls) ||
				ProcessQueuesUsing<&FAROQueue::Steal<Quantity>,	CallLimit>(Context, WorkerQueues.RightChop(WorkerIdx + 1),	ARO, Collector, NumCalls) ||
				ProcessQueuesUsing<&FAROQueue::Steal<Quantity>,	CallLimit>(Context, WorkerQueues.Slice(0, WorkerIdx),		ARO, Collector, NumCalls);
	}

	// @return if processing should stop since NumCalls reached or exceeded CallLimit
	template<TArrayView<UObject*>(FAROQueue::*PopFunc)(), uint32 CallLimit>
	FORCEINLINE_DEBUGGABLE static bool ProcessQueuesUsing(FWorkerContext& Context, TArrayView<FAROQueue> Queues, ObjectAROFn ARO, FReferenceCollector& Collector, uint32& NumCalls)
	{
		for (FAROQueue& Queue : Queues)
		{
			for (TArrayView<UObject*> Objects = (Queue.*PopFunc)(); Objects.Num(); Objects = (Queue.*PopFunc)())
			{
				for (UObject* Object : Objects)
				{
					Context.ReferencingObject = Object;
					ARO(Object, Collector);
				}

				NumCalls += Objects.Num();
				if (NumCalls >= CallLimit)
				{
					return true;
				}
			}
		}

		return false;
	}

	FORCEINLINE bool IsExtraSlow(int32 AROIdx) const { return !!((ExtraSlowAROs >> AROIdx) & 1u); }
};


template<class T>
class TSingleton
{
	// Leave all data uninitialized
	bool bValid;
	alignas(T) uint8 Data[sizeof(T)];

	T& Cast() { return *reinterpret_cast<T*>(Data); }
	
public:
	T& Get()
	{
		struct FLifetime
		{
			TSingleton& Singleton;

			FLifetime(TSingleton& S)
			: Singleton(S)
			{
				check(!Singleton.bValid);
				new (Singleton.Data) T;
				Singleton.bValid = true;
			}

			~FLifetime()
			{
				Singleton.bValid = false;
				Singleton.Cast().~T();
			}
		};
		static FLifetime FunctionStaticLifetime(*this);

		return Cast();
	}

	T& GetPostInit()
	{
		checkf(bValid, TEXT("Must call Get() at least once first"));
		return Cast();
	}
};

static TSingleton<FSlowAROManager> GSlowARO;


void FSlowARO::CallSync(uint32 SlowAROIndex, UObject* Object, FReferenceCollector& Collector)
{
	GSlowARO.GetPostInit().CallSync(SlowAROIndex, Object, Collector);
}

bool FSlowARO::TryQueueCall(uint32 SlowAROIndex, UObject* Object, FWorkerContext& Context)
{
	check(Object);
	check(Object->GetClass());
	checkf(SlowAROIndex < uint32(GSlowARO.GetPostInit().NumAROs()), TEXT("SlowAROIndex out of bounds %d/%d"), SlowAROIndex, GSlowARO.GetPostInit().NumAROs());
	checkf(SlowAROIndex == GSlowARO.GetPostInit().FindImplementation(Object->GetClass()->CppClassStaticFunctions.GetAddReferencedObjects()),
		TEXT("Queueing a '%s' named '%s' for slow AddReferenceObjects call, but the class or ARO doesn't match. "
			 "Slow ARO index from schema is %d but found index %d."),
		*Object->GetClass()->GetFName().ToString(), *Object->GetFName().ToString(),
		SlowAROIndex, GSlowARO.GetPostInit().FindImplementation(Object->GetClass()->CppClassStaticFunctions.GetAddReferencedObjects()));

	return GSlowARO.GetPostInit().QueueCall(SlowAROIndex, Context.GetWorkerIndex(), Object);
}

void FSlowARO::ProcessUnbalancedCalls(FWorkerContext& Context, FReferenceCollector& Collector)
{
	GSlowARO.GetPostInit().DrainLocalUnbalancedQueues(Context, Collector);
}
bool FSlowARO::ProcessAllCalls(FWorkerContext& Context, FReferenceCollector& Collector)
{
	return GSlowARO.GetPostInit().ProcessAllQueues(Context, Collector);
}

void RegisterSlowImplementation(ObjectAROFn ARO, EAROFlags Flags)
{
	GSlowARO.Get().RegisterImplementation(ARO, Flags);
}

int32 FindSlowImplementation(ObjectAROFn ARO)
{
	return GSlowARO.GetPostInit().FindImplementation(ARO);
}

int32 GetNumSlowAROs()
{
	return GSlowARO.GetPostInit().NumAROs();
}

//////////////////////////////////////////////////////////////////////////

// Helps identify code locations that kill references
FORCEINLINE_DEBUGGABLE void KillReference(UObject*& Object) { Object = nullptr; }

template <EGCOptions Options>
constexpr FORCEINLINE EKillable MayKill(EOrigin Origin, bool bAllowKill)
{
	// To avoid content changes, allow reference elimination inside of Blueprints
	return (bAllowKill & (IsEliminatingGarbage(Options) || Origin == EOrigin::Blueprint)) ? EKillable::Yes : EKillable::No;
}

FReachabilityAnalysisState GReachabilityState;

template <EGCOptions InOptions>
class TReachabilityProcessor 
{
public:
	FORCEINLINE void BeginTimingObject(UObject* CurrentObject)
	{
		UE::GC::GDetailedStats.BeginTimingObject(CurrentObject);
	}

	FORCEINLINE void UpdateDetailedStats(UObject* CurrentObject)
	{
		UE::GC::GDetailedStats.UpdateDetailedStats(CurrentObject);
	}

	FORCEINLINE void LogDetailedStatsSummary()
	{
		UE::GC::GDetailedStats.LogDetailedStatsSummary();
	}

	FORCEINLINE bool IsTimeLimitExceeded() const
	{
		return IsWithIncrementalReachabilityAnalysis() && GReachabilityState.IsTimeLimitExceeded();
	}

	static constexpr EGCOptions Options = InOptions;

	static constexpr FORCEINLINE bool IsEliminatingGarbage() {	return !!(Options & EGCOptions::EliminateGarbage);}

	static constexpr EInternalObjectFlags KillFlag = EInternalObjectFlags::Garbage;

	constexpr static FORCEINLINE EKillable MayKill(EOrigin Origin, bool bAllowKill) { return UE::GC::MayKill<Options>(Origin, bAllowKill); }	

	static constexpr FORCEINLINE bool IsWithIncrementalReachabilityAnalysis() { return !!(Options & EGCOptions::IncrementalReachability); }

	static FORCEINLINE void ProcessReferenceDirectly(FWorkerContext& Context, FPermanentObjectPoolExtents PermanentPool, const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId, EKillable Killable)
	{
		return Killable == EKillable::No	? ProcessReferenceDirectly<EKillable::No >(Context, PermanentPool, ReferencingObject, Object, MemberId)
											: ProcessReferenceDirectly<EKillable::Yes>(Context, PermanentPool, ReferencingObject, Object, MemberId);
	}

	static FORCEINLINE void DetectGarbageReference(FWorkerContext& Context, FReferenceMetadata Metadata)
	{
		Context.Stats.TrackPotentialGarbageReference(!IsEliminatingGarbage() && Metadata.Has(KillFlag));
	}

	/**
	 * Handles object reference, potentially NULL'ing
	 *
	 * @param Object						Object pointer passed by reference
	 * @param ReferencingObject UObject which owns the reference (can be NULL)
	 * @param bAllowReferenceElimination	Whether to allow NULL'ing the reference if RF_PendingKill is set
	*/
	template<EKillable Killable>
	static FORCEINLINE_DEBUGGABLE void ProcessReferenceDirectly(FWorkerContext& Context, FPermanentObjectPoolExtents PermanentPool, const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId)
	{
		UE::GC::GDetailedStats.IncreaseObjectRefStats(Object);
		if (ValidateReference(Object, PermanentPool, FReferenceToken(ReferencingObject), MemberId))
		{
			const int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
			FImmutableReference Reference = {Object};	
			FReferenceMetadata Metadata(ObjectIndex);
			bool bKillable = Killable == EKillable::Yes;
			if (Metadata.Has(KillFlag) & bKillable) //-V792
			{
				check(ReferencingObject || IsEliminatingGarbage());
				checkSlow(Metadata.ObjectItem->GetOwnerIndex() <= 0);
				KillReference(Object);
				return;
			}
			
			DetectGarbageReference(Context, Metadata);
			HandleValidReference(Context, /*ReferencingObject, */Reference, Metadata);
		}
	}

	FORCEINLINE static void HandleBatchedReference(FWorkerContext& Context, FResolvedMutableReference Reference, FReferenceMetadata Metadata)
	{
		UE::GC::GDetailedStats.IncreaseObjectRefStats(GetObject(Reference));
		if (Metadata.Has(KillFlag))
		{
			checkSlow(Metadata.ObjectItem->GetOwnerIndex() <= 0);
			KillReference(*Reference.Mutable);
		}
		else
		{
			HandleValidReference(Context, Reference, Metadata);
		}
	}

	FORCEINLINE static void HandleBatchedReference(FWorkerContext& Context, FImmutableReference Reference, FReferenceMetadata Metadata)
	{
		UE::GC::GDetailedStats.IncreaseObjectRefStats(GetObject(Reference));
		DetectGarbageReference(Context, Metadata);
		HandleValidReference(Context, Reference, Metadata);
	}

	FORCEINLINE static bool HandleValidReference(FWorkerContext& Context, FImmutableReference Reference, FReferenceMetadata Metadata)
	{
		if (Metadata.ObjectItem->MarkAsReachableInterlocked_ForGC())
		{
			// Objects that are part of a GC cluster should never have the unreachable flag set!
			checkSlow(Metadata.ObjectItem->GetOwnerIndex() <= 0);

			if (!Metadata.Has(EInternalObjectFlags::ClusterRoot))
			{
				// Add it to the list of objects to serialize.
				Context.ObjectsToSerialize.Add<Options>(Reference.Object);
			}
			else
			{
				// This is a cluster root reference so mark all referenced clusters as reachable
				MarkReferencedClustersAsReachableThunk<Options>(Metadata.ObjectItem->GetClusterIndex(), Context.ObjectsToSerialize);
			}

			return true;
		}
		else
		{
			if ((Metadata.ObjectItem->GetOwnerIndex() > 0) & !Metadata.Has(EInternalObjectFlags::ReachableInCluster))
			{
				// Make sure cluster root object is reachable too
				FUObjectItem* RootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(Metadata.ObjectItem->GetOwnerIndex());
				checkSlow(RootObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));

				if (Metadata.ObjectItem->ThisThreadAtomicallySetFlag_ForGC(EInternalObjectFlags::ReachableInCluster))
				{
					if (RootObjectItem->MarkAsReachableInterlocked_ForGC())
					{
						// Make sure all referenced clusters are marked as reachable too
						MarkReferencedClustersAsReachableThunk<Options>(RootObjectItem->GetClusterIndex(), Context.ObjectsToSerialize);
					}
				}
			}
		}

		return false;
	}
};

//////////////////////////////////////////////////////////////////////////

/** Batches up references before dispatching them to the processor, unlike TDirectDispatcher */
template <class ProcessorType>
struct TBatchDispatcher
{
	static constexpr bool bBatching = true;
	static constexpr bool bParallel = IsParallel(ProcessorType::Options);

	typedef FDebugSchemaStackNoOpScope SchemaStackScopeType;

	FWorkerContext& Context;
	const FPermanentObjectPoolExtents PermanentPool;
	FReferenceCollector& Collector;
	TReferenceBatcher<FMutableReference, FResolvedMutableReference, ProcessorType> KillableBatcher;
	TReferenceBatcher<FImmutableReference, FImmutableReference, ProcessorType> ImmutableBatcher;
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	Verse::FMarkStack VerseGCMarkStack;
#endif
	FStructBatcher StructBatcher;

	UE_NONCOPYABLE(TBatchDispatcher);
	explicit TBatchDispatcher(FWorkerContext& InContext, FReferenceCollector& InCollector)
	: Context(InContext)
	, Collector(InCollector)
	, KillableBatcher(InContext)
	, ImmutableBatcher(InContext)
	, StructBatcher(InContext)
	{}

	// ProcessObjectArray API

	FORCENOINLINE void HandleReferenceDirectly(const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId, EKillable Killable)
	{
		ProcessorType::ProcessReferenceDirectly(Context, PermanentPool, ReferencingObject, Object, MemberId, Killable);
		Context.Stats.AddReferences(1);
	}

	FORCEINLINE void HandleReferenceDirectly(const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId, EOrigin Origin, bool bAllowReferenceElimination)
	{
		HandleReferenceDirectly(ReferencingObject, Object, MemberId, ProcessorType::MayKill(Origin, bAllowReferenceElimination));
	}

	FORCEINLINE_DEBUGGABLE void HandleKillableReference(UObject*& Object, FMemberId MemberId, EOrigin Origin)
	{
		QueueReference(Context.GetReferencingObject(), Object, MemberId, ProcessorType::MayKill(Origin, true));
	}

	FORCEINLINE void HandleImmutableReference(UObject* Object, FMemberId MemberId, EOrigin Origin)
	{
		ImmutableBatcher.PushReference(FImmutableReference{Object});
	}

	template<class ArrayType>
	FORCEINLINE void HandleKillableArray(ArrayType&& Array, FMemberId MemberId, EOrigin Origin)
	{
		QueueArray(Context.GetReferencingObject(), (ArrayType&&)Array, MemberId, ProcessorType::MayKill(Origin, true));
	}

	FORCEINLINE void HandleKillableReferences(TArrayView<UObject*> Objects, FMemberId MemberId, EOrigin Origin)
	{
		QueueReferences(Context.GetReferencingObject(), Objects, MemberId, ProcessorType::MayKill(Origin, true));
	}

	FORCEINLINE void HandleWeakReference(FWeakObjectPtr& WeakPtr, const UObject* ReferencingObject, FMemberId MemberId, EMemberType)
	{
		UObject* WeakObject = WeakPtr.Get(true);
		HandleReferenceDirectly(ReferencingObject, WeakObject, MemberId, EKillable::No);
	}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	FORCEINLINE_DEBUGGABLE void HandleVerseValueDirectly(UObject* ReferencingObject, Verse::VValue Value, FMemberId MemberId, EOrigin Origin)
	{
		if (Verse::VCell* Cell = Value.ExtractCell())
		{
			VerseGCMarkStack.Mark(Cell);
			Context.Stats.AddVerseCells(1);
		}
		else if (Value.IsUObject())
		{
			HandleImmutableReference(Value.AsUObject(), MemberId, Origin);
		}
	}

	FORCEINLINE_DEBUGGABLE void HandleVerseValue(Verse::VValue Value, FMemberId MemberId, EOrigin Origin)
	{
		HandleVerseValueDirectly(Context.GetReferencingObject(), Value, MemberId, Origin);
	}

	FORCEINLINE void HandleVerseValueArray(TArrayView<Verse::VValue> Values, FMemberId MemberId, EOrigin Origin)
	{
		for (Verse::VValue Value : Values)
		{
			HandleVerseValueDirectly(Context.GetReferencingObject(), Value, MemberId, Origin);
		}
	}

	FORCEINLINE_DEBUGGABLE void FlushWork()
	{
		if (GIsFrankenGCCollecting)
		{
			Verse::FHeap::AddExternalMarkStack(MoveTemp(VerseGCMarkStack));
		}
	}
#endif

	FORCENOINLINE void FlushQueuedReferences()
	{
		KillableBatcher.FlushQueues();
		ImmutableBatcher.FlushQueues();
	}

	FORCENOINLINE bool FlushToStructBlocks()
	{
		StructBatcher.FlushBoundedQueues();
		return StructBatcher.GetUnboundedQueue().CanPop();
	}

	FORCEINLINE_DEBUGGABLE FStructBlockifier& GetStructBlocks()
	{
		return StructBatcher.GetUnboundedQueue();
	}

	FORCEINLINE_DEBUGGABLE void QueueStructArray(FSchemaView Schema, uint8* Data, int32 Num)
	{
		StructBatcher.PushStructArray(Schema, Data, Num);
	}

	FORCEINLINE_DEBUGGABLE void QueueSparseStructArray(FSchemaView Schema, FScriptSparseArray& Array)
	{
		StructBatcher.PushSparseStructArray(Schema, Array);
	}

	// Collector API

	FORCEINLINE_DEBUGGABLE void QueueReference(const UObject* ReferencingObject,  UObject*& Object, FMemberId MemberId, EKillable Killable)
	{
		if (Killable == EKillable::Yes)
		{
			FPlatformMisc::Prefetch(&Object);
			KillableBatcher.PushReference(FMutableReference{&Object});
		}
		else
		{
			ImmutableBatcher.PushReference(FImmutableReference{Object});
		}
	}

	FORCEINLINE_DEBUGGABLE void QueueReferences(const UObject* ReferencingObject, TArrayView<UObject*> References, FMemberId MemberId, EKillable Killable)
	{
		if (Killable == EKillable::Yes)
		{
			KillableBatcher.PushReferenceView(References);
		}
		else
		{
			ImmutableBatcher.PushReferenceView(References);
		}
	}
	
	template<class ArrayType>
	FORCEINLINE_DEBUGGABLE void QueueArray(const UObject* ReferencingObject, ArrayType&& Array, FMemberId MemberId, EKillable Killable)
	{
		if (Killable == EKillable::Yes)
		{
			KillableBatcher.PushArray((ArrayType&&)Array);
		}
		else
		{
			ImmutableBatcher.PushArray((ArrayType&&)Array);
		}
	}

	FORCEINLINE_DEBUGGABLE void QueueSet(const UObject* ReferencingObject, TSet<UObject*>& Objects, FMemberId MemberId, EKillable Killable)
	{
		if (Killable == EKillable::Yes)
		{
			for (UObject*& Object : Objects)
			{
				QueueReference(ReferencingObject, Object, MemberId, EKillable::Yes);
			}
		}
		else
		{
			for (UObject*& Object : Objects)
			{
				QueueReference(ReferencingObject, Object, MemberId, EKillable::No);
			}
		}
	}

	void Suspend()
	{
		FlushQueuedReferences();
		Context.IncrementalStructs = StructBatcher.Suspend();
	}

	void SetDebugSchemaStackMemberId(FMemberId Member)
	{
	}
};

//////////////////////////////////////////////////////////////////////////

#if !UE_BUILD_SHIPPING
FORCEINLINE static TArray<FGCDirectReference>& GetContextHistoryReferences(FWorkerContext& Context, FReferenceToken Referencer, uint32 Reserve)
{
	TArray<FGCDirectReference>*& DirectReferences = Context.History.FindOrAdd(Referencer);
	if (!DirectReferences)
	{
		DirectReferences = new TArray<FGCDirectReference>();
		if (Reserve > 0)
		{
			DirectReferences->Reserve(Reserve);
		}
	}
	return *DirectReferences;
}

FORCEINLINE static FName GetReferencerName(const UObject* Referencer, FMemberId MemberId)
{
	if (Referencer != FGCObject::GGCObjectReferencer)
	{
		return GetMemberDebugInfo(Referencer->GetClass()->ReferenceSchema.Get(), MemberId).Name;
	}
	else if (FGCObject::GGCObjectReferencer == Referencer)
	{
		FGCObject* SerializingObj = FGCObject::GGCObjectReferencer->GetCurrentlySerializingObject();
		if (SerializingObj)
		{
			return FName(*SerializingObj->GetReferencerName());
		}
		else
		{
			static const FName NAME_Unknown = FName(TEXT("Unknown"));
			return NAME_Unknown;
		}
	}
	return NAME_None;
}
#endif

//////////////////////////////////////////////////////////////////////////

#if !UE_BUILD_SHIPPING && (WITH_VERSE_VM || defined(__INTELLISENSE__))
template <EGCOptions Options>
struct TVerseDebugReachabilityVisitor : public Verse::FAbstractVisitor
{
	UE_NONCOPYABLE(TVerseDebugReachabilityVisitor);

	TVerseDebugReachabilityVisitor(FWorkerContext& InContext, bool bInTrackHistory, const FPermanentObjectPoolExtents& InPermanentPool, Verse::FMarkStack& InMarkStack)
		: Context(InContext)
		, PermanentPool(InPermanentPool)
		, MarkStack(InMarkStack)
		, bTrackHistory(bInTrackHistory)
	{
	}

	virtual void VisitNonNull(Verse::VCell*& InCell, const TCHAR* ElementName) override
	{
		Context.Stats.AddVerseCells(1);
		if (MarkStack.TryMarkNonNull(InCell) && bTrackHistory)
		{
			Verse::FAbstractVisitor::FReferrerContext* VisitorContext = GetContext();
			if (VisitorContext != nullptr && VisitorContext->GetReferrer().IsCell())
			{
				const Verse::VCell* Referencer = VisitorContext->GetReferrer().AsCell();
				GetContextHistoryReferences(Context, FReferenceToken(Referencer), 0).Add(FGCDirectReference(InCell, FName(ElementName)));
			}
		}
	}

	virtual void VisitNonNull(UObject*& InObject, const TCHAR* ElementName) override
	{
		UE::GC::GDetailedStats.IncreaseObjectRefStats(InObject);
		Verse::FAbstractVisitor::FReferrerContext* VisitorContext = GetContext();
		const Verse::VCell* Referencer = VisitorContext != nullptr && VisitorContext->GetReferrer().IsCell() ? VisitorContext->GetReferrer().AsCell() : nullptr;
		if (ValidateReference(InObject, PermanentPool, FReferenceToken(Referencer), FMemberId(0)))
		{
			FReferenceMetadata Metadata(GUObjectArray.ObjectToIndex(InObject));
			bool bReachedFirst = TReachabilityProcessor<Options>::HandleValidReference(Context, FImmutableReference{ InObject }, Metadata);
			if (bReachedFirst && bTrackHistory)
			{
				if (Referencer != nullptr)
				{
					GetContextHistoryReferences(Context, FReferenceToken(Referencer), 0).Add(FGCDirectReference(InObject, FName(ElementName)));
				}
			}
		}
	}

private:
	FWorkerContext& Context;
	const FPermanentObjectPoolExtents& PermanentPool;
	Verse::FMarkStack& MarkStack;
	bool bTrackHistory;
};
#endif

//////////////////////////////////////////////////////////////////////////

template <EGCOptions Options>
class TReachabilityCollectorBase : public FReferenceCollector
{
protected:
	bool bAllowEliminatingReferences = true;
	EOrigin CurrentOrigin = EOrigin::Other;

	FORCEINLINE EKillable MayKill() const { return UE::GC::MayKill<Options>(CurrentOrigin, bAllowEliminatingReferences); }

public:
	virtual bool IsIgnoringArchetypeRef() const override final { return false;}
	virtual bool IsIgnoringTransient() const override final { return false; }
	virtual bool NeedsInitialReferences() const override final { return !IsParallel(Options); }
	virtual void AllowEliminatingReferences(bool bAllow) override final { bAllowEliminatingReferences = bAllow; }

	virtual void SetIsProcessingNativeReferences(bool bIsNative) override final
	{
		CurrentOrigin = bIsNative ? EOrigin::Other : EOrigin::Blueprint;
	}

	virtual bool NeedsPropertyReferencer() const override final { return ENABLE_GC_OBJECT_CHECKS; }
};

template <EGCOptions Options>
class TReachabilityCollector final : public TReachabilityCollectorBase<Options>
{
	using ProcessorType = TReachabilityProcessor<Options>;
	using DispatcherType = TBatchDispatcher<ProcessorType>;
	using Super =  TReachabilityCollectorBase<Options>;
	using Super::MayKill;

	DispatcherType Dispatcher;

	friend DispatcherType& GetDispatcher(TReachabilityCollector<Options>& Collector, ProcessorType&, FWorkerContext&) { return Collector.Dispatcher; }

public:
	TReachabilityCollector(ProcessorType&, FWorkerContext& Context) : Dispatcher(Context, *this) {}
		
	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override;
	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override;

	virtual void AddStableReference(UObject** Object) override
	{
		Dispatcher.QueueReference(Dispatcher.Context.GetReferencingObject(), *Object, EMemberlessId::Collector, MayKill());
	}
	
	virtual void AddStableReferenceArray(TArray<UObject*>* Objects) override
	{
		Dispatcher.QueueArray(Dispatcher.Context.GetReferencingObject(), *Objects, EMemberlessId::Collector, MayKill());
	}

	virtual void AddStableReferenceSet(TSet<UObject*>* Objects) override
	{
		Dispatcher.QueueSet(Dispatcher.Context.GetReferencingObject(), *Objects, EMemberlessId::Collector, MayKill());
	}

	virtual void AddStableReference(TObjectPtr<UObject>* Object) override
	{
		Dispatcher.QueueReference(Dispatcher.Context.GetReferencingObject(), UE::Core::Private::Unsafe::Decay(*Object), ETokenlessId::Collector, MayKill());
	}
	
	virtual void AddStableReferenceArray(TArray<TObjectPtr<UObject>>* Objects) override
	{
		Dispatcher.QueueArray(Dispatcher.Context.GetReferencingObject(), UE::Core::Private::Unsafe::Decay(*Objects), ETokenlessId::Collector, MayKill());
	}

	virtual void AddStableReferenceSet(TSet<TObjectPtr<UObject>>* Objects) override
	{

		Dispatcher.QueueSet(Dispatcher.Context.GetReferencingObject(), UE::Core::Private::Unsafe::Decay(*Objects), ETokenlessId::Collector, MayKill());
	}
	
	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference, UObject* ReferenceOwner) override
	{
		Dispatcher.Context.WeakReferences.Add({ *WeakReference, WeakReference, ReferenceOwner });
		return true;
	}
};

#if !UE_BUILD_SHIPPING

#if !ENABLE_GC_HISTORY
#error TDebugReachabilityProcessor assumes ENABLE_GC_HISTORY is enabled in non-shipping configs
#endif

template <EGCOptions InOptions>
class TDebugReachabilityProcessor 
{
public:
	FORCEINLINE void BeginTimingObject(UObject* CurrentObject)
	{
		UE::GC::GDetailedStats.BeginTimingObject(CurrentObject);
	}

	FORCEINLINE void UpdateDetailedStats(UObject* CurrentObject)
	{
		UE::GC::GDetailedStats.UpdateDetailedStats(CurrentObject);
	}

	FORCEINLINE void LogDetailedStatsSummary()
	{
		UE::GC::GDetailedStats.LogDetailedStatsSummary();
	}

	FORCEINLINE bool IsTimeLimitExceeded() const
	{
		if (IsWithIncrementalReachabilityAnalysis() && bTrackHistory && !bTrackGarbage)
		{
			return GReachabilityState.IsTimeLimitExceeded();
		}
		return false;
	}

	static constexpr EGCOptions Options = InOptions;

	static constexpr FORCEINLINE bool IsWithIncrementalReachabilityAnalysis() { return !!(Options & EGCOptions::IncrementalReachability); }

	TDebugReachabilityProcessor()
	: bTrackGarbage(GGarbageReferenceTrackingEnabled != 0)
	, bTrackHistory(FGCHistory::Get().IsActive())
	, bForceEnable(GForceEnableDebugGCProcessor  != 0)
	{}
	
	bool TracksHistory() const { return bTrackHistory; }
	bool TracksGarbage() const { return bTrackGarbage; }
	bool IsForceEnabled() const { return bForceEnable; }

	FORCENOINLINE void HandleTokenStreamObjectReference(FWorkerContext& Context, const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId, EOrigin Origin, bool bAllowReferenceElimination)
	{
		UE::GC::GDetailedStats.IncreaseObjectRefStats(Object);
		if (ValidateReference(Object, PermanentPool, FReferenceToken(ReferencingObject), MemberId))
		{
			FReferenceMetadata Metadata(GUObjectArray.ObjectToIndex(Object));
			if (bAllowReferenceElimination & Metadata.Has(TReachabilityProcessor<Options>::KillFlag)) //-V792
			{
				if (MayKill<Options>(Origin, bAllowReferenceElimination) == EKillable::Yes)
				{
					check(IsEliminatingGarbage(Options) || ReferencingObject);
					checkSlow(Metadata.ObjectItem->GetOwnerIndex() <= 0);
					KillReference(Object);
					return;
				}
				else if (!IsEliminatingGarbage(Options) && bTrackGarbage)
				{
					check(Origin == EOrigin::Other);
					HandleGarbageReference(Context, ReferencingObject, Object, MemberId);
				}
			}
			
			bool bReachedFirst = TReachabilityProcessor<Options>::HandleValidReference(Context, FImmutableReference{Object}, Metadata);

			if (bReachedFirst && bTrackHistory)
			{
				HandleHistoryReference(Context, ReferencingObject, Object, MemberId);
			}
		}
	}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	FORCEINLINE void HandleTokenStreamVerseCellReference(FWorkerContext& Context, UObject* ReferencingObject, Verse::VCell* Cell, FMemberId MemberId, EOrigin Origin)
	{
		if (GIsFrankenGCCollecting)
		{
			if (bTrackHistory)
			{
				HandleHistoryReference(Context, ReferencingObject, Cell, MemberId);
			}
			Verse::FMarkStack MarkStack;
			MarkStack.Mark(Cell);
			if (MarkStack.Num() > 0)
			{
				using VisitorType = TVerseDebugReachabilityVisitor<Options>;
				VisitorType Visitor(Context, bTrackHistory, PermanentPool, MarkStack);
				typename VisitorType::FReferrerContext VisitorContext(Visitor, typename VisitorType::FReferrerToken(ReferencingObject));
				while (Verse::VCell* LocalCell = MarkStack.Pop())
				{
					LocalCell->VisitReferences(Visitor);
				}
			}
		}
	}
#endif

private:
	const FPermanentObjectPoolExtents PermanentPool;
	const bool bTrackGarbage;
	const bool bTrackHistory;
	const bool bForceEnable;

	FORCENOINLINE static void HandleGarbageReference(FWorkerContext& Context, const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId)
	{
		const UObject* Referencer = ReferencingObject ? ReferencingObject : Context.GetReferencingObject();
		if (IsValid(Referencer))
		{
			if (Referencer != FGCObject::GGCObjectReferencer)
			{
				FName PropertyName = GetMemberDebugInfo(Referencer->GetClass()->ReferenceSchema.Get(), MemberId).Name;
				Context.GarbageReferences.Add(FGarbageReferenceInfo(Referencer, Object, PropertyName));
			}
			else
			{
				FGCObject* GCObjectReferencer = FGCObject::GGCObjectReferencer->GetCurrentlySerializingObject();
				Context.GarbageReferences.Add(FGarbageReferenceInfo(GCObjectReferencer, Object));
			}
		}
	}

	FORCENOINLINE static void HandleHistoryReference(FWorkerContext& Context, const UObject* ReferencingObject, UObject*& Object, FMemberId MemberId)
	{
		const UObject* Referencer = ReferencingObject ? ReferencingObject : Context.GetReferencingObject();
		FGCDirectReference Ref(Object, GetReferencerName(Referencer, MemberId));
		GetContextHistoryReferences(Context, FReferenceToken(Referencer), 0).Add(Ref);
	}

	FORCENOINLINE static void HandleHistoryReference(FWorkerContext& Context, const UObject* ReferencingObject, Verse::VCell* Cell, FMemberId MemberId)
	{
		const UObject* Referencer = ReferencingObject ? ReferencingObject : Context.GetReferencingObject();
		FGCDirectReference Ref(Cell, GetReferencerName(Referencer, MemberId));
		GetContextHistoryReferences(Context, FReferenceToken(Referencer), 0).Add(Ref);
	}
};

template <EGCOptions Options>
class TDebugReachabilityCollector final : public TReachabilityCollectorBase<Options>
{
	using Super = TReachabilityCollectorBase<Options>;
	using Super::bAllowEliminatingReferences;
	using Super::CurrentOrigin;
	using ProcessorType = TDebugReachabilityProcessor<Options>;
	ProcessorType& Processor;
	FWorkerContext& Context;

public:
	TDebugReachabilityCollector(ProcessorType& InProcessor, FWorkerContext& InContext) : Processor(InProcessor), Context(InContext) {}

	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
	{
#if ENABLE_GC_OBJECT_CHECKS
		if (Object && !Object->IsValidLowLevelFast())
		{
			UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, ReferencingProperty: %s"), 
				(int64)(PTRINT)Object, 
				ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
				ReferencingProperty ? *ReferencingProperty->GetFullName() : TEXT("NULL"));
		}
#endif // ENABLE_GC_OBJECT_CHECKS

		Processor.HandleTokenStreamObjectReference(Context, ReferencingObject, Object, EMemberlessId::Collector, CurrentOrigin, bAllowEliminatingReferences);
		Context.Stats.AddReferences(1);
	}

	virtual void HandleObjectReferences(UObject** Objects, int32 Num, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
	{
		for (UObject*& Object : MakeArrayView(Objects, Num))
		{
			HandleObjectReference(Object, ReferencingObject, ReferencingProperty);
		}
	}

	virtual void AddStableReference(UObject** Object) override
	{
		HandleObjectReference(*Object, Context.GetReferencingObject(), nullptr);
	}
	
	virtual void AddStableReferenceArray(TArray<UObject*>* Objects) override
	{
		for (UObject*& Object : *Objects)
		{
			HandleObjectReference(Object, Context.GetReferencingObject(), nullptr);
		}
	}

	virtual void AddStableReferenceSet(TSet<UObject*>* Objects) override
	{
		for (UObject*& Object : *Objects)
		{
			HandleObjectReference(Object, Context.GetReferencingObject(), nullptr);
		}
	}

	virtual void AddStableReference(TObjectPtr<UObject>* Object) override
	{
		HandleObjectReference(UE::Core::Private::Unsafe::Decay(*Object), Context.GetReferencingObject(), nullptr);
	}
	
	virtual void AddStableReferenceArray(TArray<TObjectPtr<UObject>>* Objects) override
	{
		for (TObjectPtr<UObject>& Object : *Objects)
		{
			HandleObjectReference(UE::Core::Private::Unsafe::Decay(Object), Context.GetReferencingObject(), nullptr);
		}
	}

	virtual void AddStableReferenceSet(TSet<TObjectPtr<UObject>>* Objects) override
	{
		for (TObjectPtr<UObject>& Object : *Objects)
		{
			HandleObjectReference(UE::Core::Private::Unsafe::Decay(Object), Context.GetReferencingObject(), nullptr);
		}
	}

	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference, UObject* ReferenceOwner) override
	{
		Context.WeakReferences.Add({ *WeakReference, WeakReference, ReferenceOwner });
		return true;
	}
};

#endif // !UE_BUILD_SHIPPING

FString FDebugSchemaStackNode::ToString() const
{
	FString Result;
#if !UE_BUILD_SHIPPING
	for (const FDebugSchemaStackNode* Node = this; Node; Node = Node->Prev)
	{
		FMemberInfo MemberInfo = GetMemberDebugInfo(Node->Schema, Node->Member);
		FString MemberInfoString = FString::Printf(TEXT("%s (0x%x)"), *MemberInfo.Name.GetPlainNameString(), MemberInfo.Offset);
		if (!Result.IsEmpty())
		{
			MemberInfoString += TEXT(" -> ");
		}
		Result = MemberInfoString + Result;
	}
#else
	Result += TEXT("Unknown");
#endif
	return Result;
}

FORCEINLINE static void CheckReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty)
{
#if ENABLE_GC_OBJECT_CHECKS
	if (Object && !Object->IsValidLowLevelFast())
	{
		UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, ReferencingProperty: %s"),
			(int64)(PTRINT)Object,
			ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
			ReferencingProperty ? *ReferencingProperty->GetFullName() : TEXT("NULL"));
	}
#endif // ENABLE_GC_OBJECT_CHECKS
}

template <EGCOptions Options>
void TReachabilityCollector<Options>::HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty)
{
	CheckReference(Object, ReferencingObject, ReferencingProperty);
	Dispatcher.HandleReferenceDirectly(ReferencingObject, Object, EMemberlessId::Collector, MayKill());
}

template <EGCOptions Options>
void TReachabilityCollector<Options>::HandleObjectReferences(UObject** InObjects, int32 ObjectNum, const UObject* ReferencingObject, const FProperty* ReferencingProperty)
{
	if (ObjectNum == 0)
	{
		return;
	}

	EKillable Killable = MayKill();

	TArrayView<UObject*> Objects(InObjects, ObjectNum);
	for (UObject*& Object : Objects)
	{
		CheckReference(Object, ReferencingObject, ReferencingProperty);
	}

	for (UObject*& Object : Objects)
	{
		Dispatcher.HandleReferenceDirectly(ReferencingObject, Object, EMemberlessId::Collector, Killable);
	}
}

//////////////////////////////////////////////////////////////////////////

template <typename ProcessorType, typename CollectorType>
void TFastReferenceCollector<ProcessorType, CollectorType>::ProcessStructs(DispatcherType& Dispatcher)
{
	if constexpr (DispatcherType::bBatching)
	{
		check(Dispatcher.GetStructBlocks().CanPop());
		FStructBlockifier& Blocks = Dispatcher.GetStructBlocks();
		Dispatcher.Context.ReferencingObject = nullptr;

		do
		{
			int32 Num;
			FStructArrayBlock* Block = Blocks.PopBlock(/* out */ Num);
			ON_SCOPE_EXIT{ Blocks.FreeBlock(Block); };
			FStructArray* ArrayIt = Block->Data;
			FStructArray* ArrayEnd = ArrayIt + Num;
		
			for (int32 PrefetchIdx = 0; PrefetchIdx < FStructArrayBlock::Lookahead; ++PrefetchIdx)
			{
				FPlatformMisc::Prefetch(ArrayIt[PrefetchIdx].Data);
			}
			FPlatformMisc::Prefetch(ArrayIt[0].Schema.GetWords());

			do
			{
				FPlatformMisc::Prefetch(ArrayIt[FStructArrayBlock::Lookahead].Data);
				FPlatformMisc::Prefetch(ArrayIt[1].Schema.GetWords());

				const FStructArray AoS = *ArrayIt;
				check(AoS.Num > 0);
				CheckValid(AoS.Schema);
				check(AoS.Stride == AoS.Schema.GetStructStride());
				uint8* StructIt = AoS.Data;
				uint8* StructEnd = AoS.Data + AoS.Num * AoS.Stride;
				typename DispatcherType::SchemaStackScopeType SchemaStack(Dispatcher.Context, AoS.Schema);
				do
				{
					Private::VisitMembers(Dispatcher, AoS.Schema, StructIt); 
					StructIt += AoS.Stride;
				}
				while (StructIt != StructEnd);
			}
			while (++ArrayIt != ArrayEnd);
		}
		while (Blocks.CanPop() || Dispatcher.FlushToStructBlocks());
	}
}

} // namespace UE::GC

//////////////////////////////////////////////////////////////////////////

void ShutdownGarbageCollection()
{
	UE::GC::FContextPoolScope().Cleanup();
	delete GAsyncPurge;
	GAsyncPurge = nullptr;
#if ENABLE_GC_HISTORY
	FGCHistory::Get().Cleanup();
#endif
}

//////////////////////////////////////////////////////////////////////////
FReferenceFinder::FReferenceFinder(TArray<UObject*>& InObjectArray, UObject* InOuter, bool bInRequireDirectOuter, bool bInShouldIgnoreArchetype, bool bInSerializeRecursively, bool bInShouldIgnoreTransient)
	: ObjectArray(InObjectArray)
	, LimitOuter(InOuter)
	, SerializedProperty(nullptr)
	, bRequireDirectOuter(bInRequireDirectOuter)
	, bShouldIgnoreArchetype(bInShouldIgnoreArchetype)
	, bSerializeRecursively(false)
	, bShouldIgnoreTransient(bInShouldIgnoreTransient)
{
	ObjectSet.Append(MakeArrayView(const_cast<const UObject**>(ObjectArray.GetData()), ObjectArray.Num()));
	bSerializeRecursively = bInSerializeRecursively && LimitOuter != NULL;
	if (InOuter)
	{
		// If the outer is specified, try to set the SerializedProperty based on its linker.
		auto OuterLinker = InOuter->GetLinker();
		if (OuterLinker)
		{
			SerializedProperty = OuterLinker->GetSerializedProperty();
		}
	}
}

void FReferenceFinder::FindReferences(UObject* Object, UObject* InReferencingObject, FProperty* InReferencingProperty)
{
	check(Object != NULL);

	if (!Object->GetClass()->IsChildOf(UClass::StaticClass()))
	{
		AddPropertyReferences(Object->GetClass(), Object, InReferencingObject);
	}
	Object->CallAddReferencedObjects(*this);
}

void FReferenceFinder::HandleObjectReference( UObject*& InObject, const UObject* InReferencingObject /*= NULL*/, const FProperty* InReferencingProperty /*= NULL*/ )
{
	// Avoid duplicate entries.
	if ( InObject != NULL )
	{		
		if ( LimitOuter == NULL || (InObject->GetOuter() == LimitOuter || (!bRequireDirectOuter && InObject->IsIn(LimitOuter))) )
		{
			// Many places that use FReferenceFinder expect the object to not be const.
			UObject* Object = const_cast<UObject*>(InObject);
			// do not add or recursively serialize objects that have already been added
			bool bAlreadyExists;
			ObjectSet.Add(Object, &bAlreadyExists);
			if (!bAlreadyExists)
			{
				check(Object->IsValidLowLevel());
				ObjectArray.Add(Object);

				// check this object for any potential object references
				if (bSerializeRecursively)
				{
					FindReferences(Object, const_cast<UObject*>(InReferencingObject), const_cast<FProperty*>(InReferencingProperty));
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

namespace UE::GC
{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
bool GIsFrankenGCCollecting = false;
static bool bInFrankenGCStartStop = false; // This just tracks if we are in a StartVersGC/StopVerseGC call pair
static bool bFrankenGCEnabled = false;
static Verse::FCollectionCycleRequest VerseCycleRequest;

static bool UpdateFrankenGCMode()
{
	bool bNewState = GEnableFrankenGC;
	if (bFrankenGCEnabled != bNewState)
	{
		bFrankenGCEnabled = bNewState;
		if (bFrankenGCEnabled)
		{
			Verse::FIOContext::Create([](Verse::FIOContext Context) {
				Verse::FHeap::EnableExternalControl(Context);
				});
			// If this trips, then the collection thread was still active after external control was enabled
			ensure(!Verse::FHeap::IsGCStartPendingExternalSignal());
		}
		else
		{
			Verse::FHeap::DisableExternalControl();
		}
	}
	return bFrankenGCEnabled;
}

void EnableFrankenGCMode(bool bEnable)
{
	GEnableFrankenGC = bEnable;
	UpdateFrankenGCMode();
}

static FORCEINLINE void StartVerseGC()
{
	ensure(!bInFrankenGCStartStop && !GIsFrankenGCCollecting);
	bInFrankenGCStartStop = true;
	GIsFrankenGCCollecting = UpdateFrankenGCMode();
	if (GIsFrankenGCCollecting)
	{
		// If this triggers, then someone is kicking off a Verse GC outside of FrankenGC which is a problem
		ensure(!Verse::FHeap::IsGCStartPendingExternalSignal());
		Verse::FIOContext::Create([](Verse::FIOContext Context) { 
			Verse::FHeap::ExternallySynchronouslyStartGC(Context);
			VerseCycleRequest = Verse::FHeap::StartCollectingIfNotCollecting();
		});
	}
}

static FORCEINLINE void StopVerseGC()
{
	ensure(bInFrankenGCStartStop);
	bInFrankenGCStartStop = false;
	if (GIsFrankenGCCollecting)
	{
		GIsFrankenGCCollecting = false;
		Verse::FIOContext::Create([](Verse::FIOContext Context) { 
			Verse::FHeap::ExternallySynchronouslyTerminateGC(Context);
			VerseCycleRequest.Wait(Context);
		});
		// If this trips, then something went wrong with waiting for the previous cycle to complete.
		ensure(!Verse::FHeap::IsGCStartPendingExternalSignal());
	}
}
#else
static FORCEINLINE void StartVerseGC()
{
}

static FORCEINLINE void StopVerseGC()
{
}
#endif
} // namespace UE::GC

//////////////////////////////////////////////////////////////////////////

namespace UE::GC
{

class FRealtimeGC : public FGarbageCollectionTracer
{
	typedef void(FRealtimeGC::*ReachabilityAnalysisFn)(FWorkerContext&);

	/** Pointers to functions used for Reachability Analysis */
	ReachabilityAnalysisFn ReachabilityAnalysisFunctions[8];

	UE::Tasks::TTask<void> InitialCollection;
	TArray<UObject**> InitialReferences;
	TArray<UObject*> InitialObjects;

	void BeginInitialReferenceCollection(EGCOptions Options)
	{
		InitialReferences.Reset();

		if (IsParallel(Options))
		{
			InitialCollection = UE::Tasks::Launch(TEXT("CollectInitialReferences"), 
				[&] () { FGCObject::GGCObjectReferencer->AddInitialReferences(InitialReferences); });
		}
	}

	TConstArrayView<UObject**> GetInitialReferences(EGCOptions Options)
	{
		if (!!(Options & EGCOptions::Parallel))
		{
			InitialCollection.Wait();
		}
		else
		{
			FGCObject::GGCObjectReferencer->AddInitialReferences(InitialReferences);
		}

		return InitialReferences;
	}

	template<class CollectorType, class ProcessorType>
	FORCEINLINE void CollectReferencesForGC(ProcessorType& Processor, UE::GC::FWorkerContext& Context)
	{
		using FastReferenceCollector = TFastReferenceCollector<ProcessorType, CollectorType>;

		if constexpr (IsParallel(ProcessorType::Options))
		{
			ProcessAsync([](void* P, FWorkerContext& C) { FastReferenceCollector(*reinterpret_cast<ProcessorType*>(P)).ProcessObjectArray(C); }, &Processor, Context);
		}
		else
		{
			FastReferenceCollector(Processor).ProcessObjectArray(Context);
		}
	}

	template <EGCOptions Options>
	void PerformReachabilityAnalysisOnObjectsInternal(FWorkerContext& Context)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PerformReachabilityAnalysisOnObjectsInternal);

#if !UE_BUILD_SHIPPING
		TDebugReachabilityProcessor<Options> DebugProcessor;
		if (DebugProcessor.IsForceEnabled() | //-V792
			DebugProcessor.TracksHistory() | 
			DebugProcessor.TracksGarbage() & Stats.bFoundGarbageRef)
		{
			CollectReferencesForGC<TDebugReachabilityCollector<Options>>(DebugProcessor, Context);
			return;
		}
#endif
		
		TReachabilityProcessor<Options> Processor;
		CollectReferencesForGC<TReachabilityCollector<Options>>(Processor, Context);
	}

	/** Calculates GC function index based on current settings */
	static FORCEINLINE int32 GetGCFunctionIndex(EGCOptions InOptions)
	{
		return (!!(InOptions & EGCOptions::Parallel)) |
			(!!(InOptions & EGCOptions::EliminateGarbage) << 1) |
			(!!(InOptions & EGCOptions::IncrementalReachability) << 2);
	}

public:
	/** Default constructor, initializing all members. */
	FRealtimeGC()
	{
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::None)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::None | EGCOptions::None>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::Parallel | EGCOptions::None)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::Parallel | EGCOptions::None>;

		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::None | EGCOptions::EliminateGarbage)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::None | EGCOptions::EliminateGarbage>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::Parallel | EGCOptions::EliminateGarbage)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::Parallel | EGCOptions::EliminateGarbage>;

		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::None | EGCOptions::IncrementalReachability)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::None | EGCOptions::IncrementalReachability>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::Parallel | EGCOptions::IncrementalReachability)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::Parallel | EGCOptions::IncrementalReachability>;

		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::EliminateGarbage | EGCOptions::IncrementalReachability)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::None | EGCOptions::EliminateGarbage | EGCOptions::IncrementalReachability>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EGCOptions::Parallel | EGCOptions::EliminateGarbage | EGCOptions::IncrementalReachability)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EGCOptions::Parallel | EGCOptions::EliminateGarbage | EGCOptions::IncrementalReachability>;

		FGCObject::StaticInit();
	}

	struct FMarkClustersArrays
	{
		TArray<FUObjectItem*> KeepClusters;
		TArray<FUObjectItem*> ClustersToDissolve;

		inline static bool Reserve(const TArray<TGatherIterator<FMarkClustersArrays>, TInlineAllocator<32>>& InGatherResults, FMarkClustersArrays& OutCombinedResults)
		{
			int32 NumKeepClusters = 0;
			int32 NumClustersToDissolve = 0;
			for (const TGatherIterator<FMarkClustersArrays>& It : InGatherResults)
			{
				NumKeepClusters += It.Payload.KeepClusters.Num();
				NumClustersToDissolve += It.Payload.ClustersToDissolve.Num();
			}
			OutCombinedResults.KeepClusters.Reserve(OutCombinedResults.KeepClusters.Num() + NumKeepClusters);
			OutCombinedResults.ClustersToDissolve.Reserve(OutCombinedResults.ClustersToDissolve.Num() + NumClustersToDissolve);
			return (NumKeepClusters + NumClustersToDissolve) > 0;
		}

		inline static int32 Num(const FMarkClustersArrays& InArrays)
		{
			return InArrays.KeepClusters.Num() + InArrays.ClustersToDissolve.Num();
		}

		inline static void Append(const FMarkClustersArrays& InSource, FMarkClustersArrays& OutDest)
		{
			OutDest.KeepClusters += InSource.KeepClusters;
			OutDest.ClustersToDissolve += InSource.ClustersToDissolve;
		}
	};
	
	FORCENOINLINE void MarkClusteredObjectsAsReachable(const EGatherOptions Options, TArray<UObject*>& OutRootObjects)
	{
		using namespace UE::GC;
		using namespace UE::GC::Private;
		using FMarkClustersState = TThreadedGather<FMarkClustersArrays, FMarkClustersArrays>;

		std::atomic<int32> TotalClusteredObjects = 0;
		FMarkClustersState GatherClustersState;
		TArray<FUObjectCluster>& ClusterArray = GUObjectClusters.GetClustersUnsafe();

		// StartGathering calculates the number of threads based on the number of objects but here the objects are actually clusters
		// that contain many more objects than the number of clusters so we want to be able to process at least two clusters per thread	
		const int32 NumThreads = !!(Options & EGatherOptions::Parallel) ? FMath::Min(GetNumCollectReferenceWorkers(), (ClusterArray.Num() + 1) / 2) : 1;
		GatherClustersState.Start(Options, ClusterArray.Num(), /* FirstIndex = */ 0, NumThreads);
		FMarkClustersState::FThreadIterators& ThreadIterators = GatherClustersState.GetThreadIterators();

		ParallelFor(TEXT("GC.MarkClusteredObjectsAsReachable"), GatherClustersState.NumWorkerThreads(), 1, [&ThreadIterators, &ClusterArray, &TotalClusteredObjects](int32 ThreadIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MarkClusteredObjectsAsReachableTask);
			FMarkClustersState::FIterator& ThreadState = ThreadIterators[ThreadIndex];
			int32 ThisThreadClusteredObjects = 0;

			while (ThreadState.Index <= ThreadState.LastIndex)
			{
				int32 ClusterIndex = ThreadState.Index++;
				FUObjectCluster& Cluster = ClusterArray[ClusterIndex];
				if (Cluster.RootIndex >= 0)
				{
					ThisThreadClusteredObjects += Cluster.Objects.Num();

					FUObjectItem* RootItem = &GUObjectArray.GetObjectItemArrayUnsafe()[Cluster.RootIndex];
					if (!RootItem->IsGarbage())
					{
						bool bKeepCluster = RootItem->HasAnyFlags(EInternalObjectFlags_RootFlags);
						if (bKeepCluster)
						{
							RootItem->FastMarkAsReachableInterlocked_ForGC();
							ThreadState.Payload.KeepClusters.Add(RootItem);
						}

						for (int32 ObjectIndex : Cluster.Objects)
						{
							FUObjectItem* ClusteredItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];

							ClusteredItem->FastMarkAsReachableAndClearReachaleInClusterInterlocked_ForGC();

							if (!bKeepCluster && ClusteredItem->HasAnyFlags(EInternalObjectFlags_RootFlags))
							{
								ThreadState.Payload.KeepClusters.Add(RootItem);
								bKeepCluster = true;
							}
						}
					}
					else
					{
						ThreadState.Payload.ClustersToDissolve.Add(RootItem);
					}
				}
			}
			TotalClusteredObjects += ThisThreadClusteredObjects;
		}, (GatherClustersState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	
		FMarkClustersArrays MarkClustersResults;
		GatherClustersState.Finish(MarkClustersResults);

		for (FUObjectItem* ObjectItem : MarkClustersResults.ClustersToDissolve)
		{
			// Check if the object is still a cluster root - it's possible one of the previous
			// DissolveClusterAndMarkObjectsAsUnreachable calls already dissolved its cluster
			if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
			{
				GUObjectClusters.DissolveClusterAndMarkObjectsAsUnreachable(ObjectItem);
				GUObjectClusters.SetClustersNeedDissolving();
			}
		}

		for (FUObjectItem* ObjectItem : MarkClustersResults.KeepClusters)
		{
			checkSlow(ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
			// this thing is definitely not marked unreachable, so don't test it here
			// Make sure all referenced clusters are marked as reachable too
			MarkReferencedClustersAsReachable<EGCOptions::None>(ObjectItem->GetClusterIndex(), OutRootObjects);
		}

		GGCStats.NumClusteredObjects = TotalClusteredObjects.load(std::memory_order_acquire);
	}

	FORCENOINLINE void MarkRootObjectsAsReachable(const EGatherOptions Options, const EObjectFlags KeepFlags, TArray<UObject*>& OutRootObjects)
	{
		using namespace UE::GC;
		using namespace UE::GC::Private;		
		using FMarkRootsState = TThreadedGather<TArray<UObject*>>;

		FMarkRootsState MarkRootsState;		

		{
			GRootsCritical.Lock();
			TArray<int32> RootsArray(GRoots.Array());				
			GRootsCritical.Unlock();
			MarkRootsState.Start(Options, RootsArray.Num());
			FMarkRootsState::FThreadIterators& ThreadIterators = MarkRootsState.GetThreadIterators();

			ParallelFor(TEXT("GC.MarkRootObjectsAsReachable"), MarkRootsState.NumWorkerThreads(), 1, [&ThreadIterators, &RootsArray](int32 ThreadIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(MarkClusteredObjectsAsReachableTask);
				FMarkRootsState::FIterator& ThreadState = ThreadIterators[ThreadIndex];

				while (ThreadState.Index <= ThreadState.LastIndex)
				{
					FUObjectItem* RootItem = &GUObjectArray.GetObjectItemArrayUnsafe()[RootsArray[ThreadState.Index++]];
					UObject* Object = static_cast<UObject*>(RootItem->Object);

					// IsValidLowLevel is extremely slow in this loop so only do it in debug
					checkSlow(Object->IsValidLowLevel());					
#if DO_GUARD_SLOW
					// We cannot mark Root objects as Garbage.
					checkCode(if (RootItem->HasAllFlags(EInternalObjectFlags::Garbage | EInternalObjectFlags::RootSet)) { UE_LOG(LogGarbage, Fatal, TEXT("Object %s is part of root set though has been marked as Garbage!"), *Object->GetFullName()); });
#endif

					RootItem->FastMarkAsReachableInterlocked_ForGC();
					ThreadState.Payload.Add(Object);
				}
			}, (MarkRootsState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);			
		}

		using FMarkObjectsState = TThreadedGather<TArray<UObject*>>;
		FMarkObjectsState MarkObjectsState;

		// This is super slow as we need to look through all existing UObjects and access their memory to check EObjectFlags
		if (KeepFlags != RF_NoFlags)
		{
			MarkObjectsState.Start(Options, GUObjectArray.GetObjectArrayNum(), GUObjectArray.GetFirstGCIndex());

			FMarkObjectsState::FThreadIterators& ThreadIterators = MarkObjectsState.GetThreadIterators();
			ParallelFor(TEXT("GC.SlowMarkObjectAsReachable"), MarkObjectsState.NumWorkerThreads(), 1, [&ThreadIterators, &KeepFlags](int32 ThreadIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(MarkClusteredObjectsAsReachableTask);
				FMarkObjectsState::FIterator& ThreadState = ThreadIterators[ThreadIndex];
				const bool bWithGarbageElimination = UObject::IsGarbageEliminationEnabled();

				while (ThreadState.Index <= ThreadState.LastIndex)
				{
					FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ThreadState.Index++];
					UObject* Object = static_cast<UObject*>(ObjectItem->Object);
					if (Object &&
						!ObjectItem->HasAnyFlags(EInternalObjectFlags_RootFlags) && // It may be counter intuitive to reject roots but these are tracked with GRoots and have already been marked and added
						!(bWithGarbageElimination && ObjectItem->IsGarbage()) && Object->HasAnyFlags(KeepFlags)) // Garbage elimination works regardless of KeepFlags
					{
						// IsValidLowLevel is extremely slow in this loop so only do it in debug
						checkSlow(Object->IsValidLowLevel());

						ObjectItem->FastMarkAsReachableInterlocked_ForGC();
						ThreadState.Payload.Add(Object);
					}
				}
			}, (MarkObjectsState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
		}

		// Preallocate the resulting array taking both MarkRootsState and MarkObjectsState results into account to avoild reallocating OutRootObjects in each of the Finish() calls.
		OutRootObjects.Reserve(OutRootObjects.Num() + MarkRootsState.NumGathered() + MarkObjectsState.NumGathered() + ObjectLookahead);
		MarkRootsState.Finish(OutRootObjects);
		MarkObjectsState.Finish(OutRootObjects);

		GGCStats.NumRoots = OutRootObjects.Num();
	}

	/**
	 * Marks all objects that don't have KeepFlags and EInternalObjectFlags_GarbageCollectionKeepFlags as MaybeUnreachable
	 */
	FORCENOINLINE void MarkObjectsAsUnreachable(const EObjectFlags KeepFlags)
	{
		using namespace UE::GC;

		// Don't swap the flags if we're re-entering this function to track garbage references
		if (const bool bInitialMark = !Stats.bFoundGarbageRef)
		{
			// This marks all UObjects as MaybeUnreachable
			Swap(GReachableObjectFlag, GMaybeUnreachableObjectFlag);
		}

		// Not counting the disregard for GC set to preserve legacy behavior
		GObjectCountDuringLastMarkPhase.Set(GUObjectArray.GetObjectArrayNumMinusAvailable() - GUObjectArray.GetFirstGCIndex());

		EGatherOptions GatherOptions = GetObjectGatherOptions();

		// Now make sure all clustered objects and root objects are marked as Reachable. 
		// This could be considered as initial part of reachability analysis and could be made incremental.
		MarkClusteredObjectsAsReachable(GatherOptions, InitialObjects);
		MarkRootObjectsAsReachable(GatherOptions, KeepFlags, InitialObjects);
	}

private:

	void ConditionallyAddBarrierReferencesToHistory(FWorkerContext& Context)
	{
#if !UE_BUILD_SHIPPING && ENABLE_GC_HISTORY
		if (FGCHistory::Get().IsActive())
		{
			static const FName NAME_Barrier = FName(TEXT("Barrier"));

			TArray<FGCDirectReference>& DirectReferences = GetContextHistoryReferences(Context, FReferenceToken(FGCHistory::Get().GetBarrierObject()), InitialObjects.Num());
			for (UObject* BarrierObject : InitialObjects)
			{
				DirectReferences.Add(FGCDirectReference(BarrierObject, NAME_Barrier));
			}
		}
#endif // !UE_BUILD_SHIPPING && ENABLE_GC_HISTORY
	}

	void StartReachabilityAnalysis(EObjectFlags KeepFlags, const EGCOptions Options)
	{
		BeginInitialReferenceCollection(Options);

		// Reset object count.
		GObjectCountDuringLastMarkPhase.Reset();
		
		InitialObjects.Reset();

		// Make sure GC referencer object is checked for references to other objects even if it resides in permanent object pool
		if (FPlatformProperties::RequiresCookedData() && GUObjectArray.IsDisregardForGC(FGCObject::GGCObjectReferencer))
		{
			InitialObjects.Add(FGCObject::GGCObjectReferencer);
		}

		{
			const double StartTime = FPlatformTime::Seconds();
			MarkObjectsAsUnreachable(KeepFlags);
			const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
			if (!Stats.bFoundGarbageRef)
			{
				GGCStats.MarkObjectsAsUnreachableTime = ElapsedTime;
			}
			UE_LOG(LogGarbage, Verbose, TEXT("%f ms for MarkObjectsAsUnreachable Phase (%d Objects To Serialize)"), ElapsedTime * 1000, InitialObjects.Num());
		}
	}

	void PerformReachabilityAnalysisPass(const EGCOptions Options)
	{
		FContextPoolScope Pool;
		FWorkerContext* Context = nullptr;
		const bool bIsSingleThreaded = !(Options & EGCOptions::Parallel);

		if (GReachabilityState.IsSuspended())
		{
			Context = GReachabilityState.GetContextArray()[0];
			Context->bDidWork = false;
			InitialObjects.Reset();
		}
		else
		{
			Context = Pool.AllocateFromPool();
			if (bIsSingleThreaded)
			{
				GReachabilityState.SetupWorkers(1);
				GReachabilityState.GetContextArray()[0] = Context;
			}
		}

		if (!Private::GReachableObjects.IsEmpty())
		{
			// Add objects marked with the GC barrier to the inital set of objects for the next iteration of incremental reachability
			Private::GReachableObjects.PopAllAndEmpty(InitialObjects);
			GGCStats.NumBarrierObjects += InitialObjects.Num();
			UE_LOG(LogGarbage, Verbose, TEXT("Adding %d object(s) marker by GC barrier to the list of objects to process"), InitialObjects.Num());
			ConditionallyAddBarrierReferencesToHistory(*Context);
		}
		else if (GReachabilityState.GetNumIterations() == 0 || (Stats.bFoundGarbageRef && !GReachabilityState.IsSuspended()))
		{
			Context->InitialNativeReferences = GetInitialReferences(Options);
		}

		if (!Private::GReachableClusters.IsEmpty())
		{
			// Process cluster roots that were marked as reachable by the GC barrier
			TArray<FUObjectItem*> KeepClusterRefs;
			Private::GReachableClusters.PopAllAndEmpty(KeepClusterRefs);
			for (FUObjectItem* ObjectItem : KeepClusterRefs)
			{
				// Mark referenced clusters and mutable objects as reachable
				MarkReferencedClustersAsReachable<EGCOptions::None>(ObjectItem->GetClusterIndex(), InitialObjects);
			}
		}

		Context->SetInitialObjectsUnpadded(InitialObjects);

		PerformReachabilityAnalysisOnObjects(Context, Options);

		if (!GReachabilityState.CheckIfAnyContextIsSuspended())
		{
			GReachabilityState.ResetWorkers();
			Stats.AddStats(Context->Stats);
			GReachabilityState.UpdateStats(Context->Stats);
			Pool.ReturnToPool(Context);
		}
		else if (bIsSingleThreaded)
		{
			Context->ResetInitialObjects();
			Context->InitialNativeReferences = TConstArrayView<UObject**>();
		}
	}

public:

	/**
	 * Performs reachability analysis.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not
	 */
	void PerformReachabilityAnalysis(EObjectFlags KeepFlags, const EGCOptions Options)
	{
		LLM_SCOPE(ELLMTag::GC);

		const bool bIsGarbageTracking = !GReachabilityState.IsSuspended() && Stats.bFoundGarbageRef;

		if (!GReachabilityState.IsSuspended())
		{
			StartReachabilityAnalysis(KeepFlags, Options);
			// We start verse GC here so that the objects are unmarked prior to verse marking them
			StartVerseGC();
		}

		{
			const double StartTime = FPlatformTime::Seconds();

			do
			{
				PerformReachabilityAnalysisPass(Options);
			// NOTE: It is critical that VerseGCActive is called prior to checking GReachableObjects.  While VerseGCActive is true,
			// items can still be added to GReachableObjects.  So if reversed, during the point where GReachableObjects is checked
			// and VerseGCActive returns false, something might have been marked.  Reversing insures that Verse will not add anything 
			// if Verse is no longer active.
			} while ((VerseGCActive() || !Private::GReachableObjects.IsEmpty() || !Private::GReachableClusters.IsEmpty()) && !GReachabilityState.IsSuspended());

			const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
			if (!bIsGarbageTracking)
			{
				GGCStats.ReferenceCollectionTime += ElapsedTime;
			}
			UE_LOG(LogGarbage, Verbose, TEXT("%f ms for Reachability Analysis"), ElapsedTime * 1000);
		}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Allowing external systems to add object roots. This can't be done through AddReferencedObjects
		// because it may require tracing objects (via FGarbageCollectionTracer) multiple times
		if (!GReachabilityState.IsSuspended())
		{
			FCoreUObjectDelegates::TraceExternalRootsForReachabilityAnalysis.Broadcast(*this, KeepFlags, !(Options & EGCOptions::Parallel));
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool VerseGCActive()
	{
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
		if (!GIsFrankenGCCollecting)
		{
			return false;
		}

		bool bIsDone = false;
		Verse::FIOContext::Create(
			[&bIsDone](Verse::FIOContext VerseContext) {
				bIsDone = Verse::FHeap::IsGCTerminationPendingExternalSignal(VerseContext);
			}
		);
		return !bIsDone;
#else
		return false;
#endif
	}

	virtual void PerformReachabilityAnalysisOnObjects(FWorkerContext* Context, EGCOptions Options) override
	{
		(this->*ReachabilityAnalysisFunctions[GetGCFunctionIndex(Options)])(*Context);
	}

	FProcessorStats Stats;
};

} // namespace UE::GC

// Allow parallel GC to be overridden to single threaded via console command.
static int32 GAllowParallelGC = 1;

static FAutoConsoleVariableRef CVarAllowParallelGC(
	TEXT("gc.AllowParallelGC"),
	GAllowParallelGC,
	TEXT("Used to control parallel GC."),
	ECVF_Default
);

/** Returns true if Garbage Collection should be forced to run on a single thread */
static bool ShouldForceSingleThreadedGC()
{
//	return true;

	const bool bForceSingleThreadedGC = !FApp::ShouldUseThreadingForPerformance() || !FPlatformProcess::SupportsMultithreading() ||
#if PLATFORM_SUPPORTS_MULTITHREADED_GC
	(FPlatformMisc::NumberOfCores() < 2 || GAllowParallelGC == 0 || PERF_DETAILED_PER_CLASS_GC_STATS);
#else	//PLATFORM_SUPPORTS_MULTITHREADED_GC
		true;
#endif	//PLATFORM_SUPPORTS_MULTITHREADED_GC
	return bForceSingleThreadedGC;
}

void AcquireGCLock()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AcquireGCLock);
	const double StartTime = FPlatformTime::Seconds();
	FGCCSyncObject::Get().GCLock();
	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	if (FPlatformProperties::RequiresCookedData() && ElapsedTime > 0.001)
	{
		UE_LOG(LogGarbage, Warning, TEXT("%f ms for acquiring GC lock"), ElapsedTime * 1000);
	}
}

void ReleaseGCLock()
{
	FGCCSyncObject::Get().GCUnlock();
}

static bool IncrementalDestroyGarbage(bool bUseTimeLimit, double TimeLimit);

/**
 * Incrementally purge garbage by deleting all unreferenced objects after routing Destroy.
 *
 * Calling code needs to be EXTREMELY careful when and how to call this function as 
 * RF_Unreachable cannot change on any objects unless any pending purge has completed!
 *
 * @param	bUseTimeLimit	whether the time limit parameter should be used
 * @param	TimeLimit		soft time limit for this function call
 */
void IncrementalPurgeGarbage(bool bUseTimeLimit, double TimeLimit)
{
	using namespace UE::GC;
	using namespace UE::GC::Private;

	if (GExitPurge)
	{
		GObjPurgeIsRequired = true;
		GUObjectArray.DisableDisregardForGC();
		GObjCurrentPurgeObjectIndexNeedsReset = true;
	}
	// Early out if there is nothing to do.
	if (!GObjPurgeIsRequired && !GObjIncrementalPurgeIsInProgress)
	{
		return;
	}

	SCOPED_NAMED_EVENT(IncrementalPurgeGarbage, FColor::Red);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("IncrementalPurgeGarbage"), STAT_IncrementalPurgeGarbage, STATGROUP_GC);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GarbageCollection);

	bool bCompleted = false;

	struct FResetPurgeProgress
	{
		bool& bCompletedRef;
		FResetPurgeProgress(bool& bInCompletedRef)
			: bCompletedRef(bInCompletedRef)
		{
			// Incremental purge is now in progress.
			GObjIncrementalPurgeIsInProgress = true;
			FPlatformMisc::MemoryBarrier();
		}
		~FResetPurgeProgress()
		{
			if (bCompletedRef)
			{
				GObjIncrementalPurgeIsInProgress = false;
				FPlatformMisc::MemoryBarrier();
			}
		}

	} ResetPurgeProgress(bCompleted);
	
	// if the purge was completed last tick, perform the trim to finalize this incremental purge
	if (!GObjPurgeIsRequired)
	{
		FMemory::Trim();
		bCompleted = true;
	}
	else
	{
		// Set 'I'm garbage collecting' flag - might be checked inside various functions.
		TGuardValue<bool> GuardIsGarbageCollecting(GIsGarbageCollecting, true);

		// Keep track of start time to enforce time limit unless bForceFullPurge is true;
		GCStartTime = FPlatformTime::Seconds();
		bool bTimeLimitReached = false;

		if (IsIncrementalUnhashPending())
		{
			bTimeLimitReached = UnhashUnreachableObjects(bUseTimeLimit, TimeLimit);

			if (GUnrechableObjectIndex >= GUnreachableObjects.Num())
			{
				FScopedCBDProfile::DumpProfile();
			}
		}

		if (!bTimeLimitReached)
		{
			bCompleted = IncrementalDestroyGarbage(bUseTimeLimit, TimeLimit);
		}
		
		if (bCompleted)
		{
			// Broadcast the post-purge garbage delegate to give systems a chance to clean up things
			// that might have been referenced by purged objects.
			TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPostPurgeGarbage);
			FCoreUObjectDelegates::GetPostPurgeGarbageDelegate().Broadcast();
		}

		// when running incrementally using a time limit, add one last tick for the memory trim
		bCompleted = bCompleted && !bUseTimeLimit;

		if (bUseTimeLimit)
		{
			// Add total time only if we're using time limit otherwise purge phase time is included in PostGarbageCollect
			GGCStats.TotalTime += FPlatformTime::Seconds() - GCStartTime;
		}
	}
	GGCStats.bInProgress = !bCompleted;

	if (bCompleted && bUseTimeLimit)
	{
		// If this was incremental purge then its completion marks the completion of the entire GC cycle (otherwise see PostCollectGarbageImpl)		
		FCoreUObjectDelegates::GarbageCollectComplete.Broadcast();
		if (GDumpGCAnalyticsToLog)
		{
			GGCStats.DumpToLog();
		}
		TRACE_END_REGION(TEXT("GarbageCollection"));
	}
}

static bool GWarningTimeOutHasBeenDisplayedGC = false;
static bool GEnableTimeoutOnPendingDestroyedObjectGC = true;
#if UE_BUILD_SHIPPING || USING_ADDRESS_SANITISER
static FAutoConsoleVariableRef CVarGCEnableTimeoutOnPendingDestroyedObjectInShipping(
	TEXT("gc.EnableTimeoutOnPendingDestroyedObjectInShipping"),
	GEnableTimeoutOnPendingDestroyedObjectGC,
	TEXT("Enable time out when there are pending destroyed object (default is true, always true in non shipping build"),
	ECVF_Default
);
#endif

static int32 GAdditionalFinishDestroyTimeGC = 40;
static FAutoConsoleVariableRef CVarAdditionalFinishDestroyTimeGC(
	TEXT("gc.AdditionalFinishDestroyTimeGC"),
	GAdditionalFinishDestroyTimeGC,
	TEXT("Additional wait time in seconds to allow FinishDestroy to complete."),
	ECVF_Default
);

bool IncrementalDestroyGarbage(bool bUseTimeLimit, double TimeLimit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IncrementalDestroyGarbage);
	const bool bMultithreadedPurge = !ShouldForceSingleThreadedGC() && GMultithreadedDestructionEnabled;
	if (!GAsyncPurge)
	{
		GAsyncPurge = new FAsyncPurge(bMultithreadedPurge);
	}
	else if (GAsyncPurge->IsMultithreaded() != bMultithreadedPurge)
	{
		check(GAsyncPurge->IsFinished());
		delete GAsyncPurge;
		GAsyncPurge = new FAsyncPurge(bMultithreadedPurge);
	}

	bool bCompleted = false;
	bool bTimeLimitReached = false;

	// Keep track of time it took to destroy objects for stats
	const double IncrementalDestroyGarbageStartTime = FPlatformTime::Seconds();
	double LastTimeoutWarningTime = IncrementalDestroyGarbageStartTime;	

	// Depending on platform FPlatformTime::Seconds might take a noticeable amount of time if called thousands of times so we avoid
	// enforcing the time limit too often, especially as neither Destroy nor actual deletion should take significant
	// amounts of time.
	const int32	TimeLimitEnforcementGranularityForDestroy = 10;
	const int32	TimeLimitEnforcementGranularityForDeletion = 100;

	// Set 'I'm garbage collecting' flag - might be checked inside UObject::Destroy etc.
	TGuardValue<bool> GuardIsGarbageCollecting(GIsGarbageCollecting, true);

	if( !GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached )
	{
		check(GUnrechableObjectIndex >= GUnreachableObjects.Num());

		// Try to dispatch all FinishDestroy messages to unreachable objects.  We'll iterate over every
		// single object and destroy any that are ready to be destroyed.  The objects that aren't yet
		// ready will be added to a list to be processed afterwards.
		int32 TimeLimitTimePollCounter = 0;
		int32 FinishDestroyTimePollCounter = 0;

		if (GObjCurrentPurgeObjectIndexNeedsReset)
		{
			GObjCurrentPurgeObjectIndex = 0;
			GObjCurrentPurgeObjectIndexNeedsReset = false;
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalFinishDestroy);
			while (GObjCurrentPurgeObjectIndex < GUnreachableObjects.Num())
			{
				FUObjectItem* ObjectItem = GUnreachableObjects[GObjCurrentPurgeObjectIndex];
				checkSlow(ObjectItem);

				//@todo UE - A prefetch was removed here. Re-add it. It wasn't right anyway, since it was ten items ahead and the consoles on have 8 prefetch slots

				check(!ObjectItem->IsMaybeUnreachable());
				if (ObjectItem->IsUnreachable())
				{
					UObject* Object = static_cast<UObject*>(ObjectItem->Object);
					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if(Object->IsReadyForFinishDestroy())
					{
						UE::GC::GDetailedStats.IncPurgeCount(Object);
						// Send FinishDestroy message.
						Object->ConditionalFinishDestroy();
					}
					else
					{
						// The object isn't ready for FinishDestroy to be called yet.  This is common in the
						// case of a graphics resource that is waiting for the render thread "release fence"
						// to complete.  Just calling IsReadyForFinishDestroy may begin the process of releasing
						// a resource, so we don't want to block iteration while waiting on the render thread.

						// Add the object index to our list of objects to revisit after we process everything else
						GGCObjectsPendingDestruction.Add(Object);
						GGCObjectsPendingDestructionCount++;
					}
				}

				// We've processed the object so increment our global iterator.  It's important to do this before
				// we test for the time limit so that we don't process the same object again next tick!
				++GObjCurrentPurgeObjectIndex;

				// Only check time limit every so often to avoid calling FPlatformTime::Seconds too often.
				const bool bPollTimeLimit = ((TimeLimitTimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
				if( bUseTimeLimit && bPollTimeLimit && ((FPlatformTime::Seconds() - GCStartTime) > TimeLimit) )
				{
					bTimeLimitReached = true;
					break;
				}
			}
		}

		// Have we finished the first round of attempting to call FinishDestroy on unreachable objects?
		if (GObjCurrentPurgeObjectIndex >= GUnreachableObjects.Num())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalFinishDestroyDeferred);
			double MaxTimeForFinishDestroy = 10.00;
			bool bFinishDestroyTimeExtended = false;
			FString FirstObjectNotReadyWhenTimeExtended;
			int32 StartObjectsPendingDestructionCount = GGCObjectsPendingDestructionCount;

			// We've finished iterating over all unreachable objects, but we need still need to handle
			// objects that were deferred.
			int32 LastLoopObjectsPendingDestructionCount = GGCObjectsPendingDestructionCount;
			while( GGCObjectsPendingDestructionCount > 0 )
			{
				int32 CurPendingObjIndex = 0;
				while( CurPendingObjIndex < GGCObjectsPendingDestructionCount )
				{
					// Grab the actual object for the current pending object list iteration
					UObject* Object = GGCObjectsPendingDestruction[ CurPendingObjIndex ];

					// Object should never have been added to the list if it failed this criteria
					check( Object != nullptr && Object->IsUnreachable() );

					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if( Object->IsReadyForFinishDestroy() )
					{
						UE::GC::GDetailedStats.IncPurgeCount(Object);
						// Send FinishDestroy message.
						Object->ConditionalFinishDestroy();

						// Remove the object index from our list quickly (by swapping with the last object index).
						// NOTE: This is much faster than calling TArray.RemoveSwap and avoids shrinking allocations
						{
							// Swap the last index into the current index
							GGCObjectsPendingDestruction[ CurPendingObjIndex ] = GGCObjectsPendingDestruction[ GGCObjectsPendingDestructionCount - 1 ];

							// Decrement the object count
							GGCObjectsPendingDestructionCount--;
						}
					}
					else
					{
						// We'll revisit this object the next time around.  Move on to the next.
						CurPendingObjIndex++;
					}

					// Only check time limit every so often to avoid calling FPlatformTime::Seconds too often.
					const bool bPollTimeLimit = ((TimeLimitTimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
					if( bUseTimeLimit && bPollTimeLimit && ((FPlatformTime::Seconds() - GCStartTime) > TimeLimit) )
					{
						bTimeLimitReached = true;
						break;
					}
				}

				if( bUseTimeLimit )
				{
					// A time limit is set and we've completed a full iteration over all leftover objects, so
					// go ahead and bail out even if we have more time left or objects left to process.  It's
					// likely in this case that we're waiting for the render thread.
					break;
				}
				else if( GGCObjectsPendingDestructionCount > 0 )
				{
					if (FPlatformProperties::RequiresCookedData())
					{
						const bool bPollTimeLimit = ((FinishDestroyTimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
#if PLATFORM_IOS || PLATFORM_ANDROID
						if(bPollTimeLimit && !bFinishDestroyTimeExtended && (FPlatformTime::Seconds() - GCStartTime) > MaxTimeForFinishDestroy )
						{
							MaxTimeForFinishDestroy = GAdditionalFinishDestroyTimeGC; // was 30s, now CVar specified. see FORT-305812.
							bFinishDestroyTimeExtended = true;
#if USE_HITCH_DETECTION
							GHitchDetected = true;
#endif
							FirstObjectNotReadyWhenTimeExtended = GetFullNameSafe(GGCObjectsPendingDestruction[0]);
						}
						else
#endif
						// Check if we spent too much time on waiting for FinishDestroy without making any progress
						if (LastLoopObjectsPendingDestructionCount == GGCObjectsPendingDestructionCount && bPollTimeLimit &&
							((FPlatformTime::Seconds() - LastTimeoutWarningTime) > MaxTimeForFinishDestroy))
						{
							LastTimeoutWarningTime = FPlatformTime::Seconds();
							if (GEnableTimeoutOnPendingDestroyedObjectGC)
							{
								UE_LOG(LogGarbage, Warning, TEXT("Spent more than %.2fs on routing FinishDestroy to objects (objects in queue: %d)"), MaxTimeForFinishDestroy, GGCObjectsPendingDestructionCount);
								UObject* LastObjectNotReadyForFinishDestroy = nullptr;
								for (int32 ObjectIndex = 0; ObjectIndex < GGCObjectsPendingDestructionCount && ObjectIndex < GMaxFinishDestroyTimeoutObjectsToLog; ++ObjectIndex)
								{
									UObject* Obj = GGCObjectsPendingDestruction[ObjectIndex];
									bool bReady = Obj->IsReadyForFinishDestroy();
									UE_LOG(LogGarbage, Warning, TEXT("  [%d]: %s, IsReadyForFinishDestroy: %s"),
										ObjectIndex,
										*GetFullNameSafe(Obj),
										bReady ? TEXT("true") : TEXT("false"));
									if (!bReady)
									{
										LastObjectNotReadyForFinishDestroy = Obj;
									}
								}

#if PLATFORM_DESKTOP
								ensureMsgf(0, TEXT("Spent to much time waiting for FinishDestroy for %d object(s) (last object: %s), check log for details"),
									GGCObjectsPendingDestructionCount,
									*GetFullNameSafe(LastObjectNotReadyForFinishDestroy));
#else
								//for non-desktop platforms, make this a warning so that we can die inside of an object member call.
								//this will give us a greater chance of getting useful memory inside of the platform minidump.
								UE_LOG(LogGarbage, Warning, TEXT("Spent to much time waiting for FinishDestroy for %d object(s) (last object: %s), check log for details"),
									GGCObjectsPendingDestructionCount,
									*GetFullNameSafe(LastObjectNotReadyForFinishDestroy));
								if (LastObjectNotReadyForFinishDestroy)
								{
									LastObjectNotReadyForFinishDestroy->AbortInsideMemberFunction();
								}
								else
								{
									//go through the standard fatal error path if LastObjectNotReadyForFinishDestroy is null.
									//this could happen in the current code flow, in the odd case where an object finished readying just in time for the loop above.
									UE_LOG(LogGarbage, Fatal, TEXT("LastObjectNotReadyForFinishDestroy is NULL."));
								}
#endif
							}
							else if (!GWarningTimeOutHasBeenDisplayedGC)
							{
								UE_LOG(LogGarbage, Warning, TEXT("Spent more than %.2fs on routing FinishDestroy to objects (objects in queue: %d) - Skipping FATAL - GC Time out is disabled"), MaxTimeForFinishDestroy, GGCObjectsPendingDestructionCount);
								GWarningTimeOutHasBeenDisplayedGC = true;
							}
						}

					}
					// Sleep before the next pass to give the render thread some time to release fences.
					FPlatformProcess::Sleep( 0 );
				}

				LastLoopObjectsPendingDestructionCount = GGCObjectsPendingDestructionCount;
			}

			// Have all objects been destroyed now?
			if( GGCObjectsPendingDestructionCount == 0 )
			{
				if (bFinishDestroyTimeExtended)
				{
					FString Msg = FString::Printf(TEXT("Additional time was required to finish routing FinishDestroy, spent %.2fs on routing FinishDestroy to %d objects. 1st obj not ready: '%s'."), (FPlatformTime::Seconds() - GCStartTime), StartObjectsPendingDestructionCount, *FirstObjectNotReadyWhenTimeExtended);
					UE_LOG(LogGarbage, Warning, TEXT("%s"), *Msg );
					FCoreDelegates::OnGCFinishDestroyTimeExtended.Broadcast(Msg);
				}

				// Release memory we used for objects pending destruction, leaving some slack space
				GGCObjectsPendingDestruction.Empty( 256 );

				// Destroy has been routed to all objects so it's safe to delete objects now.
				GObjFinishDestroyHasBeenRoutedToAllObjects = true;
				GObjCurrentPurgeObjectIndexNeedsReset = true;
				GWarningTimeOutHasBeenDisplayedGC = false;
			}
		}
	}		
	
	if (GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DestroyObjects);
		// Perform actual object deletion.
		int32 ProcessCount = 0;
		if (GObjCurrentPurgeObjectIndexNeedsReset)
		{
			GAsyncPurge->BeginPurge();
			// Reset the reset flag but don't reset the actual index yet for stat purposes
			GObjCurrentPurgeObjectIndexNeedsReset = false;
		}

		GAsyncPurge->TickPurge(bUseTimeLimit, TimeLimit, GCStartTime);

		if (GAsyncPurge->IsFinished())
		{
#if UE_BUILD_DEBUG
			GAsyncPurge->VerifyAllObjectsDestroyed();
#endif

			bCompleted = true;
			// Incremental purge is finished, time to reset variables.
			GObjFinishDestroyHasBeenRoutedToAllObjects		= false;
			GObjPurgeIsRequired								= false;
			GObjCurrentPurgeObjectIndexNeedsReset			= true;

			// Log status information.
			const int32 PurgedObjectCountSinceLastMarkPhase = GAsyncPurge->GetObjectsDestroyedSinceLastMarkPhase();
			UE_LOG(LogGarbage, Log, TEXT("GC purged %i objects (%i -> %i) in %.3fms"), PurgedObjectCountSinceLastMarkPhase, 
				GObjectCountDuringLastMarkPhase.GetValue(), 
				GObjectCountDuringLastMarkPhase.GetValue() - PurgedObjectCountSinceLastMarkPhase,
				(FPlatformTime::Seconds() - IncrementalDestroyGarbageStartTime) * 1000);
			UE::GC::GDetailedStats.LogPurgeStats(PurgedObjectCountSinceLastMarkPhase);
			GAsyncPurge->ResetObjectsDestroyedSinceLastMarkPhase();
		}
	}

	const double ElapsedTime = FPlatformTime::Seconds() - IncrementalDestroyGarbageStartTime;
	GGCStats.DestroyGarbageTime += ElapsedTime;
	GGCStats.DestroyGarbageTimeLimit = TimeLimit;

	if (bUseTimeLimit && !bCompleted)
	{
		UE_LOG(LogGarbage, Log, TEXT("%.3f ms for incrementally purging unreachable objects (FinishDestroyed: %d, Destroyed: %d / %d)"),
			ElapsedTime * 1000,
			GObjCurrentPurgeObjectIndex,
			GAsyncPurge->GetObjectsDestroyedSinceLastMarkPhase(),
			GUnreachableObjects.Num());
	}

	return bCompleted;
}

/**
 * Returns whether an incremental purge is still pending/ in progress.
 *
 * @return	true if incremental purge needs to be kicked off or is currently in progress, false othwerise.
 */
bool IsIncrementalPurgePending()
{
	return GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired;
}

bool IsGarbageCollectingAndLockingUObjectHashTables()
{
	return GIsGarbageCollectingAndLockingUObjectHashTables;
}

// This counts how many times GC was skipped
static int32 GNumAttemptsSinceLastGC = 0;

// Number of times GC can be skipped.
static int32 GNumRetriesBeforeForcingGC = 10;
static FAutoConsoleVariableRef CVarNumRetriesBeforeForcingGC(
	TEXT("gc.NumRetriesBeforeForcingGC"),
	GNumRetriesBeforeForcingGC,
	TEXT("Maximum number of times GC can be skipped if worker threads are currently modifying UObject state."),
	ECVF_Default
	);

// Force flush streaming on GC console variable
static int32 GFlushStreamingOnGC = 0;
static FAutoConsoleVariableRef CVarFlushStreamingOnGC(
	TEXT("gc.FlushStreamingOnGC"),
	GFlushStreamingOnGC,
	TEXT("If enabled, streaming will be flushed each time garbage collection is triggered."),
	ECVF_Default
	);

void DissolveUnreachableClusters(UE::GC::EGatherOptions Options)
{
	const int32 InitialNumClusters = GUObjectClusters.GetNumAllocatedClusters();
	if (!InitialNumClusters)
	{
		// Early out if clustering is disabled or there's no clusters
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(DissolveUnreachableClusters);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.DissolveUnreachableClusters"), STAT_CollectGarbageInternal_DissolveUnreachableClusters, STATGROUP_GC);

	const double StartTime = FPlatformTime::Seconds();

	using namespace UE::GC;
	using namespace UE::GC::Private;
	using FGatherClustersState = TThreadedGather<TArray<int32>>;

	std::atomic<int32> TotalClusteredObjects = 0;
	FGatherClustersState GatherClustersState;
	TArray<FUObjectCluster>& ClusterArray = GUObjectClusters.GetClustersUnsafe();
	
	// StartGathering calculates the number of threads based on the number of objects but here the objects are actually clusters
	// that contain many more objects than the number of clusters so we want to be able to process at least two clusters per thread	
	const int32 NumThreads = !!(Options & EGatherOptions::Parallel) ? FMath::Min(GetNumCollectReferenceWorkers(), (ClusterArray.Num() + 1) / 2) : 1;
	GatherClustersState.Start(Options, ClusterArray.Num(), /* FirstIndex = */ 0, NumThreads);
	FGatherClustersState::FThreadIterators& ThreadIterators = GatherClustersState.GetThreadIterators();

	ParallelFor(TEXT("GC.DissolveUnreachableClusters"), GatherClustersState.NumWorkerThreads(), 1, [&ThreadIterators, &ClusterArray, &TotalClusteredObjects](int32 ThreadIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DissolveUnreachableClustersTask);
		FGatherClustersState::FIterator& ThreadState = ThreadIterators[ThreadIndex];
		int32 ThisThreadClusteredObjects = 0;

		while (ThreadState.Index <= ThreadState.LastIndex)
		{
			int32 ClusterIndex = ThreadState.Index++;
			FUObjectCluster& Cluster = ClusterArray[ClusterIndex];
			if (Cluster.RootIndex >= 0)
			{
				FUObjectItem* RootItem = &GUObjectArray.GetObjectItemArrayUnsafe()[Cluster.RootIndex];
				if (RootItem->IsUnreachable())
				{
					for (int32 ObjectIndex : Cluster.Objects)
					{
						FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];
						if (!ObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
						{
							ObjectItem->SetUnreachable();
							ThisThreadClusteredObjects++;
						}
					}
					ThreadState.Payload.Add(ClusterIndex);
				}
			}
		}
		TotalClusteredObjects += ThisThreadClusteredObjects;
	}, (GatherClustersState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
	
	TArray<int32> ClustersToDestroy;
	GatherClustersState.Finish(ClustersToDestroy);

	// @todo: if GUObjectClusters.FreeCluster() was thread safe we could do this in parallel too
	for (int32 ClusterIndex : ClustersToDestroy)
	{
#if UE_GCCLUSTER_VERBOSE_LOGGING
		UE_LOG(LogGarbage, Log, TEXT("Destroying cluster (%d) %s"), ClusterRootItem->GetClusterIndex(), *static_cast<UObject*>(ClusterRootItem->Object)->GetFullName());
#endif
		GUObjectClusters.FreeCluster(ClusterIndex);
	}

	GGCStats.NumUnreachableClusteredObjects = TotalClusteredObjects.load(std::memory_order_relaxed);
	GGCStats.NumDissolvedClusters = ClustersToDestroy.Num();
	GGCStats.DissolveUnreachableClustersTime = FPlatformTime::Seconds() - StartTime;

	UE_LOG(LogGarbage, Log, TEXT("%f ms for Dissolve Unreachable Clusters (%d/%d clusters dissolved containing %d cluster objects)"),
		GGCStats.DissolveUnreachableClustersTime * 1000,
		GGCStats.NumDissolvedClusters,
		InitialNumClusters,
		GGCStats.NumUnreachableClusteredObjects);
}

bool GatherUnreachableObjects(UE::GC::EGatherOptions Options, double TimeLimit /*= 0.0*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GatherUnreachableObjects);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.GatherUnreachableObjects"), STAT_CollectGarbageInternal_GatherUnreachableObjects, STATGROUP_GC);

	using namespace UE::GC;
	using namespace UE::GC::Private;

	const double GatherStartTime = FPlatformTime::Seconds();

	GUnreachableObjects.Reset();
	GUnrechableObjectIndex = 0;

	FGatherUnreachableObjectsState::FThreadIterators& ThreadIterators = GGatherUnreachableObjectsState.GetThreadIterators();
	if (!GGatherUnreachableObjectsState.IsSuspended())
	{
		const int32 FirstIndex = GExitPurge ? 0 : GUObjectArray.GetFirstGCIndex();
		GGatherUnreachableObjectsState.Start(Options, GUObjectArray.GetObjectArrayNum(), FirstIndex);
	}

	std::atomic<int32> TimeLimitExceededFlag = 0;

	// Iterate over all objects. Note that we iterate over the UObjectArray and usually check only internal flags which
	// are part of the array so we don't suffer from cache misses as much as we would if we were to check ObjectFlags.
	ParallelFor( TEXT("GC.GatherUnreachable"), GGatherUnreachableObjectsState.NumWorkerThreads(), 1, [&ThreadIterators, &GatherStartTime, &TimeLimit, &TimeLimitExceededFlag](int32 ThreadIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherUnreachableObjectsTask);
		constexpr int32 TimeLimitPollInterval = 10;
		FTimeSlicer Timer(TimeLimitPollInterval, ThreadIndex * TimeLimitPollInterval, GatherStartTime, TimeLimit, TimeLimitExceededFlag);
		FGatherUnreachableObjectsState::FIterator& Iterator = ThreadIterators[ThreadIndex];
		
		while (Iterator.Index <= Iterator.LastIndex)
		{
			FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[Iterator.Index++];
			if (ObjectItem->IsUnreachable())
			{
				checkf(!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot), TEXT("Unreachable cluster root found. Unreachable clusters should have been dissolved in DissolveUnreachableClusters!"));
				Iterator.Payload.Add(ObjectItem);
			}

			if (Timer.IsTimeLimitExceeded())
			{
				return;
			}
		}
	}, (GGatherUnreachableObjectsState.NumWorkerThreads() == 1) ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	// Need to grab the stats now as Finish() will reset the global state but we still want to include Finish() execution time when logging
	const int32 NumGathered = GGatherUnreachableObjectsState.NumGathered();
	const int32 NumScanned = GGatherUnreachableObjectsState.NumScanned();
	const int32 NumThreads = GGatherUnreachableObjectsState.NumWorkerThreads();
	const bool bCompleted = !TimeLimitExceededFlag.load();
	if (bCompleted)
	{
		GGatherUnreachableObjectsState.Finish(GUnreachableObjects);
	}

	const double GatherEndTime = FPlatformTime::Seconds();
	const double GatherIterationTime = GatherEndTime - GatherStartTime;
	GGCStats.GatherUnreachableTime += GatherIterationTime;

	UE_LOG(LogGarbage, Log, TEXT("%f ms for Gather Unreachable Objects (%d objects collected / %d scanned with %d thread(s))"),
		GatherIterationTime * 1000,
		NumGathered,
		NumScanned,
		NumThreads);

	if (bCompleted)
	{
		GGCStats.NumUnreachableObjects = GUnreachableObjects.Num();

		// NotifyUnreachableObjects needs to be called from within a GC lock so that Async Loading Thread does not attempt to create new exports  
		// before it gets a chance to process all unreachable objects
		const bool bNeedsGCLock = !FGCCSyncObject::Get().IsGCLocked();
		if (bNeedsGCLock)
		{
			AcquireGCLock();
		}
		NotifyUnreachableObjects(GUnreachableObjects);
		if (bNeedsGCLock)
		{
			ReleaseGCLock();
		}

		const double NotifyEndTime = FPlatformTime::Seconds();
		GGCStats.NotifyUnreachableTime = NotifyEndTime - GatherEndTime;

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
		if (GVerifyNoUnreachableObjects != 0 && !GExitPurge)
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.VerifyNoUnreachableObjects"), STAT_CollectGarbageInternal_VerifyNoUnreachableObjects, STATGROUP_GC);
			VerifyNoUnreachableObjects(GUnreachableObjects.Num());
			GGCStats.VerifyNoUnreachableTime = FPlatformTime::Seconds() - NotifyEndTime;
		}
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
	}

	// !bCompleted = bTimeLimitReached (to make this function return value behave like UnhashUnreachableObjects)
	return !bCompleted;
}

void GatherUnreachableObjects(bool bForceSingleThreaded)
{
	using namespace UE::GC;
	GatherUnreachableObjects(bForceSingleThreaded ? EGatherOptions::None : EGatherOptions::Parallel);
}

namespace UE::GC
{

class FWeakReferenceEliminator final : public TReachabilityCollectorBase<EGCOptions::None>
{

public:
	FWeakReferenceEliminator() = default;
		
	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override 
	{
		if (InObject && InObject->IsUnreachable())
		{
			InObject = nullptr;
		}
	}
	
	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference, UObject* ReferenceOwner) override
	{
		if (WeakReference && *WeakReference && (*WeakReference)->IsUnreachable())
		{
			*WeakReference = nullptr;
		}
		return true;
	}
};

template <bool bGatheredWithIncrementalReachability = true>
static void ClearWeakReferences(TConstArrayView<TUniquePtr<FWorkerContext>> Contexts)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ClearWeakReferences);
	TSet<UObject*> ObjectsThatNeedWeakReferenceClearing;
	for (const TUniquePtr<FWorkerContext>& Context : Contexts)
	{
		GGCStats.NumWeakReferencesForClearing += Context->WeakReferences.Num();
		for (FWeakReferenceInfo& ReferenceInfo : Context->WeakReferences)
		{
			UObject* ReferencedObject = ReferenceInfo.ReferencedObject;
			if (ReferencedObject && ReferencedObject->IsUnreachable())
			{
				if constexpr (bGatheredWithIncrementalReachability)
				{
					// When running incremental reachability we can't assume the Reference pointer is still valid
					// so instead collect all referencing objects and run AddReferencedObjects on them again with
					// a special collector that will null out references to unrachable objects
					ObjectsThatNeedWeakReferenceClearing.Add(ReferenceInfo.ReferenceOwner);
				}
				else
				{
					*ReferenceInfo.Reference = nullptr;
				}
			}
		}
		Context->WeakReferences.Reset();
	}
	if constexpr (bGatheredWithIncrementalReachability)
	{
		GGCStats.NumObjectsThatNeedWeakReferenceClearing = ObjectsThatNeedWeakReferenceClearing.Num();
		FWeakReferenceEliminator ReferenceEliminator;
		for (UObject* Object : ObjectsThatNeedWeakReferenceClearing)
		{
			if (Object && !Object->IsUnreachable())
			{
				Object->GetClass()->CallAddReferencedObjects(Object, ReferenceEliminator);
			}
		}
	}
}

static void DumpGarbageReferencers(TConstArrayView<TUniquePtr<FWorkerContext>> Contexts)
{	
#if !UE_BUILD_SHIPPING
	if (GGarbageReferenceTrackingEnabled == 0)
	{
		return;
	}

	// We don't care about leaks when engine exit was requested since the final GC pass will destroy everything anyway
	// We still want to clear the GarbageReferences array though
	if (IsEngineExitRequested())
	{
		for (const TUniquePtr<FWorkerContext>& Context : Contexts)
		{
			Context->GarbageReferences.Reset();
		}
		return;
	}

	// Dump all garbage references in detail
	if (GGarbageReferenceTrackingEnabled == 1)
	{
		int32 NumGarbageReferences = 0;
		for (const TUniquePtr<FWorkerContext>& Context : Contexts)
		{
			for (FGarbageReferenceInfo& GarbageReference : Context->GarbageReferences)
			{
				UE_LOG(LogGarbage, Warning, TEXT("Reachable garbage object: %s"), *GarbageReference.GarbageObject->GetFullName());
				UE_LOG(LogGarbage, Warning, TEXT("Referenced by:            %s"), *GarbageReference.GetReferencingObjectInfo());
				UE_LOG(LogGarbage, Warning, TEXT(""));
				NumGarbageReferences++;
			}
			Context->GarbageReferences.Reset();
		}
		
		UE_LOG(LogGarbage, Log, TEXT("Reported %d garbage references"), NumGarbageReferences);
	}
	// Dump one reference for each source property
	else if (GGarbageReferenceTrackingEnabled == 2)
	{
		typedef TPair<void*, FName> FKey;
		TSet<FKey> Seen;
		int32 TotalGarbageReferences = 0, ReportedGarbageReferences = 0;
		for (const TUniquePtr<FWorkerContext>& Context : Contexts)
		{
			for (FGarbageReferenceInfo& GarbageReference : Context->GarbageReferences)
			{
				FKey Key(GarbageReference.bReferencerUObject ? (void*)GarbageReference.Referencer.Object->GetClass() : (void*)  GarbageReference.Referencer.GCObject, GarbageReference.PropertyName);
				if (Seen.Contains(Key) == false)
				{
					UE_LOG(LogGarbage, Warning, TEXT("Reachable garbage object: %s"), *GarbageReference.GarbageObject->GetFullName());
					UE_LOG(LogGarbage, Warning, TEXT("Referenced by:            %s"), *GarbageReference.GetReferencingObjectInfo());
					UE_LOG(LogGarbage, Warning, TEXT(""));
					Seen.Add(Key);
					ReportedGarbageReferences++;
				}
				TotalGarbageReferences++;
			}
			Context->GarbageReferences.Reset();
		}
		UE_LOG(LogGarbage, Log, TEXT("Reported %d/%d garbage references"), ReportedGarbageReferences, TotalGarbageReferences);
	}
#endif // !UE_BUILD_SHIPPING
}

static void UpdateGCHistory(TConstArrayView<TUniquePtr<FWorkerContext>> Contexts)
{
#if ENABLE_GC_HISTORY
	FGCHistory::Get().Update(Contexts);

	for (const TUniquePtr<FWorkerContext>& Context : Contexts)
	{
		for (TPair<FReferenceToken, TArray<FGCDirectReference>*>& DirectReferenceInfos : Context->History)
		{
			delete DirectReferenceInfos.Value;
		}
		Context->History.Reset();
	}
#endif // ENABLE_GC_HISTORY
}

template<bool bPerformFullPurge>
FORCENOINLINE void CollectGarbageImpl(EObjectFlags KeepFlags);

FORCENOINLINE static void CollectGarbageIncremental(EObjectFlags KeepFlags)
{
	SCOPE_TIME_GUARD(TEXT("Collect Garbage Incremental"));
	SCOPED_NAMED_EVENT(CollectGarbageIncremental, FColor::Red);
	CSV_EVENT_GLOBAL(TEXT("GC"));
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GarbageCollection);

	CollectGarbageImpl<false>(KeepFlags);
}

FORCENOINLINE static void CollectGarbageFull(EObjectFlags KeepFlags)
{
	SCOPE_TIME_GUARD(TEXT("Collect Garbage Full"));
	SCOPED_NAMED_EVENT(CollectGarbageFull, FColor::Red);
	CSV_EVENT_GLOBAL(TEXT("GC"));
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GarbageCollectionFull);

	CollectGarbageImpl<true>(KeepFlags);
}

FORCEINLINE void CollectGarbageInternal(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	const double StartTime = FPlatformTime::Seconds();

	GReachabilityState.CollectGarbage(KeepFlags, bPerformFullPurge);

	GTimingInfo.LastGCDuration = FPlatformTime::Seconds() - StartTime;

	CSV_CUSTOM_STAT(GC, Count, 1, ECsvCustomStatOp::Accumulate);
}

EGCOptions GetReferenceCollectorOptions(bool bPerformFullPurge)
{
	return
		// Fall back to single threaded GC if processor count is 1 or parallel GC is disabled
		// or detailed per class gc stats are enabled (not thread safe)
		(ShouldForceSingleThreadedGC() ? EGCOptions::None : EGCOptions::Parallel) |
		// Toggle between Garbage Eliination enabled or disabled
		(UObjectBaseUtility::IsGarbageEliminationEnabled() ? EGCOptions::EliminateGarbage : EGCOptions::None) |
		// Toggle between Incremental Reachability enabled or disabled
		((GAllowIncrementalReachability && !bPerformFullPurge) ? EGCOptions::IncrementalReachability : EGCOptions::None);
}

EGatherOptions GetObjectGatherOptions()
{
	return (ShouldForceSingleThreadedGC() ? EGatherOptions::None : EGatherOptions::Parallel);
}

/** 
 * Deletes all unreferenced objects, keeping objects that have any of the passed in KeepFlags set
 *
 * @param	KeepFlags			objects with those flags will be kept regardless of being referenced or not
 * @param	bPerformFullPurge	if true, perform a full purge after the mark pass
 */
template<bool bPerformFullPurge>
void PreCollectGarbageImpl(EObjectFlags KeepFlags)
{
	using namespace UE::GC::Private;

	const double PreCollectStartTime = FPlatformTime::Seconds();

	FGCCSyncObject::Get().ResetGCIsWaiting();

#if defined(WITH_CODE_GUARD_HANDLER) && WITH_CODE_GUARD_HANDLER
	void CheckImageIntegrityAtRuntime();
	CheckImageIntegrityAtRuntime();
#endif

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "CollectGarbageInternal" ), STAT_CollectGarbageInternal, STATGROUP_GC );
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "GarbageCollection - Begin" ) );
	TRACE_BEGIN_REGION(TEXT("GarbageCollection"));

	// We can't collect garbage while there's a load in progress. E.g. one potential issue is Import.XObject
	check(!IsLoading());

	// Reset GC skip counter
	GNumAttemptsSinceLastGC = 0;

	// Flush streaming before GC if requested
	if (!GIsIncrementalReachabilityPending && GFlushStreamingOnGC && IsAsyncLoading())
	{
		UE_LOG(LogGarbage, Log, TEXT("CollectGarbageInternal() is flushing async loading"));
		ReleaseGCLock();
		FlushAsyncLoading();
		AcquireGCLock();

		GGCStats.bFlushedAsyncLoading = true;
	}

	// Route callbacks so we can ensure that we are e.g. not in the middle of loading something by flushing
	// the async loading, etc...
	if (!GIsIncrementalReachabilityPending)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPreGarbageCollect);
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Broadcast();
	}
	GLastGCFrame = GFrameCounter;

	{
		GIsGarbageCollecting = true;

		// Make sure previous incremental purge has finished or we do a full purge pass in case we haven't kicked one
		// off yet since the last call to garbage collection.
		if (IsIncrementalPurgePending())
		{
			IncrementalPurgeGarbage(false);
			if (!bPerformFullPurge)
			{
				FMemory::Trim();
			}
			GGCStats.bPurgedPreviousGCObjects = true;
			// IncrementalPurgeGarbage sets bInProgress to false when all objects have been purged because IncrementalPurgeGarbage is also called by engine Tick
			GGCStats.bInProgress = true;
		}


		UE_LOG(LogGarbage, Log, TEXT("%s%sCollecting garbage%s"),
			GIsIncrementalReachabilityPending ? TEXT("Resuming ") : TEXT(""),
			(!bPerformFullPurge && GAllowIncrementalReachability) ? TEXT("Incrementally ") : TEXT(""),
			IsAsyncLoading() ? TEXT(" while async loading") : TEXT(""));

		// The hash tables are only locked during this scope of reachability analysis.
		GIsGarbageCollectingAndLockingUObjectHashTables = true;
		LockUObjectHashTables();

		if (!GIsIncrementalReachabilityPending)
		{
			check(!GObjIncrementalPurgeIsInProgress);
			check(!GObjPurgeIsRequired);

			// This can happen if someone disables clusters from the console (gc.CreateGCClusters)
			if (!GCreateGCClusters && GUObjectClusters.GetNumAllocatedClusters())
			{
				GUObjectClusters.DissolveClusters(true);
			}

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
			// Only verify assumptions if option is enabled. This avoids false positives in the Editor or commandlets.
			bool bShouldRandomlyVerifyGCAssumptions = GVerifyGCAssumptionsChance != 0.0 && FMath::FRand() < GVerifyGCAssumptionsChance;
			if (GShouldVerifyGCAssumptions || (bPerformFullPurge && GShouldVerifyGCAssumptionsOnFullPurge) || bShouldRandomlyVerifyGCAssumptions)
			{
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.VerifyGCAssumptions"), STAT_CollectGarbageInternal_VerifyGCAssumptions, STATGROUP_GC);
				const double StartTime = FPlatformTime::Seconds();
				if (GUObjectArray.DisregardForGCEnabled())
				{
					VerifyGCAssumptions();
				}
				if (GUObjectClusters.GetNumAllocatedClusters())
				{
					VerifyClustersAssumptions();
				}
				VerifyObjectFlags();
				GGCStats.VerifyTime = FPlatformTime::Seconds() - StartTime;
				UE_LOG(LogGarbage, Log, TEXT("%.2f ms for Verify GC Assumptions"), GGCStats.VerifyTime * 1000);
			}
#endif
		}
	}

	GGCStats.TotalTime += FPlatformTime::Seconds() - PreCollectStartTime;
}

template<bool bPerformFullPurge>
void CollectGarbageImpl(EObjectFlags KeepFlags)
{
	{
		// Reachability analysis.
		{
			const EGCOptions Options = GetReferenceCollectorOptions(bPerformFullPurge);

			// Perform reachability analysis.
			FRealtimeGC GC;
			GC.PerformReachabilityAnalysis(KeepFlags, Options);
		}
	}
}

template<bool bPerformFullPurge>
void PostCollectGarbageImpl(EObjectFlags KeepFlags)
{
	const double PostCollectStartTime = FPlatformTime::Seconds();

	using namespace UE::GC;
	using namespace UE::GC::Private;	

	if (!GIsIncrementalReachabilityPending)
	{
		FContextPoolScope ContextPool;
		TConstArrayView<TUniquePtr<FWorkerContext>> AllContexts = ContextPool.PeekFree();
		// This needs to happen before clusters get dissolved otherwisise cluster information will be missing from history
		UpdateGCHistory(AllContexts);

		// Reconstruct clusters if needed
		if (GUObjectClusters.ClustersNeedDissolving())
		{
			const double StartTime = FPlatformTime::Seconds();
			GUObjectClusters.DissolveClusters();
			UE_LOG(LogGarbage, Log, TEXT("%f ms for dissolving GC clusters"), (FPlatformTime::Seconds() - StartTime) * 1000);
		}

		DumpGarbageReferencers(AllContexts);

		Swap(GUnreachableObjectFlag, GMaybeUnreachableObjectFlag);

		const EGatherOptions GatherOptions = GetObjectGatherOptions();
		DissolveUnreachableClusters(GatherOptions);

		// This needs to happen after DissolveUnreachableClusters since it can mark more objects as unreachable
		if (GReachabilityState.GetNumIterations() > 1)
		{
			ClearWeakReferences<true>(AllContexts);
		}
		else
		{
			ClearWeakReferences<false>(AllContexts);
		}

		StopVerseGC();

		if (bPerformFullPurge)
		{
			ContextPool.Cleanup();
		}

		GGatherUnreachableObjectsState.Init();

		if (bPerformFullPurge || !GAllowIncrementalGather)
		{
			GatherUnreachableObjects(GatherOptions, /*TimeLimit =*/ 0.0);
		}
	}

	GIsGarbageCollectingAndLockingUObjectHashTables = false;
	UnlockUObjectHashTables();

	GIsGarbageCollecting = false;

	// The hash tables lock was released when reachability analysis was done.
	// BeginDestroy, FinishDestroy, destructors and callbacks are allowed to call functions like StaticAllocateObject and StaticFindObject.
	// Now release the GC lock to allow async loading and other threads to perform UObject operations under the FGCScopeGuard.
	ReleaseGCLock();

	if (!GIsIncrementalReachabilityPending)
	{
		// Fire post-reachability analysis hooks
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPostReachabilityAnalysis);
			FCoreUObjectDelegates::PostReachabilityAnalysis.Broadcast();
		}

		if (bPerformFullPurge || !GIncrementalBeginDestroyEnabled)
		{
			UnhashUnreachableObjects(/**bUseTimeLimit = */ false);
			FScopedCBDProfile::DumpProfile();
		}

		// Set flag to indicate that we are relying on a purge to be performed.
		GObjPurgeIsRequired = true;

		// Perform a full purge by not using a time limit for the incremental purge.
		if (bPerformFullPurge)
		{
			IncrementalPurgeGarbage(false);
		}

		if (bPerformFullPurge)
		{
			ShrinkUObjectHashTables();
		}

		// Destroy all pending delete linkers
		DeleteLoaders();

		if (bPerformFullPurge)
		{
			FMemory::Trim();
		}

		// Route callbacks to verify GC assumptions
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPostGarbageCollect);
			FCoreUObjectDelegates::GetPostGarbageCollect().Broadcast();
		}
	}

	const double PostCollectEndTime = FPlatformTime::Seconds();
	GTimingInfo.LastGCTime = PostCollectEndTime;
	GGCStats.TotalTime += PostCollectEndTime - PostCollectStartTime;
	STAT_ADD_CUSTOMMESSAGE_NAME(STAT_NamedMarker, TEXT("GarbageCollection - End"));

	if (bPerformFullPurge)
	{
		// If this was a full purge then PostCollectGarbageImpl completion marks the completion of the entire GC cycle (otherwise see IncrementalPurgeGarbage)
		FCoreUObjectDelegates::GarbageCollectComplete.Broadcast();
		if (GDumpGCAnalyticsToLog)
		{
			GGCStats.DumpToLog();
		}
		TRACE_END_REGION(TEXT("GarbageCollection"));
	}
}

void FReachabilityAnalysisState::CollectGarbage(EObjectFlags KeepFlags, bool bFullPurge)
{
	using namespace UE::GC::Private;

	if (GIsIncrementalReachabilityPending)
	{
		// Something triggered a new GC run but we're in the middle of incremental reachability analysis.
		// Finish the current GC pass (including purging all unreachable objects) and then kick off another GC run as requested
		bPerformFullPurge = true;
		PerformReachabilityAnalysisAndConditionallyPurgeGarbage(/*bReachabilityUsingTimeLimit =*/ false);

		checkf(!GIsIncrementalReachabilityPending, TEXT("Flushing incremental reachability analysis did not complete properly"));

		// Need to acquire GC lock again as it was released in PerformReachabilityAnalysisAndConditionallyPurgeGarbage() -> UE::GC::PostCollectGarbageImpl()
		AcquireGCLock();
	}

	ObjectKeepFlags = KeepFlags;
	bPerformFullPurge = bFullPurge;

	const bool bReachabilityUsingTimeLimit = !bFullPurge && GAllowIncrementalReachability;
	PerformReachabilityAnalysisAndConditionallyPurgeGarbage(bReachabilityUsingTimeLimit);
}

void FReachabilityAnalysisState::PerformReachabilityAnalysisAndConditionallyPurgeGarbage(bool bReachabilityUsingTimeLimit)
{
	using namespace UE::GC::Private;

	LLM_SCOPE(ELLMTag::GC);

	if (!GIsIncrementalReachabilityPending)
	{
		GGCStats = UE::GC::Private::FStats();
		GGCStats.bInProgress = true;
		GGCStats.bStartedAsFullPurge = bPerformFullPurge;
		GGCStats.NumObjects = GUObjectArray.GetObjectArrayNumMinusAvailable() - GUObjectArray.GetFirstGCIndex();
		GGCStats.NumClusters = GUObjectClusters.GetNumAllocatedClusters();
		GGCStats.ReachabilityTimeLimit = GetReachabilityAnalysisTimeLimit();
	}
	GGCStats.bFinishedAsFullPurge = bPerformFullPurge;

	if (bPerformFullPurge)
	{
		UE::GC::PreCollectGarbageImpl<true>(ObjectKeepFlags);
	}
	else
	{
		UE::GC::PreCollectGarbageImpl<false>(ObjectKeepFlags);
	}	
	
	const bool bForceNonIncrementalReachability =
		!GIsIncrementalReachabilityPending &&
		(bPerformFullPurge || !GAllowIncrementalReachability);

	{
		// When incremental reachability is enabled we start the timer before acquiring GC lock
		// and here we keep track of reference processing time only
		const double ReferenceProcessingStartTime = FPlatformTime::Seconds();
		// When performing the first iteration of reachability analysis start the timer when we actually start processing
		// iteration as we don't have control over various callbacks being fired in PreCollectGarbageImpl and can't be responsible for any hitches in them
		if (IterationStartTime == 0.0)
		{
			IterationStartTime = ReferenceProcessingStartTime;
			IterationTimeLimit = bReachabilityUsingTimeLimit ? GIncrementalReachabilityTimeLimit : 0.0;
		}

		SCOPED_NAMED_EVENT(FRealtimeGC_PerformReachabilityAnalysis, FColor::Red);
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FRealtimeGC::PerformReachabilityAnalysis"), STAT_FArchiveRealtimeGC_PerformReachabilityAnalysis, STATGROUP_GC);

		if (bForceNonIncrementalReachability)
		{
			IncrementalMarkPhaseTotalTime = 0.0;
			ReferenceProcessingTotalTime = 0.0;
			PerformReachabilityAnalysis();
		}
		else
		{
			if (!GIsIncrementalReachabilityPending)
			{
				ReferenceProcessingTotalTime = 0.0;
				IncrementalMarkPhaseTotalTime = 0.0;
			}

			PerformReachabilityAnalysis();
		}

		GIsIncrementalReachabilityPending = GReachabilityState.IsSuspended();

		const double CurrentTime = FPlatformTime::Seconds();
		const double ReferenceProcessingElapsedTime = CurrentTime - ReferenceProcessingStartTime;
		const double ElapsedTime = CurrentTime - IterationStartTime;
		ReferenceProcessingTotalTime += ReferenceProcessingElapsedTime;
		IncrementalMarkPhaseTotalTime += ElapsedTime;

		GGCStats.ReachabilityTime += ReferenceProcessingElapsedTime;
		GGCStats.TotalTime += ReferenceProcessingElapsedTime;

		if (UE_LOG_ACTIVE(LogGarbage, Log))
		{
			if (GIsIncrementalReachabilityPending)
			{
				const double SuspendLatency = FMath::Max(0.0, CurrentTime - (IterationStartTime + IterationTimeLimit));
				UE_LOG(LogGarbage, Log, TEXT("GC Reachability iteration time: %.2f ms (%.2f ms on reference traversal, latency: %.3f ms)"), ElapsedTime * 1000, ReferenceProcessingElapsedTime * 1000, SuspendLatency * 1000.0);
			}
			else
			{
				const double ReferenceProcessingTotalTimeMs = ReferenceProcessingTotalTime * 1000;
				const double IncrementalMarkPhaseTotalTimeMs = IncrementalMarkPhaseTotalTime * 1000;
				UE_LOG(LogGarbage, Log, TEXT("GC Reachability Analysis total time: %.2f ms (%.2f ms on reference traversal)"), IncrementalMarkPhaseTotalTimeMs, ReferenceProcessingTotalTimeMs);
				FString ExtraDetail = WITH_VERSE_VM ? FString::Printf(TEXT("and %d verse cells"), Stats.NumVerseCells) : FString();
				UE_LOG(LogGarbage, Log, TEXT("%.2f ms for %sGC - %d refs/ms while processing %d references from %d objects %s with %d clusters"),
					IncrementalMarkPhaseTotalTimeMs,
					bForceNonIncrementalReachability ? TEXT("") : TEXT("Incremental "),
					(int32)(Stats.NumReferences / ReferenceProcessingTotalTimeMs),
					Stats.NumReferences,
					Stats.NumObjects,
					*ExtraDetail,
					GUObjectClusters.GetNumAllocatedClusters());
			}
		}

		// Reset timer and the time limit. These values will be set to their target values in the next iteration but we don't want 
		// them to be set before the debug reachability run below
		IterationTimeLimit = 0.0;
		IterationStartTime = 0.0;
	}

	if (!GIsIncrementalReachabilityPending && Stats.bFoundGarbageRef && GGarbageReferenceTrackingEnabled > 0)
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GarbageCollectionDebug);
		SCOPED_NAMED_EVENT(FRealtimeGC_PerformReachabilityAnalysisRerun, FColor::Orange);
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FRealtimeGC::PerformReachabilityAnalysisRerun"), STAT_FArchiveRealtimeGC_PerformReachabilityAnalysisRerun, STATGROUP_GC);		
		const double StartTime = FPlatformTime::Seconds();
		{
			// If we are going to scan again, we need to cycle verse GC so the cells are unmarked.
			StopVerseGC();
			TGuardValue<bool> GuardReachabilityUsingTimeLimit(bReachabilityUsingTimeLimit, false);
			FRealtimeGC GC;
			GC.Stats = Stats; // This is to pass Stats.bFoundGarbageRef to CG
			GC.PerformReachabilityAnalysis(ObjectKeepFlags, GetReferenceCollectorOptions(bPerformFullPurge));
		}
		const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
		GGCStats.GarbageTrackingTime = ElapsedTime;
		GGCStats.TotalTime += ElapsedTime;
		UE_LOG(LogGarbage, Log, TEXT("%.2f ms for GC rerun to track garbage references (gc.GarbageReferenceTrackingEnabled=%d)"), ElapsedTime * 1000, GGarbageReferenceTrackingEnabled);
	}
	// Maybe purge garbage (if we're done with incremental reachability and there's still time left)

	if (bPerformFullPurge)
	{
		UE::GC::PostCollectGarbageImpl<true>(ObjectKeepFlags);
	}
	else
	{
		UE::GC::PostCollectGarbageImpl<false>(ObjectKeepFlags);
	}
}

void FReachabilityAnalysisState::PerformReachabilityAnalysis()
{
	if (!bIsSuspended)
	{
		Init();
		NumRechabilityIterationsToSkip = FMath::Max(0, GDelayReachabilityIterations);
	}

	if (bPerformFullPurge)
	{
		UE::GC::CollectGarbageFull(ObjectKeepFlags);
	}
	else if (NumRechabilityIterationsToSkip == 0 || // Delay reachability analysis by NumRechabilityIterationsToSkip (if desired)
		!bIsSuspended || // but only but only after the first iteration (which also does MarkObjectsAsUnreachable)
		IterationTimeLimit <= 0.0f) // and only when using time limit (we're not using the limit when we're flushing reachability analysis when starting a new one or on exit)
	{
		UE::GC::CollectGarbageIncremental(ObjectKeepFlags);
	}
	else
	{
		--NumRechabilityIterationsToSkip;
	}

	FinishIteration();
}

} // namespace UE::GC

void SetIncrementalReachabilityAnalysisEnabled(bool bEnabled)
{
	GAllowIncrementalReachability = bEnabled;
}

bool GetIncrementalReachabilityAnalysisEnabled()
{
	return !!GAllowIncrementalReachability;
}

void SetReachabilityAnalysisTimeLimit(float TimeLimitSeconds)
{
	GIncrementalReachabilityTimeLimit = TimeLimitSeconds;
}

float GetReachabilityAnalysisTimeLimit()
{
	return GIncrementalReachabilityTimeLimit;
}

bool IsIncrementalReachabilityAnalysisPending()
{
	return UE::GC::GIsIncrementalReachabilityPending;
}

void PerformIncrementalReachabilityAnalysis(double TimeLimit)
{
	checkf(UE::GC::GIsIncrementalReachabilityPending, TEXT("Incremental reachability must be pending to perform its next iteration"));
	// When performing Reachability Analysis iterations start the internal timer before acquiring GC lock 
	// so that we don't spend more time than GIncrementalReachabilityTimeLimit on fully completing an iteration
	const bool bUsingTimeLimit = TimeLimit > 0.0;
	UE::GC::GReachabilityState.StartTimer(bUsingTimeLimit ? TimeLimit : 0.0);
	AcquireGCLock();	
	UE::GC::GReachabilityState.PerformReachabilityAnalysisAndConditionallyPurgeGarbage(bUsingTimeLimit);

	checkf(bUsingTimeLimit || !UE::GC::GIsIncrementalReachabilityPending, TEXT("Incremental Reachability is still pending after completing the previous iteration without time limit."));
}

void FinalizeIncrementalReachabilityAnalysis()
{
	if (IsIncrementalReachabilityAnalysisPending())
	{
		PerformIncrementalReachabilityAnalysis(0.0);
	}
}

namespace UE::GC::Private
{
	UE::GC::Private::FStats GetGarbageCollectionStats()
	{
		return GGCStats;
	}
}

FString FGarbageReferenceInfo::GetReferencingObjectInfo() const
{
	if (bReferencerUObject)
	{
		return FString::Printf(TEXT("%s->%s"),
			*Referencer.Object->GetFullName(),
			PropertyName != NAME_None ? *PropertyName.ToString() : TEXT("AddRereferencedObjects")
			);
	}
	else
	{
		return FString::Printf(TEXT("%s->AddRereferencedObjects"),
			Referencer.GCObject ? *Referencer.GCObject->GetReferencerName() : TEXT("Unknown")
			);
	}
}

double GetLastGCTime()
{
	return GTimingInfo.LastGCTime;
}

double GetLastGCDuration()
{
	return GTimingInfo.LastGCDuration;
}

bool IsIncrementalUnhashPending()
{
	return GUnrechableObjectIndex < GUnreachableObjects.Num() || UE::GC::Private::GGatherUnreachableObjectsState.IsPending();
}

bool UnhashUnreachableObjects(bool bUseTimeLimit, double TimeLimit)
{
	using namespace UE::GC;
	using namespace UE::GC::Private;

	bool bTimeLimitReached = false;

	if (GGatherUnreachableObjectsState.IsPending())
	{
		// Incremental Gather needs to be called from UnhashUnreachableObjects to match changes in IsIncrementalUnhashPending() (and not introduce IsIncrementalGatherPending())
		const EGatherOptions GatherOptions = GetObjectGatherOptions();
		const double GatherTimeLimit = GIncrementalGatherTimeLimit > 0.0f ? GIncrementalGatherTimeLimit : TimeLimit;
		bTimeLimitReached = GatherUnreachableObjects(GatherOptions, bUseTimeLimit ? GatherTimeLimit : 0.0);
		if (!bTimeLimitReached)
		{
			if (bUseTimeLimit)
			{
				TimeLimit -= FMath::Min(TimeLimit, FPlatformTime::Seconds() - GCStartTime);
			}
		}
		else
		{
			return bTimeLimitReached;
		}
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UnhashUnreachableObjects);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UnhashUnreachableObjects"), STAT_UnhashUnreachableObjects, STATGROUP_GC);

	TGuardValue<bool> GuardObjUnhashUnreachableIsInProgress(GObjUnhashUnreachableIsInProgress, true);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastGarbageCollectConditionalBeginDestroy);
		FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.Broadcast();
	}

	// Unhash all unreachable objects.
	const double StartTime = FPlatformTime::Seconds();
	double LastPollTime = 0.0;
	const int32 TimeLimitEnforcementGranularityForBeginDestroy = 10;
	int32 TimePollCounter = 0;
	const bool bFirstIteration = (GUnrechableObjectIndex == 0);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ConditionalBeginDestroy);
		while (GUnrechableObjectIndex < GUnreachableObjects.Num())
		{
			//@todo UE - A prefetch was removed here. Re-add it. It wasn't right anyway, since it was ten items ahead and the consoles on have 8 prefetch slots

			FUObjectItem* ObjectItem = GUnreachableObjects[GUnrechableObjectIndex++];
			{
				UObject* Object = static_cast<UObject*>(ObjectItem->Object);
				FScopedCBDProfile Profile(Object);
				// Begin the object's asynchronous destruction.
				Object->ConditionalBeginDestroy();
			}

			const bool bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForBeginDestroy == 0);
			if (bUseTimeLimit & bPollTimeLimit) //-V792
			{
				LastPollTime = FPlatformTime::Seconds();
				if ((LastPollTime - StartTime) > TimeLimit)
				{
					break;
				}
			}
		}
	}

	bTimeLimitReached = (GUnrechableObjectIndex < GUnreachableObjects.Num());

	const double ElapsedTime = (LastPollTime == 0.0 ? FPlatformTime::Seconds() : LastPollTime) - StartTime;
	GGCStats.UnhashingTime += ElapsedTime;
	GGCStats.UnhashingTimeLimit = TimeLimit;

	if (!bUseTimeLimit)
	{
		UE_LOG(LogGarbage, Log, TEXT("%f ms for %sunhashing unreachable objects (%d objects unhashed)"),
			ElapsedTime * 1000,
			bUseTimeLimit ? TEXT("incrementally ") : TEXT(""),
			GUnreachableObjects.Num());
	}
	else if (!bTimeLimitReached)
	{
		// When doing incremental unhashing log only the first and last iteration (this was the last one)
		UE_LOG(LogGarbage, Log, TEXT("Finished unhashing unreachable objects (%d objects unhashed)."), GUnreachableObjects.Num());
	}
	else if (bFirstIteration)
	{
		// When doing incremental unhashing log only the first and last iteration (this was the first one)
		UE_LOG(LogGarbage, Log, TEXT("Starting unhashing unreachable objects (%d objects to unhash)."), GUnreachableObjects.Num());
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadCastPostGarbageCollectConditionalBeginDestroy);
		FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.Broadcast();
	}

	// Return true if time limit has been reached
	return bTimeLimitReached;
}

void CollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	if (GIsInitialLoad)
	{
		// During initial load classes may not yet have their GC token streams assembled
		UE_LOG(LogGarbage, Log, TEXT("Skipping CollectGarbage() call during initial load. It's not safe."));
		return;
	}

	// No other thread may be performing UObject operations while we're running
	AcquireGCLock();

	// Perform actual garbage collection
	UE::GC::CollectGarbageInternal(KeepFlags, bPerformFullPurge);

	// GC lock was released after reachability analysis inside CollectGarbageInternal
}

bool TryCollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	if (GIsInitialLoad)
	{
		// During initial load classes may not yet have their GC token streams assembled
		UE_LOG(LogGarbage, Log, TEXT("Skipping CollectGarbage() call during initial load. It's not safe."));
		return false;
	}

	// No other thread may be performing UObject operations while we're running so try to acquire GC lock
	if (UE::GC::GIsIncrementalReachabilityPending)
	{
		// Since we're already in the middle of a previous GC acquire GC lock even if it means we have to block main thread
		AcquireGCLock();
	}	
	else if (!FGCCSyncObject::Get().TryGCLock())
	{
		if (GNumRetriesBeforeForcingGC > 0 && GNumAttemptsSinceLastGC > GNumRetriesBeforeForcingGC)
		{
			// Force acquire GC lock and block main thread		
			UE_LOG(LogGarbage, Warning, TEXT("TryCollectGarbage: forcing GC after %d skipped attempts."), GNumAttemptsSinceLastGC);
			GNumAttemptsSinceLastGC = 0;
			AcquireGCLock();
		}
		else
		{
			++GNumAttemptsSinceLastGC;
			return false;
		}
	}

	// Perform actual garbage collection
	UE::GC::CollectGarbageInternal(KeepFlags, bPerformFullPurge);

	// GC lock was released after reachability analysis inside CollectGarbageInternal

	return true;
}

void UObject::CallAddReferencedObjects(FReferenceCollector& Collector)
{
	GetClass()->CallAddReferencedObjects(this, Collector);
}

void UObject::AddReferencedObjects(UObject*, FReferenceCollector&)
{
	// This function exists to compare against in GetAROFunc()
}

bool UObject::IsDestructionThreadSafe() const
{
	return false;
}

template <bool bIsVerse>
FORCEINLINE static void MarkObjectItemAsReachable(FUObjectItem* ObjectItem)
{
	using namespace UE::GC;
	using namespace UE::GC::Private;

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	if constexpr (bIsVerse)
	{
		// When verse VM is enabled, this method is also used to report that a UObject is being referenced inside of a VCell
		checkf(GIsFrankenGCCollecting, TEXT("%s is marked as MaybeUnreachable but Reachability Analysis is not in progress"), *static_cast<UObject*>(ObjectItem->Object)->GetFullName());
	}
	else
#endif
	{
		checkf(GIsIncrementalReachabilityPending, TEXT("%s is marked as MaybeUnreachable but Incremental Reachability Analysis is not in progress"), *static_cast<UObject*>(ObjectItem->Object)->GetFullName());
	}
	if (ObjectItem->MarkAsReachableInterlocked_ForGC())
	{
		if (ObjectItem->GetOwnerIndex() >= 0)
		{
			// This object became reachable so add it to a list of new objects to process in the next iteration of incremental GC because
			// we need to mark objects it's referencing as reachable too
			GReachableObjects.Push(static_cast<UObject*>(ObjectItem->Object));
		}
		else
		{
			GReachableClusters.Push(ObjectItem);
		}
	}
}

template <bool bIsVerse>
FORCEINLINE static void MarkAsReachable(const UObjectBase* Obj)
{
	FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Obj);
	if (ObjectItem->IsMaybeUnreachable())
	{
		MarkObjectItemAsReachable<bIsVerse>(ObjectItem);
	}
	else if (ObjectItem->GetOwnerIndex() > 0) // Clustered objects are never marked as MaybeUnreachable so we need to check if the cluster root is MaybeUnreachable
	{
		FUObjectItem* ClusterRootObjectItem = GUObjectArray.IndexToObject(ObjectItem->GetOwnerIndex());
		if (ClusterRootObjectItem->IsMaybeUnreachable())
		{
			MarkObjectItemAsReachable<bIsVerse>(ClusterRootObjectItem);
		}
	}
}

void UObjectBase::MarkAsReachable() const
{
	// It is safe to perform mark as reachable in the open - the worst case is that we'll mark an object reachable that
	// should/would be destroyed, and so in the next GC iteration it will be destroyed instead of in this iteration.
	UE_AUTORTFM_OPEN(
	{
		::MarkAsReachable<false>(this);
	});
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UObject::VerseMarkAsReachable() const
{
	::MarkAsReachable<true>(this);
}
#endif

/*-----------------------------------------------------------------------------
	Implementation of realtime garbage collection helper functions in 
	FProperty, UClass, ...
-----------------------------------------------------------------------------*/

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool FProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	return false;
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool FArrayProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	check(Inner);
	return Inner->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool FMapProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	check(KeyProp);
	check(ValueProp);
	return KeyProp->ContainsObjectReference(EncounteredStructProps, InReferenceType) || ValueProp->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

/**
* Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
* UObject reference.
*
* @return true if property (or sub- properties) contain a UObject reference, false otherwise
*/
bool FSetProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	check(ElementProp);
	return ElementProp->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool FStructProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	if (EncounteredStructProps.Contains(this))
	{
		return false;
	}

	if (!Struct)
	{
		UE_LOG(LogGarbage, Warning, TEXT("Broken FStructProperty does not have a UStruct: %s"), *GetFullName() );
		return false;
	}

	if (EnumHasAnyFlags(InReferenceType, EPropertyObjectReferenceType::Strong) && (Struct->StructFlags & STRUCT_AddStructReferencedObjects))
	{
		return true;
	}

	if (Struct->StructFlags & STRUCT_SerializeNative)
	{
		UScriptStruct::ICppStructOps* Ops = Struct->GetCppStructOps();
		if (Ops && Ops->HasSerializerObjectReferences(InReferenceType))
		{
			return true;
		}
	}

	EncounteredStructProps.Add(this);
	bool bValue = false;
	FProperty* Property = Struct->PropertyLink;
	while (Property)
	{
		if (Property->ContainsObjectReference(EncounteredStructProps, InReferenceType))
		{
			bValue = true;
			break;
		}
		Property = Property->PropertyLinkNext;
	}
	EncounteredStructProps.RemoveSingleSwap(this, EAllowShrinking::No);
	return bValue;
}

bool FFieldPathProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	return !!(InReferenceType & EPropertyObjectReferenceType::Strong);
}

// Returns true if this property contains a weak UObject reference.
bool FDelegateProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	return !!(InReferenceType & EPropertyObjectReferenceType::Weak);
}

// Returns true if this property contains a weak UObject reference.
bool FMulticastDelegateProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType /*= EPropertyObjectReferenceType::Strong*/) const
{
	return !!(InReferenceType & EPropertyObjectReferenceType::Weak);
}

void FOptionalProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	using namespace UE::GC;
	if (IsValueNonNullablePointer())
	{
		ValueProperty->EmitReferenceInfo(Schema, BaseOffset + GetOffset_ForGC(), EncounteredStructProps, DebugPath);
	}
	else if (ValueProperty->ContainsObjectReference(EncounteredStructProps))
	{
		FSchemaBuilder InnerSchema(ValueProperty->GetSize());
		{
			FPropertyStackScope PropertyScope(DebugPath, ValueProperty);
			ValueProperty->EmitReferenceInfo(InnerSchema, 0, EncounteredStructProps, DebugPath);
		}
		Schema.Add(DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC(), EMemberType::Optional, InnerSchema.Build()));
	}
}

void FProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
}

void FObjectProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
	{
		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC() + Idx * sizeof(FObjectPtr), UE::GC::EMemberType::Reference));
	}
}

void FArrayProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	using namespace UE::GC;
	if (Inner->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong))
	{
		bool bUsesFreezableAllocator = EnumHasAnyFlags(ArrayFlags, EArrayPropertyFlags::UsesMemoryImageAllocator);
		EMemberType Type = bUsesFreezableAllocator ?  EMemberType::FreezableStructArray : EMemberType::StructArray;
		FSchemaBuilder InnerSchema(Inner->ElementSize);

		// Structs and nested arrays share the same implementation on the Garbage Collector side
		// as arrays of structs already push the array memory into the GC stack and process its tokens
		// which is exactly what is required for nested arrays to work
		if( Inner->IsA(FObjectProperty::StaticClass()) )
		{
			Type = bUsesFreezableAllocator ?  EMemberType::FreezableReferenceArray :  EMemberType::ReferenceArray;
		}
		else if( Inner->IsA(FInterfaceProperty::StaticClass()) )
		{
			FPropertyStackScope PropertyScope(DebugPath, Inner);
			InnerSchema.Add(UE::GC::DeclareMember(DebugPath, /* offsetof(FScriptInterface, ObjectPointer) */ 0, EMemberType::Reference));
		}
		else if (Inner->IsA(FFieldPathProperty::StaticClass()))
		{
			Type = EMemberType::FieldPathArray;
		}
		else
		{
			FPropertyStackScope PropertyScope(DebugPath, Inner);
			Inner->EmitReferenceInfo(InnerSchema, 0, EncounteredStructProps, DebugPath);
		}

		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC(), Type, InnerSchema.Build()));
	}
}

void FMapProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	using namespace UE::GC;
	if (ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong))
	{
		FSchemaBuilder PairSchema(GetPairStride());
		for (FProperty* Prop : {KeyProp, ValueProp})
		{
			if (Prop->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong))
			{
				FPropertyStackScope PropertyScope(DebugPath, Prop);
				Prop->EmitReferenceInfo(PairSchema, 0, EncounteredStructProps, DebugPath);
			}
		}

		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC(), EMemberType::SparseStructArray, PairSchema.Build()));
	}
}

void FSetProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	using namespace UE::GC;
	if (ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong))
	{
		FSchemaBuilder ElementSchema(GetStride());
		FPropertyStackScope PropertyScope(DebugPath, ElementProp);
		ElementProp->EmitReferenceInfo(ElementSchema, 0, EncounteredStructProps, DebugPath);
		Schema.Add(UE::GC::DeclareMember(DebugPath, BaseOffset + GetOffset_ForGC(), EMemberType::SparseStructArray, ElementSchema.Build()));
	}
}

void FStructProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	using namespace UE::GC;
	
	const int32 Offset = BaseOffset + GetOffset_ForGC();
	if (Struct->StructFlags & STRUCT_AddStructReferencedObjects)
	{
		StructAROFn StructARO = Struct->GetCppStructOps()->AddStructReferencedObjects();	
		for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
		{	
			Schema.Add(UE::GC::DeclareMember(DebugPath, Offset + Idx * ElementSize, EMemberType::MemberARO, StructARO) );
		}
	}

	// Check if the struct has any properties that reference UObjects
	bool bHasPropertiesWithObjectReferences = false;
	if (Struct->PropertyLink)
	{
		// Can't use ContainObjectReference here as it also checks for STRUCT_AddStructReferencedObjects but we only care about property exposed refs
		EncounteredStructProps.Add(this);
		for (FProperty* Property = Struct->PropertyLink; Property && !bHasPropertiesWithObjectReferences; Property = Property->PropertyLinkNext)
		{
			bHasPropertiesWithObjectReferences = Property->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong);
		}
		EncounteredStructProps.RemoveSingleSwap(this, EAllowShrinking::No);
	}

	// Emit schema members
	if (bHasPropertiesWithObjectReferences)
	{
		for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
		{
			for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext)
			{
				Property->EmitReferenceInfo(Schema, Offset + Idx * ElementSize, EncounteredStructProps, DebugPath);
			}
		}
	}
}

void FInterfaceProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	const int32 Offset = BaseOffset + GetOffset_ForGC();
	for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
	{
		Schema.Add(UE::GC::DeclareMember(DebugPath, Offset + Idx * sizeof(FScriptInterface), UE::GC::EMemberType::Reference) );
	}
}

void FFieldPathProperty::EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath)
{
	static_assert(sizeof(FFieldPath) == sizeof(TFieldPath<FProperty>), "TFieldPath should have the same size as the underlying FFieldPath");
	const int32 Offset = BaseOffset + GetOffset_ForGC();
	for (int32 Idx = 0, Num = ArrayDim; Idx < Num; ++Idx)
	{
		Schema.Add(UE::GC::DeclareMember(DebugPath, Offset + Idx * sizeof(FFieldPath), UE::GC::EMemberType::FieldPath));
	}
}

bool FOptionalProperty::ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	return ValueProperty->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

/** Both game thread and async loading can assemble schemas */
static FCriticalSection GAssembleSchemaLock;

void UClass::AssembleReferenceTokenStream(bool bForce)
{
	LLM_SCOPE(ELLMTag::GC);

	UE_CLOG(!IsInGameThread() && !IsGarbageCollectionLocked(), LogGarbage, Fatal, TEXT("AssembleReferenceTokenStream for %s called on a non-game thread while GC is not locked."), *GetFullName());

	if (ClassFlags & CLASS_Native)
	{
		AssembleReferenceTokenStreamInternal(bForce);
	}
	else
	{
		FScopeLock NonNativeLock(&GAssembleSchemaLock);
		AssembleReferenceTokenStreamInternal(bForce);
	}
}

static UE::GC::ObjectAROFn GetARO(UClass* Class)
{
	UE::GC::ObjectAROFn ARO = Class->CppClassStaticFunctions.GetAddReferencedObjects();
	check(ARO != nullptr);
	return ARO != &UObject::AddReferencedObjects ? ARO : nullptr;
}

void UClass::AssembleReferenceTokenStreamInternal(bool bForce)
{
	using namespace UE::GC;

	if (!HasAnyClassFlags(CLASS_TokenStreamAssembled) || bForce)
	{
		if (bForce)
		{
			ClassFlags &= ~CLASS_TokenStreamAssembled;
		}

		FSchemaBuilder Schema(/* don't store sizeof(class) to enable super class schema reuse */ 0);
		FSchemaView SuperSchema;
		if (UClass* SuperClass = GetSuperClass())
		{
			SuperClass->AssembleReferenceTokenStreamInternal();	
			SuperSchema = SuperClass->ReferenceSchema.Get();
			Schema.Append(SuperSchema);
		}
		const int32 NumSuperMembers = Schema.NumMembers();

		{
			FPropertyStack DebugPath;
			TArray<const FStructProperty*> EncounteredStructProps;

			// Iterate over properties defined in this class
			for (TFieldIterator<FProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty* Property = *It;
				FPropertyStackScope PropertyScope(DebugPath, Property);
				Property->EmitReferenceInfo(Schema, 0, EncounteredStructProps, DebugPath);
			}
		}
		
		if (ClassFlags & CLASS_Intrinsic)
		{
			Schema.Append(UE::GC::GetIntrinsicSchema(this));
		}
		
		// Make sure all Blueprint properties are marked as non-native
		// @todo Currently native super class properties of BP base classes are incorrectly marked as Blueprint
		// @todo Investigate if CLASS_CompiledFromBlueprint is better to avoid reference eliminating non-native non-blueprint properties
		EOrigin Origin = GetClass()->HasAnyClassFlags(CLASS_NeedsDeferredDependencyLoading) ? EOrigin::Blueprint : EOrigin::Other;

		bool bReuseSuper = Schema.NumMembers() == NumSuperMembers && NumSuperMembers > 0 && GetARO(this) == GetARO(GetSuperClass());
		FSchemaView View(bReuseSuper ? SuperSchema : Schema.Build(GetARO(this)), Origin);
		ReferenceSchema.Set(View);
		
		checkf(!HasAnyClassFlags(CLASS_TokenStreamAssembled), TEXT("GC schema already assembled for class '%s'"), *GetPathName()); // recursion here is probably bad
		ClassFlags |= CLASS_TokenStreamAssembled;
	}
}

//////////////////////////////////////////////////////////////////////////
namespace UE::GC
{
#if !UE_BUILD_SHIPPING

static void DumpMemoryStats(FOutputDevice& OutputDevice)
{
	FContextPoolScope Pool;
	uint32 TotalWeakCapacity = 0;
	uint32 NumContexts = Pool.PeekFree().Num();
	for (const TUniquePtr<FWorkerContext>& Context : Pool.PeekFree())
	{
		TotalWeakCapacity += Context->WeakReferences.Max();
	}

	uint32 NumSchemaWords = 0;
	uint32 NumSchemas = CountSchemas(NumSchemaWords);
	uint32 SchemaKB = (NumSchemas * sizeof(FSchemaHeader) + NumSchemaWords * 8) / 1024;
	uint32 ContextsKB = (NumContexts * sizeof(FWorkerContext) + TotalWeakCapacity * sizeof(UObject**))/1024;
	uint32 ScratchKB = static_cast<int32>(GScratchPages.CountBytes() / 1024);
	uint32 GlobalsKB = (sizeof(GScratchPages) + sizeof(GAROBlocks) + sizeof(GWorkstealingManager) + sizeof(GSlowARO)) / 1024;
	
	uint32 TotalKB = ContextsKB + ScratchKB + GlobalsKB + SchemaKB;

	OutputDevice.Logf(TEXT("GC mem usage: %dKB total, %dKB scratch, %dKB for %d contexts and %dKB globals (code size) and %dKB for %d schemas"),
						TotalKB, ScratchKB, ContextsKB, NumContexts, GlobalsKB, SchemaKB, NumSchemas);
}

static FAutoConsoleCommandWithOutputDevice GDumpMemoryStats(TEXT("gc.DumpMemoryStats"), TEXT("Print GC memory usage"), FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&DumpMemoryStats));

#endif //!UE_BUILD_SHIPPING
//////////////////////////////////////////////////////////////////////////

// Coordinates worker starting, tail spinning and stopping
class FWorkCoordinator
{
	class FStealableContext : public std::atomic<FWorkerContext*>
	{
		// Put each atomic on its own cache line to avoid false sharing
		char CacheLinePadding[PLATFORM_CACHE_LINE_SIZE - sizeof(std::atomic<FWorkerContext*>)];
	};
	static_assert(sizeof(FStealableContext) == PLATFORM_CACHE_LINE_SIZE);

	const TArrayView<FStealableContext> Contexts;
	// Ensure the immutable Contexts is on a different cache line than NumUsedContexts 
	char CacheLinePadding[PLATFORM_CACHE_LINE_SIZE - sizeof(Contexts)]; 

	std::atomic<int32> NumUsedContexts {0};
	std::atomic<int32> NumWorkless{0}; // Number of workless workers spinning to steal work
	std::atomic<int32> NumStopDirectly; // Stop a few workless workers directly to pick up non-GC tasks
	std::atomic<int32> NumStopped{0}; // Synchronize when all workers are done

public:
	FWorkCoordinator(TArrayView<FWorkerContext*> InContexts, int32 NumTaskgraphWorkers)
	: Contexts(reinterpret_cast<FStealableContext*>(FMemory::Malloc(sizeof(FStealableContext) * InContexts.Num())), InContexts.Num())
	, NumStopDirectly(NumTaskgraphWorkers > MaxWorkers ? 0 : 1 + (InContexts.Num() > 5))
	{
		for (FStealableContext& Context : Contexts)
		{
			Context.store(InContexts[UE_PTRDIFF_TO_INT32(&Context - Contexts.GetData())], std::memory_order_relaxed);
		}
	}

	~FWorkCoordinator()
	{
		check(NumUsedContexts == Contexts.Num());
		for (FStealableContext& Context : Contexts)
		{
			check(Context.load(std::memory_order_relaxed) == nullptr);
		}

		FMemory::Free(Contexts.GetData());
	}

	// @return nullptr if late to the party and initial objects / references are stolen by another worker
	// @see StealContext
	FWorkerContext* TryStartWorking(int32 WorkerIndex)
	{
		if (FWorkerContext* Out = Contexts[WorkerIndex].exchange(nullptr))
		{
			++NumUsedContexts;
			return Out;
		}

		return nullptr;
	}

	// Allows one thread to finish GC alone if task workers are busy processing long tasks
	FWorkerContext* StealContext()
	{
		if (NumUsedContexts.load(std::memory_order_relaxed) < Contexts.Num())
		{
			for (FStealableContext& Context : Contexts)
			{
				if (FWorkerContext* Out = Context.exchange(nullptr))
				{
					++NumUsedContexts;
					++NumWorkless;
					++NumStopped;
					--NumStopDirectly;
					return Out;
				}
			}	
		}

		return nullptr;	
	}

	// @return if worker should start spinning
	// 
	// Some worker threads stop immediately to reduce context switching with render flipping and audio mixing threads
	bool ReportOutOfWork()
	{
		++NumWorkless;

		// Keep main thread spinning to reduce risk that it is swapped out when all work is done
		if (IsInGameThread() || --NumStopDirectly < 0)
		{
			return true;
		}
		
		++NumStopped;
		return false;
	}

	// Notify that current work successfully stole some work
	void ReportBackToWork()
	{
		--NumWorkless;
	}
	
	// There's an acceptable race condition when some worker steals from the last worker, 
	// which reports out-of-work before the thief reports back-to-work. Other workers
	// might then stop prematurely.
	// 
	// This is fairly unlikely, would likely happen very late when little work remains
	// and the remaining workers might not yield more stealable work anyway.
	bool KeepSpinning()
	{
		if (NumWorkless.load(std::memory_order_acquire) < Contexts.Num())
		{
			return true;
		}

		++NumStopped;
		return false;
	}

	void Suspend()
	{
		++NumStopped;
	}

	FORCENOINLINE void SpinUntilAllStopped()
	{
		while (NumStopped.load() < Contexts.Num())
		{
			FPlatformProcess::Yield();
		}
	}
};

void SuspendWork(FWorkerContext& Context)
{
	Context.bIsSuspended = true;
	if (Context.Coordinator)
	{
		Context.Coordinator->Suspend();
	}
}

ELoot StealWork(FWorkerContext& Context, FReferenceCollector& Collector, FWorkBlock*& OutBlock, EGCOptions Options)
{
	FWorkBlockifier& RemainingObjects = Context.ObjectsToSerialize;
	FWorkCoordinator& Tailspin = *Context.Coordinator;

	if (OutBlock = RemainingObjects.StealFullBlock(); OutBlock)
	{
		return ELoot::Block;
	}
	else if (FSlowARO::ProcessAllCalls(Context, Collector))
	{
		return ELoot::ARO;
	}
	else if (!(Options & EGCOptions::IncrementalReachability))
	{
		if (FWorkerContext* StolenContext = Tailspin.StealContext())
		{
			Context.InitialNativeReferences = StolenContext->InitialNativeReferences;
			Context.SetInitialObjectsPrepadded(StolenContext->GetInitialObjects());
			return ELoot::Context;
		}
	}
	if (Tailspin.ReportOutOfWork())
	{
		while (Tailspin.KeepSpinning())
		{
			FPlatformProcess::Yield();

			OutBlock = RemainingObjects.StealFullBlock();
			if (OutBlock || FSlowARO::ProcessAllCalls(Context, Collector))
			{
				Tailspin.ReportBackToWork();
				return OutBlock ? ELoot::Block : ELoot::ARO;
			}

			if (GReachabilityState.IsTimeLimitExceeded())
			{
				SuspendWork(Context);
				break;
			}
		}
	}

	return ELoot::Nothing;
}

TArrayView<FWorkerContext*> InitializeAsyncProcessingContexts(FWorkerContext& InContext)
{
	TArrayView<FWorkerContext*> Contexts;

	if (!GReachabilityState.IsSuspended())
	{
		FContextPoolScope ContextPool;

		checkf(InContext.ObjectsToSerialize.IsUnused(), TEXT("Use InitialObjects instead, ObjectsToSerialize.Add() may only be called during reference processing"));
		check(InContext.Stats.NumObjects == 0 && InContext.Stats.NumReferences == 0 && InContext.Stats.bFoundGarbageRef == false);

		const int32 NumTaskgraphWorkers = FTaskGraphInterface::Get().GetNumWorkerThreads();
		const int32 NumWorkers = FMath::Clamp(NumTaskgraphWorkers, 1, MaxWorkers);

		GReachabilityState.SetupWorkers(NumWorkers);

		// Allocate contexts	
		checkf(ContextPool.NumAllocated() == 1, TEXT("Other contexts forbidden during parallel reference collection. Work-stealing from all live contexts. "));

		Contexts = MakeArrayView(GReachabilityState.GetContextArray(), GReachabilityState.GetNumWorkers());
		Contexts[0] = &InContext;
		for (FWorkerContext*& Context : Contexts.RightChop(1))
		{
			Context = ContextPool.AllocateFromPool();
		}

		GSlowARO.GetPostInit().SetupWorkerQueues(NumWorkers);

		// Setup work-stealing queues
		for (FWorkerContext* Context : Contexts)
		{
			const int32 Idx = Context->GetWorkerIndex();
			check(Idx >= 0 && Idx < NumWorkers);
			Context->ObjectsToSerialize.SetAsyncQueue(GWorkstealingManager.Queues[Idx]);
			checkf(!Context->IncrementalStructs.ContainsBatchData(), TEXT("Reachability analysis is done but worker context holds suspended dispatcher state"));
		}
	}
	else
	{
		Contexts = MakeArrayView(GReachabilityState.GetContextArray(), GReachabilityState.GetNumWorkers());
	}

	TConstArrayView<UObject*> InitialObjects = InContext.GetInitialObjects();
	TConstArrayView<UObject**> InitialReferences = InContext.InitialNativeReferences;
	const int32 ObjPerWorker = (InitialObjects.Num() + GReachabilityState.GetNumWorkers() - 1) / GReachabilityState.GetNumWorkers();
	const int32 RefPerWorker = (InitialReferences.Num() + GReachabilityState.GetNumWorkers() - 1) / GReachabilityState.GetNumWorkers();

	// Distribute initial workload across worker contexts
	for (FWorkerContext* Context : Contexts)
	{
		const int32 Idx = Context->GetWorkerIndex();
		// Initial objects is already padded at the end and its safe to prefetch in the middle too
		Context->SetInitialObjectsPrepadded(InitialObjects.Mid(Idx * ObjPerWorker, ObjPerWorker));
		Context->InitialNativeReferences = InitialReferences.Mid(Idx * RefPerWorker, RefPerWorker);
	}

	return Contexts;
}

void ReleaseAsyncProcessingContexts(FWorkerContext& InContext, TArrayView<FWorkerContext*> Contexts)
{
	FContextPoolScope ContextPool;

	// Tear down contexts and work-stealing queues
	for (FWorkerContext* Context : Contexts)
	{
		Context->Coordinator = nullptr;
		GWorkstealingManager.Queues[Context->GetWorkerIndex()].CheckEmpty();
		Context->ObjectsToSerialize.ResetAsyncQueue();
	}

	GSlowARO.GetPostInit().ResetWorkerQueues();

	for (FWorkerContext* Context : Contexts.RightChop(1))
	{
		InContext.Stats.AddStats(Context->Stats);
		ContextPool.ReturnToPool(Context);
	}
}

void ProcessAsync(void (*ProcessSync)(void*, FWorkerContext&), void* Processor, FWorkerContext& InContext)
{
	using namespace UE::GC;

	TArrayView<FWorkerContext*> Contexts = InitializeAsyncProcessingContexts(InContext);

	TSharedRef<FWorkCoordinator> WorkCoordinator = MakeShared<FWorkCoordinator>(Contexts, FTaskGraphInterface::Get().GetNumWorkerThreads());
	for (FWorkerContext* Context : Contexts)
	{
		Context->bDidWork = false;
		Context->Coordinator = &WorkCoordinator.Get();
	}

	// Kick workers
	for (int32 Idx = 1; Idx < GReachabilityState.GetNumWorkers(); ++Idx)
	{
		Tasks::Launch(TEXT("CollectReferences"), [=]() 
			{
				if (FWorkerContext* Context = WorkCoordinator->TryStartWorking(Idx))
				{
					ProcessSync(Processor, *Context);
				}
			});
	}

	// Start working ourselves
	if (FWorkerContext* Context = WorkCoordinator->TryStartWorking(0))
	{
		ProcessSync(Processor, *Context);
	}

	// Wait until all work is complete. Current thread can steal and complete everything 
	// alone if task workers are busy with long-running tasks.
	WorkCoordinator->SpinUntilAllStopped();
	
	for (FWorkerContext* Context : Contexts)
	{
		// Reset initial object sets so that we don't process again them in the next iteration or when we're done processing references
		Context->ResetInitialObjects();
		Context->InitialNativeReferences = TConstArrayView<UObject**>();

		// Update contexts' suspended state. This is necessary because some context may have their work stolen and never started (resumed) work.
		if (Context->ObjectsToSerialize.HasWork())
		{
			if (!Context->bDidWork)
			{
				if (GReachabilityState.IsTimeLimitExceeded())
				{
					Context->bIsSuspended = true;
				}
				else
				{
					// Rare case where this context's work has been completely dropped because incremental reachability does not support context stealing
					WorkCoordinator->TryStartWorking(Context->GetWorkerIndex());
					ProcessSync(Processor, *Context);
					checkf(!Context->ObjectsToSerialize.HasWork(), TEXT("GC Context %d was processed but it stil has unfinished work"), Context->GetWorkerIndex());
				}
			}
			check(GReachabilityState.IsTimeLimitExceeded() || !Context->ObjectsToSerialize.HasWork());
		}
		else if (!GReachabilityState.IsTimeLimitExceeded()) // !WithTimeLimit
		{
			// This context's work has been stolen by another context so it never had a chance to spin and clear its bIsSuspended flag
			Context->bIsSuspended = false;
		}
	}

	if (!GReachabilityState.CheckIfAnyContextIsSuspended())
	{
		ReleaseAsyncProcessingContexts(InContext, Contexts);		
	}
}

//////////////////////////////////////////////////////////////////////////

} // namespace UE::GC
 
int32 GetNumCollectReferenceWorkers()
{
	return FMath::Clamp(FTaskGraphInterface::Get().GetNumWorkerThreads(), 1, UE::GC::MaxWorkers);
}
