// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorGraphNode.h"

#include "PCGEditorGraphNodeReroute.generated.h"

UCLASS()
class UPCGEditorGraphNodeReroute : public UPCGEditorGraphNode
{
	GENERATED_BODY()

public:
	// ~Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldOverridePinNames() const override;
	virtual FText GetPinNameOverride(const UEdGraphPin& Pin) const override;
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override;
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override;
	virtual FText GetTooltipText() const override;
	virtual UEdGraphPin* GetPassThroughPin(const UEdGraphPin* FromPin) const override;
	// ~End UEdGraphNode interface

	UEdGraphPin* GetInputPin() const;
	UEdGraphPin* GetOutputPin() const;
};
