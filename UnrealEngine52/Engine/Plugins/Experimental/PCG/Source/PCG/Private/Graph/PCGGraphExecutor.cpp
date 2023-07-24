// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutor.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCrc.h"
#include "PCGGraph.h"
#include "PCGGraphCompiler.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Graph/PCGGraphCache.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Algo/AnyOf.h"
#include "Async/Async.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Editor.h"
#else
#include "GameFramework/Actor.h"
#endif

// World partition support for in-editor workflows needs these includes
#if WITH_EDITOR
#include "FileHelpers.h"
#endif

static TAutoConsoleVariable<int32> CVarMaxNumTasks(
	TEXT("pcg.MaxConcurrentTasks"),
	4096,
	TEXT("Maximum number of concurrent tasks for PCG processing"));

static TAutoConsoleVariable<float> CVarMaxPercentageOfThreadsToUse(
	TEXT("pcg.MaxPercentageOfThreadsToUse"),
	0.9f,
	TEXT("Maximum percentage of number of threads for concurrent PCG processing"));

static TAutoConsoleVariable<float> CVarTimePerFrame(
	TEXT("pcg.FrameTime"),
	1000.0f / 60.0f,
	TEXT("Allocated time in ms per frame"));

static TAutoConsoleVariable<float> CVarEditorTimePerFrame(
	TEXT("pcg.EditorFrameTime"),
	1000.0f / 20.0f,
	TEXT("Allocated time in ms per frame when running in editor (non pie)"));

static TAutoConsoleVariable<bool> CVarGraphMultithreading(
	TEXT("pcg.GraphMultithreading"),
	false,
	TEXT("Controls whether the graph can dispatch multiple tasks at the same time"));

FPCGGraphExecutor::FPCGGraphExecutor(UObject* InOwner)
	: GraphCompiler(MakeUnique<FPCGGraphCompiler>())
	, GraphCache(InOwner, &DataRootSet)
#if WITH_EDITOR
	, GenerationProgressNotification(GetNotificationTextFormat())
#endif
{
}

FPCGGraphExecutor::~FPCGGraphExecutor()
{
	// We don't really need to do this here (it would be done in the destructor of these both)
	// but this is to clarify/ensure the order in which this happens
	GraphCache.ClearCache();
	DataRootSet.Clear();

#if WITH_EDITOR
	// Cleanup + clear notification
	ClearAllTasks();
	UpdateGenerationNotification();
#endif
}

void FPCGGraphExecutor::Compile(UPCGGraph* Graph)
{
	GraphCompiler->Compile(Graph);
}

FPCGTaskId FPCGGraphExecutor::Schedule(UPCGComponent* Component, const TArray<FPCGTaskId>& ExternalDependencies)
{
	check(Component);
	UPCGGraph* Graph = Component->GetGraph();

	return Schedule(Graph, Component, GetFetchInputElement(), ExternalDependencies);
}

FPCGTaskId FPCGGraphExecutor::Schedule(UPCGGraph* Graph, UPCGComponent* SourceComponent, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& ExternalDependencies)
{
	if (SourceComponent && IsGraphCacheDebuggingEnabled())
	{
		UE_LOG(LogPCG, Log, TEXT("[%s] --- SCHEDULE GRAPH ---"), *SourceComponent->GetOwner()->GetName());
	}

#if WITH_EDITOR
	if (UPCGSubsystem* Subsystem = SourceComponent ? UPCGSubsystem::GetInstance(SourceComponent->GetWorld()) : nullptr)
	{
		for (const UPCGNode* Node : Graph->GetNodes())
		{
			// Always clear warnings/errors before compile regardless of connectivity
			Subsystem->GetNodeVisualLogsMutable().ClearLogs(Node, SourceComponent);
		}
	}
#endif

	FPCGTaskId ScheduledId = InvalidPCGTaskId;

	// Get compiled tasks from compiler
	TArray<FPCGGraphTask> CompiledTasks = GraphCompiler->GetCompiledTasks(Graph);

	// Assign this component to the tasks
	for (FPCGGraphTask& Task : CompiledTasks)
	{
		Task.SourceComponent = SourceComponent;
	}

	// Prepare scheduled task that will be promoted in the next Execute call.
	if (CompiledTasks.Num() > 0)
	{ 
		check(CompiledTasks[0].Node == Graph->GetInputNode());

		// Setup fetch task on input node
		CompiledTasks[0].Element = InputElement;

		ScheduleLock.Lock();

		FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks.Emplace_GetRef();
		ScheduledTask.Tasks = MoveTemp(CompiledTasks);
		ScheduledTask.SourceComponent = SourceComponent;

		// Offset task node ids
		FPCGGraphCompiler::OffsetNodeIds(ScheduledTask.Tasks, NextTaskId, InvalidPCGTaskId);
		NextTaskId += ScheduledTask.Tasks.Num();
		ScheduledId = NextTaskId - 1; // This is true because the last task is from the output node or is the post-execute task

		// Push task (not data) dependencies on the pre-execute task
		// Note must be done after the offset ids, otherwise we'll break the dependencies
		check(ScheduledTask.Tasks.Num() >= 2 && ScheduledTask.Tasks[ScheduledTask.Tasks.Num() - 2].Node == nullptr);
		for (FPCGTaskId ExternalDependency : ExternalDependencies)
		{
			// For the pre-task, we don't consume any input
			ScheduledTask.Tasks[ScheduledTask.Tasks.Num() - 2].Inputs.Emplace(ExternalDependency, nullptr, nullptr, /*bConsumeData=*/false);
		}

		ScheduledTask.FirstTaskIndex = ScheduledTask.Tasks.Num() - 2;
		ScheduledTask.LastTaskIndex = ScheduledTask.Tasks.Num() - 1;

		ScheduleLock.Unlock();
	}

	return ScheduledId;
}

TArray<UPCGComponent*> FPCGGraphExecutor::Cancel(UPCGComponent* InComponent)
{
	auto CancelIfSameComponent = [InComponent](TWeakObjectPtr<UPCGComponent> Component)
	{
		return Component.IsValid() && InComponent == Component;
	};

	return Cancel(CancelIfSameComponent).Array();
}

TArray<UPCGComponent*> FPCGGraphExecutor::Cancel(UPCGGraph* InGraph)
{
	auto CancelIfComponentUsesGraph = [InGraph](TWeakObjectPtr<UPCGComponent> Component)
	{
		return (Component.IsValid() && Component->GetGraph() == InGraph);
	};

	return Cancel(CancelIfComponentUsesGraph).Array();
}

