// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/GC_GraphInstanceComponent.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorInterfaces/IGarbageCollection.h"

namespace UE::AnimNext
{
	void FGCGraphInstanceComponent::Register(FAnimNextGraphInstance& GraphInstance, const FWeakDecoratorPtr& DecoratorPtr)
	{
		DecoratorsWithReferences.Add(FEntry(GraphInstance, DecoratorPtr));
	}

	void FGCGraphInstanceComponent::Unregister(const FWeakDecoratorPtr& DecoratorPtr)
	{
		const int32 EntryIndex = DecoratorsWithReferences.IndexOfByPredicate(
			[&DecoratorPtr](const FEntry& Entry)
			{
				return Entry.DecoratorPtr == DecoratorPtr;
			});

		if (ensure(EntryIndex != INDEX_NONE))
		{
			DecoratorsWithReferences.RemoveAtSwap(EntryIndex);
		}
	}

	void FGCGraphInstanceComponent::AddReferencedObjects(FReferenceCollector& Collector) const
	{
		FExecutionContext Context;
		TDecoratorBinding<IGarbageCollection> GCDecorator;

		// TODO: If we kept the entries sorted by graph instance, we could re-use the execution context
		for (const FEntry& Entry : DecoratorsWithReferences)
		{
			Context.BindTo(Entry.GraphInstance);
			ensure(Context.GetInterface(Entry.DecoratorPtr, GCDecorator));

			GCDecorator.AddReferencedObjects(Context, Collector);
		}
	}
}
