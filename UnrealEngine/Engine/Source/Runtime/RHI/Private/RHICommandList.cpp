// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHICommandList.h"
#include "Misc/App.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/ConsumeAllMpmcQueue.h"
#include "RHI.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "RHIBreadcrumbs.h"
#include "RHIResourceUpdates.h"
#include "RHIContext.h"
#include "RHIFwd.h"
#include "RHITransition.h"
#include "Stats/StatsTrace.h"

CSV_DEFINE_CATEGORY_MODULE(RHI_API, RHITStalls, false);
CSV_DEFINE_CATEGORY_MODULE(RHI_API, RHITFlushes, false);

DECLARE_CYCLE_STAT(TEXT("Nonimmed. Command List Execute"), STAT_NonImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command List memory"), STAT_NonImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command count"), STAT_NonImmedCmdListCount, STATGROUP_RHICMDLIST);

DECLARE_CYCLE_STAT(TEXT("All Command List Execute"), STAT_ImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command List memory"), STAT_ImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command count"), STAT_ImmedCmdListCount, STATGROUP_RHICMDLIST);

UE_TRACE_CHANNEL_DEFINE(RHICommandsChannel);

#if VALIDATE_UNIFORM_BUFFER_STATIC_BINDINGS
bool FScopedUniformBufferStaticBindings::bRecursionGuard = false;
#endif

#if !PLATFORM_USES_FIXED_RHI_CLASS
#include "RHICommandListCommandExecutes.inl"
#endif

static TAutoConsoleVariable<int32> CVarRHICmdBypass(
	TEXT("r.RHICmdBypass"),
	0,
	TEXT("Whether to bypass the rhi command list and send the rhi commands immediately.\n")
	TEXT("0: Disable (required for the multithreaded renderer)\n")
	TEXT("1: Enable (convenient for debugging low level graphics API calls, can suppress artifacts from multithreaded renderer code)"));

TAutoConsoleVariable<int32> CVarRHICmdWidth(
	TEXT("r.RHICmdWidth"), 
	8,
	TEXT("Controls the task granularity of a great number of things in the parallel renderer."));

TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasks(
	TEXT("r.RHICmdFlushRenderThreadTasks"),
	0,
	TEXT("If true, then we flush the render thread tasks every pass. For issue diagnosis. This is a main switch for more granular cvars."));

static TAutoConsoleVariable<int32> CVarRHICmdMergeSmallDeferredContexts(
	TEXT("r.RHICmdMergeSmallDeferredContexts"),
	1,
	TEXT("When it can be determined, merge small parallel translate tasks based on r.RHICmdMinDrawsPerParallelCmdList."));

static TAutoConsoleVariable<int32> CVarRHICmdBufferWriteLocks(
	TEXT("r.RHICmdBufferWriteLocks"),
	1,
	TEXT("Only relevant with an RHI thread. Debugging option to diagnose problems with buffered locks."));

static TAutoConsoleVariable<int32> CVarRHICmdMaxOutstandingMemoryBeforeFlush(
	TEXT("r.RHICmdMaxOutstandingMemoryBeforeFlush"),
	256,
	TEXT("In kilobytes. The amount of outstanding memory before the RHI will force a flush. This should generally be set high enough that it doesn't happen on typical frames."));

