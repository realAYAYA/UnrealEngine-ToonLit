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

#include "CustomizableObjectNodeMeshMorphStackApplication.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshMorphStackApplication : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshMorphStackApplication();

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TittleType)const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins) override;
	bool IsNodeOutDatedAndNeedsRefresh() override;

	// Fills the list with all the morphs
	void UpdateMorphList();

	// Returns the mesh pin
	UEdGraphPin* GetMeshPin() const
	{
		return FindPin(TEXT("InMesh"), EGPD_Input);
	}
	
	// Returns the stack pin
	UEdGraphPin* GetStackPin() const
	{
		return FindPin(TEXT("Stack"), EGPD_Input);
	}

public:

	// List with all the morphs of the linked skeletal mesh
	UPROPERTY()
	TArray<FString> MorphNames;
};
