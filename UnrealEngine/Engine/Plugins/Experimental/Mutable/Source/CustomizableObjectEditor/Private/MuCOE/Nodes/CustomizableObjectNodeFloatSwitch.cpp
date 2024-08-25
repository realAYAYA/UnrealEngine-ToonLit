// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeFloatSwitch::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Float"));	
	}
}


FName UCustomizableObjectNodeFloatSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Float;
}


#undef LOCTEXT_NAMESPACE

