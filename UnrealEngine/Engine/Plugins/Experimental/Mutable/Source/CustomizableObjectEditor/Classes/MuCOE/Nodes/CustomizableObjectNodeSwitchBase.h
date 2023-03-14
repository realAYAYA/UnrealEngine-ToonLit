// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "CustomizableObjectNodeSwitchBase.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UObject;
struct FFrame;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeSwitchBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()


	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	
	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostPasteNode() override;


	// UCustomizableObjectNode interface
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins) override;

	/** Get the output pin catergory. Override. */
	virtual FName GetCategory() const { return FName(); };

	UEdGraphPin* OutputPin() const;

	UEdGraphPin* SwitchParameter() const;

	UEdGraphPin* GetElementPin(int32 Index) const
	{
		return FindPin(GetPinPrefix(Index));
	}

	int32 GetNumElements() const;

	/** Links the PostEditChangeProperty delegate */
	void LinkPostEditChangePropertyDelegate(const UEdGraphPin& Pin);

protected:
	/** Get the pin prefix. Used for retrocompatibility. Override. */
	virtual FString GetPinPrefix() const { return FString(); };

	/** Get the ouput pin name. Override. */
	virtual FString GetOutputPinName() const { return FString(); };

	UPROPERTY()
	FEdGraphPinReference OutputPinReference;

private:
	/** Get the pin prefix with index. Used for retrocompatibility.*/
	FString GetPinPrefix(int32 Index) const;

	void ReloadEnumParam();

	/** Last NodeEnumParameter connected. Used to remove the callback once desconnected. */
	TWeakObjectPtr<UCustomizableObjectNode> LastNodeEnumParameterConnected;

	/** NodeEnumParameter property changed callback function. Reconstructs the node. */
	UFUNCTION()
	void EnumParameterPostEditChangeProperty(FPostEditChangePropertyDelegateParameters& Parameters);

	/** The node has to be reconstructed. PinConnectionListChanged(...) can not reconstruct the node, flag used to reconstruct the node on NodeConnectionListChanged(). */
	bool bMarkReconstruct = false;

	UPROPERTY()
	TArray<FString> ReloadingElementsNames;

private:
	UPROPERTY()
	FEdGraphPinReference SwitchParameterPinReference;
};

