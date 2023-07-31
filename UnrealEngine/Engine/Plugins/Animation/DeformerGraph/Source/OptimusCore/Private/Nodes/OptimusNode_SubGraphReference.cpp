// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_SubGraphReference.h"

#include "OptimusNodePin.h"
#include "OptimusNodeSubGraph.h"
#include "OptimusBindingTypes.h"
#include "OptimusNode_GraphTerminal.h"


FText UOptimusNode_SubGraphReference::GetDisplayName() const
{
	if (SubGraph)
	{
		return FText::FromString(SubGraph->GetName());
	}

	return FText::FromString("<graph missing>");
}


void UOptimusNode_SubGraphReference::ConstructNode()
{
	if (ensure(SubGraph))
	{
		// After a duplicate, the kernel node has no pins, so we need to reconstruct them from
		// the bindings. We can assume that all naming clashes have already been dealt with.
		for (const FOptimusParameterBinding& Binding: SubGraph->InputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Input);
		}
		for (const FOptimusParameterBinding& Binding: SubGraph->OutputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Output);
		}
	}
}


FOptimusRoutedNodePin UOptimusNode_SubGraphReference::GetPinCounterpart(
	UOptimusNodePin* InNodePin,
	const FOptimusPinTraversalContext& InTraversalContext
) const
{
	if (!InNodePin || InNodePin->GetOwningNode() != this)
	{
		return {};
	}

	if (!ensure(SubGraph))
	{
		return {};
	}

	UOptimusNode_GraphTerminal* CounterpartNode = nullptr;
	if (InNodePin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		CounterpartNode = SubGraph->EntryNode.Get();
	}
	else if (InNodePin->GetDirection() == EOptimusNodePinDirection::Output)
	{
		CounterpartNode = SubGraph->ReturnNode.Get();
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