static FAutoConsoleTaskPriority CPrio_RHIThreadOnTaskThreads(
	TEXT("TaskGraph.TaskPriorities.RHIThreadOnTaskThreads"),
	TEXT("Task and thread priority for when we are running 'RHI thread' tasks on any thread."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::NormalTaskPriority
);

static FAutoConsoleTaskPriority CPrio_FParallelTranslateCommandList(
	TEXT("TaskGraph.TaskPriorities.ParallelTranslateCommandList"),
	TEXT("Task and thread priority for FParallelTranslateCommandList."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::NormalTaskPriority
);

static FAutoConsoleTaskPriority CPrio_FParallelTranslateCommandListPrepass(
	TEXT("TaskGraph.TaskPriorities.ParallelTranslateCommandListPrepass"),
	TEXT("Task and thread priority for FParallelTranslateCommandList for the prepass, which we would like to get to the GPU asap."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::HighTaskPriority
);

FAutoConsoleTaskPriority CPrio_SceneRenderingTask(
	TEXT("TaskGraph.TaskPriorities.SceneRenderingTask"),
	TEXT("Task and thread priority for various scene rendering tasks."),
	ENamedThreads::NormalThreadPriority,
	ENamedThreads::HighTaskPriority
);

DECLARE_CYCLE_STAT(TEXT("Parallel Translate"),                 STAT_ParallelTranslate,      STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("RHI Thread Parallel Translate Wait"), STAT_ParallelTranslateWait,  STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for command list dispatch"),     STAT_WaitForCmdListDispatch, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Explicit wait for tasks"),            STAT_ExplicitWait,           STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Explicit wait for RHI thread"),       STAT_ExplicitWaitRHIThread,  STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Spin RHIThread wait for stall"),      STAT_SpinWaitRHIThreadStall, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("RHI Thread Execute"),                 STAT_RHIThreadExecute,       STATGROUP_RHICMDLIST);

bool GUseRHIThread_InternalUseOnly = false;
bool GUseRHITaskThreads_InternalUseOnly = false;
bool GIsRunningRHIInSeparateThread_InternalUseOnly = false;
bool GIsRunningRHIInDedicatedThread_InternalUseOnly = false;
bool GIsRunningRHIInTaskThread_InternalUseOnly = false;

/** Accumulates how many cycles the renderthread has been idle. */
uint32 GRenderThreadIdle[ERenderThreadIdleTypes::Num] = { 0 };

uint32 GWorkingRHIThreadTime = 0;
uint32 GWorkingRHIThreadStartCycles = 0;

/** How many cycles the from sampling input to the frame being flipped. */
uint64 GInputLatencyTime = 0;

FRHICommandBase* GCurrentCommand = nullptr;

bool GEnableAsyncCompute = true;
FRHICommandListExecutor GRHICommandList;

FGraphEventArray FRHICommandListImmediate::WaitOutstandingTasks;
FGraphEventRef   FRHICommandListImmediate::RHIThreadTask;
FRHIDrawStats    FRHICommandListImmediate::FrameDrawStats;

static FGraphEventRef GRHIThreadEndDrawingViewportFences[2];
static uint32 GRHIThreadEndDrawingViewportFenceIndex = 0;

static TStatId GCurrentExecuteStat;

static FCriticalSection GRHIThreadOnTasksCritical;
static std::atomic<int32> GRHIThreadStallRequestCount;

static std::atomic<int32> GRHIResourceLifetimeRefCount{0};

void RHIResourceLifetimeAddRef(int32 NumRefs)
{
	GRHIResourceLifetimeRefCount.fetch_add(NumRefs, std::memory_order_relaxed);
}

void RHIResourceLifetimeReleaseRef(FRHICommandListImmediate& RHICmdList, int32 NumRefs)
{
	int32 RefCount = GRHIResourceLifetimeRefCount.fetch_sub(NumRefs, std::memory_order_release) - 1;
	check(RefCount >= 0);
	if (!RefCount)
	{
		RHICmdList.FlushExtendedLifetimeResourceDeletes();
	}
}

FRHICommandListBase::FRHICommandListBase(FRHIGPUMask InGPUMask, ERecordingThread InRecordingThread, bool bImmediate)
	: FRHICommandListBase(FPersistentState(InGPUMask, InRecordingThread, bImmediate))
{}

FRHICommandListBase::FRHICommandListBase(FPersistentState&& InPersistentState)
	: DispatchEvent(FGraphEvent::CreateGraphEvent())
	, PersistentState(MoveTemp(InPersistentState))
{
	DispatchEvent->SetDebugName(TEXT("FRHICommandListBase::DispatchEvent"));

	CommandLink = &Root;
	UID = GRHICommandList.UIDCounter.Increment();

	if (!IsImmediate())
	{
		PersistentState.FenceCandidate = new FPersistentState::FFenceCandidate;
	}

#if DO_CHECK
	if (PersistentState.RecordingThread == ERecordingThread::Render)
	{
		GRHICommandList.OutstandingCmdListCount.Increment();
	}
#endif
}

FRHICommandListBase::FRHICommandListBase(FRHICommandListBase&& Other)
    : Root            (MoveTemp(Other.Root))
    , CommandLink     (MoveTemp(Other.CommandLink))
    , GraphicsContext (MoveTemp(Other.GraphicsContext))
    , ComputeContext  (MoveTemp(Other.ComputeContext))
    , Contexts        (MoveTemp(Other.Contexts))
#if RHI_COUNT_COMMANDS
    , NumCommands     (MoveTemp(Other.NumCommands))
#endif				  
    , UID             (MoveTemp(Other.UID))
    , bExecuting      (MoveTemp(Other.bExecuting))
    , ActivePipeline  (MoveTemp(Other.ActivePipeline))
#if DO_CHECK
	, AllowedPipelines(MoveTemp(Other.AllowedPipelines))
#endif			  
	, DispatchEvent   (MoveTemp(Other.DispatchEvent))
    , ExecuteStat     (MoveTemp(Other.ExecuteStat))
    , MemManager      (MoveTemp(Other.MemManager))
    , PersistentState (Other.PersistentState) // Always copy this
#if RHI_WANT_BREADCRUMB_EVENTS
    , Breadcrumbs     (MoveTemp(Other.Breadcrumbs))
#endif
	, QueryBatchData  (MoveTemp(Other.QueryBatchData))
{
#if DO_CHECK
	if (PersistentState.RecordingThread == ERecordingThread::Render)
	{
		GRHICommandList.OutstandingCmdListCount.Increment();
	}
#endif

	Other.Root = nullptr;
}

FRHICommandListBase::~FRHICommandListBase()
{
#if DO_CHECK
	// Some configurations enable checks in shipping/test, particularly server builds. Skip these checks explicitly in that case, as they can fire very late in
	// the shutdown process and crash in unexpected ways because the log output channel has already been destroyed. Also, having pending commands
	// on shutdown shouldn't really be a fatal error, it's a fairly harmless condition.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	checkf(!HasCommands(), TEXT("FRHICommandListBase has been deleted while it still contained commands. The command list was not submitted."));

	for (void* Data : QueryBatchData)
	{
		check(Data == nullptr);
	}
#endif

	if (PersistentState.RecordingThread == ERecordingThread::Render)
	{
		GRHICommandList.OutstandingCmdListCount.Decrement();
	}
#endif
}

void FRHICommandListImmediate::Reset()
{
#if RHI_WANT_BREADCRUMB_EVENTS
	Breadcrumbs.Stack.ValidateEmpty();
#endif

	// Destruct and reconstruct the base type in-place to resets all members to their defaults.
	// We also need to preserve the contents of PersistentState.
	FPersistentState LocalPersistentState = MoveTemp(PersistentState);

	// The initial GPU mask must be updated here to preserve the last mask set on the immediate command list.
	// If we don't do this, the first set of commands recorded in the immediate command list after an Execute/Reset will inherit the wrong mask.
	LocalPersistentState.InitialGPUMask = LocalPersistentState.CurrentGPUMask;

	FRHICommandListBase* Base = static_cast<FRHICommandListBase*>(this);
	Base->~FRHICommandListBase();
	new (Base) FRHICommandListBase(MoveTemp(LocalPersistentState));
}

const int32 FRHICommandListBase::GetUsedMemory() const
{
	return MemManager.GetByteCount();
}

FGraphEventArray& FRHICommandListImmediate::GetRenderThreadTaskArray()
{
	check(IsInRenderingThread());
	return WaitOutstandingTasks;
}

void FRHICommandListBase::AddDispatchPrerequisite(const FGraphEventRef& Prereq)
{
	checkf(!Bypass(), TEXT("Dispatch prerequisites cannot be used in bypass mode."));
	checkf(!IsImmediate() || IsInRenderingThread(), TEXT("Only the rendering thread is allowed to add dispatch prerequisites to the immediate command list."));

	if (Prereq.GetReference())
	{
		DispatchEvent->DontCompleteUntil(Prereq);
	}
}

void FRHICommandListBase::FinishRecording()
{
	checkf(!IsImmediate(), TEXT("Do not call FinishRecording() on the immediate RHI command list."));

	PersistentState.FenceCandidate->Fence = PersistentState.RHIThreadBufferLockFence;

	// "Complete" the dispatch event. This unblocks waiting tasks but only when
	// all dependencies added via AddDispatchPrerequisite() have been resolved.
	DispatchEvent->DispatchSubsequents();
}

void FRHICommandListBase::WaitForDispatchEvent()
{
	if (!DispatchEvent->IsComplete())
	{
		SCOPE_CYCLE_COUNTER(STAT_WaitForCmdListDispatch);
		DispatchEvent->Wait();
	}
}

FRHICOMMAND_MACRO(FRHICommandStat)
{
	TStatId CurrentExecuteStat;
	FORCEINLINE_DEBUGGABLE FRHICommandStat(TStatId InCurrentExecuteStat)
		: CurrentExecuteStat(InCurrentExecuteStat)
	{
	}
	void Execute(FRHICommandListBase & CmdList)
	{
		GCurrentExecuteStat = CurrentExecuteStat;
	}
};

void FRHICommandListBase::SetCurrentStat(TStatId Stat)
{
	if (!Bypass())
	{
		ALLOC_COMMAND(FRHICommandStat)(Stat);
	}
}

#if HAS_GPU_STATS
void FRHICommandListBase::SetStatsCategory(FDrawCallCategoryName* Category)
{
	check(!Category || Category->ShouldCountDraws());

	PersistentState.Stats.CategoryTOP = Category;
	EnqueueLambda([Category](FRHICommandListBase& ExecutingCmdList)
	{
		ExecutingCmdList.PersistentState.Stats.CategoryBOP = Category;

		for (IRHIComputeContext* Context : ExecutingCmdList.Contexts)
		{
			if (Context)
			{
				ExecutingCmdList.PersistentState.Stats.ApplyToContext(Context);
			}
		}
	});
}
#endif

ERHIPipeline FRHICommandListBase::SwitchPipeline(ERHIPipeline Pipeline)
{
	checkf(Pipeline == ERHIPipeline::None || FMath::IsPowerOfTwo((__underlying_type(ERHIPipeline))Pipeline), TEXT("Only one pipeline may be active at a time."));
	checkf(Pipeline == ERHIPipeline::None || EnumHasAnyFlags(AllowedPipelines, Pipeline), TEXT("The specified pipeline is not allowed on this RHI command list."));

	Exchange(ActivePipeline, Pipeline);
	if (ActivePipeline != Pipeline)
	{
		EnqueueLambda([NewPipeline = ActivePipeline](FRHICommandListBase& ExecutingCmdList)
		{
			ExecutingCmdList.ActivePipeline = NewPipeline;

			//
			// Grab the appropriate command contexts from the RHI if we don't already have them.
			// Also update the GraphicsContext/ComputeContext pointers to direct recorded commands
			// to the correct target context, based on which pipeline is now active.
			//
			if (NewPipeline == ERHIPipeline::None)
			{
				ExecutingCmdList.GraphicsContext = nullptr;
				ExecutingCmdList.ComputeContext  = nullptr;
			}
			else
			{
				IRHIComputeContext*& Context = ExecutingCmdList.Contexts[NewPipeline];

				switch (NewPipeline)
				{
				default: checkNoEntry();
				case ERHIPipeline::Graphics:
					{
						if (!Context)
						{
							// Need to handle the "immediate" context separately.
							Context = ExecutingCmdList.PersistentState.bImmediate
								? ::RHIGetDefaultContext()
								: GDynamicRHI->RHIGetCommandContext(NewPipeline, FRHIGPUMask::All()); // This mask argument specifies which contexts are included in an mGPU redirector (we always want all of them).
						}

						ExecutingCmdList.GraphicsContext = static_cast<IRHICommandContext*>(Context);
						ExecutingCmdList.ComputeContext  = Context;
					}
					break;

				case ERHIPipeline::AsyncCompute:
					{
						if (!Context)
						{
							Context = GDynamicRHI->RHIGetCommandContext(NewPipeline, FRHIGPUMask::All()); // This mask argument specifies which contexts are included in an mGPU redirector (we always want all of them).
							check(Context);
						}

						ExecutingCmdList.GraphicsContext = nullptr;
						ExecutingCmdList.ComputeContext  = Context;
					}
					break;
				}

				// (Re-)apply the current GPU mask.
				Context->RHISetGPUMask(ExecutingCmdList.PersistentState.CurrentGPUMask);

				// Ensure the context has an up-to-date stat pointer.
				ExecutingCmdList.PersistentState.Stats.ApplyToContext(Context);
			}
		});
	}

	return Pipeline;
}

void FRHICommandListBase::Execute(TRHIPipelineArray<IRHIComputeContext*>& InOutContexts, FPersistentState::FGPUStats* ParentStats)
{
	check(!IsExecuting());
	bExecuting = true;

	Contexts = InOutContexts;
	PersistentState.CurrentGPUMask = PersistentState.InitialGPUMask;
	PersistentState.Stats.InitFrom(ParentStats);

	ON_SCOPE_EXIT
	{
		// Setting Root to nullptr indicates the commands have
		// been consumed, and HasCommands() will return false.
		Root = nullptr;

		// Also pass back the list of contexts
		InOutContexts = Contexts;
	};

	FScopeCycleCounter ScopeOuter(ExecuteStat);

#if WITH_ADDITIONAL_CRASH_CONTEXTS && RHI_WANT_BREADCRUMB_EVENTS
	bool PopStack = Breadcrumbs.PushStack();
	ON_SCOPE_EXIT { if (PopStack) { Breadcrumbs.PopStack(); } };

	FScopedAdditionalCrashContextProvider CrashContext(
	[
		Stack      = &Breadcrumbs.StackTop[0],
		StackIndex = Breadcrumbs.StackIndex,
		ThreadName = 
			  IsInRHIThread()             ? TEXT("RHIThread")
			: IsInActualRenderingThread() ? TEXT("RenderingThread")
			: IsInGameThread()            ? TEXT("GameThread")
			:                               TEXT("Parallel")
	](FCrashContextExtendedWriter& Writer)
	{
		FRHIBreadcrumbStack::WriteRenderBreadcrumbs(Writer, Stack, StackIndex, ThreadName);
	});
#endif

	FRHICommandListDebugContext DebugContext;
	FRHICommandListIterator Iter(*this);

#if STATS || ENABLE_STATNAMEDEVENTS
	if (GCycleStatsShouldEmitNamedEvents STAT( || FThreadStats::IsCollectingData() ) )
	{
		while (Iter.HasCommandsLeft())
		{
			TStatId Stat = GCurrentExecuteStat;
			FScopeCycleCounter Scope(Stat);
			while (Iter.HasCommandsLeft() && Stat == GCurrentExecuteStat)
			{
				FRHICommandBase* Cmd = Iter.NextCommand();
				GCurrentCommand = Cmd;
				//FPlatformMisc::Prefetch(Cmd->Next);
				Cmd->ExecuteAndDestruct(*this, DebugContext);
			}
		}
	}
	else
#endif
	{
		while (Iter.HasCommandsLeft())
		{
			FRHICommandBase* Cmd = Iter.NextCommand();
			GCurrentCommand = Cmd;
			//FPlatformMisc::Prefetch(Cmd->Next);
			Cmd->ExecuteAndDestruct(*this, DebugContext);
		}
	}
}

void FRHICommandListImmediate::QueueAsyncCommandListSubmit(TArrayView<FQueuedCommandList> CommandLists, ETranslatePriority ParallelTranslatePriority, int32 MinDrawsPerTranslate)
{
	check(IsInRenderingThread());

	if (CommandLists.Num() == 0)
		return;

	for (FQueuedCommandList const& QueuedCmdList : CommandLists)
	{
		check(QueuedCmdList.CmdList);

		// Accumulate dispatch ready events into the WaitOutstandingTasks list.
		// This is used by FRHICommandListImmediate::WaitForTasks() when the render thread 
		// wants to block until all parallel RHICmdList recording tasks are completed.
		WaitOutstandingTasks.Add(QueuedCmdList.CmdList->DispatchEvent);
	}

	if (ParallelTranslatePriority != ETranslatePriority::Disabled && GRHISupportsParallelRHIExecute && IsRunningRHIInSeparateThread())
	{
		// The provided RHI command lists will be translated to platform command lists in parallel.
		
		// Commands may already be queued on the immediate command list. These need to be executed
		// first before any parallel commands can be inserted, otherwise commands will run out-of-order.
		ExecuteAndReset(false);
		InitializeImmediateContexts();

		struct FTask
		{
			FGraphEventRef Event;
			TArrayView<FRHICommandListBase*> InCmdLists;
			TArray<IRHIPlatformCommandList*, TInlineAllocator<GetRHIPipelineCount()>> OutCmdLists;
			FRHIDrawStats Stats;
		};

		uint32 NumTasks = 0;
		TArrayView<FTask> Tasks = AllocArrayUninitialized<FTask>(CommandLists.Num());

		const bool bMerge = !!CVarRHICmdMergeSmallDeferredContexts.GetValueOnRenderThread();
		for (int32 RangeStart = 0, RangeEnd = 0; RangeStart < CommandLists.Num(); RangeStart = RangeEnd)
		{
			RangeEnd = RangeStart + 1;

			if (bMerge)
			{
				for (int32 NumDraws = 0, Index = RangeStart; Index < CommandLists.Num(); ++Index)
				{
					// Command lists without NumDraws set are translated on their own
					if (!CommandLists[Index].NumDraws.IsSet())
						break;

					// Otherwise group command lists into batches to reach at least MinDrawsPerTranslate
					NumDraws += CommandLists[Index].NumDraws.GetValue();
					RangeEnd = Index + 1;

					if (NumDraws >= MinDrawsPerTranslate)
						break;
				}
			}

			const int32 NumCmdListsInBatch = RangeEnd - RangeStart;

			FTask& Task = *(new (&Tasks[NumTasks++]) FTask());
			Task.InCmdLists = AllocArrayUninitialized<FRHICommandListBase*>(NumCmdListsInBatch);

			// Gather the list of active pipelines and prerequisites for this batch of command lists
			FGraphEventArray Prereqs;
			for (int32 Index = 0; Index < NumCmdListsInBatch; ++Index)
			{
				FRHICommandListBase* CmdList = CommandLists[RangeStart + Index].CmdList;

				Task.InCmdLists[Index] = CmdList;
				Prereqs.Add(CmdList->DispatchEvent);
			}

			if (PersistentState.QueuedFenceCandidates.Num() > 0)
			{
				FGraphEventRef FenceCandidateEvent = FGraphEvent::CreateGraphEvent();

				if (PersistentState.RHIThreadBufferLockFence.GetReference())
				{
					FenceCandidateEvent->DontCompleteUntil(PersistentState.RHIThreadBufferLockFence);
				}

				PersistentState.RHIThreadBufferLockFence = FenceCandidateEvent;
				Prereqs.Add(FenceCandidateEvent);

				FFunctionGraphTask::CreateAndDispatchWhenReady(
					[FenceCandidates = MoveTemp(PersistentState.QueuedFenceCandidates), FenceCandidateEvent](ENamedThreads::Type, const FGraphEventRef&) mutable
				{
					SCOPED_NAMED_EVENT(FRHICommandListBase_SignalLockFence, FColor::Magenta);

					for (int32 Index = FenceCandidates.Num() - 1; Index >= 0; Index--)
					{
						if (FenceCandidates[Index]->Fence)
						{
							FenceCandidateEvent->DontCompleteUntil(FenceCandidates[Index]->Fence);
							break;
						}
					}

					FenceCandidateEvent->DispatchSubsequents();

				}, TStatId(), &PersistentState.QueuedFenceCandidateEvents);

				PersistentState.QueuedFenceCandidateEvents.Reset();
			}
			else if (PersistentState.RHIThreadBufferLockFence)
			{
				Prereqs.Add(PersistentState.RHIThreadBufferLockFence);
			}

			// Start a parallel translate task to replay the command list batch into the given pipeline contexts
			Task.Event = FFunctionGraphTask::CreateAndDispatchWhenReady(
				[&Task, GPUStatsInitial = PersistentState.Stats]()
				{
					FOptionalTaskTagScope Scope(ETaskTag::EParallelRhiThread);
					SCOPE_CYCLE_COUNTER(STAT_ParallelTranslate);
					SCOPED_NAMED_EVENT(FParallelTranslateCommandList_DoTask, FColor::Magenta);

					TRHIPipelineArray<IRHIComputeContext*> Contexts = {};

					// Replay the recorded commands. The Contexts array accumulates any used
					// contexts depending on the SwitchPipeline commands that were recorded.
					for (FRHICommandListBase* RHICmdList : Task.InCmdLists)
					{
						// Redirect the output stats to this parallel task's copy
						FPersistentState::FGPUStats Stats = GPUStatsInitial;
						Stats.Ptr = &Task.Stats;

						RHICmdList->Execute(Contexts, &Stats);
						delete RHICmdList;
					}

					// Convert the completed contexts into IRHIPlatformCommandList instances.
					// These are submitted by the RHI thread waiting on this translate task.
					for (IRHIComputeContext* Context : Contexts)
					{
						if (Context)
						{
							IRHIPlatformCommandList* CommandList = GDynamicRHI->RHIFinalizeContext(Context);
							if (CommandList)
							{
								Task.OutCmdLists.Add(CommandList);
							}
						}
					}
				}
				, QUICK_USE_CYCLE_STAT(FParallelTranslateCommandList, STATGROUP_TaskGraphTasks)
				, &Prereqs
				, ParallelTranslatePriority == ETranslatePriority::High
					? CPrio_FParallelTranslateCommandListPrepass.Get()
					: CPrio_FParallelTranslateCommandList.Get()
			);
		}

		// Resize the tasks array view to how many tasks we actually created after merging
		Tasks = TArrayView<FTask>(Tasks.GetData(), NumTasks);

		// Finally, add an RHI thread task to submit the completed platform command lists.
		// The task blocks for each parallel translate completion, in the order they will be submitted in.
		EnqueueLambda(TEXT("SubmitParallelCommandLists"), [Tasks](FRHICommandListBase& ExecutingCmdList)
		{
			TArray<IRHIPlatformCommandList*> AllCmdLists;

			for (FTask& Task : Tasks)
			{
				if (!Task.Event->IsComplete())
				{
					SCOPE_CYCLE_COUNTER(STAT_ParallelTranslateWait);
					Task.Event->Wait();
				}

				AllCmdLists.Append(Task.OutCmdLists);
				ExecutingCmdList.PersistentState.Stats.Ptr->Accumulate(Task.Stats);

				Task.~FTask();
			}

			if (AllCmdLists.Num())
			{
				GDynamicRHI->RHISubmitCommandLists(AllCmdLists, false);
			}
		});
	}
	else
	{
		// Commands will be executed directly on the RHI thread / default contexts
		TArrayView<FRHICommandListBase*> CmdListsView = AllocArrayUninitialized<FRHICommandListBase*>(CommandLists.Num());
		for (int32 Index = 0; Index < CommandLists.Num(); ++Index)
		{
			FRHICommandListBase* CommandList = CommandLists[Index].CmdList;
			PersistentState.QueuedFenceCandidateEvents.Emplace(CommandList->DispatchEvent);
			PersistentState.QueuedFenceCandidates.Emplace(CommandList->PersistentState.FenceCandidate);
			CmdListsView[Index] = CommandList;
		}

		EnqueueLambda(TEXT("ExecuteCommandLists"), [CmdListsView](FRHICommandListBase& ParentCmdList)
		{
			for (FRHICommandListBase* CmdList : CmdListsView)
			{
				CmdList->WaitForDispatchEvent();
				CmdList->Execute(ParentCmdList.Contexts, &ParentCmdList.PersistentState.Stats);
				delete CmdList;
			}
		});
	}
}

void FRHICommandListImmediate::ExecuteAndReset(bool bFlushResources)
{
	check(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_ImmedCmdListExecuteTime);
	INC_MEMORY_STAT_BY(STAT_ImmedCmdListMemory, GetUsedMemory());
#if RHI_COUNT_COMMANDS
	INC_DWORD_STAT_BY(STAT_ImmedCmdListCount, NumCommands);
#endif

#if RHI_WANT_BREADCRUMB_EVENTS
	FRHIBreadcrumbState BreadcrumbState;
	// Once executed, the memory containing the breadcrumbs will be freed, so any open markers are popped and stored into BreadcrumbState
	ExportBreadcrumbState(BreadcrumbState);
	Breadcrumbs.Stack.Reset();

	// And then pushed into the newly opened list on exit
	ON_SCOPE_EXIT { ImportBreadcrumbState(BreadcrumbState); };
#endif // RHI_WANT_BREADCRUMB_EVENT

	// Always reset the immediate command list when we're done.
	ON_SCOPE_EXIT { Reset(); };

	//
	// In bypass mode, the immediate command list will never contain recorded commands (since these were forwarded directly into the immediate RHI contexts).
	// However, platforms that use explicit command list submission still need to be told to submit the enqueued work in the default contexts. Do that now.
	//
	// In non-bypass mode, always submit.
	//
	EnqueueLambda(TEXT("FinalizeAndSubmitCommandLists"), [bFlushResources](FRHICommandListImmediate& ExecutingCmdList)
	{
		TArray<IRHIPlatformCommandList*, TInlineAllocator<GetRHIPipelineCount()>> CommandLists;
		for (IRHIComputeContext* Context : ExecutingCmdList.Contexts)
		{
			if (Context)
			{
				IRHIPlatformCommandList* CommandList = GDynamicRHI ? GDynamicRHI->RHIFinalizeContext(Context) : nullptr;
				if (CommandList)
				{
					CommandLists.Add(CommandList);
				}
			}
		}

		if (CommandLists.Num() || (GDynamicRHI && bFlushResources))
		{
			GDynamicRHI->RHISubmitCommandLists(CommandLists, bFlushResources);
		}
	});

	if (bFlushResources)
	{
		CSV_SCOPED_TIMING_STAT(RHITFlushes, FlushRHIThreadFlushResourcesTotal);
		FlushPendingDeletes();
	}

	// Equivalent to FinishRecording(), without the check(!IsImmediate()).
	DispatchEvent->DispatchSubsequents();

	if (HasCommands())
	{
	    if (IsRunningRHIInSeparateThread())
	    {
		    // The RHI thread/task is going to handle executing this command list.
		    FGraphEventArray Prereqs;
    
		    if (!DispatchEvent->IsComplete())
		    {
			    Prereqs.Add(DispatchEvent);
			    WaitOutstandingTasks.Add(DispatchEvent);
		    }
    
		    // Chain RHI tasks together, so they run in-order
		    if (RHIThreadTask)
		    {
			    Prereqs.Add(RHIThreadTask);
		    }
    
		    // Enqueue a task for the RHI thread
		    RHIThreadTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			    [RHICmdList = MoveTemp(static_cast<FRHICommandListBase&>(*this))]() mutable
			    {
				    // RHI thread tasks main entry point
					check(IsInRHIThread());
				    SCOPE_CYCLE_COUNTER(STAT_RHIThreadExecute);
				    if (IsRunningRHIInTaskThread())
				    {
					    PRAGMA_DISABLE_DEPRECATION_WARNINGS
					    GRHIThreadId = FPlatformTLS::GetCurrentThreadId();
					    PRAGMA_ENABLE_DEPRECATION_WARNINGS
				    }
    
				    {
					    FScopeLock Lock(&GRHIThreadOnTasksCritical);
					    GWorkingRHIThreadStartCycles = FPlatformTime::Cycles();
    
					    RHICmdList.Execute(RHICmdList.Contexts, nullptr);
    
					    GWorkingRHIThreadTime += (FPlatformTime::Cycles() - GWorkingRHIThreadStartCycles);
				    }
    
				    if (IsRunningRHIInTaskThread())
				    {
					    PRAGMA_DISABLE_DEPRECATION_WARNINGS
					    GRHIThreadId = 0;
					    PRAGMA_ENABLE_DEPRECATION_WARNINGS
				    }
			    }
			    , QUICK_USE_CYCLE_STAT(FExecuteRHIThreadTask, STATGROUP_TaskGraphTasks)
			    , &Prereqs
			    , IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : CPrio_RHIThreadOnTaskThreads.Get()
		    );
	    }
	    else
	    {
		    // We're going to be executing the command list on the render thread.
		    WaitForDispatchEvent();
		    FRHICommandListBase::Execute(Contexts, nullptr);
	    }
	}
}

