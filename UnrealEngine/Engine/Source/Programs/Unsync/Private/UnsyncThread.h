// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"

#include "UnsyncLog.h"
#include "UnsyncUtil.h"

UNSYNC_THIRD_PARTY_INCLUDES_START
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>
#include <vector>
#if UNSYNC_USE_CONCRT
#	include <concrt.h>
#	include <concurrent_queue.h>
#	include <ppl.h>
#	ifdef Yield
#		undef Yield  // WinBase.h defines this :-/
#	endif
#else
#	include <semaphore>
#	ifdef __APPLE__
#		define UNSYNC_USE_MACH_SEMAPHORE 1
#	endif
#endif	// UNSYNC_USE_CONCRT
#if !defined(UNSYNC_USE_MACH_SEMAPHORE)
#	define UNSYNC_USE_MACH_SEMAPHORE 0
#endif
#if UNSYNC_USE_MACH_SEMAPHORE
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#endif // UNSYNC_USE_MACH_SEMAPHORE
UNSYNC_THIRD_PARTY_INCLUDES_END

namespace unsync {

static constexpr uint32 UNSYNC_MAX_TOTAL_THREADS = 64;

extern uint32 GMaxThreads;

class FConcurrencyPolicyScope
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FConcurrencyPolicyScope)
	explicit FConcurrencyPolicyScope(uint32 MaxConcurrency);
	~FConcurrencyPolicyScope();
};

struct FThreadElectScope
{
	const bool			 bValue;	  // NOLINT
	const bool			 bCondition;  // NOLINT
	std::atomic<uint64>& Counter;
	FThreadElectScope(std::atomic<uint64>& InCounter, bool bInCondition)
	: bValue(bInCondition && (InCounter.fetch_add(1) == 0))
	, bCondition(bInCondition)
	, Counter(InCounter)
	{
	}
	~FThreadElectScope()
	{
		if (bCondition)
		{
			Counter.fetch_sub(1);
		}
	}

	operator bool() const { return bValue; }
};

struct FThreadLogConfig
{
	FThreadLogConfig() : ParentThreadIndent(GLogIndent), bParentThreadVerbose(GLogVerbose) {}


	uint32				ParentThreadIndent;
	bool				bParentThreadVerbose;
	std::atomic<uint64> NumActiveVerboseLogThreads = {};

	struct FScope
	{
		FScope(FThreadLogConfig& Parent)
		: AllowVerbose(Parent.NumActiveVerboseLogThreads, Parent.bParentThreadVerbose)
		, VerboseScope(AllowVerbose.bValue)
		, IndentScope(Parent.ParentThreadIndent, true)
		{
		}
		FThreadElectScope  AllowVerbose;
		FLogVerbosityScope VerboseScope;
		FLogIndentScope	   IndentScope;
	};
};

void SchedulerSleep(uint32 Milliseconds);
void SchedulerYield();

class FThreadPool
{
public:

	using FTaskFunction = std::function<void()>;

	FThreadPool() = default;
	~FThreadPool();

	// Launches worker threads until total started worker count reaches NumWorkers.
	// Does nothing if the number of already launched workers is lower than given value.
	void StartWorkers(uint32 NumWorkers);

	// Adds a task to the FIFO queue
	void PushTask(FTaskFunction&& Fun);

	// Try to pop the next task from the queue and execute it on the current thread.
	// Returns false if queue is empty, which may happen if worker threads have picked up the tasks already.
	bool TryExecuteTask() { return DoWorkInternal(false); }

private:

	// Try to execute a task and return whether there may be more tasks to run
	bool DoWorkInternal(bool bWaitForSignal);

	FTaskFunction PopTask(bool bWaitForSignal);

	std::vector<std::thread>  Threads;
	std::deque<FTaskFunction> Tasks;

	std::mutex				Mutex;
	std::condition_variable WorkerWakeupCondition;
	std::atomic<bool>		bShutdown;
};

