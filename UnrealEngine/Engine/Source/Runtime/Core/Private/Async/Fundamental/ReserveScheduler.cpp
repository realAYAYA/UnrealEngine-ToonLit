// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Fundamental/ReserveScheduler.h"

#include "Async/Fundamental/LocalQueue.h"
#include "Async/Fundamental/Scheduler.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Fork.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Trace/Trace.h"

extern CORE_API bool GTaskGraphUseDynamicPrioritization;

namespace LowLevelTasks
{

FReserveScheduler FReserveScheduler::Singleton;

TUniquePtr<FThread> FReserveScheduler::CreateWorker(FThread::EForkable IsForkable, FYieldedWork* ReserveEvent, EThreadPriority Priority)
{
	uint32 WorkerId = NextWorkerId++;
	return MakeUnique<FThread>
	(
		*FString::Printf(TEXT("Reserve Worker #%d"), WorkerId),
		[this, ReserveEvent]
		{
			FSchedulerTls::ActiveScheduler = this;
			FSchedulerTls::LocalQueue = nullptr;

			while (true)
			{
				EventStack.Push(ReserveEvent);
				ReserveEvent->SleepEvent->Wait();
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FReserveScheduler::BusyWaitUntil);
					FSchedulerTls::WorkerType = ReserveEvent->bPermitBackgroundWork ? FSchedulerTls::EWorkerType::Background : FSchedulerTls::EWorkerType::Foreground;

					if (ActiveWorkers.load(std::memory_order_relaxed) == 0)
					{
						break;
					}

					BusyWaitUntil(MoveTemp(ReserveEvent->CompletedDelegate), ReserveEvent->bPermitBackgroundWork);
				}
			}
			FSchedulerTls::WorkerType = EWorkerType::None;
			FSchedulerTls::ActiveScheduler = nullptr;
		}, 0, Priority, FThreadAffinity{ FPlatformAffinity::GetTaskGraphThreadMask(), 0 }, IsForkable
	);
}

bool FReserveScheduler::DoReserveWorkUntil(FConditional&& Condition)
{
	if (FYieldedWork* WorkerEvent = EventStack.Pop())
	{
		WorkerEvent->CompletedDelegate = MoveTemp(Condition);
		// become a background worker if the reserve worker is replacing a blocked background worker
		WorkerEvent->bPermitBackgroundWork = FSchedulerTls::IsBackgroundWorker();
		WorkerEvent->SleepEvent->Trigger();
		return true;
	}
	return false;
}

void FReserveScheduler::StartWorkers(uint32 NumWorkers, FThread::EForkable IsForkable, EThreadPriority WorkerPriority)
{
	if (NumWorkers == 0)
	{
		NumWorkers = FMath::Min(FPlatformMisc::NumberOfWorkerThreadsToSpawn(), 64);
	}

	WorkerPriority = GTaskGraphUseDynamicPrioritization ? LowLevelTasks::FScheduler::Get().GetWorkerPriority() : WorkerPriority;

	const bool bSupportsMultithreading = FPlatformProcess::SupportsMultithreading() || FForkProcessHelper::IsForkedMultithreadInstance();

	uint32 OldActiveWorkers = ActiveWorkers.load(std::memory_order_relaxed);
	if(OldActiveWorkers == 0 && bSupportsMultithreading && ActiveWorkers.compare_exchange_strong(OldActiveWorkers, NumWorkers, std::memory_order_relaxed))
	{
		LLM_SCOPE_BYNAME(TEXT("EngineMisc/WorkerThreads"));

		FScopeLock Lock(&WorkerThreadsCS);
		check(!WorkerThreads.Num());
		check(NextWorkerId == 0);

		ReserveEvents.AddDefaulted(NumWorkers);
		WorkerThreads.Reserve(NumWorkers);
		UE::Trace::ThreadGroupBegin(TEXT("Reserve Workers"));
		for (uint32 WorkerId = 0; WorkerId < NumWorkers; ++WorkerId)
		{
			WorkerThreads.Add(CreateWorker(IsForkable, &ReserveEvents[WorkerId], WorkerPriority));
		}
		UE::Trace::ThreadGroupEnd();
	}
}

void FReserveScheduler::StopWorkers()
{
	uint32 OldActiveWorkers = ActiveWorkers.load(std::memory_order_relaxed);
	if(OldActiveWorkers != 0 && ActiveWorkers.compare_exchange_strong(OldActiveWorkers, 0, std::memory_order_relaxed))
	{
		FScopeLock Lock(&WorkerThreadsCS);

		for (FYieldedWork& Event : ReserveEvents)
		{
			Event.SleepEvent->Trigger();
		}

		while (FYieldedWork* Event = EventStack.Pop())
		{
		}

		for (TUniquePtr<FThread>& Thread : WorkerThreads)
		{
			Thread->Join();
		}
		NextWorkerId = 0;
		WorkerThreads.Reset();
		ReserveEvents.Reset();
	}
}

}