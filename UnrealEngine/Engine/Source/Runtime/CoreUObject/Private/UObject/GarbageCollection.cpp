// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjGC.cpp: Unreal object garbage collection code.
=============================================================================*/

#include "UObject/GarbageCollection.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/TimeGuard.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
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

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

// FastReferenceCollector uses PERF_DETAILED_PER_CLASS_GC_STATS
#include "UObject/FastReferenceCollector.h"

DEFINE_LOG_CATEGORY(LogGarbage);

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
static double GLastGCTime = 0;

/** Helpful constant for determining how many token slots we need to store a pointer **/
static const uint32 GNumTokensPerPointer = sizeof(void*) / sizeof(uint32); //-V514

bool GIsGarbageCollecting = false;

int32 FGCReferenceTokenStream::MaxStackSize = 1;

const FGCReferenceInfo FGCReferenceInfo::EndOfStreamToken(GCRT_EndOfStream, 0);

/**
* Call back into the async loading code to inform of the destruction of serialized objects
*/
void NotifyUnreachableObjects(const TArrayView<FUObjectItem*>& UnreachableObjects);

enum FGCLockBehavior : int32
{
	Default = 0,
	Legacy = 1,
};
static int32 GGCLockBehavior = FGCLockBehavior::Default;
static FAutoConsoleVariable CVarGCLockBehavior(
	TEXT("gc.LockBehavior"),
	GGCLockBehavior,
	TEXT("Set the GC lock behavior: 0=Default, 1=Legacy (more restrictive and less performant)."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		GGCLockBehavior = FMath::Clamp(Variable->GetInt(), 0, 1);
	}),
	ECVF_Default);

/** Locks all UObject hash tables for performing GC reachability analysis.
 * Can be queried with IsGarbageCollectingAndLockingUObjectHashTables().
 * */
class FGCHashTableScopeLock
{
	bool bEnabled = (GGCLockBehavior == FGCLockBehavior::Default);
public:

	FORCEINLINE FGCHashTableScopeLock()
	{
		if (bEnabled)
		{
			GIsGarbageCollectingAndLockingUObjectHashTables = true;
			LockUObjectHashTables();
		}
	}
	FORCEINLINE ~FGCHashTableScopeLock()
	{
		if (bEnabled)
		{
			UnlockUObjectHashTables();
			GIsGarbageCollectingAndLockingUObjectHashTables = false;
		}
	}
};

/** Locks all UObject hash tables when performing GC */
class FGCLegacyHashTableScopeLock
{
	bool bEnabled = (GGCLockBehavior == FGCLockBehavior::Legacy);
	/** Previous value of the GetGarbageCollectingFlag() */
	bool bPreviousGabageCollectingFlagValue = GIsGarbageCollectingAndLockingUObjectHashTables;
public:

	/**
	 * We're storing the value of GetGarbageCollectingFlag in the constructor, it's safe as only
	 * one thread is ever going to be setting it and calling this code - the game thread.
	 **/
	FORCEINLINE FGCLegacyHashTableScopeLock()
	{
		if (bEnabled)
		{
			LockUObjectHashTables();
			GIsGarbageCollectingAndLockingUObjectHashTables = true;
		}
	}
	FORCEINLINE ~FGCLegacyHashTableScopeLock()
	{
		if (bEnabled)
		{
			GIsGarbageCollectingAndLockingUObjectHashTables = bPreviousGabageCollectingFlagValue;
			UnlockUObjectHashTables();
		}
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

// Minimum number of objects to spawn a GC sub-task for
static int32 GMinDesiredObjectsPerSubTask = 128;
static FAutoConsoleVariableRef CVarMinDesiredObjectsPerSubTask(
	TEXT("gc.MinDesiredObjectsPerSubTask"),
	GMinDesiredObjectsPerSubTask,
	TEXT("Minimum number of objects to spawn a GC sub-task for."),
	ECVF_Default
	);

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

#if UE_BUILD_SHIPPING
static constexpr int32 GGarbageReferenceTrackingEnabled = 0;
#else
int32 GGarbageReferenceTrackingEnabled = 0;
static FAutoConsoleVariableRef CGarbageReferenceTrackingEnabled(
	TEXT("gc.GarbageReferenceTrackingEnabled"),
	GGarbageReferenceTrackingEnabled,
	TEXT("Causes the Garbage Collector to track and log unreleased garbage objects. If 1, will dump every reference. If 2, will dump a sample of the references to highlight problematic properties."),
	ECVF_Default
);
#endif

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand CmdCalculateTokenStreamSize(
	TEXT("gc.CalculateTokenStreamSize"),
	TEXT(""),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			int64 TokenSize = 0;
			int64 DebugSize = 0;
			int32 NumClasses = 0;
			for (TObjectIterator<UClass> It; It; ++It)
			{
				TokenSize += It->ReferenceTokenStream.GetTokenAllocatedSize();
				DebugSize += It->ReferenceTokenStream.GetDebugInfoAllocatedSize();
				NumClasses++;
			}
			UE_LOG(LogGarbage, Display, TEXT("Memory allocated for GC Tokens: %lldb, Debug Info: %lldb, Total: %lldb (%d classes)"), TokenSize, DebugSize, (TokenSize + DebugSize), NumClasses);
		})
);
#endif // !UE_BUILD_SHIPPING

#if PERF_DETAILED_PER_CLASS_GC_STATS
/** Map from a UClass' FName to the number of objects that were purged during the last purge phase of this class.	*/
static TMap<const FName,uint32> GClassToPurgeCountMap;
/** Map from a UClass' FName to the number of "Disregard For GC" object references followed for all instances.		*/
static TMap<const FName,uint32> GClassToDisregardedObjectRefsMap;
/** Map from a UClass' FName to the number of regular object references followed for all instances.					*/
static TMap<const FName,uint32> GClassToRegularObjectRefsMap;
/** Map from a UClass' FName to the number of cycles spent with GC.													*/
static TMap<const FName,uint32> GClassToCyclesMap;

/** Number of disregarded object refs for current object.															*/
static uint32 GCurrentObjectDisregardedObjectRefs;
/** Number of regulard object refs for current object.																*/
static uint32 GCurrentObjectRegularObjectRefs;

/**
 * Helper structure used for sorting class to count map.
 */
struct FClassCountInfo
{
	FName	ClassName;
	uint32	InstanceCount;
};

/**
 * Helper function to log the various class to count info maps.
 *
 * @param	LogText				Text to emit between number and class 
 * @param	ClassToCountMap		TMap from a class' FName to "count"
 * @param	NumItemsToList		Number of items to log
 * @param	TotalCount			Total count, if 0 will be calculated
 */
static void LogClassCountInfo( const TCHAR* LogText, TMap<const FName,uint32>& ClassToCountMap, int32 NumItemsToLog, uint32 TotalCount )
{
	// Array of class name and counts.
	TArray<FClassCountInfo> ClassCountArray;
	ClassCountArray.Empty( ClassToCountMap.Num() );

	// Figure out whether we need to calculate the total count.
	bool bNeedToCalculateCount = false;
	if( TotalCount == 0 )
	{
		bNeedToCalculateCount = true;
	}
	// Copy TMap to TArray for sorting purposes (and to calculate count if needed).
	for( TMap<const FName,uint32>::TIterator It(ClassToCountMap); It; ++It )
	{
		FClassCountInfo ClassCountInfo;
		ClassCountInfo.ClassName		= It.Key();
		ClassCountInfo.InstanceCount	= It.Value();
		ClassCountArray.Add( ClassCountInfo );
		if( bNeedToCalculateCount )
		{
			TotalCount += ClassCountInfo.InstanceCount;
		}
	}
	// Sort array by instance count.
	struct FCompareFClassCountInfo
	{
		FORCEINLINE bool operator()( const FClassCountInfo& A, const FClassCountInfo& B ) const
		{
			return B.InstanceCount < A.InstanceCount;
		}
	};
	ClassCountArray.Sort( FCompareFClassCountInfo() );

	// Log top NumItemsToLog class counts
	for( int32 Index=0; Index<FMath::Min(NumItemsToLog,ClassCountArray.Num()); Index++ )
	{
		const FClassCountInfo& ClassCountInfo = ClassCountArray[Index];
		const float Percent = 100.f * ClassCountInfo.InstanceCount / TotalCount;
		const FString PercentString = (TotalCount > 0) ? FString::Printf(TEXT("%6.2f%%"), Percent) : FString(TEXT("  N/A  "));
		UE_LOG(LogGarbage, Log, TEXT("%5d [%s] %s Class %s"), ClassCountInfo.InstanceCount, *PercentString, LogText, *ClassCountInfo.ClassName.ToString() ); 
	}

	// Empty the map for the next run.
	ClassToCountMap.Empty();
};
#endif

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
	int32 NumObjectsToDestroyOnGameThread;
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
	bool TickDestroyObjects(bool bUseTimeLimit, float TimeLimit, double StartTime)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPurge::TickDestroyObjects);
		const int32 TimeLimitEnforcementGranularityForDeletion = 100;
		int32 ProcessedObjectsCount = 0;
		bool bFinishedDestroyingObjects = true;

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
				FPlatformMisc::MemoryBarrier();
				++NumObjectsToDestroyOnGameThread;
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
	bool TickDestroyGameThreadObjects(bool bUseTimeLimit, float TimeLimit, double StartTime)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPurge::TickDestroyGameThreadObjects);
		const int32 TimeLimitEnforcementGranularityForDeletion = 100;
		int32 ProcessedObjectsCount = 0;
		bool bFinishedDestroyingObjects = true;

		// Lock once for the entire batch
		GUObjectArray.LockInternalArray();

		// Cache the number of objects to destroy locally. The number may grow later but that's ok, we'll catch up to it in the next tick
		const int32 LocalNumObjectsToDestroyOnGameThread = NumObjectsToDestroyOnGameThread;

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
	void TickPurge(bool bUseTimeLimit, float TimeLimit, double StartTime)
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

/** Called on shutdown to free GC memory */
void ShutdownGarbageCollection()
{
	FGCArrayPool::Get().Cleanup();
	delete GAsyncPurge;
	GAsyncPurge = nullptr;
#if ENABLE_GC_HISTORY
	FGCHistory::Get().Cleanup();
#endif
}

/**
* Handles UObject references found by TFastReferenceCollector
*/

#if UE_WITH_GC
template <EFastReferenceCollectorOptions Options>
class FGCReferenceProcessor
{
	// Local copies of globals to make sure they're cached
	const int32 MinDesiredObjectsPerSubTask;
	const bool bGarbageReferenceTrackingEnabled;
#if ENABLE_GC_HISTORY
	const bool bWithHistory;
#endif // ENABLE_GC_HISTORY

public:

	constexpr static FORCEINLINE bool IsParallel()
	{
		return !!(Options & EFastReferenceCollectorOptions::Parallel);
	}
	constexpr static FORCEINLINE bool IsWithClusters()
	{
		return !!(Options & EFastReferenceCollectorOptions::WithClusters);
	}
	constexpr static FORCEINLINE bool IsWithPendingKill()
	{
		return !!(Options & EFastReferenceCollectorOptions::WithPendingKill);
	}

	FGCReferenceProcessor()
		: MinDesiredObjectsPerSubTask(GMinDesiredObjectsPerSubTask)
		, bGarbageReferenceTrackingEnabled(!!GGarbageReferenceTrackingEnabled)
#if ENABLE_GC_HISTORY
		, bWithHistory(FGCHistory::Get().IsActive())
#endif // ENABLE_GC_HISTORY
	{
	}

	FORCEINLINE int32 GetMinDesiredObjectsPerSubTask() const
	{
		return MinDesiredObjectsPerSubTask;
	}

