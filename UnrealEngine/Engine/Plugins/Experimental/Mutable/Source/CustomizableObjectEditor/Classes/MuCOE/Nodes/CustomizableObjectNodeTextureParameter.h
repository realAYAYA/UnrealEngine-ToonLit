// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeTextureParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UObject;
class UTexture2D;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureParameter : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	/** Default value of the parameter. */
	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TObjectPtr<UTexture2D> DefaultValue;

	/** Reference Texture where this parameter copies some properties from. */
	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TObjectPtr<UTexture2D> ReferenceValue;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString ParameterName = "Default Name";

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	/** Set the width of the Texture when there is no texture reference.*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 TextureSizeX = 0;

	/** Set the height of the Texture when there is no texture reference.*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 TextureSizeY = 0;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsAffectedByLOD() const override { return false; }
};

