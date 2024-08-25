// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeTextureVariation.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectTextureVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureVariation : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeTextureVariation();

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FCustomizableObjectTextureVariation> Variations;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	
	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* OutputPin() const
	{
		return FindPin(TEXT("Texture"));
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

