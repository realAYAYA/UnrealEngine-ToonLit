// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"

#include "CustomizableObjectNodePassThroughTexture.generated.h"


UCLASS(hidecategories = ("Texture2D"))
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodePassThroughTexture : public UCustomizableObjectNodeTextureBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Texture, Meta = (DisplayName = Texture))
	TObjectPtr<UTexture> PassThroughTexture = nullptr;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
};
