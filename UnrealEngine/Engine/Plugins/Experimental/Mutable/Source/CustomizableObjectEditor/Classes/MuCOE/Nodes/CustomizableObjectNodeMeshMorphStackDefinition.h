// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeMeshMorphStackDefinition.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshMorphStackDefinition : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TittleType)const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsNodeOutDatedAndNeedsRefresh() override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Own interface
	/** Returns the mesh pin of the node. */
	UEdGraphPin* GetMeshPin() const;

	/** Returns the stack pin of the node. */
	UEdGraphPin* GetStackPin() const;

	/** Returns the morph pin at Index. */
	UEdGraphPin* GetMorphPin(int32 Index) const;
	
	/** Returns the index of the next connected stack node. Returns -1 if there is none. */
	int32 NextConnectedPin(int32 Index, TArray<FString> AvailableMorphs)const;

private:
	/** Fills the list with all the morphs. */
	void UpdateMorphList();

	/** List with all the morphs of the linked skeletal mesh. */
	UPROPERTY()
	TArray<FString> MorphNames;
};
