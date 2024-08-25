// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"

#include "CustomizableObjectNodeFloatSwitch.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeFloatSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;

	// UCustomizableObjectNodeSwitchBase interface
	virtual FName GetCategory() const override;
};
