// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMaterialVariation::UCustomizableObjectNodeMaterialVariation()
	: Super()
{

}


void UCustomizableObjectNodeMaterialVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged 
		//&& PropertyThatChanged->GetName() == TEXT("Variations")
		)
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeMaterialVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Material");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	for ( int VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		PinName = FString::Printf( TEXT("Variation %d"), VariationIndex);
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(*PinName), true);
		VariationPin->bDefaultValueIsIgnored = true;

		FString FriendlyName = FString::Printf(TEXT("Variation %d [%s]"), VariationIndex, *Variations[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::FromString(FriendlyName);
	}

	PinName = TEXT("Default");
	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(*PinName), true);
	DefaultVariation->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMaterialVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Material_Variation", "Material Variation");
}


FLinearColor UCustomizableObjectNodeMaterialVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


FText UCustomizableObjectNodeMaterialVariation::GetTooltipText() const
{
	return LOCTEXT("Material_Variation_Tooltip", "Changes the materials given depending on what tags or state is active.");
}

#undef LOCTEXT_NAMESPACE

