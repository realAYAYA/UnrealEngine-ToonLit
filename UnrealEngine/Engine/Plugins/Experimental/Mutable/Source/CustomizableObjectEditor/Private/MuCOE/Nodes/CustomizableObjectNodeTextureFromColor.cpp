// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromColor.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTextureFromColor::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Color");
	UEdGraphPin* RPin = CustomCreatePin(EGPD_Input, Schema->PC_Color, FName(*PinName));
	RPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeTextureFromColor::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_From_Color", "Texture From Color");
}


FLinearColor UCustomizableObjectNodeTextureFromColor::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureFromColor::GetTooltipText() const
{
	return LOCTEXT("Texture_From_Color_Tooltip", "Creates a flat color texture from the color provided.");
}

#undef LOCTEXT_NAMESPACE
