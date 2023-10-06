// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeTextureTransform.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UObject;

UENUM()
enum class ETextureTransformAddressMode
{
	Wrap,
	ClampToEdge,
	ClampToBlack
};

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureTransform : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ETextureTransformAddressMode AddressMode = ETextureTransformAddressMode::Wrap;

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
