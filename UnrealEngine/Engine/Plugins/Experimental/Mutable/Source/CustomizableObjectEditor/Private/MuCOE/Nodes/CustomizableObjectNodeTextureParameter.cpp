// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureParameter.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Value");
	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeTextureParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture Parameter", "Texture Parameter");
}


FLinearColor UCustomizableObjectNodeTextureParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureParameter::GetTooltipText() const
{
	return LOCTEXT("Texture_Parameter_Tooltip", "Expose a runtime modifiable texture parameter from the Customizable Object.");
}


void UCustomizableObjectNodeTextureParameter::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeTextureParameterDefaultToReferenceValue)
	{
		ReferenceValue = DefaultValue;
		DefaultValue = {};
	}	
}

#undef LOCTEXT_NAMESPACE

