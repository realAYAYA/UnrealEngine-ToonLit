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

	FScheduler::FLocalQueueInstaller::FLocalQueueInstaller(FScheduler& Scheduler)
	{
		RegisteredLocalQueue = FSchedulerTls::LocalQueue == nullptr;
		if (RegisteredLocalQueue)
		{
			bool bPermitBackgroundWork = FTask::PermitBackgroundWork();
			FSchedulerTls::LocalQueue = FSchedulerTls::FLocalQueueType::AllocateLocalQueue(Scheduler.QueueRegistry, bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground);
		}
	}

	FScheduler::FLocalQueueInstaller::~FLocalQueueInstaller()
	{
		if (RegisteredLocalQueue)
		{
			bool bPermitBackgroundWork = FTask::PermitBackgroundWork();
			FSchedulerTls::FLocalQueueType::DeleteLocalQueue(FSchedulerTls::LocalQueue, bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground);
			FSchedulerTls::LocalQueue = nullptr;
		}
	}

	TUniquePtr<FThread> FScheduler::CreateWorker(bool bPermitBackgroundWork, FThread::EForkable IsForkable, FSleepEvent* ExternalWorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, EThreadPriority Priority, uint64 InAffinity)
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

		//offset the firt set of workers to leave space for Game, RHI and Renderthread.
		uint64 GroupWorkerId = WorkerId + 2;
		for (uint16 GroupIndex = 0; GroupIndex < CpuGroupCount; GroupIndex++)
		{
			CpuGroup = GroupIndex;

			uint32 CpusInGroup = FMath::CountBits(ProcessorGroups.ThreadAffinities[GroupIndex]);
			if(GroupWorkerId < CpusInGroup)
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

			WorkerThreads.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			WorkerLocalQueues.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			WorkerEvents.Reserve(NumForegroundWorkers + NumBackgroundWorkers);
			UE::Trace::ThreadGroupBegin(TEXT("Foreground Workers"));
			for (uint32 WorkerId = 0; WorkerId < NumForegroundWorkers; ++WorkerId)
			{
				WorkerEvents.Emplace();
				WorkerLocalQueues.Emplace(QueueRegistry, ELocalQueueType::EForeground, &WorkerEvents.Last());
				WorkerThreads.Add(CreateWorker(false, IsForkable, &WorkerEvents.Last(), &WorkerLocalQueues.Last(), WorkerPriority, WorkerAffinity));
			}
			UE::Trace::ThreadGroupEnd();
			UE::Trace::ThreadGroupBegin(TEXT("Background Workers"));
			for (uint32 WorkerId = 0; WorkerId < NumBackgroundWorkers; ++WorkerId)
			{
				WorkerEvents.Emplace();
				WorkerLocalQueues.Emplace(QueueRegistry, ELocalQueueType::EBackground, &WorkerEvents.Last());
				WorkerThreads.Add(CreateWorker(true, IsForkable, &WorkerEvents.Last(), &WorkerLocalQueues.Last(), GTaskGraphUseDynamicPrioritization ? WorkerPriority : BackgroundPriority, BackgroundAffinity));
			}
			UE::Trace::ThreadGroupEnd();
		}
	}

	inline void FScheduler::ExecuteTask(FTask*& InOutTask)
	{
		FTask* ParentTask = FTask::ActiveTask;
		FTask::ActiveTask = InOutTask;

		if (!InOutTask->IsBackgroundTask())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteForegroundTask);
			InOutTask = InOutTask->ExecuteTask();
		}
		else
		{
			// Dynamic priority only enables for root task when we're not inside a named thread (i.e. GT, RT)
			const bool bSkipPriorityChange = ParentTask || !GTaskGraphUseDynamicPrioritization || !FSchedulerTls::IsWorkerThread();

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
				InOutTask = InOutTask->ExecuteTask();
			}

			if (!bSkipPriorityChange)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RaiseThreadPriority);
				RunnableThread->SetThreadPriority(WorkerPriority);
			}
		}

		FTask::ActiveTask = ParentTask;
	}

	void FScheduler::StopWorkers(bool DrainGlobalQueue)
	{
		uint32 OldActiveWorkers = ActiveWorkers.load(std::memory_order_relaxed);
		if(OldActiveWorkers != 0 && ActiveWorkers.compare_exchange_strong(OldActiveWorkers, 0, std::memory_order_relaxed))
		{
			FScopeLock Lock(&WorkerThreadsCS);
			while (WakeUpWorker(true)) {}
			while (WakeUpWorker(false)) {}

			for (TUniquePtr<FThread>& Thread : WorkerThreads)
			{
				Thread->Join();
			}
			NextWorkerId = 0;
			WorkerThreads.Reset();
			WorkerLocalQueues.Reset();
			WorkerEvents.Reset();

			if (DrainGlobalQueue)
			{
				for (FTask* Task = QueueRegistry.Dequeue(); Task != nullptr; Task = QueueRegistry.Dequeue())
				{
					while(Task)
					{
						ExecuteTask(Task);
						if(Task)
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
				if (FSchedulerTls::LocalQueue->Enqueue(&Task, uint32(Task.GetPriority())))
				{
					if(bWakeUpWorker && !WakeUpWorker(bIsBackgroundTask) && !bIsBackgroundTask)
					{
						WakeUpWorker(true);
					}
				}
			}
			else
			{
				if (QueueRegistry.Enqueue(&Task, uint32(Task.GetPriority())))
				{
					if (bWakeUpWorker && !WakeUpWorker(bIsBackgroundTask) && !bIsBackgroundTask)
					{
						WakeUpWorker(true);
					}
				}
			}
		}
		else
		{
			FTask* TaskPtr = &Task;
			while(TaskPtr)
			{
				ExecuteTask(TaskPtr);
				if(TaskPtr)
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

	template<FTask* (FSchedulerTls::FLocalQueueType::*DequeueFunction)(bool, bool), bool bIsBusyWaiting>
	bool FScheduler::TryExecuteTaskFrom(FSchedulerTls::FLocalQueueType* Queue, FSchedulerTls::FQueueRegistry::FOutOfWork& OutOfWork, bool bPermitBackgroundWork, bool bDisableThrottleStealing)
	{
		bool AnyExecuted = false;

		FTask* Task = (Queue->*DequeueFunction)(bPermitBackgroundWork, bDisableThrottleStealing);
		while (Task)
		{
			if constexpr (bIsBusyWaiting)
			{
				FTask::FInitData InitData = Task->GetInitData();
				bool bAllowBusyWaiting = EnumHasAnyFlags(InitData.Flags, ETaskFlags::AllowBusyWaiting);
				
				if (!bAllowBusyWaiting || AnyExecuted) 
				{
					//either the task is not allowed during busy waiting or we have a  
					//symetric switching task that we do not want to execute during BusyWaiting
					//in either case we requeue and try again or we exit early with success.
					QueueRegistry.Enqueue(Task, uint32(InitData.Priority));

					if(AnyExecuted)
					{
						return AnyExecuted; 
					}

					Task = (Queue->*DequeueFunction)(bPermitBackgroundWork, bDisableThrottleStealing);
					if(Task == nullptr)
					{
						return false;
					}
				}
			}
			else
			{
				checkSlow(FTask::ActiveTask == nullptr);
			}

			if (OutOfWork.Stop())
			{
				//if we are ramping up work again we start waking up workers that might not have been woken while a lot of work might have been queued
				if (!WakeUpWorker(bPermitBackgroundWork) && !FSchedulerTls::IsBackgroundWorker())
				{
					WakeUpWorker(!bPermitBackgroundWork);
				}
			}
			
			ExecuteTask(Task);
			if(Task)
			{
				verifySlow(Task->TryPrepareLaunch());
			}
			AnyExecuted = true;
		}
		return AnyExecuted;
	}

	void FScheduler::WorkerMain(FSleepEvent* WorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, uint32 WaitCycles, bool bPermitBackgroundWork)
	{
		FTaskTagScope WorkerScope(ETaskTag::EWorkerThread);
		FSchedulerTls::ActiveScheduler = this;

		FMemory::SetupTLSCachesOnCurrentThread();
		FSchedulerTls::WorkerType = bPermitBackgroundWork ? FSchedulerTls::EWorkerType::Background : FSchedulerTls::EWorkerType::Foreground;

		FSleepEvent LocalWorkerEvent;
		if (!WorkerEvent)
		{
			WorkerEvent = &LocalWorkerEvent;
		}

		checkSlow(FSchedulerTls::LocalQueue == nullptr);
		if(ExternalWorkerLocalQueue)
		{
			FSchedulerTls::LocalQueue = ExternalWorkerLocalQueue;
		}
		else
		{
			FSchedulerTls::LocalQueue = FSchedulerTls::FLocalQueueType::AllocateLocalQueue(QueueRegistry, bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground, WorkerEvent);
		}

		FSchedulerTls::FLocalQueueType* WorkerLocalQueue = FSchedulerTls::LocalQueue;

		bool bDrowsing = false;
		uint32 WaitCount = 0;
		FSchedulerTls::FQueueRegistry::FOutOfWork OutOfWork = QueueRegistry.GetOutOfWorkScope(bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground);
		while (true)
		{
			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal,  false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, bDrowsing)
			   || TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueGlobal, false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, bDrowsing))
			{		
				bDrowsing = false;
				WaitCount = 0;
			}

			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal, false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, bDrowsing)
			   || TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueSteal, false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, bDrowsing))
			{
				bDrowsing = false;
				WaitCount = 0;
			}

			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal, false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, bDrowsing)
				|| TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueAffinity, false>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true))
			{
				bDrowsing = false;
				WaitCount = 0;
			}

			if (ActiveWorkers.load(std::memory_order_relaxed) == 0)
			{
				break;
			}

			if (WaitCount == 0 && !bDrowsing)
			{
				verifySlow(TrySleeping(WorkerEvent, OutOfWork.Start(), false, bPermitBackgroundWork));
				WaitCount++;
			}
			else if (WaitCount < WorkerSpinCycles)
			{
				FPlatformProcess::YieldCycles(WaitCycles);
				WaitCount++;
			}
			else
			{
				bDrowsing = TrySleeping(WorkerEvent, OutOfWork.Stop(), bDrowsing, bPermitBackgroundWork);
			}			
		}

		while (WakeUpWorker(bPermitBackgroundWork)) {}

		FSchedulerTls::FLocalQueueType::DeleteLocalQueue(WorkerLocalQueue, bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground, ExternalWorkerLocalQueue != nullptr);
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

		checkSlow(FSchedulerTls::LocalQueue != nullptr);
		check(ActiveWorkers.load(std::memory_order_relaxed));
		FSchedulerTls::FLocalQueueType* WorkerLocalQueue = FSchedulerTls::LocalQueue;

		uint32 WaitCount = 0;
		bool HasWokenEmergencyWorker = false;
		const bool bIsBackgroundWorker = FSchedulerTls::IsBackgroundWorker();
		bool bPermitBackgroundWork = FTask::PermitBackgroundWork() || ForceAllowBackgroundWork;
		FSchedulerTls::FQueueRegistry::FOutOfWork OutOfWork = QueueRegistry.GetOutOfWorkScope(bPermitBackgroundWork ? ELocalQueueType::EBackground : ELocalQueueType::EForeground);
		while (true)
		{
			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true)
			   || TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueGlobal, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true))
			{
				if (Conditional())
				{
					return;
				}
				WaitCount = 0;
			}

			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true)
			   || TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueSteal, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true))
			{
				if (Conditional())
				{
					return;
				}
				WaitCount = 0;
			}

			while(TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueLocal, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true)
				|| TryExecuteTaskFrom<&FSchedulerTls::FLocalQueueType::DequeueAffinity, true>(WorkerLocalQueue, OutOfWork, bPermitBackgroundWork, true))
			{
				if (Conditional())
				{
					return;
				}
				WaitCount = 0;
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