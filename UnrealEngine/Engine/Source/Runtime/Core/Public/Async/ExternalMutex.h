// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/LockTags.h"
#include "CoreTypes.h"
#include <atomic>

#define UE_API CORE_API

namespace UE
{

/**
 * A mutex which takes it state from an external source and uses only its 2 LSBs.
 * The external source must ensure that the state is valid for the lifetime of the mutex.
 * 
 * Note: Changes to this class should also be ported to FMutex.
 *		 These classes could be merged via templatization but we would want 
 *		 to make sure this doesn't cause any undesired code-bloat / side effects.
 */
class FExternalMutex final
{
public:
	inline constexpr explicit FExternalMutex(std::atomic<uint8>& InState)
		: State(InState)
	{
	}

	/** Construct in a locked state. Avoids an expensive compare-and-swap at creation time. */
	inline explicit FExternalMutex(std::atomic<uint8>& InState, FAcquireLock)
		: State(InState)
	{
		State.fetch_or(IsLockedFlag, std::memory_order_acquire);
	}

	FExternalMutex(const FExternalMutex&) = delete;
	FExternalMutex& operator=(const FExternalMutex&) = delete;

	inline bool IsLocked() const
	{
		return (State.load(std::memory_order_relaxed) & IsLockedFlag);
	}

	inline bool TryLock()
	{
		uint8 Expected = State.load(std::memory_order_acquire);
		if (LIKELY(!(Expected & IsLockedFlag) && State.compare_exchange_strong(Expected, Expected | IsLockedFlag, std::memory_order_acquire)))
		{
			return true;
		}
		return false;
	}

	inline void Lock()
	{
		uint8 Expected = State.load(std::memory_order_acquire) & ~IsLockedFlag & ~HasWaitingThreadsFlag;
		if (LIKELY(State.compare_exchange_weak(Expected, Expected | IsLockedFlag, std::memory_order_acquire)))
		{
			return;
		}
		LockSlow();
	}

	inline void Unlock()
	{
		uint8 Expected = (State.load(std::memory_order_acquire) | IsLockedFlag) & ~HasWaitingThreadsFlag;
		if (LIKELY(State.compare_exchange_weak(Expected, Expected & ~IsLockedFlag, std::memory_order_release)))
		{
			return;
		}
		UnlockSlow();
	}

private:
	UE_API void LockSlow();
	UE_API void UnlockSlow();

	static constexpr uint8 IsLockedFlag = 1 << 0;
	static constexpr uint8 HasWaitingThreadsFlag = 1 << 1;

	std::atomic<uint8>& State;
};

} // UE

#undef UE_API
