// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorParameter.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeColorParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName("Value"));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeColorParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Color Parameter", "Color Parameter");
}


FLinearColor UCustomizableObjectNodeColorParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeColorParameter::GetTooltipText() const
{
	return LOCTEXT("Color_Parameter_Tooltip", "Expose a runtime modifiable color parameter from the Customizable Object.");
}


#undef LOCTEXT_NAMESPACE

