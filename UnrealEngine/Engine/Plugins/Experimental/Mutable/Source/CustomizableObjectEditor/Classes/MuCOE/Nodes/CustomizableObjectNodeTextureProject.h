// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeTextureProject.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FEdGraphPinReference;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureProject : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeTextureProject();

	// Layout to use for the generated images.
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 Layout = 0;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 TextureSizeX = 0; 

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 TextureSizeY = 0;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 Textures = 0;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;
	
	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* MeshPin() const
	{
		return FindPin(TEXT("Mesh"));
	}

	UEdGraphPin* MeshMaskPin() const
	{
		return FindPin(TEXT("Mesh Mask"));
	}

	UEdGraphPin* AngleFadeStartPin() const
	{
		return FindPin(TEXT("Fade Start Angle"));
	}

	UEdGraphPin* AngleFadeEndPin() const
	{
		return FindPin(TEXT("Fade End Angle"));
	}

	UEdGraphPin* ProjectorPin() const
	{
		return FindPin(TEXT("Projector"));
	}

	UEdGraphPin* TexturePins(int32 Index) const;

	UEdGraphPin* OutputPins(int32 Index) const;

	int32 GetNumTextures() const;

	int32 GetNumOutputs() const;

private:
	UPROPERTY()
	TArray<FEdGraphPinReference> TexturePinsReferences;

	UPROPERTY()
	TArray<FEdGraphPinReference> OutputPinsReferences;
};

