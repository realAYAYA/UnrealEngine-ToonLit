// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeCurve.generated.h"

class FArchive;
class UCurveBase;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeCurve : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeCurve();

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* InputPin() const;
	UEdGraphPin* CurvePins(int32 Index) const;
	int32 GetNumCurvePins() const;

	UPROPERTY(EditAnywhere, Category = Curve)
	TObjectPtr<UCurveBase> CurveAsset;

	// UCustomizableObjectNode interface
	virtual bool IsAffectedByLOD() const override { return false; }

	bool ProvidesCustomPinRelevancyTest() const override { return true; }
	bool IsPinRelevant(const UEdGraphPin* Pin) const override;

	/** Override the default behaivour of remap pins. Use remap pins by position by default. */
	UCustomizableObjectNodeRemapPins* CreateRemapPinsDefault() const override;
};

