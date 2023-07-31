// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Templates/TypeCompatibleBytes.h"
#include <atomic>
#include <type_traits>

namespace UE
{
	/**
	 * Multi-producer/single-consumer unbounded concurrent queue that is atomically consumed and is reset to its 
	 * default empty state.
	 * A typical use case is when the consumer doesn't stop until the queue is depleted.
	 * Is faster than traditional MPSC queues, especially for consumer.
	 */
	template<typename T, typename AllocatorType = FMemory>
	class TDepletableMpscQueue final
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
		UE_NONCOPYABLE(TDepletableMpscQueue);

		TDepletableMpscQueue() = default;

		~TDepletableMpscQueue()
		{
			// delete remaining elements
			FNode* Node = Sentinel.Next.load(std::memory_order_relaxed);
			while (Node != nullptr)
			{
				DestructItem(Node->Value.GetTypedPtr());
				FNode* Next = Node->Next.load(std::memory_order_relaxed);
				static_assert(std::is_trivially_destructible_v<FNode>);
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

			FNode* Prev = Tail.exchange(New, std::memory_order_release); // `release` to make sure the new node is fully constructed before
			// it becomes visible to the consumer
			check(Prev->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next

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
			FNode* First = Sentinel.Next.load(std::memory_order_relaxed);
			if (First == nullptr)
			{
				return; // empty
			}

			// reset the head so the next consumption can detect that the queue is empty
			// `Sentinel.Next` is not touched by producers right now because it's already not null
			Sentinel.Next.store(nullptr, std::memory_order_relaxed);

			// reset the queue to the empty state. this redirects producers to start from `Sentinel` again.
			// take note of the tail on resetting it because the list can be still not fully linked and so `Node.Next == nullptr` can't be 
			// used to detect the end of the list
			FNode* Last = Tail.exchange(&Sentinel, std::memory_order_acquire); // `acquire` to sync with producers' tail modifications,
			// `Sentinel.Next = nullptr` must happen before modifying `Tail` but it's important only for the single consumer so doesn't need extra sync
			check(Last->Next.load(std::memory_order_relaxed) == nullptr); // `Tail` is assigned before its Next
			// the previously queued items are detached from the instance (as a linked list, though potentially not fully linked yet)

			check(Last != &Sentinel); // can't be empty because of `First != nullptr` above

			Consume(First, Last, Consumer);
		}

		// the result can be relied upon only in special cases, as the state can change concurrently. use with caution 
		bool IsEmpty() const
		{
			return Tail.load(std::memory_order_relaxed) == &Sentinel;
		}

	private:
		template<typename F>
		static void Consume(FNode* First, FNode* Last, const F& Consumer)
		{
			auto GetNext = [](FNode* Node)
			{
				FNode* Next;
				// producers can be still updating `Next`, wait until the link to the next element is established
				do
				{
					Next = Node->Next.load(std::memory_order_relaxed);
				} while (Next == nullptr);

				return Next;
			};

			auto Consume = [&Consumer](FNode* Node)
			{
				T* ValuePtr = (T*)&Node->Value;
				Consumer(MoveTemp(*ValuePtr));
				DestructItem(ValuePtr);
			};

			while (First != Last)
			{
				Consume(First);
				FNode* Next = GetNext(First);
				static_assert(std::is_trivially_destructible_v<FNode>);
				AllocatorType::Free(First);
				First = Next;
			}

			Consume(Last);
		}
	};
}
