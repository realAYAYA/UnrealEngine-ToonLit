// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IUpdate.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IHierarchy.h"

namespace UE::AnimNext
{
	void IUpdate::PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PreUpdate(Context);
		}
	}

	void IUpdate::PostUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const
	{
		TDecoratorBinding<IUpdate> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.PostUpdate(Context);
		}
	}

	enum class EUpdateStep
	{
		PreUpdate,
		PostUpdate,
	};

	struct FUpdateEntry
	{
		FWeakDecoratorPtr	DecoratorPtr;
		EUpdateStep			DesiredStep = EUpdateStep::PreUpdate;
	};

	void UpdateGraph(FExecutionContext& Context, FWeakDecoratorPtr GraphRootPtr)
	{
		if (!GraphRootPtr.IsValid())
		{
			return;	// Nothing to update
		}

		FMemMark Mark(FMemStack::Get());

		TArray<FUpdateEntry, TMemStackAllocator<>> NodesPendingUpdate;
		NodesPendingUpdate.Reserve(64);

		FChildrenArray Children;
		Children.Reserve(64);

		FUpdateTraversalContext TraversalContext;

		FScopedTraversalContext ScopedTraversalContext(Context, TraversalContext);

		// Add the graph root to kick start the update process
		NodesPendingUpdate.Push({ GraphRootPtr, EUpdateStep::PreUpdate });

		// Process every node twice: pre-update and post-update
		while (!NodesPendingUpdate.IsEmpty())
		{
			// Grab the top most entry
			const bool bAllowShrinking = false;	// Don't allow shrinking to avoid churn
			const FUpdateEntry Entry = NodesPendingUpdate.Pop(bAllowShrinking);

			if (Entry.DesiredStep == EUpdateStep::PreUpdate)
			{
				// This is the first time we visit this node, time to pre-update
				// Queue our node again so that post-update is called afterwards
				NodesPendingUpdate.Push({ Entry.DecoratorPtr, EUpdateStep::PostUpdate });

				TDecoratorBinding<IUpdate> UpdateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, UpdateDecorator))
				{
					UpdateDecorator.PreUpdate(Context);
				}

				TDecoratorBinding<IHierarchy> HierarchyDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, HierarchyDecorator))
				{
					HierarchyDecorator.GetChildren(Context, Children);

					// Append our children in reserve order so that they are visited in the same order they were added
					for (int32 ChildIndex = Children.Num() - 1; ChildIndex >= 0; --ChildIndex)
					{
						NodesPendingUpdate.Push({ Children[ChildIndex], EUpdateStep::PreUpdate });
					}

					// Reset our container for the next time we need it
					Children.Reset();
				}

				// Break and continue to the next top-most entry
				// It is either a child ready for its pre-update or our current entry ready for its post-update (leaf)
			}
			else
			{
				// We've already visited this node once, time to post-update
				TDecoratorBinding<IUpdate> UpdateDecorator;
				if (Context.GetInterface(Entry.DecoratorPtr, UpdateDecorator))
				{
					UpdateDecorator.PostUpdate(Context);
				}

				// Break and continue to the next top-most entry
				// It is either a sibling read for its post-update or our parent entry ready for its post-update
			}
		}
	}
}
