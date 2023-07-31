// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationSideEffectNode.h"
#include "ConversationInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationSideEffectNode)

void UConversationSideEffectNode::CauseSideEffect(const FConversationContext& Context) const
{
	TGuardValue<decltype(EvalWorldContextObj)> Swapper(EvalWorldContextObj, Context.GetWorld());
	
	if (Context.IsServerContext())
	{
		ServerCauseSideEffect(Context);
	}

	if (Context.IsClientContext())
	{
		ClientCauseSideEffect(Context);
	}
}

void UConversationSideEffectNode::ServerCauseSideEffect_Implementation(const FConversationContext& Context) const
{
}

void UConversationSideEffectNode::ClientCauseSideEffect_Implementation(const FConversationContext& Context) const
{
}
