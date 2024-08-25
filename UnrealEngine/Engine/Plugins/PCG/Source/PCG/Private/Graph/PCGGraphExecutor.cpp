// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutor.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGCrc.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Utils/PCGGraphExecutionLogging.h"

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

namespace PCGGraphExecutor
{
	TAutoConsoleVariable<float> CVarTimePerFrame(
		TEXT("pcg.FrameTime"),
		1000.0f / 60.0f,
		TEXT("Allocated time in ms per frame"));

	TAutoConsoleVariable<bool> CVarGraphMultithreading(
		TEXT("pcg.GraphMultithreading"),
		false,
		TEXT("Controls whether the graph can dispatch multiple tasks at the same time"));

#if WITH_EDITOR
	TAutoConsoleVariable<float> CVarEditorTimePerFrame(
		TEXT("pcg.EditorFrameTime"),
		1000.0f / 20.0f,
		TEXT("Allocated time in ms per frame when running in editor (non pie)"));
#endif

	TAutoConsoleVariable<bool> CVarDynamicTaskCulling(
		TEXT("pcg.Graph.DynamicTaskCulling"),
		true,
		TEXT("Controls whether tasks are culled at execution time, for example in response to an deactivated dynamic branch pin"));
}

const FPCGStack* FPCGGraphTask::GetStack() const
{
	return (StackContext && StackIndex != INDEX_NONE) ? StackContext->GetStack(StackIndex) : nullptr;
}

#if WITH_EDITOR
void FPCGGraphTask::LogVisual(ELogVerbosity::Type InVerbosity, const FText& InMessage) const
{
	if (!SourceComponent.IsValid())
	{
		return;
	}

	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(SourceComponent->GetWorld()))
	{
		const FPCGStack* TaskStack = GetStack();
		FPCGStack StackWithNode = TaskStack ? FPCGStack(*TaskStack) : FPCGStack();
		StackWithNode.PushFrame(Node);
		Subsystem->GetNodeVisualLogsMutable().Log(StackWithNode, InVerbosity, InMessage);
	}
}

bool FPCGGraphTaskInput::operator==(const FPCGGraphTaskInput& Other) const
{
	return (TaskId == Other.TaskId)
		&& (InPin == Other.InPin)
		&& (OutPin == Other.OutPin)
		&& (bProvideData == Other.bProvideData);
}

bool FPCGGraphTask::IsApproximatelyEqual(const FPCGGraphTask& Other) const
{
	// Do trivial pointer comparisons first, then run == operator to determine equivalence.
	bool bElementsMatch = (Element == Other.Element);
	if (!bElementsMatch && Element && Other.Element)
	{
		if (Element->IsGridLinkage() && Other.Element->IsGridLinkage())
		{
			const PCGGraphExecutor::FPCGGridLinkageElement& LinkageElement = static_cast<const PCGGraphExecutor::FPCGGridLinkageElement&>(*Element);
			const PCGGraphExecutor::FPCGGridLinkageElement& OtherLinkageElement = static_cast<const PCGGraphExecutor::FPCGGridLinkageElement&>(*Other.Element);
			bElementsMatch = (LinkageElement == OtherLinkageElement);
		}
		else
		{
			ensureMsgf(false, TEXT("Graph compilation emitted an element type that is not a trivial element or a grid linkage element. Element comparison will fail. ")
				TEXT("Equivalence operator needs to be implemented for this new element."));
		}
	}

	return (Inputs == Other.Inputs)
		&& (Node == Other.Node)
		&& (SourceComponent == Other.SourceComponent)
		&& bElementsMatch
		&& (Context == Other.Context)
		&& (NodeId == Other.NodeId)
		&& (CompiledTaskId == Other.CompiledTaskId)
		&& (ParentId == Other.ParentId)
		&& (PinDependency == Other.PinDependency)
		&& (StackIndex == Other.StackIndex)
		&& (StackContext == Other.StackContext);
}
#endif // WITH_EDITOR

FPCGGraphExecutor::FPCGGraphExecutor()
#if WITH_EDITOR
	: GenerationProgressNotification(GetNotificationTextFormat())
#endif
{
}

FPCGGraphExecutor::~FPCGGraphExecutor()
{
	// We don't really need to do this here (it would be done in the destructor of these both)
	// but this is to clarify/ensure the order in which this happens
	GraphCache.ClearCache();

#if WITH_EDITOR
	// Cleanup + clear notification
	ClearAllTasks();
	UpdateGenerationNotification();
#endif
}

void FPCGGraphExecutor::Compile(UPCGGraph* Graph)
{
	GraphCompiler.Compile(Graph);
}

FPCGTaskId FPCGGraphExecutor::Schedule(UPCGComponent* Component, const TArray<FPCGTaskId>& ExternalDependencies, const FPCGStack* InFromStack)
{
	check(Component);
	UPCGGraph* Graph = Component->GetGraph();

	return Schedule(Graph, Component, FPCGElementPtr(), GetFetchInputElement(), ExternalDependencies, InFromStack, /*bAllowHierarchicalGeneration=*/true);
}

