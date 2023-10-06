// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationGraphNode.h"
#include "ConversationGraphNode_SideEffect.generated.h"

UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraphNode_SideEffect : public UConversationGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeBodyTintColor() const override;
};

