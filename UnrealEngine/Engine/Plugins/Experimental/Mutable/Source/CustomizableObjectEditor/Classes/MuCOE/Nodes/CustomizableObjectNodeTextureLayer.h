// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeTextureLayer.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UEdGraph;
class UObject;
struct FPropertyChangedEvent;


UENUM()
enum ECustomizableObjectTextureLayerEffect
{
	COTLE_MODULATE			= 0 UMETA(DisplayName = "MODULATE"), 
	COTLE_MULTIPLY				UMETA(DisplayName = "MULTIPLY"), 
	COTLE_SOFTLIGHT				UMETA(DisplayName = "SOFTLIGHT"),
	COTLE_HARDLIGHT				UMETA(DisplayName = "HARDLIGHT"),
	COTLE_DODGE					UMETA(DisplayName = "DODGE"),
	COTLE_BURN					UMETA(DisplayName = "BURN"),
	COTLE_SCREEN				UMETA(DisplayName = "SCREEN"),
	COTLE_OVERLAY				UMETA(DisplayName = "OVERLAY"),
	COTLE_ALPHA_OVERLAY			UMETA(DisplayName = "ALPHA_OVERLAY"),
	COTLE_NORMAL_COMBINE		UMETA(DisplayName = "COMBINE")
};


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectTextureLayer
{
	GENERATED_USTRUCT_BODY()

	FCustomizableObjectTextureLayer()
	{
		Effect = COTLE_SOFTLIGHT;
	}

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TEnumAsByte<enum ECustomizableObjectTextureLayerEffect> Effect;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureLayer : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeTextureLayer();

	/**  */
	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray<FCustomizableObjectTextureLayer> Layers;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;
	
	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	void PrepareForCopying() override;
	void PostPasteNode() override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;
		
	UEdGraphPin* BasePin() const
	{
		return FindPin(TEXT("Base"));
	}

	UEdGraphPin* OutputPin() const;

	UEdGraphPin* LayerPin(int32 Index) const
	{
		FString PinName = FString::Printf(TEXT("Layer %d "), Index);
		return FindPin(PinName);
	}

	UEdGraphPin* MaskPin(int32 Index) const
	{
		FString PinName = FString::Printf(TEXT("Mask %d "), Index);
		return FindPin(PinName);
	}

	int32 GetNumLayers() const
	{
		int32 Count = 0;

		for (UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if (Pin->GetName().StartsWith(TEXT("Layer ")))
			{
				Count++;
			}
		}

		return Count;
	}
protected:
	// This struct will store the information of each textue layer node when its destroyed, to be able to recover the UCustomizableObjectNodeTextureLayer::Layers
	// data in a post paste operation. Otherwise, this information is lost.
	struct TextureLayerLayersData
	{
		FString Name; // Texture layer node name
		TArray<FCustomizableObjectTextureLayer> Layers;
		UEdGraph* Graph; // Graph that contained this node
	};
	static int32 GetIndexInArrayDestroyedNodes(UCustomizableObjectNodeTextureLayer* Node);
	static TArray<TextureLayerLayersData> ArrayDestroyedNodes;

private:
	UPROPERTY()
	FEdGraphPinReference OutputPinReference;
};

