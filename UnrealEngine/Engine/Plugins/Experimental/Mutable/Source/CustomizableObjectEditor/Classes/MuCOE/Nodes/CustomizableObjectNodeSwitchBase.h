// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeSwitchBase.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UObject;
struct FFrame;


UCLASS(Abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeSwitchBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	
	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostPasteNode() override;

	// UCustomizableObjectNode interface
	virtual void PostBackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins) override;

	/** Get the output pin category. Override. */
	virtual FName GetCategory() const PURE_VIRTUAL(UCustomizableObjectNodeSwitchBase::GetCategory, return {}; );

	UEdGraphPin* OutputPin() const;

	UEdGraphPin* SwitchParameter() const;

	UEdGraphPin* GetElementPin(int32 Index) const
	{
		return FindPin(GetPinPrefix(Index));
	}

	int32 GetNumElements() const;

	/** Links the PostEditChangeProperty delegate */
	void LinkPostEditChangePropertyDelegate(const UEdGraphPin& Pin);

	/** Get the ouput pin name. Override. */
	virtual FString GetOutputPinName() const;

private:
	/** Get the pin prefix. Used for retrocompatibility. Override. */
	virtual FString GetPinPrefix() const;

protected:
	UPROPERTY()
	FEdGraphPinReference OutputPinReference;

private:
	/** Get the pin prefix with index. Used for retrocompatibility.*/
	FString GetPinPrefix(int32 Index) const;

	void ReloadEnumParam();

	/** Last NodeEnumParameter connected. Used to remove the callback once disconnected. */
	TWeakObjectPtr<UCustomizableObjectNode> LastNodeEnumParameterConnected;

	/** NodeEnumParameter property changed callback function. Reconstructs the node. */
	UFUNCTION()
	void EnumParameterPostEditChangeProperty(FPostEditChangePropertyDelegateParameters& Parameters);

	/** The node has to be reconstructed. PinConnectionListChanged(...) can not reconstruct the node, flag used to reconstruct the node on NodeConnectionListChanged(). */
	bool bMarkReconstruct = false;

	UPROPERTY()
	TArray<FString> ReloadingElementsNames;

	UPROPERTY()
	FEdGraphPinReference SwitchParameterPinReference;
};

