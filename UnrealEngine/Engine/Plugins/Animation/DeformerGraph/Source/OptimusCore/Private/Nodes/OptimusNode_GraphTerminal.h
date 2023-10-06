// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IOptimusNodePinRouter.h"
#include "OptimusNode.h"

#include "OptimusNode_GraphTerminal.generated.h"


class UOptimusNodeSubGraph;


UENUM()
enum class EOptimusTerminalType
{
	Unknown,
	Entry,
	Return
};


UCLASS(Hidden)
class UOptimusNode_GraphTerminal :
	public UOptimusNode,
	public IOptimusNodePinRouter
{
	GENERATED_BODY()
	
public:
	// UOptimusNode overrides
	bool CanUserDeleteNode() const override { return false; }
	FName GetNodeCategory() const override { return NAME_None; }
	FText GetDisplayName() const override;
	void ConstructNode() override;
	
	// IOptimusNodePinRouter implementation
	FOptimusRoutedNodePin GetPinCounterpart(
		UOptimusNodePin* InNodePin,
		const FOptimusPinTraversalContext& InTraversalContext
	) const override;

protected:
	friend class UOptimusNodeGraph;

	/** Indicates whether this is an entry or a return terminal node */
	UPROPERTY()
	EOptimusTerminalType TerminalType;

	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node.
	 */
	UPROPERTY()
	TWeakObjectPtr<UOptimusNodeSubGraph> OwningGraph;
};
