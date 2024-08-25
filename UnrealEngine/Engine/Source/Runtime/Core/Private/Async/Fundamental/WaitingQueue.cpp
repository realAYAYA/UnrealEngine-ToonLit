// Copyright (C) 2016 Dmitry Vyukov <dvyukov@google.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This implementation is based on EventCount.h
// included in the Eigen library but almost everything has been
// rewritten.

#include "Async/Fundamental/WaitingQueue.h"
#include "Async/TaskTrace.h"
#include "Logging/LogMacros.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "CoreGlobals.h"

// Activating the waiting queue tracing can help understand exactly what's going on
// from UnrealInsights or another external profiler.
// 
// Note that we're using empty WAITINGQUEUE_EVENT_SCOPE in almost every condition
// so we can follow along which code path are taken.
#define WITH_WAITINGQUEUE_TRACING 0
#define WITH_WAITINGQUEUE_CHECK   DO_CHECK
#define WITH_WAITINGQUEUE_DEBUG   0

#if WITH_WAITINGQUEUE_TRACING

namespace LowLevelTasks::Impl
{
	// This helps with visibility of events in UnrealInsights during debugging
	// of the waiting queue because any events below the 100 ns resolution
	// can often end up with 0 ns. This makes it very hard to see the order of
	// events since 0 sized events are unzoomable.
	struct NonEmptyEventScope
	{
		uint64 StartCycle{0};
		NonEmptyEventScope()
			: StartCycle(FPlatformTime::Cycles64())
		{
		}
		~NonEmptyEventScope()
		{
			while (StartCycle == FPlatformTime::Cycles64())
			{
			}
		}
	};
}

#define WAITINGQUEUE_EVENT_SCOPE(Name) SCOPED_NAMED_EVENT(Name, FColor::Turquoise) LowLevelTasks::Impl::NonEmptyEventScope __nonEmptyEventScope;
#else
#define WAITINGQUEUE_EVENT_SCOPE(Name)
#endif

#define WAITINGQUEUE_EVENT_SCOPE_ALWAYS(Name) SCOPED_NAMED_EVENT(Name, FColor::Turquoise)

#if WITH_WAITINGQUEUE_DEBUG
UE_DISABLE_OPTIMIZATION
#endif

namespace LowLevelTasks::Private
{

void FWaitingQueue::Init()
{
	check(NodesArray.Num() < (1 << WaiterBits) - 1);
	check(State == StackMask);
}

void FWaitingQueue::Shutdown()
{
	check((State & (StackMask | WaiterMask)) == StackMask);
}

void FWaitingQueue::PrepareWait(FWaitEvent* Node)
{
	WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_PrepareWait);

	State.fetch_add(WaiterInc, std::memory_order_relaxed);
}

void FWaitingQueue::CheckState(uint64_t InState, bool bInIsWaiter)
{
	static_assert(EpochBits >= 20, "Not enough bits to prevent ABA problem");
#if WITH_WAITINGQUEUE_CHECK
	const uint64_t Waiters = (InState & WaiterMask) >> WaiterShift;
	const uint64_t Signals = (InState & SignalMask) >> SignalShift;
	check(Waiters >= Signals);
	check(Waiters < (1 << WaiterBits) - 1);
	check(!bInIsWaiter || Waiters > 0);
	(void)Waiters;
	(void)Signals;
#endif
}

