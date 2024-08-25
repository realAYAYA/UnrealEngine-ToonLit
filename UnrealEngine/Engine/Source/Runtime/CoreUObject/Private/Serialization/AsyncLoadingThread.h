// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncLoadingThread.h: Unreal async loading code.
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/AsyncLoading.h"
#include "Serialization/LoadTimeTracePrivate.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/LowLevelMemTracker.h"
#include "Serialization/AsyncPackageLoader.h"

#include <atomic>

struct FPrecacheCallbackHandler;

/** [EDL] Event Driven loader event */
struct FAsyncLoadEvent
{
	enum
	{
		EventSystemPriority_MAX = MAX_int32
	};

	int32 UserPriority;
	int32 PackageSerialNumber;
	int32 EventSystemPriority;
	int32 SerialNumber;
	TFunction<void(FAsyncLoadEventArgs& Args)> Payload;

	FAsyncLoadEvent()
		: UserPriority(0)
		, PackageSerialNumber(0)
		, EventSystemPriority(0)
		, SerialNumber(0)
	{
	}
	FAsyncLoadEvent(int32 InUserPriority, int32 InPackageSerialNumber, int32 InEventSystemPriority, int32 InSerialNumber, TFunction<void(FAsyncLoadEventArgs& Args)>&& InPayload)
		: UserPriority(InUserPriority)
		, PackageSerialNumber(InPackageSerialNumber)
		, EventSystemPriority(InEventSystemPriority)
		, SerialNumber(InSerialNumber)
		, Payload(Forward<TFunction<void(FAsyncLoadEventArgs& Args)>>(InPayload))
	{
	}

	FORCEINLINE bool operator<(const FAsyncLoadEvent& Other) const
	{
		if (UserPriority != Other.UserPriority)
		{
			return UserPriority > Other.UserPriority;
		}
		if (EventSystemPriority != Other.EventSystemPriority)
		{
			return EventSystemPriority > Other.EventSystemPriority;
		}
		if (PackageSerialNumber != Other.PackageSerialNumber)
		{
			return PackageSerialNumber > Other.PackageSerialNumber; // roughly DFS
		}
		return SerialNumber < Other.SerialNumber;
	}
};

/** [EDL] Event queue for the event driven loader */
struct FAsyncLoadEventQueue
{
	//FCriticalSection AsyncLoadEventQueueCritical; //@todoio maybe this doesn't need to be protected by a critical section. 
	int32 RunningSerialNumber;
	TArray<FAsyncLoadEvent> EventQueue;

	FAsyncLoadEventQueue()
		: RunningSerialNumber(0)
	{
	}

	FORCEINLINE void AddAsyncEvent(int32 UserPriority, int32 PackageSerialNumber, int32 EventSystemPriority, TFunction<void(FAsyncLoadEventArgs& Args)>&& Payload)
	{
		//FScopeLock Lock(&AsyncLoadEventQueueCritical);
		//@todoio check(FAsyncLoadingThread::IsInAsyncLoadThread());
		EventQueue.HeapPush(FAsyncLoadEvent(UserPriority, PackageSerialNumber, EventSystemPriority, ++RunningSerialNumber, Forward<TFunction<void(FAsyncLoadEventArgs& Args)>>(Payload)));
	}
	bool PopAndExecute(FAsyncLoadEventArgs& Args)
	{
		FAsyncLoadEvent Event;
		bool bResult = false;
		{
			//FScopeLock Lock(&AsyncLoadEventQueueCritical);
			//@todoio check(FAsyncLoadingThread::IsInAsyncLoadThread());
			if (EventQueue.Num())
			{
				EventQueue.HeapPop(Event, EAllowShrinking::No);
				bResult = true;
			}
		}
		if (bResult)
		{
			Event.Payload(Args);
		}
		return bResult;
	}
};

// Shared data for FFlushRequest
struct FFlushRequestData
{
	FFlushRequestData(TConstArrayView<int32> InRequestIDs)
		: RequestIDs(MakeUnique<std::atomic<int32>[]>(InRequestIDs.Num()))
		, NumRequests(InRequestIDs.Num())
	{
		for (int32 i=0; i < InRequestIDs.Num(); ++i)
		{
			RequestIDs.Get()[i] = InRequestIDs[i];
		}
	}
	
