// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"
#include <atomic>
#include <type_traits>

namespace UE
{
	/**
	 * WARNING: this queue can cause priority inversion or a livelock due to spinlocking in	`Deplete()` method, though we haven't seen this
	 * happened in practice. Prefer `TConsumeAllMpmcQueue` which is equally fast in LIFO mode, and only slightly slower in FIFO mode.
	 *
	 * Multi-producer/multi-consumer unbounded concurrent queue that is atomically consumed and is reset to its
	 * default empty state.
	 * A typical use case is when the consumer doesn't stop until the queue is depleted.
	 * Is faster than traditional MPSC and MPMC queues, especially for consumer.
	 * Consumes elements in FIFO order.
	 */
	template<typename T, typename AllocatorType = FMemory>
	class UE_DEPRECATED(5.3, "This concurrent queue was deprecated because it uses spin-waiting that can cause priority inversion and subsequently deadlocks on some platforms. Please use TConsumeAllMpmcQueue.") TDepletableMpmcQueue final
	{
	private:
		struct FNode
		{
			std::atomic<FNode*> Next{ nullptr };
			TTypeCompatibleBytes<T> Value;
		};

		FNode Sentinel; // `Sentinel.Next` is the head of the queue
		std::atomic<FNode*> Tail{ &Sentinel };

	public:
		UE_NONCOPYABLE(TDepletableMpmcQueue);

		TDepletableMpmcQueue() = default;

		~TDepletableMpmcQueue()
		{
			static_assert(std::is_trivially_destructible_v<FNode>);

			// delete remaining elements
			FNode* Node = Sentinel.Next.load(std::memory_order_relaxed);
			while (Node != nullptr)
			{
				DestructItem(Node->Value.GetTypedPtr());
				FNode* Next = Node->Next.load(std::memory_order_relaxed);
				AllocatorType::Free(Node);
				Node = Next;
			}
		}

		template <typename... ArgTypes>
		void Enqueue(ArgTypes&&... Args)
		{
			EnqueueAndReturnWasEmpty(Forward<ArgTypes>(Args)...);
		}

		template <typename... ArgTypes>
		bool EnqueueAndReturnWasEmpty(ArgTypes&&... Args)
		{
			FNode* New = new(AllocatorType::Malloc(sizeof(FNode), alignof(FNode))) FNode;
			new (&New->Value) T(Forward<ArgTypes>(Args)...);

			// switch `Tail` to the new node and only then link the old tail to the new one. The list is not fully linked between these ops,
			// this is explicitly handled by the consumer by waiting for the link

			FNode* Prev = Tail.exchange(New, std::memory_order_acq_rel); // "acquire" to sync with `Prev->Next` initialisation from a concurrent calls,
			// `release` to make sure the new node is fully constructed before it becomes visible to the consumer or a concurrent enqueueing
			
			// the following `check` is commented out because it can be reordered after the following `Prev->Next.store` which unlocks 
			// the consumer that will free `Prev`. left commented as a documentation
			// check(Prev->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next

			Prev->Next.store(New, std::memory_order_relaxed);

			return Prev == &Sentinel;
		}

		/**
		 * Takes all items from the queue atomically and then consumes them.
		 * @param Consumer: a functor with signature `AnyReturnType (T Value)`
		 */
		template<typename F>
		void Deplete(const F& Consumer)
		{
			// reset the head so the next consumption can detect that the queue is empty
			FNode* First = Sentinel.Next.exchange(nullptr, std::memory_order_relaxed);
			if (First == nullptr)
			{
				return; // empty
			}

			// reset the queue to the empty state. this redirects producers to start from `Sentinel` again.
			// take note of the tail on resetting it because the list can be still not fully linked and so `Node.Next == nullptr` can't be 
			// used to detect the end of the list
			FNode* Last = Tail.exchange(&Sentinel, std::memory_order_acq_rel); // `acquire` to sync with producers' tail modifications, and
			// "release" to force `Sentinel.Next = nullptr` happening before modifying `Tail
			check(Last->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next
			// the previously queued items are detached from the instance (as a linked list, though potentially not fully linked yet)

			check(Last != &Sentinel); // can't be empty because of `First != nullptr` above
			Deplete_Internal(First, Last, Consumer);
		}

		// the result can be relied upon only in special cases (e.g. debug checks), as the state can change concurrently. use with caution 
		bool IsEmpty() const
		{
			return Tail.load(std::memory_order_relaxed) == &Sentinel;
		}

	private:
		template<typename F>
		static void Deplete_Internal(FNode* First, FNode* Last, const F& Consumer)
		{
			auto GetNext = [](FNode* Node)
			{
				FNode* Next = nullptr;
				// producers can be still updating `Next`, wait until the link to the next element is established
				while (Next == nullptr) // <- This loop has the potential for live locking if enqueue was not completed (e.g was running at lower priority)
				{
					Next = Node->Next.load(std::memory_order_relaxed);
				};
				return Next;
			};

			auto Consume = [&Consumer](FNode* Node)
			{
				T* ValuePtr = (T*)&Node->Value;
				Consumer(MoveTemp(*ValuePtr));
				DestructItem(ValuePtr);
				AllocatorType::Free(Node);
			};

			while (First != Last)
			{
				FNode* Next = GetNext(First);
				Consume(First);
				First = Next;
			}

			Consume(Last);
		}
	};

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template<typename T, typename AllocatorType = FMemory>
	using TDepletableMpscQueue UE_DEPRECATED(5.2, "This concurrent queue was deprecated because it uses spin-waiting that can cause priority inversion and subsequently deadlocks on some platforms. Please use TConsumeAllMpmcQueue.") = TDepletableMpmcQueue<T, AllocatorType>;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
