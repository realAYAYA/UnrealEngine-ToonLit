// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Math/RandomStream.h"
#include "Experimental/Containers/FAAArrayQueue.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Async/Fundamental/Task.h"

#include <atomic>

namespace LowLevelTasks
{
namespace LocalQueue_Impl
{
template<uint32 NumItems>
class TWorkStealingQueueBase2
{
	enum class ESlotState : uintptr_t
	{
		Free  = 0, //The slot is free and items can be put there
		Taken = 1, //The slot is in the proccess of beeing stolen 
	};

protected:
	//insert an item at the head position (this can only safe on a single thread, shared with Get) 
	inline bool Put(uintptr_t Item)
	{
		checkSlow(Item != uintptr_t(ESlotState::Free));
		checkSlow(Item != uintptr_t(ESlotState::Taken));

		uint32 Idx = (Head + 1) % NumItems;
		uintptr_t Slot = ItemSlots[Idx].Value.load(std::memory_order_acquire);

		if (Slot == uintptr_t(ESlotState::Free))
		{
			ItemSlots[Idx].Value.store(Item, std::memory_order_release);
			Head++;
			checkSlow(Head % NumItems == Idx);
			return true;		
		}
		return false;
	}

	//remove an item at the head position in FIFO order (this can only safe on a single thread, shared with Put) 
	inline bool Get(uintptr_t& Item)
	{
		uint32 Idx = Head % NumItems;
		uintptr_t Slot = ItemSlots[Idx].Value.load(std::memory_order_acquire);

		if (Slot > uintptr_t(ESlotState::Taken) && ItemSlots[Idx].Value.compare_exchange_strong(Slot, uintptr_t(ESlotState::Free), std::memory_order_acq_rel))
		{
			Head--;
			checkSlow((Head + 1) % NumItems == Idx);
			Item = Slot;
			return true;
		}
		return false;
	}

	//remove an item at the tail position in LIFO order (this can be done from any thread including the one that accesses the head)
	inline bool Steal(uintptr_t& Item)
	{
		do
		{
			uint32 IdxVer = Tail.load(std::memory_order_acquire);
			uint32 Idx = IdxVer % NumItems;
			uintptr_t Slot = ItemSlots[Idx].Value.load(std::memory_order_acquire);

			if (Slot == uintptr_t(ESlotState::Free))
			{
				return false;
			}
			else if (Slot != uintptr_t(ESlotState::Taken) && ItemSlots[Idx].Value.compare_exchange_weak(Slot, uintptr_t(ESlotState::Taken), std::memory_order_acq_rel))
			{
				if(IdxVer == Tail.load(std::memory_order_acquire))
				{
					uint32 Prev = Tail.fetch_add(1, std::memory_order_release); (void)Prev;
					checkSlow(Prev % NumItems == Idx);
					ItemSlots[Idx].Value.store(uintptr_t(ESlotState::Free), std::memory_order_release);
					Item = Slot;
					return true;
				}
				ItemSlots[Idx].Value.store(Slot, std::memory_order_release);
			}
		} while(true);
	}

private:
	struct FAlignedElement
	{
		alignas(PLATFORM_CACHE_LINE_SIZE * 2) std::atomic<uintptr_t> Value = {};
	};

	alignas(PLATFORM_CACHE_LINE_SIZE * 2) uint32 Head { ~0u };
	alignas(PLATFORM_CACHE_LINE_SIZE * 2) std::atomic_uint Tail { 0 };
	alignas(PLATFORM_CACHE_LINE_SIZE * 2) FAlignedElement ItemSlots[NumItems] = {};
};

template<typename Type, uint32 NumItems>
class TWorkStealingQueue2 final : protected TWorkStealingQueueBase2<NumItems>
{
	using PointerType = Type*;

public:
	inline bool Put(PointerType Item)
	{
		return TWorkStealingQueueBase2<NumItems>::Put(reinterpret_cast<uintptr_t>(Item));
	}

