// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMorphMaterial.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMorphMaterial : public UCustomizableObjectNodeEditMaterialBase
{
public:

	GENERATED_BODY()
	
	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	bool IsNodeOutDatedAndNeedsRefresh() override;
	FString GetRefreshMessage() const override;

	UPROPERTY(Category=CustomizableObject, EditAnywhere)
	FString MorphTargetName;

	bool IsSingleOutputNode() const override;

	UEdGraphPin* FactorPin() const
	{
		return FindPin(TEXT("Factor"));
	}
};

