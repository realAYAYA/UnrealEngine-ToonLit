// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeFloatSwitch.generated.h"

class FArchive;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeFloatSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	
	// UCustomizableObjectNodeSwitchBase interface
	FString GetOutputPinName() const override;

	FName GetCategory() const override;

	// Own interface
	FString GetPinPrefix() const;
};
