// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/FMutableTaskGraph.h"

#include "MuCO/CustomizableObject.h"
#include "MuR/MutableTrace.h"


constexpr LowLevelTasks::ETaskPriority TASKGRAPH_PRIORITY = LowLevelTasks::ETaskPriority::BackgroundHigh;

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


UE::Tasks::FTask FMutableTaskGraph::AddMutableThreadTask(const TCHAR* DebugName, TUniqueFunction<void()>&& TaskBody)
{
	FScopeLock Lock(&MutableTaskLock);

	TArray<UE::Tasks::FTask, TFixedAllocator<1>> Prerequisites = LastMutableTask.IsValid() 
			? TArray<UE::Tasks::FTask, TFixedAllocator<1>>{LastMutableTask} 
			: TArray<UE::Tasks::FTask, TFixedAllocator<1>>{};

	LastMutableTask = UE::Tasks::Launch(DebugName, MoveTemp(TaskBody), Prerequisites, TASKGRAPH_PRIORITY);	
	
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


bool FMutableTaskGraph::CancelMutableThreadTaskLowPriority(uint32 Id)
{
	FScopeLock Lock(&MutableTaskLock);

	for (int32 Index = 0; Index < QueueMutableTasksLowPriority.Num(); ++Index)
	{
		if (QueueMutableTasksLowPriority[Index].Id == Id)
		{
			QueueMutableTasksLowPriority.RemoveAt(Index);
			return true;
		}
	}

	return false;
}


void FMutableTaskGraph::AddAnyThreadTask(const TCHAR* DebugName, TUniqueFunction<void()>&& TaskBody) const
{
	UE::Tasks::Launch(DebugName, MoveTemp(TaskBody));
}
	

void FMutableTaskGraph::WaitForMutableTasks()
{
	FScopeLock Lock(&MutableTaskLock);

	if (LastMutableTask.IsValid())
	{
		if (CVarTaskGraphBusyWait->GetBool())
		{
			LastMutableTask.BusyWait();
		}
		else
		{
			LastMutableTask.Wait();
		}

		LastMutableTask = {};
	}
}


void FMutableTaskGraph::WaitForLaunchedLowPriorityTask(uint32 TaskID)
{
	UE::Tasks::FTask Task;

	{
		FScopeLock Lock(&MutableTaskLock);

		if (LastMutableTaskLowPriorityID == TaskID)
		{
			Task = LastMutableTaskLowPriority;
		}
	}

	if (Task.IsValid())
	{
		Task.Wait();
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

	double TimeLimit;
	double TimeElapsed;
	
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

		LastMutableTaskLowPriorityID = NextTask.Id;
		LastMutableTaskLowPriority = AddMutableThreadTask(*NextTask.DebugName, [this, Task = MoveTemp(NextTask)]() // Moves the task, not the pointer.
		{
			MUTABLE_CPUPROFILER_SCOPE(LowPriorityTaskBody)
			Task.Body();

			{
				FScopeLock Lock(&MutableTaskLock);
				LastMutableTaskLowPriorityID = INVALID_ID;
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


bool FMutableTaskGraph::IsTaskCompleted(const UE::Tasks::FTask& Task) const
{
	return Task.IsCompleted();
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


int32 FMutableTaskGraph::Tick()
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
			LastMutableTaskLowPriorityID = INVALID_ID;
			LastMutableTaskLowPriority = {};
		}
	}
	
	return !IsTaskCompleted(LastMutableTask) + !IsTaskCompleted(LastMutableTaskLowPriority) + QueueMutableTasksLowPriority.Num();
}

