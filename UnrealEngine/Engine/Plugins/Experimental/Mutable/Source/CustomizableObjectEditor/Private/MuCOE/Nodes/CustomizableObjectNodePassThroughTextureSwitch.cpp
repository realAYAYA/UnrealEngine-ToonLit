// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FName UCustomizableObjectNodePassThroughTextureSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_PassThroughImage;
}


#undef LOCTEXT_NAMESPACE

