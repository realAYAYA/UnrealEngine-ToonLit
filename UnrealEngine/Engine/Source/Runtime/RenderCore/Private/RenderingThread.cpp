// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingThread.cpp: Rendering thread implementation.
=============================================================================*/

#include "RenderingThread.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ExceptionHandling.h" // IWYU pragma: keep
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CoreStats.h"
#include "Misc/TimeGuard.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RenderCore.h"
#include "RenderCommandFence.h"
#include "RenderDeferredCleanup.h"
#include "TickableObjectRenderThread.h"
#include "Stats/StatsData.h"
#include "HAL/ThreadHeartBeat.h"
#include "RenderResource.h"
#include "RHIUtilities.h"
#include "Misc/ScopeLock.h"
#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/TaskTrace.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ProfilingDebugging/CountersTrace.h"

//
// Globals
//

FCoreRenderDelegates::FOnFlushRenderingCommandsStart FCoreRenderDelegates::OnFlushRenderingCommandsStart;
FCoreRenderDelegates::FOnFlushRenderingCommandsEnd FCoreRenderDelegates::OnFlushRenderingCommandsEnd;

UE_TRACE_CHANNEL_DEFINE(RenderCommandsChannel);

RENDERCORE_API bool GIsThreadedRendering = false;
RENDERCORE_API bool GUseThreadedRendering = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RENDERCORE_API TAtomic<bool> GMainThreadBlockedOnRenderThread(false);
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static FRunnable* GRenderingThreadRunnable = NULL;

/** If the rendering thread has been terminated by an unhandled exception, this contains the error message. */
FString GRenderingThreadError;

/**
 * Polled by the game thread to detect crashes in the rendering thread.
 * If the rendering thread crashes, it sets this variable to false.
 */
volatile bool GIsRenderingThreadHealthy = true;


/**
 * Maximum rate the rendering thread will tick tickables when idle (in Hz)
 */
float GRenderingThreadMaxIdleTickFrequency = 40.f;

/**
 * RT Task Graph polling.
 */

extern CORE_API bool GRenderThreadPollingOn;
extern CORE_API int32 GRenderThreadPollPeriodMs;

static void OnRenderThreadPollPeriodMsChanged(IConsoleVariable* Var)
{
	const int32 DesiredRTPollPeriod = Var->GetInt();

	GRenderThreadPollingOn = (DesiredRTPollPeriod >= 0);
	ENQUEUE_RENDER_COMMAND(WakeupCommand)([DesiredRTPollPeriod](FRHICommandListImmediate&)
	{
		GRenderThreadPollPeriodMs = DesiredRTPollPeriod;
	});
}

static FAutoConsoleVariable CVarRenderThreadPollPeriodMs(
	TEXT("TaskGraph.RenderThreadPollPeriodMs"),
	1,
	TEXT("Render thread polling period in milliseconds. If value < 0, task graph tasks explicitly wake up RT, otherwise RT polls for tasks."),
	FConsoleVariableDelegate::CreateStatic(&OnRenderThreadPollPeriodMsChanged)
);

/** Function to stall the rendering thread **/
static void SuspendRendering()
{
	++GIsRenderingThreadSuspended;
}

/** Function to wait and resume rendering thread **/
static void WaitAndResumeRendering()
{
	while ( GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed) )
	{
		// Just sleep a little bit.
		FPlatformProcess::Sleep( 0.001f ); //@todo this should be a more principled wait
	}
    
	// set the thread back to real time mode
	FPlatformProcess::SetRealTimeMode();
}

/**
 *	Constructor that flushes and suspends the renderthread
 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
 */
FSuspendRenderingThread::FSuspendRenderingThread( bool bInRecreateThread )
{
	// Pause asset streaming to prevent rendercommands from being enqueued.
	SuspendTextureStreamingRenderTasks();

	bRecreateThread = bInRecreateThread;
	bUseRenderingThread = GUseThreadedRendering;
	bWasRenderingThreadRunning = GIsThreadedRendering;
	if ( bRecreateThread )
	{
		StopRenderingThread();
		// GUseThreadedRendering should be set to false after StopRenderingThread call since
		// otherwise a wrong context could be used.
		GUseThreadedRendering = false;
		++GIsRenderingThreadSuspended;
	}
	else
	{
		if ( GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed) == 0 )
		{
			// First tell the render thread to finish up all pending commands and then suspend its activities.
			// this ensures that async stuff will be completed too
			FlushRenderingCommands();
			
			if (GIsThreadedRendering)
			{
				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SuspendRendering"),
					STAT_FSimpleDelegateGraphTask_SuspendRendering,
					STATGROUP_TaskGraphTasks);

				ENamedThreads::Type RenderThread = ENamedThreads::GetRenderThread();

				FGraphEventRef CompleteHandle = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SuspendRendering),
					GET_STATID(STAT_FSimpleDelegateGraphTask_SuspendRendering), NULL, RenderThread);

				// Busy wait while Kismet debugging, to avoid opportunistic execution of game thread tasks
				// If the game thread is already executing tasks, then we have no choice but to spin
				if (GIntraFrameDebuggingGameThread || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread) ) 
				{
					while (!GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
					{
						FPlatformProcess::Sleep(0.0f);
					}
				}
				else
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FSuspendRenderingThread);
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompleteHandle, ENamedThreads::GameThread);
				}
				check(GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed));
			
				// Now tell the render thread to busy wait until it's resumed
				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.WaitAndResumeRendering"),
					STAT_FSimpleDelegateGraphTask_WaitAndResumeRendering,
					STATGROUP_TaskGraphTasks);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&WaitAndResumeRendering),
					GET_STATID(STAT_FSimpleDelegateGraphTask_WaitAndResumeRendering), NULL, RenderThread);
			}
			else
			{
				SuspendRendering();
			}
		}
		else
		{
			// The render-thread is already suspended. Just bump the ref-count.
			++GIsRenderingThreadSuspended;
		}
	}
}

/** Destructor that starts the renderthread again */
FSuspendRenderingThread::~FSuspendRenderingThread()
{
	if ( bRecreateThread )
	{
		GUseThreadedRendering = bUseRenderingThread;
		--GIsRenderingThreadSuspended;
		if ( bUseRenderingThread && bWasRenderingThreadRunning )
		{
			StartRenderingThread();
            
            // Now tell the render thread to set it self to real time mode
			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SetRealTimeMode"),
				STAT_FSimpleDelegateGraphTask_SetRealTimeMode,
				STATGROUP_TaskGraphTasks);

            FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
                FSimpleDelegateGraphTask::FDelegate::CreateStatic(&FPlatformProcess::SetRealTimeMode),
				GET_STATID(STAT_FSimpleDelegateGraphTask_SetRealTimeMode), NULL, ENamedThreads::GetRenderThread()
			);
        }
	}
	else
	{
		// Resume the render thread again.
		--GIsRenderingThreadSuspended;
	}

	// Resume any asset streaming
	ResumeTextureStreamingRenderTasks();

	if (IsAsyncLoadingMultithreaded())
	{
		ResumeAsyncLoading();
	}
}


/**
 * Tick all rendering thread tickable objects
 */

/** Static array of tickable objects that are ticked from rendering thread*/
FTickableObjectRenderThread::FRenderingThreadTickableObjectsArray FTickableObjectRenderThread::RenderingThreadTickableObjects;
FTickableObjectRenderThread::FRenderingThreadTickableObjectsArray FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects;

void TickHighFrequencyTickables(double CurTime)
{
	static double LastHighFreqTime = FPlatformTime::Seconds();
	float DeltaSecondsHighFreq = float(CurTime - LastHighFreqTime);

	// tick any high frequency rendering thread tickables.
	for (int32 ObjectIndex = 0; ObjectIndex < FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects.Num(); ObjectIndex++)
	{
		FTickableObjectRenderThread* TickableObject = FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects[ObjectIndex];
		// make sure it wants to be ticked and the rendering thread isn't suspended
		if (TickableObject->IsTickable())
		{
			STAT(FScopeCycleCounter(TickableObject->GetStatId());)
				TickableObject->Tick(DeltaSecondsHighFreq);
		}
	}

	LastHighFreqTime = CurTime;
}

void TickRenderingTickables()
{
	static double LastTickTime = FPlatformTime::Seconds();
	

	// calc how long has passed since last tick
	double CurTime = FPlatformTime::Seconds();
	float DeltaSeconds = float(CurTime - LastTickTime);
		
	TickHighFrequencyTickables(CurTime);

	if (DeltaSeconds < (1.f/GRenderingThreadMaxIdleTickFrequency))
	{
		return;
	}

	// tick any rendering thread tickables
	for (int32 ObjectIndex = 0; ObjectIndex < FTickableObjectRenderThread::RenderingThreadTickableObjects.Num(); ObjectIndex++)
	{
		FTickableObjectRenderThread* TickableObject = FTickableObjectRenderThread::RenderingThreadTickableObjects[ObjectIndex];
		// make sure it wants to be ticked and the rendering thread isn't suspended
		if (TickableObject->IsTickable())
		{
			STAT(FScopeCycleCounter(TickableObject->GetStatId());)
			TickableObject->Tick(DeltaSeconds);
		}
	}
	// update the last time we ticked
	LastTickTime = CurTime;
}

