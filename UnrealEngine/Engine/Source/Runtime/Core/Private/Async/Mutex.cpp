// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Mutex.h"

#include "Async/ParkingLot.h"
#include "HAL/PlatformProcess.h"

namespace UE
{

void FMutex::LockSlow()
{
	constexpr int32 SpinLimit = 40;
	int32 SpinCount = 0;
	for (;;)
	{
		uint8 CurrentState = State.load(std::memory_order_acquire);

		// Try to acquire the lock if it was unlocked, even if there are waiting threads.
		// Acquiring the lock despite the waiting threads means that this lock is not FIFO and thus not fair.
		if (LIKELY(!(CurrentState & IsLockedFlag)))
		{
			if (LIKELY(State.compare_exchange_weak(CurrentState, CurrentState | IsLockedFlag, std::memory_order_acquire)))
			{
				return;
			}
			continue;
		}

		if (LIKELY(!(CurrentState & HasWaitingThreadsFlag)))
		{
			// Spin up to the spin limit while there are no waiting threads.
			if (LIKELY(SpinCount < SpinLimit))
			{
				FPlatformProcess::YieldThread();
				++SpinCount;
				continue;
			}

			// Store that there are waiting threads. Restart if the state has changed since it was loaded.
			if (UNLIKELY(!State.compare_exchange_weak(CurrentState, CurrentState | HasWaitingThreadsFlag, std::memory_order_acquire)))
			{
				continue;
			}
			CurrentState |= HasWaitingThreadsFlag;
		}

		// Wait if the state has not changed. Either way, loop back and try to acquire the lock trying to wait.
		ParkingLot::Wait(&State, [this, CurrentState] { return State.load(std::memory_order_acquire) == CurrentState; }, []{});
	}
}

void FMutex::UnlockSlow()
{
	uint8 CurrentState = State.load(std::memory_order_acquire);
	checkSlow(CurrentState & IsLockedFlag);

	// Spin on the fast path because there may be spurious failures.
	while (LIKELY(CurrentState == IsLockedFlag))
	{
		if (LIKELY(State.compare_exchange_weak(CurrentState, 0, std::memory_order_release)))
		{
			return;
		}
	}

	// There is at least one thread waiting. Wake one thread and return.
	ParkingLot::WakeOne(&State, [this](ParkingLot::FWakeState WakeState) -> uint64
	{
		constexpr uint8 ExpectedState = IsLockedFlag | HasWaitingThreadsFlag;
		const uint8 NewState = WakeState.bHasWaitingThreads ? HasWaitingThreadsFlag : 0;
		uint8 OldState = ExpectedState;
		State.compare_exchange_strong(OldState, NewState, std::memory_order_release);
		checkSlow(OldState == ExpectedState);
		return 0;
	});
}

} // UE
