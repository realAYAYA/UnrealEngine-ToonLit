// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationNode.h"
#include "ConversationEntryPointNode.generated.h"

UCLASS(meta=(DisplayName="Entry Point"))
class COMMONCONVERSATIONRUNTIME_API UConversationEntryPointNode : public UConversationNodeWithLinks
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Conversation)
	FGameplayTag EntryTag;
};
