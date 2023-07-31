// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshGeometryOperation.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshGeometryOperation : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshGeometryOperation();

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	inline UEdGraphPin* MeshAPin() const
	{
		return FindPin(TEXT("Mesh A"), EGPD_Input);
	}

	inline UEdGraphPin* MeshBPin() const
	{
		return FindPin(TEXT("Mesh B"), EGPD_Input);
	}

	inline UEdGraphPin* ScalarAPin() const
	{
		return FindPin(TEXT("Scalar A"));
	}

	inline UEdGraphPin* ScalarBPin() const
	{
		return FindPin(TEXT("Scalar B"));
	}
	
};