FPCGTaskId FPCGGraphExecutor::Schedule(
	UPCGGraph* Graph,
	UPCGComponent* SourceComponent,
	FPCGElementPtr PreGraphElement,
	FPCGElementPtr InputElement,
	const TArray<FPCGTaskId>& ExternalDependencies,
	const FPCGStack* InFromStack,
	bool bAllowHierarchicalGeneration)
{
	check(SourceComponent);

	PCGGraphExecutionLogging::LogGraphSchedule(SourceComponent, Graph);
	
	FPCGTaskId ScheduledId = InvalidPCGTaskId;

	uint32 GenerationGridSize = PCGHiGenGrid::UninitializedGridSize();
	if (bAllowHierarchicalGeneration && Graph->IsHierarchicalGenerationEnabled())
	{
		if (SourceComponent->IsLocalComponent() || SourceComponent->IsPartitioned())
		{
			GenerationGridSize = SourceComponent->GetGenerationGridSize();
		}
	}

	// Get compiled tasks from compiler
	TSharedPtr<FPCGStackContext> StackContextPtr = MakeShared<FPCGStackContext>();
	TArray<FPCGGraphTask> CompiledTasks = GraphCompiler.GetCompiledTasks(Graph, GenerationGridSize, *StackContextPtr);

	// Create the final stack context by including the current stack frames
	if (InFromStack)
	{
		StackContextPtr->PrependParentStack(InFromStack);
	}
	else
	{
		FPCGStack ComponentStack;
		ComponentStack.PushFrame(SourceComponent);
		StackContextPtr->PrependParentStack(&ComponentStack);		
	}

#if WITH_EDITOR
	if (UPCGSubsystem* Subsystem = SourceComponent->GetSubsystem())
	{
		Subsystem->OnScheduleGraph(*StackContextPtr);
	}
#endif

	// Assign this component to the tasks
	for (FPCGGraphTask& Task : CompiledTasks)
	{
		Task.SourceComponent = SourceComponent;
		Task.StackContext = StackContextPtr;
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
		FPCGGraphTask& PreGraphTask = ScheduledTask.Tasks[ScheduledTask.Tasks.Num() - 2];

		if(PreGraphElement.IsValid())
		{
			PreGraphTask.Element = PreGraphElement;
		}

		for (FPCGTaskId ExternalDependency : ExternalDependencies)
		{
			// For the pre-task, we don't consume any input
			PreGraphTask.Inputs.Emplace(ExternalDependency, nullptr, nullptr, /*bConsumeData=*/false);
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

	// In one instance this function was observed to return nullptr in the CancelledComponents set.
	// All the cancel filter lambdas check the ptr is valid, so it's not clear why. It seems perhaps
	// the SourceComponent weak ptr became nullptr between calling CancelFilter and adding the component
	// to the set.
	ensure(CancelledComponents.Remove(nullptr) == 0);

	// Early out - nothing to cancel
	if (CancelledComponents.IsEmpty())
	{
		return CancelledComponents;
	}

	PCGGraphExecutionLogging::LogComponentCancellation(CancelledComponents);

	auto TryAbortScheduledTasks = [](FPCGGraphScheduleTask& ScheduledTask)
	{
		if (ScheduledTask.bHasAbortCallbacks)
		{
			for (FPCGGraphTask& InternalTask : ScheduledTask.Tasks)
			{
				if (InternalTask.Element)
				{
					InternalTask.Element->Abort(InternalTask.Context);
				}
			}
		}
	};

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

				TryAbortScheduledTasks(ScheduledTask);
				ScheduledTasks.RemoveAtSwap(ScheduledTaskIndex);
			}
		}

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
					UPCGComponent* TaskComponent = ScheduledTask.SourceComponent.Get();
					if (TaskComponent && !CancelledComponents.Contains(TaskComponent))
					{
						CancelledComponents.Add(TaskComponent);
						bStableCancellationSet = false;
					}

					CancelledScheduledTasks.Add(ScheduledTask.Tasks[ScheduledTask.LastTaskIndex].NodeId);

					TryAbortScheduledTasks(ScheduledTask);
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
				if (Task.Element)
				{
					Task.Element->Abort(Task.Context);
				}

				FPCGTaskId CancelledTaskId = Task.NodeId;
				RemoveTaskFromInputSuccessors(CancelledTaskId, Task.Inputs);

				delete Task.Context;
				ReadyTasks.RemoveAtSwap(ReadyTaskIndex);
				bStableCancellationSet &= !CancelNextTasks(CancelledTaskId, CancelledComponents);
			}
		}

		// Mark as cancelled in the active tasks - needed to make sure we're not breaking the current execution (if any)
		for (int32 ActiveTaskIndex = ActiveTasks.Num() - 1; ActiveTaskIndex >= 0; --ActiveTaskIndex)
		{
			FPCGGraphActiveTask& Task = ActiveTasks[ActiveTaskIndex];
			if (Task.Context && CancelledComponents.Contains(Task.Context->SourceComponent.Get()))
			{
				check(Task.Element);
				Task.Element->Abort(Task.Context.Get());

				FPCGTaskId CancelledTaskId = Task.NodeId;
				Task.bWasCancelled = true;
				RemoveTaskFromInputSuccessors(CancelledTaskId, Task.Inputs);

				bStableCancellationSet &= !CancelNextTasks(CancelledTaskId, CancelledComponents);
			}
		}

		// Remove from sleeping tasks
		for (int32 SleepingTaskIndex = SleepingTasks.Num() - 1; SleepingTaskIndex >= 0; --SleepingTaskIndex)
		{
			FPCGGraphActiveTask& Task = SleepingTasks[SleepingTaskIndex];
			if (Task.Context && CancelledComponents.Contains(Task.Context->SourceComponent.Get()))
			{
				check(Task.Element);
				Task.Element->Abort(Task.Context.Get());

				FPCGTaskId CancelledTaskId = Task.NodeId;
				RemoveTaskFromInputSuccessors(CancelledTaskId, Task.Inputs);

				SleepingTasks.RemoveAtSwap(SleepingTaskIndex);
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

bool FPCGGraphExecutor::IsAnyGraphCurrentlyExecuting() const
{
	if (GetNonScheduledRemainingTaskCount() > 0)
	{
		return true;
	}

	// No need for locking here as we don't need the value to be precise here and it can change right after a lock anyways.
	return ScheduledTasks.Num() > 0;
}

int32 FPCGGraphExecutor::GetNonScheduledRemainingTaskCount() const
{
	return Tasks.Num() + ReadyTasks.Num() + ActiveTasks.Num() + SleepingTasks.Num();
}

FPCGTaskId FPCGGraphExecutor::ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies)
{
	return ScheduleGeneric(
		InOperation,
		TFunction<void()>(),
		InSourceComponent,
		TaskExecutionDependencies);
}

FPCGTaskId FPCGGraphExecutor::ScheduleGeneric(TFunction<bool()> InOperation, TFunction<void()> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies)
{
	// Since we have no context, the generic task will consume no input (no data dependencies).
	return ScheduleGenericWithContext(
		[Operation = MoveTemp(InOperation)](FPCGContext*) -> bool
		{
			return Operation && Operation();
		}, 
		[AbortOperation = MoveTemp(InAbortOperation)](FPCGContext*)
		{
			if(AbortOperation)
			{
				AbortOperation();
			}
		},
		InSourceComponent,
		TaskExecutionDependencies,
		/*TaskDataDependencies=*/{});
}

FPCGTaskId FPCGGraphExecutor::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies)
{
	return ScheduleGenericWithContext(
		InOperation,
		TFunction<void(FPCGContext*)>(),
		InSourceComponent,
		TaskExecutionDependencies,
		TaskDataDependencies);
}

