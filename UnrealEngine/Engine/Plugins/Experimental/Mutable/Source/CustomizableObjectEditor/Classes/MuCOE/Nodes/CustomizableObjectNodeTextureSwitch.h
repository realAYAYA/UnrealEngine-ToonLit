// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSwitchBase.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeTextureSwitch.generated.h"

class FArchive;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureSwitch : public UCustomizableObjectNodeSwitchBase
{
public:
	GENERATED_BODY()

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;

	// UCustomizableObjectNodeSwitchBase interface
	FString GetOutputPinName() const override;

	FName GetCategory() const override;

	FString GetPinPrefix() const;
};
