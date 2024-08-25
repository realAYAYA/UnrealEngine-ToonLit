// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMeshSwitch::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Mesh"));	
	}
}


FName UCustomizableObjectNodeMeshSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Mesh;
}


#undef LOCTEXT_NAMESPACE

