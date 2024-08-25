// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"

#include "CustomizableObjectNodePassThroughTextureSwitch.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodePassThroughTextureSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()
	
	// UCustomizableObjectNodeSwitchBase interface
	virtual FName GetCategory() const override;
};