	inline bool Get(PointerType& Item)
	{
		return TWorkStealingQueueBase2<NumItems>::Get(reinterpret_cast<uintptr_t&>(Item));
	}

	inline bool Steal(PointerType& Item)
	{
		return TWorkStealingQueueBase2<NumItems>::Steal(reinterpret_cast<uintptr_t&>(Item));
	}
};
}

namespace Private {

enum class ELocalQueueType
{
	EBackground,
	EForeground,
};

/********************************************************************************************************************************************
 * A LocalQueueRegistry is a collection of LockFree queues that store pointers to Items, there are ThreadLocal LocalQueues with LocalItems. *
 * LocalQueues can only be Enqueued and Dequeued by the current Thread they were installed on. But Items can be stolen from any Thread      *
 * There is a global OverflowQueue than is used when a LocalQueue goes out of scope to dump all the remaining Items in                      *
 * or when a Thread has no LocalQueue installed or when the LocalQueue is at capacity. A new LocalQueue is registers itself always.         *
 * A Dequeue Operation can only be done starting from a LocalQueue, than the GlobalQueue will be checked.                                   *
 * Finally Items might get Stolen from other LocalQueues that are registered with the LocalQueueRegistry.                                   *
 ********************************************************************************************************************************************/
template<uint32 NumLocalItems = 1024>
class TLocalQueueRegistry
{
	static uint32 Rand()
	{
		uint32 State = FPlatformTime::Cycles();
		State = State * 747796405u + 2891336453u;
		State = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
		return (State >> 22u) ^ State;
	}

public:
	class TLocalQueue;

private:
	using FLocalQueueType	 = LocalQueue_Impl::TWorkStealingQueue2<FTask, NumLocalItems>;
	using FOverflowQueueType = FAAArrayQueue<FTask>;
	using DequeueHazard		 = typename FOverflowQueueType::DequeueHazard;

	enum class EOverflowType
	{
		// Tasks that can't be run inside busy waiting will be moved in that queue during busy wait loops.
		// This queue is always looked at first by non busy waiting workers to keep ordering as much as possible.
		DoNotRunInsideBusyWait,
		// All tasks including those that can't run inside busy wait are queued here by default to keep FIFO ordering as much as possible.
		Default,
		Count,
	};

public:
	class TLocalQueue
	{
		template<uint32>
		friend class TLocalQueueRegistry;

	public:
		TLocalQueue(TLocalQueueRegistry& InRegistry, ELocalQueueType InQueueType) : Registry(&InRegistry), QueueType(InQueueType)
		{
			AffinityIndex = Registry->AddLocalQueue(this, QueueType);
			for (int32 PriorityIndex = 0; PriorityIndex < int32(ETaskPriority::Count); ++PriorityIndex)
			{
				for (int32 OverflowIndex = 0; OverflowIndex < int32(EOverflowType::Count); ++OverflowIndex)
				{
					DequeueHazards[PriorityIndex][OverflowIndex] = Registry->OverflowQueues[PriorityIndex][OverflowIndex].getHeadHazard();
				}
			}
		}

		~TLocalQueue()
		{
			for (int32 PriorityIndex = 0; PriorityIndex < int32(ETaskPriority::Count); PriorityIndex++)
			{
				while (true)
				{
					FTask* Item;
					if (!LocalQueues[PriorityIndex].Get(Item))
					{
						break;
					}
					Registry->OverflowQueues[PriorityIndex][(int32)EOverflowType::Default].enqueue(Item);
				}
			}
			Registry->DeleteLocalQueue(this, QueueType);
		}

		// add an item to the local queue and overflow into the global queue if full
		// returns true if we should wake a worker
		inline void Enqueue(FTask* Item, uint32 PriorityIndex)
		{
			checkSlow(Registry);
			checkSlow(PriorityIndex < int32(ETaskPriority::Count));
			checkSlow(Item != nullptr);

			if (!LocalQueues[PriorityIndex].Put(Item))
			{
				Registry->OverflowQueues[PriorityIndex][(int32)EOverflowType::Default].enqueue(Item);
			}
		}

