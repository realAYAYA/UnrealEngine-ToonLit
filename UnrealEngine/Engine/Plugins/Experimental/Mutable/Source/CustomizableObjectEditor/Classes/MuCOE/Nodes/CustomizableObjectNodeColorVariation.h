// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeColorVariation.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectColorVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeColorVariation : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeColorVariation();

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FCustomizableObjectColorVariation> Variations;

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
		return FindPin(TEXT("Color"));
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

private:
	TMap<UEdGraphPin*, UEdGraphPin*> RemapPinsMap;
};