	TConstArrayView<std::atomic<int32>> GetRequestIDs() const
	{
		return MakeArrayView(RequestIDs.Get(), NumRequests);
	}

	TArrayView<std::atomic<int32>> GetRequestIDs() 
	{
		return MakeArrayView(RequestIDs.Get(), NumRequests);
	}
	
	void OnRequestComplete(int32 InCompletedRequest)
	{
		for (std::atomic<int32>& RequestID : GetRequestIDs())
		{
			if (RequestID.load(std::memory_order_relaxed) == InCompletedRequest)
			{
				++NumCompletedRequests;
			}
		}
	}
	
	bool IsComplete()
	{
		return NumRequests == NumCompletedRequests;
	}

private:
	/** 
	 * Shared request id state between main thread and async loading thread. 
	 * @note because request id are assigned to package request after being generated
	 * multiple request ids can identify a specific package request however only the original request id
	 * will properly identify unloaded dependencies. The shared nature of the request allows the async loading thread to
	 * fix up the main thread flush request id to identify the original request id instead of duplicate when trying to identify
	 * which main thread packages should be processed when flushing.
	 */
	TUniquePtr<std::atomic<int32>[]> RequestIDs;	
	int32 NumRequests;
	int32 NumCompletedRequests = 0;
};

/** 
 * Request to flush a specific package using the request id 
 */
struct FFlushRequest
{
	FFlushRequest() = default;

	FFlushRequest(TConstArrayView<int32> InRequestIDs)
		: Data(MakeShared<FFlushRequestData>(InRequestIDs))
	{
	}
	
	TConstArrayView<std::atomic<int32>> GetRequestIDs() const 
	{
		return Data->GetRequestIDs();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}
	
	bool IsComplete() const
	{
		return Data->IsComplete();
	}

	operator bool() const
	{
		return IsValid();
	}

private:
	void OnRequestComplete(int32 RequestID)
	{
		Data->OnRequestComplete(RequestID);	
	}

	void AdjustRequestIDs(TMap<int32, int32>& DuplicateRequestMap)
	{
		for (std::atomic<int32>& RequestID : Data->GetRequestIDs())
		{	
			if (int32* MainRequest = DuplicateRequestMap.Find(RequestID.load(std::memory_order_relaxed)))
			{
				RequestID.store(*MainRequest, std::memory_order_relaxed);
			}
		}
	}

	friend class FAsyncLoadingThread;

	TSharedPtr<FFlushRequestData, ESPMode::ThreadSafe> Data;
};

/** Holds the maximum package summary size that can be set via ini files
  * This is used for the initial precache and should be large enough to hold the actual Sum.TotalHeaderSize 
  */
struct FMaxPackageSummarySize
{
	static int32 Value;
	static void Init();
};

/**
 * Async loading thread. Preloads/serializes packages on async loading thread. Postloads objects on the game thread.
 */
class FAsyncLoadingThread final : public FRunnable, public IAsyncPackageLoader
{
	friend struct FAsyncPackage;

	struct FEDLBootNotificationManager& EDLBootNotificationManager;

	/** Thread to run the worker FRunnable on */
	FRunnableThread* Thread;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;

	/** [ASYNC/GAME THREAD] true if the async thread is actually started. We don't start it until after we boot because the boot process on the game thread can create objects that are also being created by the loader */
	static bool bThreadStarted;

