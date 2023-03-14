// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatSwitch.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeFloatSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Float"));	
	}
}


FString UCustomizableObjectNodeFloatSwitch::GetOutputPinName() const
{
	return TEXT("Float");
}


FName UCustomizableObjectNodeFloatSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Float;
}


FString UCustomizableObjectNodeFloatSwitch::GetPinPrefix() const
{
	return TEXT("Float ");
}


#undef LOCTEXT_NAMESPACE