		// Check both the local and global queue in priority order
		template <bool bIsInsideBusyWait = false>
		inline FTask* Dequeue(bool GetBackGroundTasks)
		{
			const int32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count)   : int32(ETaskPriority::ForegroundCount);
			constexpr int32 MinOverflow = bIsInsideBusyWait  ? int32(EOverflowType::Default) : int32(EOverflowType::DoNotRunInsideBusyWait);

			for (int32 PriorityIndex = 0; PriorityIndex < MaxPriority; ++PriorityIndex)
			{
				FTask* Item;
				if (LocalQueues[PriorityIndex].Get(Item))
				{
					return Item;
				}

				for (int32 OverflowIndex = MinOverflow; OverflowIndex < int32(EOverflowType::Count); ++OverflowIndex)
				{
					Item = Registry->OverflowQueues[PriorityIndex][OverflowIndex].dequeue(DequeueHazards[PriorityIndex][OverflowIndex]);
					if (Item)
					{
						return Item;
					}
				}
			}
			return nullptr;
		}

		inline FTask* DequeueSteal(bool GetBackGroundTasks)
		{
			if (CachedRandomIndex == InvalidIndex)
			{
				CachedRandomIndex = Rand();
			}

			FTask* Result = Registry->StealItem(CachedRandomIndex, CachedPriorityIndex, GetBackGroundTasks);
			if (Result)
			{
				return Result;
			}
			return nullptr;
		}

		uint32 GetAffinityIndex() const
		{
			return AffinityIndex;
		}

	private:
		static constexpr uint32    InvalidIndex = ~0u;
		FLocalQueueType            LocalQueues[uint32(ETaskPriority::Count)];
		DequeueHazard              DequeueHazards[uint32(ETaskPriority::Count)][int32(EOverflowType::Count)];
		TLocalQueueRegistry*       Registry;
		uint32                     CachedRandomIndex = InvalidIndex;
		uint32                     CachedPriorityIndex = 0;
		uint32                     AffinityIndex = ~0u;
		ELocalQueueType            QueueType;
	};

	TLocalQueueRegistry() = default;

private:
	// add a queue to the Registry
	uint32 AddLocalQueue(TLocalQueue* QueueToAdd, ELocalQueueType QueueType)
	{
		NumActiveWorkers[QueueType == ELocalQueueType::EBackground]++;
		LocalQueues.Add(QueueToAdd);
		return LocalQueues.Num() - 1;
	}

	// remove a queue from the Registry
	void DeleteLocalQueue(TLocalQueue* QueueToRemove, ELocalQueueType QueueType)
	{
		NumActiveWorkers[QueueType == ELocalQueueType::EBackground]--;
		verify(LocalQueues.Remove(QueueToRemove) == 1);
	}

	// StealItem tries to steal an Item from a Registered LocalQueue
	FTask* StealItem(uint32& CachedRandomIndex, uint32& CachedPriorityIndex, bool GetBackGroundTasks)
	{
		uint32 NumQueues = LocalQueues.Num();
		uint32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
		CachedRandomIndex = CachedRandomIndex % NumQueues;

		for(uint32 i = 0; i < NumQueues; i++)
		{
			TLocalQueue* LocalQueue = LocalQueues[CachedRandomIndex];
			for(uint32 PriorityIndex = 0; PriorityIndex < MaxPriority; PriorityIndex++)
			{
				FTask* Item;
				if (LocalQueue->LocalQueues[CachedPriorityIndex].Steal(Item))
				{
					return Item;
				}
				CachedPriorityIndex = ++CachedPriorityIndex < MaxPriority ? CachedPriorityIndex : 0;
			}
			CachedRandomIndex = ++CachedRandomIndex < NumQueues ? CachedRandomIndex : 0;
		}
		CachedPriorityIndex = 0;
		CachedRandomIndex = TLocalQueue::InvalidIndex;
		return nullptr;
	}

public:
	// enqueue an Item directy into the Global OverflowQueue
	template <bool bAllowInsideBusyWaiting = true>
	void Enqueue(FTask* Item, uint32 PriorityIndex)
	{
		check(PriorityIndex < int32(ETaskPriority::Count));
		check(Item != nullptr);

		constexpr int32 OverflowIndex = bAllowInsideBusyWaiting ? (int32)EOverflowType::Default : (int32)EOverflowType::DoNotRunInsideBusyWait;
		OverflowQueues[PriorityIndex][OverflowIndex].enqueue(Item);
	}

