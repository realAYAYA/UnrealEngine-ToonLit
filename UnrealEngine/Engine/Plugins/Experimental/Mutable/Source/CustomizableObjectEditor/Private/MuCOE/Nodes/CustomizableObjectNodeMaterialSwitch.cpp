// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FName UCustomizableObjectNodeMaterialSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Material;
}


#undef LOCTEXT_NAMESPACE

