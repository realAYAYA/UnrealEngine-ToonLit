// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutor.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"
#include "PCGGraphCompiler.h"
#include "PCGHelpers.h"
#include "PCGInputOutputSettings.h"
#include "PCGSubgraph.h"

#include "Async/Async.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// World partition support for in-editor workflows needs these includes
#if WITH_EDITOR
#include "FileHelpers.h"
#include "WorldPartition/WorldPartition.h"
#endif

static TAutoConsoleVariable<int32> CVarMaxNumTasks(
	TEXT("pcg.MaxConcurrentTasks"),
	4096,
	TEXT("Maximum number of concurrent tasks for PCG processing"));

static TAutoConsoleVariable<float> CVarTimePerFrame(
	TEXT("pcg.FrameTime"),
	1000.0f / 60.0f,
	TEXT("Allocated time in ms per frame"));

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
	EndGenerationNotification();
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

		// Offset task node ids
		FPCGGraphCompiler::OffsetNodeIds(ScheduledTask.Tasks, NextTaskId);
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

		ScheduleLock.Unlock();
	}

	return ScheduledId;
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
		Tasks.Reset();
	}

	// TODO: change this if we support tasks that are not framebound
	bool bAnyTaskEnded = false;

	const double StartTime = FPlatformTime::Seconds();
	const double EndTime = StartTime + (CVarTimePerFrame.GetValueOnAnyThread() / 1000.0);
	const int32 MaxNumThreads = FMath::Max(0, FMath::Min(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, CVarMaxNumTasks.GetValueOnAnyThread() - 1));
	const bool bAllowMultiDispatch = CVarGraphMultithreading.GetValueOnAnyThread();

#if WITH_EDITOR
	UpdateGenerationNotification(Tasks.Num() + ReadyTasks.Num() + ActiveTasks.Num());
#endif

	while(ReadyTasks.Num() > 0 || ActiveTasks.Num() > 0 || SleepingTasks.Num() > 0)
	{
		// First: if we have free resources, move ready tasks to the active tasks
		bool bMainThreadAvailable = (ActiveTasks.Num() == 0 || !ActiveTasks[0].Element->CanExecuteOnlyOnMainThread(ActiveTasks[0].Context.Get()));
		int32 NumAvailableThreads = FMath::Max(0, MaxNumThreads - CurrentlyUsedThreads);

		const bool bMainThreadWasAvailable = bMainThreadAvailable;
		const int32 TasksAlreadyLaunchedCount = ActiveTasks.Num();

		if (bMainThreadAvailable || NumAvailableThreads > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::Execute::PrepareTasks);
			// Sort tasks by priority (highest priority should be at the end)
			// TODO
			bool bHasDispatchedTasks = false;

			auto CannotDispatchMoreTasks = [&bMainThreadAvailable, &NumAvailableThreads, &bHasDispatchedTasks, bAllowMultiDispatch]
			{
				return ((!bAllowMultiDispatch && bHasDispatchedTasks) || (!bMainThreadAvailable && NumAvailableThreads == 0));
			};

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
					const UPCGSettings* Settings = TaskInput.GetSettings(Task.Node->DefaultSettings);

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
				const UPCGSettings* TaskSettings = PCGContextHelpers::GetInputSettings<UPCGSettings>(Task.Node, TaskInput);
				FPCGDataCollection CachedOutput;
				const bool bResultAlreadyInCache = Task.Element->IsCacheable(TaskSettings) && GraphCache.GetFromCache(Task.Element.Get(), TaskInput, TaskSettings, Task.SourceComponent.Get(), CachedOutput);
#if WITH_EDITOR
				const bool bNeedsToCreateActiveTask = !bResultAlreadyInCache || TaskSettings->ExecutionMode == EPCGSettingsExecutionMode::Debug || TaskSettings->ExecutionMode == EPCGSettingsExecutionMode::Isolated;
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
					Task.Context->TaskId = Task.NodeId;
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
				if (ActiveTask.Context->NumAvailableTasks == 0)
				{
					ActiveTask.Context->NumAvailableTasks = 1 + NumAdditionalThreads;
					CurrentlyUsedThreads += ActiveTask.Context->NumAvailableTasks;
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
					ActiveTask.Context->EndTime = EndTime;
					ActiveTask.Context->bIsRunningOnMainThread = false;

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

#if WITH_EDITOR
			if (!ActiveTask.bIsBypassed)
#endif
			{
				// Store result in cache as needed - done here because it needs to be done on the main thread
				const UPCGSettings* ActiveTaskSettings = ActiveTask.Context->GetInputSettings<UPCGSettings>();
				if (ActiveTaskSettings && ActiveTask.Element->IsCacheable(ActiveTaskSettings))
				{
					GraphCache.StoreInCache(ActiveTask.Element.Get(), ActiveTask.Context->InputData, ActiveTaskSettings, ActiveTask.Context->SourceComponent.Get(), ActiveTask.Context->OutputData);
				}
			}

			check(ActiveTask.Context->NumAvailableTasks >= 0);
			CurrentlyUsedThreads -= ActiveTask.Context->NumAvailableTasks;

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
				MainThreadTask.Context->EndTime = EndTime;
				MainThreadTask.Context->bIsRunningOnMainThread = true;

#if WITH_EDITOR
				if(MainThreadTask.bIsBypassed || MainThreadTask.Element->Execute(MainThreadTask.Context.Get()))
#else
				if(MainThreadTask.Element->Execute(MainThreadTask.Context.Get()))
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

					if (bTaskDone)
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
					check(ActiveTask.Context->NumAvailableTasks > 0);
					CurrentlyUsedThreads -= ActiveTask.Context->NumAvailableTasks;
					ActiveTask.Context->NumAvailableTasks = 0;

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
			check(TaskSuccessors.IsEmpty());
			ClearResults();

#if WITH_EDITOR
			EndGenerationNotification();
#endif
		}

#if WITH_EDITOR
		// Save & release resources when running in-editor
		SaveDirtyActors();
		ReleaseUnusedActors();
#endif
	}
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

void FPCGGraphExecutor::BuildTaskInput(const FPCGGraphTask& Task, FPCGDataCollection& TaskInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphExecutor::BuildTaskInput);
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
		}
		else
		{
			TaskInput.TaggedData.Append(InputCollection.TaggedData);
		}

		if (TaskInput.TaggedData.Num() == TaggedDataOffset && InputCollection.bCancelExecutionOnEmpty)
		{
			TaskInput.bCancelExecution = true;
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
		--CountUntilGC;
		if (CountUntilGC <= 0)
		{
			CountUntilGC = 30;
			CollectGarbage(RF_NoFlags, true);
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

void FPCGGraphExecutor::UpdateGenerationNotification(int32 RemainingTaskNum)
{
	if(RemainingTaskNum > 0)
	{
		GenerationProgressNotification.Update(RemainingTaskNum);
	}
}

void FPCGGraphExecutor::EndGenerationNotification()
{
	GenerationProgressNotification.Update(0);
	// To reset the UI notification
	GenerationProgressNotification = FAsyncCompilationNotification(GetNotificationTextFormat());
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

	if (Context->Node->IsOutputPinConnected(PCGInputOutputConstants::DefaultExcludedActorsLabel))
	{
		TArray<UPCGData*> ExclusionsPCGData = Component->GetPCGExclusionData();
		for (UPCGData* ExclusionPCGData : ExclusionsPCGData)
		{
			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = ExclusionPCGData;
			TaggedData.Pin = PCGInputOutputConstants::DefaultExcludedActorsLabel;
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
