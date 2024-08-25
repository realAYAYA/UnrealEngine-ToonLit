// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorGraphNode.h"

class UPCGNode;

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

UCLASS()
class UPCGEditorGraphNodeNamedRerouteBase : public UPCGEditorGraphNode
{
	GENERATED_BODY()

public:
	// ~Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// ~End UEdGraphNode interface
};

UCLASS()
class UPCGEditorGraphNodeNamedRerouteUsage : public UPCGEditorGraphNodeNamedRerouteBase
{
	GENERATED_BODY()
	friend class UPCGEditorGraphNodeNamedRerouteDeclaration;

protected:
	virtual void RebuildEdgesFromPins_Internal();
	virtual bool CanPickColor() const override { return false; }
	virtual FText GetPinFriendlyName(const UPCGPin* InPin) const override;
};

UCLASS()
class UPCGEditorGraphNodeNamedRerouteDeclaration : public UPCGEditorGraphNodeNamedRerouteBase
{
	GENERATED_BODY()

public:
	virtual void OnRenameNode(const FString& NewName) override;
	virtual void PostPaste() override;

	void SetNodeName(const UPCGNode* FromNode, FName FromPinName);
	void FixNodeNameCollision();
	FString GetCollisionFreeNodeName(const FString& BaseName) const;

protected:
	virtual FText GetPinFriendlyName(const UPCGPin* InPin) const override;
	virtual void OnColorPicked(FLinearColor NewColor) override;
	virtual void ReconstructNodeOnChange() override;

	void ApplyToUsageNodes(TFunctionRef<void(UPCGEditorGraphNodeNamedRerouteUsage*)> Action);
};