	// grab an Item directy from the Global OverflowQueue
	template <bool bIsInsideBusyWait = false>
	FTask* DequeueGlobal(bool GetBackGroundTasks = true)
	{
		const int32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
		constexpr int32 MinOverflow = bIsInsideBusyWait ? int32(EOverflowType::Default) : int32(EOverflowType::DoNotRunInsideBusyWait);

		for (int32 PriorityIndex = 0; PriorityIndex < MaxPriority; ++PriorityIndex)
		{
			for (int32 OverflowIndex = MinOverflow; OverflowIndex < int32(EOverflowType::Count); ++OverflowIndex)
			{
				if (FTask* Item = OverflowQueues[PriorityIndex][OverflowIndex].dequeue())
				{
					return Item;
				}
			}
		}
		return nullptr;
	}

	inline FTask* DequeueSteal(bool GetBackGroundTasks)
	{
		uint32 CachedRandomIndex = Rand();
		uint32 CachedPriorityIndex = 0;
		FTask* Result = StealItem(CachedRandomIndex, CachedPriorityIndex, GetBackGroundTasks);
		if (Result)
		{
			return Result;
		}
		return nullptr;
	}

private:
	FOverflowQueueType     OverflowQueues[uint32(ETaskPriority::Count)][uint32(EOverflowType::Count)];
	TArray<TLocalQueue*>   LocalQueues;
	int                    NumActiveWorkers[2] = { 0, 0 };
};

} // namespace Private

enum class UE_DEPRECATED(5.4, "This was meant for internal use only and will be removed") ELocalQueueType
{
	EBackground,
	EForeground,
};


template<uint32 NumLocalItems = 1024>
class UE_DEPRECATED(5.4, "This was meant for internal use only and will be removed") TLocalQueueRegistry
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	static uint32 Rand()
	{
		uint32 State = FPlatformTime::Cycles();
		State = State * 747796405u + 2891336453u;
		State = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
		return (State >> 22u) ^ State;
	}

public:
	class TLocalQueue;

	// FOutOfWork is used to track the time while a worker is waiting for work
	// this happens after a worker was unable to aquire any task from the queues and until it finds work again or it goes into drowsing state.
	class FOutOfWork
	{
	private:
		template<uint32>
		friend class TLocalQueueRegistry;

		std::atomic_int& NumWorkersLookingForWork;
		bool ActivelyLookingForWork = false;

#if CPUPROFILERTRACE_ENABLED
		bool bCpuBeginEventEmitted = false;
#endif

		inline FOutOfWork(std::atomic_int& InNumWorkersLookingForWork) : NumWorkersLookingForWork(InNumWorkersLookingForWork)
		{
		}

	public:
		inline ~FOutOfWork()
		{
			Stop();
		}

		inline bool Start()
		{
			if (!ActivelyLookingForWork)
			{
#if CPUPROFILERTRACE_ENABLED
				if (CpuChannel)
				{
					static uint32 WorkerLookingForWorkTraceId = FCpuProfilerTrace::OutputEventType("TaskWorkerIsLookingForWork");
					FCpuProfilerTrace::OutputBeginEvent(WorkerLookingForWorkTraceId);
					bCpuBeginEventEmitted = true;
				}
#endif
				NumWorkersLookingForWork.fetch_add(1, std::memory_order_relaxed);
				ActivelyLookingForWork = true;
				return true;
			}
			return false;
		}

		inline bool Stop()
		{
			if (ActivelyLookingForWork)
			{
#if CPUPROFILERTRACE_ENABLED
				if (bCpuBeginEventEmitted)
				{
					FCpuProfilerTrace::OutputEndEvent();
					bCpuBeginEventEmitted = false;
				}
#endif
				NumWorkersLookingForWork.fetch_sub(1, std::memory_order_release);
				ActivelyLookingForWork = false;
				return true;
			}
			return false;
		}
	};

