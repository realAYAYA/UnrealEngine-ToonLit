// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeTextureInvert.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UObject;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureInvert : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()
	
	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TittleType)const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	
	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void BackwardsCompatibleFixup() override;

	// Own interface
	UEdGraphPin* GetBaseImagePin() const;

private:
	UPROPERTY()
	FEdGraphPinReference BaseImagePinReference;
};
