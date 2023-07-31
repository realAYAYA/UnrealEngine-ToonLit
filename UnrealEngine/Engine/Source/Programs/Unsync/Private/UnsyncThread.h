// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncUtil.h"

UNSYNC_THIRD_PARTY_INCLUDES_START
#include <atomic>
#if UNSYNC_USE_CONCRT
#	include <concrt.h>
#	include <concurrent_queue.h>
#	include <ppl.h>
#	ifdef Yield
#		undef Yield  // WinBase.h defines this :-/
#	endif
#else
#	include <semaphore>
#	include <thread>
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

void SchedulerSleep(uint32 Milliseconds);
void SchedulerYield();

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

}  // namespace unsync