	/** [ASYNC/GAME THREAD] Event used to signal there's queued packages to stream */
	FEvent* QueuedRequestsEvent;
	/** [ASYNC/GAME THREAD] Event used to signal loading should be cancelled */
	FEvent* CancelLoadingEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread should be suspended */
	FEvent* ThreadSuspendedEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread has resumed */
	FEvent* ThreadResumedEvent;
	/** [ASYNC/GAME THREAD] List of queued packages to stream */
	TArray<FAsyncPackageDesc*> QueuedPackages;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Package queue critical section */
	FCriticalSection QueueCritical;
#endif
	/** [ASYNC/GAME THREAD] True if the async loading thread received a request to cancel async loading **/
	FThreadSafeBool bShouldCancelLoading;
	/** [ASYNC/GAME THREAD] True if the async loading thread received a request to suspend **/
	FThreadSafeCounter IsLoadingSuspended;
	/** [ASYNC/GAME THREAD] Event used to signal there's queued packages to stream */
	TArray<FAsyncPackage*> LoadedPackages;	
	TMap<FName, FAsyncPackage*> LoadedPackagesNameLookup;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Critical section for LoadedPackages list */
	FCriticalSection LoadedPackagesCritical;
#endif
	/** [GAME THREAD] Event used to signal there's queued packages to stream */
	TArray<FAsyncPackage*> LoadedPackagesToProcess;
	TArray<FAsyncPackage*> PackagesToDelete;
	TMap<FName, FAsyncPackage*> LoadedPackagesToProcessNameLookup;
#if WITH_EDITOR
	TSet<FWeakObjectPtr> LoadedAssets;
#endif
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Critical section for LoadedPackagesToProcess list. 
	 * Note this is only required for looking up existing packages on the async loading thread 
	 */
	FCriticalSection LoadedPackagesToProcessCritical;
#endif

	/** [ASYNC/GAME THREAD] FIFO of current flush requests from the game thread, managed by the async thread.
	 * producer can only be the game thread.
	 */
	TArray<FFlushRequest, TInlineAllocator<4>> FlushRequests;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Critical section for the FlushRequests stack. needed if running with the async loading thread. */
	FCriticalSection FlushRequestCritical;
#endif
	/** [ASYNC THREAD] Map containing a mapping of duplicate request to their main request. Use to operate flush request accurately. */
	TMap<int32, int32> DuplicateRequestMap;

	/** [ASYNC THREAD] Array of packages that are being preloaded */
	TArray<FAsyncPackage*> AsyncPackages;
	TMap<FName, FAsyncPackage*> AsyncPackageNameLookup;
public:
	/** [EDL] Async Packages that are ready for tick */
	TArray<FAsyncPackage*> AsyncPackagesReadyForTick;
private:

#if THREADSAFE_UOBJECTS
	/** We only lock AsyncPackages array to make GetAsyncLoadPercentage thread safe, so we only care about locking Add/Remove operations on the async thread */
	FCriticalSection AsyncPackagesCritical;
#endif

	/** List of all pending package requests */
	TSet<int32> PendingRequests;
#if THREADSAFE_UOBJECTS
	/** Synchronization object for PendingRequests list */
	FCriticalSection PendingRequestsCritical;
#endif

	/** [ASYNC/GAME THREAD] Number of package load requests in the async loading queue */
	FThreadSafeCounter QueuedPackagesCounter;
	/** [ASYNC/GAME THREAD] Number of packages being loaded on the async thread and post loaded on the game thread */
	FThreadSafeCounter ExistingAsyncPackagesCounter;

	FThreadSafeCounter AsyncThreadReady;

	/** When cancelling async loading: list of package requests to cancel */
	TArray<FAsyncPackageDesc*> QueuedPackagesToCancel;
	/** When cancelling async loading: list of packages to cancel */
	TSet<FAsyncPackage*> PackagesToCancel;

	/** Async loading thread ID */
	static uint32 AsyncLoadingThreadID;

	/** Enum describing async package request insertion mode */
	enum class EAsyncPackageInsertMode
	{
		InsertBeforeMatchingPriorities,	// Insert this package before all other packages of the same priority
		InsertAfterMatchingPriorities	// Insert this package after all other packages of the same priority
	};

#if LOOKING_FOR_PERF_ISSUES
	/** Thread safe counter used to accumulate cycles spent on blocking. Using stats may generate to many stats messages. */
	static FThreadSafeCounter BlockingCycles;
#endif

	IAsyncPackageLoader* IoStorePackageLoader = nullptr;

public:

	FAsyncLoadingThread(int32 InThreadIndex);
	virtual ~FAsyncLoadingThread();