void FRHICommandListImmediate::InitializeImmediateContexts()
{
	check(Contexts[ERHIPipeline::Graphics    ] == nullptr);
	check(Contexts[ERHIPipeline::AsyncCompute] == nullptr);

	// This can be called before the RHI is initialized, in which case
	// leave the immediate command list as default (contexts are nullptr).
	if (GDynamicRHI)
	{
		// The immediate command list always starts with Graphics as the active pipeline.
		SwitchPipeline(ERHIPipeline::Graphics);
	}
}

FRHICOMMAND_MACRO(FRHICommandRHIThreadFence)
{
	FGraphEventRef Fence;
	FORCEINLINE_DEBUGGABLE FRHICommandRHIThreadFence()
		: Fence(FGraphEvent::CreateGraphEvent())
{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		check(IsInRHIThread());
		Fence->DispatchSubsequents(IsRunningRHIInDedicatedThread() ? ENamedThreads::RHIThread : ENamedThreads::AnyThread);
		Fence = nullptr;
	}
};

FGraphEventRef FRHICommandListBase::RHIThreadFence(bool bSetLockFence)
{
	if (bSetLockFence)
	{
		PersistentState.QueuedFenceCandidateEvents.Empty();
		PersistentState.QueuedFenceCandidates.Empty();
	}

	if (IsRunningRHIInSeparateThread())
	{
		FRHICommandRHIThreadFence* Cmd = ALLOC_COMMAND(FRHICommandRHIThreadFence)();
		if (bSetLockFence)
		{
			PersistentState.RHIThreadBufferLockFence = Cmd->Fence;
		}
		return Cmd->Fence;
	}

	return nullptr;
}

