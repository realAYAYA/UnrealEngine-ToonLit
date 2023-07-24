// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TaskGraphInterfaces.h: TaskGraph library
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Delegates/Delegate.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/LockFreeList.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Event.h"
#include "HAL/LowLevelMemTracker.h"
#include "Templates/RefCounting.h"
#include "Containers/LockFreeFixedSizeAllocator.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "Misc/MemStack.h"
#include "Templates/Atomic.h"
#include "ProfilingDebugging/MetadataTrace.h"

#include "Async/Fundamental/Task.h"

#include "Async/TaskGraphFwd.h"
#include "Async/TaskTrace.h"
#include "Tasks/TaskPrivate.h"
#include "Async/InheritedContext.h"

#if !defined(STATS)
#error "STATS must be defined as either zero or one."
#endif

// what level of checking to perform...normally checkSlow but could be ensure or check
#define checkThreadGraph checkSlow

//#define checkThreadGraph(x) ((x)||((*(char*)3) = 0))

DECLARE_STATS_GROUP(TEXT("Task Graph Tasks"), STATGROUP_TaskGraphTasks, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("FReturnGraphTask"), STAT_FReturnGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FTriggerEventGraphTask"), STAT_FTriggerEventGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ParallelFor"), STAT_ParallelFor, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("ParallelForTask"), STAT_ParallelForTask, STATGROUP_TaskGraphTasks, CORE_API);

namespace ENamedThreads
{
	enum Type : int32
	{
		UnusedAnchor = -1,
		/** The always-present, named threads are listed next **/
		RHIThread,
		GameThread,
		// The render thread is sometimes the game thread and is sometimes the actual rendering thread
		ActualRenderingThread = GameThread + 1,
		// CAUTION ThreadedRenderingThread must be the last named thread, insert new named threads before it

		/** not actually a thread index. Means "Unknown Thread" or "Any Unnamed Thread" **/
		AnyThread = 0xff, 

		/** High bits are used for a queue index and priority**/

		MainQueue =			0x000,
		LocalQueue =		0x100,

		NumQueues =			2,
		ThreadIndexMask =	0xff,
		QueueIndexMask =	0x100,
		QueueIndexShift =	8,

		/** High bits are used for a queue index task priority and thread priority**/

		NormalTaskPriority =	0x000,
		HighTaskPriority =		0x200,

		NumTaskPriorities =		2,
		TaskPriorityMask =		0x200,
		TaskPriorityShift =		9,

		NormalThreadPriority = 0x000,
		HighThreadPriority = 0x400,
		BackgroundThreadPriority = 0x800,

		NumThreadPriorities = 3,
		ThreadPriorityMask = 0xC00,
		ThreadPriorityShift = 10,

		/** Combinations **/
		GameThread_Local = GameThread | LocalQueue,
		ActualRenderingThread_Local = ActualRenderingThread | LocalQueue,

		AnyHiPriThreadNormalTask = AnyThread | HighThreadPriority | NormalTaskPriority,
		AnyHiPriThreadHiPriTask = AnyThread | HighThreadPriority | HighTaskPriority,

		AnyNormalThreadNormalTask = AnyThread | NormalThreadPriority | NormalTaskPriority,
		AnyNormalThreadHiPriTask = AnyThread | NormalThreadPriority | HighTaskPriority,

		AnyBackgroundThreadNormalTask = AnyThread | BackgroundThreadPriority | NormalTaskPriority,
		AnyBackgroundHiPriTask = AnyThread | BackgroundThreadPriority | HighTaskPriority,
	};

	struct FRenderThreadStatics
	{
	private:
		// These are private to prevent direct access by anything except the friend functions below
		static CORE_API TAtomic<Type> RenderThread;
		static CORE_API TAtomic<Type> RenderThread_Local;

		friend Type GetRenderThread();
		friend Type GetRenderThread_Local();
		friend void SetRenderThread(Type Thread);
		friend void SetRenderThread_Local(Type Thread);
	};

	FORCEINLINE Type GetRenderThread()
	{
		return FRenderThreadStatics::RenderThread.Load(EMemoryOrder::Relaxed);
	}

	FORCEINLINE Type GetRenderThread_Local()
	{
		return FRenderThreadStatics::RenderThread_Local.Load(EMemoryOrder::Relaxed);
	}

	FORCEINLINE void SetRenderThread(Type Thread)
	{
		FRenderThreadStatics::RenderThread.Store(Thread, EMemoryOrder::Relaxed);
	}

	FORCEINLINE void SetRenderThread_Local(Type Thread)
	{
		FRenderThreadStatics::RenderThread_Local.Store(Thread, EMemoryOrder::Relaxed);
	}

	// these allow external things to make custom decisions based on what sorts of task threads we are running now.
	// this are bools to allow runtime tuning.
	extern CORE_API int32 bHasBackgroundThreads; 
	extern CORE_API int32 bHasHighPriorityThreads;

	FORCEINLINE Type GetThreadIndex(Type ThreadAndIndex)
	{
		return ((ThreadAndIndex & ThreadIndexMask) == AnyThread) ? AnyThread : Type(ThreadAndIndex & ThreadIndexMask);
	}

	FORCEINLINE int32 GetQueueIndex(Type ThreadAndIndex)
	{
		return (ThreadAndIndex & QueueIndexMask) >> QueueIndexShift;
	}

	FORCEINLINE int32 GetTaskPriority(Type ThreadAndIndex)
	{
		return (ThreadAndIndex & TaskPriorityMask) >> TaskPriorityShift;
	}

	FORCEINLINE int32 GetThreadPriorityIndex(Type ThreadAndIndex)
	{
		int32 Result = (ThreadAndIndex & ThreadPriorityMask) >> ThreadPriorityShift;
		check(Result >= 0 && Result < NumThreadPriorities);
		return Result;
	}

	FORCEINLINE Type SetPriorities(Type ThreadAndIndex, Type ThreadPriority, Type TaskPriority)
	{
		check(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(ThreadPriority & ~ThreadPriorityMask) && // not a thread priority
			(ThreadPriority & ThreadPriorityMask) != ThreadPriorityMask && // not a valid thread priority
			!(TaskPriority & ~TaskPriorityMask) // not a task priority
			);
		return Type(ThreadAndIndex | ThreadPriority | TaskPriority);
	}

	FORCEINLINE Type SetPriorities(Type ThreadAndIndex, int32 PriorityIndex, bool bHiPri)
	{
		check(
			!(ThreadAndIndex & ~ThreadIndexMask) && // not a thread index
			PriorityIndex >= 0 && PriorityIndex < NumThreadPriorities // not a valid thread priority
			);
		return Type(ThreadAndIndex | (PriorityIndex << ThreadPriorityShift) | (bHiPri ? HighTaskPriority : NormalTaskPriority));
	}

	FORCEINLINE Type SetThreadPriority(Type ThreadAndIndex, Type ThreadPriority)
	{
		check(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(ThreadPriority & ~ThreadPriorityMask) && // not a thread priority
			(ThreadPriority & ThreadPriorityMask) != ThreadPriorityMask // not a valid thread priority
			);
		return Type(ThreadAndIndex | ThreadPriority);
	}

	FORCEINLINE Type SetTaskPriority(Type ThreadAndIndex, Type TaskPriority)
	{
		check(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(TaskPriority & ~TaskPriorityMask) // not a task priority
			);
		return Type(ThreadAndIndex | TaskPriority);
	}
}

DECLARE_INTRINSIC_TYPE_LAYOUT(ENamedThreads::Type);

UE_DEPRECATED(4.26, "No longer supported") extern CORE_API int32 GEnablePowerSavingThreadPriorityReductionCVar;

enum class UE_DEPRECATED(4.26, "No longer supported") EPowerSavingEligibility : uint8
{
	Unknown,
	Eligible,			// When set high priority tasks are eligible for downgrade to normal priority when power saving is required.
	NotEligible			// When set high priority tasks will not be downgraded when power saving is required.
};

class CORE_API FAutoConsoleTaskPriority
{
	FString RawSetting;
	FString FullHelpText;
	FAutoConsoleVariableRef Variable;
	ENamedThreads::Type ThreadPriority;
	ENamedThreads::Type TaskPriority;
	ENamedThreads::Type TaskPriorityIfForcedToNormalThreadPriority;

