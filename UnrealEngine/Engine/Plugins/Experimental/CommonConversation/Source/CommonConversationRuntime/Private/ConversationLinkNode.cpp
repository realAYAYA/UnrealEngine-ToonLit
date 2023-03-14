// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationLinkNode.h"
#include "ConversationRegistry.h"
#include "ConversationInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationLinkNode)

#define LOCTEXT_NAMESPACE "ConversationLinkNode"

UConversationLinkNode::UConversationLinkNode()
{
#if WITH_EDITORONLY_DATA
	bHasDynamicChoices = true;
#endif
}

FConversationTaskResult UConversationLinkNode::ExecuteTaskNode_Implementation(const FConversationContext& Context) const
{
	return FConversationTaskResult::AdvanceConversation();
}

void UConversationLinkNode::GatherChoices(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& Context) const
{
#if WITH_SERVER_CODE
	TArray<FGuid> PotentialStartingPoints = Context.GetConversationRegistry().GetOutputLinkGUIDs(RemoteEntryTag);
	
	if (PotentialStartingPoints.Num() > 0)
	{
		TArray<FGuid> LegalStartingPoints = Context.GetActiveConversation()->DetermineBranches(PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);
		FConversationContext ReturnScopeContext = Context.CreateReturnScopeContext(GetNodeGuid());
		UConversationTaskNode::GenerateChoicesForDestinations(BranchBuilder, ReturnScopeContext, LegalStartingPoints);
	}
#endif
}

#undef LOCTEXT_NAMESPACE
