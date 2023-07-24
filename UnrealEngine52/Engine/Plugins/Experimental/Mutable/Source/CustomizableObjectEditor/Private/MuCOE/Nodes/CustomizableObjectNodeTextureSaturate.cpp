// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSaturate.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeTextureSaturate::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Base Texture");
	UEdGraphPin* ImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));

	BaseImagePinReference = FEdGraphPinReference(ImagePin);
	
	PinName = TEXT("Factor");
	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	FactorPin->bDefaultValueIsIgnored = true;

	FactorPinReference = FEdGraphPinReference(FactorPin);
}

UEdGraphPin* UCustomizableObjectNodeTextureSaturate::GetBaseImagePin() const
{
	return BaseImagePinReference.Get();
}

UEdGraphPin* UCustomizableObjectNodeTextureSaturate::GetFactorPin() const
{
	return FactorPinReference.Get();
}

FText UCustomizableObjectNodeTextureSaturate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Saturate", "Texture Saturate");
}

FLinearColor UCustomizableObjectNodeTextureSaturate::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}

FText UCustomizableObjectNodeTextureSaturate::GetTooltipText() const
{
	return LOCTEXT("Texture_Saturate_Tooltip", "Get the provided texture with its saturation adjusted based on the numerical input provided where 1 equals to full saturation and 0 to no saturation.");
}

#undef LOCTEXT_NAMESPACE
