// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/LockTags.h"
#include "CoreTypes.h"
#include <atomic>

#define UE_API CORE_API

namespace UE
{

/**
 * A one-byte mutex that is not fair and does not support recursive locking.
 * Note: Changes to this class should also be ported to FExternalMutex.
 */
class FMutex final
{
public:
	constexpr FMutex() = default;

	/** Construct in a locked state. Avoids an expensive compare-and-swap at creation time. */
	inline explicit FMutex(FAcquireLock)
		: State(IsLockedFlag)
	{
	}

	FMutex(const FMutex&) = delete;
	FMutex& operator=(const FMutex&) = delete;

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
		uint8 Expected = 0;
		if (LIKELY(State.compare_exchange_weak(Expected, IsLockedFlag, std::memory_order_acquire)))
		{
			return;
		}
		LockSlow();
	}

	inline void Unlock()
	{
		uint8 Expected = IsLockedFlag;
		if (LIKELY(State.compare_exchange_weak(Expected, 0, std::memory_order_release)))
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

	std::atomic<uint8> State = 0;
};

} // UE

#undef UE_API