/** How many cycles the renderthread used (excluding idle time). It's set once per frame in FViewport::Draw. */
uint32 GRenderThreadTime = 0;
/** How many cycles of wait time renderthread used. It's set once per frame in FViewport::Draw. */
uint32 GRenderThreadWaitTime = 0;
/** How many cycles the rhithread used (excluding idle time). */
uint32 GRHIThreadTime = 0;
/** How many cycles the renderthread used, including dependent wait time. */
uint32 GRenderThreadTimeCriticalPath = 0;

/** The RHI thread runnable object. */
class FRHIThread : public FRunnable
{
public:
	FRunnableThread* Thread;

	FRHIThread()
		: Thread(nullptr)
	{
		check(IsInGameThread());
	}

	virtual bool Init(void) override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GRHIThreadId = FPlatformTLS::GetCurrentThreadId();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return true;
	}

	virtual uint32 Run() override
	{
		LLM_SCOPE(ELLMTag::RHIMisc);

#if CSV_PROFILER
		FCsvProfiler::Get()->SetRHIThreadId(FPlatformTLS::GetCurrentThreadId());
#endif

		FMemory::SetupTLSCachesOnCurrentThread();
		{
			FTaskTagScope Scope(ETaskTag::ERhiThread);
			FPlatformProcess::SetupRHIThread();
			FTaskGraphInterface::Get().AttachToThread(ENamedThreads::RHIThread);
			FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::RHIThread);
		}
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return 0;
	}

	static FRHIThread& Get()
	{
		static FRHIThread Singleton;
		return Singleton;
	}

	void Start()
	{
		UE::Trace::ThreadGroupBegin(TEXT("Render"));
		Thread = FRunnableThread::Create(this, TEXT("RHIThread"), 512 * 1024, FPlatformAffinity::GetRHIThreadPriority(),
			FPlatformAffinity::GetRHIThreadMask(), FPlatformAffinity::GetRHIThreadFlags()
			);
		check(Thread);
		UE::Trace::ThreadGroupEnd();
	}
};

/** The rendering thread main loop */
void RenderingThreadMain( FEvent* TaskGraphBoundSyncEvent )
{
	LLM_SCOPE(ELLMTag::RenderingThreadMemory);

	ENamedThreads::Type RenderThread = ENamedThreads::Type(ENamedThreads::ActualRenderingThread);

	ENamedThreads::SetRenderThread(RenderThread);
	ENamedThreads::SetRenderThread_Local(ENamedThreads::Type(ENamedThreads::ActualRenderingThread_Local));

	FTaskGraphInterface::Get().AttachToThread(RenderThread);
	FPlatformMisc::MemoryBarrier();

	// Inform main thread that the render thread has been attached to the taskgraph and is ready to receive tasks
	if( TaskGraphBoundSyncEvent != NULL )
	{
		TaskGraphBoundSyncEvent->Trigger();
	}

	// set the thread back to real time mode
	FPlatformProcess::SetRealTimeMode();

#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FTaskTagScope Scope(ETaskTag::ERenderingThread);
		FThreadStats::ExplicitFlush(); // flush the stats and set update the scope so we don't flush again until a frame update, this helps prevent fragmentation
	}
#endif

	FCoreDelegates::PostRenderingThreadCreated.Broadcast();
	check(GIsThreadedRendering);
	{
		FTaskTagScope TaskTagScope(ETaskTag::ERenderingThread);

		struct FScopedRHIThreadOwnership
		{
			/** Tracks if we have acquired ownership */
			bool bAcquiredThreadOwnership = false;

			FScopedRHIThreadOwnership()
			{
				// Acquire rendering context ownership on the current thread, unless using an RHI thread, which will be the real owner
				if (!IsRunningRHIInSeparateThread())
				{
					bAcquiredThreadOwnership = true;
					RHIAcquireThreadOwnership();
				}
			}

			~FScopedRHIThreadOwnership()
			{
				// Release rendering context ownership on the current thread if we had acquired it
				if (bAcquiredThreadOwnership)
				{
					RHIReleaseThreadOwnership();
				}
			}
		} ThreadOwnershipScope;

		FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(RenderThread);
	}
	FPlatformMisc::MemoryBarrier();
	check(!GIsThreadedRendering);
	FCoreDelegates::PreRenderingThreadDestroyed.Broadcast();
	
#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FThreadStats::ExplicitFlush(); // Another explicit flush to clean up the ScopeCount established above for any stats lingering since the last frame
	}
#endif
	
	ENamedThreads::SetRenderThread(ENamedThreads::GameThread);
	ENamedThreads::SetRenderThread_Local(ENamedThreads::GameThread_Local);
	FPlatformMisc::MemoryBarrier();
}

/**
 * Advances stats for the rendering thread.
 */
static void AdvanceRenderingThreadStats(int64 StatsFrame, int32 DisableChangeTagStartFrame)
{
#if STATS
	int64 Frame = StatsFrame;
	if (!FThreadStats::IsCollectingData() || DisableChangeTagStartFrame != FThreadStats::PrimaryDisableChangeTag())
	{
		Frame = -StatsFrame; // mark this as a bad frame
	}
	FThreadStats::AddMessage(FStatConstants::AdvanceFrame.GetEncodedName(), EStatOperation::AdvanceFrameEventRenderThread, Frame);
	if( IsInActualRenderingThread() )
	{
		FThreadStats::ExplicitFlush();
	}
#endif
}

/**
 * Advances stats for the rendering thread. Called from the game thread.
 */
void AdvanceRenderingThreadStatsGT( bool bDiscardCallstack, int64 StatsFrame, int32 DisableChangeTagStartFrame )
{
	ENQUEUE_RENDER_COMMAND(RenderingThreadTickCommand)(
		[StatsFrame, DisableChangeTagStartFrame](FRHICommandList& RHICmdList)
		{
			AdvanceRenderingThreadStats(StatsFrame, DisableChangeTagStartFrame);
		}
	);
	if( bDiscardCallstack )
	{
		// we need to flush the rendering thread here, otherwise it can get behind and then the stats will get behind.
		FlushRenderingCommands();
	}
}

/** The rendering thread runnable object. */
class FRenderingThread : public FRunnable
{
public:
	/** 
	 * Sync event to make sure that render thread is bound to the task graph before main thread queues work against it.
	 */
	FEvent* TaskGraphBoundSyncEvent;

	FRenderingThread()
	{
		TaskGraphBoundSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);
		RHIFlushResources();
	}

	virtual ~FRenderingThread()
	{
		FPlatformProcess::ReturnSynchEventToPool(TaskGraphBoundSyncEvent);
		TaskGraphBoundSyncEvent = nullptr;
	}

	// FRunnable interface.
	virtual bool Init(void) override
	{ 
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GRenderThreadId = FPlatformTLS::GetCurrentThreadId();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		FTaskTagScope::SetTagNone();
		return true; 
	}

	virtual void Exit(void) override
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GRenderThreadId = 0;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	static int32 FlushRHILogsAndReportCrash(Windows::LPEXCEPTION_POINTERS ExceptionInfo)
	{
		if (GDynamicRHI)
		{
			GDynamicRHI->FlushPendingLogs();
		}

		return ReportCrash(ExceptionInfo);
	}
#endif
	
	void SetupRenderThread()
	{
		FTaskTagScope Scope(ETaskTag::ERenderingThread);
		FPlatformProcess::SetupRenderThread();
	}


	virtual uint32 Run(void) override
	{
		FMemory::SetupTLSCachesOnCurrentThread();
		SetupRenderThread();

#if PLATFORM_WINDOWS
		bool bNoExceptionHandler = FParse::Param(FCommandLine::Get(), TEXT("noexceptionhandler"));
		if ( !bNoExceptionHandler && (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash))
		{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
			__try
#endif
			{
				RenderingThreadMain( TaskGraphBoundSyncEvent );
			}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
			__except (FPlatformMisc::GetCrashHandlingType() == ECrashHandlingType::Default ?
							FlushRHILogsAndReportCrash(GetExceptionInformation()) : 
							EXCEPTION_CONTINUE_SEARCH)
			{
#if !NO_LOGGING
				// Dump the error and flush the log. This is the same logging behavior as FWindowsErrorOutputDevice::HandleError which is called in GuardedMain's caller's __except
				FDebug::LogFormattedMessageWithCallstack(LogWindows.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
				GLog->Panic();

				GRenderingThreadError = GErrorHist;

				// Use a memory barrier to ensure that the game thread sees the write to GRenderingThreadError before
				// the write to GIsRenderingThreadHealthy.
				FPlatformMisc::MemoryBarrier();

				GIsRenderingThreadHealthy = false;
			}
#endif
		}
		else
#endif // PLATFORM_WINDOWS
		{
			RenderingThreadMain( TaskGraphBoundSyncEvent );
		}
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return 0;
	}
};

/**
 * If the rendering thread is in its idle loop (which ticks rendering tickables
 */
TAtomic<bool> GRunRenderingThreadHeartbeat;

FThreadSafeCounter OutstandingHeartbeats;

