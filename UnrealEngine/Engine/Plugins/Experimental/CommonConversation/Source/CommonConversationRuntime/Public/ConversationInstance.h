// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationRequirementNode.h"
#include "ConversationMemory.h"

#include "ConversationTypes.h"
#include "Math/RandomStream.h"
#include "ConversationInstance.generated.h"

class UConversationChoiceNode;
class UConversationDatabase;
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
	/** Should be called with a copy of the conversation participants before any removals happen, that way clients can properly respond to the end of their respective conversations
	  * with an accurate account of who was in that conversation. */
	void ServerRemoveParticipant(const FGameplayTag& ParticipantID, const FConversationParticipants& PreservedParticipants);

	void ServerAssignParticipant(const FGameplayTag& ParticipantID, AActor* ParticipantActor);

	void ServerStartConversation(const FGameplayTag& EntryPoint, const UConversationDatabase* Graph = nullptr);

	void ServerAdvanceConversation(const FAdvanceConversationRequest& InChoicePicked);

	virtual void OnInvalidBranchChoice(const FAdvanceConversationRequest& InChoicePicked);

	void ServerAbortConversation();

	void ServerRefreshConversationChoices();

	void ServerRefreshTaskChoiceData(const FConversationNodeHandle& Handle);

	/** Attempts to process the current conversation node again - only useful in very specific circumstances where you'd want to re-run the current node
	  * without having to deal with conversation flow changes. */
	void ServerRefreshCurrentConversationNode();

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

	const UConversationDatabase* GetActiveConversationGraph() const
	{
		return ActiveConversationGraph.Get();
	}

	const FConversationNodeHandle& GetCurrentNodeHandle() const { return CurrentBranchPoint.GetNodeHandle(); }
	const FConversationChoiceReference& GetCurrentChoiceReference() const { return CurrentBranchPoint.ClientChoice.ChoiceReference; }
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
	virtual void OnChoiceNodePickedByUser(const FConversationContext& Context, const UConversationChoiceNode* ChoiceNode, const TArray<FConversationBranchPoint>& ValidDestinations) {};
#endif

private:
	bool AreAllParticipantsReadyToConverse() const;
	void TryStartingConversation();

	const FConversationBranchPoint& GetCurrentBranchPoint() const { return CurrentBranchPoint; }

	void ResetConversationProgress();
	void UpdateNextChoices(const FConversationContext& Context);
	void SetNextChoices(const TArray<FConversationBranchPoint>& InAllChoices);
	const FConversationBranchPoint* FindBranchPointFromClientChoice(const FConversationChoiceReference& InChoice) const;

#if WITH_SERVER_CODE
	void OnCurrentConversationNodeModified();

	void ProcessCurrentConversationNode();
#endif //WITH_SERVER_CODE

protected:

	TArray<FClientConversationOptionEntry> CurrentUserChoices;

private:
	UPROPERTY()
	FConversationParticipants Participants;

	UPROPERTY()
	TObjectPtr<const UConversationDatabase> ActiveConversationGraph = nullptr;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ConversationTaskNode.h"
#include "GameFramework/Actor.h"
#endif