	virtual ELoaderType GetLoaderType() const override
	{
		return ELoaderType::LegacyLoader;
	}

	IAsyncPackageLoader* GetIoStorePackageLoader() const
	{
		return IoStorePackageLoader;
	}

	void SetIoStorePackageLoader(IAsyncPackageLoader* InIoStorePackageLoader)
	{
		IoStorePackageLoader = InIoStorePackageLoader;
	}

	//~ Begin FRunnable Interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	//~ End FRunnable Interface

	/** Returns the async loading thread singleton */
	static FAsyncLoadingThread& Get()
	{
		check(Instance);
		return *Instance;
	}

	/** Start the async loading thread */
	void StartThread() override;

	bool ShouldAlwaysLoadPackageAsync(const FPackagePath& InPackagePath) override;

	int32 LoadPackage(const FPackagePath& PackagePath, FLoadPackageAsyncOptionalParams OptionalParams) override;

	int32 LoadPackage(
			const FPackagePath& InPackagePath,
			FName InCustomName,
			FLoadPackageAsyncDelegate InCompletionDelegate,
			EPackageFlags InPackageFlags,
			int32 InPIEInstanceID,
			int32 InPackagePriority,
			const FLinkerInstancingContext* InInstancingContext,
			uint32 InLoadFlags) override;