	static FString CreateFullHelpText(const TCHAR* Name, const TCHAR* OriginalHelp);
	static FString ConfigStringFromPriorities(ENamedThreads::Type InThreadPriority, ENamedThreads::Type InTaskPriority, ENamedThreads::Type InTaskPriorityBackup);
	void OnSettingChanged(IConsoleVariable* Variable);

public:
	FAutoConsoleTaskPriority(const TCHAR* Name, const TCHAR* Help, ENamedThreads::Type DefaultThreadPriority, ENamedThreads::Type DefaultTaskPriority, ENamedThreads::Type DefaultTaskPriorityIfForcedToNormalThreadPriority = ENamedThreads::UnusedAnchor)
		: RawSetting(ConfigStringFromPriorities(DefaultThreadPriority, DefaultTaskPriority, DefaultTaskPriorityIfForcedToNormalThreadPriority))
		, FullHelpText(CreateFullHelpText(Name, Help))
		, Variable(Name, RawSetting, *FullHelpText, FConsoleVariableDelegate::CreateRaw(this, &FAutoConsoleTaskPriority::OnSettingChanged), ECVF_Default)
		, ThreadPriority(DefaultThreadPriority)
		, TaskPriority(DefaultTaskPriority)
		, TaskPriorityIfForcedToNormalThreadPriority(DefaultTaskPriorityIfForcedToNormalThreadPriority)
	{
		// if you are asking for a hi or background thread priority, you must provide a separate task priority to use if those threads are not available.
		check(TaskPriorityIfForcedToNormalThreadPriority != ENamedThreads::UnusedAnchor || ThreadPriority == ENamedThreads::NormalThreadPriority);
	}

	FORCEINLINE ENamedThreads::Type Get(ENamedThreads::Type Thread = ENamedThreads::AnyThread)
	{
		// if we don't have the high priority thread that was asked for, or we are downgrading thread priority due to power saving
		// then use a normal thread priority with the backup task priority
		if (ThreadPriority == ENamedThreads::HighThreadPriority && !ENamedThreads::bHasHighPriorityThreads)
		{
			return ENamedThreads::SetTaskPriority(Thread, TaskPriorityIfForcedToNormalThreadPriority);
		}
		// if we don't have the background priority thread that was asked for, then use a normal thread priority with the backup task priority
		if (ThreadPriority == ENamedThreads::BackgroundThreadPriority && !ENamedThreads::bHasBackgroundThreads)
		{
			return ENamedThreads::SetTaskPriority(Thread, TaskPriorityIfForcedToNormalThreadPriority);
		}
		return ENamedThreads::SetPriorities(Thread, ThreadPriority, TaskPriority);
	}
};


namespace ESubsequentsMode
{
	enum Type
	{
		/** Necessary when another task will depend on this task. */
		TrackSubsequents,
		/** Can be used to save task graph overhead when firing off a task that will not be a dependency of other tasks. */
		FireAndForget
	};
}

/** Convenience typedef for a an array a graph events **/
typedef TArray<FGraphEventRef, TInlineAllocator<4> > FGraphEventArray;

/** returns trace IDs of given tasks **/
CORE_API TArray<TaskTrace::FId> GetTraceIds(const FGraphEventArray& Tasks);

/** Interface to the task graph system **/
class FTaskGraphInterface
{
	friend class FBaseGraphTask;

#if TASKGRAPH_NEW_FRONTEND
	friend UE::Tasks::Private::FTaskBase;
#endif

	/**
	 *	Internal function to queue a task
	 *	@param	Task; the task to queue
	 *	@param	ThreadToExecuteOn; Either a named thread for a threadlocked task or ENamedThreads::AnyThread for a task that is to run on a worker thread
	 *	@param	CurrentThreadIfKnown; This should be the current thread if it is known, or otherwise use ENamedThreads::AnyThread and the current thread will be determined.
	**/
	virtual void QueueTask(class FBaseGraphTask* Task, bool bWakeUpWorker, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) = 0;

public:

	virtual ~FTaskGraphInterface()
	{
	}

	// Startup, shutdown and singleton API

	/** 
	 *	Explicit start call for the system. The ordinary singleton pattern does not work because internal threads start asking for the singleton before the constructor has returned.
	 *	@param	NumThreads; Total number of threads in the system, must be 0 to disable separate taskgraph thread, at least 2 if !threadedrendering, else at least 3
	**/
	static CORE_API void Startup(int32 NumThreads);
	/** 
	 *	Explicit start call to shutdown the system. This is unlikely to work unless the system is idle.
	**/
	static CORE_API void Shutdown();
    /**
     *	Check to see if the system is running.
     **/
    static CORE_API bool IsRunning();
	/**
	 *	Singleton for the system
	 *	@return a reference to the task graph system
	**/
	static CORE_API FTaskGraphInterface& Get();

	/**
	* The task graph is always multi-threaded for platforms that support it.
	* For forked processes, the taskgraph will be singlethread for the master process but becomes multithread in the forked process
	*/
	static bool IsMultithread();

	/** Return the current thread type, if known. **/
	virtual ENamedThreads::Type GetCurrentThreadIfKnown(bool bLocalQueue = false) = 0;

	/** Return true if the current thread is known. **/
	virtual bool IsCurrentThreadKnown() = 0;

	/** 
		Return the number of worker (non-named) threads PER PRIORITY SET.
		This is useful for determining how many tasks to split a job into.
	**/
	virtual	int32 GetNumWorkerThreads() = 0;

	/**
		Return the number of foreground worker threads.
		If the old backend is used, return the number of high-pri workers (0 if high-pri workers are disabled).
	**/
	virtual	int32 GetNumForegroundThreads() = 0;

	/**
		Return the number of background worker threads.
		If the old backend is used, return the number of background workers (0 if background workers are disabled).
	**/
	virtual	int32 GetNumBackgroundThreads() = 0;

	/** Return true if the given named thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns. **/
	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) = 0;

	// External Thread API

	/** 
	 *	A one time call that "introduces" an external thread to the system. Basically, it just sets up the TLS info 
	 *	@param	CurrentThread; The name of the current thread.
	**/
	virtual void AttachToThread(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until idle, then return.
	 *	@param	CurrentThread; The name of this thread
	**/
	virtual uint64 ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until an explicit return request is received, then return.
	 *	@param	CurrentThread; The name of this thread
	**/
	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Request that the given thread stop when it is idle
	**/
	virtual void RequestReturn(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until a list of tasks is complete.
	 *	@param	Tasks - tasks to wait for
	 *	@param	CurrentThread - This thread, must be a named thread
	**/
	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)=0;

	/** 
	 *	When a set of tasks complete, fire a scoped event
	 *	@param	InEvent - event to fire when the task is done
	 *	@param	Tasks - tasks to wait for
	 *	@param	CurrentThreadIfKnown - This thread, if known
	**/
	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type TriggerThread = ENamedThreads::AnyHiPriThreadHiPriTask)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until a task is complete
	 *	@param	Task - task to wait for
	 *	@param	CurrentThread - This thread, must be a named thread
	**/
	void WaitUntilTaskCompletes(const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		WaitUntilTasksComplete({ Task }, CurrentThreadIfKnown);
	}

	void WaitUntilTaskCompletes(FGraphEventRef&& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		WaitUntilTasksComplete({ MoveTemp(Task) }, CurrentThreadIfKnown);
	}

	/** 
	 *	When a task completes, fire a scoped event
	 *	@param	InEvent - event to fire when the task is done
	 *	@param	Task - task to wait for
	 *	@param	CurrentThreadIfKnown - This thread, if known
	**/
	void TriggerEventWhenTaskCompletes(FEvent* InEvent, const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type TriggerThread = ENamedThreads::AnyHiPriThreadHiPriTask)
	{
		FGraphEventArray Prerequistes;
		Prerequistes.Add(Task);
		TriggerEventWhenTasksComplete(InEvent, Prerequistes, CurrentThreadIfKnown, TriggerThread);
	}

	virtual FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed) = 0;

	virtual void StallForTuning(int32 Index, bool Stall) = 0;

	/**
	*	Delegates for shutdown
	*	@param	Callback - function to call prior to shutting down the taskgraph
	**/
	virtual void AddShutdownCallback(TFunction<void()>& Callback) = 0;

	virtual void WakeNamedThread(ENamedThreads::Type ThreadToWake) = 0;

	/**
	*	A (slow) function to call a function on every known thread, both named and workers
	*	@param	Callback - function to call prior to shutting down the taskgraph
	**/
	static void BroadcastSlow_OnlyUseForSpecialPurposes(bool bDoTaskThreads, bool bDoBackgroundThreads, TFunction<void(ENamedThreads::Type CurrentThread)>& Callback);
};

