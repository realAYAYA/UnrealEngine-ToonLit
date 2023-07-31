// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureBinarise.h"

#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureBinarise::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Base Texture");
	UEdGraphPin* ImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));

	BaseImagePinReference = FEdGraphPinReference(ImagePin);
	
	PinName = TEXT("Threshold");
	UEdGraphPin* ThresholdPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
}


UEdGraphPin* UCustomizableObjectNodeTextureBinarise::GetBaseImagePin() const
{
	return BaseImagePinReference.Get();
}


FText UCustomizableObjectNodeTextureBinarise::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Binarise", "Texture Binarise");
}


FLinearColor UCustomizableObjectNodeTextureBinarise::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureBinarise::GetTooltipText() const
{
	return LOCTEXT("Texture_Binarise_Tooltip", "Turns a base texture into black and white using a threshold.");
}


void UCustomizableObjectNodeTextureBinarise::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		BaseImagePinReference = FEdGraphPinReference(FindPin(TEXT("Base Image")));
	}
}


#undef LOCTEXT_NAMESPACE