FRHICommandList_RecursiveHazardous::FRHICommandList_RecursiveHazardous(IRHICommandContext* Context)
	: FRHICommandList(Context->RHIGetGPUMask())
{
	ActivePipeline = ERHIPipeline::Graphics;
#if DO_CHECK
	AllowedPipelines = ActivePipeline;
#endif

	// Always grab the validation RHI context if active, so that the
	// validation RHI can see any RHI commands enqueued within the RHI itself.
	GraphicsContext = static_cast<IRHICommandContext*>(&Context->GetHighestLevelContext());
	ComputeContext = GraphicsContext;

	Contexts[ERHIPipeline::Graphics] = GraphicsContext;

	PersistentState.bAsyncPSOCompileAllowed = false;
}

FRHICommandList_RecursiveHazardous::~FRHICommandList_RecursiveHazardous()
{
	FinishRecording();
	WaitForDispatchEvent();

	if (HasCommands())
	{
		Execute(Contexts, nullptr);
	}
}

FRHIComputeCommandList_RecursiveHazardous::FRHIComputeCommandList_RecursiveHazardous(IRHIComputeContext* Context)
	: FRHIComputeCommandList(Context->RHIGetGPUMask())
{
	ActivePipeline = Context->GetPipeline();
#if DO_CHECK
	AllowedPipelines = ActivePipeline;
#endif

	// Always grab the validation RHI context if active, so that the
	// validation RHI can see any RHI commands enqueued within the RHI itself.
	GraphicsContext = nullptr;
	ComputeContext = &Context->GetHighestLevelContext();
	Contexts[ActivePipeline] = ComputeContext;

	PersistentState.bAsyncPSOCompileAllowed = false;
}

