// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaSynchronization.h"

#if !PLATFORM_WINDOWS
#include <sys/time.h>
#include <new>
#endif

namespace uba
{
#if !PLATFORM_WINDOWS
	struct EventImpl
	{
		EventImpl() = default;
		~EventImpl() { Destroy(); }

		bool Create(bool manualReset, bool shared = false)
		{
			static_assert(Atomic<u32>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");
			static_assert(Atomic<TriggerType>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");
			static_assert(Atomic<bool>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");

			UBA_ASSERTF(!m_initialized, "Can't create already created Event");
			m_manualReset = manualReset;

			pthread_mutexattr_t attrmutex;
			if (pthread_mutexattr_init(&attrmutex) != 0)
			{
				UBA_ASSERTF(false, "pthread_mutexattr_init failed");
				return false;
			}

			if (shared)
			{
				if (pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED) != 0)
				{
					UBA_ASSERTF(false, "pthread_mutexattr_setpshared failed");
					return false;
				}
				#if PLATFORM_LINUX
				if (pthread_mutexattr_setrobust(&attrmutex, PTHREAD_MUTEX_ROBUST) != 0)
				{
					UBA_ASSERTF(false, "pthread_mutexattr_setrobust failed");
					return false;
				}
				#endif
			}

			if (pthread_mutex_init(&m_mutex, &attrmutex) != 0)
			{
				UBA_ASSERTF(false, "pthread_mutex_init failed");
				return false;
			}
			pthread_mutexattr_destroy(&attrmutex);

			pthread_condattr_t attrcond;
			if (pthread_condattr_init(&attrcond) != 0)
			{
				UBA_ASSERTF(false, "pthread_condattr_init failed");
				return false;
			}

			if (shared)
			{
				if (pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED) != 0)
				{
					UBA_ASSERTF(false, "pthread_condattr_setpshared failed");
					return false;
				}
			}

			if (pthread_cond_init(&m_condition, &attrcond) != 0)
			{
				UBA_ASSERTF(false, "pthread_cond_init failed");
				pthread_mutex_destroy(&m_mutex);
				return false;
			}
			pthread_condattr_destroy(&attrcond);

			m_initialized = true;
			return true;
		}

		void Destroy()
		{
			if (!m_initialized)
				return;

			LockEventMutex();
			m_manualReset = true;
			UnlockEventMutex();
			Set();

			LockEventMutex();
			m_initialized = false;
			while (m_waitingThreads)
			{
				UnlockEventMutex();
				LockEventMutex();
			}

			pthread_cond_destroy(&m_condition);
			UnlockEventMutex();
			pthread_mutex_destroy(&m_mutex);
		}

		void Set()
		{
			if (!m_initialized)
				return;
			LockEventMutex();

			if (m_manualReset)
			{
				m_triggered = TriggerType_All;
				if (pthread_cond_broadcast(&m_condition) != 0)
					UBA_ASSERTF(false, "pthread_cond_broadcast failed");
			}
			else
			{
				m_triggered = TriggerType_One;
				if (pthread_cond_signal(&m_condition) != 0)  // may release multiple threads anyhow!
					UBA_ASSERTF(false, "pthread_cond_signal failed");
			}

			UnlockEventMutex();
		}

		void Reset()
		{
			if (!m_initialized)
				return;
			LockEventMutex();
			m_triggered = TriggerType_None;
			UnlockEventMutex();
		}

		bool IsSet(u32 timeoutMs = ~0u)
		{
			if (!m_initialized)
				return false;

			struct timeval startTime;

			// We need to know the start time if we're going to do a timed wait.
			if ((timeoutMs > 0) && (timeoutMs != ~0u))  // not polling and not infinite wait.
				gettimeofday(&startTime, NULL);

			LockEventMutex();

			// loop in case we fall through the Condition signal but someone else claims the event.
			do
			{
				if (m_triggered == TriggerType_One)
				{
					m_triggered = TriggerType_None;
					UnlockEventMutex();
					return true;
				}

				if (m_triggered == TriggerType_All)
				{
					UnlockEventMutex();
					return true;
				}

				// No event signalled yet.
				if (timeoutMs != 0)  // not just polling, wait on the condition variable.
				{
					++m_waitingThreads;
					if (timeoutMs == ~0u) // infinite wait?
					{
						if (pthread_cond_wait(&m_condition, &m_mutex) != 0)  // unlocks Mutex while blocking...
							UBA_ASSERTF(false, "pthread_cond_wait failed");
					}
					else  // timed wait.
					{
						struct timespec TimeOut;
						const int ms = int(startTime.tv_usec / 1000) + int(timeoutMs);
						TimeOut.tv_sec = (startTime.tv_sec) + (ms / 1000);
						TimeOut.tv_nsec = (ms % 1000) * 1000000;  // remainder of milliseconds converted to nanoseconds.
						int rc = pthread_cond_timedwait(&m_condition, &m_mutex, &TimeOut);    // unlocks Mutex while blocking...
						UBA_ASSERTF((rc == 0) || (rc == ETIMEDOUT), "pthread_cond_timedwait failed"); (void)rc;

						// Update timeoutMs and startTime in case we have to go again...
						struct timeval now, difference;
						gettimeofday(&now, NULL);
						SubtractTimevals(&now, &startTime, &difference);
						const u32 differenceMS = ((u32(difference.tv_sec) * 1000) + (u32(difference.tv_usec) / 1000));
						timeoutMs = ((differenceMS >= timeoutMs) ? 0 : (timeoutMs - differenceMS));
						startTime = now;
					}
					--m_waitingThreads;
					UBA_ASSERTF(m_waitingThreads >= 0, "m_waitingThreads less than 0");
				}

			} while (timeoutMs != 0);

			UnlockEventMutex();
			return false;
		}

	private:
		enum TriggerType { TriggerType_None, TriggerType_One, TriggerType_All };

		Atomic<bool> m_initialized;
		Atomic<bool> m_manualReset;
		Atomic<TriggerType> m_triggered;
		Atomic<u32> m_waitingThreads;
		pthread_mutex_t m_mutex;
		pthread_cond_t m_condition;

		inline void LockEventMutex()
		{
			int res = pthread_mutex_lock(&m_mutex);(void)res;
			UBA_ASSERTF(res == 0, "pthread_mutex_lock failed (error code %i)", res);
		}

		inline void UnlockEventMutex()
		{
			int res = pthread_mutex_unlock(&m_mutex);
			UBA_ASSERTF(res == 0, "pthread_mutex_unlock failed (error code %i)", res);
		}

		static inline void SubtractTimevals(const struct timeval* FromThis, struct timeval* SubThis, struct timeval* Difference)
		{
			if (FromThis->tv_usec < SubThis->tv_usec)
			{
				int nsec = int((SubThis->tv_usec - FromThis->tv_usec) / 1000000) + 1;
				SubThis->tv_usec -= 1000000 * nsec;
				SubThis->tv_sec += nsec;
			}

			if (FromThis->tv_usec - SubThis->tv_usec > 1000000)
			{
				int nsec = int((FromThis->tv_usec - SubThis->tv_usec) / 1000000);
				SubThis->tv_usec += 1000000 * nsec;
				SubThis->tv_sec -= nsec;
			}

			Difference->tv_sec = FromThis->tv_sec - SubThis->tv_sec;
			Difference->tv_usec = FromThis->tv_usec - SubThis->tv_usec;
		}
	};
#endif