	void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		// Keep track of how many refs we encountered for the object's class.
		const FName& ClassName = CurrentObject->GetClass()->GetFName();
		// Refs to objects that reside in permanent object pool.
		uint32 ClassDisregardedObjRefs = GClassToDisregardedObjectRefsMap.FindRef(ClassName);
		GClassToDisregardedObjectRefsMap.Add(ClassName, ClassDisregardedObjRefs + GCurrentObjectDisregardedObjectRefs);
		// Refs to regular objects.
		uint32 ClassRegularObjRefs = GClassToRegularObjectRefsMap.FindRef(ClassName);
		GClassToRegularObjectRefsMap.Add(ClassName, ClassRegularObjRefs + GCurrentObjectRegularObjectRefs);
		// Track per class cycle count spent in GC.
		uint32 ClassCycles = GClassToCyclesMap.FindRef(ClassName);
		GClassToCyclesMap.Add(ClassName, ClassCycles + DeltaCycles);
		// Reset current counts.
		GCurrentObjectDisregardedObjectRefs = 0;
		GCurrentObjectRegularObjectRefs = 0;
#endif
	}

	void LogDetailedStatsSummary()
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		LogClassCountInfo(TEXT("references to regular objects from"), GClassToRegularObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("references to permanent objects from"), GClassToDisregardedObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("cycles for GC"), GClassToCyclesMap, 20, 0);
#endif
	}

	/** Marks all objects that can't be directly in a cluster but are referenced by it as reachable */
	static FORCENOINLINE bool MarkClusterMutableObjectsAsReachable(FUObjectCluster& Cluster, TArray<UObject*>& ObjectsToSerialize)
	{
		check(IsWithClusters());

		// This is going to be the return value and basically means that we ran across some pending kill objects
		bool bAddClusterObjectsToSerialize = false;
		for (int32& ReferencedMutableObjectIndex : Cluster.MutableObjects)
		{
			if (ReferencedMutableObjectIndex >= 0) // Pending kill support
			{
				FUObjectItem* ReferencedMutableObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectIndex);
				if (IsParallel())
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					if (!ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage))
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
					{
						if (ReferencedMutableObjectItem->IsUnreachable())
						{
							if (ReferencedMutableObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
							{
								// Needs doing because this is either a normal unclustered object (clustered objects are never unreachable) or a cluster root
								ObjectsToSerialize.Add(static_cast<UObject*>(ReferencedMutableObjectItem->Object));

								// So is this a cluster root maybe?
								if (ReferencedMutableObjectItem->GetOwnerIndex() < 0)
								{
									MarkReferencedClustersAsReachable(ReferencedMutableObjectItem->GetClusterIndex(), ObjectsToSerialize);
								}
							}
						}
						else if (ReferencedMutableObjectItem->GetOwnerIndex() > 0 && !ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
						{
							// This is a clustered object that maybe hasn't been processed yet
							if (ReferencedMutableObjectItem->ThisThreadAtomicallySetFlag(EInternalObjectFlags::ReachableInCluster))
							{
								// Needs doing, we need to get its cluster root and process it too
								FUObjectItem* ReferencedMutableObjectsClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectItem->GetOwnerIndex());
								if (ReferencedMutableObjectsClusterRootItem->IsUnreachable())
								{
									// The root is also maybe unreachable so process it and all the referenced clusters
									if (ReferencedMutableObjectsClusterRootItem->ThisThreadAtomicallyClearedRFUnreachable())
									{
										MarkReferencedClustersAsReachable(ReferencedMutableObjectsClusterRootItem->GetClusterIndex(), ObjectsToSerialize);
									}
								}
							}
						}
					}
					else
					{
						// Pending kill support for clusters (multi-threaded case)
						ReferencedMutableObjectIndex = -1;
						bAddClusterObjectsToSerialize = true;
					}
				}
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				else if (!ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage))
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					if (ReferencedMutableObjectItem->IsUnreachable())
					{
						// Needs doing because this is either a normal unclustered object (clustered objects are never unreachable) or a cluster root
						ReferencedMutableObjectItem->ClearFlags(EInternalObjectFlags::Unreachable);
						ObjectsToSerialize.Add(static_cast<UObject*>(ReferencedMutableObjectItem->Object));

						// So is this a cluster root?
						if (ReferencedMutableObjectItem->GetOwnerIndex() < 0)
						{
							MarkReferencedClustersAsReachable(ReferencedMutableObjectItem->GetClusterIndex(), ObjectsToSerialize);
						}
					}
					else if (ReferencedMutableObjectItem->GetOwnerIndex() > 0 && !ReferencedMutableObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
					{
						// This is a clustered object that hasn't been processed yet
						ReferencedMutableObjectItem->SetFlags(EInternalObjectFlags::ReachableInCluster);
						
						// If the root is also unreachable, process it and all its referenced clusters
						FUObjectItem* ReferencedMutableObjectsClusterRootItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectItem->GetOwnerIndex());
						if (ReferencedMutableObjectsClusterRootItem->IsUnreachable())
						{
							ReferencedMutableObjectsClusterRootItem->ClearFlags(EInternalObjectFlags::Unreachable);
							MarkReferencedClustersAsReachable(ReferencedMutableObjectsClusterRootItem->GetClusterIndex(), ObjectsToSerialize);
						}
					}
				}
				else
				{
					// Pending kill support for clusters (single-threaded case)
					ReferencedMutableObjectIndex = -1;
					bAddClusterObjectsToSerialize = true;
				}
			}
		}
		return bAddClusterObjectsToSerialize;
	}

	/** Marks all clusters referenced by another cluster as reachable */
	static FORCENOINLINE void MarkReferencedClustersAsReachable(int32 ClusterIndex, TArray<UObject*>& ObjectsToSerialize)
	{
		check(IsWithClusters());

		// If we run across some PendingKill objects we need to add all objects from this cluster
		// to ObjectsToSerialize so that we can properly null out all the references.
		// It also means this cluster will have to be dissolved because we may no longer guarantee all cross-cluster references are correct.

		bool bAddClusterObjectsToSerialize = false;
		FUObjectCluster& Cluster = GUObjectClusters[ClusterIndex];
		// Also mark all referenced objects from outside of the cluster as reachable
		for (int32& ReferncedClusterIndex : Cluster.ReferencedClusters)
		{
			if (ReferncedClusterIndex >= 0) // Pending Kill support
			{
				FUObjectItem* ReferencedClusterRootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferncedClusterIndex);
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (!ReferencedClusterRootObjectItem->HasAnyFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage))
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					// This condition should get collapsed by the compiler based on the template argument
					if (IsParallel())
					{
						if (ReferencedClusterRootObjectItem->IsUnreachable())
						{
							ReferencedClusterRootObjectItem->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::Unreachable);
						}
					}
					else
					{
						ReferencedClusterRootObjectItem->ClearFlags(EInternalObjectFlags::Unreachable);
					}
				}
				else
				{
					// Pending kill support for clusters
					ReferncedClusterIndex = -1;
					bAddClusterObjectsToSerialize = true;
				}
			}
		}
		if (MarkClusterMutableObjectsAsReachable(Cluster, ObjectsToSerialize))
		{
			bAddClusterObjectsToSerialize = true;
		}
		if (bAddClusterObjectsToSerialize)
		{
			// We need to process all cluster objects to handle PendingKill objects we nulled out (-1) from the cluster.
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

	FORCENOINLINE void HandleGarbageReference(FGCArrayStruct& ObjectsToSerializeStruct, const UObject* const ReferencingObject, UObject*& Object, const int32 TokenIndex)
	{
#if !UE_BUILD_SHIPPING
		FName PropertyName;
		UObject* GarbageReferencer = ReferencingObject ? const_cast<UObject*>(ReferencingObject) : ObjectsToSerializeStruct.GetReferencingObject();
		if (IsValid(GarbageReferencer))
		{
			if (GarbageReferencer != FGCObject::GGCObjectReferencer)
			{
				if (TokenIndex >= 0)
				{
					UClass* Class = GarbageReferencer->GetClass();
					FTokenInfo TokenInfo = Class->ReferenceTokenStream.GetTokenInfo(TokenIndex);
					PropertyName = TokenInfo.Name;
				}
				ObjectsToSerializeStruct.GarbageReferences.Add(FGarbageReferenceInfo(GarbageReferencer, Object, Object, PropertyName));
			}
			else
			{
				FGCObject* GCObjectReferencer = FGCObject::GGCObjectReferencer->GetCurrentlySerializingObject();
				ObjectsToSerializeStruct.GarbageReferences.Add(FGarbageReferenceInfo(GCObjectReferencer, Object, Object));
			}
		}
#endif
	}

#if ENABLE_GC_HISTORY
	FORCEINLINE void HandleHistoryReference(FGCArrayStruct& ObjectsToSerializeStruct, const UObject* const ReferencingObject, UObject*& Object, const int32 TokenIndex)
	{
		UObject* Referencer = ReferencingObject ? const_cast<UObject*>(ReferencingObject) : ObjectsToSerializeStruct.GetReferencingObject();		
		FGCDirectReference Ref(Object);

		if (TokenIndex >= 0)
		{
			UClass* Class = Referencer->GetClass();	
			Ref.ReferencerName = Class->ReferenceTokenStream.GetTokenInfo(TokenIndex).Name;
		}
		else if (FGCObject::GGCObjectReferencer == Referencer)
		{
			Ref.ReferencerName = *FGCObject::GGCObjectReferencer->GetCurrentlySerializingObject()->GetReferencerName();
		}

		TArray<FGCDirectReference>*& DirectReferences = ObjectsToSerializeStruct.History.FindOrAdd(Referencer);
		if (!DirectReferences)
		{
			DirectReferences = new TArray<FGCDirectReference>();
		}
		DirectReferences->Add(Ref);
	}
#endif // ENABLE_GC_HISTORY

	/**
	 * Handles object reference, potentially NULL'ing
	 *
	 * @param Object						Object pointer passed by reference
	 * @param ReferencingObject UObject which owns the reference (can be NULL)
	 * @param bAllowReferenceElimination	Whether to allow NULL'ing the reference if RF_PendingKill is set
	*/
	FORCEINLINE void HandleObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, const UObject * const ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, const bool bAllowReferenceElimination)
	{
		if (Object == nullptr)
		{
			return;
		}

		TArray<UObject*>& ObjectsToSerialize = ObjectsToSerializeStruct.ObjectsToSerialize;

		// Disregard NULL objects and perform very fast check to see whether object is part of permanent
		// object pool and should therefore be disregarded. The check doesn't touch the object and is
		// cache friendly as it's just a pointer compare against to globals.
		if (GUObjectAllocator.ResidesInPermanentPool(Object))
		{
#if PERF_DETAILED_PER_CLASS_GC_STATS
			GCurrentObjectDisregardedObjectRefs++;
#endif
			return;
		}

		const int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ObjectIndex);

		// Check for garbage objects but skip objects that were already marked as persistent garbage
		if (!IsWithPendingKill() && ObjectItem->HasAnyFlags(EInternalObjectFlags::Garbage))
		{
			if (bAllowReferenceElimination)
			{
				// To avoid content changes, allow reference elimination inside of Blueprints
				if (ReferencingObject && TokenType == EGCTokenType::NonNative)
				{
					Object = nullptr;
					return;
				}

				// Disable checking this flag because it's too aggressive at removing references from reporting.
				if (bGarbageReferenceTrackingEnabled /*&& !ObjectItem->HasAnyFlags(EInternalObjectFlags::PersistentGarbage)*/)
				{
					HandleGarbageReference(ObjectsToSerializeStruct, ReferencingObject, Object, TokenIndex);
				}
			}
			else if (bGarbageReferenceTrackingEnabled)
			{
				// This object is being referenced by a persistent reference which means it wouldn't have been GC'd anyway
				// so no need to track this object
				ObjectItem->ThisThreadAtomicallySetFlag(EInternalObjectFlags::PersistentGarbage);
			}
		}

		// Remove references to pending kill objects if we're allowed to do so.
		if (IsWithPendingKill() && ObjectItem->IsPendingKill() && bAllowReferenceElimination)
		{
			checkSlow(ObjectItem->GetOwnerIndex() <= 0);

			// Null out reference.
			Object = nullptr;
		}
		// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
		else if (ObjectItem->IsUnreachable())
		{
			if (IsParallel())
			{
				// Mark it as reachable.
				if (ObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
				{
#if ENABLE_GC_HISTORY
					if (bWithHistory)
					{
						HandleHistoryReference(ObjectsToSerializeStruct, ReferencingObject, Object, TokenIndex);
					}
#endif // ENABLE_GC_HISTORY

					// Objects that are part of a GC cluster should never have the unreachable flag set!
					checkSlow(ObjectItem->GetOwnerIndex() <= 0);

					if (!IsWithClusters() || !ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
					{
						// Add it to the list of objects to serialize.
						ObjectsToSerialize.Add(Object);
					}
					else
					{
						// This is a cluster root reference so mark all referenced clusters as reachable
						MarkReferencedClustersAsReachable(ObjectItem->GetClusterIndex(), ObjectsToSerialize);
					}
				}
			}
			else
			{
#if ENABLE_GC_DEBUG_OUTPUT
				// this message is to help track down culprits behind "Object in PIE world still referenced" errors
				if (GIsEditor && !GIsPlayInEditorWorld && ReferencingObject != nullptr && Object != nullptr && !ReferencingObject->HasAnyFlags(RF_Transient) && Object->RootPackageHasAnyFlags(PKG_PlayInEditor))
				{
					UPackage* ReferencingPackage = ReferencingObject->GetOutermost();
					if (!ReferencingPackage->HasAnyPackageFlags(PKG_PlayInEditor) && !ReferencingPackage->HasAnyFlags(RF_Transient))
					{
						UE_LOG(LogGarbage, Warning, TEXT("GC detected illegal reference to PIE object from content [possibly via [todo]]:"));
						UE_LOG(LogGarbage, Warning, TEXT("      PIE object: %s"), *Object->GetFullName());
						UE_LOG(LogGarbage, Warning, TEXT("  NON-PIE object: %s"), *ReferencingObject->GetFullName());
					}
				}
#endif

#if ENABLE_GC_HISTORY
				if (bWithHistory)
				{
					HandleHistoryReference(ObjectsToSerializeStruct, ReferencingObject, Object, TokenIndex);
				}
#endif // ENABLE_GC_HISTORY

				// Mark it as reachable.
				ObjectItem->ClearUnreachable();

				// Objects that are part of a GC cluster should never have the unreachable flag set!
				checkSlow(ObjectItem->GetOwnerIndex() <= 0);

				if (!IsWithClusters() || !ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					// Add it to the list of objects to serialize.
					ObjectsToSerialize.Add(Object);
				}
				else
				{
					// This is a cluster root reference so mark all referenced clusters as reachable
					MarkReferencedClustersAsReachable(ObjectItem->GetClusterIndex(), ObjectsToSerialize);
				}
			}
		}
		else if (IsWithClusters() && (ObjectItem->GetOwnerIndex() > 0 && !ObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster)))
		{
			bool bNeedsDoing = true;
			if (IsParallel())
			{
				bNeedsDoing = ObjectItem->ThisThreadAtomicallySetFlag(EInternalObjectFlags::ReachableInCluster);
			}
			else
			{
				ObjectItem->SetFlags(EInternalObjectFlags::ReachableInCluster);
			}
			if (bNeedsDoing)
			{
				// Make sure cluster root object is reachable too
				const int32 OwnerIndex = ObjectItem->GetOwnerIndex();
				FUObjectItem* RootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(OwnerIndex);
				checkSlow(RootObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
				if (IsParallel())
				{
					if (RootObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
					{
						// Make sure all referenced clusters are marked as reachable too
						MarkReferencedClustersAsReachable(RootObjectItem->GetClusterIndex(), ObjectsToSerialize);
					}
				}
				else if (RootObjectItem->IsUnreachable())
				{
					RootObjectItem->ClearFlags(EInternalObjectFlags::Unreachable);
					// Make sure all referenced clusters are marked as reachable too
					MarkReferencedClustersAsReachable(RootObjectItem->GetClusterIndex(), ObjectsToSerialize);
				}
			}
		}
#if PERF_DETAILED_PER_CLASS_GC_STATS
		GCurrentObjectRegularObjectRefs++;
#endif
	}

	/**
	* Handles UObject reference from the token stream.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize.
	* @param ReferencingObject Object referencing the object to process.
	* @param Object Object being referenced.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed.
	*/
	FORCEINLINE void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
	{
#if ENABLE_GC_OBJECT_CHECKS
		if (Object && IsObjectHandleResolved(*reinterpret_cast<FObjectHandle*>(&Object)))
		{
			if (
#if DO_POINTER_CHECKS_ON_GC
				!IsPossiblyAllocatedUObjectPointer(Object) ||
#endif
				!Object->IsValidLowLevelFast())
			{
				FString TokenDebugInfo;
				if (UClass *Class = (ReferencingObject ? ReferencingObject->GetClass() : nullptr))
				{
					FTokenInfo TokenInfo = Class->ReferenceTokenStream.GetTokenInfo(TokenIndex);
					TokenDebugInfo = FString::Printf(TEXT("ReferencingObjectClass: %s, Property Name: %s, Offset: %d"),
						*Class->GetFullName(), *TokenInfo.Name.GetPlainNameString(), TokenInfo.Offset);
				}
				else
				{
					// This means this objects is most likely being referenced by AddReferencedObjects
					TokenDebugInfo = TEXT("Native Reference");
				}

				UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, %s, TokenIndex: %d"),
					(int64)(PTRINT)Object,
					ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
					*TokenDebugInfo, TokenIndex);
			}
		}
#endif // ENABLE_GC_OBJECT_CHECKS
		HandleObjectReference(ObjectsToSerializeStruct, ReferencingObject, Object, TokenIndex, TokenType, bAllowReferenceElimination);
	}
};

