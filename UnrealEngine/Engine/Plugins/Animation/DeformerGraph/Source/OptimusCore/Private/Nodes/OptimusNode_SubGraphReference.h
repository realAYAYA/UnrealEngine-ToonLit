// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodePinRouter.h"
#include "OptimusNode.h"

#include "OptimusNode_SubGraphReference.generated.h"


class UOptimusNodeSubGraph;


UCLASS(Hidden)
class UOptimusNode_SubGraphReference :
	public UOptimusNode,
	public IOptimusNodePinRouter
{
	GENERATED_BODY()

public:
	// UOptimusNode overrides
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
	
	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node.
	 */
	UPROPERTY()
	TObjectPtr<UOptimusNodeSubGraph> SubGraph;
};
