// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithUVMask.h"

#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"



void UCustomizableObjectNodeModifierClipWithUVMask::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ClipMaskPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName("Clip Mask"));
	ClipMaskPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* OutputPin_p = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Modifier"));
}


FText UCustomizableObjectNodeModifierClipWithUVMask::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Clip_With_UV_Mask", "Clip With UV Mask");
}


FLinearColor UCustomizableObjectNodeModifierClipWithUVMask::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


void UCustomizableObjectNodeModifierClipWithUVMask::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

	if (Editor.IsValid())
	{
		Editor->UpdateGraphNodeProperties();
	}
}


UEdGraphPin* UCustomizableObjectNodeModifierClipWithUVMask::OutputPin() const
{
	return FindPin(TEXT("Modifier"));
}


UEdGraphPin* UCustomizableObjectNodeModifierClipWithUVMask::ClipMaskPin() const
{
	return FindPin(TEXT("Clip Mask"));
}


FText UCustomizableObjectNodeModifierClipWithUVMask::GetTooltipText() const
{
	return LOCTEXT("Clip_Mask_Tooltip", "Removes the part of a material that has a UV layout inside a mask defined with a texture.\nIt only removes the faces that fall completely inside the mask, along with the vertices and edges that define only faces that are deleted.");
}

#undef LOCTEXT_NAMESPACE