FPCGTaskId FPCGGraphExecutor::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, UPCGComponent* InSourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies)
{
	// Build task & element to hold the operation to perform
	FPCGGraphTask Task;

	for (FPCGTaskId TaskDependency : TaskExecutionDependencies)
	{
		ensure(TaskDependency != InvalidPCGTaskId);
		Task.Inputs.Emplace(TaskDependency, /*InPin=*/nullptr, /*OutPin=*/nullptr, /*bConsumeInputData=*/false);
	}

	for (FPCGTaskId TaskDependency : TaskDataDependencies)
	{
		ensure(TaskDependency != InvalidPCGTaskId);
		Task.Inputs.Emplace(TaskDependency, /*InPin=*/nullptr, /*OutPin=*/nullptr, /*bConsumeInputData=*/true);
	}

	Task.SourceComponent = InSourceComponent;
	Task.Element = MakeShared<FPCGGenericElement>(InOperation, InAbortOperation);

	ScheduleLock.Lock();

	// Assign task id
	Task.NodeId = NextTaskId++;

	FPCGGraphScheduleTask& ScheduledTask = ScheduledTasks.Emplace_GetRef();
	ScheduledTask.Tasks.Add(Task);
	ScheduledTask.SourceComponent = InSourceComponent;
	ScheduledTask.bHasAbortCallbacks = !!InAbortOperation;

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

	for (int32 ScheduledTaskIndex = ScheduledTasks.Num() - 1; ScheduledTaskIndex >= 0; --ScheduledTaskIndex)
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

	if (!ScheduledTasks.IsEmpty())
	{
		PCGGraphExecutionLogging::LogGraphPostSchedule(Tasks, TaskSuccessors);
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

#if WITH_EDITOR
	// Update Notifications before capturing StartTime so that this call doesn't eat up task budget
	UpdateGenerationNotification();
#endif

	const double StartTime = FPlatformTime::Seconds();

	double VarTimePerFrame = PCGGraphExecutor::CVarTimePerFrame.GetValueOnAnyThread() / 1000.0;

#if WITH_EDITOR
	if (GEditor && !GEditor->IsPlaySessionInProgress())
	{
		VarTimePerFrame = PCGGraphExecutor::CVarEditorTimePerFrame.GetValueOnAnyThread() / 1000.0;
	}
#endif

	const double EndTime = StartTime + VarTimePerFrame;
	const float MaxPercentageOfThreadsToUse = FMath::Clamp(CVarMaxPercentageOfThreadsToUse.GetValueOnAnyThread(), 0.0f, 1.0f);
	const int32 MaxNumThreads = FMath::Max(0, FMath::Min((int32)(FPlatformMisc::NumberOfCoresIncludingHyperthreads() * MaxPercentageOfThreadsToUse), CVarMaxNumTasks.GetValueOnAnyThread() - 1));
	const bool bAllowMultiDispatch = PCGGraphExecutor::CVarGraphMultithreading.GetValueOnAnyThread();
	const bool bDynamicTaskCulling = PCGGraphExecutor::CVarDynamicTaskCulling.GetValueOnAnyThread();

	while (ReadyTasks.Num() > 0 || ActiveTasks.Num() > 0 || (!bHasAlreadyCheckedSleepingTasks && SleepingTasks.Num() > 0))
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

		if (!CannotDispatchMoreTasks())
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

			for (int32 ReadyTaskIndex = ReadyTasks.Num() - 1; ReadyTaskIndex >= 0; --ReadyTaskIndex)
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

				PCGGraphExecutionLogging::LogTaskExecute(Task);

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

				if (!bCacheable)
				{
					PCGGraphExecutionLogging::LogTaskExecuteCachingDisabled(Task);
				}

				FPCGDataCollection CachedOutput;
				const bool bResultAlreadyInCache = bCacheable && DependenciesCrc.IsValid() && GraphCache.GetFromCache(Task.Node, Task.Element.Get(), DependenciesCrc, Task.SourceComponent.Get(), CachedOutput);
#if WITH_EDITOR
				const bool bNeedsToCreateActiveTask = !bResultAlreadyInCache || TaskSettingsInterface->bDebug;
#else
				const bool bNeedsToCreateActiveTask = !bResultAlreadyInCache;
#endif

				if (!bNeedsToCreateActiveTask)
				{
#if WITH_EDITOR
					// Doing this now since we're about to modify ReadyTasks potentially reallocating while Task is a reference. 
					if (UPCGComponent* SourceComponent = Task.SourceComponent.Get())
					{
						if (Task.StackIndex != INDEX_NONE)
						{
							const FPCGStack* Stack = Task.GetStack();
							SourceComponent->StoreInspectionData(Stack, Task.Node, TaskInput, CachedOutput, /*bUsedCache=*/true);
						}
					}
#endif

					if (bDynamicTaskCulling && TaskSettings && TaskSettings->OutputPinsCanBeDeactivated() && CachedOutput.InactiveOutputPinBitmask != 0)
					{
						CullInactiveDownstreamNodes(Task.NodeId, CachedOutput.InactiveOutputPinBitmask);

#if WITH_EDITOR
						SendInactivePinNotification(Task.Node, Task.GetStack(), CachedOutput.InactiveOutputPinBitmask);
#endif
					}

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
					Task.Context->DependenciesCrc = DependenciesCrc;
					Task.Context->Stack = Task.GetStack();
				}

				// Validate that we can start this task now
				const bool bIsMainThreadTask = Task.Element->CanExecuteOnlyOnMainThread(Task.Context);

				if (!bIsMainThreadTask || bMainThreadAvailable)
				{
					FPCGGraphActiveTask& ActiveTask = ActiveTasks.Emplace_GetRef();
					ActiveTask.Inputs = MoveTemp(Task.Inputs);
					ActiveTask.Element = Task.Element;
					ActiveTask.NodeId = Task.NodeId;
					ActiveTask.Context = TUniquePtr<FPCGContext>(Task.Context);
					ActiveTask.StackIndex = Task.StackIndex;
					ActiveTask.StackContext = Task.StackContext;

#if WITH_EDITOR
					if (bResultAlreadyInCache)
					{
						ActiveTask.bIsBypassed = true;
						ActiveTask.Context->OutputData = CachedOutput;
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
			for (int32 ExecutionIndex = 1; ExecutionIndex < ActiveTasks.Num(); ++ExecutionIndex)
			{
				FPCGGraphActiveTask& ActiveTask = ActiveTasks[ExecutionIndex];
				check(!ActiveTask.Context->bIsPaused);

	#if WITH_EDITOR
				if (!ActiveTask.bIsBypassed)
	#endif
				{
					check(!ActiveTask.Element->CanExecuteOnlyOnMainThread(ActiveTask.Context.Get()));
					ActiveTask.Context->AsyncState.EndTime = EndTime;
					ActiveTask.Context->AsyncState.bIsRunningOnMainThread = false;

					Futures.Emplace(ExecutionIndex, Async(EAsyncExecution::ThreadPool, [&ActiveTask]()
					{
						return ActiveTask.Element->Execute(ActiveTask.Context.Get());
					}));
				}
			}
		}

		auto PostTaskExecute = [this, &bAnyTaskEnded](int32 TaskIndex)
		{
			FPCGGraphActiveTask& ActiveTask = ActiveTasks[TaskIndex];
			check(ActiveTask.Context);

			const UPCGSettingsInterface* ActiveTaskSettingsInterface = ActiveTask.Context->GetInputSettingsInterface();
			const uint64 InactivePinMask = ActiveTask.Context->OutputData.InactiveOutputPinBitmask;

			if (InactivePinMask != 0 && ActiveTaskSettingsInterface)
			{
				const UPCGSettings* ActiveTaskSettings = ActiveTaskSettingsInterface ? ActiveTaskSettingsInterface->GetSettings() : nullptr;

				// If output pins may have been deactivated then perform culling and update information for editor visualization.
				if (ActiveTaskSettings && ActiveTaskSettings->OutputPinsCanBeDeactivated())
				{
					CullInactiveDownstreamNodes(ActiveTask.NodeId, InactivePinMask);

#if WITH_EDITOR
					SendInactivePinNotification(ActiveTask.Context->Node, ActiveTask.StackContext->GetStack(ActiveTask.StackIndex), InactivePinMask);
#endif
				}
			}

#if WITH_EDITOR
			if (!ActiveTask.bWasCancelled && !ActiveTask.bIsBypassed)
#else
			if (!ActiveTask.bWasCancelled)
#endif
			{
				// Store result in cache as needed - done here because it needs to be done on the main thread

				// Don't store if errors or warnings present
#if WITH_EDITOR
				const bool bHasErrorOrWarning = ActiveTask.Context->Node && (ActiveTask.Context->HasVisualLogs());
#else
				const bool bHasErrorOrWarning = false;
#endif

				if (ActiveTaskSettingsInterface && !bHasErrorOrWarning && ActiveTask.Element->IsCacheableInstance(ActiveTaskSettingsInterface))
				{
					GraphCache.StoreInCache(ActiveTask.Element.Get(), ActiveTask.Context->DependenciesCrc, ActiveTask.Context->OutputData);
				}
			}

			check(ActiveTask.Context->AsyncState.NumAvailableTasks >= 0);
			CurrentlyUsedThreads -= ActiveTask.Context->AsyncState.NumAvailableTasks;

#if WITH_EDITOR
			if (!ActiveTask.bWasCancelled)
			{
				// Execute debug display code as needed - done here because it needs to be done on the main thread
				// Additional note: this needs to be executed before the StoreResults since debugging might cancel further tasks
				ActiveTask.Element->DebugDisplay(ActiveTask.Context.Get());

				if (UPCGComponent* SourceComponent = ActiveTask.Context->SourceComponent.Get())
				{
					if (ActiveTask.StackIndex != INDEX_NONE)
					{
						const FPCGStack* Stack = ActiveTask.StackContext->GetStack(ActiveTask.StackIndex);
						SourceComponent->StoreInspectionData(Stack, ActiveTask.Context->Node, ActiveTask.Context->InputData, ActiveTask.Context->OutputData, /*bUsedCache=*/false);
					}
				}
			}
#endif

			// Store output in data map.
			// TODO - investigate if we should avoid doing this if the task was cancelled.
			StoreResults(ActiveTask.NodeId, ActiveTask.Context->OutputData);

			// Book-keeping
			QueueNextTasks(ActiveTask.NodeId);
			bAnyTaskEnded = true;

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
				for (int32 ExecutionIndex = ActiveTasks.Num() - 1; ExecutionIndex > 0; --ExecutionIndex)
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
				}
			}

			if (bMainTaskDone)
			{
				PostTaskExecute(0);
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
		if (GetNonScheduledRemainingTaskCount() == 0)
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

			PCGGraphExecutionLogging::LogGraphExecuteFrameFinished();
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
	TaskSuccessors.Reset();
}

void FPCGGraphExecutor::QueueNextTasks(FPCGTaskId FinishedTask, bool bIgnoreMissingTasks)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::QueueNextTasks);

	if (TSet<FPCGTaskId>* Successors = TaskSuccessors.Find(FinishedTask))
	{
		for (FPCGTaskId Successor : *Successors)
		{
			bool bAllPrerequisitesMet = true;
			FPCGGraphTask* SuccessorTaskPtr = Tasks.Find(Successor);

			// This should never be null, but later recovery should be able to cleanup this properly
			if (SuccessorTaskPtr)
			{
				FPCGGraphTask& SuccessorTask = *SuccessorTaskPtr;

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
			else
			{
				ensure(bIgnoreMissingTasks);
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
		TSet<FPCGTaskId> LocalSuccessors = MoveTemp(*Successors);
		TaskSuccessors.Remove(CancelledTask);

		for (FPCGTaskId Successor : LocalSuccessors)
		{
			if (FPCGGraphTask* Task = Tasks.Find(Successor))
			{
				if(!OutCancelledComponents.Contains(Task->SourceComponent.Get()))
				{
					OutCancelledComponents.Add(Task->SourceComponent.Get());
					bAddedComponents = true;
				}

				if (Task->Element)
				{
					Task->Element->Abort(Task->Context);
				}

				RemoveTaskFromInputSuccessors(Task->NodeId, Task->Inputs);
				Tasks.Remove(Successor);
			}

			bAddedComponents |= CancelNextTasks(Successor, OutCancelledComponents);
		}
	}

	// Tasks cancelled might have an impact on scheduled-but-not-processed tasks
	ScheduleLock.Lock();
	for (FPCGGraphScheduleTask& ScheduledTask : ScheduledTasks)
	{
		if (!OutCancelledComponents.Contains(ScheduledTask.SourceComponent.Get()) &&
			Algo::AnyOf(ScheduledTask.Tasks[ScheduledTask.FirstTaskIndex].Inputs, [CancelledTask](const FPCGGraphTaskInput& Input) { return Input.TaskId == CancelledTask; }))
		{
			OutCancelledComponents.Add(ScheduledTask.SourceComponent.Get());
			bAddedComponents = true;
		}
	}
	ScheduleLock.Unlock();

	return bAddedComponents;
}

void FPCGGraphExecutor::RemoveTaskFromInputSuccessors(FPCGTaskId CancelledTask, const TArray<FPCGGraphTaskInput>& CancelledTaskInputs)
{
	for (const FPCGGraphTaskInput& Input : CancelledTaskInputs)
	{
		if (TSet<FPCGTaskId>* Successors = TaskSuccessors.Find(Input.TaskId))
		{
			Successors->Remove(CancelledTask);
		}
	}
}

void FPCGGraphExecutor::BuildTaskInput(const FPCGGraphTask& Task, FPCGDataCollection& TaskInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::BuildTaskInput);

	auto LogDiscardedData = [&Task](const UPCGPin* InPin)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
		// Turn off eventual errors/warnings when the node is disabled, as this is irrelevant.
		const bool bNodeIsDisabled = (Task.Context && Task.Context->GetOriginalSettings<UPCGSettings>() && !Task.Context->GetOriginalSettings<UPCGSettings>()->bEnabled);
		if (bNodeIsDisabled)
		{
			return;
		}

		const FString Message = FString::Printf(
			TEXT("[%s] %s - BuildTaskInput - too many data items arriving on single data pin '%s', only first data item will be used"),
			(Task.SourceComponent.Get() && Task.SourceComponent->GetOwner()) ? *Task.SourceComponent->GetOwner()->GetName() : TEXT("MissingComponent"),
			Task.Node ? *Task.Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("MissingNode"),
			InPin ? *InPin->Properties.Label.ToString() : TEXT("MissingPin"));

#if WITH_EDITOR
		Task.LogVisual(ELogVerbosity::Warning, FText::FromString(Message));
#endif // WITH_EDITOR

		UE_LOG(LogPCG, Warning, TEXT("%s"), *Message);
#endif
	};

	// Initialize a Crc onto which each input Crc will be combined (using random prime number).
	FPCGCrc Crc(1000033);

	// Random prime numbers to use as placeholders in the CRC computation when there are no defined in/out pins.
	// Note that they aren't strictly needed, but will make sure we don't introduce issues if we rework this bit of code.
	constexpr uint32 DefaultHashForNoInputPin = 955333;
	constexpr uint32 DefaultHashForNoOutputPin = 999983;

	// Hoisted out of loop for performance reasons.
	TArray<FPCGTaggedData, TInlineAllocator<16>> InputDataOnPin;
	TArray<FPCGCrc, TInlineAllocator<16>> InputDataCrcsOnPin;

	for (const FPCGGraphTaskInput& Input : Task.Inputs)
	{
		check(OutputData.Contains(Input.TaskId));

		// If the input does not provide any data, don't add it to the task input.
		if (!Input.bProvideData)
		{
			continue;
		}

		const bool bAllowMultipleData = Input.OutPin ? Input.OutPin->Properties.bAllowMultipleData : true;
		const uint32 InputPinLabelCrc = Input.OutPin ? GetTypeHash(Input.OutPin->Properties.Label) : DefaultHashForNoOutputPin;

		// Enforce single data - if already have input for this pin, don't add more. Early check before other side effects below.
		if (Input.OutPin && !bAllowMultipleData && TaskInput.GetInputCountByPin(Input.OutPin->Properties.Label) > 0)
		{
			LogDiscardedData(Input.OutPin);
			continue;
		}

		const FPCGDataCollection& InputCollection = OutputData[Input.TaskId];

		TaskInput.bCancelExecution |= InputCollection.bCancelExecution;

		const int32 TaggedDataOffset = TaskInput.TaggedData.Num();

		// Get input data at the given pin (or everything). This will add the data and include the input pin Crc to uniquely identify
		// inputs per-pin, or use a placeholder for symmetry.
		// Note: The input data CRC will already contain the output pin (calculated in element post execute).
		if (Input.InPin)
		{
			InputDataOnPin.Reset();
			InputDataCrcsOnPin.Reset();
			InputCollection.GetInputsAndCrcsByPin(Input.InPin->Properties.Label, InputDataOnPin, InputDataCrcsOnPin);

			if (!InputDataOnPin.IsEmpty())
			{
				// Proceed carefully when adding data items - if pin is single-data, only add first item.
				if (!ensure(InputDataOnPin.Num() == InputDataCrcsOnPin.Num()))
				{
					InputDataCrcsOnPin.SetNumZeroed(InputDataOnPin.Num());
				}

				const int NumberDataItemsToTake = bAllowMultipleData ? InputDataOnPin.Num() : 1;

				TaskInput.AddDataForPin(
					MakeArrayView(InputDataOnPin.GetData(), NumberDataItemsToTake),
					MakeArrayView(InputDataCrcsOnPin.GetData(), NumberDataItemsToTake),
					InputPinLabelCrc);

				if (NumberDataItemsToTake < InputDataOnPin.Num())
				{
					LogDiscardedData(Input.OutPin);
				}
			}
		}
		else
		{
			TaskInput.AddData(InputCollection.TaggedData, InputCollection.DataCrcs);
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
}

void FPCGGraphExecutor::CombineParams(FPCGTaskId InTaskId, FPCGDataCollection& InTaskInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::CombineParams);

	TArray<FPCGTaggedData> AllParamsData = InTaskInput.GetParamsByPin(PCGPinConstants::DefaultParamsLabel);
	if (AllParamsData.Num() > 1)
	{
		UPCGParamData* CombinedParamData = nullptr;
		bool bSuccess = true;

		for (const FPCGTaggedData& TaggedDatum : AllParamsData)
		{
			const UPCGParamData* ParamData = CastChecked<UPCGParamData>(TaggedDatum.Data);
			if (!CombinedParamData)
			{
				CombinedParamData = ParamData->DuplicateData();
			}
			else
			{
				bSuccess &= PCGMetadataHelpers::CopyAllAttributes(ParamData, CombinedParamData, nullptr);
			}
		}

		if (!bSuccess)
		{
			return;
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
		FPCGTaggedData CombineParams{};
		CombineParams.Data = CombinedParamData;
		CombineParams.Pin = PCGPinConstants::DefaultParamsLabel;
		TempTaggedData.Add(CombineParams);

		InTaskInput.TaggedData = std::move(TempTaggedData);
	}
}

void FPCGGraphExecutor::StoreResults(FPCGTaskId InTaskId, const FPCGDataCollection& InTaskOutput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::StoreResults);

	// Store output in map
	OutputData.Add(InTaskId, InTaskOutput);
}

void FPCGGraphExecutor::ClearResults()
{
	ScheduleLock.Lock();

	// Only reset if we have no more scheduled tasks, to avoid breaking dependencies
	if (ScheduledTasks.IsEmpty())
	{
		NextTaskId = 0;
	}

	OutputData.Reset();

	ScheduleLock.Unlock();
}

void FPCGGraphExecutor::GetPinIdsToDeactivate(FPCGTaskId TaskId, uint64 InactiveOutputPinBitmask, TArray<FPCGPinId>& InOutPinIds)
{
	InOutPinIds.Reserve(InOutPinIds.Num() + FMath::CountBits(InactiveOutputPinBitmask));

	int OutputPinIndex = 0;

	while (InactiveOutputPinBitmask != 0)
	{
		if (InactiveOutputPinBitmask & 1)
		{
			InOutPinIds.AddUnique(PCGPinIdHelpers::NodeIdAndPinIndexToPinId(TaskId, OutputPinIndex));
		}

		InactiveOutputPinBitmask >>= 1;
		++OutputPinIndex;
	}
}

void FPCGGraphExecutor::CullInactiveDownstreamNodes(FPCGTaskId InCompletedTaskId, uint64 InInactiveOutputPinBitmask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::CullInactiveDownstreamNodes);

	TArray<FPCGPinId> PinIdsToDeactivate;
	GetPinIdsToDeactivate(InCompletedTaskId, InInactiveOutputPinBitmask, PinIdsToDeactivate);
	check(!PinIdsToDeactivate.IsEmpty());

	PCGGraphExecutionLogging::LogTaskCullingBegin(InCompletedTaskId, InInactiveOutputPinBitmask, PinIdsToDeactivate);

	TSet<FPCGTaskId> AllRemovedTasks;

	// Hoisted out of loop for performance reasons.
	TArray<FPCGTaskId, TInlineAllocator<64>> TasksToRemove;

	while (!PinIdsToDeactivate.IsEmpty())
	{
		const FPCGPinId PinId = PinIdsToDeactivate.Pop(EAllowShrinking::No);
		const FPCGTaskId PinTaskId = PCGPinIdHelpers::GetNodeIdFromPinId(PinId);

		PCGGraphExecutionLogging::LogTaskCullingBeginLoop(PinTaskId, PCGPinIdHelpers::GetPinIndexFromPinId(PinId), PinIdsToDeactivate);
		LogTaskState();

		const TSet<FPCGTaskId>* Successors = TaskSuccessors.Find(PinTaskId);
		if (!Successors)
		{
			continue;
		}

		TasksToRemove.SetNum(0, EAllowShrinking::No);

		// Build set of tasks that are candidates for culling when PinId is deactivated.
		for (const FPCGTaskId SuccessorTaskId : *Successors)
		{
			// Successors are updated at the end of this function, which means it may
			// contain task IDs that have been removed.
			if (FPCGGraphTask* FoundTask = Tasks.Find(SuccessorTaskId))
			{
				bool bDependencyExpressionBecameFalse;
				FoundTask->PinDependency.DeactivatePin(PinId, bDependencyExpressionBecameFalse);

				if (bDependencyExpressionBecameFalse)
				{
					TasksToRemove.AddUnique(SuccessorTaskId);
				}

				PCGGraphExecutionLogging::LogTaskCullingUpdatedPinDeps(SuccessorTaskId, FoundTask->PinDependency, bDependencyExpressionBecameFalse);
			}
		}

		// Now remove the tasks.
		for (const FPCGTaskId RemovedTaskId : TasksToRemove)
		{
			// Scope in which RemovedTask reference is valid.
			{
				FPCGGraphTask& RemovedTask = Tasks[RemovedTaskId];

				const UPCGNode* Node = RemovedTask.Node;
				const int PinCount = Node ? Node->GetOutputPins().Num() : 0;

				if (PinCount > 0)
				{
					// Deactivate all output pins.
					const uint64 InactiveOutputPinBitmask = (1ULL << PinCount) - 1;

					// Deactivate its pins - add to set of pins to deactivate.
					GetPinIdsToDeactivate(RemovedTaskId, InactiveOutputPinBitmask, PinIdsToDeactivate);

#if WITH_EDITOR
					SendInactivePinNotification(RemovedTask.Node, RemovedTask.GetStack(), InactiveOutputPinBitmask);
#endif
				}

				// Also register a special pin-less pin ID for this node, for task dependencies that do not have a specific pin.
				PinIdsToDeactivate.AddUnique(PCGPinIdHelpers::NodeIdToPinId(RemovedTaskId));

				// Remove task as successor of upstream node.
				RemoveTaskFromInputSuccessors(RemovedTaskId, RemovedTask.Inputs);

				// Remove the deleted tasks from the inputs of downstream tasks.
				if (const TSet<FPCGTaskId>* SuccessorsOfRemovedTask = TaskSuccessors.Find(RemovedTaskId))
				{
					for (const FPCGTaskId SuccessorTaskId : *SuccessorsOfRemovedTask)
					{
						if (FPCGGraphTask* SuccessorTask = Tasks.Find(SuccessorTaskId))
						{
							for (int InputIndex = SuccessorTask->Inputs.Num() - 1; InputIndex >= 0; --InputIndex)
							{
								if (SuccessorTask->Inputs[InputIndex].TaskId == RemovedTaskId)
								{
									SuccessorTask->Inputs.RemoveAtSwap(InputIndex);
								}
							}
						}
					}
				}
			}

			// Remove from tasks. After this step all traces of RemovedTaskId should be erased from tasks, task inputs. Task successors will be
			// updated below when queuing next tasks.
			Tasks.Remove(RemovedTaskId);
		}

		AllRemovedTasks.Append(TasksToRemove);
	}

	// Ensure any downstream tasks are enqueued.
	for (const FPCGTaskId TaskId : AllRemovedTasks)
	{
		// Queue downstream tasks in a similar manner to when a task draws from the cache and is skipped.
		// Some downstream tasks will have been culled which we don't care about (hence the ignore flag),
		// but some may not be queued and may be ready for queuing.
		QueueNextTasks(TaskId, /*bIgnoreMissingTasks=*/true);
	}
}

#if WITH_EDITOR
void FPCGGraphExecutor::SendInactivePinNotification(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactiveOutputPinBitmask)
{
	const UPCGComponent* Component = InStack ? InStack->GetRootComponent() : nullptr;
	if (Component && InNode)
	{
		Component->NotifyNodeDynamicInactivePins(InNode, InStack, InactiveOutputPinBitmask);
	}
}
#endif

void FPCGGraphExecutor::AddReferencedObjects(FReferenceCollector& Collector)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::AddReferencedObjects);

	// Go through all data in the cached output map
	for (auto& OutputDataEntry : OutputData)
	{
		OutputDataEntry.Value.AddReferences(Collector);
	}

	// Go through ready tasks, active tasks and sleeping tasks contexts
	auto AddReferences = [&Collector](auto& TaskContainer)
	{
		for (auto& Task : TaskContainer)
		{
			if (Task.Context)
			{
				Task.Context->AddStructReferencedObjects(Collector);
			}
		}
	};

	AddReferences(ReadyTasks);
	AddReferences(ActiveTasks);
	AddReferences(SleepingTasks);
}

