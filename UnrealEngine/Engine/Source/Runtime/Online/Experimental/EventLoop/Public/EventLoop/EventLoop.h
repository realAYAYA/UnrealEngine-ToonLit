// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Algo/ForEach.h"
#include "Async/Mutex.h"
#include "EventLoop/IEventLoop.h"
#include "EventLoop/EventLoopTimer.h"
#include "EventLoop/IEventLoopIOManager.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopeExit.h"
#include "Stats/Stats.h"
#include <limits>

namespace UE::EventLoop {

struct FEventLoopTraitsBase
{
	static FORCEINLINE uint32 GetCurrentThreadId()
	{
		return FPlatformTLS::GetCurrentThreadId();
	}

	static FORCEINLINE FTimespan GetCurrentTime()
	{
		return FTimespan(FPlatformTime::Seconds() * ETimespan::TicksPerSecond);
	}

	static FORCEINLINE bool IsEventLoopThread(uint32 EventLoopThreadId)
	{
		return EventLoopThreadId == GetCurrentThreadId();
	}

	static FORCEINLINE bool IsInitialized(uint32 EventLoopThreadId)
	{
		return (EventLoopThreadId != 0);
	}

	static FORCEINLINE void CheckInitialized(uint32 EventLoopThreadId)
	{
		check(IsInitialized(EventLoopThreadId));
	}

	static FORCEINLINE void CheckNotInitialized(uint32 EventLoopThreadId)
	{
		check(!IsInitialized(EventLoopThreadId));
	}

	static FORCEINLINE void CheckIsEventLoopThread(uint32 EventLoopThreadId)
	{
		check(IsEventLoopThread(EventLoopThreadId));
	}

	static FORCEINLINE bool IsShutdownRequested(bool bShutdownRequested)
	{
		return bShutdownRequested;
	}
};

struct FEventLoopDefaultTraits : public FEventLoopTraitsBase
{
	using FTimerManagerTraits = FTimerManagerDefaultTraits;
};

template <typename IOManagerType, typename Traits = FEventLoopDefaultTraits>
class TEventLoop final : public IEventLoop
{
private:
	IOManagerType IOManager;
	TTimerManager<typename Traits::FTimerManagerTraits> TimerManager;
	TQueue<FAsyncTask, EQueueMode::Mpsc> AsyncTasks;
	TArray<FOnShutdownComplete> ShutdownRequests;
	FTimespan LoopTime;
	TAtomic<uint32> EventLoopThreadId;
	TAtomic<bool> bShutdownRequested;
	TAtomic<bool> bShutdownCompleted;
	TAtomic<bool> bNotifyOnInit;
	UE::FMutex InitMutex;
	UE::FMutex ShutdownMutex;

public:
	struct FParams
	{
		// Initial IO manager parameters.
		typename IOManagerType::FParams IOManagerParams;
	};

	TEventLoop(FParams&& Params = FParams())
		: IOManager(*this, MoveTemp(Params.IOManagerParams))
		, TimerManager()
		, AsyncTasks()
		, LoopTime()
		, EventLoopThreadId(0)
		, bShutdownRequested(false)
		, bShutdownCompleted(false)
		, bNotifyOnInit(false)
	{
		// Make sure request manager type derives from IIOManager.
		static_assert(std::is_base_of_v<IIOManager, IOManagerType> == true);
	}

	virtual ~TEventLoop()
	{
	}

	typename IOManagerType::FIOAccess& GetIOAccess()
	{
		return IOManager.GetIOAccess();
	}

	virtual bool Init() override
	{
		bool bInitSuccessful = false;

		{
			InitMutex.Lock();
			ON_SCOPE_EXIT
			{
				InitMutex.Unlock();
			};

			Traits::CheckNotInitialized(EventLoopThreadId);
			LoopTime = Traits::GetCurrentTime();
			TimerManager.Init();
			bInitSuccessful = IOManager.Init();
			EventLoopThreadId = Traits::GetCurrentThreadId();
		}

		if (bNotifyOnInit)
		{
			IOManager.Notify();
		}

		return bInitSuccessful;
	}

	virtual void RequestShutdown(FOnShutdownComplete&& OnShutdownComplete = FOnShutdownComplete()) override
	{
		bool bRunLocally = false;

		{
			ShutdownMutex.Lock();
			ON_SCOPE_EXIT
			{
				ShutdownMutex.Unlock();
			};

			// Check if this call to RequestShutdown is the first call.
			bool bExpectedValue = false;
			if (bShutdownRequested.CompareExchange(bExpectedValue, true))
			{
				// This is the first call to RequestShutdown. Notify the request manager to wake
				// it so that the loop will see the request.
				ShutdownRequests.Add(MoveTemp(OnShutdownComplete));
				TryNotify();
			}
			else if (!bShutdownCompleted)
			{
				// Shutdown has been requested, but not yet acknowledged.
				ShutdownRequests.Add(MoveTemp(OnShutdownComplete));
			}
			else
			{
				// Loop shutdown has already completed. Run the callback locally outside of the lock.
				bRunLocally = true;
			}
		}

		// Loop shutdown has already completed.
		if (bRunLocally && OnShutdownComplete)
		{
			OnShutdownComplete();
		}
	}

