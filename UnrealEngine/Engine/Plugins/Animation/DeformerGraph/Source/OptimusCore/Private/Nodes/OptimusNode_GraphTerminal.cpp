// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_GraphTerminal.h"

#include "OptimusFunctionNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusBindingTypes.h"
#include "OptimusNode_ComputeKernelBase.h"


#define LOCTEXT_NAMESPACE "OptimusNodeGraphTerminal"


FText UOptimusNode_GraphTerminal::GetDisplayName() const
{
	switch(TerminalType)
	{
	case EOptimusTerminalType::Entry:
		return LOCTEXT("TerminalType_Entry", "Entry"); 
	case EOptimusTerminalType::Return:
		return LOCTEXT("TerminalType_Return", "Return");
	case EOptimusTerminalType::Unknown:
		checkNoEntry();
	}
	return FText();
}


void UOptimusNode_GraphTerminal::ConstructNode()
{
	UOptimusNodeSubGraph* Graph = OwningGraph.Get();
	if (!ensure(Graph))
	{
		return;
	}

	switch(TerminalType)
	{
	case EOptimusTerminalType::Entry:
		{
			// The input bindings represent the _inputs_ to the graph, so when they are accessed
			// from within the graph, they get added as _outputs_ to the entry node.
			for (const FOptimusParameterBinding& Binding: Graph->InputBindings)
			{
				AddPinDirect(Binding, EOptimusNodePinDirection::Output);
			}
		}
		break;
	case EOptimusTerminalType::Return:
		// And vice-versa.
		{
			for (const FOptimusParameterBinding& Binding: Graph->OutputBindings)
			{
				AddPinDirect(Binding, EOptimusNodePinDirection::Input);
			}
		}
		break;
	case EOptimusTerminalType::Unknown:
		checkNoEntry();
	}
}


FOptimusRoutedNodePin UOptimusNode_GraphTerminal::GetPinCounterpart(
	UOptimusNodePin* InNodePin,
	const FOptimusPinTraversalContext& InTraversalContext
) const
{
	if (!InNodePin || InNodePin->GetOwningNode() != this)
	{
		return {};
	}
	
	if (!ensure(!InTraversalContext.ReferenceNesting.IsEmpty()))
	{
		return {};
	}

	const UOptimusNode* CounterpartNode = Cast<const UOptimusNode>(InTraversalContext.ReferenceNesting.Last());
	if (!ensure(CounterpartNode))
	{
		return {};
	}

	FOptimusRoutedNodePin Result{
		CounterpartNode->FindPinFromPath(InNodePin->GetPinNamePath()),
		InTraversalContext
	};
	Result.TraversalContext.ReferenceNesting.Pop();
	
	return Result;
}

#undef LOCTEXT_NAMESPACE