template <EFastReferenceCollectorOptions Options>
FGCCollector<Options>::FGCCollector(FGCReferenceProcessor<Options>& InProcessor, FGCArrayStruct& InObjectArrayStruct)
		: ReferenceProcessor(InProcessor)
		, ObjectArrayStruct(InObjectArrayStruct)
		, bAllowEliminatingReferences(true)
		, bIsProcessingNativeReferences(true)
{
}

template <EFastReferenceCollectorOptions Options>
FORCEINLINE void FGCCollector<Options>::InternalHandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty, const EGCTokenType InTokenType)
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
		const int32 InvalidTokenIndex = -1; // this reference is coming from a native AddReferencedObjects so no valid token index is available
		ReferenceProcessor.HandleObjectReference(ObjectArrayStruct, const_cast<UObject*>(ReferencingObject), Object, InvalidTokenIndex, InTokenType, bAllowEliminatingReferences);
}

template <EFastReferenceCollectorOptions Options>
void FGCCollector<Options>::HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty)
{
	const EGCTokenType TokenType = bIsProcessingNativeReferences ? EGCTokenType::Native : EGCTokenType::NonNative;
	InternalHandleObjectReference(Object, ReferencingObject, ReferencingProperty, TokenType);
}

template <EFastReferenceCollectorOptions Options>
void FGCCollector<Options>::HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const FProperty* InReferencingProperty)
{
	const EGCTokenType TokenType = bIsProcessingNativeReferences ? EGCTokenType::Native : EGCTokenType::NonNative;
	for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
	{
		UObject*& Object = InObjects[ObjectIndex];
		InternalHandleObjectReference(Object, InReferencingObject, InReferencingProperty, TokenType);
	}
}
#endif	// UE_WITH_GC

/*----------------------------------------------------------------------------
	FReferenceFinder.
----------------------------------------------------------------------------*/
FReferenceFinder::FReferenceFinder(TArray<UObject*>& InObjectArray, UObject* InOuter, bool bInRequireDirectOuter, bool bInShouldIgnoreArchetype, bool bInSerializeRecursively, bool bInShouldIgnoreTransient)
	: ObjectArray(InObjectArray)
	, LimitOuter(InOuter)
	, SerializedProperty(nullptr)
	, bRequireDirectOuter(bInRequireDirectOuter)
	, bShouldIgnoreArchetype(bInShouldIgnoreArchetype)
	, bSerializeRecursively(false)
	, bShouldIgnoreTransient(bInShouldIgnoreTransient)
{
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
		FVerySlowReferenceCollectorArchiveScope CollectorScope(GetVerySlowReferenceCollectorArchive(), InReferencingObject, SerializedProperty);
		Object->SerializeScriptProperties(CollectorScope.GetArchive());
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
			// do not attempt to serialize objects that have already been 
			if ( ObjectArray.Contains( Object ) == false )
			{
				check( Object->IsValidLowLevel() );
				ObjectArray.Add( Object );
			}

			// check this object for any potential object references
			if ( bSerializeRecursively == true && !SerializedObjects.Find(Object) )
			{
				SerializedObjects.Add(Object);
				FindReferences(Object, const_cast<UObject*>(InReferencingObject), const_cast<FProperty*>(InReferencingProperty));
			}
		}
	}
}

void FGarbageCollectionTracer::PerformReachabilityAnalysisOnObjects(FGCArrayStruct* ArrayStruct, bool bForceSingleThreaded, bool bWithClusters)
{
	const EFastReferenceCollectorOptions Options =
		(bForceSingleThreaded ? EFastReferenceCollectorOptions::None : EFastReferenceCollectorOptions::Parallel) |
		(bWithClusters ? EFastReferenceCollectorOptions::WithClusters : EFastReferenceCollectorOptions::None) |
		(UObjectBaseUtility::IsPendingKillEnabled() ? EFastReferenceCollectorOptions::WithPendingKill : EFastReferenceCollectorOptions::None);

	PerformReachabilityAnalysisOnObjects(ArrayStruct, Options);
}

/**
 * Implementation of parallel realtime garbage collector using recursive subdivision
 *
 * The approach is to create an array of uint32 tokens for each class that describe object references. This is done for 
 * script exposed classes by traversing the properties and additionally via manual function calls to emit tokens for
 * native only classes in the construction singleton IMPLEMENT_INTRINSIC_CLASS. 
 * A third alternative is a AddReferencedObjects callback per object which 
 * is used to deal with object references from types that aren't supported by the reflectable type system.
 * interface doesn't make sense to implement for.
 */

#if UE_WITH_GC

class FRealtimeGC : public FGarbageCollectionTracer
{
	typedef void(FRealtimeGC::*MarkObjectsFn)(TArray<UObject*>&, const EObjectFlags);
	typedef void(FRealtimeGC::*ReachabilityAnalysisFn)(FGCArrayStruct*);

	/** Pointers to functions used for Marking objects as unreachable */
	MarkObjectsFn MarkObjectsFunctions[4];
	/** Pointers to functions used for Reachability Analysis */
	ReachabilityAnalysisFn ReachabilityAnalysisFunctions[8];

	template <EFastReferenceCollectorOptions CollectorOptions>
	void PerformReachabilityAnalysisOnObjectsInternal(FGCArrayStruct* ArrayStruct)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PerformReachabilityAnalysisOnObjectsInternal);
		FGCReferenceProcessor<CollectorOptions> ReferenceProcessor;
		// NOTE: we want to run with automatic token stream generation off as it should be already generated at this point,
		// BUT we want to be ignoring Noop tokens as they're only pointing either at null references or at objects that never get GC'd (native classes)
		TFastReferenceCollector<
			FGCReferenceProcessor<CollectorOptions>,
			FGCCollector<CollectorOptions>,
			FGCArrayPool,
			CollectorOptions
			>  ReferenceCollector(ReferenceProcessor, FGCArrayPool::Get());
		ReferenceCollector.CollectReferences(*ArrayStruct);
	}

	/** Calculates GC function index based on current settings */
	static FORCEINLINE int32 GetGCFunctionIndex(EFastReferenceCollectorOptions InOptions)
	{
		return (!!(InOptions & EFastReferenceCollectorOptions::Parallel)) |
			(!!(InOptions & EFastReferenceCollectorOptions::WithClusters) << 1) |
			(!!(InOptions & EFastReferenceCollectorOptions::WithPendingKill) << 2);
	}

