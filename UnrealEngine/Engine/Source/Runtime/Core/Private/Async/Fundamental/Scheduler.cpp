// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Fundamental/Scheduler.h"
#include "Async/Fundamental/Task.h"
#include "Async/TaskTrace.h"
#include "Logging/LogMacros.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/Fork.h"
#include "CoreGlobals.h"

extern CORE_API bool GTaskGraphUseDynamicPrioritization;

namespace LowLevelTasks
{
	DEFINE_LOG_CATEGORY(LowLevelTasks);

	thread_local FSchedulerTls::FLocalQueueType* FSchedulerTls::LocalQueue = nullptr;
	thread_local FTask* FTask::ActiveTask = nullptr;
	thread_local FSchedulerTls* FSchedulerTls::ActiveScheduler = nullptr;
	thread_local FSchedulerTls::EWorkerType FSchedulerTls::WorkerType = FSchedulerTls::EWorkerType::None;
	thread_local uint32 FSchedulerTls::BusyWaitingDepth = 0;

	FScheduler FScheduler::Singleton;

	TUniquePtr<FThread> FScheduler::CreateWorker(bool bPermitBackgroundWork, FThread::EForkable IsForkable, Private::FWaitEvent* ExternalWorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, EThreadPriority Priority, uint64 InAffinity)
	{
		uint32 WorkerId = NextWorkerId++;
		const uint32 WaitTimes[8] = { 719, 991, 1361, 1237, 1597, 953, 587, 1439 };
		uint32 WaitTime = WaitTimes[WorkerId % 8];
		uint64 ThreadAffinityMask = FPlatformAffinity::GetTaskGraphThreadMask();
		if (bPermitBackgroundWork && FPlatformAffinity::GetTaskGraphBackgroundTaskMask() != 0xFFFFFFFFFFFFFFFF)
		{
			ThreadAffinityMask = FPlatformAffinity::GetTaskGraphBackgroundTaskMask();
		}
		if (InAffinity)
		{
			// we can override the affinity!
			ThreadAffinityMask = InAffinity;
		}

		const FProcessorGroupDesc& ProcessorGroups = FPlatformMisc::GetProcessorGroupDesc();
		int32 CpuGroupCount = ProcessorGroups.NumProcessorGroups;
		uint16 CpuGroup = 0;

		//offset the first set of workers to leave space for Game, RHI and Renderthread.
		uint64 GroupWorkerId = WorkerId + 2;
		for (uint16 GroupIndex = 0; GroupIndex < CpuGroupCount; GroupIndex++)
		{
			CpuGroup = GroupIndex;

			uint32 CpusInGroup = FMath::CountBits(ProcessorGroups.ThreadAffinities[GroupIndex]);
			if (GroupWorkerId < CpusInGroup)
			{
				if (CpuGroup != 0) //pin larger groups workers to a core and leave first group as is for legacy reasons
				{
					ThreadAffinityMask = 1ull << GroupWorkerId;
				}
				break;
			}
			GroupWorkerId -= CpusInGroup;
		}
		
		return MakeUnique<FThread>
		(
			bPermitBackgroundWork ? *FString::Printf(TEXT("Background Worker #%d"), WorkerId) : *FString::Printf(TEXT("Foreground Worker #%d"), WorkerId),
			[this, ExternalWorkerEvent, ExternalWorkerLocalQueue, WaitTime, bPermitBackgroundWork]
			{ 
				WorkerMain(ExternalWorkerEvent, ExternalWorkerLocalQueue, WaitTime, bPermitBackgroundWork);
			}, 0, Priority, FThreadAffinity{ ThreadAffinityMask & ProcessorGroups.ThreadAffinities[CpuGroup], CpuGroup }, IsForkable
		);
	}

