// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusNode.h"
#include "OptimusNodeSubGraph.h"
#include "Nodes/OptimusNode_ComputeKernelBase.h"


#include "OptimusFunctionNodeGraph.generated.h"

/**
 * 
 */
UCLASS()
class OPTIMUSCORE_API UOptimusFunctionNodeGraph :
	public UOptimusNodeSubGraph
{
	GENERATED_BODY()

public:
	/** The name to give the node based off of this graph */
	FString GetNodeName() const; 

	/** The category of the node based of of this graph for listing purposes */ 
	UPROPERTY(EditAnywhere, Category=Settings)
	FName Category = UOptimusNode::CategoryName::Deformers;
};