FRHIComputeCommandList_RecursiveHazardous::~FRHIComputeCommandList_RecursiveHazardous()
{
	FinishRecording();
	WaitForDispatchEvent();

	if (HasCommands())
	{
		Execute(Contexts, nullptr);
	}
}
	
void FRHICommandListExecutor::LatchBypass()
{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList();

	if (IsRunningRHIInSeparateThread())
	{
		if (bLatchedBypass)
		{
			CheckNoOutstandingCmdLists();
			check(!RHICmdList.HasCommands());

			bLatchedBypass = false;
		}
	}
	else
	{
		RHICmdList.ExecuteAndReset(false);

		CheckNoOutstandingCmdLists();
		check(!RHICmdList.HasCommands());

		struct FOnce
		{
			FOnce()
			{
				if (FParse::Param(FCommandLine::Get(), TEXT("forcerhibypass")) && CVarRHICmdBypass.GetValueOnRenderThread() == 0)
				{
					IConsoleVariable* BypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBypass"));
					BypassVar->Set(1, ECVF_SetByCommandline);
				}
				else if (FParse::Param(FCommandLine::Get(), TEXT("parallelrendering")) && CVarRHICmdBypass.GetValueOnRenderThread() >= 1)
				{
					IConsoleVariable* BypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBypass"));
					BypassVar->Set(0, ECVF_SetByCommandline);
				}
			}
		} static Once;

		check(!GDynamicRHI || IsInRenderingThread());

		bool NewBypass = IsInGameThread() || (CVarRHICmdBypass.GetValueOnAnyThread() >= 1);
		if (NewBypass && !bLatchedBypass)
		{
			RHICmdList.ExecuteAndReset(true);
		}

		bLatchedBypass = NewBypass;

		RHICmdList.InitializeImmediateContexts();
	}
#endif

	if (bLatchedBypass || (!GSupportsParallelRenderingTasksWithSeparateRHIThread && IsRunningRHIInSeparateThread()))
	{
		bLatchedUseParallelAlgorithms = false;
	}
	else
	{
		bLatchedUseParallelAlgorithms = FApp::ShouldUseThreadingForPerformance();
	}
}

bool FRHICommandListExecutor::IsRHIThreadActive()
{
	checkSlow(IsInRenderingThread());
	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList();
	return RHICmdList.RHIThreadTask != nullptr && !RHICmdList.RHIThreadTask->IsComplete();
}

bool FRHICommandListExecutor::IsRHIThreadCompletelyFlushed()
{
	if (IsRHIThreadActive() || GetImmediateCommandList().HasCommands())
	{
		return false;
	}

	return true;
}

void FRHICommandListExecutor::WaitOnRHIThreadFence(FGraphEventRef& Fence)
{
	// Exclude RHIT waits from the RT critical path stat (these waits simply get longer if the RT is running faster, so we don't get useful results)
	FThreadIdleStats::FScopeNonCriticalPath NonCriticalPathScope;

	check(IsInRenderingThread());
	if (Fence.GetReference() && !Fence->IsComplete())
	{
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitOnRHIThreadFence_Dispatch);
			GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // necessary to prevent deadlock
		}
		check(IsRunningRHIInSeparateThread());
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitOnRHIThreadFence_Wait);
		ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(RenderThread_Local))
		{
			// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
			UE_LOG(LogRHI, Fatal, TEXT("Deadlock in WaitOnRHIThreadFence."));
		}
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Fence, RenderThread_Local);
	}
}

DECLARE_CYCLE_STAT(TEXT("Delete Resources"), STAT_DeleteResources, STATGROUP_RHICMDLIST);

extern UE::TConsumeAllMpmcQueue<FRHIResource*> PendingDeletes;
extern UE::TConsumeAllMpmcQueue<FRHIResource*> PendingDeletesWithLifetimeExtension;

int32 FRHICommandListImmediate::FlushPendingDeletes()
{
	SCOPE_CYCLE_COUNTER(STAT_DeleteResources);
	check(IsInRenderingThread());

	TArray<FRHIResource*, FConcurrentLinearArrayAllocator> DeletedResources;

	TArray<FRHIResource*, FConcurrentLinearArrayAllocator>& DeletedResourcesWithLifetimeExtension = GRHIResourceLifetimeRefCount > 0
		? PersistentState.ExtendedLifetimeResources 
		: DeletedResources;

	PendingDeletesWithLifetimeExtension.ConsumeAllLifo([&DeletedResourcesWithLifetimeExtension](FRHIResource* Resource)
	{
		DeletedResourcesWithLifetimeExtension.Emplace(Resource);
	});

	PendingDeletes.ConsumeAllLifo([&DeletedResources](FRHIResource* Resource)
	{
		DeletedResources.Push(Resource);
	});

	const int32 NumDeletes = DeletedResources.Num();

	EnqueueLambda(TEXT("FlushPendingDeletes"), [DeletedResources = MoveTemp(DeletedResources)](FRHICommandListImmediate& RHICmdList) mutable
	{
		SCOPED_NAMED_EVENT(STAT_FRHICommandListImmediate_DeleteResources, FColor::Magenta);
		for (int32 i = DeletedResources.Num() - 1; i >= 0; i--)
		{
			FRHIResource* Resource = DeletedResources[i];
			if (Resource->AtomicFlags.Deleteing())
			{
				FRHIResource::CurrentlyDeleting = Resource;
				delete Resource;
			}
		}
	});

	return NumDeletes;
}

int32 FRHICommandListImmediate::FlushExtendedLifetimeResourceDeletes()
{
	int32 NumDeletes = PersistentState.ExtendedLifetimeResources.Num();

	if (NumDeletes > 0)
	{
		EnqueueLambda(TEXT("FlushExtendedLifetimeResourceDeletes"), [DeletedResources = MoveTemp(PersistentState.ExtendedLifetimeResources)](FRHICommandListImmediate& RHICmdList) mutable
		{
			SCOPED_NAMED_EVENT(STAT_FRHICommandListImmediate_DeleteExtendedLifetimeResources, FColor::Magenta);

			for (int32 i = DeletedResources.Num() - 1; i >= 0; i--)
			{
				FRHIResource* Resource = DeletedResources[i];
				if (Resource->AtomicFlags.Deleteing())
				{
					FRHIResource::CurrentlyDeleting = Resource;
					delete Resource;
				}
			}
		});
	}

	return NumDeletes;
}

