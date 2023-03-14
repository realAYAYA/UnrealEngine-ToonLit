// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationGraphNode.h"
#include "ConversationGraphNode_Task.generated.h"

class UEdGraph;
class UEdGraphSchema;

UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraphNode_Task : public UConversationGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeBodyTintColor() const override;

	/** Gets a list of actions that can be done to this particular node */
	virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	virtual bool CanPlaceBreakpoints() const override { return true; }
};