private:
	using FLocalQueueType	 = LocalQueue_Impl::TWorkStealingQueue2<FTask, NumLocalItems>;
	using FOverflowQueueType = FAAArrayQueue<FTask>;
	using DequeueHazard		 = typename FOverflowQueueType::DequeueHazard;

public:
	class TLocalQueue
	{
		template<uint32>
		friend class TLocalQueueRegistry;

	public:
		TLocalQueue(TLocalQueueRegistry& InRegistry, ELocalQueueType InQueueType, FSleepEvent* InSleepEvent) : Registry(&InRegistry), SleepEvent(InSleepEvent), QueueType(InQueueType)
		{
			AffinityIndex = Registry->AddLocalQueue(this, QueueType);
			for (int32 PriorityIndex = 0; PriorityIndex < int32(ETaskPriority::Count); PriorityIndex++)
			{
				DequeueHazards[PriorityIndex] = Registry->OverflowQueues[PriorityIndex].getHeadHazard();
			}
			AffinityHazard = AffinityQueue.getHeadHazard();
		}

		~TLocalQueue()
		{
			for (int32 PriorityIndex = 0; PriorityIndex < int32(ETaskPriority::Count); PriorityIndex++)
			{
				while (true)
				{
					FTask* Item;
					if (!LocalQueues[PriorityIndex].Get(Item))
					{
						break;
					}
					Registry->OverflowQueues[PriorityIndex].enqueue(Item);
				}
			}
			Registry->DeleteLocalQueue(this, QueueType);
		}

		//add an item to the local queue and overflow into the global queue if full
		// returns true if we should wake a worker
		inline bool Enqueue(FTask* Item, uint32 PriorityIndex)
		{
			checkSlow(Registry);
			checkSlow(PriorityIndex < int32(ETaskPriority::Count));
			checkSlow(Item != nullptr);

			bool bBackgroundTask = Item->IsBackgroundTask();
			if (!LocalQueues[PriorityIndex].Put(Item))
			{
				Registry->OverflowQueues[PriorityIndex].enqueue(Item);
			}
			return Registry->LessThanHalfWorkersLookingForWork(bBackgroundTask);
		}

		inline FTask* DequeueLocal(bool GetBackGroundTasks, bool bDisableThrottleStealing)
		{
			int32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
			for (int32 PriorityIndex = 0; PriorityIndex < MaxPriority; PriorityIndex++)
			{
				FTask* Item;
				if (LocalQueues[PriorityIndex].Get(Item))
				{
					return Item;
				}
			}
			return nullptr;
		}

		inline FTask* DequeueGlobal(bool GetBackGroundTasks, bool bDisableThrottleStealing)
		{
			if (bDisableThrottleStealing || (Registry->NumActiveWorkers[GetBackGroundTasks] >= (2 * Registry->NumWorkersLookingForWork[GetBackGroundTasks].load(std::memory_order_relaxed) - 1)) || ((Rand() % 4) == 0))
			{
				int32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
				for (int32 PriorityIndex = 0; PriorityIndex < MaxPriority; PriorityIndex++)
				{
					FTask* Item = Registry->OverflowQueues[PriorityIndex].dequeue(DequeueHazards[PriorityIndex]);
					if (Item)
					{
						return Item;
					}
				}
			}
			return nullptr;
		}

		inline FTask* DequeueSteal(bool GetBackGroundTasks, bool bDisableThrottleStealing)
		{
			if (bDisableThrottleStealing || (Registry->NumActiveWorkers[GetBackGroundTasks] >= (2 * Registry->NumWorkersLookingForWork[GetBackGroundTasks].load(std::memory_order_relaxed) - 1)) || ((Rand() % 4) == 0))
			{
				if (CachedRandomIndex == InvalidIndex)
				{
					CachedRandomIndex = Rand();
				}

				FTask* Result = Registry->StealItem(CachedRandomIndex, CachedPriorityIndex, GetBackGroundTasks);
				if (Result)
				{
					return Result;
				}
			}
			return nullptr;
		}

		inline FTask* DequeueAffinity(bool GetBackGroundTasks, bool bDisableThrottleStealing)
		{
			return AffinityQueue.dequeue(AffinityHazard);
		}

		uint32 GetAffinityIndex() const
		{
			return AffinityIndex;
		}

	private:
		inline void EnqueueAffinity(FTask* Item)
		{
			check(SleepEvent != nullptr);
			AffinityQueue.enqueue(Item);
			
			ESleepState SleepState = ESleepState::Sleeping;
			ESleepState DrowsingState = ESleepState::Drowsing;
			if (SleepEvent->SleepState.compare_exchange_strong(DrowsingState, ESleepState::Affinity, std::memory_order_acquire))
			{
			}
			else if (SleepEvent->SleepState.compare_exchange_strong(SleepState, ESleepState::Drowsing, std::memory_order_acquire))
			{
				SleepEvent->SleepEvent->Trigger();
			}
		}

	private:
		static constexpr uint32	InvalidIndex = ~0u;
		FLocalQueueType			LocalQueues[uint32(ETaskPriority::Count)];
		DequeueHazard			DequeueHazards[uint32(ETaskPriority::Count)];
		FOverflowQueueType		AffinityQueue;
		DequeueHazard			AffinityHazard;
		TLocalQueueRegistry*	Registry;
		FSleepEvent*			SleepEvent;
		uint32					CachedRandomIndex = InvalidIndex;
		uint32					CachedPriorityIndex = 0;
		uint32					AffinityIndex = ~0u;
		ELocalQueueType			QueueType;
	};

	TLocalQueueRegistry() = default;

