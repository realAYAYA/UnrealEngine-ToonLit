// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"

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