/** rendering tickables shouldn't be updated during a flush */
TAtomic<int32> GSuspendRenderingTickables;
struct FSuspendRenderingTickables
{
	FSuspendRenderingTickables()
	{
		++GSuspendRenderingTickables;
	}
	~FSuspendRenderingTickables()
	{
		--GSuspendRenderingTickables;
	}
};

/** The rendering thread heartbeat runnable object. */
class FRenderingThreadTickHeartbeat : public FRunnable
{
public:

	// FRunnable interface.
	virtual bool Init(void) 
	{
		GSuspendRenderingTickables = 0;
		OutstandingHeartbeats.Reset();
		return true; 
	}

	virtual void Exit(void) 
	{
	}

	virtual void Stop(void)
	{
	}

	virtual uint32 Run(void)
	{
		while(GRunRenderingThreadHeartbeat.Load(EMemoryOrder::Relaxed))
		{
			FPlatformProcess::Sleep(1.f/(4.0f * GRenderingThreadMaxIdleTickFrequency));
			if (!GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed) && OutstandingHeartbeats.GetValue() < 4)
			{
				OutstandingHeartbeats.Increment();
				ENQUEUE_RENDER_COMMAND(HeartbeatTickTickables)(
					[](FRHICommandList& RHICmdList)
					{
						OutstandingHeartbeats.Decrement();
						// make sure that rendering thread tickables get a chance to tick, even if the render thread is starving
						// but if GSuspendRenderingTickables is != 0 a flush is happening so don't tick during it
						if (!GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed) && !GSuspendRenderingTickables.Load(EMemoryOrder::Relaxed))
						{
							TickRenderingTickables();
						}
					});
			}
		}
		return 0;
	}
};

FRunnableThread* GRenderingThreadHeartbeat = NULL;
FRunnable* GRenderingThreadRunnableHeartbeat = NULL;

// not done in the CVar system as we don't access to render thread specifics there
struct FConsoleRenderThreadPropagation : public IConsoleThreadPropagation
{
	virtual void OnCVarChange(int32& Dest, int32 NewValue)
	{
		int32* DestPtr = &Dest;
		ENQUEUE_RENDER_COMMAND(OnCVarChange1)(
			[DestPtr, NewValue](FRHICommandListImmediate& RHICmdList)
			{
				*DestPtr = NewValue;
			});
	}
	
	virtual void OnCVarChange(float& Dest, float NewValue)
	{
		float* DestPtr = &Dest;
		ENQUEUE_RENDER_COMMAND(OnCVarChange2)(
			[DestPtr, NewValue](FRHICommandListImmediate& RHICmdList)
			{
				*DestPtr = NewValue;
			});
	}

	virtual void OnCVarChange(bool& Dest, bool NewValue)
	{
		bool* DestPtr = &Dest;
		ENQUEUE_RENDER_COMMAND(OnCVarChange2)(
			[DestPtr, NewValue](FRHICommandListImmediate& RHICmdList)
			{
				*DestPtr = NewValue;
			});
	}
	
	virtual void OnCVarChange(FString& Dest, const FString& NewValue)
	{
		FString* DestPtr = &Dest;
		ENQUEUE_RENDER_COMMAND(OnCVarChange3)(
			[DestPtr, NewValue](FRHICommandListImmediate& RHICmdList)
			{
				*DestPtr = NewValue;
			});
	}

	static FConsoleRenderThreadPropagation& GetSingleton()
	{
		static FConsoleRenderThreadPropagation This;

		return This;
	}

};

static FString BuildRenderingThreadName( uint32 ThreadIndex )
{
	return FString::Printf( TEXT( "%s %u" ), *FName( NAME_RenderThread ).GetPlainNameString(), ThreadIndex );
}



class FOwnershipOfRHIThreadTask : public FCustomStatIDGraphTaskBase
{
public:
	/**
	*	Constructor
	*	@param StatId The stat id for this task.
	*	@param InDesiredThread; Thread to run on, can be ENamedThreads::AnyThread
	**/
	FOwnershipOfRHIThreadTask(bool bInAcquireOwnership, TStatId StatId)
		: FCustomStatIDGraphTaskBase(StatId)
		, bAcquireOwnership(bInAcquireOwnership)
	{
	}

	/**
	*	Retrieve the thread that this task wants to run on.
	*	@return the thread that this task should run on.
	**/
	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::RHIThread;
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
		check(IsInRHIThread());
		if (bAcquireOwnership)
		{
			GDynamicRHI->RHIAcquireThreadOwnership();
		}
		else
		{
			GDynamicRHI->RHIReleaseThreadOwnership();
		}
	}

private:
	bool bAcquireOwnership;
};



void StartRenderingThread()
{
	static uint32 ThreadCount = 0;
	check(!GIsThreadedRendering && GUseThreadedRendering);

	check(!IsRHIThreadRunning() && !GIsRunningRHIInSeparateThread_InternalUseOnly && !GIsRunningRHIInDedicatedThread_InternalUseOnly && !GIsRunningRHIInTaskThread_InternalUseOnly);

	// Pause asset streaming to prevent rendercommands from being enqueued.
	SuspendTextureStreamingRenderTasks();

	// Flush GT since render commands issued by threads other than GT are sent to
	// the main queue of GT when RT is disabled. Without this flush, those commands
	// will run on GT after RT is enabled
	FlushRenderingCommands();

	if (GUseRHIThread_InternalUseOnly)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);		
		if (!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RHIThread))
		{
			FRHIThread::Get().Start();
		}
		DECLARE_CYCLE_STAT(TEXT("Wait For RHIThread"), STAT_WaitForRHIThread, STATGROUP_TaskGraphTasks);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GRHIThread_InternalUseOnly = FRHIThread::Get().Thread;
		GRHIThreadId = FRHIThread::Get().Thread->GetThreadID();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		GIsRunningRHIInDedicatedThread_InternalUseOnly = true;
		GIsRunningRHIInSeparateThread_InternalUseOnly = true;

		FGraphEventRef CompletionEvent = TGraphTask<FOwnershipOfRHIThreadTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(true, GET_STATID(STAT_WaitForRHIThread));
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StartRenderingThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionEvent, ENamedThreads::GameThread_Local);

		GRHICommandList.LatchBypass();
	}
	else if (GUseRHITaskThreads_InternalUseOnly)
	{
		GIsRunningRHIInSeparateThread_InternalUseOnly = true;
		GIsRunningRHIInTaskThread_InternalUseOnly = true;
	}

	// Turn on the threaded rendering flag.
	GIsThreadedRendering = true;

	// Create the rendering thread.
	GRenderingThreadRunnable = new FRenderingThread();

	UE::Trace::ThreadGroupBegin(TEXT("Render"));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GRenderingThread = 
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		FRunnableThread::Create(GRenderingThreadRunnable, *BuildRenderingThreadName(ThreadCount), 0, FPlatformAffinity::GetRenderingThreadPriority(), FPlatformAffinity::GetRenderingThreadMask(), FPlatformAffinity::GetRenderingThreadFlags());
	UE::Trace::ThreadGroupEnd();

	// Wait for render thread to have taskgraph bound before we dispatch any tasks for it.
	((FRenderingThread*)GRenderingThreadRunnable)->TaskGraphBoundSyncEvent->Wait();

	// register
	IConsoleManager::Get().RegisterThreadPropagation(0, &FConsoleRenderThreadPropagation::GetSingleton());

	// ensure the thread has actually started and is idling
	FRenderCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();

	GRunRenderingThreadHeartbeat = true;
	// Create the rendering thread heartbeat
	GRenderingThreadRunnableHeartbeat = new FRenderingThreadTickHeartbeat();

	UE::Trace::ThreadGroupBegin(TEXT("Render"));
	GRenderingThreadHeartbeat = FRunnableThread::Create(GRenderingThreadRunnableHeartbeat, *FString::Printf(TEXT("RTHeartBeat %d"), ThreadCount), 80 * 1024, TPri_AboveNormal, FPlatformAffinity::GetRTHeartBeatMask());
	UE::Trace::ThreadGroupEnd();

	ThreadCount++;

	// Update can now resume.
	ResumeTextureStreamingRenderTasks();
}

static FStopRenderingThread GStopRenderingThreadDelegate;

FDelegateHandle RegisterStopRenderingThreadDelegate(const FStopRenderingThread::FDelegate& InDelegate)
{
	return GStopRenderingThreadDelegate.Add(InDelegate);
}

void UnregisterStopRenderingThreadDelegate(FDelegateHandle InDelegateHandle)
{
	GStopRenderingThreadDelegate.Remove(InDelegateHandle);
}

