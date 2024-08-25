// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeModifierClipWithUVMask.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeModifierClipWithUVMask : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	/** Materials in all other objects that activate this tags will be clipped with this UV mask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClip)
	TArray<FString> Tags;

	/** Policy to use tags in case more than one is added. */
	UPROPERTY(EditAnywhere, Category = MeshToClip)
	EMutableMultipleTagPolicy MultipleTagPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

	/** UV channel index that will be used to get the UVs to apply the clipping mask to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClip)
	int32 UVChannelForMask = 0;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNodeMaterialBase interface
	virtual UEdGraphPin* OutputPin() const override;

	// Own interface
	UEdGraphPin* ClipMaskPin() const;
};

