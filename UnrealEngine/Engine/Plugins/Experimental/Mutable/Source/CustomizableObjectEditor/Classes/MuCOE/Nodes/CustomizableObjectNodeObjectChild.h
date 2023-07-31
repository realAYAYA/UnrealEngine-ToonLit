// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeObjectChild.generated.h"

class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeObjectChild : public UCustomizableObjectNodeObject
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeObjectChild();

public:
	// UEdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	void PrepareForCopying() override;
	FText GetTooltipText() const override;
	bool CanUserDeleteNode() const override;
	bool CanDuplicateNode() const override;
};