	void FScheduler::StartWorkers(uint32 NumForegroundWorkers, uint32 NumBackgroundWorkers, FThread::EForkable IsForkable, EThreadPriority InWorkerPriority,  EThreadPriority InBackgroundPriority, uint64 InWorkerAffinity, uint64 InBackgroundAffinity)
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("TaskGraphUseDynamicPrioritization="), Value))
		{
			GTaskGraphUseDynamicPrioritization = Value != 0;
		}

		if (NumForegroundWorkers == 0 && NumBackgroundWorkers == 0)
		{
			NumForegroundWorkers = FMath::Max<int32>(1, FMath::Min<int32>(2, FPlatformMisc::NumberOfWorkerThreadsToSpawn() - 1));
			NumBackgroundWorkers = FMath::Max<int32>(1, FPlatformMisc::NumberOfWorkerThreadsToSpawn() - NumForegroundWorkers);
		}

		WorkerPriority = InWorkerPriority;
		BackgroundPriority = InBackgroundPriority;

		if (InWorkerAffinity)
		{
			WorkerAffinity = InWorkerAffinity;
		}
		if (InBackgroundAffinity)
		{
			BackgroundAffinity = InBackgroundAffinity;
		}

		const bool bSupportsMultithreading = FPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance();

		uint32 OldActiveWorkers = ActiveWorkers.load(std::memory_order_relaxed);
		if(OldActiveWorkers == 0 && bSupportsMultithreading && ActiveWorkers.compare_exchange_strong(OldActiveWorkers, NumForegroundWorkers + NumBackgroundWorkers, std::memory_order_relaxed))
		{
			FScopeLock Lock(&WorkerThreadsCS);
			check(!WorkerThreads.Num());
			check(!WorkerLocalQueues.Num());
			check(!WorkerEvents.Num());		
			check(NextWorkerId == 0);

			WorkerEvents.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			WorkerLocalQueues.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			for (uint32 WorkerId = 0; WorkerId < NumForegroundWorkers; ++WorkerId)
			{
				WorkerEvents.Emplace();
				WorkerLocalQueues.Emplace(QueueRegistry, Private::ELocalQueueType::EForeground);
			}

			for (uint32 WorkerId = 0; WorkerId < NumBackgroundWorkers; ++WorkerId)
			{
				WorkerEvents.Emplace();
				WorkerLocalQueues.Emplace(QueueRegistry, Private::ELocalQueueType::EBackground);
			}

			// WorkerEvents are now ready, time to init.
			WaitingQueue[0].Init();
			WaitingQueue[1].Init();

			WorkerThreads.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			UE::Trace::ThreadGroupBegin(TEXT("Foreground Workers"));
			for (uint32 WorkerId = 0; WorkerId < NumForegroundWorkers; ++WorkerId)
			{
				WorkerThreads.Add(CreateWorker(false, IsForkable, &WorkerEvents[WorkerId], &WorkerLocalQueues[WorkerId], WorkerPriority, WorkerAffinity));
			}
			UE::Trace::ThreadGroupEnd();
			UE::Trace::ThreadGroupBegin(TEXT("Background Workers"));
			for (uint32 WorkerId = 0; WorkerId < NumBackgroundWorkers; ++WorkerId)
			{
				WorkerThreads.Add(CreateWorker(true, IsForkable, &WorkerEvents[NumForegroundWorkers + WorkerId], &WorkerLocalQueues[NumForegroundWorkers + WorkerId], GTaskGraphUseDynamicPrioritization ? WorkerPriority : BackgroundPriority, BackgroundAffinity));
			}
			UE::Trace::ThreadGroupEnd();
		}
	}

	inline FTask* FScheduler::ExecuteTask(FTask* InTask)
	{
		FTask* ParentTask = FTask::ActiveTask;
		FTask::ActiveTask = InTask;
		FTask* OutTask;

		if (!InTask->IsBackgroundTask())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteForegroundTask);
			OutTask = InTask->ExecuteTask();
		}
		else
		{
			// Dynamic priority only enables for root task when we're not inside a named thread (i.e. GT, RT)
			const bool bSkipPriorityChange = ParentTask || !GTaskGraphUseDynamicPrioritization || !FSchedulerTls::IsWorkerThread() || InTask->WasCanceledOrIsExpediting();

			FRunnableThread* RunnableThread = nullptr;
			if (!bSkipPriorityChange)
			{
				// We assume all threads executing tasks are RunnableThread and this can't be null or it will crash. 
				// Which is fine since we want to know about it sooner rather than later.
				RunnableThread = FRunnableThread::GetRunnableThread();

				checkSlow(RunnableThread && RunnableThread->GetThreadPriority() == WorkerPriority);

				TRACE_CPUPROFILER_EVENT_SCOPE(LowerThreadPriority);
				RunnableThread->SetThreadPriority(BackgroundPriority);
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteBackgroundTask);
				OutTask = InTask->ExecuteTask();
			}

			if (!bSkipPriorityChange)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RaiseThreadPriority);
				RunnableThread->SetThreadPriority(WorkerPriority);
			}
		}

		FTask::ActiveTask = ParentTask;
		return OutTask;
	}

	void FScheduler::StopWorkers(bool DrainGlobalQueue)
	{
		uint32 OldActiveWorkers = ActiveWorkers.load(std::memory_order_relaxed);
		if(OldActiveWorkers != 0 && ActiveWorkers.compare_exchange_strong(OldActiveWorkers, 0, std::memory_order_relaxed))
		{
			FScopeLock Lock(&WorkerThreadsCS);

			WaitingQueue[0].NotifyAll();
			WaitingQueue[1].NotifyAll();

			for (TUniquePtr<FThread>& Thread : WorkerThreads)
			{
				Thread->Join();
			}

			WaitingQueue[0].Shutdown();
			WaitingQueue[1].Shutdown();

			NextWorkerId = 0;
			WorkerThreads.Reset();
			WorkerLocalQueues.Reset();
			WorkerEvents.Reset();

			if (DrainGlobalQueue)
			{
				for (FTask* Task = QueueRegistry.DequeueGlobal(); Task != nullptr; Task = QueueRegistry.DequeueGlobal())
				{
					while(Task)
					{
						if ((Task = ExecuteTask(Task)) != nullptr)
						{
							verifySlow(Task->TryPrepareLaunch());
						}
					}
				}
			}
		}
	}

	void FScheduler::RestartWorkers(uint32 NumForegroundWorkers, uint32 NumBackgroundWorkers, FThread::EForkable IsForkable, EThreadPriority InWorkerPriority, EThreadPriority InBackgroundPriority, uint64 InWorkerAffinity, uint64 InBackgroundAffinity)
	{
		FScopeLock Lock(&WorkerThreadsCS);
		TemporaryShutdown.store(true, std::memory_order_release);
		StopWorkers(false);
		StartWorkers(NumForegroundWorkers, NumBackgroundWorkers, IsForkable, InWorkerPriority, InBackgroundPriority, InWorkerAffinity, InBackgroundAffinity);
		TemporaryShutdown.store(false, std::memory_order_release);
	}

	void FScheduler::LaunchInternal(FTask& Task, EQueuePreference QueuePreference, bool bWakeUpWorker)
	{
		if (ActiveWorkers.load(std::memory_order_relaxed) || TemporaryShutdown.load(std::memory_order_acquire))
		{
			const bool bIsBackgroundTask = Task.IsBackgroundTask();
			const bool bIsBackgroundWorker = FSchedulerTls::IsBackgroundWorker();
			if (bIsBackgroundTask && !bIsBackgroundWorker)
			{
				QueuePreference = EQueuePreference::GlobalQueuePreference;
			}

			bWakeUpWorker |= FSchedulerTls::LocalQueue == nullptr;

			if (FSchedulerTls::LocalQueue && QueuePreference != EQueuePreference::GlobalQueuePreference)
			{
				FSchedulerTls::LocalQueue->Enqueue(&Task, uint32(Task.GetPriority()));
			}
			else
			{
				QueueRegistry.Enqueue(&Task, uint32(Task.GetPriority()));
			}

			if (bWakeUpWorker)
			{
				if (bWakeUpWorker && !WakeUpWorker(bIsBackgroundTask) && !bIsBackgroundTask)
				{
					WakeUpWorker(true);
				}
			}
		}
		else
		{
			FTask* TaskPtr = &Task;
			while (TaskPtr)
			{
				if ((TaskPtr = ExecuteTask(TaskPtr)) != nullptr)
				{
					verifySlow(TaskPtr->TryPrepareLaunch());
				}
			}
		}
	}