private:
	// add a queue to the Registry
	uint32 AddLocalQueue(TLocalQueue* QueueToAdd, ELocalQueueType QueueType)
	{
		NumActiveWorkers[QueueType == ELocalQueueType::EBackground]++;
		LocalQueues.Add(QueueToAdd);
		return LocalQueues.Num() - 1;
	}

	// remove a queue from the Registry
	void DeleteLocalQueue(TLocalQueue* QueueToRemove, ELocalQueueType QueueType)
	{
		NumActiveWorkers[QueueType == ELocalQueueType::EBackground]--;
		verify(LocalQueues.Remove(QueueToRemove) == 1);
	}

	// StealItem tries to steal an Item from a Registered LocalQueue
	FTask* StealItem(uint32& CachedRandomIndex, uint32& CachedPriorityIndex, bool GetBackGroundTasks)
	{
		uint32 NumQueues = LocalQueues.Num();
		uint32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
		CachedRandomIndex = CachedRandomIndex % NumQueues;

		for(uint32 i = 0; i < NumQueues; i++)
		{
			TLocalQueue* LocalQueue = LocalQueues[CachedRandomIndex];
			for(uint32 PriorityIndex = 0; PriorityIndex < MaxPriority; PriorityIndex++)
			{	
				FTask* Item;
				if (LocalQueue->LocalQueues[CachedPriorityIndex].Steal(Item))
				{
					return Item;
				}
				CachedPriorityIndex = ++CachedPriorityIndex < MaxPriority ? CachedPriorityIndex : 0;
			}
			CachedRandomIndex = ++CachedRandomIndex < NumQueues ? CachedRandomIndex : 0;
		}
		CachedPriorityIndex = 0;
		CachedRandomIndex = TLocalQueue::InvalidIndex;
		return nullptr;
	}