TArray<UPCGComponent*> FPCGGraphExecutor::CancelAll()
{
	auto CancelAllGeneration = [](TWeakObjectPtr<UPCGComponent> Component)
	{
		return Component.IsValid();
	};

	return Cancel(CancelAllGeneration).Array();
}

TSet<UPCGComponent*> FPCGGraphExecutor::Cancel(TFunctionRef<bool(TWeakObjectPtr<UPCGComponent>)> CancelFilter)
{
	TSet<UPCGComponent*> CancelledComponents;

	// Visit scheduled tasks
	ScheduleLock.Lock();
	for (const FPCGGraphScheduleTask& ScheduledTask : ScheduledTasks)
	{
		if (CancelFilter(ScheduledTask.SourceComponent))
		{
			CancelledComponents.Add(ScheduledTask.SourceComponent.Get());
		}
	}
	ScheduleLock.Unlock();

	// Visit ready tasks
	for (const FPCGGraphTask& Task : ReadyTasks)
	{
		if (CancelFilter(Task.SourceComponent))
		{
			CancelledComponents.Add(Task.SourceComponent.Get());
		}
	}

	// Visit active tasks
	for (const FPCGGraphActiveTask& Task : ActiveTasks)
	{
		if (Task.Context && CancelFilter(Task.Context->SourceComponent))
		{
			CancelledComponents.Add(Task.Context->SourceComponent.Get());
		}
	}

	// Visit sleeping tasks
	for (const FPCGGraphActiveTask& Task : SleepingTasks)
	{
		if (Task.Context && CancelFilter(Task.Context->SourceComponent))
		{
			CancelledComponents.Add(Task.Context->SourceComponent.Get());
		}
	}

	// Early out - nothing to cancel
	if (CancelledComponents.IsEmpty())
	{
		return CancelledComponents;
	}

	TArray<FPCGTaskId> CancelledScheduledTasks;

	bool bStableCancellationSet = false;
	while (!bStableCancellationSet)
	{
		bStableCancellationSet = true;

		// Remove from scheduled tasks
		ScheduleLock.Lock();
		for (int32 ScheduledTaskIndex = ScheduledTasks.Num() - 1; ScheduledTaskIndex >= 0; --ScheduledTaskIndex)
		{
			FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks[ScheduledTaskIndex];
			if (CancelledComponents.Contains(ScheduledTask.SourceComponent.Get()))
			{
				CancelledScheduledTasks.Add(ScheduledTask.Tasks[ScheduledTask.LastTaskIndex].NodeId);
				ScheduledTasks.RemoveAtSwap(ScheduledTaskIndex);
			}
		}

		auto RemoveScheduledTasks = [this, &bStableCancellationSet, &CancelledComponents, &CancelledScheduledTasks](FPCGTaskId EndTaskId)
		{

		};

		// WARNING: variable upper bound
		for (int32 CancelledTaskIdIndex = 0; CancelledTaskIdIndex < CancelledScheduledTasks.Num(); ++CancelledTaskIdIndex)
		{
			const FPCGTaskId EndTaskId = CancelledScheduledTasks[CancelledTaskIdIndex];
			for (int32 ScheduledTaskIndex = ScheduledTasks.Num() - 1; ScheduledTaskIndex >= 0; --ScheduledTaskIndex)
			{
				FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks[ScheduledTaskIndex];
				const bool bContainsDependency = Algo::AnyOf(ScheduledTask.Tasks[ScheduledTask.FirstTaskIndex].Inputs, [EndTaskId](const FPCGGraphTaskInput& Input) { return Input.TaskId == EndTaskId; });

				if (bContainsDependency)
				{
					if (!CancelledComponents.Contains(ScheduledTask.SourceComponent.Get()))
					{
						CancelledComponents.Add(ScheduledTask.SourceComponent.Get());
						bStableCancellationSet = false;
					}

					CancelledScheduledTasks.Add(ScheduledTask.Tasks[ScheduledTask.LastTaskIndex].NodeId);
					ScheduledTasks.RemoveAtSwap(ScheduledTaskIndex);
				}
			}
		}

		CancelledScheduledTasks.Reset();
		ScheduleLock.Unlock();

		// Remove from ready tasks
		for (int32 ReadyTaskIndex = ReadyTasks.Num() - 1; ReadyTaskIndex >= 0; --ReadyTaskIndex)
		{
			FPCGGraphTask& Task = ReadyTasks[ReadyTaskIndex];

			if (CancelledComponents.Contains(Task.SourceComponent.Get()))
			{
				FPCGTaskId CancelledTaskId = Task.NodeId;
				delete Task.Context;
				ReadyTasks.RemoveAtSwap(ReadyTaskIndex);
				bStableCancellationSet &= !CancelNextTasks(CancelledTaskId, CancelledComponents);
			}
		}

		// Mark as cancelled in the active tasks - needed to make sure we're not breaking the current execution (if any)
		for (int32 ActiveTaskIndex = ActiveTasks.Num() - 1; ActiveTaskIndex >= 0; --ActiveTaskIndex)
		{
			FPCGGraphActiveTask& Task = ActiveTasks[ActiveTaskIndex];
			if(Task.Context && CancelledComponents.Contains(Task.Context->SourceComponent.Get()))
			{
				FPCGTaskId CancelledTaskId = Task.NodeId;
				Task.bWasCancelled = true;
				bStableCancellationSet &= !CancelNextTasks(CancelledTaskId, CancelledComponents);
			}
		}

		// Remove from sleeping tasks
		for (int32 SleepingTaskIndex = SleepingTasks.Num() - 1; SleepingTaskIndex >= 0; --SleepingTaskIndex)
		{
			FPCGGraphActiveTask& Task = SleepingTasks[SleepingTaskIndex];
			if(Task.Context && CancelledComponents.Contains(Task.Context->SourceComponent.Get()))
			{
				FPCGTaskId CancelledTaskId = Task.NodeId;
				SleepingTasks.RemoveAtSwap(CancelledTaskId);
				bStableCancellationSet &= !CancelNextTasks(CancelledTaskId, CancelledComponents);
			}
		}
	}

	// Finally, update the notification so it shows the new information
#if WITH_EDITOR
	UpdateGenerationNotification();
#endif

	return CancelledComponents;
}

