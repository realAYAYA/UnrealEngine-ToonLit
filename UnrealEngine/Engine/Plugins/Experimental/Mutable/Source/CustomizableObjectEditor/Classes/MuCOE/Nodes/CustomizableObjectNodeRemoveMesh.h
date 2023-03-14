// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeRemoveMesh.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeRemoveMesh : public UCustomizableObjectNodeEditMaterialBase
{
public:
	GENERATED_BODY()

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin * Pin);

	UEdGraphPin* RemoveMeshPin() const
	{
		return FindPin(TEXT("Remove Mesh"));
	}

	bool IsSingleOutputNode() const override;
};