void FRHICommandListImmediate::WaitForTasks()
{
	check(IsInRenderingThread());

	if (WaitOutstandingTasks.Num())
	{
		bool bAny = false;
		for (int32 Index = 0; Index < WaitOutstandingTasks.Num(); Index++)
		{
			if (!WaitOutstandingTasks[Index]->IsComplete())
			{
				bAny = true;
				break;
			}
		}

		if (bAny)
		{
			SCOPE_CYCLE_COUNTER(STAT_ExplicitWait);
			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			check(!FTaskGraphInterface::Get().IsThreadProcessingTasks(RenderThread_Local));
			FTaskGraphInterface::Get().WaitUntilTasksComplete(WaitOutstandingTasks, RenderThread_Local);
		}

		WaitOutstandingTasks.Reset();
	}
}

void FRHICommandListImmediate::WaitForRHIThreadTasks()
{
	check(IsInRenderingThread());

	WaitForTasks();

	if (RHIThreadTask && !RHIThreadTask->IsComplete())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread);
		RHIThreadTask->Wait();
	}
}

void FRHICommandListImmediate::Transition(TArrayView<const FRHITransitionInfo> Infos, ERHIPipeline SrcPipelines, ERHIPipeline DstPipelines)
{
	check(IsInRenderingThread());

#if DO_CHECK
	for (const FRHITransitionInfo& Info : Infos)
	{
		checkf(Info.IsWholeResource(), TEXT("Only whole resource transitions are allowed in FRHICommandListImmediate::Transition."));
	}
#endif

	if (!GSupportsEfficientAsyncCompute)
	{
		checkf(SrcPipelines != ERHIPipeline::AsyncCompute, TEXT("Async compute is disabled. Cannot transition from it."));
		checkf(DstPipelines != ERHIPipeline::AsyncCompute, TEXT("Async compute is disabled. Cannot transition to it."));

		EnumRemoveFlags(SrcPipelines, ERHIPipeline::AsyncCompute);
		EnumRemoveFlags(DstPipelines, ERHIPipeline::AsyncCompute);
	}

	const FRHITransition* Transition = RHICreateTransition({ SrcPipelines, DstPipelines, ERHITransitionCreateFlags::None, Infos });

	EnumerateRHIPipelines(SrcPipelines, [&](ERHIPipeline Pipeline)
	{
		FRHICommandListScopedPipeline Scope(*this, Pipeline);
		BeginTransition(Transition);
	});

	EnumerateRHIPipelines(DstPipelines, [&](ERHIPipeline Pipeline)
	{
		FRHICommandListScopedPipeline Scope(*this, Pipeline);
		EndTransition(Transition);
	});

	if (EnumHasAnyFlags(SrcPipelines | DstPipelines, ERHIPipeline::Graphics))
	{
		FRHICommandListScopedPipeline Scope(*this, ERHIPipeline::Graphics);
		SetTrackedAccess(Infos);
	}
}

bool FRHICommandListImmediate::IsStalled()
{
	return GRHIThreadStallRequestCount.load() > 0;
}

bool FRHICommandListImmediate::StallRHIThread()
{
	check(IsInRenderingThread() && IsRunningRHIInSeparateThread());

	if (GRHIThreadStallRequestCount.load() > 0)
	{
		return false;
	}

	if (!FRHICommandListExecutor::IsRHIThreadActive())
	{
		return false;
	}

	CSV_SCOPED_TIMING_STAT(RHITStalls, Total);
	SCOPED_NAMED_EVENT(StallRHIThread, FColor::Red);

	const int32 OldStallCount = GRHIThreadStallRequestCount.fetch_add(1);
	if (OldStallCount > 0)
	{
		return true;
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThreadStall);

		{
			SCOPED_NAMED_EVENT(RHIThreadLock_Wait, FColor::Red);
#if PLATFORM_USES_UNFAIR_LOCKS
			// When we have unfair locks, we're not guaranteed to get the lock between the RHI tasks if our thread goes to sleep,
			// so we need to be more aggressive here as this is time critical.
			while (!GRHIThreadOnTasksCritical.TryLock())
			{
				FPlatformProcess::YieldThread();
			}
#else
			GRHIThreadOnTasksCritical.Lock();
#endif
		}
	}
	return true;
}

void FRHICommandListImmediate::UnStallRHIThread()
{
	check(IsInRenderingThread() && IsRunningRHIInSeparateThread());
	const int32 NewStallCount = GRHIThreadStallRequestCount.fetch_sub(1) - 1;
	check(NewStallCount >= 0);
	if (NewStallCount == 0)
	{
		GRHIThreadOnTasksCritical.Unlock();
	}
}

void FRHICommandListImmediate::BeginScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		GetContext().RHIBeginScene();
		return;
	}
	ALLOC_COMMAND(FRHICommandBeginScene)();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginScene_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, BeginScene);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandListImmediate::EndScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		GetContext().RHIEndScene();
		return;
	}
	ALLOC_COMMAND(FRHICommandEndScene)();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndScene_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, EndScene);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandListImmediate::BeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI)
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		GetContext().RHIBeginDrawingViewport(Viewport, RenderTargetRHI);
		return;
	}
	ALLOC_COMMAND(FRHICommandBeginDrawingViewport)(Viewport, RenderTargetRHI);
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginDrawingViewport_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, BeginDrawingViewport);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandListImmediate::EndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync)
{
	// Make sure all prior graphics and async compute work has been submitted.
	// This is necessary because platform RHIs often submit additional work on the graphics queue during present, and we need to ensure we won't deadlock on async work that wasn't yet submitted by the renderer.
	// In future, Present() itself should be an enqueued / recorded command, and platform RHIs should never implicitly submit graphics or async compute work.
	FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		GetContext().RHIEndDrawingViewport(Viewport, bPresent, bLockToVsync);
	}
	else
	{
		ALLOC_COMMAND(FRHICommandEndDrawingViewport)(Viewport, bPresent, bLockToVsync);

		if (IsRunningRHIInSeparateThread())
		{
			// Insert a fence to prevent the renderthread getting more than a frame ahead of the RHIThread
			GRHIThreadEndDrawingViewportFences[GRHIThreadEndDrawingViewportFenceIndex] = static_cast<FRHICommandListImmediate*>(this)->RHIThreadFence();
		}
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_EndDrawingViewport_Dispatch);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	if (IsRunningRHIInSeparateThread())
	{
		// Wait on the previous frame's RHI thread fence (we never want the rendering thread to get more than a frame ahead)
		uint32 PreviousFrameFenceIndex = 1 - GRHIThreadEndDrawingViewportFenceIndex;
		FGraphEventRef& LastFrameFence = GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex];
		FRHICommandListExecutor::WaitOnRHIThreadFence(LastFrameFence);
		GRHIThreadEndDrawingViewportFences[PreviousFrameFenceIndex] = nullptr;
		GRHIThreadEndDrawingViewportFenceIndex = PreviousFrameFenceIndex;
	}

	RHIAdvanceFrameForGetViewportBackBuffer(Viewport);
}

void FRHICommandListImmediate::BeginFrame()
{
	check(IsImmediate() && IsInRenderingThread());

	GDynamicRHI->RHIBeginFrame(*this);

	if (Bypass())
	{
		ProcessStats();
		GetContext().RHIBeginFrame();
		return;
	}
	ALLOC_COMMAND(FRHICommandBeginFrame)();
	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginFrame_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, BeginFrame);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}

}

void FRHICommandListImmediate::EndFrame()
{
	check(IsImmediate() && IsInRenderingThread());

	// Flush any queued fence candidates by injecting a fence. This is to handle cases where no parallel translates happened within the frame.
	if (!PersistentState.QueuedFenceCandidates.IsEmpty())
	{
		RHIThreadFence(true);
	}

	if (Bypass())
	{
		GetContext().RHIEndFrame();
		GDynamicRHI->RHIAdvanceFrameFence();
		return;
	}

	ALLOC_COMMAND(FRHICommandEndFrame)();
	GDynamicRHI->RHIAdvanceFrameFence();

	if (!IsRunningRHIInSeparateThread())
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndFrame_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, EndFrame);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	else
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}
}