struct FTaskGraphBlockAllocationTag : FDefaultBlockAllocationTag
{
	static constexpr uint32 BlockSize = 64 * 1024;
	static constexpr bool AllowOversizedBlocks = false;
	static constexpr bool RequiresAccurateSize = false;
	static constexpr bool InlineBlockAllocation = true;
	static constexpr const char* TagName = "TaskGraphLinear";

	using Allocator = TBlockAllocationCache<BlockSize, FAlignedAllocator>;
};

#if TASKGRAPH_NEW_FRONTEND

/** 
 *	Base class for all tasks. A replacement for `FBaseGraphTask` and `FGraphEvent` from the old API, based on `Tasks::Private::FTaskBase` functionality
 **/

class FBaseGraphTask : public UE::Tasks::Private::FTaskBase
{
public:
	explicit FBaseGraphTask(const FGraphEventArray* InPrerequisites)
		: FTaskBase(/*InitRefCount=*/ 1)
	{		
		if (InPrerequisites != nullptr)
		{
			for (const FGraphEventRef& Prereq : *InPrerequisites)
			{
				AddPrerequisites(*Prereq);
			}
		}
	}

	void Init(const TCHAR* InDebugName, UE::Tasks::ETaskPriority InPriority, UE::Tasks::EExtendedTaskPriority InExtendedPriority)
	{
		FTaskBase::Init(InDebugName, InPriority, InExtendedPriority);
	}

	void Unlock(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		TryLaunch();
	}

	void Execute(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread, bool bDeleteOnCompletion)
	{	// only called for named thread tasks, normal tasks are executed using `FTaskBase` API directly (see `TGraphTask`)
		checkSlow(NewTasks.Num() == 0);
		checkSlow(bDeleteOnCompletion);
		checkSlow(IsNamedThreadTask());
		verify(TryExecuteTask());
		ReleaseInternalReference(); // named tasks are executed by named threads, outside of the scheduler
	}

	FGraphEventRef GetCompletionEvent()
	{
		return this;
	}

	void DontCompleteUntil(FGraphEventRef NestedTask)
	{
		checkSlow(UE::Tasks::Private::GetCurrentTask() == this); // a nested task can be added only from inside of parent's execution
		AddNested(*NestedTask);
	}

	bool IsComplete() const
	{
		return IsCompleted(); // the new API uses a slightly different name
	}

	static FGraphEventRef CreateGraphEvent();

	void DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		AddRef(); // scheduler's reference
		TryLaunch();
	}

	void DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		check(NewTasks.Num() == 0); // the feature is not used
		DispatchSubsequents();
	}

	void SetDebugName(const TCHAR* InDebugName)
	{	// incompatible with the new API that requires debug name during task construction and doesn't allow to set it later. "debug name" feature was added
		// to the old API recently, is used only in a couple of places and will be fixed manually by switching to the new API
	}

	void Wait(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		// local queue have to be handled by the original TaskGraph implementation. The new frontend doesn't support local queues
		if (ENamedThreads::GetQueueIndex(CurrentThreadIfKnown) != ENamedThreads::MainQueue)
		{
			return FTaskGraphInterface::Get().WaitUntilTaskCompletes(this, CurrentThreadIfKnown);
		}

		return FTaskBase::Wait();
	}

	ENamedThreads::Type GetThreadToExecuteOn() const
	{
		return TranslatePriority(GetExtendedPriority());
	}

	// task priority translation from the old API to the new API
	static void TranslatePriority(ENamedThreads::Type ThreadType, UE::Tasks::ETaskPriority& OutPriority, UE::Tasks::EExtendedTaskPriority& OutExtendedPriority)
	{
		using namespace UE::Tasks;

		ENamedThreads::Type ThreadIndex = ENamedThreads::GetThreadIndex(ThreadType);
		if (ThreadIndex != ENamedThreads::AnyThread)
		{
			check(ThreadIndex == ENamedThreads::GameThread || ThreadIndex == ENamedThreads::GetRenderThread() || ThreadIndex == ENamedThreads::RHIThread);
			EExtendedTaskPriority ConversionMap[] =
			{
				EExtendedTaskPriority::RHIThreadNormalPri,
				EExtendedTaskPriority::GameThreadNormalPri,
				EExtendedTaskPriority::RenderThreadNormalPri
			};
			OutExtendedPriority = ConversionMap[ThreadIndex - ENamedThreads::RHIThread];
			OutExtendedPriority = (EExtendedTaskPriority)((int32)OutExtendedPriority + (ENamedThreads::GetTaskPriority(ThreadType) != ENamedThreads::NormalTaskPriority ? 1 : 0));
			OutExtendedPriority = (EExtendedTaskPriority)((int32)OutExtendedPriority + (ENamedThreads::GetQueueIndex(ThreadType) != ENamedThreads::MainQueue ? 2 : 0));
			OutPriority = ETaskPriority::Count;
		}
		else
		{
			OutExtendedPriority = EExtendedTaskPriority::None;
			uint32 ThreadPriority = GetThreadPriorityIndex(ThreadType);
			check(ThreadPriority < uint32(ENamedThreads::NumThreadPriorities));
			ETaskPriority ConversionMap[int(ENamedThreads::NumThreadPriorities)] = { ETaskPriority::Normal, ETaskPriority::High, ETaskPriority::BackgroundNormal };
			OutPriority = ConversionMap[ThreadPriority];
		}

		if (OutPriority == ETaskPriority::BackgroundNormal && GetTaskPriority(ThreadType))
		{
			OutPriority = ETaskPriority::BackgroundHigh;
		}
	}

	// task priority translation from the new API to the old API
	static ENamedThreads::Type TranslatePriority(UE::Tasks::EExtendedTaskPriority Priority)
	{
		using namespace UE::Tasks;

		checkf(Priority >= EExtendedTaskPriority::GameThreadNormalPri && Priority < EExtendedTaskPriority::Count, TEXT("only named threads can call this method: %d"), Priority);

		int32 ConversionMap[] =
		{
				ENamedThreads::GameThread, // GameThreadNormalPri
				ENamedThreads::GameThread | ENamedThreads::HighTaskPriority, // GameThreadHiPri
				ENamedThreads::GameThread | ENamedThreads::LocalQueue, // GameThreadNormalPriLocalQueue
				ENamedThreads::GameThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue, // GameThreadHiPriLocalQueue

				ENamedThreads::GetRenderThread(), // RenderThreadNormalPri
				ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority, // RenderThreadHiPri
				ENamedThreads::GetRenderThread() | ENamedThreads::LocalQueue, // RenderThreadNormalPriLocalQueue
				ENamedThreads::GetRenderThread() | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue, // RenderThreadHiPriLocalQueue

				ENamedThreads::RHIThread, // RHIThreadNormalPri
				ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority, // RHIThreadHiPri
				ENamedThreads::RHIThread | ENamedThreads::LocalQueue, // RHIThreadNormalPriLocalQueue
				ENamedThreads::RHIThread | ENamedThreads::HighTaskPriority | ENamedThreads::LocalQueue // RHIThreadHiPriLocalQueue
		};

		return (ENamedThreads::Type)ConversionMap[(int32)Priority - (int32)EExtendedTaskPriority::GameThreadNormalPri];
	}
};

static constexpr int32 SmallTaskSize = 256;
using FGraphTaskAllocator = TLockFreeFixedSizeAllocator_TLSCache<SmallTaskSize, PLATFORM_CACHE_LINE_SIZE>;
CORE_API extern FGraphTaskAllocator SmallTaskAllocator;

// the new task implementation integrated into the old task API
template<typename TTask>
class TGraphTask : public FBaseGraphTask
{
public:
	/**
	 *	This is a helper class returned from the factory. It constructs the embeded task with a set of arguments and sets the task up and makes it ready to execute.
	 *	The task may complete before these routines even return.
	 **/
	class FConstructor
	{
	public:
		UE_NONCOPYABLE(FConstructor);

		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename...T>
		FORCEINLINE_DEBUGGABLE FGraphEventRef ConstructAndDispatchWhenReady(T&&... Args)
		{
			FGraphEventRef Ref{ ConstructAndHoldImpl(Forward<T>(Args)...) };
			Ref->TryLaunch();
			return Ref;
		}

		/** Passthrough internal task constructor and hold. */
		template<typename...T>
		FORCEINLINE_DEBUGGABLE TGraphTask* ConstructAndHold(T&&... Args)
		{
			TGraphTask* Task = ConstructAndHoldImpl(Forward<T>(Args)...);
			TaskTrace::Created(Task->GetTraceId());
			return Task;
		}