bool FPCGGraphExecutor::IsGraphCurrentlyExecuting(UPCGGraph* InGraph)
{
	bool bAnyPresent = false;

	// This makes use of the Cancel function which runs over all tasks, but it always returns false so no tasks are cancelled.
	auto CheckIfAnyGraphTasksPresent = [InGraph, &bAnyPresent](TWeakObjectPtr<UPCGComponent> Component)
	{
		bAnyPresent |= Component.IsValid() && Component->GetGraph() == InGraph;
		return false;
	};

	Cancel(CheckIfAnyGraphTasksPresent);

	return bAnyPresent;
}

FPCGTaskId FPCGGraphExecutor::ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskDependencies)
{
	// Since we have no context, the generic task will consume no input
	constexpr bool bConsumeInputData = false;
	return ScheduleGenericWithContext([Operation = MoveTemp(InOperation)](FPCGContext*) -> bool
		{
			return Operation();
		}, InSourceComponent, TaskDependencies, bConsumeInputData);
}

FPCGTaskId FPCGGraphExecutor::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskDependencies, bool bConsumeInputData)
{
	// Build task & element to hold the operation to perform
	FPCGGraphTask Task;
	for (FPCGTaskId TaskDependency : TaskDependencies)
	{
		Task.Inputs.Emplace(TaskDependency, nullptr, nullptr, bConsumeInputData);
	}

	Task.SourceComponent = InSourceComponent;
	Task.Element = MakeShared<FPCGGenericElement>(InOperation);

	ScheduleLock.Lock();

	// Assign task id
	Task.NodeId = NextTaskId++;

	FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks.Emplace_GetRef();
	ScheduledTask.Tasks.Add(Task);
	ScheduledTask.SourceComponent = InSourceComponent;

	ScheduleLock.Unlock();

	return Task.NodeId;
}

bool FPCGGraphExecutor::GetOutputData(FPCGTaskId TaskId, FPCGDataCollection& OutData)
{
	// TODO: this is not threadsafe - make threadsafe once we multithread execution
	if (OutputData.Contains(TaskId))
	{
		OutData = OutputData[TaskId];
		return true;
	}
	else
	{
		return false;
	}
}

void FPCGGraphExecutor::Execute()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute);

	// Process any newly scheduled graphs to execute
	ScheduleLock.Lock();

	for(int32 ScheduledTaskIndex = ScheduledTasks.Num() - 1; ScheduledTaskIndex >= 0; --ScheduledTaskIndex)
	{
		FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks[ScheduledTaskIndex];

		check(ScheduledTask.Tasks.Num() > 0);
		// Push tasks to the primary task list & build successors map
		for (FPCGGraphTask& Task : ScheduledTask.Tasks)
		{
			FPCGTaskId TaskId = Task.NodeId;

			// TODO: review if it's actually possible for a task with inputs to be ready at this point - it seems very unlikely
			bool bPushToReady = true;
			for (const FPCGGraphTaskInput& Input : Task.Inputs)
			{
				if (!OutputData.Contains(Input.TaskId))
				{
					TaskSuccessors.FindOrAdd(Input.TaskId).Add(TaskId);
					bPushToReady = false;
				}
			}

			// Automatically push inputless/already satisfied tasks to the ready queue
			if (bPushToReady)
			{
				ReadyTasks.Emplace(MoveTemp(Task));
			}
			else
			{
				Tasks.Add(TaskId, MoveTemp(Task));
			}
		}
	}

	ScheduledTasks.Reset();

	ScheduleLock.Unlock();

	// TODO: add optimization phase if we've added new graph(s)/tasks to be executed

	// This is a safeguard to check if we're in a stuck state
	if (ReadyTasks.Num() == 0 && ActiveTasks.Num() == 0 && SleepingTasks.Num() == 0 && Tasks.Num() > 0)
	{
		UE_LOG(LogPCG, Error, TEXT("PCG Graph executor error: tasks are in a deadlocked state. Will drop all tasks."));
		ClearAllTasks();
	}

	// TODO: change this if we support tasks that are not framebound
	bool bAnyTaskEnded = false;
	bool bHasAlreadyCheckedSleepingTasks = false;

	const double StartTime = FPlatformTime::Seconds();

	double VarTimePerFrame = CVarTimePerFrame.GetValueOnAnyThread() / 1000.0;

#if WITH_EDITOR
	if (GEditor && !GEditor->IsPlaySessionInProgress())
	{
		VarTimePerFrame = CVarEditorTimePerFrame.GetValueOnAnyThread() / 1000.0;
	}
#endif

	const double EndTime = StartTime + VarTimePerFrame;
	const float MaxPercentageOfThreadsToUse = FMath::Clamp(CVarMaxPercentageOfThreadsToUse.GetValueOnAnyThread(), 0.0f, 1.0f);
	const int32 MaxNumThreads = FMath::Max(0, FMath::Min((int32)(FPlatformMisc::NumberOfCoresIncludingHyperthreads() * MaxPercentageOfThreadsToUse), CVarMaxNumTasks.GetValueOnAnyThread() - 1));
	const bool bAllowMultiDispatch = CVarGraphMultithreading.GetValueOnAnyThread();
	const bool bGraphCacheDebuggingEnabled = IsGraphCacheDebuggingEnabled();

#if WITH_EDITOR
	UpdateGenerationNotification();
