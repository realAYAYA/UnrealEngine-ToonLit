// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraphSchema.h"
#include "AnimNextGraph_EdGraphNode.h"
#include "AnimNextGraph_EdGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraphSchema : public UControlRigGraphSchema
{
	GENERATED_BODY()

	// UControlRigGraphSchema interface
	virtual TSubclassOf<URigVMEdGraphNode> GetGraphNodeClass(const URigVMEdGraph* InGraph) const override { return UAnimNextGraph_EdGraphNode::StaticClass(); }

	// UEdGraphSchema interface
	virtual void TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue, bool bMarkAsModified) const override;
};