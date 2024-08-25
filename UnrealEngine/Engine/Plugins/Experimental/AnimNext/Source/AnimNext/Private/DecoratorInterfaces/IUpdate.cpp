// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IUpdate.h"

#include "DecoratorInterfaces/IHierarchy.h"
#include "Graph/GraphInstanceComponent.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_UpdateGraph);

namespace UE::AnimNext
{
	void IUpdate::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PreUpdate(Context, DecoratorState);
		}
	}

	void IUpdate::QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.QueueChildrenForTraversal(Context, DecoratorState, TraversalQueue);
		}
	}

	void IUpdate::PostUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PostUpdate(Context, DecoratorState);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Traversal implementation

	// This structure is transient and lives either on the stack or the memstack and its destructor may not be called
	struct FUpdateEntry
	{
		// The decorator state for this entry
		FDecoratorUpdateState		DecoratorState;

		// The decorator handle that points to our node to update
		FWeakDecoratorPtr			DecoratorPtr;

		// Whether or not PreUpdate had been called already
		// TODO: Store bHasPreUpdated in the LSB of the entry pointer to save padding?
		bool						bHasPreUpdated = false;

		// Once we've called PreUpdate, we cache the decorator binding to avoid a redundant query to call PostUpdate
		TDecoratorBinding<IUpdate>	UpdateDecorator;

		// These pointers are mutually exclusive
		// An entry is either part of the queued update stack, the update stack, the free list, or none of the above
		union
		{
			// Next entry in the stack of free entries
			FUpdateEntry*			NextFreeEntry = nullptr;

			// Previous entry on the update stack
			FUpdateEntry*			PrevUpdateStackEntry;

			// Previous entry on the queued update stack
			FUpdateEntry*			PrevQueuedUpdateStackEntry;
		};

		FUpdateEntry(const FWeakDecoratorPtr& InDecoratorPtr, const FDecoratorUpdateState InDecoratorState)
			: DecoratorState(InDecoratorState)
			, DecoratorPtr(InDecoratorPtr)
		{
		}
	};

	FUpdateTraversalContext::FUpdateTraversalContext(const FExecutionContext& InExecutionContext, FMemStack& InMemStack)
		: FExecutionContextProxy(InExecutionContext)
		, MemStack(InMemStack)
	{}

	void FUpdateTraversalContext::PushQueuedUpdateEntries(FUpdateTraversalQueue& TraversalQueue)
	{
		// Pop every entry from the queued update stack and push them onto the update stack
		// reversing their order
		while (FUpdateEntry* Entry = TraversalQueue.QueuedUpdateStackHead)
		{
			// Update our queued stack head
			TraversalQueue.QueuedUpdateStackHead = Entry->PrevQueuedUpdateStackEntry;

			// Push our new entry onto the update stack
			PushUpdateEntry(Entry);
		}
	}

	void FUpdateTraversalContext::PushUpdateEntry(FUpdateEntry* Entry)
	{
		Entry->PrevUpdateStackEntry = UpdateStackHead;
		UpdateStackHead = Entry;
	}

	FUpdateEntry* FUpdateTraversalContext::PopUpdateEntry()
	{
		FUpdateEntry* ChildEntry = UpdateStackHead;
		if (ChildEntry != nullptr)
		{
			// We have a child, set our new head
			UpdateStackHead = ChildEntry->PrevUpdateStackEntry;
		}

		return ChildEntry;
	}

	void FUpdateTraversalContext::PushFreeEntry(FUpdateEntry* Entry)
	{
		Entry->NextFreeEntry = FreeEntryStackHead;
		FreeEntryStackHead = Entry;
	}

	FUpdateEntry* FUpdateTraversalContext::GetNewEntry(const FWeakDecoratorPtr& DecoratorPtr, const FDecoratorUpdateState& DecoratorState)
	{
		FUpdateEntry* FreeEntry = FreeEntryStackHead;
		if (FreeEntry != nullptr)
		{
			// We have a free entry, set our new head
			FreeEntryStackHead = FreeEntry->NextFreeEntry;

			// Update our entry
			FreeEntry->DecoratorState = DecoratorState;
			FreeEntry->DecoratorPtr = DecoratorPtr;
			FreeEntry->bHasPreUpdated = false;
			FreeEntry->NextFreeEntry = nullptr;		// Mark it as not being a member of any list
		}
		else
		{
			// Allocate a new entry
			FreeEntry = new(MemStack) FUpdateEntry(DecoratorPtr, DecoratorState);
		}

		return FreeEntry;
	}

	FUpdateTraversalQueue::FUpdateTraversalQueue(FUpdateTraversalContext& InTraversalContext)
		: TraversalContext(InTraversalContext)
	{
	}

	void FUpdateTraversalQueue::Push(const FWeakDecoratorPtr& ChildPtr, const FDecoratorUpdateState& ChildDecoratorState)
	{
		if (!ChildPtr.IsValid())
		{
			return;	// Don't queue invalid pointers
		}

		FUpdateEntry* ChildEntry = TraversalContext.GetNewEntry(ChildPtr, ChildDecoratorState);

		// We push children that are queued onto a stack
		// Once pre-update is done, we'll pop queued entries one by one and push them
		// onto the update stack
		// This has the effect of reversing the entries so that they are traversed in
		// the same order they are queued in:
		//    - First queued will be at the bottom of the queued stack and it ends up at the
		//      at the top of the update stack (last entry pushed)
		ChildEntry->PrevQueuedUpdateStackEntry = QueuedUpdateStackHead;
		QueuedUpdateStackHead = ChildEntry;
	}

	// Performance note
	// When we process an animation graph for a frame, typically we'll update first before we evaluate
	// As a result of this, when we query for the update interface here, we will likely hit cold memory
	// which will cache miss (by touching the graph instance for the first time).
	// 
	// The processor will cache miss and continue to process as many instructions as it can before the
	// out-of-order execution window fills up. This is problematic here because a lot of the subsequent
	// instructions depend on the node instance and the interface it returns. The processor will be unable
	// to execute any of the instructions that follow in the current loop iteration. However, it might
	// be able to get started on the next node entry which is likely to cache miss as well. Should the
	// processor make it that far and it turns out that we have to push a child onto the stack, all
	// of the work it tried to do ahead of time will have to be thrown away.
	// 
	// There are two things that we can do here to try and help performance: prefetch ahead and bulk
	// query.
	// 
	// If we prefetch, we have to be careful because we do not know what the node will do in its PreUpdate.
	// If it turns out that it does a lot of work, our prefetch might end up getting thrown out. This is
	// because prefetched cache lines typically end up being the first evicted unless they are touched first.
	// It is thus dangerous to use manual prefetching when the memory access pattern isn't fully known.
	// In practice, it is likely viable as most nodes won't do too much work.
	// 
	// A better approach could be to instead bulk query for our interfaces. We could cache in the FUpdateEntry
	// the decorator bindings for IUpdate and IHierarchy (and re-use the binding for IUpdate for PostUpdate).
	// Every iteration we could check how many children are queued up on the stack. We could then grab N
	// entries (2 to 4) and query their interfaces in bulk. The idea is to clump the cache miss instructions
	// together and to interleave the interface queries. This will queue up as much work as possible in the
	// out-of-order execution window that will not be thrown away because of a branch. Eventually the first
	// interface query will complete and execution will resume here to call PreUpdate etc. This will be able
	// to happen while the processor still waits on the cache misses and finishes the interface query of the
	// other bulked children. The same effect could be achieved by querying the interfaces after the call
	// to GetChildren by bulk querying all of them right then. This way, as soon as the execution window
	// can clear the end of the loop, it can start working on the next entry which will be warm in the L1
	// cache allowing the CPU to carry ahead before all child interfaces are fully resolved.
	// 
	// The above may seem like a stretch and an insignificant over optimization but it could very well be the
	// key to unlocking large performance gains during traversal. The above optimization would allow us to
	// perform as much useful work as possible while waiting for memory, hiding its slow latency by fully
	// leveraging out-of-order CPU execution.

	void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_UpdateGraph);
		
		if (!GraphInstance.IsValid())
		{
			return;	// Nothing to update
		}

		if (!ensure(GraphInstance.IsRoot()))
		{
			return;	// We can only update starting at the root
		}

		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);

		FChildrenArray Children;
		TDecoratorBinding<IHierarchy> HierarchyDecorator;

		FExecutionContext ExecutionContext;
		FUpdateTraversalContext TraversalContext(ExecutionContext, MemStack);
		FUpdateTraversalQueue TraversalQueue(TraversalContext);

		// Before we start the traversal, we give the graph instance components the chance to do some work
		ExecutionContext.BindTo(GraphInstance);
		for (auto It = ExecutionContext.GetComponentIterator(); It; ++It)
		{
			It.Value()->PreUpdate(ExecutionContext);
		}

		// Add the graph root to start the update process
		FUpdateEntry RootEntry(GraphInstance.GetGraphRootPtr(), FDecoratorUpdateState(DeltaTime));
		TraversalContext.PushUpdateEntry(&RootEntry);

		while (FUpdateEntry* Entry = TraversalContext.PopUpdateEntry())
		{
			const FWeakDecoratorPtr& EntryDecoratorPtr = Entry->DecoratorPtr;

			// Make sure the execution context is bound to our graph instance
			ExecutionContext.BindTo(EntryDecoratorPtr);

			if (!Entry->bHasPreUpdated)
			{
				// This is the first time we visit this node, time to pre-update
				// But first, if it has latent pins, we must execute and cache their results
				// This will ensure that other calls into this node will have a consistent view of
				// what the node saw when it started to update. We thus take a snapshot.
				const bool bIsFrozen = false;	// Not yet supported
				ExecutionContext.SnapshotLatentProperties(EntryDecoratorPtr, bIsFrozen);

				if (ExecutionContext.GetInterface(EntryDecoratorPtr, Entry->UpdateDecorator))
				{
					Entry->UpdateDecorator.PreUpdate(TraversalContext, Entry->DecoratorState);

					// Make sure that next time we visit this entry, we'll post-update
					Entry->bHasPreUpdated = true;

					// Push this entry onto the update stack, we'll call it once all our children have finished executing
					TraversalContext.PushUpdateEntry(Entry);

					// Request that the decorator queues the children it wants to visit
					// This is a separate function from PreUpdate to simplify traversal management. It is often the case that
					// the base decorator is the one best placed to figure out how to optimally queue children since it
					// owns the handles to them. However, if an additive decorator wishes to override PreUpdate, it might want
					// to perform logic after the base PreUpdate but before children are queued. Without a separate function,
					// we would have to rewrite the base PreUpdate entirely and use IHierarchy to query the handles of our children.
					Entry->UpdateDecorator.QueueChildrenForTraversal(TraversalContext, Entry->DecoratorState, TraversalQueue);

					// Iterate over our queued children and push them onto the update stack
					// We do this to allow children to be queued in traversal order which is intuitive
					// but to traverse them in that order, they must be pushed in reverse order onto the update stack
					TraversalContext.PushQueuedUpdateEntries(TraversalQueue);
				}
				else
				{
					// This node doesn't implement IUpdate
					// We'll grab its children and traverse them
					if (ExecutionContext.GetInterface(EntryDecoratorPtr, HierarchyDecorator))
					{
						HierarchyDecorator.GetChildren(ExecutionContext, Children);

						// Append our children in reserve order so that they are visited in the same order they were added
						for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
						{
							FUpdateEntry* ChildEntry = TraversalContext.GetNewEntry(Children[ChildIndex], Entry->DecoratorState);
							TraversalContext.PushUpdateEntry(ChildEntry);
						}

						// Reset our container for the next time we need it
						Children.Reset();
					}

					// We don't need this entry anymore
					TraversalContext.PushFreeEntry(Entry);
				}
			}
			else
			{
				// We've already visited this node once, time to PostUpdate
				check(Entry->UpdateDecorator.IsValid());
				Entry->UpdateDecorator.PostUpdate(TraversalContext, Entry->DecoratorState);

				// We don't need this entry anymore
				TraversalContext.PushFreeEntry(Entry);
			}
		}

		// After we finish the traversal, we give the graph instance components the chance to do some work
		ExecutionContext.BindTo(GraphInstance);
		for (auto It = ExecutionContext.GetComponentIterator(); It; ++It)
		{
			It.Value()->PostUpdate(ExecutionContext);
		}
	}
}
