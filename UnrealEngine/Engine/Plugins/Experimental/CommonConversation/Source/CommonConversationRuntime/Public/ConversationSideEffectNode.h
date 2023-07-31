// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationSubNode.h"
#include "ConversationContext.h"
#include "ConversationSideEffectNode.generated.h"

struct FConversationContext;

/**
 * Side effects are actions that are performed just after a task is executed
 * (this allows state-altering or cosmetic actions to be mixed in to other nodes)
 * 
 * When a task executes on the server, it replicates to the client that it executed and
 * to then execute any client side effects that may be necessary for that task.
 */
UCLASS(Abstract, Blueprintable)
class COMMONCONVERSATIONRUNTIME_API UConversationSideEffectNode : public UConversationSubNode
{
	GENERATED_BODY()

public:
	/** Called by the client and server code executes the side effect. */
	void CauseSideEffect(const FConversationContext& Context) const;

protected:
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ServerCauseSideEffect(const FConversationContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void ClientCauseSideEffect(const FConversationContext& Context) const;
};
