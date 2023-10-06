// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraphSchema.h"
#include "AnimNextParameterBlock_EdGraphNode.h"
#include "AnimNextParameterBlock_EdGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextParameterBlock_EdGraphSchema : public UControlRigGraphSchema
{
	GENERATED_BODY()

	// UControlRigGraphSchema interface
	virtual TSubclassOf<URigVMEdGraphNode> GetGraphNodeClass(const URigVMEdGraph* InGraph) const override { return UAnimNextParameterBlock_EdGraphNode::StaticClass(); }

	// UEdGraphSchema interface
	virtual void TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue, bool bMarkAsModified) const override;
};