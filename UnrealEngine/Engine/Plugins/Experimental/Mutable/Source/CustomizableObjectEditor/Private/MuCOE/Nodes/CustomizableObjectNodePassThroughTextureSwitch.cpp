// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureSwitch.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FString UCustomizableObjectNodePassThroughTextureSwitch::GetOutputPinName() const
{
	return TEXT("PassThrough Texture");
}


FName UCustomizableObjectNodePassThroughTextureSwitch::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_PassThroughImage;
}


FString UCustomizableObjectNodePassThroughTextureSwitch::GetPinPrefix() const
{
	return TEXT("PassThrough Texture ");
}


#undef LOCTEXT_NAMESPACE

