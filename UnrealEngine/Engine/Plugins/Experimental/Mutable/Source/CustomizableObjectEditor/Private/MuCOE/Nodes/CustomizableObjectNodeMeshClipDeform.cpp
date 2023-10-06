// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipDeform.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMeshClipDeform::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	
	UEdGraphPin* ClipMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Clip Shape"));
	ClipMeshPin->bDefaultValueIsIgnored = true;
	
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Material"));
	ClipMeshPin->bDefaultValueIsIgnored = true;
}


UEdGraphPin* UCustomizableObjectNodeMeshClipDeform::ClipShapePin() const
{
	return FindPin(TEXT("Clip Shape"), EGPD_Input);
}


UEdGraphPin* UCustomizableObjectNodeMeshClipDeform::OutputPin() const
{
	return FindPin(TEXT("Material"));
}


FText UCustomizableObjectNodeMeshClipDeform::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Clip_Deform_Mesh", "Clip Deform Mesh");
}

FLinearColor UCustomizableObjectNodeMeshClipDeform::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}

FText UCustomizableObjectNodeMeshClipDeform::GetTooltipText() const
{
	return LOCTEXT("Clip_Deform_Tooltip", "Defines a clip with mesh deformation based on a shape mesh and blend weights.");

}
#undef LOCTEXT_NAMESPACE
