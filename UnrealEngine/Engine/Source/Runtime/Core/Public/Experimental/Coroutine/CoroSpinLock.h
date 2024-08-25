// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Coroutine.h"

#if WITH_CPP_COROUTINES

namespace CoroSpinLock_Detail
{
	class FScope;
	class FAwaitable;
}

class FCoroSpinLock
{
	friend class CoroSpinLock_Detail::FScope;
	friend class CoroSpinLock_Detail::FAwaitable;
	std::atomic_uint Taken {0};

public:
	inline CoroSpinLock_Detail::FAwaitable Lock(bool WriteLock = true);
};


namespace CoroSpinLock_Detail
{
	class [[nodiscard]] FScope
	{
		FCoroSpinLock* CoroSpinLock = nullptr;
		bool WriteLock = true;

	public:
		FScope() = default;

		inline FScope(FCoroSpinLock* InCoroSpinLock, bool InWriteLock) : CoroSpinLock(InCoroSpinLock), WriteLock(InWriteLock)
		{
		}

		inline FScope(FScope&& Other)
		{
			WriteLock = Other.WriteLock;
			CoroSpinLock = Other.CoroSpinLock;
			Other.CoroSpinLock = nullptr;
		}

		inline void Release()
		{
			if(CoroSpinLock)
			{
				if (WriteLock)
				{
					coroCheck(CoroSpinLock->Taken.load(std::memory_order_relaxed) == 1);
					CoroSpinLock->Taken.store(0, std::memory_order_release);
				}
				else
				{
					uint32 Next;
					uint32 Previous = CoroSpinLock->Taken.load(std::memory_order_relaxed);
					do
					{
						coroCheck((Previous & 0x1) == 0);
						Next = Previous - 2;
					}
					while (!CoroSpinLock->Taken.compare_exchange_weak(Previous, Next, std::memory_order_release, std::memory_order_relaxed));
				}
				CoroSpinLock = nullptr;
			}
		}

		inline ~FScope()
		{
			Release();
		}

		inline FScope& operator= (FScope&& Other)
		{
			if(this != &Other)
			{
				Release();
				this->WriteLock = Other.WriteLock;
				this->CoroSpinLock = MoveTemp(Other.CoroSpinLock);
				Other.CoroSpinLock = nullptr;
			}
			return *this;
		}
	};

	class [[nodiscard]] FAwaitable
	{
		FCoroSpinLock* CoroSpinLock = nullptr;
		TLaunchedCoroTask<void> Task;
		bool WriteLock = true;

	private:
		inline bool TryTakeLock()
		{
			int Timeout = 8;
			uint32 Previous = 0;		
			if (!WriteLock)
			{
				Previous = CoroSpinLock->Taken.load(std::memory_order_relaxed);
			}

			do
			{
				uint32 Next;
				if (WriteLock)
				{
					Previous = 0;
					Next = 1;
				}
				else
				{
					Previous = Previous & ~0x1;
					Next = Previous + 2;
				}

				if (CoroSpinLock->Taken.compare_exchange_weak(Previous, Next, std::memory_order_acquire, std::memory_order_relaxed))
				{
					return true;
				}

				FPlatformProcess::YieldCycles(193);
				Timeout--;
			} while(Timeout >= 0);
			return false;
		}

		inline void LaunchTask()
		{
			Task = [](FAwaitable* Self) noexcept -> CORO_TASK(void)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CoroTask_ScopeLock);
				while (true)
				{
					if (Self->TryTakeLock())
					{
						co_return;
					}
					co_await suspend_always();
				}
			}(this).Launch(TEXT("FCoroSpinLock"), LowLevelTasks::ETaskPriority::Inherit, LowLevelTasks::EQueuePreference::GlobalQueuePreference);
		}

	public:
		inline FAwaitable(FCoroSpinLock* InCoroSpinLock, bool InWriteLock) : CoroSpinLock(InCoroSpinLock), WriteLock(InWriteLock)
		{
		}

		inline bool await_ready() noexcept
		{
			return TryTakeLock();
		}

		template<typename PromiseType>
		inline void await_suspend(coroutine_handle<PromiseType> Continuation) noexcept
		{
			LaunchTask();
			Continuation.promise().Suspend(Task.GetPromise());
		}

		inline FScope await_resume() noexcept
		{
			return FScope(CoroSpinLock, WriteLock);
		}
	};
}

inline CoroSpinLock_Detail::FAwaitable FCoroSpinLock::Lock(bool WriteLock)
{
	return CoroSpinLock_Detail::FAwaitable(this, WriteLock);
}

#else

class FCoroSpinLock
{
	FRWLock CS;

	class [[nodiscard]] FScope
	{
		FRWLock* LockObject = nullptr;
		bool WriteLock = true;

	public:
		FScope() = default;

		inline FScope(FRWLock* InLockObject, bool InWriteLock) : LockObject(InLockObject), WriteLock(InWriteLock)
		{
		}

		inline FScope(FScope&& Other)
		{
			WriteLock = Other.WriteLock;
			LockObject = Other.LockObject;
			Other.LockObject = nullptr;
		}

		inline void Release()
		{
			if(LockObject)
			{
				if (WriteLock)
				{
					LockObject->WriteUnlock();
				}
				else
				{
					LockObject->ReadUnlock();
				}
				LockObject = nullptr;
			}
		}

		inline ~FScope()
		{
			Release();
		}

		inline FScope& operator= (FScope&& Other)
		{
			if(this != &Other)
			{
				this->WriteLock = Other.WriteLock;
				this->LockObject = MoveTemp(Other.LockObject);
				Other.LockObject = nullptr;
			}
			return *this;
		}
	};

public:
	inline FScope Lock(bool WriteLock = true)
	{
		if (WriteLock)
		{
			CS.WriteLock();
		}
		else
		{
			CS.ReadLock();
		}

		return FScope(&CS, WriteLock);
	}
};

#endif