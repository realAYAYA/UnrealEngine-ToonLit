// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationGraphNode.h"
#include "ConversationGraphNode_EntryPoint.generated.h"

/** Root node of this conversation block */
UCLASS()
class COMMONCONVERSATIONGRAPH_API UConversationGraphNode_EntryPoint : public UConversationGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FName GetNameIcon() const override;
};
