// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorVariation.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeColorVariation::UCustomizableObjectNodeColorVariation()
	: Super()
{

}


void UCustomizableObjectNodeColorVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged  )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeColorVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString RemapIndex = Helper_GetPinName(OutputPin());

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName(TEXT("Color")));
	OutputPin->bDefaultValueIsIgnored = true;

	for ( int VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		FString PinName = FString::Printf( TEXT("Variation %d"), VariationIndex);
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Schema->PC_Color, FName(*PinName), false);
		VariationPin->bDefaultValueIsIgnored = true;

		FString FriendlyName = FString::Printf(TEXT("Variation %d [%s]"), VariationIndex, *Variations[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::FromString(FriendlyName);
	}

	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, Schema->PC_Color, FName(TEXT("Default")), false);
	DefaultVariation->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeColorVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Color_Variation", "Color Variation");
}


FLinearColor UCustomizableObjectNodeColorVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeColorVariation::GetTooltipText() const
{
	return LOCTEXT("Color_Variation_Tooltip", "Select a color depending on what tags are active.");
}

#undef LOCTEXT_NAMESPACE

