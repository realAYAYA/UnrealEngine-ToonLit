// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


const UEdGraphPin* UCustomizableObjectNodeFloatParameter::GetDescriptionImagePin(int32 Index) const
{
	return DescriptionImagePinsReferences[Index].Get();
}


void UCustomizableObjectNodeFloatParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged && (PropertyThatChanged->GetName() == TEXT("DescriptionImage") || PropertyThatChanged->GetName() == TEXT("Name")) )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeFloatParameter::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		for (int32 Index = 0; Index < DescriptionImage.Num(); ++Index)
		{
			DescriptionImagePinsReferences.Add(FindPin(FString::Printf(TEXT("Description Image %d"), Index)));
		}
	}
}


void UCustomizableObjectNodeFloatParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName("Value"));
	ValuePin->bDefaultValueIsIgnored = true;

	DescriptionImagePinsReferences.Reset();
	
	for (int32 Index = 0; Index <  DescriptionImage.Num(); ++Index )
	{
		FString PinName = DescriptionImage[Index].Name.IsEmpty() ?
			FString::Printf(TEXT("Description Texture %d"), Index) :
			DescriptionImage[Index].Name;
		
		UEdGraphPin* DescriptionImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
		DescriptionImagePin->bDefaultValueIsIgnored = true;

		DescriptionImagePinsReferences.Add(DescriptionImagePin);
	}
}


bool UCustomizableObjectNodeFloatParameter::IsAffectedByLOD() const
{
	return false;
}


int32 UCustomizableObjectNodeFloatParameter::GetNumDescriptionImage() const
{
	return DescriptionImagePinsReferences.Num();
}


FText UCustomizableObjectNodeFloatParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Float_Parameter", "Float Parameter");
}


FLinearColor UCustomizableObjectNodeFloatParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Float);
}


void UCustomizableObjectNodeFloatParameter::PostPasteNode()
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	DescriptionImage.SetNum(InputPins.Num());
	const int32 MaxIndex = InputPins.Num();
	for (int32 i = 0; i < MaxIndex; ++i)
	{
		const UEdGraphPin* DescriptionImagePin = InputPins[i];
		if (DescriptionImagePin->LinkedTo.Num())
		{
			DescriptionImage[i].Name = Helper_GetPinName(InputPins[i]);
		}
	}
}


FText UCustomizableObjectNodeFloatParameter::GetTooltipText() const
{
	return LOCTEXT("Float_Parameter_Tooltip", "Expose a numeric parameter from the Customizable Object that can be modified at runtime.");
}

#undef LOCTEXT_NAMESPACE
