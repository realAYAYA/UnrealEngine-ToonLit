// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "Misc/MonotonicTime.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace UE::HAL::Private
{

/**
 * A manual reset event that supports only one thread waiting and one thread notifying at a time.
 *
 * Only one waiting thread may call Reset() or the Wait() functions.
 * Only one notifying thread may call Notify() once until the event is reset.
 */
class FGenericPlatformManualResetEvent
{
public:
	FGenericPlatformManualResetEvent() = default;
	FGenericPlatformManualResetEvent(const FGenericPlatformManualResetEvent&) = delete;
	FGenericPlatformManualResetEvent& operator=(const FGenericPlatformManualResetEvent&) = delete;

	/**
	 * Resets the event to permit another Wait/Notify cycle.
	 *
	 * Must only be called by the waiting thread, and only when there is no possibility of waking
	 * occurring concurrently with the reset.
	 */
	void Reset()
	{
		bWait.store(true, std::memory_order_release);
	}

	/**
	 * Waits for Notify() to be called.
	 *
	 * Notify() may be called prior to Wait(), and this will return immediately in that case.
	 */
	void Wait()
	{
		WaitUntil(FMonotonicTimePoint::Infinity());
	}

	/**
	 * Waits until the wait time for Notify() to be called.
	 *
	 * Notify() may be called prior to WaitUntil(), and this will return immediately in that case.
	 *
	 * @param WaitTime   Absolute time after which waiting is canceled and the thread wakes.
	 * @return True if Notify() was called before the wait time elapsed, otherwise false.
	 */
	bool WaitUntil(FMonotonicTimePoint WaitTime)
	{
		std::unique_lock SelfLock(Lock);
		if (WaitTime.IsInfinity())
		{
			Condition.wait(SelfLock, [this] { return !bWait.load(std::memory_order_acquire); });
			return true;
		}
		if (FMonotonicTimeSpan WaitSpan = WaitTime - FMonotonicTimePoint::Now(); WaitSpan > FMonotonicTimeSpan::Zero())
		{
			const int64 WaitMs = FPlatformMath::CeilToInt64(WaitSpan.ToMilliseconds());
			return Condition.wait_for(SelfLock, std::chrono::milliseconds(WaitMs), [this] { return !bWait.load(std::memory_order_acquire); });
		}
		return !bWait.load(std::memory_order_acquire);
	}

	/**
	 * Notifies the waiting thread.
	 *
	 * Notify() may be called prior to one of the wait functions, and the eventual wait call will
	 * return immediately when that occurs.
	 */
	void Notify()
	{
		// We need this lock to ensure wait_for(SelfLock, std::chrono::milliseconds(WaitMs) does not return until Condition.notify_one() is fully called.
		// Otherwise it's possible to destroy the Event right after Notify() call and while some other thread is still waiting on it
		std::unique_lock SelfLock(Lock);
		bWait.store(false, std::memory_order_relaxed);
		Condition.notify_one();
	}

private:
	std::mutex Lock;
	std::condition_variable Condition;
	std::atomic<bool> bWait = true;
};

} // UE::HAL::Private
