// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAgentsTrainer.h" // Required for ELearningAgentsCompletion::Termination
#include "LearningAgentsDebug.h"

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "LearningAgentsCompletions.generated.h"

namespace UE::Learning
{
	struct FCompletionObject;
	struct FConditionalCompletion;
	struct FTimeElapsedCompletion;
	struct FPlanarPositionDifferenceCompletion;
	struct FPlanarPositionSimilarityCompletion;
}

// For functions in this file, we are favoring having more verbose names such as "AddConditionalCompletion" vs simply "Add" in 
// order to keep it easy to find the correct function in blueprints.

//------------------------------------------------------------------

/**
 * The base class for all completions. Completions contain logic that determines if an agent's current episode should
 * end, e.g. because the agent achieved the normal win/loss condition for the game. Additionally, completions can speed
 * up training by ending episodes early if the agent has gotten into a state where training data is no longer useful,
 * e.g. the agent is stuck somewhere. These two modes of completions are expressed with the following enum values:
 *   ELearningAgentsCompletion::Termination - used when the episode ends in an expected way and no further rewards
 *       should be expected, i.e. do not use the value function to estimate future rewards.
 *   ELearningAgentsCompletion::Truncation - used when the episode ends in an unexpected way, mainly to speed up the 
 *       training process. The agent should expect additional rewards if training were to continue, so it should use
 *       its value function to estimate future rewards.
 */
UCLASS(Abstract, BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsCompletion : public UObject
{
	GENERATED_BODY()

public:

	/** Reference to the Trainer this completion is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsTrainer> AgentTrainer;

public:

	/** Initialize the internal state for a given maximum number of agents */
	void Init(const int32 MaxAgentNum);

	/**
	 * Called whenever agents are added to the associated ULearningAgentsTrainer object.
	 * @param AgentIds Array of agent ids which have been added
	 */
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are removed from the associated ULearningAgentsTrainer object.
	 * @param AgentIds Array of agent ids which have been removed
	 */
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are reset on the associated ULearningAgentsTrainer object.
	 * @param AgentIds Array of agent ids which have been reset
	 */
	virtual void OnAgentsReset(const TArray<int32>& AgentIds);

	/** Get the number of times a completion has been set for the given agent id. */
	uint64 GetAgentIteration(const int32 AgentId) const;

public:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Color used to draw this completion in the visual log */
	FLinearColor VisualLogColor = FColor::Yellow;

	/** Describes this completion to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const {}
#endif

protected:

	/** Number of times this completion has been set for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> AgentIteration;
};

//------------------------------------------------------------------

/** A simple boolean completion. Used as a catch-all for situations where a more type-specific completion does not exist yet. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UConditionalCompletion : public ULearningAgentsCompletion
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new conditional completion to the given trainer. Call during ULearningAgentsTrainer::SetupCompletions event.
	 * @param InAgentTrainer The trainer to add this completion to.
	 * @param Name The name of this new completion. Used for debugging.
	 * @param InCompletionMode The completion mode.
	 * @return The newly created completion.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UConditionalCompletion* AddConditionalCompletion(ULearningAgentsTrainer* InAgentTrainer, const FName Name = NAME_None, const ELearningAgentsCompletion InCompletionMode = ELearningAgentsCompletion::Termination);

	/**
	 * Sets the data for this completion. Call during ULearningAgentsTrainer::SetCompletions event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param bIsCompleted Pass in true if condition is met. Otherwise, false.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetConditionalCompletion(const int32 AgentId, const bool bIsCompleted);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this completion to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FConditionalCompletion> CompletionObject;
};

//------------------------------------------------------------------

/** A completion for if a given amount of time has elapsed. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UTimeElapsedCompletion : public ULearningAgentsCompletion
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new time elapsed completion to the given trainer. Call during ULearningAgentsTrainer::SetupCompletions event.
	 * @param InAgentTrainer The trainer to add this completion to.
	 * @param Name The name of this new completion. Used for debugging.
	 * @param Threshold How much time should be elapsed for the completion to trigger.
	 * @param InCompletionMode The completion mode.
	 * @return The newly created completion.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UTimeElapsedCompletion* AddTimeElapsedCompletion(ULearningAgentsTrainer* InAgentTrainer, const FName Name = NAME_None, const float Threshold = 10.0f, const ELearningAgentsCompletion InCompletionMode = ELearningAgentsCompletion::Termination);

	/**
	 * Sets the data for this completion. Call during ULearningAgentsTrainer::SetCompletions event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Time The amount of time that has passed
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetTimeElapsedCompletion(const int32 AgentId, const float Time);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this completion to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FTimeElapsedCompletion> CompletionObject;
};

//------------------------------------------------------------------

/**
 * A completion for if two positions differ by some threshold in a plane, e.g. if the agent gets too far from a
 * starting position.
 */
UCLASS()
class LEARNINGAGENTSTRAINING_API UPlanarPositionDifferenceCompletion : public ULearningAgentsCompletion
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar position difference completion to the given trainer. The axis parameters define the plane.
	 * Call during ULearningAgentsTrainer::SetupCompletions event.
	 * @param InAgentTrainer The trainer to add this completion to.
	 * @param Name The name of this new completion. Used for debugging.
	 * @param Threshold If the distance becomes greater than this threshold, then the episode will complete.
	 * @param InCompletionMode The completion mode.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created completion.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UPlanarPositionDifferenceCompletion* AddPlanarPositionDifferenceCompletion(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Threshold = 100.0f,
		const ELearningAgentsCompletion InCompletionMode = ELearningAgentsCompletion::Termination,
		const FVector Axis0 = FVector::ForwardVector,
		const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this completion. Call during ULearningAgentsTrainer::SetCompletions event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position0 The first position.
	 * @param Position1 The second position.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarPositionDifferenceCompletion(const int32 AgentId, const FVector Position0, const FVector Position1);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this completion to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarPositionDifferenceCompletion> CompletionObject;
};

/**
 * A completion for if two positions are near by some threshold in a plane, e.g. if the agent gets close to a position.
 */
UCLASS()
class LEARNINGAGENTSTRAINING_API UPlanarPositionSimilarityCompletion : public ULearningAgentsCompletion
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar position difference completion to the given trainer. The axis parameters define the plane.
	 * Call during ULearningAgentsTrainer::SetupCompletions event.
	 * @param InAgentTrainer The trainer to add this completion to.
	 * @param Name The name of this new completion. Used for debugging.
	 * @param Threshold If the distance becomes greater than this threshold, then the episode will complete.
	 * @param InCompletionMode The completion mode.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created completion.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UPlanarPositionSimilarityCompletion* AddPlanarPositionSimilarityCompletion(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Threshold = 100.0f,
		const ELearningAgentsCompletion InCompletionMode = ELearningAgentsCompletion::Termination,
		const FVector Axis0 = FVector::ForwardVector,
		const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this completion. Call during ULearningAgentsTrainer::SetCompletions event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position0 The first position.
	 * @param Position1 The second position.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarPositionSimilarityCompletion(const int32 AgentId, const FVector Position0, const FVector Position1);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this completion to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarPositionSimilarityCompletion> CompletionObject;
};
