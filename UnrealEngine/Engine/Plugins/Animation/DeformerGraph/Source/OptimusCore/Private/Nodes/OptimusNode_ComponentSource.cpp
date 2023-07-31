// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_ComponentSource.h"

#include "OptimusNodePin.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"


void UOptimusNode_ComponentSource::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (!Binding)
	{
		return;
	}
	
	if (!GetOwningGraph())
	{
		return;
	}
	
	const UOptimusDeformer* OldBindingOwner = Binding->GetOwningDeformer();
	const UOptimusDeformer* NewBindingOwner = Cast<UOptimusDeformer>(GetOwningGraph()->GetCollectionRoot());

	if (!NewBindingOwner)
	{
		return;
	}
	
	// No action needed if we are copying/pasting within the same deformer asset 
	if (OldBindingOwner == NewBindingOwner)
	{
		return;
	}

	// Refresh the binding so that we don't hold a reference to a binding in another deformer asset
	Binding = NewBindingOwner->ResolveComponentBinding(Binding->GetFName());
}

void UOptimusNode_ComponentSource::SetComponentSourceBinding(
	UOptimusComponentSourceBinding* InBinding
	)
{
	Binding = InBinding;

	SetDisplayName(FText::FromName(Binding->BindingName));
}


FName UOptimusNode_ComponentSource::GetNodeCategory() const
{
	static FName ComponentCategory("Component");
	return ComponentCategory;
}


void UOptimusNode_ComponentSource::ConstructNode()
{
	const FOptimusDataTypeRegistry& TypeRegistry = FOptimusDataTypeRegistry::Get();
	FOptimusDataTypeRef ComponentSourceType = TypeRegistry.FindType(*UOptimusComponentSourceBinding::StaticClass());

	if (ensure(ComponentSourceType.IsValid()) &&
		ensure(Binding) &&
		ensure(Binding->ComponentType))
	{
		AddPinDirect(Binding->GetComponentSource()->GetBindingName(), EOptimusNodePinDirection::Output, {}, ComponentSourceType);
	}
}

UOptimusNodePin* UOptimusNode_ComponentSource::GetComponentPin() const
{
	if (ensure(!GetPins().IsEmpty()))
	{
		return GetPins()[0];
	}
	return nullptr;
}