public:
	/** Default constructor, initializing all members. */
	FRealtimeGC()
	{
		MarkObjectsFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None)] = &FRealtimeGC::MarkObjectsAsUnreachable<false, false>;
		MarkObjectsFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::None)] = &FRealtimeGC::MarkObjectsAsUnreachable<true, false>;
		MarkObjectsFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithClusters)] = &FRealtimeGC::MarkObjectsAsUnreachable<false, true>;
		MarkObjectsFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::WithClusters)] = &FRealtimeGC::MarkObjectsAsUnreachable<true, true>;

		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::None>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::None)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::None>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithClusters)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithClusters>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::WithClusters)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::WithClusters>;

		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithPendingKill)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithPendingKill>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithPendingKill)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithPendingKill>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithClusters | EFastReferenceCollectorOptions::WithPendingKill)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::None | EFastReferenceCollectorOptions::WithClusters | EFastReferenceCollectorOptions::WithPendingKill>;
		ReachabilityAnalysisFunctions[GetGCFunctionIndex(EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::WithClusters | EFastReferenceCollectorOptions::WithPendingKill)] = &FRealtimeGC::PerformReachabilityAnalysisOnObjectsInternal<EFastReferenceCollectorOptions::Parallel | EFastReferenceCollectorOptions::WithClusters | EFastReferenceCollectorOptions::WithPendingKill>;

	}

	/** 
	 * Marks all objects that don't have KeepFlags and EInternalObjectFlags::GarbageCollectionKeepFlags as unreachable
	 * This function is a template to speed up the case where we don't need to assemble the token stream (saves about 6ms on PS4)
	 */
	template <bool bParallel, bool bWithClusters>
	void MarkObjectsAsUnreachable(TArray<UObject*>& ObjectsToSerialize, const EObjectFlags KeepFlags)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MarkObjectsAsUnreachable);
		const EInternalObjectFlags FastKeepFlags = EInternalObjectFlags::GarbageCollectionKeepFlags;
		const int32 MaxNumberOfObjects = GUObjectArray.GetObjectArrayNum() - GUObjectArray.GetFirstGCIndex();
		const int32 NumThreads = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
		const int32 NumberOfObjectsPerThread = (MaxNumberOfObjects / NumThreads) + 1;		

		TLockFreePointerListFIFO<FUObjectItem, PLATFORM_CACHE_LINE_SIZE> ClustersToDissolveList;
		TLockFreePointerListFIFO<FUObjectItem, PLATFORM_CACHE_LINE_SIZE> KeepClusterRefsList;
		FGCArrayStruct** ObjectsToSerializeArrays = new FGCArrayStruct*[NumThreads];
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex)
		{
			ObjectsToSerializeArrays[ThreadIndex] = FGCArrayPool::Get().GetArrayStructFromPool();
		}

		// Iterate over all objects. Note that we iterate over the UObjectArray and usually check only internal flags which
		// are part of the array so we don't suffer from cache misses as much as we would if we were to check ObjectFlags.
		ParallelFor( TEXT("GarbageCollection.PF"),NumThreads,1, [ObjectsToSerializeArrays, &ClustersToDissolveList, &KeepClusterRefsList, FastKeepFlags, KeepFlags, NumberOfObjectsPerThread, NumThreads, MaxNumberOfObjects](int32 ThreadIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MarkObjectsAsUnreachableTask);
			int32 FirstObjectIndex = ThreadIndex * NumberOfObjectsPerThread + GUObjectArray.GetFirstGCIndex();
			int32 NumObjects = (ThreadIndex < (NumThreads - 1)) ? NumberOfObjectsPerThread : (MaxNumberOfObjects - (NumThreads - 1) * NumberOfObjectsPerThread);
			int32 LastObjectIndex = FMath::Min(GUObjectArray.GetObjectArrayNum() - 1, FirstObjectIndex + NumObjects - 1);
			int32 ObjectCountDuringMarkPhase = 0;
			TArray<UObject*>& LocalObjectsToSerialize = ObjectsToSerializeArrays[ThreadIndex]->ObjectsToSerialize;

			for (int32 ObjectIndex = FirstObjectIndex; ObjectIndex <= LastObjectIndex; ++ObjectIndex)
			{
				FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];
				if (ObjectItem->Object)
				{
					UObject* Object = (UObject*)ObjectItem->Object;

					// We can't collect garbage during an async load operation and by now all unreachable objects should've been purged.
					checkf(!ObjectItem->HasAnyFlags(EInternalObjectFlags::Unreachable|EInternalObjectFlags::PendingConstruction|EInternalObjectFlags::PersistentGarbage),
							TEXT("Object: '%s' with ObjectFlags=0x%08x and InternalObjectFlags=0x%08x. ")
							TEXT("State: IsEngineExitRequested=%d, GIsCriticalError=%d, GExitPurge=%d, GObjPurgeIsRequired=%d, GObjIncrementalPurgeIsInProgress=%d, GObjFinishDestroyHasBeenRoutedToAllObjects=%d, GGCObjectsPendingDestructionCount=%d"),
							*Object->GetFullName(),
							Object->GetFlags(),
							Object->GetInternalFlags(),
							IsEngineExitRequested(),
							GIsCriticalError,
							GExitPurge,
							GObjPurgeIsRequired,
							GObjIncrementalPurgeIsInProgress,
							GObjFinishDestroyHasBeenRoutedToAllObjects,
							GGCObjectsPendingDestructionCount);

					// Keep track of how many objects are around.
					ObjectCountDuringMarkPhase++;
					
					if (bWithClusters)
					{
						ObjectItem->ClearFlags(EInternalObjectFlags::ReachableInCluster);
					}
					// Special case handling for objects that are part of the root set.
					if (ObjectItem->IsRootSet())
					{
						// IsValidLowLevel is extremely slow in this loop so only do it in debug
						checkSlow(Object->IsValidLowLevel());
						// We cannot use RF_PendingKill on objects that are part of the root set.
#if DO_GUARD_SLOW
						checkCode(if (ObjectItem->IsPendingKill()) { UE_LOG(LogGarbage, Fatal, TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName()); });
#endif
						if (bWithClusters)
						{
							if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot) || ObjectItem->GetOwnerIndex() > 0)
							{
								KeepClusterRefsList.Push(ObjectItem);
							}
						}

						LocalObjectsToSerialize.Add(Object);
					}
					// Cluster objects 
					else if (bWithClusters && ObjectItem->GetOwnerIndex() > 0)
					{
						// treat cluster objects with FastKeepFlags the same way as if they are in the root set
						if (ObjectItem->HasAnyFlags(FastKeepFlags))
						{
							KeepClusterRefsList.Push(ObjectItem);
							LocalObjectsToSerialize.Add(Object);
						}
					}
					// Regular objects or cluster root objects
					else if (!bWithClusters || ObjectItem->GetOwnerIndex() <= 0)
					{
						bool bMarkAsUnreachable = true;
						// Internal flags are super fast to check and is used by async loading and must have higher precedence than PendingKill
						if (ObjectItem->HasAnyFlags(FastKeepFlags))
						{
							bMarkAsUnreachable = false;
						}
						// If KeepFlags is non zero this is going to be very slow due to cache misses
						else if (!ObjectItem->IsPendingKill() && KeepFlags != RF_NoFlags && Object->HasAnyFlags(KeepFlags))
						{
							bMarkAsUnreachable = false;
						}
						PRAGMA_DISABLE_DEPRECATION_WARNINGS
						else if (ObjectItem->HasAnyFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Garbage) && bWithClusters && ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
						{
							ClustersToDissolveList.Push(ObjectItem);
						}

						// Mark objects as unreachable unless they have any of the passed in KeepFlags set and it's not marked for elimination..
						if (!bMarkAsUnreachable)
						{
							// IsValidLowLevel is extremely slow in this loop so only do it in debug
							checkSlow(Object->IsValidLowLevel());
							LocalObjectsToSerialize.Add(Object);

							if (bWithClusters)
							{
								if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
								{
									KeepClusterRefsList.Push(ObjectItem);
								}
							}
						}
						else
						{
							ObjectItem->SetFlags(EInternalObjectFlags::Unreachable);
						}
					}					
				}
			}

			GObjectCountDuringLastMarkPhase.Add(ObjectCountDuringMarkPhase);
		}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
		
		// Collect all objects to serialize from all threads and put them into a single array
		{
			int32 NumObjectsToSerialize = 0;
			for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex)
			{
				NumObjectsToSerialize += ObjectsToSerializeArrays[ThreadIndex]->ObjectsToSerialize.Num();
			}
			ObjectsToSerialize.Reserve(NumObjectsToSerialize);
			for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex)
			{
				ObjectsToSerialize.Append(ObjectsToSerializeArrays[ThreadIndex]->ObjectsToSerialize);
				FGCArrayPool::Get().ReturnToPool(ObjectsToSerializeArrays[ThreadIndex]);
			}
			delete[] ObjectsToSerializeArrays;
		}

		if (bWithClusters)
		{
			TArray<FUObjectItem*> ClustersToDissolve;
			ClustersToDissolveList.PopAll(ClustersToDissolve);
			for (FUObjectItem* ObjectItem : ClustersToDissolve)
			{
				// Check if the object is still a cluster root - it's possible one of the previous
				// DissolveClusterAndMarkObjectsAsUnreachable calls already dissolved its cluster
				if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					GUObjectClusters.DissolveClusterAndMarkObjectsAsUnreachable(ObjectItem);
					GUObjectClusters.SetClustersNeedDissolving();
				}
			}
		}

		if (bWithClusters)
		{
			TArray<FUObjectItem*> KeepClusterRefs;
			KeepClusterRefsList.PopAll(KeepClusterRefs);
			for (FUObjectItem* ObjectItem : KeepClusterRefs)
			{
				if (ObjectItem->GetOwnerIndex() > 0)
				{
					checkSlow(!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
					bool bNeedsDoing = !ObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster);
					if (bNeedsDoing)
					{
						ObjectItem->SetFlags(EInternalObjectFlags::ReachableInCluster);
						// Make sure cluster root object is reachable too
						const int32 OwnerIndex = ObjectItem->GetOwnerIndex();
						FUObjectItem* RootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(OwnerIndex);
						checkSlow(RootObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
						// if it is reachable via keep flags we will do this below (or maybe already have)
						if (RootObjectItem->IsUnreachable()) 
						{
							RootObjectItem->ClearFlags(EInternalObjectFlags::Unreachable);
							// Make sure all referenced clusters are marked as reachable too
							FGCReferenceProcessor<EFastReferenceCollectorOptions::WithClusters>::MarkReferencedClustersAsReachable(RootObjectItem->GetClusterIndex(), ObjectsToSerialize);
						}
					}
				}
				else
				{
					checkSlow(ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
					// this thing is definitely not marked unreachable, so don't test it here
					// Make sure all referenced clusters are marked as reachable too
					FGCReferenceProcessor<EFastReferenceCollectorOptions::WithClusters>::MarkReferencedClustersAsReachable(ObjectItem->GetClusterIndex(), ObjectsToSerialize);
				}
			}
		}
	}

	/**
	 * Performs reachability analysis.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not
	 */
	void PerformReachabilityAnalysis(EObjectFlags KeepFlags, const EFastReferenceCollectorOptions InOptions)
	{
		LLM_SCOPE(ELLMTag::GC);

		SCOPED_NAMED_EVENT(FRealtimeGC_PerformReachabilityAnalysis, FColor::Red);
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FRealtimeGC::PerformReachabilityAnalysis"), STAT_FArchiveRealtimeGC_PerformReachabilityAnalysis, STATGROUP_GC);

		/** Growing array of objects that require serialization */
		FGCArrayStruct* ArrayStruct = FGCArrayPool::Get().GetArrayStructFromPool();
		TArray<UObject*>& ObjectsToSerialize = ArrayStruct->ObjectsToSerialize;

		// Reset object count.
		GObjectCountDuringLastMarkPhase.Reset();

		// Make sure GC referencer object is checked for references to other objects even if it resides in permanent object pool
		if (FPlatformProperties::RequiresCookedData() && FGCObject::GGCObjectReferencer && GUObjectArray.IsDisregardForGC(FGCObject::GGCObjectReferencer))
		{
			ObjectsToSerialize.Add(FGCObject::GGCObjectReferencer);
		}

		{
			const double StartTime = FPlatformTime::Seconds();
			// Mark phase doesn't care about PendingKill being enabled or not so there's just fewer compiled in functions
			const EFastReferenceCollectorOptions OptionsForMarkPhase = InOptions & ~EFastReferenceCollectorOptions::WithPendingKill;
			(this->*MarkObjectsFunctions[GetGCFunctionIndex(OptionsForMarkPhase)])(ObjectsToSerialize, KeepFlags);
			UE_LOG(LogGarbage, Verbose, TEXT("%f ms for MarkObjectsAsUnreachable Phase (%d Objects To Serialize)"), (FPlatformTime::Seconds() - StartTime) * 1000, ObjectsToSerialize.Num());
		}

		{
			const double StartTime = FPlatformTime::Seconds();
			PerformReachabilityAnalysisOnObjects(ArrayStruct, InOptions);
			UE_LOG(LogGarbage, Verbose, TEXT("%f ms for Reachability Analysis"), (FPlatformTime::Seconds() - StartTime) * 1000);
		}
        
		// Allowing external systems to add object roots. This can't be done through AddReferencedObjects
		// because it may require tracing objects (via FGarbageCollectionTracer) multiple times
		FCoreUObjectDelegates::TraceExternalRootsForReachabilityAnalysis.Broadcast(*this, KeepFlags, !(InOptions & EFastReferenceCollectorOptions::Parallel));

		FGCArrayPool::Get().ReturnToPool(ArrayStruct);

#if UE_BUILD_DEBUG
		FGCArrayPool::Get().CheckLeaks();
#endif
	}

	virtual void PerformReachabilityAnalysisOnObjects(FGCArrayStruct* ArrayStruct, const EFastReferenceCollectorOptions InOptions) override
	{
		(this->*ReachabilityAnalysisFunctions[GetGCFunctionIndex(InOptions)])(ArrayStruct);
	}
};
#endif // UE_WITH_GC

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

/** Locks GC within a scope but only if it hasn't been locked already */
struct FLegacyConditionalIncrementalGCLock
{
	bool bNeedsUnlock;
	FLegacyConditionalIncrementalGCLock()
		: bNeedsUnlock(false)
	{
		if ((GGCLockBehavior == FGCLockBehavior::Legacy) && !FGCCSyncObject::Get().IsGCLocked())
		{
			AcquireGCLock();
			bNeedsUnlock = true;
		}
	}
	~FLegacyConditionalIncrementalGCLock()
	{
		if (bNeedsUnlock)
		{
			ReleaseGCLock();
		}
	}
};

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
#if !UE_WITH_GC
	return;
#else
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
		// Lock before settting GCStartTime as it could be slow to lock if async loading is in progress
		// but we still want to perform some GC work otherwise we'd be keeping objects in memory for a long time
		FLegacyConditionalIncrementalGCLock ScopedGCLock;
		TGuardValue<bool> GuardIsGarbageCollecting(GIsGarbageCollecting, true);

		// Keep track of start time to enforce time limit unless bForceFullPurge is true;
		GCStartTime = FPlatformTime::Seconds();
		bool bTimeLimitReached = false;

		if (GUnrechableObjectIndex < GUnreachableObjects.Num())
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

		// when running incrementally using a time limit, add one last tick for the memory trim
		bCompleted = bCompleted && !bUseTimeLimit;
	}
#endif // !UE_WITH_GC
}

