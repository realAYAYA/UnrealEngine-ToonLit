// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeTextureTransform::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Base Texture");
	UEdGraphPin* ImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));

	BaseImagePinReference = FEdGraphPinReference(ImagePin);
	
	PinName = TEXT("Offset X");
	UEdGraphPin* OffsetXPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	OffsetXPin->bDefaultValueIsIgnored = true;
	
	PinName = TEXT("Offset Y");
	UEdGraphPin* OffsetYPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	OffsetYPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Scale X");
	UEdGraphPin* ScaleXPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	ScaleXPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Scale Y");
	UEdGraphPin* ScaleYPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	ScaleYPin->bDefaultValueIsIgnored = true;
	
	PinName = TEXT("Rotation");
	UEdGraphPin* RotationPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	RotationPin->bDefaultValueIsIgnored = true;
}

UEdGraphPin* UCustomizableObjectNodeTextureTransform::GetBaseImagePin() const
{
	return BaseImagePinReference.Get();
}

FText UCustomizableObjectNodeTextureTransform::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Transform", "Texture Transform");
}

FLinearColor UCustomizableObjectNodeTextureTransform::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}

FText UCustomizableObjectNodeTextureTransform::GetTooltipText() const
{
	return LOCTEXT("Texture_Transform_Tooltip", 
			"Applies a linear transform, rotation and scale around the center of the image plus translation, "
			"to the content of Base Texture. Rotation is in the range [0 .. 1], 1 being full rotation, offset " 
			"and scale are in output image normalized coordinates with origin at the center of the image. " 
			"If Keep Aspect Ratio is set, an scaling factor preserving aspect ratio will be used as identity.");
}

void UCustomizableObjectNodeTextureTransform::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


#undef LOCTEXT_NAMESPACE