void FRHIComputeCommandList::Transition(TArrayView<const FRHITransitionInfo> Infos)
{
	const ERHIPipeline Pipeline = GetPipeline();

	if (Bypass())
	{
		// Stack allocate the transition
		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);
		FRHITransition* Transition = new (MemStack.Alloc(FRHITransition::GetTotalAllocationSize(), FRHITransition::GetAlignment())) FRHITransition(Pipeline, Pipeline);
		GDynamicRHI->RHICreateTransition(Transition, FRHITransitionCreateInfo(Pipeline, Pipeline, ERHITransitionCreateFlags::NoSplit, Infos));

		GetComputeContext().RHIBeginTransitions(MakeArrayView((const FRHITransition**)&Transition, 1));
		GetComputeContext().RHIEndTransitions(MakeArrayView((const FRHITransition**)&Transition, 1));

		// Manual release
		GDynamicRHI->RHIReleaseTransition(Transition);
		Transition->~FRHITransition();
	}
	else
	{
		// Allocate the transition in the command list
		FRHITransition* Transition = new (Alloc(FRHITransition::GetTotalAllocationSize(), FRHITransition::GetAlignment())) FRHITransition(Pipeline, Pipeline);
		GDynamicRHI->RHICreateTransition(Transition, FRHITransitionCreateInfo(Pipeline, Pipeline, ERHITransitionCreateFlags::NoSplit, Infos));

		ALLOC_COMMAND(FRHICommandResourceTransition)(Transition);
	}

	for (const FRHITransitionInfo& Info : Infos)
	{
		ensureMsgf(Info.IsWholeResource(), TEXT("The Transition method only supports whole resource transitions."));

		if (FRHIViewableResource* Resource = GetViewableResource(Info))
		{
			SetTrackedAccess({ FRHITrackedAccessInfo(Resource, Info.AccessAfter) });
		}
	}
}

#if RHI_RAYTRACING
void FRHIComputeCommandList::BuildAccelerationStructure(FRHIRayTracingGeometry* Geometry)
{
	FRayTracingGeometryBuildParams Params;
	Params.Geometry = Geometry;
	Params.BuildMode = EAccelerationStructureBuildMode::Build;

	FRHIBufferRange ScratchBufferRange{};
	
	FRHIResourceCreateInfo ScratchBufferCreateInfo(TEXT("RHIScratchBuffer"));
	ScratchBufferRange.Buffer = CreateBuffer(Geometry->GetSizeInfo().BuildScratchSize, BUF_StructuredBuffer | BUF_RayTracingScratch, 0, ERHIAccess::UAVCompute, ScratchBufferCreateInfo);

	BuildAccelerationStructures(MakeArrayView(&Params, 1), ScratchBufferRange);
}

void FRHIComputeCommandList::BuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params)
{
	uint64 TotalRequiredScratchMemorySize = 0;
	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		uint64 ScratchBufferRequiredSize = P.BuildMode == EAccelerationStructureBuildMode::Update ? P.Geometry->GetSizeInfo().UpdateScratchSize : P.Geometry->GetSizeInfo().BuildScratchSize;
		TotalRequiredScratchMemorySize += ScratchBufferRequiredSize;
	}

	FRHIResourceCreateInfo ScratchBufferCreateInfo(TEXT("RHIScratchBuffer"));
	FRHIBufferRange ScratchBufferRange{};	
	ScratchBufferRange.Buffer = CreateBuffer(TotalRequiredScratchMemorySize, BUF_StructuredBuffer | BUF_RayTracingScratch, 0, ERHIAccess::UAVCompute, ScratchBufferCreateInfo);

	BuildAccelerationStructures(Params, ScratchBufferRange);
}
#endif

static FLockTracker GLockTracker;

void* FDynamicRHI::RHILockBuffer(class FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockBuffer);

	void* Result;
	if (RHICmdList.IsTopOfPipe())
	{
		bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
		if (!bBuffer || LockMode != RLM_WriteOnly)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockBuffer_FlushAndLock);
			CSV_SCOPED_TIMING_STAT(RHITFlushes, LockBuffer_BottomOfPipe);

			FRHICommandListScopedFlushAndExecute Flush(RHICmdList.GetAsImmediate());
			Result = GDynamicRHI->LockBuffer_BottomOfPipe(RHICmdList, Buffer, Offset, SizeRHI, LockMode);
		}
		else
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockBuffer_Malloc);
			Result = FMemory::Malloc(SizeRHI, 16);
		}

		// Only use the lock tracker at the top of the pipe. There's no need to track locks
		// at the bottom of the pipe, and doing so would require a critical section.
		GLockTracker.Lock(Buffer, Result, Offset, SizeRHI, LockMode);
	}
	else
	{
		Result = GDynamicRHI->LockBuffer_BottomOfPipe(RHICmdList, Buffer, Offset, SizeRHI, LockMode);
	}

	check(Result);
	return Result;
}

void FDynamicRHI::RHIUnlockBuffer(class FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockBuffer_RenderThread);

	if (RHICmdList.IsTopOfPipe())
	{
		FLockTracker::FLockParams Params = GLockTracker.Unlock(Buffer);

		bool bBuffer = CVarRHICmdBufferWriteLocks.GetValueOnRenderThread() > 0;
		if (!bBuffer || Params.LockMode != RLM_WriteOnly)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockBuffer_FlushAndUnlock);
			CSV_SCOPED_TIMING_STAT(RHITFlushes, UnlockBuffer_BottomOfPipe);

			FRHICommandListScopedFlushAndExecute Flush(RHICmdList.GetAsImmediate());
			GDynamicRHI->UnlockBuffer_BottomOfPipe(RHICmdList, Buffer);
			GLockTracker.TotalMemoryOutstanding = 0;
		}
		else
		{
			RHICmdList.EnqueueLambda(TEXT("RHIUnlockBuffer"), [Buffer, Params](FRHICommandListBase& InRHICmdList)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateBuffer_Execute);
				void* Data = GDynamicRHI->LockBuffer_BottomOfPipe(InRHICmdList, Buffer, Params.Offset, Params.BufferSize, RLM_WriteOnly);
				{
					// If we spend a long time doing this memcpy, it means we got freshly allocated memory from the OS that has never been
					// initialized and is causing pagefault to bring zeroed pages into our process.
					TRACE_CPUPROFILER_EVENT_SCOPE(RHIUnlockBuffer_Memcpy);
					FMemory::Memcpy(Data, Params.Buffer, Params.BufferSize);
				}
				FMemory::Free(Params.Buffer);
				GDynamicRHI->UnlockBuffer_BottomOfPipe(InRHICmdList, Buffer);
			});
			RHICmdList.RHIThreadFence(true);
		}

		if (RHICmdList.IsImmediate() && GLockTracker.TotalMemoryOutstanding > uint32(CVarRHICmdMaxOutstandingMemoryBeforeFlush.GetValueOnRenderThread()) * 1024u)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockBuffer_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.GetAsImmediate().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); 
			GLockTracker.TotalMemoryOutstanding = 0;
		}
	}
	else
	{
		GDynamicRHI->UnlockBuffer_BottomOfPipe(RHICmdList, Buffer);
	}
}

void FDynamicRHI::RHIWriteGPUFence_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIGPUFence* Fence)
{
	if (RHICmdList.Bypass())
	{
		RHICmdList.GetComputeContext().RHIWriteGPUFence(Fence);
		return;
	}
	ALLOC_COMMAND_CL(RHICmdList, FRHICommandWriteGPUFence)(Fence);
}

void FDynamicRHI::RHIBeginRenderQuery_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIRenderQuery* RenderQueryRHI)
{
	if (RHICmdList.Bypass())
	{
		RHICmdList.GetContext().RHIBeginRenderQuery(RenderQueryRHI);
		return;
	}
	ALLOC_COMMAND_CL(RHICmdList, FRHICommandBeginRenderQuery)(RenderQueryRHI);
}

void FDynamicRHI::RHIEndRenderQuery_TopOfPipe(FRHICommandListBase& RHICmdList, FRHIRenderQuery* RenderQueryRHI)
{
	if (RHICmdList.Bypass())
	{
		RHICmdList.GetContext().RHIEndRenderQuery(RenderQueryRHI);
		return;
	}
	ALLOC_COMMAND_CL(RHICmdList, FRHICommandEndRenderQuery)(RenderQueryRHI);
}

