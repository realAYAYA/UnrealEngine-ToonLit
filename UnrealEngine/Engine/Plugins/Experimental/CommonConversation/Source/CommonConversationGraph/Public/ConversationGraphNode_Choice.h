// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationGraphNode.h"
#include "ConversationGraphNode_Choice.generated.h"

UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraphNode_Choice : public UConversationGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
};