void StopRenderingThread()
{
	// This function is not thread-safe. Ensure it is only called by the main game thread.
	check( IsInGameThread() );
	
	// unregister
	IConsoleManager::Get().RegisterThreadPropagation();

	// stop the render thread heartbeat first
	if (GRunRenderingThreadHeartbeat)
	{
		GRunRenderingThreadHeartbeat = false;
		// Wait for the rendering thread heartbeat to return.
		GRenderingThreadHeartbeat->WaitForCompletion();
		delete GRenderingThreadHeartbeat;
		GRenderingThreadHeartbeat = NULL;
		delete GRenderingThreadRunnableHeartbeat;
		GRenderingThreadRunnableHeartbeat = NULL;
	}

	if( GIsThreadedRendering )
	{
		GStopRenderingThreadDelegate.Broadcast();

		// Get the list of objects which need to be cleaned up when the rendering thread is done with them.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

		// Make sure we're not in the middle of streaming textures.
		SuspendTextureStreamingRenderTasks();

		// Wait for the rendering thread to finish executing all enqueued commands.
		FlushRenderingCommands();

		// The rendering thread may have already been stopped during the call to GFlushStreamingFunc or FlushRenderingCommands.
		if ( GIsThreadedRendering )
		{
			if (IsRHIThreadRunning())
			{
				DECLARE_CYCLE_STAT(TEXT("Wait For RHIThread Finish"), STAT_WaitForRHIThreadFinish, STATGROUP_TaskGraphTasks);
				FGraphEventRef ReleaseTask = TGraphTask<FOwnershipOfRHIThreadTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(false, GET_STATID(STAT_WaitForRHIThreadFinish));
				QUICK_SCOPE_CYCLE_COUNTER(STAT_StopRenderingThread_RHIThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReleaseTask, ENamedThreads::GameThread_Local);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				GRHIThread_InternalUseOnly = nullptr;
				GRHIThreadId = 0;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}

			GIsRunningRHIInSeparateThread_InternalUseOnly = false;
			GIsRunningRHIInDedicatedThread_InternalUseOnly = false;
			GIsRunningRHIInTaskThread_InternalUseOnly = false;


			check(!GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed));

			// Turn off the threaded rendering flag.
			GIsThreadedRendering = false;

			{
				FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::GetRenderThread());

				// Busy wait while BP debugging, to avoid opportunistic execution of game thread tasks
				// If the game thread is already executing tasks, then we have no choice but to spin
				if (GIntraFrameDebuggingGameThread || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread) ) 
				{
					while ((QuitTask.GetReference() != nullptr) && !QuitTask->IsComplete())
					{
						FPlatformProcess::Sleep(0.0f);
					}
				}
				else
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_StopRenderingThread);
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
				}
			}

			// Wait for the rendering thread to return.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			GRenderingThread->WaitForCompletion();

			// Destroy the rendering thread objects.
			delete GRenderingThread;

			GRenderingThread = NULL;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
			GRHICommandList.LatchBypass();

			delete GRenderingThreadRunnable;
			GRenderingThreadRunnable = NULL;
		}

		// Delete the pending cleanup objects which were in use by the rendering thread.
		delete PendingCleanupObjects;

		// Update can now resume with renderthread being the gamethread.
		ResumeTextureStreamingRenderTasks();
	}

	check(!IsRHIThreadRunning());
}