// @todo-mattc-staging Default implementation
void* FDynamicRHI::RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI)
{
	check(false);
	return nullptr;
	//return GDynamicRHI->RHILockVertexBuffer(StagingBuffer->GetSourceBuffer(), Offset, SizeRHI, RLM_ReadOnly);
}
void FDynamicRHI::RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer)
{
	check(false);
	//GDynamicRHI->RHIUnlockVertexBuffer(StagingBuffer->GetSourceBuffer());
}

void* FDynamicRHI::LockStagingBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI)
{
	check(IsInRenderingThread());
	if (!Fence || !Fence->Poll() || Fence->NumPendingWriteCommands.GetValue() != 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockStagingBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockStagingBuffer_RenderThread);
		if (GRHISupportsMultithreading)
		{
			return GDynamicRHI->RHILockStagingBuffer(StagingBuffer, Fence, Offset, SizeRHI);
		}
		else
		{
			FScopedRHIThreadStaller StallRHIThread(RHICmdList);
			return GDynamicRHI->RHILockStagingBuffer(StagingBuffer, Fence, Offset, SizeRHI);
		}
	}
}

void FDynamicRHI::UnlockStagingBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIStagingBuffer* StagingBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockStagingBuffer_RenderThread);
	check(IsInRenderingThread());
	if (GRHISupportsMultithreading)
	{
		GDynamicRHI->RHIUnlockStagingBuffer(StagingBuffer);
	}
	else
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		GDynamicRHI->RHIUnlockStagingBuffer(StagingBuffer);
	}
}

FTexture2DRHIRef FDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_AsyncReallocateTexture2D_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, AsyncReallocateTexture2D_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
	return GDynamicRHI->RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

ETextureReallocationStatus FDynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	CSV_SCOPED_TIMING_STAT(RHITStalls, FinalizeAsyncReallocateTexture2D_RenderThread);
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

ETextureReallocationStatus FDynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	CSV_SCOPED_TIMING_STAT(RHITStalls, CancelAsyncReallocateTexture2D_RenderThread);
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FUpdateTexture3DData FDynamicRHI::RHIBeginUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInParallelRenderingThread());

	const int32 FormatSize = PixelFormatBlockBytes[Texture->GetFormat()];
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;

	SIZE_T MemorySize = static_cast<SIZE_T>(DepthPitch) * UpdateRegion.Depth;
	uint8* Data = (uint8*)FMemory::Malloc(MemorySize);	

	return FUpdateTexture3DData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, Data, MemorySize, GFrameNumberRenderThread);
}

void FDynamicRHI::RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInParallelRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber); 
	GDynamicRHI->RHIUpdateTexture3D(RHICmdList, UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

void FDynamicRHI::RHIEndMultiUpdateTexture3D(FRHICommandListBase& RHICmdList, TArray<FUpdateTexture3DData>& UpdateDataArray)
{
	for (int32 Idx = 0; Idx < UpdateDataArray.Num(); ++Idx)
	{
		GDynamicRHI->RHIEndUpdateTexture3D(RHICmdList, UpdateDataArray[Idx]);
	}
}

void* FDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush, uint64* OutLockedByteCount)
{
	if (bNeedsDefaultRHIFlush) 
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, LockTexture2D_RenderThread);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		return GDynamicRHI->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
	}
	CSV_SCOPED_TIMING_STAT(RHITStalls, LockTexture2D_RenderThread);
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
}

void FDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	if (bNeedsDefaultRHIFlush)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2D_Flush);
		CSV_SCOPED_TIMING_STAT(RHITFlushes, UnlockTexture2D_RenderThread);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
		return;
	}
	CSV_SCOPED_TIMING_STAT(RHITStalls, UnlockTexture2D_RenderThread);
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	GDynamicRHI->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
}

void* FDynamicRHI::LockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2DArray_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, LockTexture2DArray_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	return GDynamicRHI->RHILockTexture2DArray(Texture, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

void FDynamicRHI::UnlockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2DArray_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, UnlockTexture2DArray_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIUnlockTexture2DArray(Texture, ArrayIndex, MipIndex, bLockWithinMiptail);
}

FRHIShaderLibraryRef FDynamicRHI::RHICreateShaderLibrary_RenderThread(class FRHICommandListImmediate& RHICmdList, EShaderPlatform Platform, FString FilePath, FString Name)
{
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return GDynamicRHI->RHICreateShaderLibrary(Platform, FilePath, Name);
}

void* FDynamicRHI::RHILockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTextureCubeFace_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, RHILockTextureCubeFace_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	return GDynamicRHI->RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

void FDynamicRHI::RHIUnlockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTextureCubeFace_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, RHIUnlockTextureCubeFace_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
}

void FDynamicRHI::RHIMapStagingSurface_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 GPUIndex, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight)
{
	if (Fence == nullptr || !Fence->Poll() || Fence->NumPendingWriteCommands.GetValue() != 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_MapStagingSurface_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_MapStagingSurface_RenderThread);
		if (GRHISupportsMultithreading)
		{
			GDynamicRHI->RHIMapStagingSurface(Texture, Fence, OutData, OutWidth, OutHeight, GPUIndex != INDEX_NONE ? GPUIndex : RHICmdList.GetGPUMask().ToIndex());
		}
		else
		{
			FScopedRHIThreadStaller StallRHIThread(RHICmdList);
			GDynamicRHI->RHIMapStagingSurface(Texture, Fence, OutData, OutWidth, OutHeight, GPUIndex != INDEX_NONE ? GPUIndex : RHICmdList.GetGPUMask().ToIndex());
		}
	}
}

void FDynamicRHI::RHIUnmapStagingSurface_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 GPUIndex)
{
	if (GRHISupportsMultithreading)
	{
		GDynamicRHI->RHIUnmapStagingSurface(Texture, GPUIndex != INDEX_NONE ? GPUIndex : RHICmdList.GetGPUMask().ToIndex());
	}
	else
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		GDynamicRHI->RHIUnmapStagingSurface(Texture, GPUIndex != INDEX_NONE ? GPUIndex : RHICmdList.GetGPUMask().ToIndex());
	}
}

void FDynamicRHI::RHIReadSurfaceFloatData_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceFloatData_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, RHIReadSurfaceFloatData_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIReadSurfaceFloatData(Texture, Rect, OutData, CubeFace, ArrayIndex, MipIndex);
}

void FDynamicRHI::RHIReadSurfaceFloatData_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags Flags)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceFloatData_Flush);
	CSV_SCOPED_TIMING_STAT(RHITFlushes, RHIReadSurfaceFloatData_RenderThread);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIReadSurfaceFloatData(Texture, Rect, OutData, Flags);
}

void FRHICommandListBase::UpdateTextureReference(FRHITextureReference* TextureRef, FRHITexture* NewTexture)
{
	if (TextureRef == nullptr)
	{
		return;
	}

	EnqueueLambda(TEXT("UpdateTextureReference"), [TextureRef, NewTexture](FRHICommandListBase& RHICmdList)
	{
		GDynamicRHI->RHIUpdateTextureReference(RHICmdList, TextureRef, NewTexture);
	});

	RHIThreadFence(true);
}

void FRHICommandListImmediate::UpdateRHIResources(FRHIResourceUpdateInfo* UpdateInfos, int32 Num, bool bNeedReleaseRefs)
{
	if (this->Bypass())
	{
		FRHICommandUpdateRHIResources Cmd(UpdateInfos, Num, bNeedReleaseRefs);
		Cmd.Execute(*this);
	}
	else
	{
		const SIZE_T NumBytes = sizeof(FRHIResourceUpdateInfo) * Num;
		FRHIResourceUpdateInfo* LocalUpdateInfos = reinterpret_cast<FRHIResourceUpdateInfo*>(this->Alloc(NumBytes, alignof(FRHIResourceUpdateInfo)));
		FMemory::Memcpy(LocalUpdateInfos, UpdateInfos, NumBytes);
		new (AllocCommand<FRHICommandUpdateRHIResources>()) FRHICommandUpdateRHIResources(LocalUpdateInfos, Num, bNeedReleaseRefs);
		RHIThreadFence(true);
		if (GetUsedMemory() > 256 * 1024)
		{
			// we could be loading a level or something, lets get this stuff going
			ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}
}

void FRHICommandListImmediate::CleanupGraphEvents()
{
	WaitOutstandingTasks.Reset();
	RHIThreadTask.SafeRelease();

	for (FGraphEventRef& GraphEvent : GRHIThreadEndDrawingViewportFences)
	{
		GraphEvent.SafeRelease();
	}
}
