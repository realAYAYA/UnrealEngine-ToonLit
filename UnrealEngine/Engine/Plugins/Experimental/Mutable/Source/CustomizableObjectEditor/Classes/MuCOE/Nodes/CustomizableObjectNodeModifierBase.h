// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeModifierBase.generated.h"

class UObject;


UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierBase : public UCustomizableObjectNodeMaterialBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeModifierBase();

};