void CheckRenderingThreadHealth()
{
	if(!GIsRenderingThreadHealthy)
	{
		GErrorHist[0] = 0;
		GIsCriticalError = false;
		UE_LOG(LogRendererCore, Fatal,TEXT("Rendering thread exception:\r\n%s"),*GRenderingThreadError);
	}

	if (IsInGameThread())
	{
		if (!GIsCriticalError)
		{
			GLog->FlushThreadedLogs(EOutputDeviceRedirectorFlushOptions::Async);
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		TGuardValue<TAtomic<bool>, bool> GuardMainThreadBlockedOnRenderThread(GMainThreadBlockedOnRenderThread,true);
#endif
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
		FPlatformApplicationMisc::PumpMessages(false);
	}
}

bool IsRenderingThreadHealthy()
{
	return GIsRenderingThreadHealthy;
}

static struct FRenderCommandFenceBundlerState
{
	TOptional<UE::Tasks::FTaskEvent> Event;
	FRenderCommandPipeBitArray RenderCommandPipeBits;
	int32 RecursionDepth = 0;

} GRenderCommandFenceBundlerState; 

#define UE_RENDER_COMMAND_FENCE_BUNDLER_REGION TEXT("Render Command Fence Bundler")
#define UE_RENDER_COMMAND_PIPE_RECORD_REGION TEXT("Render Command Pipe Recording")
#define UE_RENDER_COMMAND_PIPE_SYNC_REGION TEXT("Render Command Pipe Synced")

#if UE_TRACE_ENABLED
#define UE_RENDER_COMMAND_BEGIN_REGION(Region) \
	if (RenderCommandsChannel) \
	{ \
		TRACE_BEGIN_REGION(Region) \
	}

#define UE_RENDER_COMMAND_END_REGION(Region) \
	if (RenderCommandsChannel) \
	{ \
		TRACE_END_REGION(Region) \
	}
#else
#define UE_RENDER_COMMAND_BEGIN_REGION(Region)
#define UE_RENDER_COMMAND_END_REGION(Region)
#endif

void StartRenderCommandFenceBundler()
{
	if (!GIsThreadedRendering)
	{
		return;
	}

	check(IsInGameThread());
	check(!GRenderCommandFenceBundlerState.Event.IsSet() == !GRenderCommandFenceBundlerState.RecursionDepth);

	++GRenderCommandFenceBundlerState.RecursionDepth;

	if (GRenderCommandFenceBundlerState.RecursionDepth > 1)
	{
		return;
	}

	GRenderCommandFenceBundlerState.Event.Emplace(TEXT("RenderCommandFenceBundlerEvent"));

	// Stop render command pipes so that the bundled render command fence is serialized with other render commands.
	GRenderCommandFenceBundlerState.RenderCommandPipeBits = UE::RenderCommandPipe::StopRecording();

	StartBatchedRelease();

	UE_RENDER_COMMAND_BEGIN_REGION(UE_RENDER_COMMAND_FENCE_BUNDLER_REGION);
}

void FlushRenderCommandFenceBundler()
{
	if (GRenderCommandFenceBundlerState.Event)
	{
		EndBatchedRelease();

		ENQUEUE_RENDER_COMMAND(InsertFence)(
			[CompletionEvent = MoveTemp(*GRenderCommandFenceBundlerState.Event)](FRHICommandListBase&) mutable
		{
			CompletionEvent.Trigger();
		});

		GRenderCommandFenceBundlerState.Event.Emplace(TEXT("RenderCommandFenceBundlerEvent"));

		StartBatchedRelease();
	}
}

void StopRenderCommandFenceBundler()
{
	if (!GIsThreadedRendering || !GRenderCommandFenceBundlerState.Event)
	{
		return;
	}

	TOptional<UE::Tasks::FTaskEvent>& CompletionEvent = GRenderCommandFenceBundlerState.Event;

	check(CompletionEvent);
	check(!CompletionEvent->IsCompleted());
	check(GRenderCommandFenceBundlerState.RecursionDepth > 0);

	--GRenderCommandFenceBundlerState.RecursionDepth;

	if (GRenderCommandFenceBundlerState.RecursionDepth > 0)
	{
		return;
	}

	UE_RENDER_COMMAND_END_REGION(UE_RENDER_COMMAND_FENCE_BUNDLER_REGION);

	EndBatchedRelease();

	ENQUEUE_RENDER_COMMAND(InsertFence)(
		[CompletionEvent = MoveTemp(*CompletionEvent)](FRHICommandListBase&) mutable
	{
		CompletionEvent.Trigger();
	});

	CompletionEvent.Reset();

	// Restart render command pipes that were previously recording.
	UE::RenderCommandPipe::StartRecording(GRenderCommandFenceBundlerState.RenderCommandPipeBits);
	GRenderCommandFenceBundlerState.RenderCommandPipeBits.Empty();
}

std::atomic<int> GTimeoutSuspendCount;

void SuspendRenderThreadTimeout()
{
	++GTimeoutSuspendCount;
}

void ResumeRenderThreadTimeout()
{
	--GTimeoutSuspendCount;

	check(GTimeoutSuspendCount >= 0);
}

bool IsRenderThreadTimeoutSuspended()
{
	return GTimeoutSuspendCount > 0;
}

TAutoConsoleVariable<int32> CVarGTSyncType(
	TEXT("r.GTSyncType"),
	0,
	TEXT("Determines how the game thread syncs with the render thread, RHI thread and GPU.\n")
	TEXT("Syncing to the GPU swap chain flip allows for lower frame latency.\n")
	TEXT(" 0 - Sync the game thread with the render thread (default).\n")
	TEXT(" 1 - Sync the game thread with the RHI thread.\n")
	TEXT(" 2 - Sync the game thread with the GPU swap chain flip (only on supported platforms).\n"),
	ECVF_Default);

FRHICOMMAND_MACRO(FRHISyncFrameCommand)
{
	UE::Tasks::FTaskEvent TaskEvent;
	int32 GTSyncType;

	FORCEINLINE_DEBUGGABLE FRHISyncFrameCommand(UE::Tasks::FTaskEvent InTaskEvent, int32 InGTSyncType)
		: TaskEvent(MoveTemp(InTaskEvent))
		, GTSyncType(InGTSyncType)
	{}

	void Execute(FRHICommandListBase& CmdList)
	{
		if (GTSyncType == 1)
		{
			// Sync the Game Thread with the RHI Thread

			// "Complete" the graph event
			TaskEvent.Trigger();
		}
		else
		{
			// This command runs *after* a present has happened, so the counter has already been incremented.
			// Subtracting 1 gives us the index of the frame that has *just* been presented.
			RHITriggerTaskEventOnFlip(GRHIPresentCounter - 1, TaskEvent);
		}
	}
};

FRenderCommandFence::FRenderCommandFence() = default;
FRenderCommandFence::~FRenderCommandFence() = default;

void FRenderCommandFence::BeginFence(bool bSyncToRHIAndGPU)
{
	if (!GIsThreadedRendering)
	{
		return;
	}
	
	if (GRenderCommandFenceBundlerState.Event && IsInGameThread())
	{
		CompletionTask = *GRenderCommandFenceBundlerState.Event;
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FRenderCommandFence::BeginFence);

	struct FRenderCommandPipeFence : public TConcurrentLinearObject<FRenderCommandPipeFence>
	{
		FRenderCommandPipeFence(int32 InNumRefs)
			: NumRefs(InNumRefs)
		{}

		void Trigger(int32 NumTriggerRefs = 1)
		{
			if (NumRefs.fetch_sub(NumTriggerRefs, std::memory_order_release) == 1)
			{
				std::atomic_thread_fence(std::memory_order_acquire);
				CompletionTaskEvent.Trigger();
				delete this;
			}
		}

		UE::Tasks::FTaskEvent CompletionTaskEvent{ UE_SOURCE_LOCATION };
		std::atomic_int32_t NumRefs;
	};

	TConstArrayView<FRenderCommandPipe*> Pipes = GRenderCommandPipeMode == ERenderCommandPipeMode::All
		? UE::RenderCommandPipe::GetPipes()
		: TConstArrayView<FRenderCommandPipe*>{};

	FRenderCommandPipeBitArray ActivePipeBits;
	int32 NumActivePipes = 0;

	for (FRenderCommandPipe* Pipe : Pipes)
	{
		// Skip pipes that aren't recording or replaying any work.
		const bool bIsActive = Pipe->IsRecording() && !Pipe->IsEmpty();
		ActivePipeBits.Add(bIsActive);
		NumActivePipes += bIsActive ? 1 : 0;
	}

	FRenderCommandPipeFence* Fence = nullptr;

	if (NumActivePipes > 0)
	{
		Fence = new FRenderCommandPipeFence(NumActivePipes + 1);

		for (FRenderCommandPipeSetBitIterator BitIt(ActivePipeBits); BitIt; ++BitIt)
		{
			FRenderCommandPipe* Pipe = Pipes[BitIt.GetIndex()];

			ENQUEUE_RENDER_COMMAND(BeginFence)(Pipe, [Fence]
			{
				Fence->Trigger();
			});
		}
	}

	const int32 GTSyncType = CVarGTSyncType.GetValueOnAnyThread();

	if (bSyncToRHIAndGPU)
	{
		// Don't sync to the RHI and GPU if GtSyncType is disabled, or we're not vsyncing
		//@TODO: do this logic in the caller?
		static auto CVarVsync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		check(CVarVsync != nullptr);

		if (GTSyncType == 0 || CVarVsync->GetInt() == 0)
		{
			bSyncToRHIAndGPU = false;
		}
	}

	if (bSyncToRHIAndGPU)
	{
		UE::Tasks::FTaskEvent CompletionTaskEvent{ UE_SOURCE_LOCATION };

		if (Fence)
		{
			// RHI frame sync command requires a task event, so connect it to the ref-counted fence event.
			CompletionTaskEvent.AddPrerequisites(Fence->CompletionTaskEvent);
			Fence->Trigger();
		}

		ENQUEUE_RENDER_COMMAND(FSyncFrameCommand)(
			[CompletionTaskEvent, GTSyncType, bSyncToRHIAndGPU](FRHICommandListImmediate& RHICmdList) mutable
		{
			if (IsRHIThreadRunning())
			{
				ALLOC_COMMAND_CL(RHICmdList, FRHISyncFrameCommand)(MoveTemp(CompletionTaskEvent), GTSyncType);
				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			}
			else
			{
				FRHISyncFrameCommand Command(MoveTemp(CompletionTaskEvent), GTSyncType);
				Command.Execute(RHICmdList);
			}
		});

		CompletionTask = MoveTemp(CompletionTaskEvent);
	}
	else if (Fence)
	{
		CompletionTask = Fence->CompletionTaskEvent;

		ENQUEUE_RENDER_COMMAND(BeginFence)([Fence](FRHICommandListBase& RHICmdList)
		{
			Fence->Trigger();
		});
	}
	else
	{
		UE::Tasks::FTaskEvent CompletionTaskEvent{ UE_SOURCE_LOCATION };

		ENQUEUE_RENDER_COMMAND(BeginFence)([CompletionTaskEvent](FRHICommandListBase& RHICmdList) mutable
		{
			CompletionTaskEvent.Trigger();
		});

		CompletionTask = MoveTemp(CompletionTaskEvent);
	}
} //-V773

bool FRenderCommandFence::IsFenceComplete() const
{
	if (!GIsThreadedRendering)
	{
		return true;
	}
	check(IsInGameThread() || IsInAsyncLoadingThread());
	CheckRenderingThreadHealth();
	if (CompletionTask.IsCompleted())
	{
		CompletionTask = {}; // this frees the handle for other uses, the NULL state is considered completed
		return true;
	}
	return false;
}

/** How many cycles the gamethread used (excluding idle time). It's set once per frame in FViewport::Draw. */
uint32 GGameThreadTime = 0;

/** How much idle time on the game thread. It's set once per frame in FViewport::Draw. */
uint32 GGameThreadWaitTime = 0;

/** How many cycles it took to swap buffers to present the frame. */
uint32 GSwapBufferTime = 0;

static int32 GTimeToBlockOnRenderFence = 1;
static FAutoConsoleVariableRef CVarTimeToBlockOnRenderFence(
	TEXT("g.TimeToBlockOnRenderFence"),
	GTimeToBlockOnRenderFence,
	TEXT("Number of milliseconds the game thread should block when waiting on a render thread fence.")
	);


static int32 GTimeoutForBlockOnRenderFence = 120000;
static FAutoConsoleVariableRef CVarTimeoutForBlockOnRenderFence(
	TEXT("g.TimeoutForBlockOnRenderFence"),
	GTimeoutForBlockOnRenderFence,
	TEXT("Number of milliseconds the game thread should wait before failing when waiting on a render thread fence.")
);

/**
 * Block the game thread waiting for a task to finish on the rendering thread.
 */
static void GameThreadWaitForTask(const UE::Tasks::FTask& Task, bool bEmptyGameThreadTasks = false)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GameThreadWaitForTask);
	SCOPE_TIME_GUARD(TEXT("GameThreadWaitForTask"));

	check(IsInGameThread());
	check(Task.IsValid());

	if (!Task.IsCompleted())
	{
		SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		{
			static int32 NumRecursiveCalls = 0;
		
			// Check for recursion. It's not completely safe but because we pump messages while 
			// blocked it is expected.
			NumRecursiveCalls++;
			if (NumRecursiveCalls > 1)
			{
				UE_LOG(LogRendererCore,Warning,TEXT("FlushRenderingCommands called recursively! %d calls on the stack."), NumRecursiveCalls);
			}
			if (NumRecursiveCalls > 1 || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread))
			{
				bEmptyGameThreadTasks = false; // we don't do this on recursive calls or if we are at a blueprint breakpoint
			}

			// Check rendering thread health needs to be called from time to
			// time in order to pump messages, otherwise the RHI may block
			// on vsync causing a deadlock. Also we should make sure the
			// rendering thread hasn't crashed :)
			bool bDone;
			uint32 WaitTime = FMath::Clamp<uint32>(GTimeToBlockOnRenderFence, 0, 33);

			// Use a clamped clock to prevent taking into account time spent suspended.
			FThreadHeartBeatClock RenderThreadTimeoutClock((4 * WaitTime) / 1000.0);
			const double StartTime = RenderThreadTimeoutClock.Seconds();
			const double EndTime = StartTime + (GTimeoutForBlockOnRenderFence / 1000.0);

			bool bRenderThreadEnsured = FDebug::IsEnsuring();

			static bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("nothreadtimeout"));

			// Creating the wait task manually is a workaround for the problem of FTast::Wait creating
			// a separate wait task and event object on each call. It's a problem because we may call
			// Wait it in the loop below many times during long frame syncs (e.g. when using GPU profilers)
			// which would create thousands of such objects and run out of system resources.
			FSharedEventRef CompletionEvent;

			UE::Tasks::Launch(
				TEXT("Waiting Task (FrameSync)"),
				[CompletionEvent] { CompletionEvent->Trigger(); },
				Task,
				LowLevelTasks::ETaskPriority::Default,
				UE::Tasks::EExtendedTaskPriority::Inline,
				UE::Tasks::ETaskFlags::None
			);

			do
			{
				CheckRenderingThreadHealth();
				if (bEmptyGameThreadTasks)
				{
					// process gamethread tasks if there are any
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
				}
				bDone = CompletionEvent->Wait(FTimespan::FromMilliseconds(WaitTime));

				RenderThreadTimeoutClock.Tick();

				bool IsGpuAlive = true;
				const bool bOverdue = RenderThreadTimeoutClock.Seconds() >= EndTime && FThreadHeartBeat::Get().IsBeating();

				if (bOverdue)
				{
					if (GDynamicRHI)
					{
						IsGpuAlive = GDynamicRHI->CheckGpuHeartbeat();
					}
				}

				if (!IsGpuAlive)
				{
					UE_LOG(LogRendererCore, Fatal, TEXT("GPU has hung or crashed!"));
				}

				// track whether the thread ensured, if so don't do timeout checks
				bRenderThreadEnsured |= FDebug::IsEnsuring();

#if !WITH_EDITOR
#if !PLATFORM_IOS && !PLATFORM_MAC // @todo MetalMRT: Timeout isn't long enough...
				// editor threads can block for quite a while... 
				if (!bDone && !bRenderThreadEnsured)
				{
					if (bOverdue && !bDisabled && !IsRenderThreadTimeoutSuspended() && !FPlatformMisc::IsDebuggerPresent())
					{
						UE_LOG(LogRendererCore, Fatal, TEXT("GameThread timed out waiting for RenderThread after %.02f secs"), RenderThreadTimeoutClock.Seconds() - StartTime);
					}
				}
#endif
#endif
			}
			while (!bDone);

			NumRecursiveCalls--;
		}
	}
}