FPCGElementPtr FPCGGraphExecutor::GetFetchInputElement()
{
	if (!FetchInputElement)
	{
		FetchInputElement = MakeShared<FPCGFetchInputElement>();
	}

	return FetchInputElement;
}

void FPCGGraphExecutor::LogTaskState() const
{
#if WITH_EDITOR
	if (PCGGraphExecutionLogging::CullingLogEnabled())
	{
		UE_LOG(LogPCG, Log, TEXT("\tDORMANT (FPCGGraphExecutor::Tasks):"));
		PCGGraphExecutionLogging::LogGraphTasks(Tasks, &TaskSuccessors);
	}
#endif
}

#if WITH_EDITOR

FPCGTaskId FPCGGraphExecutor::ScheduleDebugWithTaskCallback(UPCGComponent* InComponent, TFunction<void(FPCGTaskId/* TaskId*/, const UPCGNode*/* Node*/, const FPCGDataCollection&/* TaskOutput*/)> TaskCompleteCallback)
{
	check(InComponent);
	FPCGTaskId FinalTaskID = Schedule(InComponent, {});

	const bool bNonPartitionedComponent = !InComponent->IsLocalComponent() && !InComponent->IsPartitioned();
	const uint32 GenerationGridSize = bNonPartitionedComponent ? PCGHiGenGrid::UninitializedGridSize() : InComponent->GetGenerationGridSize();

	FPCGStackContext DummyStackContext;
	TArray<FPCGGraphTask> CompiledTasks = GraphCompiler.GetCompiledTasks(InComponent->GetGraph(), GenerationGridSize, DummyStackContext, /*bIsTopGraph=*/true);
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

void FPCGGraphExecutor::NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	GraphCompiler.NotifyGraphChanged(InGraph, ChangeType);
}

