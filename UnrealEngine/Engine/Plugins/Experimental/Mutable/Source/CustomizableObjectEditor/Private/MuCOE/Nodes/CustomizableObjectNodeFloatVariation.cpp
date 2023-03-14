// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatVariation.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeFloatVariation::UCustomizableObjectNodeFloatVariation()
	: Super()
{

}


void UCustomizableObjectNodeFloatVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged  )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeFloatVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName(TEXT("Float")));
	OutputPin->bDefaultValueIsIgnored = true;

	for ( int VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		FString RemapIndex = Helper_GetPinName(VariationPin(VariationIndex));

		FString PinName = FString::Printf( TEXT("Variation %d"), VariationIndex);
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName), false);
		VariationPin->bDefaultValueIsIgnored = true;

		FString FriendlyName = FString::Printf(TEXT("Variation %d [%s]"), VariationIndex, *Variations[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::FromString(FriendlyName);
	}

	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(TEXT("Default")), false);
	DefaultVariation->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeFloatVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Float_Variation", "Float Variation");
}


FLinearColor UCustomizableObjectNodeFloatVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Float);
}


FText UCustomizableObjectNodeFloatVariation::GetTooltipText() const
{
	return LOCTEXT("Float_Variation_Tooltip", "Select a float depending on what tags are active.");
}

#undef LOCTEXT_NAMESPACE

