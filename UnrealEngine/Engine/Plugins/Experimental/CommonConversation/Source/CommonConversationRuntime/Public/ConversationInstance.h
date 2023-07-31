// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ConversationNode.h"
#include "ConversationRequirementNode.h"
#include "ConversationTaskNode.h"
#include "ConversationMemory.h"

#include "ConversationInstance.generated.h"

class UConversationInstance;
class UConversationNodeWithLinks;
class UConversationParticipantComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllParticipantsNotifiedOfStartEvent, UConversationInstance*, ConversationInstance);

//////////////////////////////////////////////////////////////////////

/**
 * An active conversation between one or more participants
 */
UCLASS()
class COMMONCONVERSATIONRUNTIME_API UConversationInstance : public UObject
{
	GENERATED_BODY()

public: 

	// Server notification sent after all participants have been individually notified of conversation start
	DECLARE_EVENT_OneParam(UConversationInstance, FOnAllParticipantsNotifiedOfStartEvent, UConversationInstance* /*ConversationInstance*/);
	FOnAllParticipantsNotifiedOfStartEvent OnAllParticipantsNotifiedOfStart;

public:
	UConversationInstance();

	virtual UWorld* GetWorld() const override;
	
#if WITH_SERVER_CODE
	void ServerRemoveParticipant(FGameplayTag ParticipantID);

	void ServerAssignParticipant(FGameplayTag ParticipantID, AActor* ParticipantActor);

	void ServerStartConversation(FGameplayTag EntryPoint);

	void ServerAdvanceConversation(const FAdvanceConversationRequest& InChoicePicked);

	virtual void OnInvalidBranchChoice(const FAdvanceConversationRequest& InChoicePicked);

	void ServerAbortConversation();

	void ServerRefreshConversationChoices();

	void ServerRefreshTaskChoiceData(const FConversationNodeHandle& Handle);

	/**
     * This is memory that will last for the duration of the conversation instance.  Don't store
     * anything here you want to be long lived.
	 */
	FConversationMemory& GetInstanceMemory() { return InstanceMemory; }

	TArray<FGuid> DetermineBranches(const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult = EConversationRequirementResult::Passed);
#endif

	//@TODO: Conversation: Meh
	TArray<FConversationParticipantEntry> GetParticipantListCopy() const
	{
		return Participants.List;
	}

	FConversationParticipants GetParticipantsCopy() const
	{
		return Participants;
	}

	const FConversationParticipantEntry* GetParticipant(FGameplayTag ParticipantID) const
	{
		return Participants.GetParticipant(ParticipantID);
	}

	UConversationParticipantComponent* GetParticipantComponent(FGameplayTag ParticipantID) const
	{
		return Participants.GetParticipantComponent(ParticipantID);
	}

	const FConversationNodeHandle& GetCurrentNodeHandle() const { return CurrentBranchPoint.GetNodeHandle(); }
	const TArray<FClientConversationOptionEntry>& GetCurrentUserConversationChoices() const { return CurrentUserChoices; }

protected:
	virtual void OnStarted() { }
	virtual void OnEnded() { }

#if WITH_SERVER_CODE
	void ModifyCurrentConversationNode(const FConversationChoiceReference& NewChoice);
	void ModifyCurrentConversationNode(const FConversationBranchPoint& NewBranchPoint);
	void ReturnToLastClientChoice(const FConversationContext& Context);
	void ReturnToCurrentClientChoice(const FConversationContext& Context);
	void ReturnToStart(const FConversationContext& Context);
	virtual void PauseConversationAndSendClientChoices(const FConversationContext& Context, const FClientConversationMessage& ClientMessage);
#endif

private:
	bool AreAllParticipantsReadyToConverse() const;
	void TryStartingConversation();

	const FConversationBranchPoint& GetCurrentBranchPoint() const { return CurrentBranchPoint; }
	const FConversationChoiceReference& GetCurrentChoiceReference() const { return CurrentBranchPoint.ClientChoice.ChoiceReference; }

	void ResetConversationProgress();
	void UpdateNextChoices(const FConversationContext& Context);
	void SetNextChoices(const TArray<FConversationBranchPoint>& InAllChoices);
	const FConversationBranchPoint* FindBranchPointFromClientChoice(const FConversationChoiceReference& InChoice) const;

#if WITH_SERVER_CODE
	void OnCurrentConversationNodeModified();
#endif

protected:

	TArray<FClientConversationOptionEntry> CurrentUserChoices;

private:
	UPROPERTY()
	FConversationParticipants Participants;

	FGameplayTag StartingEntryGameplayTag;
	FConversationBranchPoint StartingBranchPoint;

	FConversationBranchPoint CurrentBranchPoint;

	struct FCheckpoint
	{
		FConversationBranchPoint ClientBranchPoint;
		TArray<FConversationChoiceReference> ScopeStack;
	};

	TArray<FCheckpoint> ClientBranchPoints;

	TArray<FConversationBranchPoint> CurrentBranchPoints;

	TArray<FConversationChoiceReference> ScopeStack;

	FRandomStream ConversationRNG;

private:
#if WITH_SERVER_CODE
	FConversationMemory InstanceMemory;
#endif

private:
	bool bConversationStarted = false;
};
