// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IEvaluate.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_EvaluateGraph);

namespace UE::AnimNext
{
	FEvaluateTraversalContext::FEvaluateTraversalContext(const FExecutionContext& InExecutionContext, FEvaluationProgram& InEvaluationProgram)
		: FExecutionContextProxy(InExecutionContext)
		, EvaluationProgram(InEvaluationProgram)
	{
	}

	void IEvaluate::PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		TDecoratorBinding<IEvaluate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PreEvaluate(Context);
		}
	}

	void IEvaluate::PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const
	{
		TDecoratorBinding<IEvaluate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PostEvaluate(Context);
		}
	}

	enum class EEvaluateStep
	{
		PreEvaluate,
		PostEvaluate,
	};

	// This structure is transient and lives either on the stack or the memstack and its destructor may not be called
	struct FEvaluateEntry
	{
		// The decorator handle that points to our node to update
		FWeakDecoratorPtr	DecoratorPtr;

		// Which step we wish to perform when we next see this entry
		EEvaluateStep		DesiredStep = EEvaluateStep::PreEvaluate;

		// Once we've called PreUpdate, we cache the decorator binding to avoid a redundant query to call PostUpdate
		TDecoratorBinding<IEvaluate> EvaluateDecorator;

		// These pointers are mutually exclusive
		// An entry is either part of the pending stack, the free list, or neither
		union
		{
			// Next entry in the linked list of free entries
			FEvaluateEntry* NextFreeEntry = nullptr;

			// Previous entry below us in the nodes pending stack
			FEvaluateEntry* PrevStackEntry;
		};

		FEvaluateEntry(const FWeakDecoratorPtr& InDecoratorPtr, EEvaluateStep InDesiredStep, FEvaluateEntry* InPrevStackEntry = nullptr)
			: DecoratorPtr(InDecoratorPtr)
			, DesiredStep(InDesiredStep)
			, PrevStackEntry(InPrevStackEntry)
		{
		}
	};

	FEvaluationProgram EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance)
	{
		return EvaluateGraph(GraphInstance.GetGraphRootPtr());
	}

	FEvaluationProgram EvaluateGraph(const FWeakDecoratorPtr& GraphRootPtr)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_EvaluateGraph);
		
		FEvaluationProgram EvaluationProgram;

		if (!GraphRootPtr.IsValid())
		{
			return EvaluationProgram;	// Nothing to update
		}

		FMemStack& MemStack = FMemStack::Get();
		FMemMark Mark(MemStack);

		FChildrenArray Children;
		Children.Reserve(16);

		FExecutionContext ExecutionContext;
		FEvaluateTraversalContext TraversalContext(ExecutionContext, EvaluationProgram);

		// Add the graph root to kick start the evaluation process
		FEvaluateEntry GraphRootEntry(GraphRootPtr, EEvaluateStep::PreEvaluate);
		FEvaluateEntry* NodesPendingUpdateStackTop = &GraphRootEntry;

		// List of free entries we can recycle
		FEvaluateEntry* FreeEntryList = nullptr;

		TDecoratorBinding<IHierarchy> HierarchyDecorator;

		// Process every node twice: pre-evaluate and post-evaluate
		while (NodesPendingUpdateStackTop != nullptr)
		{
			// Grab the top most entry
			FEvaluateEntry* Entry = NodesPendingUpdateStackTop;
			bool bIsEntryUsed = true;

			const FWeakDecoratorPtr& EntryDecoratorPtr = Entry->DecoratorPtr;

			// Make sure the execution context is bound to our graph instance
			ExecutionContext.BindTo(EntryDecoratorPtr);

			if (Entry->DesiredStep == EEvaluateStep::PreEvaluate)
			{
				if (ExecutionContext.GetInterface(EntryDecoratorPtr, Entry->EvaluateDecorator))
				{
					// This is the first time we visit this node, time to pre-evaluate
					Entry->EvaluateDecorator.PreEvaluate(TraversalContext);

					// Leave our entry on top of the stack, we'll need to call PostEvaluate once the children
					// we'll push on top finish
					Entry->DesiredStep = EEvaluateStep::PostEvaluate;
				}
				else
				{
					// This node doesn't implement IUpdate, we can pop it from the stack
					NodesPendingUpdateStackTop = Entry->PrevStackEntry;
					bIsEntryUsed = false;
				}

				if (ExecutionContext.GetInterface(EntryDecoratorPtr, HierarchyDecorator))
				{
					HierarchyDecorator.GetChildren(ExecutionContext, Children);

					// Append our children in reserve order so that they are visited in the same order they were added
					for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
					{
						// Insert our new child on top of the stack

						FEvaluateEntry* ChildEntry;
						if (FreeEntryList != nullptr)
						{
							// Grab an entry from the free list
							ChildEntry = FreeEntryList;
							FreeEntryList = ChildEntry->NextFreeEntry;

							ChildEntry->DecoratorPtr = Children[ChildIndex];
							ChildEntry->DesiredStep = EEvaluateStep::PreEvaluate;
							ChildEntry->PrevStackEntry = NodesPendingUpdateStackTop;
						}
						else
						{
							// Allocate a new entry
							ChildEntry = new(MemStack) FEvaluateEntry(Children[ChildIndex], EEvaluateStep::PreEvaluate, NodesPendingUpdateStackTop);
						}

						NodesPendingUpdateStackTop = ChildEntry;
					}

					// Reset our container for the next time we need it
					Children.Reset();
				}

				// Break and continue to the next top-most entry
				// It is either a child ready for its pre-evaluate or our current entry ready for its post-evaluate (leaf)
			}
			else
			{
				// We've already visited this node once, time to post-update
				check(Entry->EvaluateDecorator.IsValid());
				Entry->EvaluateDecorator.PostEvaluate(TraversalContext);

				// Now that we are done processing this entry, we can pop it
				NodesPendingUpdateStackTop = Entry->PrevStackEntry;
				bIsEntryUsed = false;

				// Break and continue to the next top-most entry
				// It is either a sibling read for its post-evaluate or our parent entry ready for its post-evaluate
			}

			if (!bIsEntryUsed)
			{
				// This entry is no longer used, add it to the free list
				Entry->NextFreeEntry = FreeEntryList;
				FreeEntryList = Entry;
			}
		}

		return EvaluationProgram;
	}
}
