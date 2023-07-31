// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SGraphPin;
class UEdGraphNode;
class UEdGraphPin;


/** Custom widget for the Material node. */
class SCustomizableObjectNodeMaterial : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SCustomizableObjectNodeMaterial) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
};