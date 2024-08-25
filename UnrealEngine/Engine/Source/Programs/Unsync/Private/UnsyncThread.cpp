// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncThread.h"
#include "UnsyncUtil.h"

#include <chrono>

namespace unsync {

uint32 GMaxThreads = std::min<uint32>(UNSYNC_MAX_TOTAL_THREADS, std::thread::hardware_concurrency());

FThreadPool GThreadPool;

void
FThreadPool::StartWorkers(uint32 NumWorkers)
{
	std::unique_lock<std::mutex> LockScope(Mutex);

	while (Threads.size() < NumWorkers)
	{
		Threads.emplace_back(
			[this]()
			{
				while (DoWorkInternal(true))
				{
				}
			});
	}
}

FThreadPool::~FThreadPool()
{
	bShutdown = true;
	WorkerWakeupCondition.notify_all();

	for (std::thread& Thread : Threads)
	{
		Thread.join();
	}
}

FThreadPool::FTaskFunction
FThreadPool::PopTask(bool bWaitForSignal)
{
	std::unique_lock<std::mutex> LockScope(Mutex);

	if (bWaitForSignal)
	{
		auto WaitUntil = [this]() { return bShutdown || !Tasks.empty(); };
		WorkerWakeupCondition.wait(LockScope, WaitUntil);
	}

	FThreadPool::FTaskFunction Result;

	if (!Tasks.empty())
	{
		Result = std::move(Tasks.front());
		Tasks.pop_front();
	}

	return Result;
}

void
FThreadPool::PushTask(FTaskFunction&& Fun)
{
	if (Threads.empty())
	{
		Fun();
	}
	else
	{
		std::unique_lock<std::mutex> LockScope(Mutex);
		Tasks.push_back(std::forward<FTaskFunction>(Fun));
		WorkerWakeupCondition.notify_one();
	}
}

bool
FThreadPool::DoWorkInternal(bool bWaitForSignal)
{
	FTaskFunction Task = PopTask(bWaitForSignal);

	if (Task)
	{
		Task();
		return true;
	}
	else
	{
		return false;
	}
}

#if UNSYNC_USE_CONCRT
FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	auto Policy = Concurrency::CurrentScheduler::GetPolicy();

	const uint32 CurrentMaxConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MaxConcurrency);
	const uint32 CurrentMinConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MinConcurrency);

	MaxConcurrency = std::min(MaxConcurrency, std::thread::hardware_concurrency());
	MaxConcurrency = std::min(MaxConcurrency, CurrentMaxConcurrency);
	MaxConcurrency = std::max(1u, MaxConcurrency);

	Policy.SetConcurrencyLimits(CurrentMinConcurrency, MaxConcurrency);

	Concurrency::CurrentScheduler::Create(Policy);
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	Concurrency::CurrentScheduler::Detach();
}

void
SchedulerSleep(uint32 Milliseconds)
{
	concurrency::event E;
	E.reset();
	E.wait(Milliseconds);
}

void
SchedulerYield()
{
	concurrency::Context::YieldExecution();
}

#else  // UNSYNC_USE_CONCRT

FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	// TODO
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	// TODO
}

void
SchedulerSleep(uint32 Milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));
}

void
SchedulerYield()
{
	// TODO
}

#endif	// UNSYNC_USE_CONCRT

void
TestThread()
{
	UNSYNC_LOG(L"TestThread()");
	UNSYNC_LOG_INDENT;

	{
		UNSYNC_LOG(L"PushTask");

		const uint32		NumTasks = 1000;
		std::atomic<uint32> Counter	 = 0;

		{
			FThreadPool ThreadPool;
			ThreadPool.StartWorkers(10);

			uint32 RandomSeed = 1234;
			for (uint32 i = 0; i < NumTasks; ++i)
			{
				uint32 R = Xorshift32(RandomSeed) % 10;
				ThreadPool.PushTask(
					[R, &Counter]
					{
						SchedulerSleep(1 + R);
						Counter++;
					});
			}

			while (ThreadPool.TryExecuteTask())
			{
			}

			// thread pool destructor waits for outstanding tasks
		}

		UNSYNC_ASSERT(Counter == NumTasks)
	}
}

}  // namespace unsync