#endif

	while(ReadyTasks.Num() > 0 || ActiveTasks.Num() > 0 || (!bHasAlreadyCheckedSleepingTasks && SleepingTasks.Num() > 0))
	{
		// If we only have sleeping tasks, we will go through this loop only once. If all tasks are still sleeping after one iteration,
		// we will wake them up only at next tick. It will avoid spinning for our whole frametime budget.
		bHasAlreadyCheckedSleepingTasks = ReadyTasks.Num() == 0 && ActiveTasks.Num() == 0 && SleepingTasks.Num() > 0;

		// First: if we have free resources, move ready tasks to the active tasks
		bool bMainThreadAvailable = (ActiveTasks.Num() == 0 || !ActiveTasks[0].Element->CanExecuteOnlyOnMainThread(ActiveTasks[0].Context.Get()));
		int32 NumAvailableThreads = FMath::Max(0, MaxNumThreads - CurrentlyUsedThreads);

		const bool bMainThreadWasAvailable = bMainThreadAvailable;
		const int32 TasksAlreadyLaunchedCount = ActiveTasks.Num();

		bool bHasDispatchedTasks = !ActiveTasks.IsEmpty();

		auto CannotDispatchMoreTasks = [&bMainThreadAvailable, &NumAvailableThreads, &bHasDispatchedTasks, bAllowMultiDispatch]
		{
			return ((!bAllowMultiDispatch && bHasDispatchedTasks) || (!bMainThreadAvailable && NumAvailableThreads == 0));
		};

		if(!CannotDispatchMoreTasks())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::PrepareTasks);
			// Sort tasks by priority (highest priority should be at the end)
			// TODO
			auto TaskDispatchBookKeeping = [&bMainThreadAvailable, &NumAvailableThreads, &bHasDispatchedTasks](bool bIsMainThreadTask)
			{
				bHasDispatchedTasks = true;

				if (bIsMainThreadTask || NumAvailableThreads == 0)
				{
					bMainThreadAvailable = false;
				}
				else
				{
					--NumAvailableThreads;
				}
			};

			// First, wake up any sleeping tasks that can be reactivated
			for (int32 SleepingTaskIndex = SleepingTasks.Num() - 1; SleepingTaskIndex >= 0; --SleepingTaskIndex)
			{
				if (CannotDispatchMoreTasks())
				{
					break;
				}

				FPCGGraphActiveTask& SleepingTask = SleepingTasks[SleepingTaskIndex];

				if (SleepingTask.Context->bIsPaused)
				{
					continue; // still sleeping
				}

				// Validate that we can start this task now
				const bool bIsMainThreadTask = SleepingTask.Element->CanExecuteOnlyOnMainThread(SleepingTask.Context.Get());

				if (!bIsMainThreadTask || bMainThreadAvailable)
				{
					ActiveTasks.Emplace(MoveTemp(SleepingTask));

					// Move task up front if it has to run on the main thread
					if (bIsMainThreadTask && ActiveTasks.Num() > 1)
					{
						ActiveTasks.Swap(0, ActiveTasks.Num() - 1);
					}

					TaskDispatchBookKeeping(bIsMainThreadTask);

					SleepingTasks.RemoveAtSwap(SleepingTaskIndex);
				}
			}

			for(int32 ReadyTaskIndex = ReadyTasks.Num() - 1; ReadyTaskIndex >= 0; --ReadyTaskIndex)
			{
				if (CannotDispatchMoreTasks())
				{
					break;
				}

				FPCGGraphTask& Task = ReadyTasks[ReadyTaskIndex];

				// Build input
				FPCGDataCollection TaskInput;
				BuildTaskInput(Task, TaskInput);

				// Initialize the element if needed (required to know whether it will run on the main thread or not)
				if (!Task.Element)
				{
					// Get appropriate settings
					check(Task.Node);
					const UPCGSettings* Settings = TaskInput.GetSettings(Task.Node->GetSettings());

					if (Settings)
					{
						Task.Element = Settings->GetElement();
					}
				}

				// At this point, if the task doesn't have an element, we will never be able to execute it, so we can drop it.
				if (!Task.Element)
				{
					check(!Task.Context);
					ReadyTasks.RemoveAtSwap(ReadyTaskIndex);
					continue;
				}

				// If a task is cacheable and has been cached, then we don't need to create an active task for it unless
				// there is an execution mode that would prevent us from doing so.
				const UPCGSettingsInterface* TaskSettingsInterface = TaskInput.GetSettingsInterface(Task.Node ? Task.Node->GetSettingsInterface() : nullptr);
				const UPCGSettings* TaskSettings = TaskSettingsInterface ? TaskSettingsInterface->GetSettings() : nullptr;
				const bool bCacheable = Task.Element->IsCacheableInstance(TaskSettingsInterface);

				// Calculate Crc of dependencies (input data Crcs, settings) and use this as the key in the cache lookup
				FPCGCrc DependenciesCrc;
				if (TaskSettings && bCacheable)
				{
					Task.Element->GetDependenciesCrc(TaskInput, TaskSettings, Task.SourceComponent.Get(), DependenciesCrc);
				}

				if (bGraphCacheDebuggingEnabled && !bCacheable && Task.SourceComponent.Get() && Task.Node)
				{
					UE_LOG(LogPCG, Warning, TEXT("[%s] %s\t\tCACHING DISABLED"), *Task.SourceComponent->GetOwner()->GetName(), *Task.Node->GetNodeTitle().ToString());
				}

				FPCGDataCollection CachedOutput;
				const bool bResultAlreadyInCache = bCacheable && DependenciesCrc.IsValid() && GraphCache.GetFromCache(Task.Node, Task.Element.Get(), DependenciesCrc, TaskInput, TaskSettings, Task.SourceComponent.Get(), CachedOutput);
#if WITH_EDITOR
				const bool bNeedsToCreateActiveTask = !bResultAlreadyInCache || TaskSettingsInterface->bDebug;
#else
				const bool bNeedsToCreateActiveTask = !bResultAlreadyInCache;
#endif

				if (!bNeedsToCreateActiveTask)
				{
#if WITH_EDITOR
					// doing this now since we're about to modify ReadyTasks potentially reallocating while Task is a reference. 
					UPCGComponent* SourceComponent = Task.SourceComponent.Get();
					if (SourceComponent && SourceComponent->IsInspecting())
					{
						SourceComponent->StoreInspectionData(Task.Node, CachedOutput);
					}
#endif

					// Fast-forward cached result to stored results
					FPCGTaskId SkippedTaskId = Task.NodeId;
					StoreResults(SkippedTaskId, CachedOutput);
					delete Task.Context;
					ReadyTasks.RemoveAtSwap(ReadyTaskIndex);
					QueueNextTasks(SkippedTaskId);
					bAnyTaskEnded = true;

					continue;
				}

				// Allocate context if not previously done
				if (!Task.Context)
				{
					Task.Context = Task.Element->Initialize(TaskInput, Task.SourceComponent, Task.Node);
					Task.Context->InitializeSettings();
					Task.Context->TaskId = Task.NodeId;
					Task.Context->CompiledTaskId = Task.CompiledTaskId;
					Task.Context->InputData.Crc = TaskInput.Crc;
					Task.Context->DependenciesCrc = DependenciesCrc;
				}

				// Validate that we can start this task now
				const bool bIsMainThreadTask = Task.Element->CanExecuteOnlyOnMainThread(Task.Context);

				if (!bIsMainThreadTask || bMainThreadAvailable)
				{
					FPCGGraphActiveTask& ActiveTask = ActiveTasks.Emplace_GetRef();
					ActiveTask.Element = Task.Element;
					ActiveTask.NodeId = Task.NodeId;
					ActiveTask.Context = TUniquePtr<FPCGContext>(Task.Context);

#if WITH_EDITOR
					if (bResultAlreadyInCache)
					{
						ActiveTask.bIsBypassed = true;
						ActiveTask.Context->OutputData = CachedOutput;
						// Since we've copied the output data, we need to make sure to add count ref in the root set since it'll be removed once the task is executed
						ActiveTask.Context->OutputData.AddToRootSet(DataRootSet);
					}
#endif

					// Move the task up front if it needs to run on the main thread
					if (bIsMainThreadTask && ActiveTasks.Num() > 1)
					{
						ActiveTasks.Swap(0, ActiveTasks.Num() - 1);
					}

					TaskDispatchBookKeeping(bIsMainThreadTask);

					ReadyTasks.RemoveAtSwap(ReadyTaskIndex);
				}
			}
		}

		check(NumAvailableThreads >= 0);

		const int32 NumTasksToLaunch = ActiveTasks.Num() - TasksAlreadyLaunchedCount;

		// Assign resources
		if (NumTasksToLaunch > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::AssignResources);
			const int32 NumAdditionalThreads = ((NumTasksToLaunch > 0) ? (NumAvailableThreads / NumTasksToLaunch) : 0);
			check(NumAdditionalThreads >= 0);

			for (int32 ExecutionIndex = 0; ExecutionIndex < ActiveTasks.Num(); ++ExecutionIndex)
			{
				FPCGGraphActiveTask& ActiveTask = ActiveTasks[ExecutionIndex];

				// Tasks that were already launched already have assigned tasks, so don't touch them
				if (ActiveTask.Context->AsyncState.NumAvailableTasks == 0)
				{
					ActiveTask.Context->AsyncState.NumAvailableTasks = 1 + NumAdditionalThreads;
					CurrentlyUsedThreads += ActiveTask.Context->AsyncState.NumAvailableTasks;
				}
			}
		}

		// Dispatch async tasks
		TMap<int32, TFuture<bool>> Futures;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::StartFutures);
			for(int32 ExecutionIndex = 1; ExecutionIndex < ActiveTasks.Num(); ++ExecutionIndex)
			{
				FPCGGraphActiveTask& ActiveTask = ActiveTasks[ExecutionIndex];
				check(!ActiveTask.Context->bIsPaused);

	#if WITH_EDITOR
				if(!ActiveTask.bIsBypassed)
	#endif
				{
					check(!ActiveTask.Element->CanExecuteOnlyOnMainThread(ActiveTask.Context.Get()));
					ActiveTask.Context->AsyncState.EndTime = EndTime;
					ActiveTask.Context->AsyncState.bIsRunningOnMainThread = false;
					// Remove the precreated data so we properly count in the root set
					ActiveTask.Context->OutputData.RemoveFromRootSet(DataRootSet);

					Futures.Emplace(ExecutionIndex, Async(EAsyncExecution::ThreadPool, [&ActiveTask]()
					{
						return ActiveTask.Element->Execute(ActiveTask.Context.Get());
					}));
				}
			}
		}

		auto PostTaskExecute = [this, &bAnyTaskEnded, bGraphCacheDebuggingEnabled](int32 TaskIndex)
		{
			FPCGGraphActiveTask& ActiveTask = ActiveTasks[TaskIndex];

#if WITH_EDITOR
			if (!ActiveTask.bWasCancelled && !ActiveTask.bIsBypassed)
#else
			if (!ActiveTask.bWasCancelled)
#endif
			{
				if (bGraphCacheDebuggingEnabled && ActiveTask.Context->SourceComponent.Get() && ActiveTask.Context->Node)
				{
					UE_LOG(LogPCG, Log, TEXT("         [%s] %s\t\tOUTPUT CRC %u"), *ActiveTask.Context->SourceComponent->GetOwner()->GetName(), *ActiveTask.Context->Node->GetNodeTitle().ToString(), ActiveTask.Context->OutputData.Crc.GetValue());
				}

				// Store result in cache as needed - done here because it needs to be done on the main thread
				const UPCGSettingsInterface* ActiveTaskSettingsInterface = ActiveTask.Context->GetInputSettingsInterface();

				// Don't store if errors or warnings present
#if WITH_EDITOR
				const bool bHasErrorOrWarning = ActiveTask.Context->Node && (ActiveTask.Context->HasVisualLogs());
#else
				const bool bHasErrorOrWarning = false;
#endif

				if (ActiveTaskSettingsInterface && !bHasErrorOrWarning && ActiveTask.Element->IsCacheableInstance(ActiveTaskSettingsInterface))
				{
					const UPCGSettings* ActiveTaskSettings = ActiveTaskSettingsInterface ? ActiveTaskSettingsInterface->GetSettings() : nullptr;
					GraphCache.StoreInCache(ActiveTask.Element.Get(), ActiveTask.Context->DependenciesCrc, ActiveTask.Context->InputData, ActiveTaskSettings, ActiveTask.Context->SourceComponent.Get(), ActiveTask.Context->OutputData);
				}
			}

			check(ActiveTask.Context->AsyncState.NumAvailableTasks >= 0);
			CurrentlyUsedThreads -= ActiveTask.Context->AsyncState.NumAvailableTasks;

#if WITH_EDITOR
			// Execute debug display code as needed - done here because it needs to be done on the main thread
			// Additional note: this needs to be executed before the StoreResults since debugging might cancel further tasks
			ActiveTask.Element->DebugDisplay(ActiveTask.Context.Get());

			UPCGComponent* SourceComponent = ActiveTask.Context->SourceComponent.Get();
			if (SourceComponent && SourceComponent->IsInspecting())
			{
				SourceComponent->StoreInspectionData(ActiveTask.Context->Node, ActiveTask.Context->OutputData);
			}
#endif

			// Store output in data map
			StoreResults(ActiveTask.NodeId, ActiveTask.Context->OutputData);

			// Book-keeping
			QueueNextTasks(ActiveTask.NodeId);
			bAnyTaskEnded = true;

			// Un-root temporary input data
			if (FPCGDataCollection* TemporaryInput = InputTemporaryData.Find(ActiveTask.NodeId))
			{
				TemporaryInput->RemoveFromRootSet(DataRootSet);
				InputTemporaryData.Remove(ActiveTask.NodeId);
			}

			// Remove current active task from list
			ActiveTasks.RemoveAtSwap(TaskIndex);
		};

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::ExecuteTasks);
			bool bMainTaskDone = false;
			// Execute main thread task
			if (!ActiveTasks.IsEmpty())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::ExecuteTasks::MainThreadTask);
				FPCGGraphActiveTask& MainThreadTask = ActiveTasks[0];
				check(!MainThreadTask.Context->bIsPaused);
				MainThreadTask.Context->AsyncState.EndTime = EndTime;
				MainThreadTask.Context->AsyncState.bIsRunningOnMainThread = true;
				// Remove the precreated data so we properly count in the root set
				MainThreadTask.Context->OutputData.RemoveFromRootSet(DataRootSet);

