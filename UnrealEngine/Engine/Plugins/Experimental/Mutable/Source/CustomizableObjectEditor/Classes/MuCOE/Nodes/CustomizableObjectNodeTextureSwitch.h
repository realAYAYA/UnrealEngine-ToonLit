// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"

#include "CustomizableObjectNodeTextureSwitch.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()
	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	
	// UCustomizableObjectNodeSwitchBase interface
	virtual FName GetCategory() const override;
};