extern FThreadPool GThreadPool;

#if UNSYNC_USE_CONCRT

// Cooperative semaphore implementation.
// Using this is necessary to avoid deadlocks on low-core machines.
// https://docs.microsoft.com/en-us/cpp/parallel/concrt/how-to-use-the-context-class-to-implement-a-cooperative-semaphore?view=msvc-160
class FSemaphore
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FSemaphore)

	explicit FSemaphore(uint32 MaxCount) : Counter(MaxCount) {}

	~FSemaphore() {}

	void Acquire()
	{
		if (--Counter < 0)
		{
			WaitingQueue.push(concurrency::Context::CurrentContext());
			concurrency::Context::Block();
		}
	}

	void Release()
	{
		if (++Counter <= 0)
		{
			concurrency::Context* Waiting = nullptr;
			while (!WaitingQueue.try_pop(Waiting))
			{
				concurrency::Context::YieldExecution();
			}
			Waiting->Unblock();
		}
	}

private:
	std::atomic<int64>									 Counter;
	concurrency::concurrent_queue<concurrency::Context*> WaitingQueue;
};

using FTaskGroup = concurrency::task_group;

template<typename IT, typename FT>
inline void
ParallelForEach(IT ItBegin, IT ItEnd, FT F)
{
	concurrency::parallel_for_each(ItBegin, ItEnd, F);
}

#else  // UNSYNC_USE_CONCRT

#if UNSYNC_USE_MACH_SEMAPHORE

class FSemaphore
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FSemaphore)

	explicit FSemaphore(uint32 MaxCount)
	{
		kern_return_t InitResult = semaphore_create(mach_task_self(), &Native, SYNC_POLICY_FIFO, MaxCount);
		UNSYNC_ASSERTF(InitResult == KERN_SUCCESS, L"Failed to create a semaphore, error code: %d %hs", InitResult, mach_error_string(InitResult));
	}

	~FSemaphore()
	{
		kern_return_t DestroyResult = semaphore_destroy(mach_task_self(), Native);
		UNSYNC_ASSERTF(DestroyResult == KERN_SUCCESS, L"Failed to destroy a semaphore, error code: %d %hs", DestroyResult, mach_error_string(DestroyResult));
	}

	void Acquire()
	{
		kern_return_t WaitResult = semaphore_wait(Native);
		UNSYNC_ASSERTF(WaitResult == KERN_SUCCESS, L"Failed to wait for a semaphore, error code: %d %hs", WaitResult, mach_error_string(WaitResult));
	}

	void Release()
	{
		int32 SignalResult = semaphore_signal(Native);
		UNSYNC_ASSERTF(SignalResult == 0, L"Failed to signal a semaphore, error code: %d %hs", SignalResult, mach_error_string(SignalResult));
	}

	semaphore_t Native = {};
};

#else // UNSYNC_USE_MACH_SEMAPHORE

class FSemaphore
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FSemaphore)

	explicit FSemaphore(uint32 MaxCount) : Native(MaxCount) {}

	~FSemaphore() {}

	void Acquire() { Native.acquire(); }

	void Release() { Native.release(); }

private:

	std::counting_semaphore<UNSYNC_MAX_TOTAL_THREADS> Native;
};

#endif // UNSYNC_USE_MACH_SEMAPHORE

// Single-threaded task group implementation
struct FTaskGroup
{
	template<typename F>
	void run(F f)
	{
		f();
	}
	void wait() {}
};

template<typename IT, typename FT>
inline void
ParallelForEach(IT ItBegin, IT ItEnd, FT F)
{
	for (; ItBegin != ItEnd; ++ItBegin)
	{
		F(*ItBegin);
	}
}

#endif	// UNSYNC_USE_CONCRT

template<typename T, typename FT>
inline void
ParallelForEach(T& Container, FT F)
{
	ParallelForEach(std::begin(Container), std::end(Container), F);
}

}  // namespace unsync
