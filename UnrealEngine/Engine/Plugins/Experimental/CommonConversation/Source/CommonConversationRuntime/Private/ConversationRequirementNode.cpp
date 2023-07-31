// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationRequirementNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationRequirementNode)

EConversationRequirementResult MergeRequirements(EConversationRequirementResult CurrentResult, EConversationRequirementResult MergeResult)
{
	if ((int64)MergeResult > (int64)CurrentResult)
	{
		return MergeResult;
	}

	return CurrentResult;
}

EConversationRequirementResult UConversationRequirementNode::IsRequirementSatisfied_Implementation(const FConversationContext& Context) const
{
	return EConversationRequirementResult::FailedAndHidden;
}

