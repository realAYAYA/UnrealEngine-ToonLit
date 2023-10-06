// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		OutputPinReference = FindPin(TEXT("Image"));
	}
}


void UCustomizableObjectNodeTextureSwitch::BackwardsCompatibleFixup()
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


FString UCustomizableObjectNodeTextureSwitch::GetOutputPinName() const
{
	return TEXT("Texture");
}


FName UCustomizableObjectNodeTextureSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Image;
}


FString UCustomizableObjectNodeTextureSwitch::GetPinPrefix() const
{
	return TEXT("Texture ");
}


#undef LOCTEXT_NAMESPACE

