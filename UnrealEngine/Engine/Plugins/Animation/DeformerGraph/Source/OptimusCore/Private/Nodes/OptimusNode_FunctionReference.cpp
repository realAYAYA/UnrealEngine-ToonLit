// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_FunctionReference.h"

#include "IOptimusCoreModule.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusFunctionNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusNode_GraphTerminal.h"
#include "OptimusComponentSource.h"
#include "OptimusDeformer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusNode_FunctionReference)


FName UOptimusNode_FunctionReference::GetNodeCategory() const
{
	return FunctionGraph->Category;

}

FText UOptimusNode_FunctionReference::GetDisplayName() const
{
	if (FunctionGraph.IsValid())
	{
		return FText::FromString(FunctionGraph->GetNodeName());
	}

	
	return FText::FromString("<graph missing>");
}


void UOptimusNode_FunctionReference::ConstructNode()
{
	if (FunctionGraph.IsValid())
	{
		const FOptimusDataTypeRegistry& TypeRegistry = FOptimusDataTypeRegistry::Get();
		FOptimusDataTypeRef ComponentSourceType = TypeRegistry.FindType(*UOptimusComponentSourceBinding::StaticClass());
		DefaultComponentPin = AddPinDirect(UOptimusNodeSubGraph::GraphDefaultComponentPinName, EOptimusNodePinDirection::Input, {}, ComponentSourceType);
		
		// After a duplicate, the kernel node has no pins, so we need to reconstruct them from
		// the bindings. We can assume that all naming clashes have already been dealt with.
		for (const FOptimusParameterBinding& Binding: FunctionGraph->InputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Input);
		}
		for (const FOptimusParameterBinding& Binding: FunctionGraph->OutputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Output);
		}
	}
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

	if (!FunctionGraph.IsValid())
	{
		return {};
	}

	UOptimusNode_GraphTerminal* CounterpartNode = nullptr;
	if (InNodePin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		CounterpartNode = FunctionGraph->GetTerminalNode(EOptimusTerminalType::Entry);
	}
	else if (InNodePin->GetDirection() == EOptimusNodePinDirection::Output)
	{
		CounterpartNode = FunctionGraph->GetTerminalNode(EOptimusTerminalType::Return);
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

UOptimusNodeGraph* UOptimusNode_FunctionReference::GetNodeGraphToShow()
{
	return FunctionGraph.Get();
}

UOptimusNodeSubGraph* UOptimusNode_FunctionReference::GetReferencedSubGraph() const
{
	return FunctionGraph.Get();
}

UOptimusComponentSourceBinding* UOptimusNode_FunctionReference::GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const
{
	if (!ensure(DefaultComponentPin.IsValid()))
	{
		return nullptr;
	}
	
	const UOptimusNodeGraph* OwningGraph = GetOwningGraph();
	TSet<UOptimusComponentSourceBinding*> Bindings = OwningGraph->GetComponentSourceBindingsForPin(DefaultComponentPin.Get(), InTraversalContext);
	
	if (!Bindings.IsEmpty() && ensure(Bindings.Num() == 1))
	{
		return Bindings.Array()[0];
	}

	// Default to the primary binding, but only if we're at the top-most level of the graph.
	if (const UOptimusDeformer* Deformer = Cast<UOptimusDeformer>(OwningGraph->GetCollectionOwner()))
	{
		return Deformer->GetPrimaryComponentBinding();
	}

	if (const UOptimusNodeSubGraph* OwningSubGraph = Cast<UOptimusNodeSubGraph>(OwningGraph))
	{
		return OwningSubGraph->GetDefaultComponentBinding(InTraversalContext);
	}

	return nullptr;		
}

UOptimusNodePin* UOptimusNode_FunctionReference::GetDefaultComponentBindingPin() const
{
	return DefaultComponentPin.Get();
}

FSoftObjectPath UOptimusNode_FunctionReference::GetSerializedGraphPath() const
{
	return FunctionGraph.ToSoftObjectPath();
}

void UOptimusNode_FunctionReference::InitializeSerializedGraphPath(const FSoftObjectPath& InInitialGraphPath)
{
	FunctionGraph = InInitialGraphPath;
}

void UOptimusNode_FunctionReference::RefreshSerializedGraphPath(const FSoftObjectPath& InRenamedGraphPath)
{
	// Unlike subgraph reference where we can directly query the graph for its new name/path
	// the function graph may not be loaded at the time of rename and thus
	// the caller needs to provide the path to the renamed graph 
	FunctionGraph = InRenamedGraphPath;
	FunctionGraph.LoadSynchronous();
	SetDisplayName(GetDisplayName());
}

void UOptimusNode_FunctionReference::InitializeTransientData()
{
	Super::InitializeTransientData();
	// Making sure the pointer is alive such that we avoid loading on demand everywhere
	FunctionGraph.LoadSynchronous();
}