/**
 * Waits for pending fence commands to retire.
 */
void FRenderCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	if (!IsFenceComplete())
	{
		FlushRenderCommandFenceBundler();
		GameThreadWaitForTask(CompletionTask, bProcessGameThreadTasks);
		CompletionTask = {}; // release the internal memory as soon as it's not needed anymore
	}
}

/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
void FlushRenderingCommands()
{
	if (!GIsRHIInitialized)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FlushRenderingCommands);
	FCoreRenderDelegates::OnFlushRenderingCommandsStart.Broadcast();
	FSuspendRenderingTickables SuspendRenderingTickables;

	// Need to flush GT because render commands from threads other than GT are sent to
	// the main queue of GT when RT is disabled
	if (!GIsThreadedRendering
		&& !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread)
		&& !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread_Local))
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
	}

	UE::RenderCommandPipe::StopRecording();

	ENQUEUE_RENDER_COMMAND(FlushPendingDeleteRHIResourcesCmd)([](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		//double flush to flush out the deferred deletions queued into the ImmediateCmdList
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	});

	// Find the objects which may be cleaned up once the rendering thread command queue has been flushed.
	FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

	// Issue a fence command to the rendering thread and wait for it to complete.
	FRenderCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();

	// Delete the objects which were enqueued for deferred cleanup before the command queue flush.
	delete PendingCleanupObjects;

	FCoreRenderDelegates::OnFlushRenderingCommandsEnd.Broadcast();
}

void FlushPendingDeleteRHIResources_GameThread()
{
	if (!IsRunningRHIInSeparateThread())
	{
		ENQUEUE_RENDER_COMMAND(FlushPendingDeleteRHIResources)(
			[](FRHICommandList& RHICmdList)
			{
				FlushPendingDeleteRHIResources_RenderThread();
			});
	}
}

void FlushPendingDeleteRHIResources_RenderThread()
{
	if (!IsRunningRHIInSeparateThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	}
}

FRHICommandListImmediate& GetImmediateCommandList_ForRenderCommand()
{
	return FRHICommandListExecutor::GetImmediateCommandList();
}

static bool bEnablePendingCleanupObjectsCommandBatching = true;
static FAutoConsoleVariableRef CVarEnablePendingCleanupObjectsCommandBatching(
	TEXT("g.bEnablePendingCleanupObjectsCommandBatching"),
	bEnablePendingCleanupObjectsCommandBatching,
	TEXT("Enable batching PendingCleanupObjects destruction.")
);

#if WITH_EDITOR || IS_PROGRAM

// mainly concerned about the cooker here, but anyway, the editor can run without a frame for a very long time (hours) and we do not have enough lock free links. 

/** The set of deferred cleanup objects which are pending cleanup. */
TArray<FDeferredCleanupInterface*> PendingCleanupObjectsList;
FCriticalSection PendingCleanupObjectsListLock;

FPendingCleanupObjects::FPendingCleanupObjects()
{
	check(IsInGameThread());
	{
		FScopeLock Lock(&PendingCleanupObjectsListLock);
		Exchange(CleanupArray, PendingCleanupObjectsList);
	}
}

FPendingCleanupObjects::~FPendingCleanupObjects()
{
	if (CleanupArray.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPendingCleanupObjects_Destruct);

		const bool bBatchingEnabled = bEnablePendingCleanupObjectsCommandBatching;
		if (bBatchingEnabled)
		{
			StartRenderCommandFenceBundler();
		}
		for (int32 ObjectIndex = 0; ObjectIndex < CleanupArray.Num(); ObjectIndex++)
		{
			delete CleanupArray[ObjectIndex];
		}
		if (bBatchingEnabled)
		{
			StopRenderCommandFenceBundler();
		}
	}
}

void BeginCleanup(FDeferredCleanupInterface* CleanupObject)
{
	{
		FScopeLock Lock(&PendingCleanupObjectsListLock);
		PendingCleanupObjectsList.Add(CleanupObject);
	}
}

#else

/** The set of deferred cleanup objects which are pending cleanup. */
static TLockFreePointerListUnordered<FDeferredCleanupInterface, PLATFORM_CACHE_LINE_SIZE>	PendingCleanupObjectsList;

FPendingCleanupObjects::FPendingCleanupObjects()
{
	check(IsInGameThread());
	PendingCleanupObjectsList.PopAll(CleanupArray);
}

FPendingCleanupObjects::~FPendingCleanupObjects()
{
	if (CleanupArray.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPendingCleanupObjects_Destruct);

		const bool bBatchingEnabled = bEnablePendingCleanupObjectsCommandBatching;
		if (bBatchingEnabled)
		{
			StartRenderCommandFenceBundler();
		}
		for (int32 ObjectIndex = 0; ObjectIndex < CleanupArray.Num(); ObjectIndex++)
		{
			delete CleanupArray[ObjectIndex];
		}
		if (bBatchingEnabled)
		{
			StopRenderCommandFenceBundler();
		}
	}
}

void BeginCleanup(FDeferredCleanupInterface* CleanupObject)
{
	PendingCleanupObjectsList.Push(CleanupObject);
}

#endif

FPendingCleanupObjects* GetPendingCleanupObjects()
{
	return new FPendingCleanupObjects;
}

void SetRHIThreadEnabled(bool bEnableDedicatedThread, bool bEnableRHIOnTaskThreads)
{
	if (bEnableDedicatedThread != GUseRHIThread_InternalUseOnly || bEnableRHIOnTaskThreads != GUseRHITaskThreads_InternalUseOnly)
	{
		if ((bEnableRHIOnTaskThreads || bEnableDedicatedThread) && !GIsThreadedRendering)
		{
			check(!IsRunningRHIInSeparateThread());
			UE_LOG(LogConsoleResponse, Display, TEXT("Can't switch to RHI thread mode when we are not running a multithreaded renderer."));
		}
		else
		{
			StopRenderingThread();
			if (bEnableRHIOnTaskThreads)
			{
				GUseRHIThread_InternalUseOnly = false;
				GUseRHITaskThreads_InternalUseOnly = true;
			}
			else if (bEnableDedicatedThread)
			{
				GUseRHIThread_InternalUseOnly = true;
				GUseRHITaskThreads_InternalUseOnly = false;
			}
			else
			{
				GUseRHIThread_InternalUseOnly = false;
				GUseRHITaskThreads_InternalUseOnly = false;
			}
			StartRenderingThread();
		}
	}
	if (IsRunningRHIInSeparateThread())
	{
		if (IsRunningRHIInDedicatedThread())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("RHIThread is now running on a dedicated thread."));
		}
		else
		{
			check(IsRunningRHIInTaskThread());
			UE_LOG(LogConsoleResponse, Display, TEXT("RHIThread is now running on task threads."));
		}
	}
	else
	{
		check(!IsRunningRHIInTaskThread() && !IsRunningRHIInDedicatedThread());
		UE_LOG(LogConsoleResponse, Display, TEXT("RHIThread is disabled."));
	}

}

static void HandleRHIThreadEnableChanged(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		const int32 UseRHIThread = FCString::Atoi(*Args[0]);
		SetRHIThreadEnabled(UseRHIThread == 1, UseRHIThread == 2);
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Usage: r.RHIThread.Enable 0=off,  1=dedicated thread,  2=task threads; Currently %d"), IsRunningRHIInSeparateThread() ? (IsRunningRHIInDedicatedThread() ? 1 : 2) : 0);
	}
}

static FAutoConsoleCommand CVarRHIThreadEnable(
	TEXT("r.RHIThread.Enable"),
	TEXT("Enables/disabled the RHI Thread and determine if the RHI work runs on a dedicated thread or not.\n"),	
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleRHIThreadEnableChanged)
	);