#if WITH_EDITOR
				if(MainThreadTask.bIsBypassed || MainThreadTask.bWasCancelled || MainThreadTask.Element->Execute(MainThreadTask.Context.Get()))
#else
				if(MainThreadTask.bWasCancelled || MainThreadTask.Element->Execute(MainThreadTask.Context.Get()))
#endif
				{
					bMainTaskDone = true;
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::ExecuteTasks::WaitForFutures);

				// Then wait after all futures - start from the back so we can more easily manage the ActiveTasks array
				for(int32 ExecutionIndex = ActiveTasks.Num() - 1; ExecutionIndex > 0; --ExecutionIndex)
				{
					bool bTaskDone = false;
					// Wait on the future if any
					if (TFuture<bool>* Future = Futures.Find(ExecutionIndex))
					{
						Future->Wait();
						bTaskDone = Future->Get();
					}

					if (bTaskDone || ActiveTasks[ExecutionIndex].bWasCancelled)
					{
						PostTaskExecute(ExecutionIndex);
					}
					else
					{
						// Task isn't done, but we need to make sure the data isn't garbage collected
						ActiveTasks[ExecutionIndex].Context->OutputData.AddToRootSet(DataRootSet);
					}
				}
			}

			if (bMainTaskDone)
			{
				PostTaskExecute(0);
			}
			else if(!ActiveTasks.IsEmpty())
			{
				// Task isn't done, make sure we don't garbage collected the precreated data
				ActiveTasks[0].Context->OutputData.AddToRootSet(DataRootSet);
			}
		}

		// Any paused tasks at that point should relinquish their resources
		if (ActiveTasks.Num() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::ExecuteTasks::CheckSleepingTasks);
			for (int32 ActiveTaskIndex = ActiveTasks.Num() - 1; ActiveTaskIndex >= 0; --ActiveTaskIndex)
			{
				FPCGGraphActiveTask& ActiveTask = ActiveTasks[ActiveTaskIndex];
				
				// Any task that asks to be paused or now needs to run on the main thread but doesn't have that slot currently will be moved to the sleeping queue
				const bool bTaskShouldBePutAside = (ActiveTask.Context->bIsPaused || (ActiveTaskIndex > 0 && ActiveTask.Element->CanExecuteOnlyOnMainThread(ActiveTask.Context.Get())));

				if(bTaskShouldBePutAside)
				{
					check(ActiveTask.Context->AsyncState.NumAvailableTasks > 0);
					CurrentlyUsedThreads -= ActiveTask.Context->AsyncState.NumAvailableTasks;
					ActiveTask.Context->AsyncState.NumAvailableTasks = 0;

					SleepingTasks.Emplace(MoveTemp(ActiveTask));
					ActiveTasks.RemoveAtSwap(ActiveTaskIndex);
				}
			}
		}

		check(ActiveTasks.Num() > 0 || CurrentlyUsedThreads == 0);

		if (FPlatformTime::Seconds() >= EndTime)
		{
			break;
		}
	}

	if (bAnyTaskEnded)
	{
		// Nothing left to do; we'll release everything here.
		// TODO: this is fine and will make sure any intermediate data is properly
		// garbage collected, however, this goes a bit against our goals if we want to
		// keep a cache of intermediate results.
		if (ReadyTasks.Num() == 0 && ActiveTasks.Num() == 0 && SleepingTasks.Num() == 0 && Tasks.Num() == 0)
		{
			if (!ensure(TaskSuccessors.IsEmpty()))
			{
				TaskSuccessors.Reset();
			}
			
			ClearResults();

#if WITH_EDITOR
			// Call the notification update here to prevent it from sticking around - needed because we early out before this
			UpdateGenerationNotification();
#endif

			if (bGraphCacheDebuggingEnabled)
			{
				UE_LOG(LogPCG, Log, TEXT("--- FINISH FPCGGRAPHEXECUTOR::EXECUTE ---"));
			}
		}

		// Purge things from cache if memory usage is too high
		const bool bSomethingTidied = GraphCache.EnforceMemoryBudget();

#if WITH_EDITOR
		if (bSomethingTidied && !PCGHelpers::IsRuntimeOrPIE())
		{
			--TidyCacheCountUntilGC;
			if (TidyCacheCountUntilGC <= 0)
			{
				TidyCacheCountUntilGC = 100;
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
			}
		}
#endif

#if WITH_EDITOR
		// Save & release resources when running in-editor
		SaveDirtyActors();
		ReleaseUnusedActors();
#endif
	}
}