void FPCGGraphExecutor::UpdateGenerationNotification()
{
	const int32 RemainingTaskNum = GetNonScheduledRemainingTaskCount();

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

#if WITH_EDITOR
	Component->StartGenerationInProgress();
#endif

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

FPCGGenericElement::FPCGGenericElement(TFunction<bool(FPCGContext*)> InOperation, const FContextAllocator& InContextAllocator)
	: Operation(InOperation)
	, ContextAllocator(InContextAllocator)
{
}

FPCGGenericElement::FPCGGenericElement(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, const FContextAllocator& InContextAllocator)
	: Operation(InOperation)
	, AbortOperation(InAbortOperation)
	, ContextAllocator(InContextAllocator)
{
}

FPCGContext* FPCGGenericElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	check(ContextAllocator);
	FPCGContext* Context = ContextAllocator(InputData, SourceComponent, Node);
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGGenericElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGenericElement::Execute);
	return Operation && Operation(Context);
}

void FPCGGenericElement::AbortInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGenericElement::Abort);
	if(AbortOperation)
	{
		AbortOperation(Context);
	}
}

namespace PCGGraphExecutor
{
#if WITH_EDITOR
	bool FPCGGridLinkageElement::operator==(const FPCGGridLinkageElement& Other) const
	{
		return FromGrid == Other.FromGrid && ToGrid == Other.ToGrid && ResourceKey == Other.ResourceKey;
	}
#endif

