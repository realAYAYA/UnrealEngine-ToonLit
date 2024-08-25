// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "CustomizableObjectNodeMeshVariation.generated.h"

struct FCustomizableObjectMeshVariation;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshVariation : public UCustomizableObjectNodeVariation
{
public:
	GENERATED_BODY()

	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectMeshVariation> Variations_DEPRECATED;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;
};

