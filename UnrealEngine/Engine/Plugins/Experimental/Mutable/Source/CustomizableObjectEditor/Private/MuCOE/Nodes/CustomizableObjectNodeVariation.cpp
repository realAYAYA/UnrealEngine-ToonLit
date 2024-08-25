// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	const FName Category = GetCategory();
	const bool bIsInputPinArray = IsInputPinArray();

	{
		const FName PinName = FName(UEdGraphSchema_CustomizableObject::GetPinCategoryName(GetCategory()).ToString());
		CustomCreatePin(EGPD_Output, Category, PinName);
	}
	
	VariationsPins.SetNum(VariationsData.Num());
	for (int32 VariationIndex = VariationsData.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		const FName PinName = FName(FString::Printf( TEXT("Variation %d"), VariationIndex));
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Category, PinName, bIsInputPinArray);

		VariationPin->PinFriendlyName = FText::Format(LOCTEXT("Variation_Pin_FriendlyName", "Variation {0} [{1}]"), VariationIndex, FText::FromString(*VariationsData[VariationIndex].Tag));
		
		VariationsPins[VariationIndex] = VariationPin;
	}

	CustomCreatePin(EGPD_Input, Category, FName(TEXT("Default")), bIsInputPinArray);
}


bool UCustomizableObjectNodeVariation::IsInputPinArray() const
{
	return false;
}


int32 UCustomizableObjectNodeVariation::GetNumVariations() const
{
	return VariationsData.Num();
}


const FCustomizableObjectVariation& UCustomizableObjectNodeVariation::GetVariation(int32 Index) const
{
	return VariationsData[Index];
}


UEdGraphPin* UCustomizableObjectNodeVariation::DefaultPin() const
{
	return FindPin(TEXT("Default"));
}


UEdGraphPin* UCustomizableObjectNodeVariation::VariationPin(int32 Index) const
{
	return VariationsPins[Index].Get();
}


FText UCustomizableObjectNodeVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("Variation_Node_Title", "{0} Variation"), UEdGraphSchema_CustomizableObject::GetPinCategoryName(GetCategory()));
}


FLinearColor UCustomizableObjectNodeVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(GetCategory());
}


FText UCustomizableObjectNodeVariation::GetTooltipText() const
{
	return FText::Format(LOCTEXT("Variation_Tooltip", "Select a {0} depending on what tags are active."), UEdGraphSchema_CustomizableObject::GetPinCategoryName(GetCategory()));
}


void UCustomizableObjectNodeVariation::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeVariationSerializationIssue)
	{
		ReconstructNode();
	}
}


#undef LOCTEXT_NAMESPACE