	bool ExecuteGridLinkage(
		EPCGHiGenGrid InGenerationGrid,
		EPCGHiGenGrid InFromGrid,
		EPCGHiGenGrid InToGrid,
		const FString& InResourceKey,
		const FName& InOutputPinLabel,
		const UPCGNode* InDownstreamNode,
		FPCGGridLinkageContext* InContext)
	{
		check(InContext);
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGGraphExecutor::ExecuteGridLinkage);

		if (!InContext->SourceComponent.IsValid())
		{
			// Source no longer exists, nothing to be done.
			return true;
		}

		// Non-hierarchical generation - no linkage required - data should just pass through.
		if (!InContext->SourceComponent->GetGraph()->IsHierarchicalGenerationEnabled()
			|| !ensure(PCGHiGenGrid::IsValidGrid(InFromGrid) || InFromGrid == EPCGHiGenGrid::Unbounded))
		{
			InContext->OutputData = InContext->InputData;
			return true;
		}

		const uint32 FromGridSize = PCGHiGenGrid::IsValidGrid(InFromGrid) ? PCGHiGenGrid::GridToGridSize(InFromGrid) : PCGHiGenGrid::UnboundedGridSize();
		const uint32 ToGridSize = PCGHiGenGrid::IsValidGrid(InToGrid) ? PCGHiGenGrid::GridToGridSize(InToGrid) : PCGHiGenGrid::UnboundedGridSize();

