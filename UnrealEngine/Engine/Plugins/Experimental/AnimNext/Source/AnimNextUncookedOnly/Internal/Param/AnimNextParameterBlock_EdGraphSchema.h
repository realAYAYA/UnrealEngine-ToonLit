// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "AnimNextParameterBlock_EdGraphNode.h"
#include "AnimNextParameterBlock_EdGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EdGraphSchema : public URigVMEdGraphSchema
{
	GENERATED_BODY()

	// URigVMEdGraphSchema interface
	virtual TSubclassOf<URigVMEdGraphNode> GetGraphNodeClass(const URigVMEdGraph* InGraph) const override { return UAnimNextParameterBlock_EdGraphNode::StaticClass(); }

	// UEdGraphSchema interface
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
};