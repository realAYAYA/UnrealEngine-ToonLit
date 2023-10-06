// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureColourMap.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureColourMap::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Base");
	UEdGraphPin* SourcePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
	SourcePin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Mask");
	UEdGraphPin* MaskPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
	MaskPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Map");
	UEdGraphPin* GradientPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
	GradientPin->bDefaultValueIsIgnored = true;
}


void UCustomizableObjectNodeTextureColourMap::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::FixPinsNamesImageToTexture2)
	{
		if (UEdGraphPin* TexturePin = FindPin(TEXT("Image")))
		{
			TexturePin->PinName = TEXT("Texture");
			UCustomizableObjectNode::ReconstructNode();
		}
	}
}


FText UCustomizableObjectNodeTextureColourMap::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Map", "Texture Colour Map");
}


FLinearColor UCustomizableObjectNodeTextureColourMap::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureColourMap::GetTooltipText() const
{
	return LOCTEXT("Texture_Gradient_Sample_Tooltip", "Map colours of map using values form image.");
}

#undef LOCTEXT_NAMESPACE

