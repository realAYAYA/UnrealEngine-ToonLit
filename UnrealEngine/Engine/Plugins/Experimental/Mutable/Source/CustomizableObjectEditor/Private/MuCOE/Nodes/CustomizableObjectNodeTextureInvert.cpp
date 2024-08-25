// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureInvert.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureInvert::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Base Texture");
	UEdGraphPin* ImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));

	BaseImagePinReference = FEdGraphPinReference(ImagePin);
}


void UCustomizableObjectNodeTextureInvert::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		BaseImagePinReference = FEdGraphPinReference(FindPin(TEXT("Base Image")));
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::FixPinsNamesImageToTexture2)
	{
		bool Replaced = false;
		if (UEdGraphPin* TexturePin = FindPin(TEXT("Image")))
		{
			TexturePin->PinName = TEXT("Texture");
			Replaced = true;
		}

		if (UEdGraphPin* TexturePin = FindPin(TEXT("Base Image")))
		{
			TexturePin->PinName = TEXT("Base Texture");
			Replaced = true;
		}

		if(Replaced)
		{
			UCustomizableObjectNode::ReconstructNode();
		}
	}
}


UEdGraphPin* UCustomizableObjectNodeTextureInvert::GetBaseImagePin() const
{
	return BaseImagePinReference.Get();
}


FText UCustomizableObjectNodeTextureInvert::GetNodeTitle(ENodeTitleType::Type TitleType)const
{
	return LOCTEXT("Texture_Invert", "Texture Invert");
}


FLinearColor UCustomizableObjectNodeTextureInvert::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureInvert::GetTooltipText() const
{
	return LOCTEXT("Texture_Invert_Tooltip", "Inverts the colors of a base texture.");
}


#undef LOCTEXT_NAMESPACE
