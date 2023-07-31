// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectKey.h"
#include "ConversationTaskNode.h"

#include "ConversationLinkNode.generated.h"

UCLASS()
class COMMONCONVERSATIONRUNTIME_API UConversationLinkNode : public UConversationTaskNode
{
	GENERATED_BODY()

public:
	UConversationLinkNode();

	FGameplayTag GetRemoteEntryTag() const { return RemoteEntryTag; }

protected:
	virtual FConversationTaskResult ExecuteTaskNode_Implementation(const FConversationContext& Context) const override;
	virtual void GatherChoices(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Link")
	FGameplayTag RemoteEntryTag;
};
