// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include <atomic>

#define UE_API CORE_API

namespace UE
{

/**
 * An eight-byte mutex that is not fair and supports recursive locking.
 *
 * Prefer FMutex when recursive locking is not required.
 */
class FRecursiveMutex final
{
public:
	constexpr FRecursiveMutex() = default;

	FRecursiveMutex(const FRecursiveMutex&) = delete;
	FRecursiveMutex& operator=(const FRecursiveMutex&) = delete;

	UE_API bool TryLock();
	UE_API void Lock();
	UE_API void Unlock();

private:
	union FState;

	void LockSlow(FState CurrentState, uint32 CurrentThreadId);
	void UnlockSlow(FState CurrentState);

	std::atomic<uint32> State = 0;
	std::atomic<uint32> ThreadId = 0;
};

} // UE

#undef UE_API
