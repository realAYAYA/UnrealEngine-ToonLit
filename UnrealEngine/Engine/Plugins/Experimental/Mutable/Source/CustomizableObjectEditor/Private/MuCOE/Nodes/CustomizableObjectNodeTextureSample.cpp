// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureSample.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeTextureSample::UCustomizableObjectNodeTextureSample()
	: Super()
{

}


void UCustomizableObjectNodeTextureSample::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	//if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs") )
	{
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeTextureSample::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Color");
	UEdGraphPin* ColorPin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName(*PinName));
	ColorPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Texture");
	UEdGraphPin* TexturePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
	TexturePin->bDefaultValueIsIgnored = true;

	PinName = TEXT("X");
	UEdGraphPin* XPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	XPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Y");
	UEdGraphPin* YPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	YPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeTextureSample::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Sample_Texture", "Sample Texture");
}


FLinearColor UCustomizableObjectNodeTextureSample::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeTextureSample::GetTooltipText() const
{
	return LOCTEXT("Texture_Sample_Tooltip","Texture Sample");
}

#undef LOCTEXT_NAMESPACE

