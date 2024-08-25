// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/TaskConcurrencyLimiter.h"
#include "Tasks/Task.h"
#include "Containers/LockFreeList.h"
#include "Templates/SharedPointer.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "CoreTypes.h"

#include <AtomicQueue.h>

namespace UE::Tasks::TaskConcurrencyLimiter_Private
{
	FPimpl::~FPimpl()
	{
		if (FEvent* Event = CompletionEvent.exchange(nullptr, std::memory_order_relaxed))
		{
			FPlatformProcess::ReturnSynchEventToPool(Event);
		}
	}

	void FPimpl::AddWorkItem(LowLevelTasks::FTask* Task)
	{
		NumWorkItems.fetch_add(1, std::memory_order_acquire);

		WorkQueue.Push(Task);

		uint32 ConcurrencySlot;
		if (ConcurrencySlots.Alloc(ConcurrencySlot))
		{
			ProcessQueueFromPush(ConcurrencySlot);
		}
	}

	bool FPimpl::Wait(FTimespan Timeout)
	{
		if (NumWorkItems.load(std::memory_order_relaxed) == 0)
		{
			return true;
		}

		FEvent* LocalCompletionEvent = CompletionEvent.load(std::memory_order_acquire);
		if (LocalCompletionEvent == nullptr)
		{
			FEvent* NewEvent = FPlatformProcess::GetSynchEventFromPool(true);
			if (!CompletionEvent.compare_exchange_strong(LocalCompletionEvent, NewEvent, std::memory_order_acq_rel, std::memory_order_acquire))
			{
				FPlatformProcess::ReturnSynchEventToPool(NewEvent);
			}
			else
			{
				LocalCompletionEvent = NewEvent;
			}
		}

		if (NumWorkItems.load(std::memory_order_acquire) == 0)
		{
			return true;
		}

		return LocalCompletionEvent->Wait(Timeout);
	}

	void FPimpl::ProcessQueue(uint32 ConcurrencySlot, bool bSkipFirstWakeUp)
	{
		bool bWakeUpWorker = !bSkipFirstWakeUp;
		do
		{
			if (LowLevelTasks::FTask* Task = WorkQueue.Pop())
			{
				// Now that we know the ConcurrencySlot, set it at launch time so
				// the executor can retrieve it.
				Task->SetUserData((void*)(UPTRINT)ConcurrencySlot);

				LowLevelTasks::TryLaunch(*Task, bWakeUpWorker ? LowLevelTasks::EQueuePreference::GlobalQueuePreference : LowLevelTasks::EQueuePreference::LocalQueuePreference, bWakeUpWorker);
			}
			else
			{
				ConcurrencySlots.Release(ConcurrencySlot);
				break;
			}

			// Don't skip wake-up if we launch any additional tasks.
			bWakeUpWorker = true;

		} while (ConcurrencySlots.Alloc(ConcurrencySlot));
	}

	void FPimpl::ProcessQueueFromWorker(uint32 ConcurrencySlot)
	{
		// Once we are in a worker thread, we want to schedule on the local queue without waking up additional workers
		// to allow our own worker to pick up the next item and avoid wake-up cost / context switch.
		static constexpr bool bSkipFirstWakeUp = true;

		ProcessQueue(ConcurrencySlot, bSkipFirstWakeUp);
	}

	void FPimpl::ProcessQueueFromPush(uint32 ConcurrencySlot)
	{
		// When we push new items, we don't want to skip any wake-up.
		static constexpr bool bSkipFirstWakeUp = false;

		ProcessQueue(ConcurrencySlot, bSkipFirstWakeUp);
	}

	void FPimpl::CompleteWorkItem(uint32 ConcurrencySlot)
	{
		if (NumWorkItems.fetch_sub(1, std::memory_order_release) == 1)
		{
			if (FEvent* LocalCompletionEvent = CompletionEvent.load(std::memory_order_acquire))
			{
				LocalCompletionEvent->Trigger();
			}
		}

		ProcessQueueFromWorker(ConcurrencySlot);
	}

} // namespace UE::Tasks::TaskConcurrencyLimiter_Private