		// Never allow a large grid to read data from small grid - this violates hierarchy.
		if (FromGridSize < ToGridSize)
		{
#if WITH_EDITOR
			if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(InContext->SourceComponent->GetWorld()))
			{
				// Using the low level logging call because we have only a node pointer for the downstream node. Note that InContext
				// is the context for the linkage element/task, which is not represented on the graph and cannot receive graph warnings/errors.
				if (ToGridSize == PCGHiGenGrid::UnboundedGridSize())
				{
					Subsystem->GetNodeVisualLogsMutable().Log(
						*InContext->Stack,
						ELogVerbosity::Error,
						FText::Format(
							NSLOCTEXT("PCGGraphCompiler", "InvalidLinkageToUnbounded", "Could not read data across grid levels - cannot read from grid size {0} to Unbounded domain."),
							FromGridSize,
							ToGridSize));
				}
				else
				{
					Subsystem->GetNodeVisualLogsMutable().Log(
						*InContext->Stack,
						ELogVerbosity::Error,
						FText::Format(
							NSLOCTEXT("PCGGraphCompiler", "InvalidLinkageInvalidGridSizes", "Could not read data across grid levels - origin grid size {0} must be greater than destination grid size {1}. Graph default grid size may need increasing."),
							FromGridSize,
							ToGridSize));
				}
			}
#endif