inline ERenderCommandPipeMode GetValidatedRenderCommandPipeMode(int32 CVarValue)
{
	ERenderCommandPipeMode Mode = ERenderCommandPipeMode::None;

	switch (CVarValue)
	{
	case 1:
		Mode = ERenderCommandPipeMode::RenderThread;
		break;
	case 2:
		Mode = ERenderCommandPipeMode::All;
		break;
	}

	const bool bAllowThreading = !GRHICommandList.Bypass() && FApp::ShouldUseThreadingForPerformance() && GIsThreadedRendering;

	if (Mode == ERenderCommandPipeMode::All && !bAllowThreading)
	{
		Mode = ERenderCommandPipeMode::RenderThread;
	}

	if (!FApp::CanEverRender() || IsMobilePlatform(GMaxRHIShaderPlatform))
	{
		Mode = ERenderCommandPipeMode::None;
	}

	return Mode;
}

ERenderCommandPipeMode GRenderCommandPipeMode = ERenderCommandPipeMode::None;
FAutoConsoleVariable CVarRenderCommandPipeMode(
	TEXT("r.RenderCommandPipeMode"),
	2,
	TEXT("Controls behavior of the main render thread command pipe.")
	TEXT(" 0: Render commands are launched individually as tasks;\n")
	TEXT(" 1: Render commands are enqueued into a render command pipe for the render thread only.;\n")
	TEXT(" 2: Render commands are enqueued into a render command pipe for all declared pipes.;\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
	{
		UE::RenderCommandPipe::StopRecording();
		GRenderCommandPipeMode = GetValidatedRenderCommandPipeMode(Variable->GetInt());
	}));

FRenderThreadCommandPipe FRenderThreadCommandPipe::Instance;

void FRenderThreadCommandPipe::EnqueueAndLaunch(const TCHAR* Name, uint32& SpecId, TStatId StatId, TUniqueFunction<void(FRHICommandListImmediate&)>&& Function)
{
	Mutex.Lock();
	bool bWasEmpty = Queues[ProduceIndex].IsEmpty();
	Queues[ProduceIndex].Emplace(Name, SpecId, StatId, MoveTemp(Function));
	Mutex.Unlock();

	if (bWasEmpty)
	{
		TGraphTask<TFunctionGraphTaskImpl<void(), ESubsequentsMode::FireAndForget>>::CreateTask().ConstructAndDispatchWhenReady([this]
		{
			FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

			Mutex.Lock();
			TArray<FCommand>& ConsumeCommands = Queues[ProduceIndex];
			ProduceIndex ^= 1;
			Mutex.Unlock();

			for (FCommand& Command : ConsumeCommands)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE_USE_ON_CHANNEL(*Command.SpecId, Command.Name, EventScope, RenderCommandsChannel, true);
				FScopeCycleCounter Scope(Command.StatId, true);
				Command.Function(RHICmdList);

				// Release the command immediately to match destruction order with task version.
				Command.Function = {};
			}
			ConsumeCommands.Reset();

		}, TStatId(), ENamedThreads::GetRenderThread());
	}
}

class FRenderCommandPipeRegistry
{
public:
	static TLinkedList<FRenderCommandPipe*>*& GetGlobalList()
	{
		static TLinkedList<FRenderCommandPipe*>* GlobalList = nullptr;
		return GlobalList;
	}

	void Initialize()
	{
		AllPipes.Reset();

		for (TLinkedList<FRenderCommandPipe*>::TIterator PipeIt(GetGlobalList()); PipeIt; PipeIt.Next())
		{
			FRenderCommandPipe* Pipe = *PipeIt;
			Pipe->SetEnabled(Pipe->ConsoleVariable->GetBool());
			Pipe->Index = AllPipes.Num();

			AllPipes.Emplace(*PipeIt);
		}

		GRenderCommandPipeMode = GetValidatedRenderCommandPipeMode(CVarRenderCommandPipeMode->GetInt());
	}

	void StartRecording()
	{
		if (GRenderCommandPipeMode != ERenderCommandPipeMode::All || !GIsThreadedRendering)
		{
			return;
		}

		FRenderCommandPipeBitArray PipeBits;
		PipeBits.Init(true, AllPipes.Num());
		StartRecording(PipeBits);
	}

	void StartRecording(const FRenderCommandPipeBitArray& PipeBits)
	{
		if (GRenderCommandPipeMode != ERenderCommandPipeMode::All || !GIsThreadedRendering || PipeBits.IsEmpty())
		{
			return;
		}

		SCOPED_NAMED_EVENT(FRenderCommandPipe_StartRecording, FColor::Magenta);

		check(PipeBits.Num() == AllPipes.Num());

		UE::TScopeLock Lock(Mutex);

		bool bAnyPipesToStartRecording = false;

		for (FRenderCommandPipeSetBitIterator BitIt(PipeBits); BitIt; ++BitIt)
		{
			FRenderCommandPipe* Pipe = AllPipes[BitIt.GetIndex()];

			if (Pipe->bEnabled && !Pipe->bRecording)
			{
				bAnyPipesToStartRecording = true;
				break;
			}
		}

		if (!bAnyPipesToStartRecording)
		{
			return;
		}

		UE_RENDER_COMMAND_BEGIN_REGION(UE_RENDER_COMMAND_PIPE_RECORD_REGION);

		UE::Tasks::FTaskEvent TaskEvent{ UE_SOURCE_LOCATION };

		struct FPipeToStartRecording
		{
			FPipeToStartRecording(FRenderCommandPipe* InPipe, FRenderCommandPipe::FFrame* InFrame)
				: Pipe(InPipe)
				, Frame(InFrame)
			{}

			FRenderCommandPipe* Pipe;
			FRenderCommandPipe::FFrame* Frame;
		};

		TArray<FPipeToStartRecording, FConcurrentLinearArrayAllocator> PipesToStartRecording;
		PipesToStartRecording.Reserve(AllPipes.Num());

		for (FRenderCommandPipeSetBitIterator BitIt(PipeBits); BitIt; ++BitIt)
		{
			FRenderCommandPipe* Pipe = AllPipes[BitIt.GetIndex()];

			if (Pipe->bEnabled && !Pipe->bRecording)
			{
				Pipe->bRecording = true;

				FRenderCommandPipe::FFrame* NextFrame = new FRenderCommandPipe::FFrame(Pipe->Name, TaskEvent);
				PipesToStartRecording.Emplace(Pipe, NextFrame);

				UE::TScopeLock PipeLock(Pipe->Mutex);
				Pipe->Frame_GameThread = NextFrame;
			}
		}

		NumPipesRecording += PipesToStartRecording.Num();

		ENQUEUE_RENDER_COMMAND(RenderCommandPipe_Start)([this, TaskEvent, PipesToStartRecording = MoveTemp(PipesToStartRecording)](FRHICommandListImmediate&) mutable
		{
			RHIResourceLifetimeAddRef(PipesToStartRecording.Num());

			for (FPipeToStartRecording Pipe : PipesToStartRecording)
			{
				Pipe.Pipe->Frame_RenderThread = Pipe.Frame;
			}

			NumPipesReplaying += PipesToStartRecording.Num();
			TaskEvent.Trigger();
		});
	}

	FRenderCommandPipeBitArray StopRecording()
	{
		UE::TScopeLock Lock(Mutex);
		if (!NumPipesRecording)
		{
			return {};
		}

		FRenderCommandPipeBitArray PipeBits;
		PipeBits.Init(false, AllPipes.Num());

		for (int32 PipeIndex = 0; PipeIndex < AllPipes.Num(); ++PipeIndex)
		{
			if (FRenderCommandPipe* Pipe = AllPipes[PipeIndex]; Pipe->bRecording)
			{
				PipeBits[PipeIndex] = true;
			}
		}

		StopRecording(PipeBits);
		return PipeBits;
	}

	FRenderCommandPipeBitArray StopRecording(TConstArrayView<FRenderCommandPipe*> Pipes)
	{
		if (Pipes.IsEmpty())
		{
			return {};
		}

		UE::TScopeLock Lock(Mutex);
		if (!NumPipesRecording)
		{
			return {};
		}

		bool bAnyPipesToStopRecording = false;
		FRenderCommandPipeBitArray PipeBits;
		PipeBits.Init(false, AllPipes.Num());

		for (FRenderCommandPipe* Pipe : Pipes)
		{
			if (Pipe->bRecording)
			{
				PipeBits[Pipe->Index] = true;
				bAnyPipesToStopRecording = true;
			}
		}

		if (!bAnyPipesToStopRecording)
		{
			return {};
		}

		StopRecording(PipeBits);
		return PipeBits;
	}

	TConstArrayView<FRenderCommandPipe*> GetPipes() const
	{
		return AllPipes;
	}

	bool IsRecording() const
	{
		ensureMsgf(!FTaskTagScope::IsCurrentTag(ETaskTag::EParallelRenderingThread) && !FTaskTagScope::IsCurrentTag(ETaskTag::ERenderingThread),
			TEXT("IsRecording() is not valid from the render thread timeline."));

		return NumPipesRecording > 0;
	}