void FPCGGraphExecutor::ClearAllTasks()
{
	Tasks.Reset();

	// Make sure we don't leak preallocated contexts
	for (FPCGGraphTask& ReadyTask : ReadyTasks)
	{
		delete ReadyTask.Context;
	}

	ReadyTasks.Reset();
	ActiveTasks.Reset();
	SleepingTasks.Reset();
}

void FPCGGraphExecutor::QueueNextTasks(FPCGTaskId FinishedTask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::QueueNextTasks);

	if (TSet<FPCGTaskId>* Successors = TaskSuccessors.Find(FinishedTask))
	{
		for (FPCGTaskId Successor : *Successors)
		{
			bool bAllPrerequisitesMet = true;
			FPCGGraphTask& SuccessorTask = Tasks[Successor];

			for (const FPCGGraphTaskInput& Input : SuccessorTask.Inputs)
			{
				bAllPrerequisitesMet &= OutputData.Contains(Input.TaskId);
			}

			if (bAllPrerequisitesMet)
			{
				ReadyTasks.Emplace(MoveTemp(SuccessorTask));
				Tasks.Remove(Successor);
			}
		}

		TaskSuccessors.Remove(FinishedTask);
	}
}

bool FPCGGraphExecutor::CancelNextTasks(FPCGTaskId CancelledTask, TSet<UPCGComponent*>& OutCancelledComponents)
{
	bool bAddedComponents = false;

	if (TSet<FPCGTaskId>* Successors = TaskSuccessors.Find(CancelledTask))
	{
		for (FPCGTaskId Successor : *Successors)
		{
			if (FPCGGraphTask* Task = Tasks.Find(Successor))
			{
				if(!OutCancelledComponents.Contains(Task->SourceComponent.Get()))
				{
					OutCancelledComponents.Add(Task->SourceComponent.Get());
					bAddedComponents = true;
				}

				Tasks.Remove(Successor);
			}

			bAddedComponents |= CancelNextTasks(Successor, OutCancelledComponents);
		}

		TaskSuccessors.Remove(CancelledTask);
	}

	return bAddedComponents;
}

