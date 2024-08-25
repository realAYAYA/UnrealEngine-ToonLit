// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusNodeSubGraphReferencer.generated.h"

class UOptimusNodeGraph;
class UOptimusNodeSubGraph;
class UOptimusComponentSourceBinding;
class UOptimusNodePin;
struct FOptimusPinTraversalContext;

UINTERFACE()
class OPTIMUSCORE_API UOptimusNodeSubGraphReferencer :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to get a node graph to display
*/
class OPTIMUSCORE_API IOptimusNodeSubGraphReferencer
{
	GENERATED_BODY()

public:
	virtual UOptimusNodeSubGraph* GetReferencedSubGraph() const = 0;
	virtual UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const = 0;
	virtual UOptimusNodePin* GetDefaultComponentBindingPin() const = 0;
};