#if UE_WITH_GC


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
	double IncrementalDestroyGarbageStartTime = FPlatformTime::Seconds();

	// Depending on platform FPlatformTime::Seconds might take a noticeable amount of time if called thousands of times so we avoid
	// enforcing the time limit too often, especially as neither Destroy nor actual deletion should take significant
	// amounts of time.
	const int32	TimeLimitEnforcementGranularityForDestroy = 10;
	const int32	TimeLimitEnforcementGranularityForDeletion = 100;

	// Set 'I'm garbage collecting' flag - might be checked inside UObject::Destroy etc.
	FGCLegacyHashTableScopeLock GCLegacyHashTableLock;
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

				check(ObjectItem->IsUnreachable());
				{
					UObject* Object = static_cast<UObject*>(ObjectItem->Object);
					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if(Object->IsReadyForFinishDestroy())
					{
#if PERF_DETAILED_PER_CLASS_GC_STATS
						// Keep track of how many objects of a certain class we're purging.
						const FName& ClassName = Object->GetClass()->GetFName();
						int32 InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
						GClassToPurgeCountMap.Add( ClassName, ++InstanceCount );
#endif
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
					check( Object != NULL && Object->IsUnreachable() );

					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if( Object->IsReadyForFinishDestroy() )
					{
#if PERF_DETAILED_PER_CLASS_GC_STATS
						// Keep track of how many objects of a certain class we're purging.
						const FName& ClassName = Object->GetClass()->GetFName();
						int32 InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
						GClassToPurgeCountMap.Add( ClassName, ++InstanceCount );
#endif
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
							((FPlatformTime::Seconds() - GCStartTime) > MaxTimeForFinishDestroy))
						{
							if (GEnableTimeoutOnPendingDestroyedObjectGC)
							{
								UE_LOG(LogGarbage, Warning, TEXT("Spent more than %.2fs on routing FinishDestroy to objects (objects in queue: %d)"), MaxTimeForFinishDestroy, GGCObjectsPendingDestructionCount);
								UObject* LastObjectNotReadyForFinishDestroy = nullptr;
								for (int32 ObjectIndex = 0; ObjectIndex < GGCObjectsPendingDestructionCount; ++ObjectIndex)
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
#if PERF_DETAILED_PER_CLASS_GC_STATS
			LogClassCountInfo( TEXT("objects of"), GClassToPurgeCountMap, 10, PurgedObjectCountSinceLastMarkPhase);
#endif
			GAsyncPurge->ResetObjectsDestroyedSinceLastMarkPhase();
		}
	}

	if (bUseTimeLimit && !bCompleted)
	{
		UE_LOG(LogGarbage, Log, TEXT("%.3f ms for incrementally purging unreachable objects (FinishDestroyed: %d, Destroyed: %d / %d)"),
			(FPlatformTime::Seconds() - IncrementalDestroyGarbageStartTime) * 1000,
			GObjCurrentPurgeObjectIndex,
			GAsyncPurge->GetObjectsDestroyedSinceLastMarkPhase(),
			GUnreachableObjects.Num());
	}

	return bCompleted;
}
#endif // UE_WITH_GC

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

void GatherUnreachableObjects(bool bForceSingleThreaded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GatherUnreachableObjects);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.GatherUnreachableObjects"), STAT_CollectGarbageInternal_GatherUnreachableObjects, STATGROUP_GC);

	const double StartTime = FPlatformTime::Seconds();

	GUnreachableObjects.Reset();
	GUnrechableObjectIndex = 0;

	int32 MaxNumberOfObjects = GUObjectArray.GetObjectArrayNum() - (GExitPurge ? 0 : GUObjectArray.GetFirstGCIndex());
	int32 NumThreads = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	int32 NumberOfObjectsPerThread = (MaxNumberOfObjects / NumThreads) + 1;

	TArray<FUObjectItem*> ClusterItemsToDestroy;
	int32 ClusterObjects = 0;

	// Iterate over all objects. Note that we iterate over the UObjectArray and usually check only internal flags which
	// are part of the array so we don't suffer from cache misses as much as we would if we were to check ObjectFlags.
	ParallelFor( TEXT("GarbageCollection.PF"),NumThreads,1, [&ClusterItemsToDestroy, NumberOfObjectsPerThread, NumThreads, MaxNumberOfObjects](int32 ThreadIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherUnreachableObjectsTask);
		int32 FirstObjectIndex = ThreadIndex * NumberOfObjectsPerThread + (GExitPurge ? 0 : GUObjectArray.GetFirstGCIndex());
		int32 NumObjects = (ThreadIndex < (NumThreads - 1)) ? NumberOfObjectsPerThread : (MaxNumberOfObjects - (NumThreads - 1) * NumberOfObjectsPerThread);
		int32 LastObjectIndex = FMath::Min(GUObjectArray.GetObjectArrayNum() - 1, FirstObjectIndex + NumObjects - 1);
		TArray<FUObjectItem*> ThisThreadUnreachableObjects;
		TArray<FUObjectItem*> ThisThreadClusterItemsToDestroy;

		for (int32 ObjectIndex = FirstObjectIndex; ObjectIndex <= LastObjectIndex; ++ObjectIndex)
		{
			FUObjectItem* ObjectItem = &GUObjectArray.GetObjectItemArrayUnsafe()[ObjectIndex];
			if (ObjectItem->IsUnreachable())
			{
				ThisThreadUnreachableObjects.Add(ObjectItem);
				if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					// We can't mark cluster objects as unreachable here as they may be currently being processed on another thread
					ThisThreadClusterItemsToDestroy.Add(ObjectItem);
				}
			}
			ObjectItem->ClearFlags(EInternalObjectFlags::PersistentGarbage);
		}
		if (ThisThreadUnreachableObjects.Num())
		{
			FScopeLock UnreachableObjectsLock(&GUnreachableObjectsCritical);
			GUnreachableObjects.Append(ThisThreadUnreachableObjects);
			ClusterItemsToDestroy.Append(ThisThreadClusterItemsToDestroy);
		}
	}, bForceSingleThreaded ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	{
		// @todo: if GUObjectClusters.FreeCluster() was thread safe we could do this in parallel too
		for (FUObjectItem* ClusterRootItem : ClusterItemsToDestroy)
		{
#if UE_GCCLUSTER_VERBOSE_LOGGING
			UE_LOG(LogGarbage, Log, TEXT("Destroying cluster (%d) %s"), ClusterRootItem->GetClusterIndex(), *static_cast<UObject*>(ClusterRootItem->Object)->GetFullName());
#endif
			ClusterRootItem->ClearFlags(EInternalObjectFlags::ClusterRoot);

			const int32 ClusterIndex = ClusterRootItem->GetClusterIndex();
			FUObjectCluster& Cluster = GUObjectClusters[ClusterIndex];
			for (int32 ClusterObjectIndex : Cluster.Objects)
			{
				FUObjectItem* ClusterObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ClusterObjectIndex);
				ClusterObjectItem->SetOwnerIndex(0);

				if (!ClusterObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
				{
					ClusterObjectItem->SetFlags(EInternalObjectFlags::Unreachable);
					ClusterObjects++;
					GUnreachableObjects.Add(ClusterObjectItem);
				}
			}
			GUObjectClusters.FreeCluster(ClusterIndex);
		}
	}

	UE_LOG(LogGarbage, Log, TEXT("%f ms for Gather Unreachable Objects (%d objects collected including %d cluster objects from %d clusters)"),
		(FPlatformTime::Seconds() - StartTime) * 1000,
		GUnreachableObjects.Num(),
		ClusterObjects,
		ClusterItemsToDestroy.Num());
}

/** 
 * Deletes all unreferenced objects, keeping objects that have any of the passed in KeepFlags set
 *
 * @param	KeepFlags			objects with those flags will be kept regardless of being referenced or not
 * @param	bPerformFullPurge	if true, perform a full purge after the mark pass
 */
void CollectGarbageInternal(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
#if UE_WITH_GC
	SCOPE_TIME_GUARD(TEXT("Collect Garbage"));
	SCOPED_NAMED_EVENT(CollectGarbageInternal, FColor::Red);
	CSV_EVENT_GLOBAL(TEXT("GC"));
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(GarbageCollection);
	LLM_SCOPE(ELLMTag::GC);

	FGCCSyncObject::Get().ResetGCIsWaiting();

#if defined(WITH_CODE_GUARD_HANDLER) && WITH_CODE_GUARD_HANDLER
	void CheckImageIntegrityAtRuntime();
	CheckImageIntegrityAtRuntime();
#endif

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "CollectGarbageInternal" ), STAT_CollectGarbageInternal, STATGROUP_GC );
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "GarbageCollection - Begin" ) );

	// We can't collect garbage while there's a load in progress. E.g. one potential issue is Import.XObject
	check(!IsLoading());

	// Reset GC skip counter
	GNumAttemptsSinceLastGC = 0;

	// Flush streaming before GC if requested
	if (GFlushStreamingOnGC && IsAsyncLoading())
	{
		UE_LOG(LogGarbage, Log, TEXT("CollectGarbageInternal() is flushing async loading"));
		ReleaseGCLock();
		FlushAsyncLoading();
		AcquireGCLock();
	}

	// Route callbacks so we can ensure that we are e.g. not in the middle of loading something by flushing
	// the async loading, etc...
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPreGarbageCollect);
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Broadcast();
	}
	GLastGCFrame = GFrameCounter;

	{
		// Set 'I'm garbage collecting' flag - might be checked inside various functions.
		// This has to be unlocked before we call post GC callbacks
		FGCLegacyHashTableScopeLock GCLegacyHashTableLock;
		TGuardValue<bool> GuardIsGarbageCollecting(GIsGarbageCollecting, true);

		UE_LOG(LogGarbage, Log, TEXT("Collecting garbage%s"), IsAsyncLoading() ? TEXT(" while async loading") : TEXT(""));

		// Make sure previous incremental purge has finished or we do a full purge pass in case we haven't kicked one
		// off yet since the last call to garbage collection.
		if (GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired)
		{
			IncrementalPurgeGarbage(false);
			if (!bPerformFullPurge)
			{
				FMemory::Trim();
			}
		}

		// Reachability analysis.
		// When exiting this scope all objects to be destroyed have been marked unreachable and any weak references have been cleared.
		{
			// With the new FGCLockBehavior::Default behavior hash tables are only locked during this scope of reachability analysis.
			FGCHashTableScopeLock GCHashTableLock;

			check(!GObjIncrementalPurgeIsInProgress);
			check(!GObjPurgeIsRequired);

			// This can happen if someone disables clusters from the console (gc.CreateGCClusters)
			if (!GCreateGCClusters && GUObjectClusters.GetNumAllocatedClusters())
			{
				GUObjectClusters.DissolveClusters(true);
			}

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
			// Only verify assumptions if option is enabled. This avoids false positives in the Editor or commandlets.
			if (GShouldVerifyGCAssumptions)
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
				VerifyObjectFlagMirroring();
				UE_LOG(LogGarbage, Log, TEXT("%f ms for Verify GC Assumptions"), (FPlatformTime::Seconds() - StartTime) * 1000);
			}
#endif

			const EFastReferenceCollectorOptions Options = 
				// Fall back to single threaded GC if processor count is 1 or parallel GC is disabled
				// or detailed per class gc stats are enabled (not thread safe)
				(ShouldForceSingleThreadedGC() ? EFastReferenceCollectorOptions::None : EFastReferenceCollectorOptions::Parallel) |
				// Run with GC clustering code enabled only if clustering is enabled and there's actual allocated clusters
				((!!GCreateGCClusters && GUObjectClusters.GetNumAllocatedClusters()) ? EFastReferenceCollectorOptions::WithClusters : EFastReferenceCollectorOptions::None) |
				// Toggle between PendingKill enabled or disabled
				(UObjectBaseUtility::IsPendingKillEnabled() ? EFastReferenceCollectorOptions::WithPendingKill : EFastReferenceCollectorOptions::None);

			// Perform reachability analysis.
			{
				const double StartTime = FPlatformTime::Seconds();
				FRealtimeGC TagUsedRealtimeGC;

				TagUsedRealtimeGC.PerformReachabilityAnalysis(KeepFlags, Options);
				UE_LOG(LogGarbage, Log, TEXT("%f ms for GC"), (FPlatformTime::Seconds() - StartTime) * 1000);
			}

			FGCArrayPool& ArrayPool = FGCArrayPool::Get();
			TArray<FGCArrayStruct*> AllArrays;
			ArrayPool.GetAllArrayStructsFromPool(AllArrays);
			// This needs to happen before clusters get dissolved otherwisise cluster information will be missing from history
			ArrayPool.UpdateGCHistory(AllArrays);

			// Reconstruct clusters if needed
			if (GUObjectClusters.ClustersNeedDissolving())
			{
				const double StartTime = FPlatformTime::Seconds();
				GUObjectClusters.DissolveClusters();
				UE_LOG(LogGarbage, Log, TEXT("%f ms for dissolving GC clusters"), (FPlatformTime::Seconds() - StartTime) * 1000);
			}

			ArrayPool.DumpGarbageReferencers(AllArrays);

			GatherUnreachableObjects(!(Options & EFastReferenceCollectorOptions::Parallel));

			// This needs to happen after GatherUnreachableObjects since it can mark more objects as unreachable
			ArrayPool.ClearWeakReferences(AllArrays);

			// Now return arrays back to the pool and free some memory if requested
			for (int32 Index = 0; Index < AllArrays.Num(); ++Index)
			{
				FGCArrayStruct* ArrayStruct = AllArrays[Index];
				if (bPerformFullPurge
					|| Index % 7 == 3) // delete 1/7th of them just to keep things from growing too much between full purges
				{
					ArrayPool.FreeArrayStruct(ArrayStruct);
				}
				else
				{
					ArrayPool.ReturnToPool(ArrayStruct);
				}
			}

			// Make sure nothing will be using potentially freed arrays
			AllArrays.Empty();

			NotifyUnreachableObjects(GUnreachableObjects);
		}
		if (GGCLockBehavior == FGCLockBehavior::Default)
		{
			// With the new FGCLockBehavior::Default behavior the hash tables lock was released when exiting the reachability analysis scope above.
			// BeginDestroy, FinishDestroy, destructors and callbacks are allowed to call functions like StaticAllocateObject and StaticFindObject.
			// Now release the GC lock to allow async loading and other threads to perform UObject operations under the FGCScopeGuard.
			ReleaseGCLock();
		}
		// The old FGCLockBehavior::Legacy behavior keeps the GC and hash table locks until GC is completed,
		// and no threads are allowed to perform UObject operations yet.

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
	}

	// Route callbacks to verify GC assumptions
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastPostGarbageCollect);
		FCoreUObjectDelegates::GetPostGarbageCollect().Broadcast();
	}

	GLastGCTime = FPlatformTime::Seconds();
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "GarbageCollection - End" ) );
#endif	// UE_WITH_GC
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
			*Referencer.GCObject->GetReferencerName()
			);
	}
}