bool FWaitingQueue::CommitWait(FWaitEvent* Node, FOutOfWork& OutOfWork, int32 SpinCycles, int32 WaitCycles)
{
	{
		WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_CommitWait);

		check((Node->Epoch & ~EpochMask) == 0);
		Node->State.store(EWaitState::NotSignaled, std::memory_order_relaxed);

		const uint64_t Myself = (Node - &NodesArray[0]) | Node->Epoch;
		uint64_t LocalState = State.load(std::memory_order_relaxed);

		CheckState(LocalState, true);
		uint64_t NewState;
		if ((LocalState & SignalMask) != 0)
		{
			WAITINGQUEUE_EVENT_SCOPE(CommitWait_TryConsume);
			// Consume the signal and return immediately.
			NewState = LocalState - WaiterInc - SignalInc;
		}
		else
		{
			WAITINGQUEUE_EVENT_SCOPE(CommitWait_TryCommit);
			// Remove this thread from pre-wait counter and add to the waiter stack.
			NewState = ((LocalState & WaiterMask) - WaiterInc) | Myself;
			Node->Next.store(LocalState & (StackMask | EpochMask), std::memory_order_relaxed);
		}
		CheckState(NewState);
		if (State.compare_exchange_weak(LocalState, NewState, std::memory_order_acq_rel, std::memory_order_relaxed))
		{
			if ((LocalState & SignalMask) == 0)
			{
				WAITINGQUEUE_EVENT_SCOPE(CommitWait_Success);
				Node->Epoch += EpochInc;

				// Fallthrough to park but we want to get out of the CommitWait scope first so it doesn't stick
			}
			else
			{
				WAITINGQUEUE_EVENT_SCOPE(CommitWait_Aborted);
				OutOfWork.Stop();
				return true;
			}
		}
		else
		{
			WAITINGQUEUE_EVENT_SCOPE(CommitWait_Backoff);
			// Avoid too much contention on commit as it's not healthy. 
			// Prefer going back validating if anything has come up in the task queues
			// in between commit retries.
			return false;
		}
	}

	Park(Node, OutOfWork, SpinCycles, WaitCycles);
	return true;
}

bool FWaitingQueue::CancelWait(FWaitEvent* Node)
{
	WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_CancelWait);

	uint64_t LocalState = State.load(std::memory_order_relaxed);
	for (;;)
	{
		bool bConsumedSignal = false;
		CheckState(LocalState, true);
		uint64_t NewState = LocalState - WaiterInc;

		// When we consume a signal, the caller will have to try to wake up an additional
		// worker otherwise we could end up missing a wakeup and end up into a deadlock.
		// The more signal we consume, the more spurious wakeups we're going to have so
		// only consume a signal when both waiters and signals are equal so we get the
		// minimal amount of consumed signals possible.
		if (((LocalState & WaiterMask) >> WaiterShift) == ((LocalState & SignalMask) >> SignalShift))
		{
			WAITINGQUEUE_EVENT_SCOPE(Try_ConsumeSignal);
			NewState -= SignalInc;
			bConsumedSignal = true;
		}
		else
		{
			WAITINGQUEUE_EVENT_SCOPE(Try_NoConsumeSignal);
			bConsumedSignal = false;
		}

		CheckState(NewState);
		if (State.compare_exchange_weak(LocalState, NewState, std::memory_order_acq_rel, std::memory_order_relaxed))
		{
			if (bConsumedSignal)
			{
				WAITINGQUEUE_EVENT_SCOPE(Success_SignalConsumed);
				// Since we consumed the event, but we don't know if we're cancelling because of the task
				// this other thread is waking us for or another task entirely. Tell the caller to wake another thread.
				return true;
			}
			else
			{
				WAITINGQUEUE_EVENT_SCOPE(Success_NoSignalConsumed);
			}
			return false;
		}
	}
}

int32 FWaitingQueue::NotifyInternal(int32 Count)
{
	WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Notify);

	int32 Notifications = 0;
	while (Count > Notifications)
	{
		uint64_t LocalState = State.load(std::memory_order_relaxed);
		for (;;)
		{
			CheckState(LocalState);
			const uint64_t Waiters = (LocalState & WaiterMask) >> WaiterShift;
			const uint64_t Signals = (LocalState & SignalMask) >> SignalShift;
			const bool bNotifyAll = Count >= NodesArray.Num();
			// Easy case: no waiters.
			if ((LocalState & StackMask) == StackMask && Waiters == Signals)
			{
				WAITINGQUEUE_EVENT_SCOPE(NoMoreWaiter1);
				return Notifications;
			}
			uint64_t NewState;
			if (bNotifyAll)
			{
				WAITINGQUEUE_EVENT_SCOPE(TryUnblockAll);
				// Empty wait stack and set signal to number of pre-wait threads.
				NewState = (LocalState & WaiterMask) | (Waiters << SignalShift) | StackMask;
			}
			else if (Signals < Waiters)
			{
				WAITINGQUEUE_EVENT_SCOPE(TryAbortOnePreWait);
				// There is a thread in pre-wait state, unblock it.
				NewState = LocalState + SignalInc;
			}
			else
			{
				WAITINGQUEUE_EVENT_SCOPE(TryUnparkOne);
				// Pop a waiter from list and unpark it.
				FWaitEvent* Node = &NodesArray[LocalState & StackMask];
				uint64_t Next = Node->Next.load(std::memory_order_relaxed);
				NewState = (LocalState & (WaiterMask | SignalMask)) | Next;
			}
			CheckState(NewState);
			if (State.compare_exchange_weak(LocalState, NewState, std::memory_order_acq_rel, std::memory_order_relaxed))
			{
				if (!bNotifyAll && (Signals < Waiters))
				{
					WAITINGQUEUE_EVENT_SCOPE(UnblockedPreWaitThread);
					Notifications++;
					break;  // unblocked pre-wait thread
				}

				if ((LocalState & StackMask) == StackMask)
				{
					WAITINGQUEUE_EVENT_SCOPE(NoMoreWaiter2);
					return Notifications;
				}

				FWaitEvent* Node = &NodesArray[LocalState & StackMask];
				if (!bNotifyAll)
				{
					WAITINGQUEUE_EVENT_SCOPE(UnparkOne);
					Node->Next.store(StackMask, std::memory_order_relaxed);
					Notifications += Unpark(Node);
					break;
				}
				else
				{
					WAITINGQUEUE_EVENT_SCOPE(UnparkAll);
					Notifications += (int32)Waiters;
					return Unpark(Node) + Notifications;
				}
			}
		}
	}

	return Notifications;
}

