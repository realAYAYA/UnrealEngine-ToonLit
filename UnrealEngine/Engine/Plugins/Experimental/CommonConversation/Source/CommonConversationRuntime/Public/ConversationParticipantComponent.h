// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ConversationNode.h"
#include "GameFramework/Actor.h"
#include "ConversationMemory.h"
#include "ConversationTypes.h"

#include "ConversationParticipantComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConversationStatusChanged, bool, bIsInConversation);

/**
 * Active conversation participants should have this component on them.
 * It keeps track of what conversations they are participating in (typically no more than one)
 */
UCLASS(BlueprintType)
class COMMONCONVERSATIONRUNTIME_API UConversationParticipantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UConversationParticipantComponent();

	// Client and server notification of the conversation starting or ending
	DECLARE_EVENT_OneParam(UConversationParticipantComponent, FConversationStatusChangedEvent, bool /*Started*/);
	FConversationStatusChangedEvent ConversationStatusChanged;

	DECLARE_EVENT(UConversationParticipantComponent, FConversationStartedEvent);
	FConversationStartedEvent ConversationStarted;

	DECLARE_EVENT_OneParam(UConversationParticipantComponent, FConversationUpdatedEvent, const FClientConversationMessagePayload& /*Message*/);
	FConversationUpdatedEvent ConversationUpdated;

	DECLARE_EVENT_TwoParams(UConversationParticipantComponent, FConversationTaskChoiceDataUpdatedEvent, const FConversationNodeHandle& /*Handle*/, const FClientConversationOptionEntry& /*OptionEntry*/);
	FConversationTaskChoiceDataUpdatedEvent ConversationTaskChoiceDataUpdated;

public:
	void SendClientConversationMessage(const FConversationContext& Context, const FClientConversationMessagePayload& Payload);
	virtual void SendClientUpdatedChoices(const FConversationContext& Context);
	void SendClientRefreshedTaskChoiceData(const FConversationNodeHandle& Handle, const FConversationContext& Context);

	UFUNCTION(BlueprintCallable, Category=Conversation)
	void RequestServerAdvanceConversation(const FAdvanceConversationRequest& InChoicePicked);

#if WITH_SERVER_CODE
	void ServerNotifyConversationStarted(UConversationInstance* Conversation, FGameplayTag AsParticipant);
	void ServerNotifyConversationEnded(UConversationInstance* Conversation);
	void ServerNotifyExecuteTaskAndSideEffects(const FConversationNodeHandle& Handle);
	void ServerForAllConversationsRefreshChoices(UConversationInstance* IgnoreConversation = nullptr);
	void ServerForAllConversationsRefreshTaskChoiceData(const FConversationNodeHandle& Handle, UConversationInstance* IgnoreConversation /*= nullptr*/);

	/** Check if this actor is in a good state to start a conversation */
	virtual bool ServerIsReadyToConverse() const { return true; }

	/** Ask to this actor to change is state to be able to start a conversation */
	virtual void ServerGetReadyToConverse();

	/** Ask this actor to abort all active conversations */
	void ServerAbortAllConversations();

	FConversationMemory& GetParticipantMemory() { return ParticipantMemory; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FParticipantReadyToConverse, UConversationParticipantComponent*);
	/** Delegate send when this actor enter in the good state to start a conversation */
	FParticipantReadyToConverse OnParticipantReadyToConverseEvent;
#endif

	UFUNCTION(BlueprintCallable, Category=Conversation)
	virtual FText GetParticipantDisplayName();

	UFUNCTION(BlueprintCallable, Category = Conversation)
	bool IsInActiveConversation() const;

public:

	FConversationNodeHandle GetCurrentNodeHandle() const;

	const FConversationParticipantEntry* GetParticipant(const FGameplayTag& ParticipantTag) const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerAdvanceConversation(const FAdvanceConversationRequest& InChoicePicked);

	UFUNCTION(Client, Reliable)
	void ClientUpdateParticipants(const FConversationParticipants& InParticipants);

	UFUNCTION(Client, Reliable)
	void ClientExecuteTaskAndSideEffects(FConversationNodeHandle Handle);

	UFUNCTION(Client, Reliable)
	void ClientUpdateConversation(const FClientConversationMessagePayload& Message);

	UFUNCTION(Client, Reliable)
	void ClientUpdateConversationTaskChoiceData(FConversationNodeHandle Handle, const FClientConversationOptionEntry& OptionEntry);

	UFUNCTION(Client, Reliable)
	void ClientUpdateConversations(int32 InConversationsActive);

	UFUNCTION(Client, Reliable)
	void ClientStartConversation(const FGameplayTag AsParticipant);

protected:
	UFUNCTION()
	void OnRep_ConversationsActive(int32 OldConversationsActive);

	virtual void OnEnterConversationState();
	virtual void OnLeaveConversationState();
	virtual void OnConversationUpdated(const FClientConversationMessagePayload& Message);

#if WITH_SERVER_CODE
	virtual void OnServerConversationStarted(UConversationInstance* Conversation, FGameplayTag AsParticipant);
	virtual void OnServerConversationEnded(UConversationInstance* Conversation);
#endif

#if WITH_SERVER_CODE
	UConversationInstance* GetCurrentConversationForAuthority() const { return Auth_CurrentConversation; }
	const TArray<UConversationInstance*>& GetConversationsForAuthority() const { return Auth_Conversations; }
#endif

public:
    // The number of conversations active.  A given conversationalist might be in multiple conversations at once.
    // e.g. Multiple players "talking" to the same NPC in a multiplayer game.
	int32 GetConversationsActive() const { return ConversationsActive; }
	
	// A cached version of the last conversation message payload data recieved.
	const FClientConversationMessagePayload& GetLastMessage() const { return LastMessage; }
	
	// Gets the last message index recieved, this is just a monotomically increasing number each time we get a new message.
	const int32 GetLastMessageIndex() const { return MessageIndex; }

	bool GetIsFirstConversationUpdateBroadcasted() const { return bIsFirstConversationUpdateBroadcasted; }

private:

	UPROPERTY(Replicated, ReplicatedUsing=OnRep_ConversationsActive)
	int32 ConversationsActive = 0;

private:
#if WITH_SERVER_CODE
	FConversationMemory ParticipantMemory;
#endif

private:
	UPROPERTY()
	TObjectPtr<UConversationInstance> Auth_CurrentConversation;

	UPROPERTY()
	TArray<TObjectPtr<UConversationInstance>> Auth_Conversations;
	
	UPROPERTY()
    FClientConversationMessagePayload LastMessage;
    
    int32 MessageIndex = 0;
    
	bool bIsFirstConversationUpdateBroadcasted = false;
};

//////////////////////////////////////////////////////////////////////
