// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureInterpolate.h"

#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeTextureInterpolate::UCustomizableObjectNodeTextureInterpolate()
	: Super()
{
	NumTargets = 2;
}


void UCustomizableObjectNodeTextureInterpolate::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumTargets") )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeTextureInterpolate::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;
	
	PinName = TEXT("Factor");
	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	FactorPin->bDefaultValueIsIgnored = true;

	for ( int LayerIndex = 0; LayerIndex < NumTargets; ++LayerIndex )
	{
		PinName = FString::Printf(TEXT("Target %d"), LayerIndex);
		UEdGraphPin* LayerPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
		LayerPin->bDefaultValueIsIgnored = true;
	}
}


FText UCustomizableObjectNodeTextureInterpolate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Interpolate", "Texture Interpolate");
}


FLinearColor UCustomizableObjectNodeTextureInterpolate::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureInterpolate::GetTooltipText() const
{
	return LOCTEXT("Texture_Interpolate_Tooltip", "Changes between textures gradually, allowing to fully apply one texture, the next one, or any proportion between them.");
}

#undef LOCTEXT_NAMESPACE

