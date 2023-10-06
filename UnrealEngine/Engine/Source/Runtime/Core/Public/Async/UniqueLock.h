// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"

namespace UE
{

struct FDeferLock final
{
	explicit FDeferLock() = default;
};

template <typename LockType>
class TUniqueLock final
{
public:
	TUniqueLock(const TUniqueLock&) = delete;
	TUniqueLock& operator=(const TUniqueLock&) = delete;

	[[nodiscard]] inline explicit TUniqueLock(LockType& Lock)
		: Mutex(Lock)
	{
		Mutex.Lock();
	}

	inline ~TUniqueLock()
	{
		Mutex.Unlock();
	}

private:
	LockType& Mutex;
};

template <typename LockType>
class TDynamicUniqueLock final
{
public:
	TDynamicUniqueLock() = default;

	TDynamicUniqueLock(const TDynamicUniqueLock&) = delete;
	TDynamicUniqueLock& operator=(const TDynamicUniqueLock&) = delete;

	[[nodiscard]] inline explicit TDynamicUniqueLock(LockType& Lock)
		: Mutex(&Lock)
	{
		Mutex->Lock();
		bLocked = true;
	}

	[[nodiscard]] inline explicit TDynamicUniqueLock(LockType& Lock, FDeferLock)
		: Mutex(&Lock)
	{
	}

	[[nodiscard]] inline TDynamicUniqueLock(TDynamicUniqueLock&& Other)
		: Mutex(Other.Mutex)
		, bLocked(Other.bLocked)
	{
		Other.Mutex = nullptr;
		Other.bLocked = false;
	}

	inline TDynamicUniqueLock& operator=(TDynamicUniqueLock&& Other)
	{
		if (bLocked)
		{
			Mutex->Unlock();
		}
		Mutex = Other.Mutex;
		bLocked = Other.bLocked;
		Other.Mutex = nullptr;
		Other.bLocked = false;
		return *this;
	}

	inline ~TDynamicUniqueLock()
	{
		if (bLocked)
		{
			Mutex->Unlock();
		}
	}

	void Lock()
	{
		check(!bLocked);
		check(Mutex);
		Mutex->Lock();
		bLocked = true;
	}

	void Unlock()
	{
		check(bLocked);
		bLocked = false;
		Mutex->Unlock();
	}

	inline bool OwnsLock() const
	{
		return bLocked;
	}

	inline explicit operator bool() const
	{
		return OwnsLock();
	}

private:
	LockType* Mutex = nullptr;
	bool bLocked = false;
};

} // UE
