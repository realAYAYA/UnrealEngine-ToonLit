// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMaterialVariation.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectMaterialVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


UENUM(BlueprintType)
enum class ECustomizableObjectNodeMaterialVariationType : uint8
{
	Tag 		UMETA(DisplayName = "Tag"),
	State 		UMETA(DisplayName = "State"),
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialVariation : public UCustomizableObjectNodeMaterialBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMaterialVariation();
	
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ECustomizableObjectNodeMaterialVariationType Type = ECustomizableObjectNodeMaterialVariationType::Tag;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FCustomizableObjectMaterialVariation> Variations;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* OutputPin() const
	{
		return FindPin(TEXT("Material"));
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

