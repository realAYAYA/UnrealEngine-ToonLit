// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformMemory.h"

class FString;

/**
 * This is the Windows version of a critical section. It uses an aggregate
 * CRITICAL_SECTION to implement its locking.
 */
class FWindowsCriticalSection
{
public:
	FWindowsCriticalSection(const FWindowsCriticalSection&) = delete;
	FWindowsCriticalSection& operator=(const FWindowsCriticalSection&) = delete;

	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FWindowsCriticalSection()
	{
		CA_SUPPRESS(28125);
		Windows::InitializeCriticalSection(&CriticalSection);
		Windows::SetCriticalSectionSpinCount(&CriticalSection,4000);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FWindowsCriticalSection()
	{
		Windows::DeleteCriticalSection(&CriticalSection);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock()
	{
		Windows::EnterCriticalSection(&CriticalSection);
	}

	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		if (Windows::TryEnterCriticalSection(&CriticalSection))
		{
			return true;
		}
		return false;
	}

	/**
	 * Releases the lock on the critical section
	 * 
	 * Calling this when not locked is undefined behavior & may cause indefinite waiting on next lock.
	 * See: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-leavecriticalsection#remarks
	 */
	FORCEINLINE void Unlock()
	{
		Windows::LeaveCriticalSection(&CriticalSection);
	}

private:
	/**
	 * The windows specific critical section
	 */
	Windows::CRITICAL_SECTION CriticalSection;
};

/** System-Wide Critical Section for windows using mutex */
class FWindowsSystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	CORE_API explicit FWindowsSystemWideCriticalSection(const class FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	CORE_API ~FWindowsSystemWideCriticalSection();

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if obtained. WARNING: Returns true for an owned but previously abandoned locks so shared resources can be in undetermined states. You must handle shared data robustly.
	 */
	CORE_API bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	CORE_API void Release();

private:
	FWindowsSystemWideCriticalSection(const FWindowsSystemWideCriticalSection&);
	FWindowsSystemWideCriticalSection& operator=(const FWindowsSystemWideCriticalSection&);

private:
	Windows::HANDLE Mutex;
};

/**
 * FWindowsRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 *	- Windows specific lock structures/calls Ref: https://msdn.microsoft.com/en-us/library/windows/desktop/aa904937(v=vs.85).aspx
 */
class FWindowsRWLock
{
public:
	FWindowsRWLock(const FWindowsRWLock&) = delete;
	FWindowsRWLock& operator=(const FWindowsRWLock&) = delete;

	FORCEINLINE FWindowsRWLock(uint32 Level = 0)
	{
		Windows::InitializeSRWLock(&Mutex);
	}

	~FWindowsRWLock()
	{
		checkf(!IsLocked(), TEXT("Destroying a lock that is still held!"));
	}

	FORCEINLINE void ReadLock()
	{
		Windows::AcquireSRWLockShared(&Mutex);
	}

	FORCEINLINE void WriteLock()
	{
		Windows::AcquireSRWLockExclusive(&Mutex);
	}

	FORCEINLINE bool TryReadLock()
	{
		return !!Windows::TryAcquireSRWLockShared(&Mutex);
	}

	FORCEINLINE bool TryWriteLock()
	{
		return !!Windows::TryAcquireSRWLockExclusive(&Mutex);
	}

	FORCEINLINE void ReadUnlock()
	{
		Windows::ReleaseSRWLockShared(&Mutex);
	}

	FORCEINLINE void WriteUnlock()
	{
		Windows::ReleaseSRWLockExclusive(&Mutex);
	}

private:

	bool IsLocked()
	{
		if (Windows::TryAcquireSRWLockExclusive(&Mutex))
		{
			Windows::ReleaseSRWLockExclusive(&Mutex);
			return false;
		}
		else
		{
			return true;
		}
	}

	Windows::SRWLOCK Mutex;
};

typedef FWindowsCriticalSection FCriticalSection;
typedef FWindowsSystemWideCriticalSection FSystemWideCriticalSection;
typedef FWindowsRWLock FRWLock;
