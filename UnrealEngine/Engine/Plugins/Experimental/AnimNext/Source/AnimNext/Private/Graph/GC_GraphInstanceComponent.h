// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "Graph/GraphInstanceComponent.h"

struct FAnimNextGraphInstance;
class FReferenceCollector;

namespace UE::AnimNext
{
	/**
	 * FGCGraphInstanceComponent
	 *
	 * This component maintains the necessary state to garbage collection.
	 */
	struct FGCGraphInstanceComponent : public FGraphInstanceComponent
	{
		DECLARE_ANIM_GRAPH_INSTANCE_COMPONENT(FGCGraphInstanceComponent)

		// Registers the provided decorator with the GC system
		// Once registered, IGarbageCollection::AddReferencedObjects will be called on it during GC
		void Register(FAnimNextGraphInstance& GraphInstance, const FWeakDecoratorPtr& DecoratorPtr);

		// Unregisters the provided decorator from the GC system
		void Unregister(const FWeakDecoratorPtr& DecoratorPtr);

		// Called during garbage collection to collect strong object references
		void AddReferencedObjects(FReferenceCollector& Collector) const;

	private:
		struct FEntry
		{
			FEntry(FAnimNextGraphInstance& InGraphInstance, const FWeakDecoratorPtr& InDecoratorPtr)
				: GraphInstance(InGraphInstance)
				, DecoratorPtr(InDecoratorPtr)
			{
			}

			FAnimNextGraphInstance& GraphInstance;
			FWeakDecoratorPtr DecoratorPtr;
		};

		// List of decorator handles that contain UObject references and implement IGarbageCollection
		TArray<FEntry> DecoratorsWithReferences;
	};
}
