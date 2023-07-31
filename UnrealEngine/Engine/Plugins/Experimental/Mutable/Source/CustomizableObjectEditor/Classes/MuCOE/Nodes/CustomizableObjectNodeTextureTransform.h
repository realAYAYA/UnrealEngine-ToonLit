// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeTextureTransform.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UObject;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureTransform : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TittleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	virtual void Serialize(FArchive& Ar) override;
	
	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* GetBaseImagePin() const;

	UEdGraphPin* GetOffsetXPin() const
	{
		return FindPin(TEXT("Offset X"));
	}

	UEdGraphPin* GetOffsetYPin() const
	{
		return FindPin(TEXT("Offset Y"));
	}

	UEdGraphPin* GetScaleXPin() const
	{
		return FindPin(TEXT("Scale X"));
	}

	UEdGraphPin* GetScaleYPin() const
	{
		return FindPin(TEXT("Scale Y"));
	}

	UEdGraphPin* GetRotationPin() const
	{
		return FindPin(TEXT("Rotation"));
	}

private:
	UPROPERTY()
	FEdGraphPinReference BaseImagePinReference;
};
