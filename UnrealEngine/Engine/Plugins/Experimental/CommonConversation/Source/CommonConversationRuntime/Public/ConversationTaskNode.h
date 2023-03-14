// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConversationNode.h"
#include "ConversationContext.h"
#include "ConversationRequirementNode.h"
#include "ConversationTypes.h"

#include "ConversationTaskNode.generated.h"

class UConversationSubNode;

/** 
 * Task are leaf nodes of behavior tree, which perform actual actions
 *
 * Because some of them can be instanced for specific AI, following virtual functions are not marked as const:
 *  - ExecuteTask
 *  - AbortTask
 *  - TickTask
 *  - OnMessage
 *
 * If your node is not being instanced (default behavior), DO NOT change any properties of object within those functions!
 * Template nodes are shared across all behavior tree components using the same tree asset and must store
 * their runtime properties in provided NodeMemory block (allocation size determined by GetInstanceMemorySize() )
 *
 */

/**
 * The ConversationTaskNode is the basis of any task in the conversation graph,
 * that task may be as simple as saying some text to the user, and providing some choices.
 * However more complex tasks can fire off quests, can spawn actors, pretty much any arbitrary
 * thing you want.
 * 
 * The conversation system is less about just a dialogue tree, and more about a graph of
 * actions the NPC can take, and choices they can provide to the player.
 */
UCLASS(Abstract, Blueprintable)
class COMMONCONVERSATIONRUNTIME_API UConversationTaskNode : public UConversationNodeWithLinks
{
	GENERATED_BODY()

public:
	// Requirements and side effects
	UPROPERTY()
	TArray<TObjectPtr<UConversationSubNode>> SubNodes;

#if WITH_EDITORONLY_DATA
	/* EDITOR ONLY VISUALS: Does this task internally have requirements? */
	UPROPERTY(EditDefaultsOnly, Category=Conversation)
	uint32 bHasRequirements : 1;

	/** EDITOR ONLY VISUALS: Does this task generate dynamic choices? */
	UPROPERTY(EditDefaultsOnly, Category=Conversation)
	uint32 bHasDynamicChoices : 1;
#endif

	//This setting is designed for requirements that only matter when option is generated
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	bool bIgnoreRequirementsWhileAdvancingConversations = false;

public:
	UFUNCTION(BlueprintNativeEvent)
	bool GetNodeBodyColor(FLinearColor& BodyColor) const;

public:
	//@TODO: CONVERSATION: Comment me
	FConversationTaskResult ExecuteTaskNodeWithSideEffects(const FConversationContext& Context) const;

public:
#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	/**
	 * Returns the highest priority EConversationRequirementResult, so Passed, having the least priority
	 * and FailedHidden having the highest priority.
	 */
	EConversationRequirementResult CheckRequirements(const FConversationContext& InContext) const;

	static void GenerateChoicesForDestinations(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& InContext, const TArray<FGuid>& CandidateDestinations);

protected:
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	EConversationRequirementResult IsRequirementSatisfied(const FConversationContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	FConversationTaskResult ExecuteTaskNode(const FConversationContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void ExecuteClientEffects(const FConversationContext& Context) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void GatherStaticExtraData(const FConversationContext& Context, TArray<FConversationNodeParameterPair>& InOutExtraData) const;

	virtual void GatherChoices(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& Context) const;

	virtual void GatherStaticChoices(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& Context) const;
	virtual void GatherDynamicChoices(FConversationBranchPointBuilder& BranchBuilder, const FConversationContext& Context) const;

protected:
#if WITH_EDITORONLY_DATA
	/** Default color of the node. */
	UPROPERTY(EditDefaultsOnly, Category=Description)
	FLinearColor DefaultNodeBodyColor = FLinearColor::White;
#endif
};

