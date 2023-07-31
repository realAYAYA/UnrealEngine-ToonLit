// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshMorphStackDefinition.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshMorphStackDefinition : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshMorphStackDefinition();

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TittleType)const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	bool IsNodeOutDatedAndNeedsRefresh() override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Fills the list with all the morphs
	void UpdateMorphList();

	// Returns the mesh pin of the node
	UEdGraphPin* GetMeshPin() const
	{
		return FindPin(TEXT("Mesh"), EGPD_Input);
	}

	// Returns the stack pin of the node
	UEdGraphPin* GetStackPin() const
	{
		return FindPin(TEXT("Stack"), EGPD_Output);
	}

	// Returns the morph pin at Index
	UEdGraphPin* GetMorphPin(int32 Index) const;
	
	// Returns the index of the next connected stack node. Returns -1 if there is none.
	int32 NextConnectedPin(int32 Index, TArray<FString> AvailableMorphs)const;

public:

	// List with all the morphs of the linked skeletal mesh
	UPROPERTY()
	TArray<FString> MorphNames;

};