void FGCArrayPool::DumpGarbageReferencers(TArray<FGCArrayStruct*>& AllArrays)
{		
	if (GGarbageReferenceTrackingEnabled == 0)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// We don't care about leaks when engine exit was requested since the final GC pass will destroy everything anyway
	// We still want to clear the GarbageReferences array though
	if (IsEngineExitRequested())
	{
		for (FGCArrayStruct* ArrayStruct : AllArrays)
		{
			ArrayStruct->GarbageReferences.Reset();
		}
		return;
	}

	double StartTime = FPlatformTime::Seconds();
	// Dump all garbage references in detail
	if (GGarbageReferenceTrackingEnabled == 1)
	{
		int32 NumGarbageReferences = 0;
		for (FGCArrayStruct* ArrayStruct : AllArrays)
		{
			for (FGarbageReferenceInfo& GarbageReference : ArrayStruct->GarbageReferences)
			{
				// No need to spam with references that would never get released anyway (even if we still had PendingKill) because they were referenced by a persistent reference 
				// Disable checking this flag because it's too aggressive at removing references from reporting.
				// if (!GarbageReference.GarbageObject->HasAnyInternalFlags(EInternalObjectFlags::PersistentGarbage))
				{
					UE_LOG(LogGarbage, Warning, TEXT("Reachable garbage object: %s"), *GarbageReference.GarbageObject->GetFullName());
					UE_LOG(LogGarbage, Warning, TEXT("Referenced by:            %s"), *GarbageReference.GetReferencingObjectInfo());
					UE_LOG(LogGarbage, Warning, TEXT(""));
					NumGarbageReferences++;
				}
			}
			if (ArrayStruct->GarbageReferences.Num())
			{
				FCoreUObjectDelegates::GetGarbageCollectReportGarbageReferencers().Broadcast(ArrayStruct->GarbageReferences);
				ArrayStruct->GarbageReferences.Reset();
			}
		}
		UE_CLOG(NumGarbageReferences > 0, LogGarbage, Log, TEXT("Found %d garbage references in %f ms."), NumGarbageReferences, (FPlatformTime::Seconds() - StartTime) * 1000);
	}
	// Dump one reference for each source property
	else if (GGarbageReferenceTrackingEnabled == 2)
	{
		typedef TPair<void*, FName> FKey;
		TSet<FKey> Seen;
		int32 TotalGarbageReferences = 0, ReportedGarbageReferences = 0;
		for (FGCArrayStruct* ArrayStruct : AllArrays)
		{
			for (FGarbageReferenceInfo& GarbageReference : ArrayStruct->GarbageReferences)
			{
				// No need to spam with references that would never get released anyway (even if we still had PendingKill) because they were referenced by a persistent reference 
				// Disable checking this flag because it's too aggressive at removing references from reporting.
				// if (!GarbageReference.GarbageObject->HasAnyInternalFlags(EInternalObjectFlags::PersistentGarbage))
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
			}
			if (ArrayStruct->GarbageReferences.Num())
			{
				FCoreUObjectDelegates::GetGarbageCollectReportGarbageReferencers().Broadcast(ArrayStruct->GarbageReferences);
				ArrayStruct->GarbageReferences.Reset();
			}
		}
		UE_CLOG(TotalGarbageReferences > 0, LogGarbage, Log, TEXT("Reported %d/%d garbage references in %f ms. (The rest were omitted as duplicate referencing class/property.)"), ReportedGarbageReferences, TotalGarbageReferences, (FPlatformTime::Seconds() - StartTime) * 1000);
	}
#endif
}

void FGCArrayPool::UpdateGCHistory(TArray<FGCArrayStruct*>& AllArrays)
{
#if ENABLE_GC_HISTORY
	FGCHistory::Get().Update(AllArrays);

	for (FGCArrayStruct* ArrayStruct : AllArrays)
	{
		for (TPair<UObject*, TArray<FGCDirectReference>*>& DirectReferenceInfos : ArrayStruct->History)
		{
			delete DirectReferenceInfos.Value;
		}
		ArrayStruct->History.Reset();
	}
#endif // ENABLE_GC_HISTORY
}

double GetLastGCTime()
{
	return GLastGCTime;
}

bool IsIncrementalUnhashPending()
{
	return GUnrechableObjectIndex < GUnreachableObjects.Num();
}

bool UnhashUnreachableObjects(bool bUseTimeLimit, double TimeLimit)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UnhashUnreachableObjects);
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UnhashUnreachableObjects"), STAT_UnhashUnreachableObjects, STATGROUP_GC);

	TGuardValue<bool> GuardObjUnhashUnreachableIsInProgress(GObjUnhashUnreachableIsInProgress, true);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BroadcastGarbageCollectConditionalBeginDestroy);
		FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.Broadcast();
	}

	// Unhash all unreachable objects.
	const double StartTime = FPlatformTime::Seconds();
	const int32 TimeLimitEnforcementGranularityForBeginDestroy = 10;
	int32 Items = 0;
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

			Items++;

			const bool bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForBeginDestroy == 0);
			if (bUseTimeLimit && bPollTimeLimit && ((FPlatformTime::Seconds() - StartTime) > TimeLimit))
			{
				break;
			}
		}
	}

	const bool bTimeLimitReached = (GUnrechableObjectIndex < GUnreachableObjects.Num());

	if (!bUseTimeLimit)
	{
		UE_LOG(LogGarbage, Log, TEXT("%f ms for %sunhashing unreachable objects (%d objects unhashed)"),
		(FPlatformTime::Seconds() - StartTime) * 1000,
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
#if !UE_WITH_GC
	return;
#else
	if (GIsInitialLoad)
	{
		// During initial load classes may not yet have their GC token streams assembled
		UE_LOG(LogGarbage, Log, TEXT("Skipping CollectGarbage() call during initial load. It's not safe."));
		return;
	}

	// No other thread may be performing UObject operations while we're running
	AcquireGCLock();

	// Perform actual garbage collection
	CollectGarbageInternal(KeepFlags, bPerformFullPurge);

	if (GGCLockBehavior == FGCLockBehavior::Legacy)
	{
		// Release the GC lock to allow async loading and other threads to perform UObject operations under the FGCScopeGuard.
		ReleaseGCLock();
	}
	// With the new FGCLockBehavior::Default behavior the lock was released after reachability analysis inside CollectGarbageInternal
#endif
}

bool TryCollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
#if !UE_WITH_GC
	return false;
#else
	if (GIsInitialLoad)
	{
		// During initial load classes may not yet have their GC token streams assembled
		UE_LOG(LogGarbage, Log, TEXT("Skipping TryCollectGarbage() call during initial load. It's not safe."));
		return false;
	}

	// No other thread may be performing UObject operations while we're running
	bool bCanRunGC = FGCCSyncObject::Get().TryGCLock();
	if (!bCanRunGC)
	{
		if (GNumRetriesBeforeForcingGC > 0 && GNumAttemptsSinceLastGC > GNumRetriesBeforeForcingGC)
		{
			// Force GC and block main thread			
			UE_LOG(LogGarbage, Warning, TEXT("TryCollectGarbage: forcing GC after %d skipped attempts."), GNumAttemptsSinceLastGC);
			GNumAttemptsSinceLastGC = 0;
			AcquireGCLock();
			bCanRunGC = true;
		}
	}
	if (bCanRunGC)
	{
		// Perform actual garbage collection
		CollectGarbageInternal(KeepFlags, bPerformFullPurge);

		if (GGCLockBehavior == FGCLockBehavior::Legacy)
		{
			// Release the GC lock to allow async loading and other threads to perform UObject operations under the FGCScopeGuard.
			ReleaseGCLock();
		}
		// With the new FGCLockBehavior::Default behavior the lock was released after reachability analysis inside CollectGarbageInternal
	}
	else
	{
		GNumAttemptsSinceLastGC++;
	}

	return bCanRunGC;
#endif
}

void UObject::CallAddReferencedObjects(FReferenceCollector& Collector)
{
	GetClass()->CallAddReferencedObjects(this, Collector);
}

void UObject::AddReferencedObjects(UObject* This, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	//@todo UE - This seems to be required and it should not be. Seems to be related to the texture streamer.
	FLinkerLoad* LinkerLoad = This->GetLinker();
	if (LinkerLoad)
	{
		LinkerLoad->AddReferencedObjects(Collector);
	}
	// Required by the unified GC when running in the editor
	if (GIsEditor)
	{
		UObject* LoadOuter = This->GetOuter();
		UClass* Class = This->GetClass();
		UPackage* Package = This->GetExternalPackageInternal();
		Collector.AllowEliminatingReferences(false);
		Collector.AddReferencedObject(LoadOuter, This);
		Collector.AddReferencedObject(Package, This);
		Collector.AllowEliminatingReferences(true);
		Collector.AddReferencedObject(Class, This);
	}
#endif
}

bool UObject::IsDestructionThreadSafe() const
{
	return false;
}

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
	else
	{
		if (!Struct)
		{
			UE_LOG(LogGarbage, Warning, TEXT("Broken FStructProperty does not have a UStruct: %s"), *GetFullName() );
		}
		else if (Struct->StructFlags & STRUCT_AddStructReferencedObjects)
		{
			return true;
		}
		else
		{
			EncounteredStructProps.Add(this);
			FProperty* Property = Struct->PropertyLink;
			while( Property )
			{
				if (Property->ContainsObjectReference(EncounteredStructProps, InReferenceType))
				{
					EncounteredStructProps.RemoveSingleSwap(this, false /*bAllowShrinking*/);
					return true;
				}
				Property = Property->PropertyLinkNext;
			}
			EncounteredStructProps.RemoveSingleSwap(this, false /*bAllowShrinking*/);
		}
		return false;
	}
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

/**
 * Scope helper structure to emit tokens for fixed arrays in the case of ArrayDim (passed in count) being > 1.
 */
struct FGCReferenceFixedArrayTokenHelper
{
	/**
	 * Constructor, emitting necessary tokens for fixed arrays if count > 1 and also keeping track of count so 
	 * destructor can do the same.
	 *
	 * @param InReferenceTokenStream	Token stream to emit tokens to
	 * @param InOffset					offset into object/ struct
	 * @param InCount					array count
	 * @param InStride					array type stride (e.g. sizeof(struct) or sizeof(UObject*))
	 * @param InProperty                the property this array represents
	 */
	FGCReferenceFixedArrayTokenHelper(UClass& OwnerClass, int32 InOffset, int32 InCount, int32 InStride, const FProperty& InProperty, FGCStackSizeHelper& InStackSizeHelper)
		: ReferenceTokenStream(&OwnerClass.ReferenceTokenStream)
		, StackSizeHelper(InStackSizeHelper)
		, Count(InCount)
	{
		if( InCount > 1 )
		{
			OwnerClass.EmitObjectReference(InOffset, *StackSizeHelper.GetPropertyPath(), GCRT_FixedArray);

			OwnerClass.ReferenceTokenStream.EmitStride(InStride);
			OwnerClass.ReferenceTokenStream.EmitCount(InCount);
			// GCRT_FixedArray pushes a new stack frame in TFastReferenceCollector
			StackSizeHelper.Push(&InProperty);
		}
	}

