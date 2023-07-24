// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraphSchema.h"
#include "AnimNextInterfaceGraph_EdGraphNode.h"
#include "AnimNextInterfaceGraph_EdGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextInterfaceGraph_EdGraphSchema : public UControlRigGraphSchema
{
	GENERATED_BODY()

	// UControlRigGraphSchema interface
	virtual TSubclassOf<UControlRigGraphNode> GetGraphNodeClass() const override { return UAnimNextInterfaceGraph_EdGraphNode::StaticClass(); }

	// UEdGraphSchema interface
	virtual void TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue, bool bMarkAsModified) const override;
};