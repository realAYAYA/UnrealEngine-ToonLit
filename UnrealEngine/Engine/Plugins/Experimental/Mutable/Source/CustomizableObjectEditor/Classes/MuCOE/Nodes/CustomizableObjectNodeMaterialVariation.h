// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "CustomizableObjectNodeMaterialVariation.generated.h"

struct FCustomizableObjectMaterialVariation;


UENUM(BlueprintType)
enum class ECustomizableObjectNodeMaterialVariationType : uint8
{
	Tag 		UMETA(DisplayName = "Tag"),
	State 		UMETA(DisplayName = "State"),
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialVariation : public UCustomizableObjectNodeVariation
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ECustomizableObjectNodeMaterialVariationType Type = ECustomizableObjectNodeMaterialVariationType::Tag;

private:
	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectMaterialVariation> Variations_DEPRECATED;

public:
	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual bool IsSingleOutputNode() const override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;
	virtual bool IsInputPinArray() const override;
};