		FConstructor(const FGraphEventArray* InPrerequisites)
			: Prerequisites(InPrerequisites)
		{
		}

	private:
		template<typename...T>
		FORCEINLINE_DEBUGGABLE TGraphTask* ConstructAndHoldImpl(T&&... Args)
		{
			TGraphTask* Task = new TGraphTask(Prerequisites);
			TTask* TaskObject = new(&Task->TaskStorage) TTask(Forward<T>(Args)...);

			UE::Tasks::ETaskPriority Pri;
			UE::Tasks::EExtendedTaskPriority ExtPri;
			TranslatePriority(TaskObject->GetDesiredThread(), Pri, ExtPri);

			Task->Init(Pri, ExtPri);

			return Task;
		}

	private:
		const FGraphEventArray* Prerequisites;
	};

	/**
	 *	Factory to create a task and return the helper object to construct the embedded task and set it up for execution.
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return a temporary helper class which can be used to complete the process.
	**/
	static FConstructor CreateTask(const FGraphEventArray* Prerequisites = nullptr, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		return FConstructor(Prerequisites);
	}

	FORCENOINLINE virtual ~TGraphTask() override
	{
		DestructItem(TaskStorage.GetTypedPtr());
	}

	static void* operator new(size_t Size);
	static void operator delete(void* Ptr, size_t Size);

private:
	explicit TGraphTask(const FGraphEventArray* InPrerequisites)
		: FBaseGraphTask(InPrerequisites)
	{
	}

	void Init(UE::Tasks::ETaskPriority InPriority, UE::Tasks::EExtendedTaskPriority InExtendedPriority)
	{
		FBaseGraphTask::Init(TEXT("GraphTask"), InPriority, InExtendedPriority);
	}

	virtual bool TryExecuteTaskVirtual() override
	{
		return TryExecute(
			[](UE::Tasks::Private::FTaskBase& Task)
			{
				TGraphTask& This = static_cast<TGraphTask&>(Task);
				FGraphEventRef GraphEventRef{ &This };
				TTask* TaskObject = This.TaskStorage.GetTypedPtr();
				ENamedThreads::Type ThreadIndex = ENamedThreads::GetThreadIndex(TaskObject->GetDesiredThread());

				TaskObject->DoTask(ThreadIndex, GraphEventRef);
			}
		);
	}

private:
	TTypeCompatibleBytes<TTask> TaskStorage;
};

template<typename TTask>
void* TGraphTask<TTask>::operator new(size_t Size)
{
	return Size <= SmallTaskSize ? SmallTaskAllocator.Allocate() : GMalloc->Malloc(sizeof(TGraphTask), PLATFORM_CACHE_LINE_SIZE);
}

template<typename TTask>
void TGraphTask<TTask>::operator delete(void* Ptr, size_t Size)
{
	Size <= SmallTaskSize ? SmallTaskAllocator.Free(Ptr) : GMalloc->Free(Ptr);
}

// an adaptation of FBaseGraphTask to be used as a standalone FGraphEvent
class FGraphEventImpl : public FBaseGraphTask
{
public:
	FGraphEventImpl()
		: FBaseGraphTask(nullptr)
	{
		Init(TEXT("GraphEvent"), UE::Tasks::ETaskPriority::Normal, UE::Tasks::EExtendedTaskPriority::TaskEvent);
	}

	static void* operator new(size_t Size);
	static void operator delete(void* Ptr);

private:
	virtual bool TryExecuteTaskVirtual() override
	{
		checkNoEntry(); // graph events are never executed
		return true;
	}
};

using FGraphEventImplAllocator = TLockFreeFixedSizeAllocator_TLSCache<sizeof(FGraphEventImpl), PLATFORM_CACHE_LINE_SIZE>;
CORE_API extern FGraphEventImplAllocator GraphEventImplAllocator;

inline void* FGraphEventImpl::operator new(size_t Size)
{
	return GraphEventImplAllocator.Allocate();
}

inline void FGraphEventImpl::operator delete(void* Ptr)
{
	GraphEventImplAllocator.Free(Ptr);
}

inline FGraphEventRef FBaseGraphTask::CreateGraphEvent()
{
	FGraphEventImpl* GraphEvent = new FGraphEventImpl;
	return FGraphEventRef{ GraphEvent, /*bAddRef = */ false };
}

#else // TASKGRAPH_NEW_FRONTEND

struct FTaskBlockAllocationTag : FDefaultBlockAllocationTag
{
	static constexpr uint32 BlockSize = 64 * 1024;
	static constexpr bool AllowOversizedBlocks = false;
	static constexpr bool RequiresAccurateSize = false;
	static constexpr bool InlineBlockAllocation = true;
	static constexpr const char* TagName = "TaskLinearAllocator";

	using Allocator = TBlockAllocationCache<BlockSize, FAlignedAllocator>;
};

/** 
 *	Base class for all tasks. 
 *	Tasks go through a very specific life stage progression, and this is verified.
 **/

class FBaseGraphTask : private UE::FInheritedContextBase
{
protected:
	/** 
	 *	Constructor
	 *	@param InNumberOfPrerequistitesOutstanding; the number of prerequisites outstanding. We actually add one to this to prevent the task from firing while we are setting up the task
	 **/
	FBaseGraphTask(int32 InNumberOfPrerequistitesOutstanding)
		: ThreadToExecuteOn(ENamedThreads::AnyThread)
		, NumberOfPrerequistitesOutstanding(InNumberOfPrerequistitesOutstanding + 1) // + 1 is not a prerequisite, it is a lock to prevent it from executing while it is getting prerequisites, one it is safe to execute, call PrerequisitesComplete
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Contructed));
		CaptureInheritedContext();
	}
	/** 
	 *	Sets the desired execution thread. This is not part of the constructor because this information may not be known quite yet duiring construction.
	 *	@param InThreadToExecuteOn; the desired thread to execute on.
	 **/
	void SetThreadToExecuteOn(ENamedThreads::Type InThreadToExecuteOn)
	{
		ThreadToExecuteOn = InThreadToExecuteOn;
		checkThreadGraph(LifeStage.Increment() == int32(LS_ThreadSet));
	}

	/** 
	 *	Indicates that the prerequisites are set up and that the task can be executed as soon as the prerequisites are finished.
	 *	@param NumAlreadyFinishedPrequistes; the number of prerequisites that have not been set up because those tasks had already completed.
	 *	@param bUnlock; if true, let the task execute if it can
	 **/
	void PrerequisitesComplete(ENamedThreads::Type CurrentThread, int32 NumAlreadyFinishedPrequistes, bool bUnlock = true)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_PrequisitesSetup));
		int32 NumToSub = NumAlreadyFinishedPrequistes + (bUnlock ? 1 : 0); // the +1 is for the "lock" we set up in the constructor
		if (NumberOfPrerequistitesOutstanding.Subtract(NumToSub) == NumToSub) 
		{
			bool bWakeUpWorker = true;
			QueueTask(CurrentThread, bWakeUpWorker);	
		}
	}
	/** destructor, just checks the life stage **/
	virtual ~FBaseGraphTask()
	{
#if DO_GUARD_SLOW
		int32 Stage = LifeStage.Increment();
		checkf(Stage == int32(LS_Deconstucted), TEXT("LifeStage was %d"), Stage);
#endif
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Logs a task name that may contain invalid subsequents. Debug only. */
	static void CORE_API LogPossiblyInvalidSubsequentsTask(const TCHAR* TaskName);
#endif

	/** 
	 *	An indication that a prerequisite has been completed. Reduces the number of prerequisites by one and if no prerequisites are outstanding, it queues the task for execution.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 **/
	void ConditionalQueueTask(ENamedThreads::Type CurrentThread, bool& bWakeUpWorker)
	{
		if (NumberOfPrerequistitesOutstanding.Decrement()==0)
		{
			QueueTask(CurrentThread, bWakeUpWorker);
			bWakeUpWorker = true;
		}
	}

	TaskTrace::FId GetTraceId() const
	{
#if UE_TASK_TRACE_ENABLED
		return TraceId.load(std::memory_order_relaxed);
#else
		return TaskTrace::InvalidId;
#endif
	}

	void SetTraceId(TaskTrace::FId InTraceId)
	{
#if UE_TASK_TRACE_ENABLED
		TraceId = InTraceId;
#endif
	}

	LowLevelTasks::FTask& GetTaskHandle()
	{
		return TaskHandle;
	}

	ENamedThreads::Type GetThreadToExecuteOn() const
	{
		return ThreadToExecuteOn;
	}

private:
	friend class FNamedTaskThread;
	friend class FTaskThreadBase;
	friend class FTaskThreadAnyThread;
	friend class FGraphEvent;
	friend class FTaskGraphImplementation;
	friend class FTaskGraphCompatibilityImplementation;

	LowLevelTasks::FTask TaskHandle;

	// Subclass API

	/** 
	 *	Virtual call to actually execute the task. This will also call the destructor and free any memory in the old backend if bDeleteOnCompletion is set to true.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new tasks.
	 *  @param bDeleteOnCompletion; specifies if the task should delete itself after completion.
	 **/
	virtual void ExecuteTask(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread, bool bDeleteOnCompletion)=0;

	/** 
	*	Virtual call to actually delete the task any memory. This is used for the New Backend.
	**/
	virtual void DeleteTask() = 0;

	// API called from other parts of the system

	/** 
	 *	Called by the system to execute this task after it has been removed from an internal queue.
	 *	Just checks the life stage and passes off to the virtual ExecuteTask method.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new tasks.
	 *  @param bDeleteOnCompletion; specifies if the task should delete itself after completion.
	 **/
	FORCEINLINE void Execute(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread, bool bDeleteOnCompletion)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Executing));

		UE::FInheritedContextScope InheritedContextScope = RestoreInheritedContext();
		ExecuteTask(NewTasks, CurrentThread, bDeleteOnCompletion);
	}

	// Internal Use

	/** 
	 *	Queues the task for execution.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 **/
	void QueueTask(ENamedThreads::Type CurrentThreadIfKnown, bool bWakeUpWorker)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Queued));
		TaskTrace::Scheduled(GetTraceId());
		FTaskGraphInterface::Get().QueueTask(this, bWakeUpWorker, ThreadToExecuteOn, CurrentThreadIfKnown);
	}

	/**	Thread to execute on, can be ENamedThreads::AnyThread to execute on any unnamed thread **/
	ENamedThreads::Type			ThreadToExecuteOn;
	/**	Number of prerequisites outstanding. When this drops to zero, the thread is queued for execution.  **/
	FThreadSafeCounter			NumberOfPrerequistitesOutstanding; 


