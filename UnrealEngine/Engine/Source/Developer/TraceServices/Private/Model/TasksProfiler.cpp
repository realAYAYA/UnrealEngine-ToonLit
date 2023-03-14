// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/TasksProfiler.h"
#include "Model/TasksProfilerPrivate.h"
#include "AnalysisServicePrivate.h"
#include "Misc/ScopeLock.h"
#include "Misc/Timespan.h"
#include "Algo/BinarySearch.h"
#include "Common/Utils.h"
#include "Async/TaskGraphInterfaces.h"

namespace TraceServices
{
	FTasksProvider::FTasksProvider(IAnalysisSession& InSession)
		: Session(InSession)
		, EditableCounterProvider(EditCounterProvider(Session))
	{
#if 0
		////////////////////////////////
		// tests

		FAnalysisSessionEditScope _(Session);

		const int32 ValidThreadId = 42;
		const int32 InvalidThreadId = 0;
		const double InvalidTimestamp = 0;

		TArray64<TaskTrace::FId>& Thread = ExecutionThreads.FindOrAdd(ValidThreadId);

		auto MockTaskExecution = [this, Thread = &Thread](TaskTrace::FId TaskId, double StartedTimestamp, double FinishedTimestamp) -> FTaskInfo&
		{
			FTaskInfo& Task = GetOrCreateTask(TaskId);
			Task.Id = TaskId;
			Task.StartedTimestamp = StartedTimestamp;
			Task.FinishedTimestamp = FinishedTimestamp;
			Thread->Add(Task.Id);
			return Task;
		};

		FTaskInfo& Task1 = MockTaskExecution(0, 5, 10);
		FTaskInfo& Task2 = MockTaskExecution(1, 15, 20);
		
		check(GetTask(InvalidThreadId, InvalidTimestamp) == nullptr);
		check(GetTask(ValidThreadId, InvalidTimestamp) == nullptr);
		check(GetTask(ValidThreadId, 5) == &Task1);
		check(GetTask(ValidThreadId, 7) == &Task1);
		check(GetTask(ValidThreadId, 10) == nullptr);
		check(GetTask(ValidThreadId, 12) == nullptr);
		check(GetTask(ValidThreadId, 17) == &Task2);
		check(GetTask(ValidThreadId, 22) == nullptr);

		// reset
		FirstTaskId = TaskTrace::InvalidId;
		ExecutionThreads.Empty();
		Tasks.Empty();
#endif
	}

	void FTasksProvider::CreateCounters()
	{
		check(bCountersCreated == false);
		FAnalysisSessionEditScope _(Session);

		WaitingForPrerequisitesTasksCounter = EditableCounterProvider.CreateEditableCounter();
		WaitingForPrerequisitesTasksCounter->SetName(TEXT("Tasks::WaitingForPrerequisitesTasks"));
		WaitingForPrerequisitesTasksCounter->SetDescription(TEXT("Tasks: the number of tasks waiting for prerequisites (blocked by dependency)"));
		WaitingForPrerequisitesTasksCounter->SetIsFloatingPoint(false);

		TaskLatencyCounter = EditableCounterProvider.CreateEditableCounter();
		TaskLatencyCounter->SetName(TEXT("Tasks::TaskLatency"));
		TaskLatencyCounter->SetDescription(TEXT("Tasks: tasks latency - the time from scheduling to execution start"));
		TaskLatencyCounter->SetIsFloatingPoint(true);

		ScheduledTasksCounter = EditableCounterProvider.CreateEditableCounter();
		ScheduledTasksCounter->SetName(TEXT("Tasks::ScheduledTasks"));
		ScheduledTasksCounter->SetDescription(TEXT("Tasks: number of scheduled tasks excluding named threads (the size of the queue)"));
		ScheduledTasksCounter->SetIsFloatingPoint(false);

		NamedThreadsScheduledTasksCounter = EditableCounterProvider.CreateEditableCounter();
		NamedThreadsScheduledTasksCounter->SetName(TEXT("Tasks::NamedThreadsScheduledTasks"));
		NamedThreadsScheduledTasksCounter->SetDescription(TEXT("Tasks: number of scheduled tasks for named threads"));
		NamedThreadsScheduledTasksCounter->SetIsFloatingPoint(false);

		RunningTasksCounter = EditableCounterProvider.CreateEditableCounter();
		RunningTasksCounter->SetName(TEXT("Tasks::RunningTasks"));
		RunningTasksCounter->SetDescription(TEXT("Tasks: level of parallelism - the number of tasks being executed"));
		RunningTasksCounter->SetIsFloatingPoint(false);

		ExecutionTimeCounter = EditableCounterProvider.CreateEditableCounter();
		ExecutionTimeCounter->SetName(TEXT("Tasks::ExecutionTime"));
		ExecutionTimeCounter->SetDescription(TEXT("Tasks: execution time"));
		ExecutionTimeCounter->SetIsFloatingPoint(true);

		bCountersCreated = true;
	}

