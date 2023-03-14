// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationSubNode.h"
#include "ConversationContext.h"
#include "GameplayTagContainer.h"

#include "ConversationChoiceNode.generated.h"

/**
 * A choice on a task indicates that an option be presented to the user when the owning task is one of
 * the available options of a preceding task.
 */
UCLASS(Blueprintable)
class COMMONCONVERSATIONRUNTIME_API UConversationChoiceNode : public UConversationSubNode
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(ExposeOnSpawn), Category=Conversation)
	FText DefaultChoiceDisplayText;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category=Conversation)
	FGameplayTagContainer ChoiceTags;

	virtual bool GenerateChoice(const FConversationContext& Context, FClientConversationOptionEntry& ChoiceEntry) const;

	virtual void NotifyChoicePickedByUser(const FConversationContext& InContext, const FClientConversationOptionEntry& InClientChoice) const;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void FillChoice(const FConversationContext& Context, FClientConversationOptionEntry& ChoiceEntry) const;
};