void FPCGGraphExecutor::BuildTaskInput(const FPCGGraphTask& Task, FPCGDataCollection& TaskInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::BuildTaskInput);

	// Initialize a Crc onto which each input Crc will be combined.
	FPCGCrc Crc(Task.Inputs.Num());

	for (const FPCGGraphTaskInput& Input : Task.Inputs)
	{
		check(OutputData.Contains(Input.TaskId));

		// If the input does not provide any data, don't add it to the task input.
		if (!Input.bProvideData)
		{
			continue;
		}

		const FPCGDataCollection& InputCollection = OutputData[Input.TaskId];

		TaskInput.bCancelExecution |= InputCollection.bCancelExecution;

		// Get input data at the given pin (or everything)
		const int32 TaggedDataOffset = TaskInput.TaggedData.Num();
		if (Input.InPin)
		{
			TaskInput.TaggedData.Append(InputCollection.GetInputsByPin(Input.InPin->Properties.Label));

			// Write input pin name Crc to uniquely identify inputs per-pin.
			Crc.Combine(GetTypeHash(Input.OutPin ? Input.OutPin->Properties.Label : FName(TEXT("MissingLabel"))));
		}
		else
		{
			TaskInput.TaggedData.Append(InputCollection.TaggedData);
		}

		if (TaskInput.TaggedData.Num() == TaggedDataOffset && InputCollection.bCancelExecutionOnEmpty)
		{
			TaskInput.bCancelExecution = true;
		}

		// This chains the Crc of each input to produce a Crc that covers all of them.
		if (InputCollection.Crc.IsValid())
		{
			Crc.Combine(InputCollection.Crc);
		}

		// Apply labelling on data; technically, we should ensure that we do this only for pass-through nodes,
		// Otherwise we could also null out the label on the input...
		if (Input.OutPin)
		{
			for (int32 TaggedDataIndex = TaggedDataOffset; TaggedDataIndex < TaskInput.TaggedData.Num(); ++TaggedDataIndex)
			{
				TaskInput.TaggedData[TaggedDataIndex].Pin = Input.OutPin->Properties.Label;
			}
		}
	}

	// Then combine params if needed
	CombineParams(Task.NodeId, TaskInput);

	TaskInput.Crc = Crc;
}

void FPCGGraphExecutor::CombineParams(FPCGTaskId InTaskId, FPCGDataCollection& InTaskInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::CombineParams);

	TArray<FPCGTaggedData> AllParamsData = InTaskInput.GetParamsByPin(PCGPinConstants::DefaultParamsLabel);
	if (AllParamsData.Num() > 1)
	{
		UPCGParamData* CombinedParamData = NewObject<UPCGParamData>();
		for (const FPCGTaggedData& TaggedDatum : AllParamsData)
		{
			const UPCGParamData* ParamData = CastChecked<UPCGParamData>(TaggedDatum.Data);
			CombinedParamData->Metadata->AddAttributes(ParamData->Metadata);
		}

		const int32 NewNumberOfInputs = InTaskInput.TaggedData.Num() - AllParamsData.Num() + 1;
		check(NewNumberOfInputs >= 1);

		TArray<FPCGTaggedData> TempTaggedData{};
		TempTaggedData.Reserve(NewNumberOfInputs);
		for (FPCGTaggedData& TaggedData : InTaskInput.TaggedData)
		{
			if (TaggedData.Pin != PCGPinConstants::DefaultParamsLabel)
			{
				TempTaggedData.Add(std::move(TaggedData));
			}
		}

		// Add to the root set since we created a new object, that needs to be kept alive for the duration of the task.
		DataRootSet.Add(CombinedParamData);
		FPCGTaggedData CombineParams{};
		CombineParams.Data = CombinedParamData;
		CombineParams.Pin = PCGPinConstants::DefaultParamsLabel;
		TempTaggedData.Add(CombineParams);

		InTaskInput.TaggedData = std::move(TempTaggedData);

		// Also store it into the TemporaryMap to track it down
		FPCGDataCollection& TemporaryInput = InputTemporaryData.FindOrAdd(InTaskId);
		TemporaryInput.TaggedData.Add(CombineParams);
	}
}

void FPCGGraphExecutor::StoreResults(FPCGTaskId InTaskId, const FPCGDataCollection& InTaskOutput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::StoreResults);

	// Store output in map
	OutputData.Add(InTaskId, InTaskOutput);

	// Root any non-rooted results, otherwise they'll get garbage-collected
	InTaskOutput.AddToRootSet(DataRootSet);
}

void FPCGGraphExecutor::ClearResults()
{
	ScheduleLock.Lock();

	// Only reset if we have no more scheduled tasks, to avoid breaking dependencies
	if (ScheduledTasks.IsEmpty())
	{
		NextTaskId = 0;
	}

	for (const TPair<FPCGTaskId, FPCGDataCollection>& TaskTemporaryInput : InputTemporaryData)
	{
		TaskTemporaryInput.Value.RemoveFromRootSet(DataRootSet);
	}
	InputTemporaryData.Reset();

	for (const TPair<FPCGTaskId, FPCGDataCollection>& TaskOutput : OutputData)
	{
		TaskOutput.Value.RemoveFromRootSet(DataRootSet);
	}
	OutputData.Reset();

	ScheduleLock.Unlock();
}

