// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorSwitch.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeColorSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Color"));	
	}
}


FString UCustomizableObjectNodeColorSwitch::GetOutputPinName() const
{
	return TEXT("Color");
}
	

FName UCustomizableObjectNodeColorSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Color;
}


FString UCustomizableObjectNodeColorSwitch::GetPinPrefix() const
{
	return TEXT("Color ");
}


#undef LOCTEXT_NAMESPACE