	bool IsReplaying() const
	{
		ensure(IsInParallelRenderingThread());
		return NumPipesReplaying > 0;
	}

private:
	void StopRecording(const FRenderCommandPipeBitArray& PipeBits)
	{
		SCOPED_NAMED_EVENT(FRenderCommandPipe_StopRecording, FColor::Magenta);

		uint32 NumPipesToStopRecording = 0;

		for (FRenderCommandPipeSetBitIterator BitIt(PipeBits); BitIt; ++BitIt)
		{
			FRenderCommandPipe* Pipe = AllPipes[BitIt.GetIndex()];
			check(Pipe->bRecording);
			Pipe->bRecording = false;
			NumPipesToStopRecording++;

			Pipe->Mutex.Lock();
			Pipe->Frame_GameThread = nullptr;
		}

		NumPipesRecording -= NumPipesToStopRecording;

		ENQUEUE_RENDER_COMMAND(RenderCommandPipe_Stop)([this, PipeBits, NumPipesToStopRecording](FRHICommandListImmediate& RHICmdList)
		{
			TArray<FRHICommandListImmediate::FQueuedCommandList, FConcurrentLinearArrayAllocator> QueuedCommandLists;
			QueuedCommandLists.Reserve(NumPipesToStopRecording);

			for (FRenderCommandPipeSetBitIterator BitIt(PipeBits); BitIt; ++BitIt)
			{
				FRenderCommandPipe* Pipe = AllPipes[BitIt.GetIndex()];
				FRenderCommandPipe::FFrame*& Frame_RenderThread = Pipe->Frame_RenderThread;
				check(Frame_RenderThread);
				Frame_RenderThread->Pipe.WaitUntilEmpty();

				if (Frame_RenderThread->RHICmdList)
				{
					Frame_RenderThread->RHICmdList->FinishRecording();
					QueuedCommandLists.Emplace(Frame_RenderThread->RHICmdList);
				}

				delete Frame_RenderThread;
				Frame_RenderThread = nullptr;
			}

			NumPipesReplaying -= NumPipesToStopRecording;

			RHICmdList.QueueAsyncCommandListSubmit(QueuedCommandLists);
			RHIResourceLifetimeReleaseRef(RHICmdList, NumPipesToStopRecording);
		});

		// Wait to unlock the mutex until the sync command has been submitted to the render thread. This avoids
		// race conditions where a command meant for a specific pipe might be inserted to the render thread pipe
		// prior to the actual wait command.
		for (FRenderCommandPipeSetBitIterator BitIt(PipeBits); BitIt; ++BitIt)
		{
			AllPipes[BitIt.GetIndex()]->Mutex.Unlock();
		}

		UE_RENDER_COMMAND_END_REGION(UE_RENDER_COMMAND_PIPE_RECORD_REGION);
	}

	UE::FMutex Mutex;
	TArray<FRenderCommandPipe*> AllPipes;
	uint32 NumPipesRecording = 0;
	uint32 NumPipesReplaying = 0;
};

static FRenderCommandPipeRegistry GRenderCommandPipeRegistry;

inline bool HasBitsSet(const FRenderCommandPipeBitArray& Bits)
{
	for (FRenderCommandPipeBitArray::FConstWordIterator It(Bits); It; ++It)
	{
		if (It.GetWord() != 0)
		{
			return true;
		}
	}
	return false;
}

namespace UE::RenderCommandPipe
{
	static thread_local FRenderCommandPipe* ReplayingPipe = nullptr;

	void Initialize()
	{
		GRenderCommandPipeRegistry.Initialize();
	}

	bool IsRecording()
	{
		return GRenderCommandPipeRegistry.IsRecording();
	}

	bool IsReplaying()
	{
		return GRenderCommandPipeRegistry.IsReplaying();
	}

	bool IsReplaying(const FRenderCommandPipe& Pipe)
	{
		return ReplayingPipe == &Pipe;
	}

	void StartRecording()
	{
		GRenderCommandPipeRegistry.StartRecording();
	}

	void StartRecording(const FRenderCommandPipeBitArray& PipeBits)
	{
		GRenderCommandPipeRegistry.StartRecording(PipeBits);
	}

	FRenderCommandPipeBitArray StopRecording()
	{
		return GRenderCommandPipeRegistry.StopRecording();
	}

	FRenderCommandPipeBitArray StopRecording(TConstArrayView<FRenderCommandPipe*> Pipes)
	{
		return GRenderCommandPipeRegistry.StopRecording(Pipes);
	}

	TConstArrayView<FRenderCommandPipe*> GetPipes()
	{
		return GRenderCommandPipeRegistry.GetPipes();
	}

	FSyncScope::FSyncScope()
	{
		PipeBits = StopRecording();

#if UE_TRACE_ENABLED
		if (HasBitsSet(PipeBits))
		{
			UE_RENDER_COMMAND_BEGIN_REGION(UE_RENDER_COMMAND_PIPE_SYNC_REGION);
		}
#endif
	}

	FSyncScope::FSyncScope(TConstArrayView<FRenderCommandPipe*> Pipes)
	{
		PipeBits = StopRecording(Pipes);

#if UE_TRACE_ENABLED
		if (HasBitsSet(PipeBits))
		{
			UE_RENDER_COMMAND_BEGIN_REGION(UE_RENDER_COMMAND_PIPE_SYNC_REGION);
		}
#endif
	}

	FSyncScope::~FSyncScope()
	{
#if UE_TRACE_ENABLED
		if (HasBitsSet(PipeBits))
		{
			UE_RENDER_COMMAND_END_REGION(UE_RENDER_COMMAND_PIPE_SYNC_REGION);
		}
#endif

		StartRecording(PipeBits);
	}
}

FRenderCommandPipe::FRenderCommandPipe(const TCHAR* InName, ERenderCommandPipeFlags Flags, const TCHAR* CVarName, const TCHAR* CVarDescription)
	: Name(InName)
	, GlobalListLink(this)
	, ConsoleVariable(CVarName, !EnumHasAnyFlags(Flags, ERenderCommandPipeFlags::Disabled), CVarDescription, FConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* Variable)
	{
		SetEnabled(Variable->GetBool());
	}))
{
#if !UE_SERVER
	GlobalListLink.LinkHead(FRenderCommandPipeRegistry::GetGlobalList());
#endif
}

FRenderCommandPipe::~FRenderCommandPipe()
{
	delete Frame_GameThread;
	Frame_GameThread = nullptr;
	delete Frame_RenderThread;
	Frame_RenderThread = nullptr;
}

void FRenderCommandPipe::ExecuteCommand(FFunctionVariant&& FunctionVariant, const TCHAR* CommandName, uint32& CommandSpecId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_USE_ON_CHANNEL(CommandSpecId, CommandName, CommandEventScope, RenderCommandsChannel, true);
	if (FCommandListFunction* Function = FunctionVariant.TryGet<FCommandListFunction>())
	{
		if (!Frame_RenderThread->RHICmdList)
		{
			FRHICommandList* RHICmdList = new FRHICommandList(FRHIGPUMask::All());
			RHICmdList->SwitchPipeline(ERHIPipeline::Graphics);
			Frame_RenderThread->RHICmdList = RHICmdList;
		}

		(*Function)(*Frame_RenderThread->RHICmdList);
	}
	else
	{
		FunctionVariant.Get<FEmptyFunction>()();
	}
}

void FRenderCommandPipe::EnqueueAndLaunch(FFunctionVariant&& FunctionVariant, const TCHAR* CommandName, uint32& CommandSpecId)
{
	ensureMsgf(!UE::RenderCommandPipe::ReplayingPipe, TEXT("Attempting to launch render command to render command pipe %s from another pipe %s"), Name, UE::RenderCommandPipe::ReplayingPipe->Name);

	bool bWasEmpty = Frame_GameThread->Queue.IsEmpty();
	Frame_GameThread->Queue.Emplace(MoveTemp(FunctionVariant), CommandName, CommandSpecId);
	NumInFlightCommands.fetch_add(1, std::memory_order_relaxed);
	
	if (bWasEmpty)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR("RenderCommandPipe LaunchTask", RenderCommandsChannel)
	
		Frame_GameThread->Pipe.Launch(Name, [this]
		{
			check(Frame_RenderThread);
			TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR("RenderCommandPipe ReplayCommands", RenderCommandsChannel)
			SCOPED_NAMED_EVENT_TCHAR(Name, FColor::Magenta);
			FOptionalTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
	
			TArray<FCommand> PoppedQueue;
	
			Mutex.Lock();
			PoppedQueue = MoveTemp(Frame_RenderThread->Queue);
			Frame_RenderThread->Queue.Reserve(128);
			Mutex.Unlock();
	
			FRenderCommandPipe* const PreviousReplayingPipe = UE::RenderCommandPipe::ReplayingPipe;
			UE::RenderCommandPipe::ReplayingPipe = this;
	
			for (FCommand& Command : PoppedQueue)
			{
				ExecuteCommand(MoveTemp(Command.Function), Command.Name, *Command.SpecId);
			}
	
			UE::RenderCommandPipe::ReplayingPipe = PreviousReplayingPipe;
			NumInFlightCommands.fetch_sub(PoppedQueue.Num(), std::memory_order_release);
	
		}, Frame_GameThread->TaskEvent);
	}
}