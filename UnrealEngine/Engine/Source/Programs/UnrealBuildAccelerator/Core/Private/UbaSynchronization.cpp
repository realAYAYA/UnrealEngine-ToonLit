// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSynchronization.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"

namespace uba
{

	CriticalSection::CriticalSection()
	{
		#if PLATFORM_WINDOWS
		static_assert(alignof(CRITICAL_SECTION) == alignof(CriticalSection));
		static_assert(sizeof(data) >= sizeof(CRITICAL_SECTION));
		InitializeCriticalSection((CRITICAL_SECTION*)&data);
		#else
		static_assert(alignof(pthread_mutex_t) == alignof(CriticalSection));
		static_assert(sizeof(data) >= sizeof(pthread_mutex_t));
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		int res = pthread_mutex_init((pthread_mutex_t*)data, &attr);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_mutex_init failed: %i"), res);
		#endif
	}

	CriticalSection::~CriticalSection()
	{
		#if PLATFORM_WINDOWS
		#if UBA_DEBUG
		if (TryEnterCriticalSection((CRITICAL_SECTION*)&data))
			LeaveCriticalSection((CRITICAL_SECTION*)&data);
		else
			UBA_ASSERT(false);
		#endif
		DeleteCriticalSection((CRITICAL_SECTION*)&data);
		#else
		int res = pthread_mutex_destroy((pthread_mutex_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_mutex_destroy failed: %i"), res);
		#endif
	}

	void CriticalSection::Enter()
	{
		#if PLATFORM_WINDOWS
		EnterCriticalSection((CRITICAL_SECTION*)&data);
		#else
		pthread_mutex_lock((pthread_mutex_t*)data);
		#endif
	}

	void CriticalSection::Leave()
	{
		#if PLATFORM_WINDOWS
		LeaveCriticalSection((CRITICAL_SECTION*)&data);
		#else
		pthread_mutex_unlock((pthread_mutex_t*)data);
		#endif
	}

	ReaderWriterLock::ReaderWriterLock()
	{
		#if PLATFORM_WINDOWS
		static_assert(sizeof(data) >= sizeof(SRWLOCK));
		static_assert(alignof(SRWLOCK) == alignof(ReaderWriterLock));
		InitializeSRWLock((SRWLOCK*)&data);
		#else
		static_assert(alignof(pthread_rwlock_t) == alignof(ReaderWriterLock));
		static_assert(sizeof(data) >= sizeof(pthread_rwlock_t));
		int res = pthread_rwlock_init((pthread_rwlock_t*)data, NULL);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_init failed: %i"), res);
		#endif
	}

	ReaderWriterLock::~ReaderWriterLock()
	{
		#if PLATFORM_WINDOWS
		#if UBA_DEBUG
		if (TryAcquireSRWLockExclusive((SRWLOCK*)&data))
			ReleaseSRWLockExclusive((SRWLOCK*)&data);
		else
			UBA_ASSERT(false);
		#endif
		#else
		int res = pthread_rwlock_destroy((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_destroy failed: %i"), res);
		#endif
	}

	void ReaderWriterLock::EnterRead()
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_rdlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::LeaveRead()
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockShared((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::EnterWrite()
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_wrlock((pthread_rwlock_t*)data);
		#endif
	}

	void ReaderWriterLock::LeaveWrite()
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockExclusive((SRWLOCK*)&data);
		#else
		pthread_rwlock_unlock((pthread_rwlock_t*)data);
		#endif
	}

	#if UBA_TRACK_CONTENTION

	List<ContentionTracker>& GetContentionTrackerList()
	{
		static List<ContentionTracker> trackers;
		return trackers;
	}


	ContentionTracker& GetContentionTracker(const char* file, u64 line)
	{
		static ReaderWriterLock rwl;
		ScopedWriteLock l(rwl);
		ContentionTracker& ct = GetContentionTrackerList().emplace_back();
		ct.file = file;
		ct.line = line;
		return ct;
	}
	#endif
}
