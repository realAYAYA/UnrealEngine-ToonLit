// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/ICustomizableObjectExtensionNode.h"

#include "CustomizableObjectNodeExtensionDataVariation.generated.h"

USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectExtensionDataVariation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "CustomizableObject")
	FString Tag;
};

UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeExtensionDataVariation
	: public UCustomizableObjectNode
	, public ICustomizableObjectExtensionNode
{
	GENERATED_BODY()

public:

	//~Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~End UObject interface

	//~Begin UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	//~End UEdGraphNode interface

	//~Begin UCustomizableObjectNode interface
	virtual bool IsAffectedByLOD() const override;
	virtual bool ShouldAddToContextMenu(FText& OutCategory) const override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* InRemapPins) override;
	//~End UCustomizableObjectNode interface

	//~Begin ICustomizableObjectExtensionNode interface
	virtual mu::NodeExtensionDataPtr GenerateMutableNode(FExtensionDataCompilerInterface& InCompilerInterface) const override;
	//~End ICustomizableObjectExtensionNode interface

	virtual FName GetCategory() const PURE_VIRTUAL(GetCategory(), return FName();)
	virtual FName GetOutputPinName() const PURE_VIRTUAL(GetOutputPinName(), return FName();)

	FName GetDefaultPinName() const;
	FName GetVariationPinName(int32 InIndex) const;
	class UEdGraphPin* GetDefaultPin() const;
	class UEdGraphPin* GetVariationPin(int32 InIndex) const;

	int32 GetNumVariations() const;

public:

	UPROPERTY(EditAnywhere, Category = "CustomizableObject")
	TArray<FCustomizableObjectExtensionDataVariation> Variations;

};