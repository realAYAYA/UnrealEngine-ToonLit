// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationGraphNode.h"
#include "ConversationGraphNode_Requirement.generated.h"

UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraphNode_Requirement : public UConversationGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
};