	EAsyncPackageState::Type ProcessLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit) override;

	EAsyncPackageState::Type ProcessLoadingUntilComplete(TFunctionRef<bool()> CompletionPredicate, double TimeLimit) override;

	void FlushLoading(TConstArrayView<int32> RequestIDs) override;

	void NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObject) override;

	void NotifyUnreachableObjects(const TArrayView<FUObjectItem*>& UnreachableObjects) override {};

	void FireCompletedCompiledInImport(void* AsyncPacakge, FPackageIndex Import);

	void NotifyRegistrationEvent(const TCHAR* PackageName, const TCHAR* Name, ENotifyRegistrationType NotifyRegistrationType, ENotifyRegistrationPhase NotifyRegistrationPhase, UObject* (*InRegister)(), bool InbDynamic, UObject* FinishedObject) override;

	void NotifyRegistrationComplete() override;

	/** [EDL] Event queue */
	FAsyncLoadEventQueue EventQueue;

	/** [EDL] Gets a package from a weak pointer */
	FORCEINLINE FAsyncPackage* GetPackage(FWeakAsyncPackagePtr Ptr)
	{
		if (Ptr.PackageName != NAME_None && Ptr.SerialNumber)
		{
			FAsyncPackage* Package = FindAsyncPackage(Ptr.PackageName);
			if (Package && Package->SerialNumber == Ptr.SerialNumber)
			{
				return Package;
			}
		}
		return nullptr;
	}

	/** [EDL] Queues CreateLinker event */
	void QueueEvent_CreateLinker(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues FinishLinker event */
	void QueueEvent_FinishLinker(FWeakAsyncPackagePtr WeakPtr, int32 EventSystemPriority = 0);
	/** [EDL] Queues StartImportPackages event */
	void QueueEvent_StartImportPackages(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues SetupImports event */
	void QueueEvent_SetupImports(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues SetupExports event */
	void QueueEvent_SetupExports(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues ProcessImportsAndExports event */
	void QueueEvent_ProcessImportsAndExports(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues ExportsDone event */
	void QueueEvent_ExportsDone(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues ProcessPostload event */
	void QueueEvent_ProcessPostloadWait(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);
	/** [EDL] Queues StartPostLoad event */
	void QueueEvent_StartPostLoad(FAsyncPackage* Pkg, int32 EventSystemPriority = 0);

	/** True if multithreaded async loading is currently being used. */
	FORCEINLINE bool IsMultithreaded() override
	{
		return bThreadStarted;
	}

	/** Sets the current state of async loading */
	static void EnterAsyncLoadingTick(int32 ThreadIndex)
	{
		AsyncLoadingTickCounter.Increment();
		check(CurrentAsyncLoadingTickThreadIndex == INDEX_NONE || CurrentAsyncLoadingTickThreadIndex == ThreadIndex);
		CurrentAsyncLoadingTickThreadIndex = ThreadIndex;
	}

	static void LeaveAsyncLoadingTick(int32 ThreadIndex)
	{
		int32 AsyncLoadingTickCounterValue = AsyncLoadingTickCounter.Decrement();
		check(AsyncLoadingTickCounterValue >= 0);
		check(CurrentAsyncLoadingTickThreadIndex == ThreadIndex);
		if (AsyncLoadingTickCounterValue == 0)
		{
			CurrentAsyncLoadingTickThreadIndex = INDEX_NONE;
		}
	}

	/** Gets the current state of async loading */
	static bool GetIsInAsyncLoadingTick()
	{
		return !!AsyncLoadingTickCounter.GetValue();
	}

	FORCEINLINE bool IsAsyncLoadingPackages() override
	{
		FPlatformMisc::MemoryBarrier();
		return QueuedPackagesCounter.GetValue() != 0 || ExistingAsyncPackagesCounter.GetValue() != 0;
	}

	FORCEINLINE bool IsInAsyncLoadThread() override
	{
		bool bResult = false;
		if (IsMultithreaded())
		{
			// We still need to report we're in async loading thread even if 
			// we're on game thread but inside of async loading code (PostLoad mostly)
			// to make it behave exactly like the non-threaded version
			bResult = FPlatformTLS::GetCurrentThreadId() == AsyncLoadingThreadID || 
				(IsInGameThread() && GetIsInAsyncLoadingTick());
		}
		else
		{
			bResult = IsInGameThread() && GetIsInAsyncLoadingTick();
		}
		return bResult;
	}

	/** Returns true if async loading is suspended */
	FORCEINLINE bool IsAsyncLoadingSuspendedInternal() const
	{
		return !!IsLoadingSuspended.GetValue();
	}

	virtual bool IsAsyncLoadingSuspended() override
	{
		return IsAsyncLoadingSuspendedInternal();
	}

	FORCEINLINE int32 GetAsyncLoadingSuspendedCount()
	{
		FPlatformMisc::MemoryBarrier();
		return IsLoadingSuspended.GetValue();
	}

	/** Returns the number of async packages that are currently queued but not yet processed */
	FORCEINLINE int32 GetNumQueuedPackages() override
	{
		return QueuedPackagesCounter.GetValue();
	}

	/** Returns the number of async packages that are currently being processed */
	FORCEINLINE int32 GetNumAsyncPackages() override
	{
		return ExistingAsyncPackagesCounter.GetValue();
	}

	/**
	* [ASYNC THREAD] Finds an existing async package in the AsyncPackages by its name.
	*
	* @param PackageName async package name.
	* @return Pointer to the package or nullptr if not found
	*/
	FORCEINLINE FAsyncPackage* FindAsyncPackage(const FName& PackageName)
	{
		checkSlow(IsInAsyncLoadThread());
		return AsyncPackageNameLookup.FindRef(PackageName);
	}

	/**
	* [ASYNC THREAD] Inserts package to queue according to priority.
	*
	* @param PackageName - async package name.
	* @param InsertMode - Insert mode, describing how we insert this package into the request list
	*/
	void InsertPackage(FAsyncPackage* Package, bool bReinsert = false, EAsyncPackageInsertMode InsertMode = EAsyncPackageInsertMode::InsertBeforeMatchingPriorities);

	/**
	* [ASYNC THREAD] Finds an existing async package in the LoadedPackages by its name.
	*
	* @param PackageName async package name.
	* @return Index of the async package in LoadedPackages array or INDEX_NONE if not found.
	*/
	FORCEINLINE FAsyncPackage* FindLoadedPackage(const FName& PackageName)
	{
		checkSlow(IsInAsyncLoadThread());
		return LoadedPackagesNameLookup.FindRef(PackageName);
	}

	/**
	* [ASYNC/GAME THREAD] Queues a package for streaming.
	*
	* @param Package package descriptor.
	*/
	void QueuePackage(FAsyncPackageDesc& Package);

	/**
	* [GAME THREAD] Cancels streaming
	*/
	void CancelLoading() override;

	/**
	* [GAME THREAD] Stops the async loading thread and blocks until the thread has exited.
	*/
	void ShutdownLoading() override;

	/**
	* [GAME THREAD] Suspends async loading thread
	*/
	void SuspendLoading() override;

	/**
	* [GAME THREAD] Resumes async loading thread
	*/
	void ResumeLoading() override;

	/**
	* [ASYNC/GAME THREAD] Queues a package for streaming.
	*
	* @param Package package descriptor.
	*/
	FORCEINLINE FAsyncPackage* GetPackage(int32 PackageIndex)
	{
		checkSlow(IsInAsyncLoadThread());
		return AsyncPackages[PackageIndex];
	}

	/**
	* [ASYNC* THREAD] Loads all packages
	*
	* @param OutPackagesProcessed Number of packages processed in this call.
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushRequest The request to flush a package and its depedencies. May be invalid.
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessAsyncLoading(int32& OutPackagesProcessed, bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, FFlushRequest& FlushRequest);

	/**
	* [EDL] [ASYNC* THREAD] Checks fopr cycles in the event driven loader and does fatal errors in that case
	*/
	void CheckForCycles();

	/**
	* [GAME THREAD] Ticks game thread side of async loading.
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushRequest The request to flush a package and its depedencies. May be invalid.
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type TickAsyncLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, FFlushRequest FlushRequest = FFlushRequest());

	/**
	* [ASYNC THREAD] Main thread loop
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushRequest The request to flush a package and its depedencies. May be invalid.
	*/
	EAsyncPackageState::Type TickAsyncThread(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, bool& bDidSomething, FFlushRequest& FlushRequest);

	/** Initializes async loading thread */
	void InitializeLoading() override;

	/** 
	 * [GAME THREAD] Gets the load percentage of the specified package
	 * @param PackageName Name of the package to return async load percentage for
	 * @return Percentage (0-100) of the async package load or -1 of package has not been found
	 */
	float GetAsyncLoadPercentage(const FName& PackageName) override;


private:
	/** 
	 * [ASYNC/GAME THREAD] Adds a request ID to the list of pending requests
	 */
	void AddPendingRequest(int32 RequestID)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		if (!PendingRequests.Contains(RequestID))
		{
			PendingRequests.Add(RequestID);
		}
	}

	/** 
	 * [ASYNC/GAME THREAD] Removes a request ID from the list of pending requests
	 */
	void RemovePendingRequests(TArray<int32>& RequestIDs)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		for (int32 ID : RequestIDs)
		{
			PendingRequests.Remove(ID);
			TRACE_LOADTIME_END_REQUEST(ID);
		}		
	}

	/**
	 * [ASYNC/GAME THREAD] Pop flush request from the flush request stacks.
	 * @note: one of the request id passed should normally match the top of the request stack if any.
	 */
	void CompleteFlushRequests(TArray<int32>& RequestIDs);

	/**
	 * [ASYNC THREAD] Add a mapping being duplicate request and the main/first explicit request made for a package
	 * @param DuplicateRequestId the duplicate request id
	 * @param MainRequestId the main request id that will be used for processing flush request
	 */
	void MapDuplicateRequestID(int32 DuplicateRequestId, int32 MainRequestId);


	/** [ASYNC/GAME THREAD] Number of package load requests in the async loading queue */
	int32 GetQueuedPackagesCount() const
	{
		return QueuedPackagesCounter.GetValue();
	}
	/** [ASYNC/GAME THREAD] Number of packages being loaded on the async thread and post loaded on the game thread */
	int32 GetExistingAsyncPackagesCount() const
	{
		return ExistingAsyncPackagesCounter.GetValue();
	}

	/**
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsRequestInternal(int32 RequestID)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		return PendingRequests.Contains(RequestID);
	}

	/**
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsAnyRequestInternal(TConstArrayView<int32> RequestIDs)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		return Algo::AnyOf(RequestIDs, [this](int32 RequestID){ return PendingRequests.Contains(RequestID); });
	}

	/**
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsAnyRequestInternal(TConstArrayView<std::atomic<int32>> RequestIDs)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		return Algo::AnyOf(RequestIDs, [this](const std::atomic<int32>& RequestID){ return PendingRequests.Contains(RequestID.load(std::memory_order_relaxed)); });
	}

	/** 
	 * [GAME THREAD] Add a Flush request to the flush stack
	 * @param RequestIDs the request id of the packages to flush
	 * @returns the flushtree for the flush request to use on the game thread
	 * @note the FlushRequest is a shared resource between the async loading thread and the main thread
	 */
	FFlushRequest AddFlushRequest(TConstArrayView<int32> RequestIDs);

	/** 
	 * [ASYNC THREAD] Peek the top of the flush request stack
	 * @returns the current flush tree to consider 
	 */
	FFlushRequest PeekFlushRequest();

	/** 
	 * [ASYNC THREAD] Adjust the flush request if needed, in case of duplicate request
	 * @param  InFlushRequest the flush request to adjust
	 */
	void AdjustFlushRequest(FFlushRequest& FlushRequest);

	/**
	 * [GAME THREAD/ASYNC THREAD] Use by both the game thread and async thread to determine if the current package should be processed in relation to the current flush tree
	 * @param InAsyncPackage The package to potentially process
	 * @param FlushRequest the current flush request
	 * @returns true if we should process the package
	 */
	bool ShouldProcessPackage(FAsyncPackage* InAsyncPackage, const FFlushRequest& FlushRequest);
	
	/**
	* [GAME THREAD] Performs game-thread specific operations on loaded packages (not-thread-safe PostLoad, callbacks)
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param FlushRequest Package dependency tree to be flushed
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessLoadedPackages(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, bool& bDidSomething, const FFlushRequest& FlushRequest);

	/**
	* [ASYNC THREAD] Creates async packages from the queued requests
	*/
	int32 CreateAsyncPackagesFromQueue(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit);

	/**
	* [ASYNC THREAD] Internal helper function for processing a package load request. If dependency preloading is enabled, 
	* it will call itself recursively for all the package dependencies
	*/
	void ProcessAsyncPackageRequest(FAsyncPackageDesc* InRequest, FAsyncPackage* InRootPackage);

	/**
	* [ASYNC THREAD] Internal helper function for updating the priorities of an existing package and all its dependencies
	*/
	void UpdateExistingPackagePriorities(FAsyncPackage* InPackage, TAsyncLoadPriority InNewPriority);

	/**
	* [ASYNC THREAD] Finds existing async package and adds the new request's completion callback to it.
	* @param PackageRequest The package description for the request
	* @param PackageList The package list to look into
	*/
	FAsyncPackage* FindExistingPackageAndAddCompletionCallback(FAsyncPackageDesc* PackageRequest, TMap<FName, FAsyncPackage*>& PackageList);

	/**
	* [ASYNC THREAD] Adds a package to a list of packages that have finished loading on the async thread
	*/
	void AddToLoadedPackages(FAsyncPackage* Package);

	/** Cancels async loading internally */
	void CancelAsyncLoadingInternal();
	void FinalizeCancelAsyncLoadingInternal();

	/** Event graph for EDL */
	FEventLoadGraph EventGraph;	
	/** ELD precache handler */
	FPrecacheCallbackHandler* PrecacheHandler;
	/** This Async Loading Thread Index (future use) */
	int32 AsyncLoadingThreadIndex;

	/** Number of times we re-entered the async loading tick, mostly used by singlethreaded ticking. Debug purposes only. */
	static FThreadSafeCounter AsyncLoadingTickCounter;
	static int32 CurrentAsyncLoadingTickThreadIndex;

public:

	/** Gets the EDL event graph */
	FEventLoadGraph& GetEventGraph()
	{
		return EventGraph;
	}

	/** Gets the EDL precache handler */
	FPrecacheCallbackHandler& GetPrecacheHandler()
	{
		return *PrecacheHandler;
	}

	/** Gets this ALT index (future use) */
	int32 GetThreadIndex() const
	{
		return AsyncLoadingThreadIndex;
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FThreadSafeCounter RecursionNotAllowed;
#endif

	static FAsyncLoadingThread* Instance;
};
