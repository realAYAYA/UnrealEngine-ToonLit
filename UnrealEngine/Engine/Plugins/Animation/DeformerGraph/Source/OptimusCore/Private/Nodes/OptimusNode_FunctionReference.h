// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IOptimusNodePinRouter.h"
#include "OptimusNode.h"

#include "OptimusNode_FunctionReference.generated.h"


class UOptimusFunctionNodeGraph;


UCLASS(Hidden)
class UOptimusNode_FunctionReference :
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
	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node.
	 */
	UPROPERTY()
	TObjectPtr<UOptimusFunctionNodeGraph> FunctionGraph;
};
