// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorGraphNodeBase.h"

#include "PCGEditorGraphNode.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UToolMenu;

UCLASS()
class UPCGEditorGraphNode : public UPCGEditorGraphNodeBase
{
	GENERATED_BODY()

public:
	UPCGEditorGraphNode(const FObjectInitializer& ObjectInitializer);
	
	// ~Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual void AllocateDefaultPins() override;
	virtual void OnRenameNode(const FString& NewName) override;
	// ~End UEdGraphNode interface
};
