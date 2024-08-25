// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeMeshClipDeform.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;

UENUM()
enum class EShapeBindingMethod : uint32
{
	ClosestProject = 0,
	ClosestToSurface = 1,
	NormalProject = 2
};

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshClipDeform : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshClipDeform)
	TArray<FString> Tags;
	
	/** Policy to use tags in case more than one is added. */
	UPROPERTY(EditAnywhere, Category = MeshClipDeform)
	EMutableMultipleTagPolicy MultipleTagPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

	UPROPERTY(EditAnywhere, Category = MeshClipDeform)
	EShapeBindingMethod BindingMethod;
	
	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual UEdGraphPin* OutputPin() const override;

	// Own interface
	UEdGraphPin* ClipShapePin() const;
};

