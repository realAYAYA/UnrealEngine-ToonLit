// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParameterBlockProxy.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FParameterBlockProxy::FParameterBlockProxy(UAnimNextParameterBlock* InParameterBlock)
	: ParameterBlock(InParameterBlock)
	, PropertyBag(ParameterBlock->PropertyBag)
	, LayerHandle(FParamStack::MakeReferenceLayer(PropertyBag))
{
	check(ParameterBlock);
}

void FParameterBlockProxy::Update(float DeltaTime)
{
#if WITH_EDITOR	// Layout should only be changing in editor
	const FInstancedPropertyBag* HandlePropertyBag = LayerHandle.As<FInstancedPropertyBag>();
	if(HandlePropertyBag == nullptr || HandlePropertyBag->GetPropertyBagStruct() != ParameterBlock->PropertyBag.GetPropertyBagStruct())
	{
		PropertyBag = ParameterBlock->PropertyBag;
		LayerHandle = FParamStack::MakeReferenceLayer(PropertyBag);
	}
#endif

	ParameterBlock->UpdateLayer(LayerHandle, DeltaTime);
}

void FParameterBlockProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ParameterBlock);
	PropertyBag.AddStructReferencedObjects(Collector);
}

}
