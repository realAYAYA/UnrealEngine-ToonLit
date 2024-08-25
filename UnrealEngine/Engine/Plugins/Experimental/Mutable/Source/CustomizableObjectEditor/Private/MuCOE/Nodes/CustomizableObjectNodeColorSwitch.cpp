// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeColorSwitch::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Color"));	
	}
}


FName UCustomizableObjectNodeColorSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Color;
}


#undef LOCTEXT_NAMESPACE

