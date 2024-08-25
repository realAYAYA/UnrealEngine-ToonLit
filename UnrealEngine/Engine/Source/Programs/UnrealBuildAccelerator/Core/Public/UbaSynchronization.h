// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"
#include <atomic>
#include <utility>

#define UBA_TRACK_CONTENTION 0

#define STRING_JOIN(arg1, arg2) STRING_JOIN_INNER(arg1, arg2)
#define STRING_JOIN_INNER(arg1, arg2) arg1 ## arg2

namespace uba
{
	template<typename Type>
	using Atomic = std::atomic<Type>;

	struct AtomicU64 : Atomic<u64>
	{
		AtomicU64(u64 initialValue = 0) : Atomic<u64>(initialValue) {}
		AtomicU64(AtomicU64&& o) noexcept : Atomic<u64>(o.load()) {}
		void operator=(u64 o) { store(o); }
		void operator=(const AtomicU64& o) { store(o); }
	};

	class CriticalSection
	{
	public:
		CriticalSection();
		~CriticalSection();

		void Enter();
		void Leave();

		template<class Functor> auto Scoped(const Functor& f);

	private:
		#if PLATFORM_WINDOWS
		u64 data[5];
		#else
		u64 data[10];
		#endif

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(const CriticalSection&) = delete;
	};

	class ScopedCriticalSection 
	{
	public:
		ScopedCriticalSection(CriticalSection& cs) : m_cs(cs), m_active(true) { cs.Enter(); }
		~ScopedCriticalSection() { Leave(); }
		void Enter() { if (m_active) return; m_cs.Enter(); m_active = true; }
		void Leave() { if (!m_active) return; m_cs.Leave(); m_active = false; }
	private:
		CriticalSection& m_cs;
		bool m_active;
	};

	template<class Functor> auto CriticalSection::Scoped(const Functor& f) { ScopedCriticalSection c(*this); return f(); }

	class ReaderWriterLock
	{
	public:
		ReaderWriterLock();
		~ReaderWriterLock();

		void EnterRead();
		void LeaveRead();

		void EnterWrite();
		void LeaveWrite();

	private:

		#if PLATFORM_WINDOWS
		u64 data[1];
		#elif PLATFORM_LINUX
		u64 data[7];
		#else
		u64 data[25];
		#endif

		ReaderWriterLock(const ReaderWriterLock&) = delete;
		ReaderWriterLock& operator=(const ReaderWriterLock&) = delete;
	};

	#if !UBA_TRACK_CONTENTION
	#define SCOPED_READ_LOCK(readerWriterLock, name) ScopedReadLock name(readerWriterLock);
	#define SCOPED_WRITE_LOCK(readerWriterLock, name) ScopedWriteLock name(readerWriterLock);
	#else
	u64 GetTime();
	struct ContentionTracker { void Add(u64 t) { time += t; ++count; }; Atomic<u64> time; Atomic<u64> count; const char* file; u64 line; };
	ContentionTracker& GetContentionTracker(const char* file, u64 line);

	#define SCOPED_LOCK(readerWriterLock, lockType, name) \
		u64 STRING_JOIN(contentionStart, __LINE__) = GetTime(); \
		lockType name(readerWriterLock); \
		static ContentionTracker& STRING_JOIN(tracker, __LINE__) = GetContentionTracker(__FILE__, __LINE__); \
		STRING_JOIN(tracker, __LINE__).Add(GetTime() - STRING_JOIN(contentionStart, __LINE__));

	#define SCOPED_READ_LOCK(readerWriterLock, name) SCOPED_LOCK(readerWriterLock, ScopedReadLock, name)
	#define SCOPED_WRITE_LOCK(readerWriterLock, name) SCOPED_LOCK(readerWriterLock, ScopedWriteLock, name)
	#endif


	class ScopedReadLock
	{
	public:
		ScopedReadLock(ReaderWriterLock& lock) : m_lock(lock) { lock.EnterRead(); }
		~ScopedReadLock() { Leave(); }
		inline void Enter() { if (m_active) return; m_active = true; m_lock.EnterRead(); }
		inline void Leave() { if (!m_active) return; m_active = false; m_lock.LeaveRead(); }

		ReaderWriterLock& m_lock;
		bool m_active = true;
	};

	class ScopedWriteLock
	{
	public:
		ScopedWriteLock(ReaderWriterLock& lock) : m_lock(lock) { lock.EnterWrite(); }
		~ScopedWriteLock() { Leave(); }
		inline void Enter() { if (m_active) return; m_active = true; m_lock.EnterWrite(); }
		inline void Leave() { if (!m_active) return; m_active = false; m_lock.LeaveWrite(); }

		ReaderWriterLock& m_lock;
		bool m_active = true;
	};

	template<typename Lambda>
	struct ScopeGuard
	{
		void Cancel() { m_called = true; }
		auto Execute() { m_called = true; return m_lambda(); }

		ScopeGuard(Lambda lambda) : m_lambda(lambda) {}
		ScopeGuard(ScopeGuard&& o) { m_lambda = std::move(o.m_lambda); o.m_called = true; }
		ScopeGuard() = delete;
		ScopeGuard(const ScopeGuard& o) = delete;
		void operator=(const ScopeGuard&) = delete;
		void operator=(ScopeGuard&&) = delete;
		~ScopeGuard() { if (!m_called) m_lambda(); }
	private:
		Lambda m_lambda;
		bool m_called = false;
	};
	template<typename Lambda>
	ScopeGuard<Lambda> MakeGuard(Lambda&& lambda) { return ScopeGuard<Lambda>(std::move(lambda)); }
}
