// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IGarbageCollection.h"

#include "DecoratorBase/ExecutionContext.h"
#include "Graph/GC_GraphInstanceComponent.h"

namespace UE::AnimNext
{
	void IGarbageCollection::RegisterWithGC(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FGCGraphInstanceComponent& Component = Context.GetComponent<FGCGraphInstanceComponent>();
		Component.Register(Context.GetGraphInstance(), Binding.GetDecoratorPtr());
	}

	void IGarbageCollection::UnregisterWithGC(const FExecutionContext& Context, const FDecoratorBinding& Binding)
	{
		FGCGraphInstanceComponent& Component = Context.GetComponent<FGCGraphInstanceComponent>();
		Component.Unregister(Binding.GetDecoratorPtr());
	}

	void IGarbageCollection::AddReferencedObjects(const FExecutionContext& Context, const TDecoratorBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const
	{
		TDecoratorBinding<IGarbageCollection> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.AddReferencedObjects(Context, Collector);
		}
	}
}
