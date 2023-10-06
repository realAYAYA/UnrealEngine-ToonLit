// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_UNSUPPORTED - Unsupported platform

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include <pthread.h>
#include <errno.h>

/**
 * FPThreadsRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 */
class FPThreadsRWLock
{
public:
	FPThreadsRWLock(const FPThreadsRWLock&) = delete;
	FPThreadsRWLock& operator=(const FPThreadsRWLock&) = delete;

	FPThreadsRWLock()
	{
		int Err = pthread_rwlock_init(&Mutex, nullptr);
		checkf(Err == 0, TEXT("pthread_rwlock_init failed with error: %d"), Err);
	}

	~FPThreadsRWLock()
	{
		int Err = pthread_rwlock_destroy(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_destroy failed with error: %d"), Err);
	}

	void ReadLock()
	{
		int Err = pthread_rwlock_rdlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_rdlock failed with error: %d"), Err);
	}

	void WriteLock()
	{
		int Err = pthread_rwlock_wrlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_wrlock failed with error: %d"), Err);
	}

	bool TryReadLock()
	{
		int Err = pthread_rwlock_tryrdlock(&Mutex);
		return Err == 0;
	}

	bool TryWriteLock()
	{
		int Err = pthread_rwlock_trywrlock(&Mutex);
		return Err == 0;
	}

	void ReadUnlock()
	{
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}

	void WriteUnlock()
	{
		int Err = pthread_rwlock_unlock(&Mutex);
		checkf(Err == 0, TEXT("pthread_rwlock_unlock failed with error: %d"), Err);
	}

private:
	pthread_rwlock_t Mutex;
};