	void FTasksProvider::Init(uint32 InVersion)
	{
		Version = InVersion;

		if (!bCountersCreated)
		{
			CreateCounters();
		}
	}

	void FTasksProvider::TaskCreated(TaskTrace::FId TaskId, double Timestamp, uint32 ThreadId)
	{
		UE_LOG(LogTraceServices, Verbose, TEXT("TaskCreated(TaskId: %d, Timestamp %.6f)"), TaskId, Timestamp);

		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskCreated(TaskId %d, Timestamp %.6f) skipped"), TaskId, Timestamp);
			return;
		}

		Task->CreatedTimestamp = Timestamp;
		Task->CreatedThreadId = ThreadId;
	}

	void FTasksProvider::TaskLaunched(TaskTrace::FId TaskId, const TCHAR* DebugName, bool bTracked, int32 ThreadToExecuteOn, double Timestamp, uint32 ThreadId)
	{
		UE_LOG(LogTraceServices, Verbose, TEXT("TaskLaunched(TaskId: %d, DebugName: %s, bTracked: %d, Timestamp %.6f)"), TaskId, DebugName, bTracked, Timestamp);

		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskLaunched(TaskId %d, DebugName %s, bTracked %d, Timestamp %.6f) skipped"), TaskId, DebugName, bTracked, Timestamp);
			return;
		}

		checkf(Task->LaunchedTimestamp == FTaskInfo::InvalidTimestamp, TEXT("%d"), TaskId);
			
		if (Task->CreatedTimestamp == FTaskInfo::InvalidTimestamp) // created and launched in one go
		{
			Task->CreatedTimestamp = Timestamp;
			Task->CreatedThreadId = ThreadId;
		}

		Task->DebugName = DebugName;
		Task->bTracked = bTracked;
		Task->ThreadToExecuteOn = ThreadToExecuteOn;
		Task->LaunchedTimestamp = Timestamp;
		Task->LaunchedThreadId = ThreadId;

		if (!bCountersCreated)
		{
			CreateCounters();
		}

		WaitingForPrerequisitesTasksCounter->SetValue(Timestamp, ++WaitingForPrerequisitesTasksNum);
	}

	void FTasksProvider::TaskScheduled(TaskTrace::FId TaskId, double Timestamp, uint32 ThreadId)
	{
		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskScheduled(TaskId %d, Timestamp %.6f) skipped"), TaskId, Timestamp);
			return;
		}

		TaskScheduled(*Task, Timestamp, ThreadId);
	}

	void FTasksProvider::TaskScheduled(FTaskInfo& Task, double Timestamp, uint32 ThreadId)
	{
		if (!TryRegisterEvent(TEXT("TaskScheduled"), Task, &FTaskInfo::ScheduledTimestamp, Timestamp, &FTaskInfo::ScheduledThreadId, ThreadId))
		{
			return;
		}

		if (!bCountersCreated)
		{
			CreateCounters();
		}

		WaitingForPrerequisitesTasksCounter->SetValue(Timestamp, --WaitingForPrerequisitesTasksNum);
		if (IsNamedThread(Task.ThreadToExecuteOn))
		{
			NamedThreadsScheduledTasksCounter->SetValue(Timestamp, ++ScheduledTasksNum);
		}
		else
		{
			ScheduledTasksCounter->SetValue(Timestamp, ++ScheduledTasksNum);
		}
	}

	void FTasksProvider::SubsequentAdded(TaskTrace::FId TaskId, TaskTrace::FId SubsequentId, double Timestamp, uint32 ThreadId)
	{
		InitTaskIdToIndexConversion(TaskId);

		// when FGraphEvent is used to wait for a notification, it doesn't have an associated task and so is not created or launched. 
		// In this case we need to create it and initialise it before registering the completion event
		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("SubsequentAdded(TaskId %d, SubsequentId %d, Timestamp %.6f) skipped, task %d doesn't exists"), TaskId, SubsequentId, Timestamp, TaskId);
			return;
		}

		FTaskInfo* SubsequentTask = TryGetOrCreateTask(SubsequentId);
		if (SubsequentTask == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("SubsequentAdded(TaskId %d, SubsequentId %d, Timestamp %.6f) skipped, subsequent task %d doesn't exists"), TaskId, SubsequentId, Timestamp, SubsequentId);
			return;
		}

		// if a subsequent is added before this task execution started, it's a subsequent that have this task as a (execution) prerequisite.
		// otherwise, it's a subsequent that have this task as a nested task (a completion prerequisite)
		if (SubsequentTask->StartedTimestamp == FTaskInfo::InvalidTimestamp || SubsequentTask->StartedTimestamp >= Timestamp)
		{
			AddRelative(TEXT("Subsequent"), TaskId, &FTaskInfo::Subsequents, SubsequentId, Timestamp, ThreadId);
			// make a backward link from the subsequent task to this task (prerequisite)
			AddRelative(TEXT("Prerequisite"), SubsequentId, &FTaskInfo::Prerequisites, TaskId, Timestamp, ThreadId);
		}
		else
		{
			AddRelative(TEXT("Parent"), TaskId, &FTaskInfo::ParentTasks, SubsequentId, Timestamp, ThreadId);
			// make a backward link from the parent task to this nested task
			AddRelative(TEXT("Nested"), SubsequentId, &FTaskInfo::NestedTasks, TaskId, Timestamp, ThreadId);
		}
	}

	void FTasksProvider::TaskStarted(TaskTrace::FId TaskId, double Timestamp, uint32 ThreadId)
	{
		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskStarted(TaskId %d, Timestamp %.6f) skipped"), TaskId, Timestamp);
			return;
		}

		if (Task->ScheduledTimestamp == FTaskInfo::InvalidTimestamp)
		{
			// optimisation for tasks that can't have dependencies, and so are scheduled immediately after launching. in this case "scheduled" trace is not sent
			TaskScheduled(*Task, Task->LaunchedTimestamp, Task->LaunchedThreadId);
		}

		if (!TryRegisterEvent(TEXT("TaskStarted"), *Task, &FTaskInfo::StartedTimestamp, Timestamp, &FTaskInfo::StartedThreadId, ThreadId))
		{
			return;
		}

		ExecutionThreads.FindOrAdd(ThreadId).Add(TaskId);

		if (!bCountersCreated)
		{
			CreateCounters();
		}

		if (IsNamedThread(Task->ThreadToExecuteOn))
		{
			NamedThreadsScheduledTasksCounter->SetValue(Timestamp, --ScheduledTasksNum);
		}
		else
		{
			ScheduledTasksCounter->SetValue(Timestamp, --ScheduledTasksNum);
		}
		RunningTasksCounter->SetValue(Timestamp, ++RunningTasksNum);

		TaskLatencyCounter->SetValue(Timestamp, Task->StartedTimestamp - Task->ScheduledTimestamp);
	}

	void FTasksProvider::TaskFinished(TaskTrace::FId TaskId, double Timestamp)
	{
		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskFinished(TaskId %d, Timestamp %.6f) skipped"), TaskId, Timestamp);
			return;
		}

		TaskFinished(*Task, Timestamp);
	}

	void FTasksProvider::TaskFinished(FTaskInfo& Task, double Timestamp)
	{
		if (!TryRegisterEvent(TEXT("TaskFinished"), Task, &FTaskInfo::FinishedTimestamp, Timestamp))
		{
			return;
		}

		if (!bCountersCreated)
		{
			CreateCounters();
		}

		RunningTasksCounter->SetValue(Timestamp, --RunningTasksNum);
		ExecutionTimeCounter->SetValue(Timestamp, Task.FinishedTimestamp - Task.StartedTimestamp);
	}

	void FTasksProvider::TaskCompleted(TaskTrace::FId TaskId, double Timestamp, uint32 ThreadId)
	{
		InitTaskIdToIndexConversion(TaskId);

		// when FGraphEvent is used to wait for a notification, it doesn't have an associated task and so is not created or launched. 
		// In this case we need to create it and initialise it before registering the completion event
		FTaskInfo* Task = TryGetOrCreateTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("TaskCompleted(TaskId %d, Timestamp %.6f) skipped"), TaskId, Timestamp);
			return;
		}

		Task->Id = TaskId;

		if (Task->ScheduledTimestamp == FTaskInfo::InvalidTimestamp) // task events are never scheduled or executed
		{
			// pretend scheduling and execution happened at the moment of completion
			Task->LaunchedTimestamp = Timestamp;
			Task->LaunchedThreadId = ThreadId;
			Task->ScheduledTimestamp = Timestamp;
			Task->ScheduledThreadId = ThreadId;
			Task->StartedTimestamp = Timestamp;
			Task->StartedThreadId = ThreadId;
			Task->FinishedTimestamp = Timestamp;
		}
		else if (Task->FinishedTimestamp == FTaskInfo::InvalidTimestamp)
		{
			// optimisation for ParalellFor tasks that are finished and completed in one go, so no need to send separate trace messages
			TaskFinished(*Task, Timestamp);
		}

		TryRegisterEvent(TEXT("TaskCompleted"), *Task, &FTaskInfo::CompletedTimestamp, Timestamp, &FTaskInfo::CompletedThreadId, ThreadId);
	}

	void FTasksProvider::TaskDestroyed(TaskTrace::FId TaskId, double Timestamp, uint32 ThreadId)
	{
		InitTaskIdToIndexConversion(TaskId);

		TryRegisterEvent(TEXT("TaskDestroyed"), TaskId, &FTaskInfo::DestroyedTimestamp, Timestamp, &FTaskInfo::DestroyedThreadId, ThreadId);
	}

	void FTasksProvider::WaitingStarted(TArray<TaskTrace::FId> InTasks, double Timestamp, uint32 ThreadId)
	{
		FWaitingForTasks Waiting;
		Waiting.Tasks = MoveTemp(InTasks);
		Waiting.StartedTimestamp = Timestamp;

		WaitingThreads.FindOrAdd(ThreadId).Add(MoveTemp(Waiting));
	}

	void FTasksProvider::WaitingFinished(double Timestamp, uint32 ThreadId)
	{
		TArray64<FWaitingForTasks>* Thread = WaitingThreads.Find(ThreadId);
		if (Thread == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("WaitingFinished task event (Thread %d, Timestamp %.6f) skipped."), ThreadId, Timestamp);
			return;
		}

		Thread->Last().FinishedTimestamp = Timestamp;
	}

	void FTasksProvider::InitTaskIdToIndexConversion(TaskTrace::FId InFirstTaskId)
	{
		check(InFirstTaskId != TaskTrace::InvalidId);
		if (FirstTaskId == TaskTrace::InvalidId)
		{
			FirstTaskId = InFirstTaskId;
		}
	}

	int64 FTasksProvider::GetTaskIndex(TaskTrace::FId TaskId) const
	{
		return (int64)TaskId - FirstTaskId;
	}

	const FTaskInfo* FTasksProvider::TryGetTask(TaskTrace::FId TaskId) const
	{
		check(TaskId != TaskTrace::InvalidId);
		int64 TaskIndex = GetTaskIndex(TaskId);
		return Tasks.IsValidIndex(TaskIndex) ? &Tasks[TaskIndex] : nullptr;
	}

	FTaskInfo* FTasksProvider::TryGetTask(TaskTrace::FId TaskId)
	{
		return const_cast<FTaskInfo*>(const_cast<const FTasksProvider*>(this)->TryGetTask(TaskId)); // reuse the const version
	}

	FTaskInfo* FTasksProvider::TryGetOrCreateTask(TaskTrace::FId TaskId)
	{
		int64 TaskIndex = GetTaskIndex(TaskId);
		// traces can race, it's possible a trace with `TaskId = X` can come first, initialize `FirstTaskId` and only then a trace with 
		// `TaskId = X - 1` arrives. This will produce `TaskIndex < 0`. Such traces can happen only at the very beginning of the capture 
		// and are ignored
		if (TaskIndex < 0)
		{
			return nullptr;
		}

		if (TaskIndex >= Tasks.Num())
		{
			Tasks.AddDefaulted(TaskIndex - Tasks.Num() + 1);
			Tasks[TaskIndex].Id = TaskId;
		}

		return &Tasks[TaskIndex];
	}

	bool FTasksProvider::IsNamedThread(int32 Thread)
	{
		return ENamedThreads::GetThreadIndex((ENamedThreads::Type)Thread) != ENamedThreads::AnyThread;
	}

	bool FTasksProvider::TryRegisterEvent(const TCHAR* EventName, TaskTrace::FId TaskId, double FTaskInfo::* TimestampPtr, double TimestampValue, uint32 FTaskInfo::* ThreadIdPtr/* = nullptr*/, uint32 ThreadIdValue/* = 0*/)
	{
		FTaskInfo* Task = TryGetTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("%s(TaskId %d, Timestamp %.6f) skipped"), EventName, TaskId, TimestampValue);
			return false;
		}

		return TryRegisterEvent(EventName, *Task, TimestampPtr, TimestampValue, ThreadIdPtr, ThreadIdValue);
	}

	bool FTasksProvider::TryRegisterEvent(const TCHAR* EventName, FTaskInfo& Task, double FTaskInfo::* TimestampPtr, double TimestampValue, uint32 FTaskInfo::* ThreadIdPtr/* = nullptr*/, uint32 ThreadIdValue/* = 0*/)
	{
		UE_LOG(LogTraceServices, Verbose, TEXT("%s(TaskId: %d, Timestamp %.6f)"), EventName, Task.Id, TimestampValue);

		checkf(Task.*TimestampPtr == FTaskInfo::InvalidTimestamp, TEXT("%s: TaskId %d, old TS %.6f, new TS %.6f"), EventName, Task.Id, Task.*TimestampPtr, TimestampValue);
		Task.*TimestampPtr = TimestampValue;
		if (ThreadIdPtr != nullptr)
		{
			Task.*ThreadIdPtr = ThreadIdValue;
		}

		return true;
	}

	void FTasksProvider::AddRelative(const TCHAR* RelationType, TaskTrace::FId TaskId, TArray<FTaskInfo::FRelationInfo> FTaskInfo::* RelationsPtr, TaskTrace::FId RelativeId, double Timestamp, uint32 ThreadId)
	{
		UE_LOG(LogTraceServices, Verbose, TEXT("%s (%d) added to TaskId: %d, Timestamp %.6f)"), RelationType, RelativeId, TaskId, Timestamp);

		InitTaskIdToIndexConversion(TaskId);

		FTaskInfo* Task = TryGetTask(TaskId);
		if (Task == nullptr)
		{
			UE_LOG(LogTraceServices, Log, TEXT("Add%s(TaskId %d, OtherId: %d, Timestamp %.6f) skipped"), RelationType, TaskId, RelativeId, Timestamp);
			return;
		}

		(Task->*RelationsPtr).Emplace(RelativeId, Timestamp, ThreadId);
	}

	/////////////////////////////////////////////////////////////////////////////////
	// ITasksProvider impl

	const FTaskInfo* FTasksProvider::TryGetTask(uint32 ThreadId, double Timestamp) const
	{
		const TArray64<TaskTrace::FId>* Thread = ExecutionThreads.Find(ThreadId);
		if (Thread == nullptr)
		{
			return nullptr;
		}

		int64 NextTaskIndex = Algo::LowerBound(*Thread, Timestamp, 
			[this](TaskTrace::FId TaskId, double Timestamp)
			{
				const FTaskInfo* Task = TryGetTask(TaskId);
				return Task != nullptr && Task->StartedTimestamp <= Timestamp;
			}
		);

		if (NextTaskIndex == 0)
		{
			return nullptr;
		}

		TaskTrace::FId TaskId = (*Thread)[NextTaskIndex - 1];
		const FTaskInfo* Task = TryGetTask(TaskId);
		return Task != nullptr && Task->FinishedTimestamp > Timestamp ? Task : nullptr;
	}

	const FWaitingForTasks* FTasksProvider::TryGetWaiting(const TCHAR* TimerName, uint32 ThreadId, double Timestamp) const
	{
		if (FCString::Strcmp(TimerName, TEXT("WaitUntilTasksComplete")) != 0 && 
			FCString::Strcmp(TimerName, TEXT("GameThreadWaitForTask")) != 0 &&
			FCString::Strcmp(TimerName, TEXT("Tasks::Wait")) != 0 &&
			FCString::Strcmp(TimerName, TEXT("Tasks::BusyWait")) != 0)
		{
			return nullptr;
		}

		const TArray64<FWaitingForTasks>* Thread = WaitingThreads.Find(ThreadId);
		if (Thread == nullptr)
		{
			return nullptr;
		}

		int64 NextWaitingIndex = Algo::LowerBound(*Thread, Timestamp,
			[this](const FWaitingForTasks& Waiting, double Timestamp)
			{
				return Waiting.StartedTimestamp <= Timestamp;
			}
		);

		if (NextWaitingIndex == 0)
		{
			return nullptr;
		}

		const FWaitingForTasks& Waiting = (*Thread)[NextWaitingIndex - 1];
		return Waiting.FinishedTimestamp > Timestamp || Waiting.FinishedTimestamp == FTaskInfo::InvalidTimestamp ? &Waiting : nullptr;
	}

	TArray<TaskTrace::FId> FTasksProvider::TryGetParallelForTasks(const TCHAR* TimerName, uint32 ThreadId, double StartTime, double EndTime) const
	{
		TArray<TaskTrace::FId> Result;

		if (FCString::Strcmp(TimerName, TEXT("ParallelFor")) != 0)
		{
			return Result;
		}

		EnumerateTasks(StartTime, EndTime, 
			[ThreadId, StartTime, EndTime, &Result] (const FTaskInfo& Task)
			{
				if (Task.LaunchedThreadId == ThreadId && Task.LaunchedTimestamp > StartTime && Task.FinishedTimestamp < EndTime)
				{
					Result.Add(Task.Id);
				}
				return ETaskEnumerationResult::Continue;
			}
		);

		return Result;
	}

	int64 FTasksProvider::GetNumTasks() const
	{
		return Tasks.Num();
	}

	void FTasksProvider::EnumerateTasks(double StartTime, double EndTime, TaskCallback Callback) const
	{
		for (const TPair<uint32, TArray64<TaskTrace::FId>>& KeyValue : ExecutionThreads)
		{
			const TArray64<TaskTrace::FId>& Thread = KeyValue.Value;
			
			// find the first task with `StartedTimestamp <= StartTime`
			int64 TaskIndex = Algo::LowerBound(Thread, StartTime, 
				[this](TaskTrace::FId TaskId, double Timestamp)
				{
					const FTaskInfo* Task = TryGetTask(TaskId);
					return Task != nullptr && Task->StartedTimestamp <= Timestamp;
				}
			);

			// check if there's a previous task whose execution overlaps `StartTime`
			if (TaskIndex != 0)
			{
				const FTaskInfo* Task = TryGetTask(Thread[TaskIndex - 1]);
				if (Task != nullptr && Task->FinishedTimestamp > StartTime)
				{
					--TaskIndex;
				}
			}

			if (TaskIndex == Thread.Num())
			{
				continue; // all tasks on this thread are before StartTime so nothing to do here, go to the next thread
			}

			// report all tasks whose execution overlaps [StartTime, EndTime]
			const FTaskInfo* Task = TryGetTask(Thread[TaskIndex]);
			while (Task != nullptr && Task->StartedTimestamp <= EndTime && Callback(*Task) != ETaskEnumerationResult::Stop && TaskIndex < Thread.Num() - 1)
			{
				++TaskIndex;
				Task = TryGetTask(Thread[TaskIndex]);
			}
		}
	}
}