#if DO_GUARD_SLOW || USING_CODE_ANALYSIS
	// Life stage verification
	// Tasks go through 8 steps, in order. In non-final builds, we track them with a thread safe counter and verify that the progression is correct.
	enum ELifeStage
	{
		LS_BaseContructed = 0,
		LS_Contructed,
		LS_ThreadSet,
		LS_PrequisitesSetup,
		LS_Queued,
		LS_Executing,
		LS_Deconstucted,
	};
	FThreadSafeCounter			LifeStage;

#endif

#if UE_TASK_TRACE_ENABLED
	std::atomic<TaskTrace::FId> TraceId { TaskTrace::InvalidId };
#endif
};

/** 
 *	A FGraphEvent is a list of tasks waiting for something.
 *	These tasks are call the subsequents.
 *	A graph event is a prerequisite for each of its subsequents.
 *	Graph events have a lifetime managed by reference counting.
 **/
class FGraphEvent 
{
public:
	/**
	 *	A factory method to create a graph event. 
	 *	@return a reference counted pointer to the new graph event. Note this should be stored in a FGraphEventRef or it will be immediately destroyed!
	**/
	static CORE_API FGraphEventRef CreateGraphEvent();
	
	/**
	 *	Attempts to a new subsequent task. If this event has already fired, false is returned and action must be taken to ensure that the task will still fire even though this event cannot be a prerequisite (because it is already finished).
	 *	@return true if the task was successfully set up as a subsequent. false if the event has already fired.
	**/
	bool AddSubsequent(class FBaseGraphTask* Subsequent)
	{
		bool bSucceeded = SubsequentList.PushIfNotClosed(Subsequent);
		if (bSucceeded)
		{
			TaskTrace::SubsequentAdded(GetTraceId(), Subsequent->GetTraceId());
		}
		return bSucceeded;
	}

	/**
	 *	Verification function to ensure that nobody was tried to add WaitUntil's outside of the context of execution
	**/
	void CheckDontCompleteUntilIsEmpty()
	{
		checkThreadGraph(!EventsToWaitFor.Num());
	}

	/**
	 *	Delay the firing of this event until the given event fires.
	 *	CAUTION: This is only legal while executing the task associated with this event.
	 *	@param EventToWaitFor event to wait for until we fire.
	**/
	void DontCompleteUntil(FGraphEventRef EventToWaitFor)
	{
		checkThreadGraph(!IsComplete()); // it is not legal to add a DontCompleteUntil after the event has been completed. Basically, this is only legal within a task function.
		new (EventsToWaitFor) FGraphEventRef(EventToWaitFor);
		TaskTrace::SubsequentAdded(EventToWaitFor->GetTraceId(), GetTraceId());
	}

	/**
	*	Sets the thread that you want to execute the null gather task on. This is useful if the thing waiting for this chain to complete is a single, named thread. 
	*	CAUTION: This is only legal while executing the task associated with this event.
	*	@param ThreadToDoGatherOn thread and priority to execute null gather task on
	**/
	UE_DEPRECATED(4.26, "The feature is not supported anymore. Please remove the call, there's no replacement.")
	void SetGatherThreadForDontCompleteUntil(ENamedThreads::Type InThreadToDoGatherOn)
	{
		checkThreadGraph(!IsComplete()); // it is not legal to add a DontCompleteUntil after the event has been completed. Basically, this is only legal within a task function.
		ThreadToDoGatherOn = InThreadToDoGatherOn;
	}

	/**
	 *	"Complete" the event. This grabs the list of subsequents and atomically closes it. Then for each subsequent it reduces the number of prerequisites outstanding and if that drops to zero, the task is queued.
	 *	@param CurrentThreadIfKnown if the current thread is known, provide it here. Otherwise it will be determined via TLS if any task ends up being queued.
	**/
	CORE_API void DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);

	/**
	 *	"Complete" the event. This grabs the list of subsequents and atomically closes it. Then for each subsequent and for each item in "NewTasks" it reduces the number of prerequisites outstanding and if that drops to zero, the task is queued. 
	 * @param NewTasks subsequents to add
	 *	@param	 CurrentThreadIfKnown if the current thread is known, provide it here. Otherwise it will be determined via TLS if any task ends up being queued.
	**/
	CORE_API void DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);

	/**
	 *	Determine if the event has been completed. This can be used to poll for completion. 
	 *	@return true if this event has completed. 
	 *	CAUTION: If this returns false, the event could still end up completing before this function even returns. In other words, a false return does not mean that event is not yet completed!
	**/
	bool IsComplete() const
	{
		return SubsequentList.IsClosed();
	}

	/**
	 * A convenient short version of `FTaskGraphInterface::WaitUntilTaskCompletes`
	 */
	void Wait(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(this, CurrentThreadIfKnown);
	}

	/**
	 * Sets a name for the event for debugging purposes.
	 */
	void SetDebugName(const TCHAR* Name)
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		DebugName = Name;
#endif
	}

	TaskTrace::FId GetTraceId() const
	{
#if UE_TASK_TRACE_ENABLED
		return TraceId.load(std::memory_order_relaxed);
#else
		return TaskTrace::InvalidId;
#endif
	}

	// does nothing and is needed only for compatibility with the new frontend
	FGraphEventRef CreateCompletionHandle()
	{
		// nothing to do here as this instance already serves as a completion handle
		return this;
	}

private:
	friend class TRefCountPtr<FGraphEvent>;
	friend class TLockFreeClassAllocator_TLSCache<FGraphEvent, PLATFORM_CACHE_LINE_SIZE>;

	/** 
	 *	Internal function to call the destructor and recycle a graph event
	 *	@param ToRecycle; graph event to recycle
	**/
	static CORE_API void Recycle(FGraphEvent* ToRecycle);

	/**
	 *	Hidden Constructor
	**/
	friend struct FGraphEventAndSmallTaskStorage;
	FGraphEvent()
		: ThreadToDoGatherOn(ENamedThreads::AnyHiPriThreadHiPriTask)
	{
	}

	/**
	 *	Destructor. Verifies we aren't destroying it prematurely. 
	**/
	~FGraphEvent();

	// Interface for TRefCountPtr

