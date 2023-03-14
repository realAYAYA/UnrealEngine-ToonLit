// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorFromFloats.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeColorFromFloats::UCustomizableObjectNodeColorFromFloats()
	: Super()
{

}


void UCustomizableObjectNodeColorFromFloats::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeColorFromFloats::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* RPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("R"));
	RPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* GPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("G"));
	GPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* BPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("B"));
	BPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* APin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("A"));
	APin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ColorPin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName("Color"));
	ColorPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeColorFromFloats::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Color_From_Floats", "Color From Floats");
}


FLinearColor UCustomizableObjectNodeColorFromFloats::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeColorFromFloats::GetTooltipText() const
{
	return LOCTEXT("Color_From_Floats_Tooltip", "Defines a color from its four RGBA components.");
}


#undef LOCTEXT_NAMESPACE