#if PLATFORM_DESKTOP || !IS_MONOLITHIC
	const FTask* FTask::GetActiveTask()
	{
		return ActiveTask;
	}
#endif

	bool FSchedulerTls::IsWorkerThread() const
	{
		return WorkerType != FSchedulerTls::EWorkerType::None && ActiveScheduler == this;
	}

	bool FSchedulerTls::IsBusyWaiting()
	{
		return BusyWaitingDepth != 0;
	}

	uint32 FSchedulerTls::GetAffinityIndex()
	{
		if (LocalQueue)
		{
			return LocalQueue->GetAffinityIndex();
		}
		return ~0;
	}

	template<typename QueueType, FTask* (QueueType::*DequeueFunction)(bool), bool bIsBusyWaiting>
	bool FScheduler::TryExecuteTaskFrom(Private::FWaitEvent* WaitEvent, QueueType* Queue, Private::FOutOfWork& OutOfWork, bool bPermitBackgroundWork)
	{
		bool AnyExecuted = false;

		FTask* Task = (Queue->*DequeueFunction)(bPermitBackgroundWork);
		while (Task)
		{
			if constexpr (bIsBusyWaiting)
			{
				FTask::FInitData InitData = Task->GetInitData();
				const bool bAllowBusyWaiting = EnumHasAnyFlags(InitData.Flags, ETaskFlags::AllowBusyWaiting) || Task->WasCanceledOrIsExpediting();

				if (!bAllowBusyWaiting)
				{
					// The task is not allowed during busy waiting so requeue in a different
					// queue to make sure we won't pick it up again.
					constexpr static bool bAllowInsideBusyWaiting = false;
					QueueRegistry.Enqueue<bAllowInsideBusyWaiting>(Task, uint32(InitData.Priority));

					// Fetch another task we could potentially run.
					Task = (Queue->*DequeueFunction)(bPermitBackgroundWork);

					// Make sure we run the filtering logic again for the new task.
					continue;
				}
			}
			else
			{
				checkSlow(FTask::ActiveTask == nullptr);
			}

			if (OutOfWork.Stop())
			{
				bool bWakeUpWanted = true;
				if (WaitEvent)
				{
					// CancelWait will tell us if we need to start a new worker to replace
					// a potential wakeup we might have consumed during the cancellation.
					bWakeUpWanted = WaitingQueue[bPermitBackgroundWork].CancelWait(WaitEvent);
				}

				if (bWakeUpWanted)
				{
					if (!WakeUpWorker(bPermitBackgroundWork) && !FSchedulerTls::IsBackgroundWorker())
					{
						WakeUpWorker(!bPermitBackgroundWork);
					}
				}
			}

			AnyExecuted = true;

			// Executing a task can return a continuation.
			if ((Task = ExecuteTask(Task)) != nullptr)
			{
				verifySlow(Task->TryPrepareLaunch());

				// When busy waiting, we exit every time a task is run
				// so queue this new task for later and bail out.
				if constexpr (bIsBusyWaiting)
				{
					QueueRegistry.Enqueue(Task, uint32(Task->GetPriority()));
					return AnyExecuted;
				}
			}
		}
		return AnyExecuted;
	}

	void FScheduler::WorkerMain(Private::FWaitEvent* WorkerEvent, FSchedulerTls::FLocalQueueType* WorkerLocalQueue, uint32 WaitCycles, bool bPermitBackgroundWork)
	{
		checkSlow(FSchedulerTls::LocalQueue == nullptr);
		checkSlow(WorkerLocalQueue != nullptr);
		checkSlow(WorkerEvent != nullptr);

		FTaskTagScope WorkerScope(ETaskTag::EWorkerThread);
		FSchedulerTls::ActiveScheduler = this;

		FMemory::SetupTLSCachesOnCurrentThread();
		FSchedulerTls::WorkerType = bPermitBackgroundWork ? FSchedulerTls::EWorkerType::Background : FSchedulerTls::EWorkerType::Foreground;
		FSchedulerTls::LocalQueue = WorkerLocalQueue;

		bool bPreparingWait = false;
		Private::FOutOfWork OutOfWork;
		while (true)
		{
			bool bExecutedSomething = false;
			while(TryExecuteTaskFrom<FSchedulerTls::FLocalQueueType, &FSchedulerTls::FLocalQueueType::Dequeue, false>(WorkerEvent, WorkerLocalQueue, OutOfWork, bPermitBackgroundWork)
			   || TryExecuteTaskFrom<FSchedulerTls::FLocalQueueType, &FSchedulerTls::FLocalQueueType::DequeueSteal, false>(WorkerEvent, WorkerLocalQueue, OutOfWork, bPermitBackgroundWork))
			{
				bPreparingWait = false;
				bExecutedSomething = true;
			}

			if (ActiveWorkers.load(std::memory_order_relaxed) == 0)
			{
				// Don't leave the waiting queue in a bad state
				if (OutOfWork.Stop())
				{
					WaitingQueue[bPermitBackgroundWork].CancelWait(WorkerEvent);
				}
				break;
			}

			if (bExecutedSomething == false)
			{
				if (!bPreparingWait)
				{
					OutOfWork.Start();
					WaitingQueue[bPermitBackgroundWork].PrepareWait(WorkerEvent);
					bPreparingWait = true;
				}
				else if (WaitingQueue[bPermitBackgroundWork].CommitWait(WorkerEvent, OutOfWork, WorkerSpinCycles, WaitCycles))
				{
					// Only reset this when the commit succeeded, otherwise we're backing off the commit and looking at the queue again
					bPreparingWait = false;
				}
			}
		}

		WaitingQueue[bPermitBackgroundWork].NotifyAll();

		FSchedulerTls::LocalQueue = nullptr;
		FSchedulerTls::ActiveScheduler = nullptr;
		FSchedulerTls::WorkerType = FSchedulerTls::EWorkerType::None;
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
	}

	void FScheduler::BusyWaitInternal(const FConditional& Conditional, bool ForceAllowBackgroundWork)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FScheduler::BusyWaitInternal);
		FTaskTagScope WorkerScope(ETaskTag::EWorkerThread);

		++FSchedulerTls::BusyWaitingDepth;
		ON_SCOPE_EXIT{ --FSchedulerTls::BusyWaitingDepth; };

		constexpr static bool bIsInsideBusyWait = true;
		check(ActiveWorkers.load(std::memory_order_relaxed));
		FSchedulerTls::FLocalQueueType* WorkerLocalQueue = FSchedulerTls::LocalQueue;

		uint32 WaitCount = 0;
		bool HasWokenEmergencyWorker = false;
		const bool bIsBackgroundWorker = FSchedulerTls::IsBackgroundWorker();
		bool bPermitBackgroundWork = FTask::PermitBackgroundWork() || ForceAllowBackgroundWork;
		Private::FOutOfWork OutOfWork;
		while (true)
		{
			if (WorkerLocalQueue)
			{
				while(TryExecuteTaskFrom<FSchedulerTls::FLocalQueueType, &FSchedulerTls::FLocalQueueType::Dequeue<bIsInsideBusyWait>, bIsInsideBusyWait>(nullptr, WorkerLocalQueue, OutOfWork, bPermitBackgroundWork)
 									|| TryExecuteTaskFrom<FSchedulerTls::FLocalQueueType, &FSchedulerTls::FLocalQueueType::DequeueSteal, bIsInsideBusyWait>(nullptr, WorkerLocalQueue, OutOfWork, bPermitBackgroundWork))
				{
					if (Conditional())
					{
						return;
					}
					WaitCount = 0;
				}
			}
			else
			{
				while(TryExecuteTaskFrom<FSchedulerTls::FQueueRegistry, &FSchedulerTls::FQueueRegistry::DequeueGlobal<bIsInsideBusyWait>, bIsInsideBusyWait>(nullptr, &QueueRegistry, OutOfWork, bPermitBackgroundWork)
									|| TryExecuteTaskFrom<FSchedulerTls::FQueueRegistry, &FSchedulerTls::FQueueRegistry::DequeueSteal, bIsInsideBusyWait>(nullptr, &QueueRegistry, OutOfWork, bPermitBackgroundWork))
				{
					if (Conditional())
					{
						return;
					}
					WaitCount = 0;
				}
			}

			if (Conditional())
			{
				return;
			}

			if (WaitCount < WorkerSpinCycles)
			{
				OutOfWork.Start();
				FPlatformProcess::Yield();
				FPlatformProcess::Yield();
				WaitCount++;
			}
			else if (!bPermitBackgroundWork && bIsBackgroundWorker)
			{
				bPermitBackgroundWork = true;
			}
			else
			{
				if(!HasWokenEmergencyWorker)
				{
					WakeUpWorker(true);
					HasWokenEmergencyWorker = true;
				}
				WaitCount = 0;
			}
		}
	}
}