	/** Destructor, emitting return if ArrayDim > 1 */
	~FGCReferenceFixedArrayTokenHelper()
	{
		if( Count > 1 )
		{
			ReferenceTokenStream->EmitReturn();
			StackSizeHelper.Pop();
		}
	}

private:
	/** Reference token stream used to emit to */
	FGCReferenceTokenStream*	ReferenceTokenStream;
	/** GC token stream stack size helper */
	FGCStackSizeHelper&			StackSizeHelper;
	/** Size of fixed array */
	int32							Count;
};


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
}

/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FObjectProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(UObject*), *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_Object);
}

void FWeakObjectProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FWeakObjectPtr), *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_WeakObject);
}
void FLazyObjectProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FLazyObjectPtr), *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_LazyObject);
}
void FSoftObjectProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FSoftObjectPtr), *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_SoftObject);
}
void FDelegateProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, this->ElementSize, *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_Delegate);
}
void FMulticastDelegateProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, this->ElementSize, *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_MulticastDelegate);
}

/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FArrayProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	if (Inner->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
	{
		bool bUsesFreezableAllocator = EnumHasAnyFlags(ArrayFlags, EArrayPropertyFlags::UsesMemoryImageAllocator);

		// Structs and nested arrays share the same implementation on the Garbage Collector side
		// as arrays of structs already push the array memory into the GC stack and process its tokens
		// which is exactly what is required for nested arrays to work
		if( Inner->IsA(FObjectProperty::StaticClass()) || Inner->IsA(FObjectPtrProperty::StaticClass()) )
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), bUsesFreezableAllocator ? GCRT_ArrayObjectFreezable : GCRT_ArrayObject);
		}
		else if( Inner->IsA(FInterfaceProperty::StaticClass()) )
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), bUsesFreezableAllocator ? GCRT_ArrayStructFreezable : GCRT_ArrayStruct);

			// GCRT_ArrayStruct and GCRT_ArrayStructFreezable push a new stack frame in TFastReferenceCollector
			FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, Inner);

			OwnerClass.ReferenceTokenStream.EmitStride(Inner->ElementSize);
			const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();

			OwnerClass.EmitObjectReference(0, *StackSizeHelper.GetPropertyPath(), GCRT_Object);

			const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
			OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
		}
		else if (Inner->IsA(FFieldPathProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArrayAddFieldPathReferencedObject);
		}
		else if (Inner->IsA(FWeakObjectProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArrayWeakObject);
		}
		else if (Inner->IsA(FLazyObjectProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArrayLazyObject);
		}
		else if (Inner->IsA(FSoftObjectProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArraySoftObject);
		}
		else if (Inner->IsA(FDelegateProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArrayDelegate);
		}
		else if (Inner->IsA(FMulticastDelegateProperty::StaticClass()))
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_ArrayMulticastDelegate);
		}
		else
		{
			// In the general case, emit a GCRT_ArrayStruct that can handle any inner property type.
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), bUsesFreezableAllocator ? GCRT_ArrayStructFreezable : GCRT_ArrayStruct);

			// GCRT_ArrayStruct and GCRT_ArrayStructFreezable push a new stack frame in TFastReferenceCollector
			FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, Inner);

			OwnerClass.ReferenceTokenStream.EmitStride(Inner->ElementSize);
			const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();
			Inner->EmitReferenceInfo(OwnerClass, 0, EncounteredStructProps, StackSizeHelper);
			const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
			OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
		}
	}
}


/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FMapProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	if (ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
	{
		// TMap reference tokens are processed by GC in a similar way to an array of structs
		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_AddTMapReferencedObjects);
		OwnerClass.ReferenceTokenStream.EmitPointer((const void*)this);
		const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();

		if (KeyProp->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
		{
			// GCRT_AddTMapReferencedObjects pushes a new stack frame in TFastReferenceCollector
			FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, KeyProp);
			KeyProp->EmitReferenceInfo(OwnerClass, 0, EncounteredStructProps, StackSizeHelper);
		}
		if (ValueProp->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
		{
			// GCRT_AddTMapReferencedObjects pushes a new stack frame in TFastReferenceCollector
			FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, ValueProp);
			ValueProp->EmitReferenceInfo(OwnerClass, 0, EncounteredStructProps, StackSizeHelper);
		}

		const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
		OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
	}
}

/**
* Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
* to the passed in BaseOffset which is used by e.g. arrays of structs.
*/
void FSetProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	if (ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
	{
		// TSet reference tokens are processed by GC in a similar way to an array of structs
		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_AddTSetReferencedObjects);
		OwnerClass.ReferenceTokenStream.EmitPointer((const void*)this);

		// GCRT_AddTSetReferencedObjects pushes a new stack frame in TFastReferenceCollector
		FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, ElementProp);

		const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();
		ElementProp->EmitReferenceInfo(OwnerClass, 0, EncounteredStructProps, StackSizeHelper);
		const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
		OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
	}
}


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FStructProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	check(Struct);
	if (Struct->StructFlags & STRUCT_AddStructReferencedObjects)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_AddStructReferencedObjects
		FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, ElementSize, *this, StackSizeHelper);

		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_AddStructReferencedObjects);

		void *FunctionPtr = (void*)CppStructOps->AddStructReferencedObjects();
		OwnerClass.ReferenceTokenStream.EmitPointer(FunctionPtr);
	}

	// Check if the struct has any properties that reference UObjects
	bool bHasPropertiesWithObjectReferences = false;
	if (Struct->PropertyLink)
	{
		// Can't use ContainObjectReference here as it also checks for STRUCT_AddStructReferencedObjects but we only care about property exposed refs
		EncounteredStructProps.Add(this);
		for (FProperty* Property = Struct->PropertyLink; Property && !bHasPropertiesWithObjectReferences; Property = Property->PropertyLinkNext)
		{
			bHasPropertiesWithObjectReferences = Property->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak);
		}
		EncounteredStructProps.RemoveSingleSwap(this, false /* bAllowShrinking */);
	}
	// If the struct has UObject properties (and only if) emit tokens for them
	if (bHasPropertiesWithObjectReferences)
	{
		FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, ElementSize, *this, StackSizeHelper);

		FProperty* Property = Struct->PropertyLink;
		while (Property)
		{
			FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, Property);
			Property->EmitReferenceInfo(OwnerClass, BaseOffset + GetOffset_ForGC(), EncounteredStructProps, StackSizeHelper);
			Property = Property->PropertyLinkNext;
		}
	}
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void FInterfaceProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FScriptInterface), *this, StackSizeHelper);

	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_Object);
}

void FFieldPathProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, FGCStackSizeHelper& StackSizeHelper)
{
	static_assert(sizeof(FFieldPath) == sizeof(TFieldPath<FProperty>), "TFieldPath should have the same size as the underlying FFieldPath");
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FFieldPath), *this, StackSizeHelper);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), *StackSizeHelper.GetPropertyPath(), GCRT_AddFieldPathReferencedObject);
}

void UClass::EmitObjectReference(int32 Offset, const FName& DebugName, EGCReferenceType Kind)
{
	FGCReferenceInfo ObjectReference(Kind, Offset);
	ReferenceTokenStream.EmitReferenceInfo(ObjectReference, DebugName);
}

void UClass::EmitObjectArrayReference(int32 Offset, const FName& DebugName)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_ArrayObject);
}

uint32 UClass::EmitStructArrayBegin(int32 Offset, const FName& DebugName, int32 Stride)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_ArrayStruct);
	ReferenceTokenStream.EmitStride(Stride);
	const uint32 SkipIndexIndex = ReferenceTokenStream.EmitSkipIndexPlaceholder();
	return SkipIndexIndex;
}

/**
 * Realtime garbage collection helper function used to indicate the end of an array of structs. The
 * index following the current one will be written to the passed in SkipIndexIndex in order to be
 * able to skip tokens for empty dynamic arrays.
 *
 * @param SkipIndexIndex
 */
void UClass::EmitStructArrayEnd( uint32 SkipIndexIndex )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	const uint32 SkipIndex = ReferenceTokenStream.EmitReturn();
	ReferenceTokenStream.UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
}

void UClass::EmitFixedArrayBegin(int32 Offset, const FName& DebugName, int32 Stride, int32 Count)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_FixedArray);
	ReferenceTokenStream.EmitStride(Stride);
	ReferenceTokenStream.EmitCount(Count);
}

/**
 * Realtime garbage collection helper function used to indicated the end of a fixed array.
 */
void UClass::EmitFixedArrayEnd()
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	ReferenceTokenStream.EmitReturn();
}

void UClass::EmitExternalPackageReference()
{
#if WITH_EDITOR
	static const FName TokenName("ExternalPackageToken");
	ReferenceTokenStream.EmitReferenceInfo(FGCReferenceInfo(GCRT_ExternalPackage, 0), TokenName);
#endif
}

struct FScopeLockIfNotNative
{
	FCriticalSection& ScopeCritical;
	const bool bNotNative;
	FScopeLockIfNotNative(FCriticalSection& InScopeCritical, bool bIsNotNative)
		: ScopeCritical(InScopeCritical)
		, bNotNative(bIsNotNative)
	{
		if (bNotNative)
		{
			ScopeCritical.Lock();
		}
	}
	~FScopeLockIfNotNative()
	{
		if (bNotNative)
		{
			ScopeCritical.Unlock();
		}
	}
};

void UClass::AssembleReferenceTokenStream(bool bForce)
{
	LLM_SCOPE(ELLMTag::GC);

	// Lock for non-native classes
	FScopeLockIfNotNative ReferenceTokenStreamLock(ReferenceTokenStreamCritical, !(ClassFlags & CLASS_Native));

	UE_CLOG(!IsInGameThread() && !IsGarbageCollectionLocked(), LogGarbage, Fatal, TEXT("AssembleReferenceTokenStream for %s called on a non-game thread while GC is not locked."), *GetFullName());

	if (!HasAnyClassFlags(CLASS_TokenStreamAssembled) || bForce)
	{
		if (bForce)
		{
			ReferenceTokenStream.Empty();
			ClassFlags &= ~CLASS_TokenStreamAssembled;
		}
		TArray<const FStructProperty*> EncounteredStructProps;
		
		{
			FGCStackSizeHelper StackSizeHelper;

			// Iterate over properties defined in this class
			for (TFieldIterator<FProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty* Property = *It;
				FGCStackSizeHelperScope StackSizeScope(StackSizeHelper, Property);
				Property->EmitReferenceInfo(*this, 0, EncounteredStructProps, StackSizeHelper);
			}

			// The fact we set min to 1 is because we always emit noexport (non FProperty exposed) references for 
			// UObject's Outer and ClassPrivate members in EmitBaseReferences (see below)
			ReferenceTokenStream.SetStackSize(FMath::Max(1, StackSizeHelper.GetMaxStackSize()));
		}

		if (UClass* SuperClass = GetSuperClass())
		{
			// We also need to lock the super class stream in case something (like PostLoad) wants to reconstruct it on GameThread
			FScopeLockIfNotNative SuperClassReferenceTokenStreamLock(SuperClass->ReferenceTokenStreamCritical, !(SuperClass->ClassFlags & CLASS_Native));
			
			// Make sure super class has valid token stream.
			SuperClass->AssembleReferenceTokenStream();
			if (!SuperClass->ReferenceTokenStream.IsEmpty())
			{
				// Prepend super's stream. This automatically handles removing the EOS token.
				ReferenceTokenStream.PrependStream(SuperClass->ReferenceTokenStream);
			}
		}
		else
		{
			UObjectBase::EmitBaseReferences(this);
		}

		{
			check(CppClassStaticFunctions.GetAddReferencedObjects() != nullptr);
			const bool bKeepOuter = true;//GetFName() != NAME_Package;
			const bool bKeepClass = true;//!HasAnyInternalFlags(EInternalObjectFlags::Native);

			FUObjectCppClassStaticFunctions::AddReferencedObjectsType AddReferencedObjectsFn = nullptr;
#if !WITH_EDITOR
			// In no-editor builds UObject::ARO is empty, thus only classes
			// which implement their own ARO function need to have the ARO token generated.
			if (CppClassStaticFunctions.GetAddReferencedObjects() != &UObject::AddReferencedObjects)
			{
				AddReferencedObjectsFn = CppClassStaticFunctions.GetAddReferencedObjects();
			}
#else
			AddReferencedObjectsFn = CppClassStaticFunctions.GetAddReferencedObjects();
#endif
			ReferenceTokenStream.Fixup(AddReferencedObjectsFn, bKeepOuter, bKeepClass);

			// Make sure all Blueprint properties are marked as non-native
			const EGCTokenType TokenType = GetClass()->HasAnyClassFlags(CLASS_NeedsDeferredDependencyLoading) ? EGCTokenType::NonNative : EGCTokenType::Native;
			ReferenceTokenStream.SetTokenType(TokenType);
		}

		if (ReferenceTokenStream.IsEmpty())
		{
			return;
		}

		// Emit end of stream token.
		static const FName EOSDebugName("EndOfStreamToken");
		ReferenceTokenStream.EmitReferenceInfo(FGCReferenceInfo::EndOfStreamToken, EOSDebugName);

		// Shrink reference token stream to proper size.
		ReferenceTokenStream.Shrink();

		check(!HasAnyClassFlags(CLASS_TokenStreamAssembled)); // recursion here is probably bad
		ClassFlags |= CLASS_TokenStreamAssembled;

		// Update the global maximum stack size after assembling the token stream of each class
		FGCReferenceTokenStream::SetMaxStackSize(FMath::Max(FGCReferenceTokenStream::GetMaxStackSize(), ReferenceTokenStream.GetStackSize()));
	}
}