	Event::Event()
	{
		#if PLATFORM_WINDOWS
		m_ev = nullptr;
		#else
		static_assert(sizeof(m_data) >= sizeof(EventImpl));
		new (m_data) EventImpl();
		#endif
	}

	Event::Event(bool manualReset)
	{
		#if !PLATFORM_WINDOWS
		new (m_data) EventImpl();
		#endif
		Create(manualReset, false);
	}

	Event::~Event()
	{
		Destroy();
		#if !PLATFORM_WINDOWS
		((EventImpl&)m_data).~EventImpl();
		#endif
	}

	bool Event::Create(bool manualReset, bool shared)
	{
		#if PLATFORM_WINDOWS
		m_ev = CreateEvent(nullptr, manualReset, false, NULL);
		return m_ev != nullptr;
		#else
		return ((EventImpl&)m_data).Create(manualReset, shared);
		#endif
	}

	void Event::Destroy()
	{
		#if PLATFORM_WINDOWS
		CloseHandle(m_ev);
		m_ev = nullptr;
		#else
		((EventImpl&)m_data).Destroy();
		#endif
	}

	void Event::Set()
	{
		#if PLATFORM_WINDOWS
		SetEvent(m_ev);
		#else
		((EventImpl&)m_data).Set();
		#endif
	}

	void Event::Reset()
	{
		#if PLATFORM_WINDOWS
		ResetEvent(m_ev);
		#else
		((EventImpl&)m_data).Reset();
		#endif
	}

	bool Event::IsSet(u32 timeOutMs)
	{
		#if PLATFORM_WINDOWS
		return WaitForSingleObject(m_ev, timeOutMs) == 0;
		#else
		return ((EventImpl&)m_data).IsSet(timeOutMs);
		#endif
	}

	void* Event::GetHandle()
	{
		#if PLATFORM_WINDOWS
		return m_ev;
		#else
		UBA_ASSERTF(false, "Event::GetHandle not available");
		return 0;
		#endif
	}

}