public:
	/** 
	 *	Increases the reference count 
	 *	@return the new reference count
	**/
	uint32 AddRef()
	{
		int32 RefCount = ReferenceCount.Increment();
		checkThreadGraph(RefCount > 0);
		return RefCount;
	}
	/** 
	 *	Decreases the reference count and destroys the graph event if it is zero.
	 *	@return the new reference count
	**/
	uint32 Release()
	{
		int32 RefCount = ReferenceCount.Decrement();
		checkThreadGraph(RefCount >= 0);
		if (RefCount == 0)
		{
			Recycle(this);
		}
		return RefCount;
	}

	uint32 GetRefCount() const
	{
		return ReferenceCount.GetValue();
	}

private:

	/** Threadsafe list of subsequents for the event **/
	TClosableLockFreePointerListUnorderedSingleConsumer<FBaseGraphTask, 0>	SubsequentList;
	/** List of events to wait for until firing. This is not thread safe as it is only legal to fill it in within the context of an executing task. **/
	FGraphEventArray														EventsToWaitFor;
	/** Number of outstanding references to this graph event **/
	FThreadSafeCounter														ReferenceCount;
	ENamedThreads::Type														ThreadToDoGatherOn;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	const TCHAR* DebugName = nullptr;
#endif

#if UE_TASK_TRACE_ENABLED
	std::atomic<TaskTrace::FId> TraceId { TaskTrace::GenerateTaskId() };
#endif
};

/** 
 The user defined task type can take arguments to a constructor. These arguments (unfortunately) must not be references.
 The API required of TTask:

class FGenericTask
{
	TSomeType	SomeArgument;
public:
	FGenericTask(TSomeType InSomeArgument) // CAUTION!: Must not use references in the constructor args; use pointers instead if you need by reference
		: SomeArgument(InSomeArgument)
	{
		// Usually the constructor doesn't do anything except save the arguments for use in DoWork or GetDesiredThread.
	}
	~FGenericTask()
	{
		// you will be destroyed immediately after you execute. Might as well do cleanup in DoWork, but you could also use a destructor.
	}
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericTask, STATGROUP_TaskGraphTasks);
	}

	[static] ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::[named thread or AnyThread];
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// The arguments are useful for setting up other tasks. 
		// Do work here, probably using SomeArgument.
		MyCompletionGraphEvent->DontCompleteUntil(TGraphTask<FSomeChildTask>::CreateTask(NULL,CurrentThread).ConstructAndDispatchWhenReady());
	}
};
**/


/** 
 *	TGraphTask
 *	Embeds a user defined task, as exemplified above, for doing the work and provides the functionality for setting up and handling prerequisites and subsequents
 **/
template<typename TTask>
class TGraphTask final : public TConcurrentLinearObject<TGraphTask<TTask>, FTaskGraphBlockAllocationTag>, public FBaseGraphTask
{
public:
	/** 
	 *	This is a helper class returned from the factory. It constructs the embeded task with a set of arguments and sets the task up and makes it ready to execute.
	 *	The task may complete before these routines even return.
	 **/
	class FConstructor
	{
	public:
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename...T>
		FGraphEventRef ConstructAndDispatchWhenReady(T&&... Args)
		{
			new ((void *)&Owner->TaskStorage) TTask(Forward<T>(Args)...);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}

		/** Passthrough internal task constructor and hold. */
		template<typename...T>
		TGraphTask* ConstructAndHold(T&&... Args)
		{
			new ((void *)&Owner->TaskStorage) TTask(Forward<T>(Args)...);
			return Owner->Hold(Prerequisites, CurrentThreadIfKnown);
		}

	private:
		friend class TGraphTask;

		/** The task that created me to assist with embeded task construction and preparation. **/
		TGraphTask*						Owner;
		/** The list of prerequisites. **/
		const FGraphEventArray*			Prerequisites;
		/** If known, the current thread.  ENamedThreads::AnyThread is also fine, and if that is the value, we will determine the current thread, as needed, via TLS. **/
		ENamedThreads::Type				CurrentThreadIfKnown;

		/** Constructor, simply saves off the arguments for later use after we actually construct the embeded task. **/
		FConstructor(TGraphTask* InOwner, const FGraphEventArray* InPrerequisites, ENamedThreads::Type InCurrentThreadIfKnown)
			: Owner(InOwner)
			, Prerequisites(InPrerequisites)
			, CurrentThreadIfKnown(InCurrentThreadIfKnown)
		{
		}
		/** Prohibited copy construction **/
		FConstructor(const FConstructor& Other)
		{
			check(0);
		}
		/** Prohibited copy **/
		void operator=(const FConstructor& Other)
		{
			check(0);
		}
	};

	/** 
	 *	Factory to create a task and return the helper object to construct the embedded task and set it up for execution.
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return a temporary helper class which can be used to complete the process.
	**/
	static FConstructor CreateTask(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		int32 NumPrereq = Prerequisites ? Prerequisites->Num() : 0;
		return FConstructor(new TGraphTask(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget ? NULL : FGraphEvent::CreateGraphEvent(), NumPrereq), Prerequisites, CurrentThreadIfKnown);
	}

	void Unlock(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		TaskTrace::Launched(GetTraceId(), nullptr, Subsequents.IsValid(), ((TTask*)&TaskStorage)->GetDesiredThread());

		bool bWakeUpWorker = true;
		ConditionalQueueTask(CurrentThreadIfKnown, bWakeUpWorker);
	}

	FGraphEventRef GetCompletionEvent()
	{
		return Subsequents;
	}

private:
	friend class FConstructor;
	friend class FGraphEvent;

	// API derived from FBaseGraphTask

	/** 
	 *	Virtual call to actually execute the task. 
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks.
	 *  @param bDeleteOnCompletion; specifies if the task will will delete itself after execution (true) or if DeleteTask has to be called manually (false).
	 *	Executes the embedded task. 
	 *  Destroys the embedded task.
	 *	Dispatches the subsequents.
	 *	Destroys myself.
	 **/
	void ExecuteTask(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread, bool bDeleteOnCompletion) override
	{
		checkThreadGraph(TaskConstructed);

		// Fire and forget mode must not have subsequents
		// Track subsequents mode must have subsequents
		checkThreadGraph(XOR(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget, IsValidRef(Subsequents)));

		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			Subsequents->CheckDontCompleteUntilIsEmpty(); // we can only add wait for tasks while executing the task
		}
		
		TTask& Task = *(TTask*)&TaskStorage;
		{
			TaskTrace::FTaskTimingEventScope TaskEventScope(GetTraceId());
			FScopeCycleCounter Scope(Task.GetStatId(), true);
			Task.DoTask(CurrentThread, Subsequents);
			Task.~TTask();
			checkThreadGraph(ENamedThreads::GetThreadIndex(CurrentThread) <= ENamedThreads::GetRenderThread() || FMemStack::Get().IsEmpty()); // you must mark and pop memstacks if you use them in tasks! Named threads are excepted.
		}
		
