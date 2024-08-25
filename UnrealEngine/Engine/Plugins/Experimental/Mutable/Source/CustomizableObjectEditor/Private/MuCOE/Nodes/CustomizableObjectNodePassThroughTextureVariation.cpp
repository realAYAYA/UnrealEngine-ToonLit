// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureVariation.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodePassThroughTextureVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_PassThroughImage, FName(TEXT("Texture")));
	OutputPin->bDefaultValueIsIgnored = true;

	for (int VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		FString PinName = FString::Printf(TEXT("Variation %d"), VariationIndex);
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Schema->PC_PassThroughImage, FName(*PinName), false);
		VariationPin->bDefaultValueIsIgnored = true;

		FString FriendlyName = FString::Printf(TEXT("Variation %d [%s]"), VariationIndex, *Variations[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::FromString(FriendlyName);
	}

	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, Schema->PC_PassThroughImage, FName(TEXT("Default")), false);
	DefaultVariation->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodePassThroughTextureVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PassThrough_Texture_Variation", "PassThrough Texture Variation");
}

#undef LOCTEXT_NAMESPACE
