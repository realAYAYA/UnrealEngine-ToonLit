// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureSwitch::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		OutputPinReference = FindPin(TEXT("Image"));
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::FixPinsNamesImageToTexture2)
	{
		if (UEdGraphPin* TexturePin = FindPin(TEXT("Image")))
		{
			TexturePin->PinName = TEXT("Texture");
			UCustomizableObjectNode::ReconstructNode();
		}
	}
}


FName UCustomizableObjectNodeTextureSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Image;
}


#undef LOCTEXT_NAMESPACE

