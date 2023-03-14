// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"

#include "Containers/UnrealString.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"

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
	return LOCTEXT("Texture_Transform_Tooltip", "Applies linear transform, Offset followed by Scale followed by Rotation, to the content of Base Texture. Samples outside the image get tiled.");
}

void UCustomizableObjectNodeTextureTransform::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


#undef LOCTEXT_NAMESPACE
