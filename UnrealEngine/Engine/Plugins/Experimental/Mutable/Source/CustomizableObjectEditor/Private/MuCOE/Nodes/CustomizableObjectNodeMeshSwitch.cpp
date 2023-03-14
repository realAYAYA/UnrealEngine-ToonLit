// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshSwitch.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMeshSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::BugPinsSwitch)
	{
		OutputPinReference = FindPin(TEXT("Mesh"));	
	}
}


FString UCustomizableObjectNodeMeshSwitch::GetOutputPinName() const
{
	return TEXT("Mesh");
}


FName UCustomizableObjectNodeMeshSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Mesh;
}


FString UCustomizableObjectNodeMeshSwitch::GetPinPrefix() const
{
	return TEXT("Mesh ");
}


#undef LOCTEXT_NAMESPACE

