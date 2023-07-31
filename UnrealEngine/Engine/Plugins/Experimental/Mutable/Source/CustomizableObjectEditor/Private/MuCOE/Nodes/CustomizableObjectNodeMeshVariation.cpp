// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshVariation.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMeshVariation::UCustomizableObjectNodeMeshVariation()
	: Super()
{

}


void UCustomizableObjectNodeMeshVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged  )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeMeshVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(TEXT("Mesh")));
	OutputPin->bDefaultValueIsIgnored = true;

	for ( int VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		FString PinName = FString::Printf( TEXT("Variation %d"), VariationIndex);
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName), false);
		VariationPin->bDefaultValueIsIgnored = true;

		FString FriendlyName = FString::Printf(TEXT("Variation %d [%s]"), VariationIndex, *Variations[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::FromString(FriendlyName);
	}

	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(TEXT("Default")), false);
	DefaultVariation->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMeshVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Mesh_Variation", "Mesh Variation");
}


FLinearColor UCustomizableObjectNodeMeshVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshVariation::GetTooltipText() const
{
	return LOCTEXT("Mesh_Variation_Tooltip", "Select a mesh depending on what tags are active.");
}

#undef LOCTEXT_NAMESPACE