/**
 * Prepends passed in stream to existing one.
 *
 * @param Other	stream to concatenate
 */
void FGCReferenceTokenStream::PrependStream( const FGCReferenceTokenStream& Other )
{
	// Remove embedded EOS token if needed.
	int32 NumTokensToPrepend = (Other.Tokens.Num() && Other.Tokens.Last() == FGCReferenceInfo::EndOfStreamToken) ? (Other.Tokens.Num() - 1) : Other.Tokens.Num();

	TArray<uint32> TempTokens;
	TempTokens.Reserve(NumTokensToPrepend + Tokens.Num());

#if ENABLE_GC_TOKEN_DEBUG_INFO
	check(TokenDebugInfo.Num() == Tokens.Num());
	check(Other.TokenDebugInfo.Num() == Other.Tokens.Num());
	TArray<FName> TempTokenDebugInfo;
	TempTokenDebugInfo.Reserve(NumTokensToPrepend + TokenDebugInfo.Num());
#endif // ENABLE_GC_TOKEN_DEBUG_INFO

	for (int32 TokenIndex = 0; TokenIndex < NumTokensToPrepend; ++TokenIndex)
	{
		TempTokens.Add(Other.Tokens[TokenIndex]);
#if ENABLE_GC_TOKEN_DEBUG_INFO
		TempTokenDebugInfo.Add(Other.TokenDebugInfo[TokenIndex]);
#endif // ENABLE_GC_TOKEN_DEBUG_INFO
	}

	TempTokens.Append(Tokens);
	Tokens = MoveTemp(TempTokens);

#if ENABLE_GC_TOKEN_DEBUG_INFO
	TempTokenDebugInfo.Append(TokenDebugInfo);
	TokenDebugInfo = MoveTemp(TempTokenDebugInfo);
#endif // ENABLE_GC_TOKEN_DEBUG_INFO

	StackSize = FMath::Max(StackSize, Other.GetStackSize());
}

void FGCReferenceTokenStream::Fixup(void (*AddReferencedObjectsPtr)(UObject*, class FReferenceCollector&), bool bKeepOuterToken, bool bKeepClassToken)
{
	bool bReplacedARO = false;

	// Try to find exiting ARO pointer and replace it (to avoid removing and readding tokens).
	for (int32 TokenStreamIndex = 0; TokenStreamIndex < Tokens.Num(); ++TokenStreamIndex)
	{
		uint32 TokenIndex = (uint32)TokenStreamIndex;
		FGCReferenceInfo Token = Tokens[TokenIndex];
		// Read token type and skip additional data if present.
		switch (Token.Type)
		{
		case GCRT_ArrayStruct:
		case GCRT_ArrayStructFreezable:
			{
				// Skip stride and move to Skip Info
				TokenIndex += 2;
				const FGCSkipInfo SkipInfo = ReadSkipInfo(TokenIndex);
				// Set the TokenIndex to the skip index - 1 because we're going to
				// increment in the for loop anyway.
				TokenIndex = SkipInfo.SkipIndex - 1;
			}
			break;
		case GCRT_FixedArray:
			{
				// Skip stride
				TokenIndex++; 
				// Skip count
				TokenIndex++; 
			}
			break;
		case GCRT_AddStructReferencedObjects:
			{
				// Skip pointer
				TokenIndex += GNumTokensPerPointer;
			}
			break;
		case GCRT_AddReferencedObjects:
			{
				// Store the pointer after the ARO token.
				if (AddReferencedObjectsPtr)
				{
					StorePointer(&Tokens[TokenIndex + 1], (const void*)AddReferencedObjectsPtr);					
				}
				bReplacedARO = true;
				TokenIndex += GNumTokensPerPointer;
			}
			break;
		case GCRT_AddTMapReferencedObjects:
		case GCRT_AddTSetReferencedObjects:
			{
				// Skip pointer
				TokenIndex += GNumTokensPerPointer;
				TokenIndex += 1; // GCRT_EndOfPointer;
				// Move to Skip Info
				TokenIndex += 1;
				const FGCSkipInfo SkipInfo = ReadSkipInfo(TokenIndex);
				// Set the TokenIndex to the skip index - 1 because we're going to
				// increment in the for loop anyway.
				TokenIndex = SkipInfo.SkipIndex - 1;
			}
			break;
		case GCRT_Class:
		case GCRT_NoopClass:
			{
				if (bKeepClassToken)
				{
					Token.Type = GCRT_Class;
				}
				else
				{
					Token.Type = GCRT_NoopClass;
				}
				Tokens[TokenIndex] = Token;
			}
			break;
		case GCRT_PersistentObject:
		case GCRT_NoopPersistentObject:
			{
				if (bKeepOuterToken)
				{
					Token.Type = GCRT_PersistentObject;
				}
				else
				{
					Token.Type = GCRT_NoopPersistentObject;
				}
				Tokens[TokenIndex] = Token;
			}
			break;
		case GCRT_Optional:
			{
				// Move to Skip Info
				TokenIndex++;
				const FGCSkipInfo SkipInfo = ReadSkipInfo(TokenIndex);
				// Set the TokenIndex to the skip index - 1 because we're going to
				// increment in the for loop anyway.
				TokenIndex = SkipInfo.SkipIndex - 1;
			}
			break;
		case GCRT_None:
		case GCRT_Object:
		case GCRT_ExternalPackage:
		case GCRT_ArrayObject:
		case GCRT_ArrayObjectFreezable:
		case GCRT_AddFieldPathReferencedObject:
		case GCRT_ArrayAddFieldPathReferencedObject:
		case GCRT_EndOfPointer:
		case GCRT_EndOfStream:
		case GCRT_WeakObject:
		case GCRT_ArrayWeakObject:
		case GCRT_LazyObject:
		case GCRT_ArrayLazyObject:
		case GCRT_SoftObject:
		case GCRT_ArraySoftObject:
		case GCRT_Delegate:
		case GCRT_ArrayDelegate:
		case GCRT_MulticastDelegate:
		case GCRT_ArrayMulticastDelegate:
		case GCRT_DynamicallyTypedValue:
			break;
		default:
			UE_LOG(LogGarbage, Fatal, TEXT("Unknown token type (%u) when trying to add ARO token."), (uint32)Token.Type);
			break;
		};
		TokenStreamIndex = (int32)TokenIndex;
	}
	// ARO is not in the token stream yet.
	if (!bReplacedARO && AddReferencedObjectsPtr)
	{
		static const FName TokenName("AROToken");
		EmitReferenceInfo(FGCReferenceInfo(GCRT_AddReferencedObjects, 0), TokenName);
		EmitPointer((const void*)AddReferencedObjectsPtr);
	}
}

int32 FGCReferenceTokenStream::EmitReferenceInfo(FGCReferenceInfo ReferenceInfo, const FName& DebugName)
{
	int32 TokenIndex = Tokens.Add(ReferenceInfo);
#if ENABLE_GC_TOKEN_DEBUG_INFO
	check(TokenDebugInfo.Num() == TokenIndex);
	check(DebugName != NAME_None);
	TokenDebugInfo.Add(DebugName);
#endif
	return TokenIndex;
}

/**
 * Emit placeholder for aray skip index, updated in UpdateSkipIndexPlaceholder
 *
 * @return the index of the skip index, used later in UpdateSkipIndexPlaceholder
 */
uint32 FGCReferenceTokenStream::EmitSkipIndexPlaceholder()
{
	uint32 TokenIndex = Tokens.Add(E_GCSkipIndexPlaceholder);
#if ENABLE_GC_TOKEN_DEBUG_INFO
	static const FName TokenName("SkipIndexPlaceholder");
	check(TokenDebugInfo.Num() == TokenIndex);
	TokenDebugInfo.Add(TokenName);
#endif
	return TokenIndex;
}

/**
 * Updates skip index place holder stored and passed in skip index index with passed
 * in skip index. The skip index is used to skip over tokens in the case of an empty 
 * dynamic array.
 * 
 * @param SkipIndexIndex index where skip index is stored at.
 * @param SkipIndex index to store at skip index index
 */
void FGCReferenceTokenStream::UpdateSkipIndexPlaceholder( uint32 SkipIndexIndex, uint32 SkipIndex )
{
	check( SkipIndex > 0 && SkipIndex <= (uint32)Tokens.Num() );
	const FGCReferenceInfo& ReferenceInfo = Tokens[SkipIndex-1];
	check( ReferenceInfo.Type != GCRT_None );
	check( Tokens[SkipIndexIndex] == E_GCSkipIndexPlaceholder );
	check( SkipIndexIndex < SkipIndex );
	check( ReferenceInfo.ReturnCount >= 1 );
	FGCSkipInfo SkipInfo;
	SkipInfo.SkipIndex			= SkipIndex - SkipIndexIndex;
	// We need to subtract 1 as ReturnCount includes return from this array.
	SkipInfo.InnerReturnCount	= ReferenceInfo.ReturnCount - 1; 
	Tokens[SkipIndexIndex]		= SkipInfo;
}

/**
 * Emit count
 *
 * @param Count count to emit
 */
int32 FGCReferenceTokenStream::EmitCount( uint32 Count )
{
	int32 TokenIndex = Tokens.Add( Count );
#if ENABLE_GC_TOKEN_DEBUG_INFO
	static const FName TokenName("CountToken");
	check(TokenDebugInfo.Num() == TokenIndex);
	TokenDebugInfo.Add(TokenName);
#endif
	return TokenIndex;
}

int32 FGCReferenceTokenStream::EmitPointer( void const* Ptr )
{
	const int32 StoreIndex = Tokens.Num();
	Tokens.AddUninitialized(GNumTokensPerPointer);
	StorePointer(&Tokens[StoreIndex], Ptr);

#if ENABLE_GC_TOKEN_DEBUG_INFO
	static const FName TokenName("PointerToken");
	check(TokenDebugInfo.Num() == StoreIndex);
	for (int32 PointerTokenIndex = 0; PointerTokenIndex < GNumTokensPerPointer; ++PointerTokenIndex)
	{
		TokenDebugInfo.Add(TokenName);
	}
#endif

	// Now inser the end of pointer marker, this will mostly be used for storing ReturnCount value
	// if the pointer was stored at the end of struct array stream.
	static const FName EndOfPointerTokenName("EndOfPointerToken");
	EmitReferenceInfo(FGCReferenceInfo(GCRT_EndOfPointer, 0), EndOfPointerTokenName);

	return StoreIndex;
}

/**
 * Emit stride
 *
 * @param Stride stride to emit
 */
int32 FGCReferenceTokenStream::EmitStride( uint32 Stride )
{
	int32 TokenIndex = Tokens.Add( Stride );

#if ENABLE_GC_TOKEN_DEBUG_INFO
	static const FName TokenName("StrideToken");
	check(TokenDebugInfo.Num() == TokenIndex);
	TokenDebugInfo.Add(TokenName);
#endif

	return TokenIndex;
}

/**
 * Increase return count on last token.
 *
 * @return index of next token
 */
uint32 FGCReferenceTokenStream::EmitReturn()
{
	FGCReferenceInfo ReferenceInfo = Tokens.Last();
	check(ReferenceInfo.Type != GCRT_None);
	ReferenceInfo.ReturnCount++;
	Tokens.Last() = ReferenceInfo;
	return Tokens.Num();
}


FTokenInfo FGCReferenceTokenStream::GetTokenInfo(int32 TokenIndex) const
{
	FTokenInfo DebugInfo;
	DebugInfo.Offset = FGCReferenceInfo(Tokens[TokenIndex]).Offset;
#if ENABLE_GC_TOKEN_DEBUG_INFO
	DebugInfo.Name = TokenDebugInfo[TokenIndex];
#else
	DebugInfo.Name = FName(TEXT("Token"), TokenIndex);
#endif
	return DebugInfo;
}


FGCArrayPool* FGCArrayPool::GetGlobalSingleton()
{
	static FAutoConsoleCommandWithOutputDevice GCDumpPoolCommand(
		TEXT("gc.DumpPoolStats"),
		TEXT("Dumps count and size of GC Pools"),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&FGCArrayPool::DumpStats)
	);

	static FGCArrayPool* GlobalSingleton = nullptr;

	if (!GlobalSingleton)
	{
		GlobalSingleton = new FGCArrayPool();
	}
	return GlobalSingleton;
}
