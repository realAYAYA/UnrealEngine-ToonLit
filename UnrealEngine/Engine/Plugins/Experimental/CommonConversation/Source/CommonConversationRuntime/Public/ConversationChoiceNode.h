// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationSubNode.h"
#include "GameplayTagContainer.h"

#include "ConversationChoiceNode.generated.h"

struct FClientConversationOptionEntry;

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

	bool GetHideChoiceClassName() const { return bHideChoiceClassName; }

	virtual bool GenerateChoice(const FConversationContext& Context, FClientConversationOptionEntry& ChoiceEntry) const;

	virtual void NotifyChoicePickedByUser(const FConversationContext& InContext, const FClientConversationOptionEntry& InClientChoice) const;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void FillChoice(const FConversationContext& Context, FClientConversationOptionEntry& ChoiceEntry) const;

	bool bHideChoiceClassName = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ConversationContext.h"
#endif
