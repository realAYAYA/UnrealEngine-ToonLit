// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"

#include "CustomizableObjectNodeMaterialSwitch.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()
	
	// UCustomizableObjectNodeSwitchBase interface
	virtual FName GetCategory() const override;
};

