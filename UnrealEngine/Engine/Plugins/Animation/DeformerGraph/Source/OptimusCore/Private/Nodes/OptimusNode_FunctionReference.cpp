// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_FunctionReference.h"

#include "OptimusFunctionNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusNode_GraphTerminal.h"


FText UOptimusNode_FunctionReference::GetDisplayName() const
{
	if (FunctionGraph)
	{
		return FText::FromString(FunctionGraph->GetNodeName());
	}

	return FText::FromString("<graph missing>");
}


void UOptimusNode_FunctionReference::ConstructNode()
{
	
}


FOptimusRoutedNodePin UOptimusNode_FunctionReference::GetPinCounterpart(
	UOptimusNodePin* InNodePin,
	const FOptimusPinTraversalContext& InTraversalContext
) const
{
	if (!InNodePin || InNodePin->GetOwningNode() != this)
	{
		return {};
	}

	if (!ensure(FunctionGraph))
	{
		return {};
	}

	UOptimusNode_GraphTerminal* CounterpartNode = nullptr;
	if (InNodePin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		CounterpartNode = FunctionGraph->EntryNode.Get();
	}
	else if (InNodePin->GetDirection() == EOptimusNodePinDirection::Output)
	{
		CounterpartNode = FunctionGraph->ReturnNode.Get();
	}

	if (!ensure(CounterpartNode))
	{
		return {};
	}

	FOptimusRoutedNodePin Result{
		CounterpartNode->FindPinFromPath(InNodePin->GetPinNamePath()),
		InTraversalContext
	};
	Result.TraversalContext.ReferenceNesting.Push(this);

	return Result;
}