		TaskConstructed = false;

		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			FPlatformMisc::MemoryBarrier();
			Subsequents->DispatchSubsequents(NewTasks, CurrentThread);
		}

		if (bDeleteOnCompletion)
		{
			DeleteTask();
		}
	}

	void DeleteTask() final override
	{
		delete this;
	}

	// Internals 

	/** 
	 *	Private constructor, constructs the base class with the number of prerequisites.
	 *	@param InSubsequents subsequents to associate with this task. This refernence is destroyed in the process!
	 *	@param NumberOfPrerequistitesOutstanding the number of prerequisites this task will have when it is built.
	**/
	TGraphTask(FGraphEventRef InSubsequents, int32 NumberOfPrerequistitesOutstanding)
		: FBaseGraphTask(NumberOfPrerequistitesOutstanding)
		, TaskConstructed(false)
	{
		Subsequents.Swap(InSubsequents);
		SetTraceId(Subsequents.IsValid() ? Subsequents->GetTraceId() : TaskTrace::GenerateTaskId());
	}

	/** 
	 *	Private destructor, just checks that the task appears to be completed
	**/
	~TGraphTask() override
	{
		checkThreadGraph(!TaskConstructed);
	}

	/** 
	 *	Call from FConstructor to complete the setup process
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@param bUnlock; if true, task can execute right now if possible
	 * 
	 *	Create the completed event
	 *	Set the thread to execute on based on the embedded task
	 *	Attempt to add myself as a subsequent to each prerequisite
	 *	Tell the base task that I am ready to start as soon as my prerequisites are ready.
	 **/
	void SetupPrereqs(const FGraphEventArray* Prerequisites, ENamedThreads::Type CurrentThreadIfKnown, bool bUnlock)
	{
		checkThreadGraph(!TaskConstructed);
		TaskConstructed = true;
		TTask& Task = *(TTask*)&TaskStorage;
		SetThreadToExecuteOn(Task.GetDesiredThread());
		int32 AlreadyCompletedPrerequisites = 0;
		if (Prerequisites)
		{
			for (int32 Index = 0; Index < Prerequisites->Num(); Index++)
			{
				FGraphEvent* Prerequisite = (*Prerequisites)[Index];
				if (Prerequisite == nullptr || !Prerequisite->AddSubsequent(this))
				{
					AlreadyCompletedPrerequisites++;
				}
			}
		}
		PrerequisitesComplete(CurrentThreadIfKnown, AlreadyCompletedPrerequisites, bUnlock);
	}

	/** 
	 *	Call from FConstructor to complete the setup process
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return A new graph event which represents the completion of this task.
	 * 
	 *	Create the completed event
	 *	Set the thread to execute on based on the embedded task
	 *	Attempt to add myself as a subsequent to each prerequisite
	 *	Tell the base task that I am ready to start as soon as my prerequisites are ready.
	 **/
	FGraphEventRef Setup(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		TaskTrace::Launched(GetTraceId(), nullptr, Subsequents.IsValid(), ((TTask*)&TaskStorage)->GetDesiredThread());

		FGraphEventRef ReturnedEventRef = Subsequents; // very important so that this doesn't get destroyed before we return
		SetupPrereqs(Prerequisites, CurrentThreadIfKnown, true);
		return ReturnedEventRef;
	}

	/** 
	 *	Call from FConstructor to complete the setup process, but doesn't allow the task to dispatch yet
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return a pointer to the task
	 * 
	 *	Create the completed event
	 *	Set the thread to execute on based on the embedded task
	 *	Attempt to add myself as a subsequent to each prerequisite
	 *	Tell the base task that I am ready to start as soon as my prerequisites are ready.
	 **/
	TGraphTask* Hold(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		TaskTrace::Created(GetTraceId());

		SetupPrereqs(Prerequisites, CurrentThreadIfKnown, false);
		return this;
	}

	/** 
	 *	Factory to create a gather task which assumes the given subsequent list from some other tasks.
	 *	This is used to support "WaitFor" during a task execution.
	 *	@param SubsequentsToAssume; subsequents to "assume" from an existing task
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to dispatching my seubsequents.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	**/
	static FConstructor CreateTask(FGraphEventRef SubsequentsToAssume, const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		return FConstructor(new TGraphTask(SubsequentsToAssume, Prerequisites ? Prerequisites->Num() : 0), Prerequisites, CurrentThreadIfKnown);
	}

	/** An aligned bit of storage to hold the embedded task **/
	TAlignedBytes<sizeof(TTask),alignof(TTask)> TaskStorage;
	/** Used to sanity check the state of the object **/
	bool						TaskConstructed;
	/** A reference counted pointer to the completion event which lists the tasks that have me as a prerequisite. **/
	FGraphEventRef				Subsequents;
};

#endif // TASKGRAPH_NEW_FRONTEND

/** 
 *	FReturnGraphTask is a task used to return flow control from a named thread back to the original caller of ProcessThreadUntilRequestReturn
 **/
class FReturnGraphTask
{
public:

	/** 
	 *	Constructor
	 *	@param InThreadToReturnFrom; named thread to cause to return
	**/
	FReturnGraphTask(ENamedThreads::Type InThreadToReturnFrom)
		: ThreadToReturnFrom(InThreadToReturnFrom)
	{
		checkThreadGraph(ThreadToReturnFrom != ENamedThreads::AnyThread); // doesn't make any sense to return from any thread
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return GET_STATID(STAT_FReturnGraphTask);
	}

	/** 
	 *	Retrieve the thread that this task wants to run on.
	 *	@return the thread that this task should run on.
	 **/
	ENamedThreads::Type GetDesiredThread()
	{
		return ThreadToReturnFrom;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/** 
	 *	Actually execute the task.
	 *	@param	CurrentThread; the thread we are running on
	 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
	 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	 **/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		checkThreadGraph(ENamedThreads::GetThreadIndex(ThreadToReturnFrom) == ENamedThreads::GetThreadIndex(CurrentThread)); // we somehow are executing on the wrong thread.
		FTaskGraphInterface::Get().RequestReturn(ThreadToReturnFrom);
	}

private:
	/** Named thread that we want to cause to return to the caller of ProcessThreadUntilRequestReturn. **/
	ENamedThreads::Type ThreadToReturnFrom;
};

/**
 * Class that generalizes functionality of storing and exposing custom stat id.
 * Should only be used as a base of a task graph class with custom stat id.
 */
class FCustomStatIDGraphTaskBase
{
public:
	/** 
	 * Constructor.
	 *
	 * @param StatId The stat id for this task.
	 */
	FCustomStatIDGraphTaskBase(const TStatId& StatId)
	{
#if STATS || ENABLE_STATNAMEDEVENTS
		StatID = StatId;
#endif
	}

	/** 
	 * Gets stat id for this task.
	 *
	 * @return Stat id.
	 */
	FORCEINLINE TStatId GetStatId() const
	{
#if STATS|| ENABLE_STATNAMEDEVENTS
		return StatID;
#endif
		return TStatId();
	}

private:
#if STATS|| ENABLE_STATNAMEDEVENTS
	/** Stat id of this object. */
	TStatId StatID;
#endif

};

/** 
 *	FNullGraphTask is a task that does nothing. It can be used to "gather" tasks into one prerequisite.
 **/
class FNullGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	/** 
	 *	Constructor
	 *	@param StatId The stat id for this task.
	 *	@param InDesiredThread; Thread to run on, can be ENamedThreads::AnyThread
	**/
	FNullGraphTask(const TStatId& StatId, ENamedThreads::Type InDesiredThread)
		: FCustomStatIDGraphTaskBase(StatId)
		, DesiredThread(InDesiredThread)
	{
	}

	/** 
	 *	Retrieve the thread that this task wants to run on.
	 *	@return the thread that this task should run on.
	 **/
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/** 
	 *	Actually execute the task.
	 *	@param	CurrentThread; the thread we are running on
	 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
	 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	 **/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
	}
private:
	/** Thread to run on, can be ENamedThreads::AnyThread **/
	ENamedThreads::Type DesiredThread;
};

/** 
 * FTriggerEventGraphTask is a task that triggers an event
 */
class FTriggerEventGraphTask
{
public:
	/** 
	 *	Constructor
	 *	@param InScopedEvent; Scoped event to fire
	**/
	FTriggerEventGraphTask(FEvent* InEvent, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyHiPriThreadHiPriTask)
		: Event(InEvent) 
		, DesiredThread(InDesiredThread)
	{
		check(Event);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return GET_STATID(STAT_FTriggerEventGraphTask);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Event->Trigger();
	}
private:
	FEvent* Event;
	/** Thread to run on, can be ENamedThreads::AnyThread **/
	ENamedThreads::Type DesiredThread;
};

/** Task class for simple delegate based tasks. This is less efficient than a custom task, doesn't provide the task arguments, doesn't allow specification of the current thread, etc. **/
class FSimpleDelegateGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	DECLARE_DELEGATE(FDelegate);

	/** Delegate to fire when task runs **/
	FDelegate							TaskDelegate;
	/** Thread to run delegate on **/
	const ENamedThreads::Type			DesiredThread;
public:
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate.ExecuteIfBound();
	}
	/**
	  * Task constructor
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  *	@param StatId The stat id for this task.
	  * @param InDesiredThread - Thread to run on
	**/
	FSimpleDelegateGraphTask(const FDelegate& InTaskDeletegate, const TStatId StatId, ENamedThreads::Type InDesiredThread)
		: FCustomStatIDGraphTaskBase(StatId)
		, TaskDelegate(InTaskDeletegate)
		, DesiredThread(InDesiredThread)
	{
	}

	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InStatId - StatId of task for debugging or analysis tools
	  * @param InPrerequisites - Handles for prerequisites for this task, can be NULL if there are no prerequisites
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TStatId InStatId, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<FSimpleDelegateGraphTask>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady<const FDelegate&>(InTaskDeletegate, InStatId, InDesiredThread);
	}
	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InStatId - StatId of task for debugging or analysis tools
	  * @param InPrerequisite - Handle for a single prerequisite for this task
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TStatId&& InStatId, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(InTaskDeletegate, InStatId, &Prerequisites, InDesiredThread);
	}
};

/** Task class for more full featured delegate based tasks. Still less efficient than a custom task, but provides all of the args **/
class FDelegateGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	DECLARE_DELEGATE_TwoParams( FDelegate,ENamedThreads::Type, const  FGraphEventRef& );

	/** Delegate to fire when task runs **/
	FDelegate							TaskDelegate;
	/** Thread to run delegate on **/
	const ENamedThreads::Type			DesiredThread;
