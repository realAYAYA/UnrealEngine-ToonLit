// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/FMutableTaskGraph.h"

#include "MuCO/CustomizableObject.h"
#include "MuR/MutableTrace.h"


#ifdef MUTABLE_USE_NEW_TASKGRAPH
const LowLevelTasks::ETaskPriority TASKGRAPH_PRIORITY = LowLevelTasks::ETaskPriority::BackgroundHigh;
#else
const ENamedThreads::Type TASKGRAPH_PRIORITY = ENamedThreads::AnyBackgroundHiPriTask;
#endif


static TAutoConsoleVariable<float> CVarMutableTaskLowPriorityMaxWaitTime(
	TEXT("mutable.MutableTaskLowPriorityMaxWaitTime"),
	3.0f,
	TEXT("Max time a Mutable Task with Low priority can wait. Once this time has passed it will be launched unconditionally."),
	ECVF_Scalability);

static TAutoConsoleVariable<bool> CVarEnableMutableTaskLowPriority(
	TEXT("mutable.EnableMutableTaskLowPriority"),
	true,
	TEXT("Enable or disable Mutable Tasks with Low priority. If disabled, all task will have the same priority. "),
	ECVF_Scalability);


FMutableTaskGraph::TaskType FMutableTaskGraph::AddMutableThreadTask(const TCHAR* DebugName, TUniqueFunction<void()>&& TaskBody)
{
	FScopeLock Lock(&MutableTaskLock);

#ifdef MUTABLE_USE_NEW_TASKGRAPH
	if (LastMutableTask.IsValid())
	{
		LastMutableTask = UE::Tasks::Launch(DebugName, MoveTemp(TaskBody), LastMutableTask, TASKGRAPH_PRIORITY);
	}
	else
	{
		LastMutableTask = UE::Tasks::Launch(DebugName, MoveTemp(TaskBody), TASKGRAPH_PRIORITY);
	}
#else
	FGraphEventArray Prerequisites;
	if (LastMutableTask)
	{
		Prerequisites.Add(LastMutableTask);
	}

	LastMutableTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
		MoveTemp(TaskBody),
		TStatId{},
		&Prerequisites, 
		TASKGRAPH_PRIORITY);
	
	LastMutableTask->SetDebugName(DebugName);
#endif
	
	return LastMutableTask;
}


uint32 FMutableTaskGraph::AddMutableThreadTaskLowPriority(const TCHAR* DebugName, TFunction<void()>&& TaskBody)
{
	FScopeLock Lock(&MutableTaskLock);

	if (CVarEnableMutableTaskLowPriority.GetValueOnAnyThread())
	{
		const uint32 Id = ++TaskIdGenerator;
		const FTask Task { Id, DebugName, MoveTemp(TaskBody)};
		QueueMutableTasksLowPriority.Add(Task);		

		TryLaunchMutableTaskLowPriority(false);

		return Id;
	}
	else
	{
		AddMutableThreadTask(DebugName, TaskBody);
		return INVALID_ID;	
	}
}


void FMutableTaskGraph::CancelMutableThreadTaskLowPriority(uint32 Id)
{
	FScopeLock Lock(&MutableTaskLock);

	for (int32 Index = 0; Index < QueueMutableTasksLowPriority.Num(); ++Index)
	{
		if (QueueMutableTasksLowPriority[Index].Id == Id)
		{
			QueueMutableTasksLowPriority.RemoveAt(Index);
			break;
		}
	}
}


void FMutableTaskGraph::AddAnyThreadTask(const TCHAR* DebugName, TUniqueFunction<void()>&& TaskBody) const
{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::Launch(DebugName, MoveTemp(TaskBody));
#else
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
		MoveTemp(TaskBody),
		TStatId{},
		nullptr,
		TASKGRAPH_PRIORITY);

	Task->SetDebugName(DebugName);
#endif
}
	

void FMutableTaskGraph::WaitForMutableTasks()
{
	FScopeLock Lock(&MutableTaskLock);

	if (LastMutableTask.IsValid())
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		LastMutableTask.Wait();
#else
		LastMutableTask->Wait();
#endif
		LastMutableTask = {};
	}
}


void FMutableTaskGraph::TryLaunchMutableTaskLowPriority(bool bFromMutableTask)
{
	MUTABLE_CPUPROFILER_SCOPE(TryLaunchMutableTaskLowPriority)

	if (QueueMutableTasksLowPriority.IsEmpty())
	{
		return;		
	}
	
	if (!IsTaskCompleted(LastMutableTaskLowPriority)) // At any time only a single Low Priority task can be launched.
	{
		return;
	}

	float TimeLimit;
	float TimeElapsed;
	
	bool bTimeLimit;
	
	{
		FScopeLock Lock(&MutableTaskLock);

		if (QueueMutableTasksLowPriority.IsEmpty()) // Also checked inside the lock since we will be writing it
		{
			return;		
		}
		
		if (!IsTaskCompleted(LastMutableTaskLowPriority)) // Check #1 // Also check inside the lock since we will be writing it
		{
			return;
		}

		FTask& NextTask = QueueMutableTasksLowPriority[0];

		TimeLimit = CVarMutableTaskLowPriorityMaxWaitTime.GetValueOnAnyThread();
		TimeElapsed = FPlatformTime::Seconds() - NextTask.CreationTime;

		bTimeLimit = TimeElapsed >= TimeLimit;
		if (!bAllowLaunchMutableTaskLowPriority || // Check #2
			(bFromMutableTask && IsTaskCompleted(LastMutableTask) && !bTimeLimit)) // Check #3
		{			
			return;
		}

		LastMutableTaskLowPriority = AddMutableThreadTask(*NextTask.DebugName, [this, Task = MoveTemp(NextTask)]() // Moves the task, not the pointer.
		{
			MUTABLE_CPUPROFILER_SCOPE(LowPriorityTaskBody)
			Task.Body();

			{
				FScopeLock Lock(&MutableTaskLock);
				LastMutableTaskLowPriority = {};
				
				TryLaunchMutableTaskLowPriority(true);
			}
		});

		QueueMutableTasksLowPriority.RemoveAt(0);
	}

	if (bTimeLimit)
	{
		UE_LOG(LogMutable, Verbose, TEXT("Low Priority Mutable Task launched due to time limit (%f)! Waited for: %f"), TimeLimit, TimeElapsed)
	}
}


bool FMutableTaskGraph::IsTaskCompleted(const TaskType& Task) const
{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	return Task.IsCompleted();
#else
	return !Task.IsValid() || Task->IsComplete();
#endif
}


void FMutableTaskGraph::AllowLaunchingMutableTaskLowPriority(bool bAllow, bool bFromMutableTask)
{
	FScopeLock Lock(&MutableTaskLock);

	bAllowLaunchMutableTaskLowPriority = bAllow;

	if (bAllowLaunchMutableTaskLowPriority)
	{
		TryLaunchMutableTaskLowPriority(bFromMutableTask);
	}
}


void FMutableTaskGraph::Tick()
{
	check(IsInGameThread())
	
	TryLaunchMutableTaskLowPriority(false);

	// See if we can clear the last reference to the mutable-thread task
	if (IsTaskCompleted(LastMutableTask))
	{
		FScopeLock Lock(&MutableTaskLock);
		if (IsTaskCompleted(LastMutableTask))
		{
			LastMutableTask = {};
		}
	}

	if (IsTaskCompleted(LastMutableTaskLowPriority))
	{
		FScopeLock Lock(&MutableTaskLock);
		if (IsTaskCompleted(LastMutableTaskLowPriority))
		{
			LastMutableTaskLowPriority = {};
		}
	}
}

