// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "CustomizableObjectNodeColorVariation.generated.h"

struct FCustomizableObjectColorVariation;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeColorVariation : public UCustomizableObjectNodeVariation
{
public:
	GENERATED_BODY()

	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectColorVariation> Variations_DEPRECATED;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;
};

