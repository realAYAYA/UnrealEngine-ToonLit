// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "CustomizableObjectNodeFloatVariation.generated.h"

struct FCustomizableObjectFloatVariation;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeFloatVariation : public UCustomizableObjectNodeVariation
{
public:
	GENERATED_BODY()
	
	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectFloatVariation> Variations_DEPRECATED;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;
};