FPCGElementPtr FPCGGraphExecutor::GetFetchInputElement()
{
	if (!FetchInputElement)
	{
		FetchInputElement = MakeShared<FPCGFetchInputElement>();
	}

	return FetchInputElement;
}

#if WITH_EDITOR

FPCGTaskId FPCGGraphExecutor::ScheduleDebugWithTaskCallback(UPCGComponent* InComponent, TFunction<void(FPCGTaskId/* TaskId*/, const UPCGNode*/* Node*/, const FPCGDataCollection&/* TaskOutput*/)> TaskCompleteCallback)
{
	FPCGTaskId FinalTaskID = Schedule(InComponent, {});
	TArray<FPCGGraphTask> CompiledTasks = GraphCompiler->GetCompiledTasks(InComponent->GetGraph(), /*bIsTopGraph=*/true);
	CompiledTasks.Pop(); // Remove the final task

	// Set up all final dependencies for the entire execution
	TArray<FPCGTaskId> FinalDependencies;
	FinalDependencies.Reserve(CompiledTasks.Num() + 1);
	FinalDependencies.Add(FinalTaskID);

	for (const FPCGGraphTask& CompiledTask : CompiledTasks)
	{
		// Schedule the output capture hooks
		FPCGTaskId CaptureTaskId = ScheduleGeneric([this, TaskCompleteCallback, CompiledTask]
		{
			FPCGDataCollection TaskOutputData;
			if (CompiledTask.Node && GetOutputData(CompiledTask.NodeId, TaskOutputData))
			{
				TaskCompleteCallback(CompiledTask.NodeId, CompiledTask.Node, TaskOutputData);
			}

			return true;
		}, InComponent, {CompiledTask.NodeId});

		// Add these tasks to the final dependencies
		FinalDependencies.Add(CaptureTaskId);
	}

	// Finally, add a task to wait on the graph itself plus the capture tasks
	return ScheduleGeneric([] { return true; }, InComponent, FinalDependencies);
}

void FPCGGraphExecutor::AddToDirtyActors(AActor* Actor)
{
	ActorsListLock.Lock();
	ActorsToSave.Add(Actor);
	ActorsListLock.Unlock();
}

void FPCGGraphExecutor::AddToUnusedActors(const TSet<FWorldPartitionReference>& UnusedActors)
{
	ActorsListLock.Lock();
	ActorsToRelease.Append(UnusedActors);
	ActorsListLock.Unlock();
}

void FPCGGraphExecutor::SaveDirtyActors()
{
	ActorsListLock.Lock();
	TSet<AActor*> ToSave = MoveTemp(ActorsToSave);
	ActorsToSave.Reset();
	ActorsListLock.Unlock();

	TSet<UPackage*> PackagesToSave;
	for (AActor* Actor : ToSave)
	{
		PackagesToSave.Add(Actor->GetExternalPackage());
	}

	if (PackagesToSave.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave.Array(), true);
	}
}

void FPCGGraphExecutor::ReleaseUnusedActors()
{
	ActorsListLock.Lock();
	bool bRunGC = ActorsToRelease.Num() > 0;
	ActorsToRelease.Reset();
	ActorsListLock.Unlock();

#if WITH_EDITOR
	if (bRunGC && !PCGHelpers::IsRuntimeOrPIE())
	{
		--ReleaseActorsCountUntilGC;
		if (ReleaseActorsCountUntilGC <= 0)
		{
			ReleaseActorsCountUntilGC = 30;
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
		}
	}
#endif
}

void FPCGGraphExecutor::NotifyGraphChanged(UPCGGraph* InGraph)
{
	if (GraphCompiler)
	{
		GraphCompiler->NotifyGraphChanged(InGraph);
	}
}

void FPCGGraphExecutor::UpdateGenerationNotification()
{
	const int32 RemainingTaskNum = Tasks.Num() + ReadyTasks.Num() + ActiveTasks.Num() + SleepingTasks.Num();

	if (RemainingTaskNum > 0)
	{
		GenerationProgressNotification.Update(RemainingTaskNum);
	}
	else
	{
		GenerationProgressNotification.Update(0);
		// To reset the UI notification
		GenerationProgressNotification = FAsyncCompilationNotification(GetNotificationTextFormat());
	}
}

FTextFormat FPCGGraphExecutor::GetNotificationTextFormat()
{
	return NSLOCTEXT("PCG", "PCGGenerationNotificationFormat", "{0}|plural(one=PCG Task,other=PCG Tasks)");
}

#endif // WITH_EDITOR

bool FPCGFetchInputElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	// First: any input can be passed through to the output trivially
	Context->OutputData = Context->InputData;

	// Second: fetch the inputs provided by the component
	UPCGComponent* Component = Context->SourceComponent.Get();
	
	// Early out if the component has been deleted/is invalid
	if (!Component)
	{
		// If the component should exist but it doesn't (which is all the time here, previously we checked for it), then this should be cancelled
		Context->OutputData.bCancelExecution = true;
		return true;
	}

	check(Context->Node);

	if (Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultInputLabel))
	{
		if (UPCGData* PCGData = Component->GetPCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = PCGData;
			TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
		}
	}

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultInputLabel))
	{
		if (UPCGData* InputPCGData = Component->GetInputPCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = InputPCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultInputLabel;
		}
	}

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultActorLabel))
	{
		if (UPCGData* ActorPCGData = Component->GetActorPCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = ActorPCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultActorLabel;
		}
	}

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeLabel))
	{
		if (UPCGData* LandscapePCGData = Component->GetLandscapePCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = LandscapePCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultLandscapeLabel;
		}
	}

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultLandscapeHeightLabel))
	{
		if (UPCGData* LandscapeHeightPCGData = Component->GetLandscapeHeightPCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = LandscapeHeightPCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultLandscapeHeightLabel;
		}
	}

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultOriginalActorLabel))
	{
		if (UPCGData* OriginalActorPCGData = Component->GetOriginalActorPCGData())
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = OriginalActorPCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultOriginalActorLabel;
		}
	}

	return true;
}

FPCGGenericElement::FPCGGenericElement(TFunction<bool(FPCGContext*)> InOperation)
	: Operation(InOperation)
{
}

bool FPCGGenericElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGenericElement::Execute);
	return Operation(Context);
}