			return true;
		}

		if (!!(InFromGrid & InGenerationGrid) && FromGridSize != ToGridSize)
		{
			PCGGraphExecutionLogging::LogGridLinkageTaskExecuteStore(InContext, InGenerationGrid, FromGridSize, ToGridSize, InResourceKey);

			FPCGDataCollection Data;
			Data.TaggedData = InContext->InputData.GetInputsByPin(InOutputPinLabel);
			InContext->SourceComponent->StoreOutputDataForPin(InResourceKey, Data);
		}
		else if (InToGrid == InGenerationGrid && FromGridSize != ToGridSize)
		{
			PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieve(InContext, InGenerationGrid, FromGridSize, ToGridSize, InResourceKey);

			UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(InContext->SourceComponent->GetWorld());
			if (!ensure(Subsystem))
			{
				return false;
			}

			UPCGComponent* ComponentWithData = nullptr;
			bool bComponentWithDataIsOriginalComponent = false;
			if (FromGridSize == PCGHiGenGrid::UnboundedGridSize())
			{
				ComponentWithData = InContext->SourceComponent->GetOriginalComponent();
				bComponentWithDataIsOriginalComponent = true;
			}
			else
			{
				const APCGWorldActor* PCGWorldActor = Subsystem->GetPCGWorldActor();
				const AActor* ComponentActor = InContext->SourceComponent->GetOwner();
				if (PCGWorldActor && ComponentActor)
				{
					// Get grid coords using the parent grid (FromGridSize).
					const FIntVector CellCoords = UPCGActorHelpers::GetCellCoord(ComponentActor->GetActorLocation(), FromGridSize, PCGWorldActor->bUse2DGrid);

					// Search for a transient local component if the source component is runtime managed.
					const bool bTransientComponent = InContext->SourceComponent->IsManagedByRuntimeGenSystem();
					ComponentWithData = Subsystem->GetLocalComponent(FromGridSize, CellCoords, InContext->SourceComponent->GetOriginalComponent(), bTransientComponent);
				}
			}

			if (!ComponentWithData)
			{
				// Nothing we can do currently if PCG component not present. One idea is to schedule an artifact-less execution but that
				// comes with complications - artifacts/side effects are an integral part of execution. Most likely we'll do a cleanup
				// pass of any unwanted artifacts/local-components later.
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveNoLocalComponent(InContext, InResourceKey);
				return true;
			}

			// Once we've found our component, try to retrieve the data.
			if (const FPCGDataCollection* Data = ComponentWithData->RetrieveOutputDataForPin(InResourceKey))
			{
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveSuccess(InContext, ComponentWithData, InResourceKey, Data->TaggedData.Num());
				InContext->OutputData = *Data;

				return true;
			}

			// At this point we could not get to the data, so we'll try executing the graph if we did not do that already.

			auto WakeUpLambda = [InContext, ComponentWithData]()
			{
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveWakeUp(InContext, ComponentWithData);
				InContext->bIsPaused = false;
				return true;
			};

			// If we need data from a local component but the local component is still generating, then we'll wait for it. On the other hand
			// if we need data from the original component we assume the generation has already happened because it is always scheduled before
			// the local components.
			const bool bWaitForGeneration = bComponentWithDataIsOriginalComponent ? false : ComponentWithData->IsGenerating();
			if (bWaitForGeneration)
			{
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(InContext, ComponentWithData, InResourceKey);

				// The component was already generating, but we were not asleep. Not really clear what's happening here,
				// but in any case go to sleep and wake up when it's done.
				InContext->bIsPaused = true;

				// Wake up this task after graph has generated.
				FPCGTaskId GenerationTask = ComponentWithData->GetGenerationTaskId();
				if (ensure(GenerationTask != InvalidPCGTaskId))
				{
					Subsystem->ScheduleGeneric(WakeUpLambda, InContext->SourceComponent.Get(), { ComponentWithData->GetGenerationTaskId() });
				}

				return false;
			}

			// Graph is not currently generating. If we have not already tried generating, try it once now.
			// But don't do this for the original component as this will always be scheduled before the local components.
			if (!InContext->bScheduledGraph && !bComponentWithDataIsOriginalComponent)
			{
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveScheduleGraph(InContext, ComponentWithData, InResourceKey);

				EPCGComponentGenerationTrigger GenTrigger = (InContext->SourceComponent->IsManagedByRuntimeGenSystem()) ?
					EPCGComponentGenerationTrigger::GenerateAtRuntime : EPCGComponentGenerationTrigger::GenerateOnDemand;

				// Wake up this task after graph has generated.
				const FPCGTaskId GraphTaskId = ComponentWithData->GenerateLocalGetTaskId(GenTrigger, /*bForce=*/true);
				Subsystem->ScheduleGeneric(WakeUpLambda, InContext->SourceComponent.Get(), { GraphTaskId });

				// Update state and go to sleep.
				InContext->bScheduledGraph = true;
				InContext->bIsPaused = true;
				return false;
			}
			else
			{
				// We tried generating but no luck, at this point give up.
				PCGGraphExecutionLogging::LogGridLinkageTaskExecuteRetrieveNoData(InContext, ComponentWithData, InResourceKey);
				return true;
			}
		}
		else
		{
			// Graceful no op - no linkage required.
			InContext->OutputData = InContext->InputData;
		}

		return true;
	}
}
