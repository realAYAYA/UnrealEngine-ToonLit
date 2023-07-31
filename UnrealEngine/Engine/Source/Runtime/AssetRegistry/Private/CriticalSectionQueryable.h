// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "HAL/PlatformTLS.h"

#include <atomic>

/** A Critical Section with additional function to query whether it is locked on the current thread. */
class FCriticalSectionQueryable
{
public:
	FCriticalSectionQueryable(const FCriticalSectionQueryable&) = delete;
	FCriticalSectionQueryable(FCriticalSectionQueryable&&) = delete;
	FCriticalSectionQueryable& operator=(const FCriticalSectionQueryable&) = delete;
	FCriticalSectionQueryable& operator=(FCriticalSectionQueryable&&) = delete;

	FCriticalSectionQueryable()
		: OwnerThreadId(0)
	{
	}

	void Lock()
	{
		Inner.Lock();
		SetOwner();
	}

	bool TryLock()
	{
		if (Inner.TryLock())
		{
			SetOwner();
			return true;
		}
		return false;
	}

	void Unlock()
	{
		check(IsLockedOnCurrentThread());
		ClearOwner();
		Inner.Unlock();
	}

	bool IsLockedOnCurrentThread() const
	{
		return OwnerThreadId.load(std::memory_order::memory_order_relaxed) == FPlatformTLS::GetCurrentThreadId();
	}

private:
	void SetOwner()
	{
		OwnerThreadId.store(FPlatformTLS::GetCurrentThreadId(), std::memory_order::memory_order_relaxed);
	}

	void ClearOwner()
	{
		OwnerThreadId.store(0, std::memory_order::memory_order_relaxed);
	}

	FCriticalSection Inner;
	std::atomic<uint32> OwnerThreadId;
};

/**
 * Identical to FScopeLock but works with an FCriticalSectionQueryable
 */
class FScopeLockQueryable
{
public:
	FScopeLockQueryable() = delete;
	FScopeLockQueryable(const FScopeLockQueryable& InScopeLock) = delete;
	FScopeLockQueryable(FScopeLockQueryable&& InScopeLock) = delete;
	FScopeLockQueryable& operator=(FScopeLockQueryable& InScopeLock) = delete;
	FScopeLockQueryable& operator=(FScopeLockQueryable&& InScopeLock) = delete;

	FScopeLockQueryable(FCriticalSectionQueryable* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->Lock();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FScopeLockQueryable()
	{
		Unlock();
	}

	void Unlock()
	{
		if (SynchObject)
		{
			SynchObject->Unlock();
			SynchObject = nullptr;
		}
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FCriticalSectionQueryable* SynchObject;
};