public:
	// enqueue an Item directy into the Global OverflowQueue
	// returns true if we should wake a worker for stealing
	bool Enqueue(FTask* Item, uint32 PriorityIndex)
	{
		check(PriorityIndex < int32(ETaskPriority::Count));
		check(Item != nullptr);

		bool bBackgroundTask = Item->IsBackgroundTask();
		OverflowQueues[PriorityIndex].enqueue(Item);

		return LessThanHalfWorkersLookingForWork(bBackgroundTask);
	}

	inline bool EnqueueAffinity(FTask* Item, uint32 AffinityIndex)
	{
		if (AffinityIndex < uint32(LocalQueues.Num()) && LocalQueues[AffinityIndex]->SleepEvent)
		{
			LocalQueues[AffinityIndex]->EnqueueAffinity(Item);
			return true;
		}
		return false;
	}

	// grab an Item directy from the Global OverflowQueue
	FTask* DequeueGlobal(bool GetBackGroundTasks = true, bool bDisableThrottleStealing = true)
	{
		if (bDisableThrottleStealing || (NumActiveWorkers[GetBackGroundTasks] >= (2 * NumWorkersLookingForWork[GetBackGroundTasks].load(std::memory_order_relaxed) - 1)) || ((Rand() % 4) == 0))
		{
			int32 MaxPriority = GetBackGroundTasks ? int32(ETaskPriority::Count) : int32(ETaskPriority::ForegroundCount);
			for (int32 PriorityIndex = 0; PriorityIndex < MaxPriority; PriorityIndex++)
			{
				FTask* Item = OverflowQueues[PriorityIndex].dequeue();
				if (Item)
				{
					return Item;
				}
			}
		}
		return nullptr;
	}

	inline FTask* DequeueSteal(bool GetBackGroundTasks = true, bool bDisableThrottleStealing = true)
	{
		if (bDisableThrottleStealing || (NumActiveWorkers[GetBackGroundTasks] >= (2 * NumWorkersLookingForWork[GetBackGroundTasks].load(std::memory_order_relaxed) - 1)) || ((Rand() % 4) == 0))
		{
			uint32 CachedRandomIndex = Rand();
			uint32 CachedPriorityIndex = 0;
			FTask* Result = StealItem(CachedRandomIndex, CachedPriorityIndex, GetBackGroundTasks);
			if (Result)
			{
				return Result;
			}
		}
		return nullptr;
	}

	inline FOutOfWork GetOutOfWorkScope(ELocalQueueType QueueType)
	{
		return FOutOfWork(NumWorkersLookingForWork[QueueType == ELocalQueueType::EBackground]);
	}

private:
	inline bool LessThanHalfWorkersLookingForWork(bool bBackgroundTask) const
	{
		if (bBackgroundTask)
		{
			return NumWorkersLookingForWork[true].load(std::memory_order_acquire) * 2 <= NumActiveWorkers[true];
		}
		else
		{
			return (NumWorkersLookingForWork[false].load(std::memory_order_acquire) + NumWorkersLookingForWork[true].load(std::memory_order_acquire)) * 2 <= (NumActiveWorkers[false] + NumActiveWorkers[true]);
		}
	}

	FOverflowQueueType		OverflowQueues[uint32(ETaskPriority::Count)];
	TArray<TLocalQueue*>	LocalQueues;
	std::atomic_int			NumWorkersLookingForWork[2] = { {0}, {0} };
	int						NumActiveWorkers[2] = { 0, 0 };
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

}