// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeMeshVariation.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectMeshVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshVariation : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshVariation();

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FCustomizableObjectMeshVariation> Variations;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* OutputPin() const
	{
		return FindPin(TEXT("Mesh"));
	}

	UEdGraphPin* DefaultPin() const
	{
		return FindPin(TEXT("Default"));
	}

	int32 GetNumVariations() const
	{
		return Variations.Num();
	}

	UEdGraphPin* VariationPin(int Index) const
	{
		return FindPin(FString::Printf(TEXT("Variation %d"),Index));
	}
};