void FWaitingQueue::Park(FWaitEvent* Node, FOutOfWork& OutOfWork, int32 SpinCycles, int32 WaitCycles)
{
	{
		ON_SCOPE_EXIT { OutOfWork.Stop(); };
		WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Park);

		{
			// Spinning for a very short while helps reduce signaling cost
			// since we're giving the other threads a final chance to wake us with an 
			// atomic only instead of a more costly kernel call.
			WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Park_Spin);
			for (int Spin = 0; Spin < SpinCycles; ++Spin)
			{
				if (Node->State.load(std::memory_order_relaxed) == EWaitState::NotSignaled)
				{
					FPlatformProcess::YieldCycles(WaitCycles);
				}
				else
				{
					WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Park_Abort);
					return;
				}
			}
		}

		Node->Event->Reset();
		EWaitState Target = EWaitState::NotSignaled;
		if (Node->State.compare_exchange_strong(Target, EWaitState::Waiting, std::memory_order_relaxed, std::memory_order_relaxed))
		{
			WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Park_Wait);
			// Fall through to the wait function so we close all inner scope before waiting.
		}
		else
		{
			WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Park_Abort);
			return;
		}
	}

	// Flush any open scope before going to sleep so that anything that happened
	// before appears in UnrealInsights right away. If we don't do this,
	// the thread buffer will be held to this thread until we wake up and fill it
	// so it might cause events to appear as missing in UnrealInsights, especially
	// in case we never wake up again (i.e. deadlock / crash).
	#ifdef TRACE_CPUPROFILER_EVENT_FLUSH
	TRACE_CPUPROFILER_EVENT_FLUSH();
	#endif
	Node->Event->Wait();
}

int32 FWaitingQueue::Unpark(FWaitEvent* Node)
{
	WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Unpark);

	int32 UnparkedCount = 0;
	for (FWaitEvent* Next; Node; Node = Next)
	{
		uint64_t NextNode = Node->Next.load(std::memory_order_relaxed) & StackMask;
		Next = NextNode == StackMask ? nullptr : &NodesArray[(int)NextNode];

		UnparkedCount++;

		// Signaling can be very costly on some platforms. So only trigger
		// the event if the other thread was in the waiting state.
		if (Node->State.exchange(EWaitState::Signaled, std::memory_order_relaxed) == EWaitState::Waiting)
		{
			// This one  we actually care about since signaling cost is very expensive.
			WAITINGQUEUE_EVENT_SCOPE_ALWAYS(FWaitingQueue_Unpark_SignalWaitingThread);
			Node->Event->Trigger();
		}
		else
		{
			WAITINGQUEUE_EVENT_SCOPE(FWaitingQueue_Unpark_SignaledSpinningThread);
		}
	}

	return UnparkedCount;
}

} // namespace LowLevelTasks::Private

#if WITH_WAITINGQUEUE_DEBUG
UE_ENABLE_OPTIMIZATION
#endif