// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/ExecutionContextProxy.h"
#include "DecoratorBase/IDecoratorInterface.h"

struct FAnimNextGraphInstancePtr;
class FMemStack;

namespace UE::AnimNext
{
	struct FUpdateEntry;
	struct FUpdateTraversalQueue;

	/**
	  * FDecoratorUpdateState
	  * 
	  * State that propagates down the graph during the update traversal.
	  * 
	  * We align this to 16 bytes and maintain that same size to ensure efficient copying.
	  */
	struct alignas(16) FDecoratorUpdateState final
	{
		FDecoratorUpdateState() = default;

		explicit FDecoratorUpdateState(float InDeltaTime)
			: DeltaTime(InDeltaTime)
		{}

		// Returns the delta time for this decorator
		float GetDeltaTime() const { return DeltaTime; }

		// Returns the total weight for this decorator
		float GetTotalWeight() const { return TotalWeight; }

		// Returns the total trajectory weight for this decorator
		float GetTotalTrajectoryWeight() const { return TotalTrajectoryWeight; }

		// Returns whether or not this decorator is blending out
		bool IsBlendingOut() const { return !!bIsBlendingOut; }

		// Creates a new instance of the update state with the total weight scaled by the supplied weight
		FDecoratorUpdateState WithWeight(float Weight) const
		{
			FDecoratorUpdateState Result(*this);
			Result.TotalWeight *= Weight;
			return Result;
		}

		// Creates a new instance of the update state with the total trajectory weight scaled by the supplied weight
		FDecoratorUpdateState WithTrajectoryWeight(float Weight) const
		{
			FDecoratorUpdateState Result(*this);
			Result.TotalTrajectoryWeight *= Weight;
			return Result;
		}

		// Creates a new instance of the update state with the delta time scaled by the supplied scale factor
		FDecoratorUpdateState WithTimeScale(float TimeScale) const
		{
			FDecoratorUpdateState Result(*this);
			Result.DeltaTime *= TimeScale;
			return Result;
		}

		// Creates a new instance of the update state marking it as blending out
		FDecoratorUpdateState AsBlendingOut() const
		{
			FDecoratorUpdateState Result(*this);
			Result.bIsBlendingOut = true;
			return Result;
		}

	private:
		// The amount of time to move forward by at allowing parents to time scale their children
		float DeltaTime = 0.0f;

		// The current total weight factoring in all inherited blend weights
		float TotalWeight = 1.0f;

		// The current total trajectory weight factoring in all inherited blend weights
		float TotalTrajectoryWeight = 1.0f;

		// Whether or not we are blending out
		int32 bIsBlendingOut = false;
	};

	/**
	 * FUpdateTraversalContext
	 *
	 * Contains all relevant transient data for an update traversal and wraps the execution context.
	 */
	struct FUpdateTraversalContext final : FExecutionContextProxy
	{
	private:
		// Constructs a new traversal context and wraps the specified execution context
		explicit FUpdateTraversalContext(const FExecutionContext& InExecutionContext, FMemStack& InMemStack);

		// Pops entries from the traversal queue and pushes them onto the update stack
		void PushQueuedUpdateEntries(FUpdateTraversalQueue& TraversalQueue);

		// Pushes an entry onto the update stack
		void PushUpdateEntry(FUpdateEntry* Entry);

		// Pops an entry from the top of the update stack
		FUpdateEntry* PopUpdateEntry();

		// Pushes an entry onto the free entry stack
		void PushFreeEntry(FUpdateEntry* Entry);

		// Returns a new entry suitable for update queuing
		// If an entry isn't found in the free stack, a new one is allocated from the memstack
		FUpdateEntry* GetNewEntry(const FWeakDecoratorPtr& DecoratorPtr, const FDecoratorUpdateState& DecoratorState);

		// The memstack of the local thread
		FMemStack& MemStack;

		// The head pointer of the update stack
		// This is the traversal execution stack and it contains entries that are
		// pending their pre-update call and entries waiting for post-update to be called
		FUpdateEntry* UpdateStackHead = nullptr;

		// The head pointer of the free entry stack
		// Entries are allocated from the memstack and are re-used in LIFO since they'll
		// be warmer in the CPU cache
		FUpdateEntry* FreeEntryStackHead = nullptr;

		friend ANIMNEXT_API void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime);
		friend FUpdateTraversalQueue;
	};

	/**
	 * FUpdateTraversalQueue
	 *
	 * A queue of children to traverse.
	 * @see IUpdate::QueueChildrenForTraversal
	 */
	struct FUpdateTraversalQueue final
	{
		// Queued a child for traversal
		// Children are processed in the same order they are queued
		ANIMNEXT_API void Push(const FWeakDecoratorPtr& ChildPtr, const FDecoratorUpdateState& ChildDecoratorState);

	private:
		explicit FUpdateTraversalQueue(FUpdateTraversalContext& InTraversalContext);

		// The current traversal context
		FUpdateTraversalContext& TraversalContext;

		// The head pointer of the queued update stack
		// When a child is queued for traversal, it is first appended to this stack
		// Once all children have been queued and pre-update terminates, this stack is
		// emptied and pushed onto the update stack
		FUpdateEntry* QueuedUpdateStackHead = nullptr;

		friend ANIMNEXT_API void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime);
		friend FUpdateTraversalContext;
	};

	/**
	 * IUpdate
	 *
	 * This interface is called during the update traversal.
	 *
	 * When a node is visited, PreUpdate is first called on its top decorator. It is responsible for forwarding
	 * the call to the next decorator that implements this interface on the decorator stack of the node. Once
	 * all decorators have had the chance to PreUpdate, the children will then evaluate and PostUpdate will
	 * then be called afterwards on the original decorator.
	 * 
	 * If this interface is implemented on a node, it is responsible for queuing the children for traversal
	 * using the QueueChildrenForTraversal function. If the interface isn't implemented, children
	 * are discovered through the IHierarchy interface and traversal propagates to them using the current
	 * decorator state.
	 */
	struct ANIMNEXT_API IUpdate : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IUpdate, 0x59d24dc5)

		// Called before a decorator's children are updated
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const;

		// Called after PreUpdate to request that children be queued with the provided context
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const;

		// Called after a decorator's children have been updated
		virtual void PostUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IUpdate> : FDecoratorBinding
	{
		// @see IUpdate::PreUpdate
		void PreUpdate(FUpdateTraversalContext& Context, const FDecoratorUpdateState& DecoratorState) const
		{
			GetInterface()->PreUpdate(Context, *this, DecoratorState);
		}

		// @see IUpdate::QueueChildrenForTraversal
		void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
		{
			GetInterface()->QueueChildrenForTraversal(Context, *this, DecoratorState, TraversalQueue);
		}

		// @see IUpdate::PostUpdate
		void PostUpdate(FUpdateTraversalContext& Context, const FDecoratorUpdateState& DecoratorState) const
		{
			GetInterface()->PostUpdate(Context, *this, DecoratorState);
		}

	protected:
		const IUpdate* GetInterface() const { return GetInterfaceTyped<IUpdate>(); }
	};

	/**
	 * Updates a sub-graph starting at its root.
	 *
	 * For each node:
	 *     - We call PreUpdate on all its decorators
	 *     - We update all children
	 *     - We call PostUpdate on all its decorators
	 *
	 * @see IUpdate::PreUpdate, IUpdate::PostUpdate, IHierarchy::GetChildren
	 */
	ANIMNEXT_API void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime);
}
