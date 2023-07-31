// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OptimusNodeGraph.h"
#include "OptimusBindingTypes.h"
#include "OptimusNodeSubGraph.generated.h"

class UOptimusNode_GraphTerminal;
UCLASS()
class UOptimusNodeSubGraph :
	public UOptimusNodeGraph
{
	GENERATED_BODY()
public:
	UOptimusNodeSubGraph();

	// FIXME: These are uneditable for now.
	UPROPERTY(VisibleAnywhere, Category=Bindings)
	TArray<FOptimusParameterBinding> InputBindings;

	UPROPERTY(VisibleAnywhere, Category=Bindings)
	TArray<FOptimusParameterBinding> OutputBindings;

	UPROPERTY()
	TWeakObjectPtr<UOptimusNode_GraphTerminal> EntryNode; 

	UPROPERTY()
	TWeakObjectPtr<UOptimusNode_GraphTerminal> ReturnNode; 
};
