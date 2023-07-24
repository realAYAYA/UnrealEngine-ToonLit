// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeTextureProject.generated.h"

class UTexture2D;
namespace ENodeTitleType { enum Type : int; }

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
	uint8 Layout = 0;
	
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bEnableAngleFadeOutForRGB = true;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bEnableAngleFadeOutForAlpha = true;

	/** Set the width of the Texture. If the reference texture is not null, it overrides the Size X property */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	uint32 TextureSizeX = 0;

	/** Set the height of the Texture. If the reference texture is not null, it overrides the Size Y property */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	uint32 TextureSizeY = 0;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, meta=( ClampMin = 1))
	uint32 Textures = 0;

	/** Reference Texture used to decide the texture properties of the mutable-generated textures
	* connected to this material (e.g. LODBias, Size X,...). If null, mutable default texture properties will be applied. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<UTexture2D> ReferenceTexture = nullptr;

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

