// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraphNode.h"

#include "AnimNextGraph_EdGraphNode.generated.h"

// EdGraphNode representation for AnimNext nodes
// A node can hold a decorator stack or a decorator entry
UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraphNode : public URigVMEdGraphNode
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// UEdGraphNode implementation
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	//////////////////////////////////////////////////////////////////////////
	// URigVMEdGraphNode implementation
	virtual void ConfigurePin(UEdGraphPin* EdGraphPin, const URigVMPin* ModelPin) const override;

	//////////////////////////////////////////////////////////////////////////
	// Our implementation

	// Returns whether this node is a decorator stack or not
	ANIMNEXTUNCOOKEDONLY_API bool IsDecoratorStack() const;

private:
	// Populates the SubMenu with entries for each decorator that can be added through the context menu
	void BuildAddDecoratorContextMenu(class UToolMenu* SubMenu);
};