public:
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() 
	{ 
		return ESubsequentsMode::TrackSubsequents; 
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate.ExecuteIfBound(CurrentThread, MyCompletionGraphEvent);
	}
	/**
	  * Task constructor
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  *	@param InStatId - The stat id for this task.
	  * @param InDesiredThread - Thread to run on
	**/
	FDelegateGraphTask(const FDelegate& InTaskDeletegate, const TStatId InStatId, ENamedThreads::Type InDesiredThread)
		: FCustomStatIDGraphTaskBase(InStatId)
		, TaskDelegate(InTaskDeletegate)
		, DesiredThread(InDesiredThread)
	{
	}

	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  *	@param InStatId - The stat id for this task.
	  * @param InPrerequisites - Handles for prerequisites for this task, can be NULL if there are no prerequisites
	  * @param InCurrentThreadIfKnown - This thread, if known
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TStatId InStatId, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<FDelegateGraphTask>::CreateTask(InPrerequisites, InCurrentThreadIfKnown).ConstructAndDispatchWhenReady<const FDelegate&>(InTaskDeletegate, InStatId, InDesiredThread);
	}
	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  *	@param InStatId - The stat id for this task.
	  * @param InPrerequisite - Handle for a single prerequisite for this task
	  * @param InCurrentThreadIfKnown - This thread, if known
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TStatId InStatId, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(InTaskDeletegate, InStatId, &Prerequisites, InCurrentThreadIfKnown, InDesiredThread);
	}
};

/** Task class for lambda based tasks. **/
template<typename Signature, ESubsequentsMode::Type SubsequentsMode>
class TFunctionGraphTaskImpl : public FCustomStatIDGraphTaskBase
{
private:
	/** Function to run **/
	TUniqueFunction<Signature> Function;
	/** Thread to run the function on **/
	const ENamedThreads::Type DesiredThread;

public:
	/**
	 * Task constructor
	 * @param InFunction - function to execute when the prerequisites are complete
	 *	@param StatId The stat id for this task.
	 * @param InDesiredThread - Thread to run on
	 **/
	TFunctionGraphTaskImpl(TUniqueFunction<Signature>&& InFunction, TStatId StatId, ENamedThreads::Type InDesiredThread)
		: FCustomStatIDGraphTaskBase(StatId),
		Function(MoveTemp(InFunction)),
		DesiredThread(InDesiredThread)
	{}

	ENamedThreads::Type GetDesiredThread() const
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return SubsequentsMode;
	}

	FORCEINLINE void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DoTaskImpl(Function, CurrentThread, MyCompletionGraphEvent);
	}

private:
	FORCEINLINE static void DoTaskImpl(TUniqueFunction<void()>& Function, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function();
	}

	FORCEINLINE static void DoTaskImpl(TUniqueFunction<void(const FGraphEventRef&)>& Function,
		ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function(MyCompletionGraphEvent);
	}

	FORCEINLINE static void DoTaskImpl(TUniqueFunction<void(ENamedThreads::Type, const FGraphEventRef&)>& Function,
		ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function(CurrentThread, MyCompletionGraphEvent);
	}
};

struct FFunctionGraphTask
{
public:
	/**
	 * Create a task and dispatch it when the prerequisites are complete
	 * @param InFunction - a functor object to execute when the prerequisites are complete, with signature `void()` or `void(ENamedThreads::Type, const FGraphEventRef&)`
	 * @param InStatId - StatId of task for debugging or analysis tools
	 * @param InPrerequisites - Handles for prerequisites for this task, can be NULL if there are no prerequisites
	 * @param InDesiredThread - Thread to run on
	 * @return completion handle for the new task
	 **/
	static FGraphEventRef CreateAndDispatchWhenReady(TUniqueFunction<void()> InFunction, TStatId InStatId = TStatId{}, const FGraphEventArray* InPrerequisites = nullptr, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<TFunctionGraphTaskImpl<void(), ESubsequentsMode::TrackSubsequents>>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady(MoveTemp(InFunction), InStatId, InDesiredThread);
	}

	static FGraphEventRef CreateAndDispatchWhenReady(TUniqueFunction<void(ENamedThreads::Type, const FGraphEventRef&)> InFunction, TStatId InStatId = TStatId{}, const FGraphEventArray* InPrerequisites = nullptr, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<TFunctionGraphTaskImpl<void(ENamedThreads::Type, const FGraphEventRef&), ESubsequentsMode::TrackSubsequents>>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady(MoveTemp(InFunction), InStatId, InDesiredThread);
	}

	/**
	 * Create a task and dispatch it when the prerequisites are complete
	 * @param InFunction - a function to execute when the prerequisites are complete, with signature `void()` or `void(ENamedThreads::Type, const FGraphEventRef&)`
	 * @param InStatId - StatId of task for debugging or analysis tools
	 * @param InPrerequisite - Handle for a single prerequisite for this task
	 * @param InDesiredThread - Thread to run on
	 * @return completion handle for the new task
	 **/
	static FGraphEventRef CreateAndDispatchWhenReady(TUniqueFunction<void()> InFunction, TStatId InStatId, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, &Prerequisites, InDesiredThread);
	}

	static FGraphEventRef CreateAndDispatchWhenReady(TUniqueFunction<void(ENamedThreads::Type, const FGraphEventRef&)> InFunction, TStatId InStatId, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, &Prerequisites, InDesiredThread);
	}
};

/**
 * List of tasks that can be "joined" into one task which can be waited on or used as a prerequisite.
 * Note, these are FGraphEventRef's, but we manually manage the reference count instead of using a smart pointer
**/
class UE_DEPRECATED(4.26, "The feature is deprecated") FCompletionList
{
	TLockFreePointerListUnordered<FGraphEvent, 0>	Prerequisites;
public:
	/**
	 * Adds a task to the completion list, can be called from any thread
	 * @param TaskToAdd, task to add
	 */
	void Add(const FGraphEventRef& TaskToAdd)
	{
		FGraphEvent* Task = TaskToAdd.GetReference();
		checkSlow(Task);
		Task->AddRef(); // manually increasing the ref count
		Prerequisites.Push(Task);
	}
	/**
	  * Task function that waits until any newly added pending commands complete before it completes, forming a chain
	**/
	void ChainWaitForPrerequisites(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// this is tricky...
		// we have waited for a set of pending tasks to execute. However, they may have added more pending tasks that also need to be waited for.
		FGraphEventRef PendingComplete = CreatePrerequisiteCompletionHandle(CurrentThread);
		if (PendingComplete.GetReference())
		{
			MyCompletionGraphEvent->DontCompleteUntil(PendingComplete);
		}
	}
	/**
	 * Create a completion handle that represents the completion of all pending tasks
	 * This is complicated by the fact that some of the tasks we are waiting for might also add tasks
	 * So it is recursive and the task we call here uses DontCompleteUntil to build the chain
	 * this should always be called from the same thread.
	 * @return The task that when completed, indicates all tasks in the list are completed, including any tasks they added recursively. Will be a NULL reference if there are no tasks
	 */
	FGraphEventRef CreatePrerequisiteCompletionHandle(ENamedThreads::Type CurrentThread)
	{
		FGraphEventRef CompleteHandle;
		TArray<FGraphEvent*> Pending;
		// grab all pending command completion handles
		Prerequisites.PopAll(Pending);
		if (Pending.Num())
		{
			FGraphEventArray PendingHandles;
			// convert the pointer list to a list of handles
			for (int32 Index = 0; Index < Pending.Num(); Index++)
			{
				new (PendingHandles) FGraphEventRef(Pending[Index]); 
				Pending[Index]->Release(); // remove the ref count we added when we added it to the lock free list
			}
			// start a new task that won't complete until all of these tasks have executed, plus any tasks that they create when they run
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.WaitOnCompletionList"),
				STAT_FDelegateGraphTask_WaitOnCompletionList,
				STATGROUP_TaskGraphTasks);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
			CompleteHandle = FDelegateGraphTask::CreateAndDispatchWhenReady(
				FDelegateGraphTask::FDelegate::CreateRaw(this, &FCompletionList::ChainWaitForPrerequisites),
				GET_STATID(STAT_FDelegateGraphTask_WaitOnCompletionList), &PendingHandles, CurrentThread, ENamedThreads::AnyHiPriThreadHiPriTask
			);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		return CompleteHandle;
	}
};


