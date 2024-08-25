// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeVariation.generated.h"

namespace ENodeTitleType
{
	enum Type : int;
}

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectVariation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Tag;
};


UCLASS(Abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeVariation : public UCustomizableObjectNode
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FCustomizableObjectVariation> VariationsData; // The variable name can not be Variations due issues with the on to UObject Serialization system

private:
	UPROPERTY()
	TArray<FEdGraphPinReference> VariationsPins;
	
public:
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	
	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Own interface
	/** Return the pin category of this node. */
	virtual FName GetCategory() const PURE_VIRTUAL(UCustomizableObjectNodeVariation::GetCategory, return {}; );

	/** Return true if all inputs pins should be array. */
	virtual bool IsInputPinArray() const;

	/** Return the number of variations (input pins excluding the Default Pin). */
	int32 GetNumVariations() const;

	/** Get the variation at the given index. */
	const FCustomizableObjectVariation& GetVariation(int32 Index) const;

	/** Get the Default Input Pin. */
	UEdGraphPin* DefaultPin() const;

	/** Get the Variation Input Pin. */
	UEdGraphPin* VariationPin(int32 Index) const;
};