	virtual FTimerHandle SetTimer(FTimerCallback&& Callback, FTimespan InRate, bool InbRepeat = false, TOptional<FTimespan> InFirstDelay = TOptional<FTimespan>()) override
	{
		FTimerHandle OutHandle = TimerManager.SetTimer(MoveTemp(Callback), InRate, InbRepeat, InFirstDelay);
		TryNotify();
		return OutHandle;
	}

	virtual void ClearTimer(FTimerHandle& InHandle, FOnTimerCleared&& OnTimerCleared = FOnTimerCleared()) override
	{
		TimerManager.ClearTimer(InHandle, MoveTemp(OnTimerCleared));
		TryNotify();
	}

	virtual void PostAsyncTask(FAsyncTask&& Task) override
	{
		AsyncTasks.Enqueue(MoveTemp(Task));
		TryNotify();
	}

	virtual void Run() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_EventLoop_TEventLoop_Run);
		Traits::CheckInitialized(EventLoopThreadId);

		while (true)
		{
			const FTimespan InfiniteWait(std::numeric_limits<int64>::max());
			FTimespan WaitTime = InfiniteWait;

			if (!RunOnce(WaitTime))
			{
				break;
			}
		};
	}

	virtual bool RunOnce(FTimespan WaitTime) override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_EventLoop_TEventLoop_RunOnce);
		Traits::CheckInitialized(EventLoopThreadId);

		if (bShutdownCompleted)
		{
			// Terminated.
			return false;
		}

		// Check if there are any running timers. The maximum wait time will then be shortened
		// to the expiration of the next timer.
		FTimespan TimerExpirationTime;
		if (TimerManager.GetNextTimeout(TimerExpirationTime))
		{
			WaitTime = FMath::Min(WaitTime, TimerExpirationTime);
		}

		IOManager.Poll(WaitTime);

		// Run timers.
		FTimespan CurrentTime = Traits::GetCurrentTime();
		FTimespan ElapsedTime = FMath::Max(CurrentTime - LoopTime, 0.0);
		LoopTime = CurrentTime;
		TimerManager.Tick(ElapsedTime);

		// If any timers ran due to the above call to Tick, check whether any repeat timers are
		// waiting to be rescheduled. Advancing the loop time and ticking the timers again will
		// allow those timers to reschedule without requiring polling the IO manager again.
		if (TimerManager.HasPendingRepeatTimer())
		{
			CurrentTime = Traits::GetCurrentTime();
			ElapsedTime = FMath::Max(CurrentTime - LoopTime, 0.0);
			LoopTime = CurrentTime;
			TimerManager.Tick(ElapsedTime);
		}

		RunAsyncTasks();

		if (Traits::IsShutdownRequested(bShutdownRequested))
		{
			IOManager.Shutdown();

			TArray<FOnShutdownComplete> LocalShutdownRequests;
			{
				ShutdownMutex.Lock();
				ON_SCOPE_EXIT
				{
					ShutdownMutex.Unlock();
				};

				bShutdownCompleted = true;
				LocalShutdownRequests = MoveTemp(ShutdownRequests);
			}

			// Run post-shutdown tasks.
			Algo::ForEach(LocalShutdownRequests, [](FOnShutdownComplete& OnShutdownComplete){
				if (OnShutdownComplete)
				{
					OnShutdownComplete();
				}
			});

			// Terminated.
			return false;
		}

		// Continue running.
		return true;
	}

	virtual FTimespan GetLoopTime() const override
	{
		Traits::CheckInitialized(EventLoopThreadId);
		return LoopTime;
	}

private:
	void RunAsyncTasks()
	{
		while (FAsyncTask* AsyncTask = AsyncTasks.Peek())
		{
			(*AsyncTask)();
			AsyncTasks.Pop();
		}
	}

	void TryNotify()
	{
		if (Traits::IsInitialized(EventLoopThreadId))
		{
			// Init already completed. Notify the IO manager.
			IOManager.Notify();
		}
		else
		{
			InitMutex.Lock();
			ON_SCOPE_EXIT
			{
				InitMutex.Unlock();
			};

			if (!Traits::IsInitialized(EventLoopThreadId))
			{
				// Init has not yet started. Let it know to notify the IO manager of
				// pending actions once it has completed.
				bNotifyOnInit = true;
			}
			else
			{
				// Init competed before the lock was acquired. Notify the IO manager.
				IOManager.Notify();
			}
		}
	}
};

/* UE::EventLoop */ }
