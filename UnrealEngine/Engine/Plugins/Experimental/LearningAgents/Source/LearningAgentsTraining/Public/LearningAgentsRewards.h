// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAgentsDebug.h"

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "LearningAgentsRewards.generated.h"

namespace UE::Learning
{
	struct FRewardObject;
	struct FFloatReward;
	struct FConditionalConstantReward;
	struct FScalarVelocityReward;
	struct FLocalDirectionalVelocityReward;
	struct FPlanarPositionDifferencePenalty;
	struct FPositionDifferencePenalty;
	struct FPositionArraySimilarityReward;
}

class ULearningAgentsTrainer;

// For functions in this file, we are favoring having more verbose names such as "AddFloatReward" vs simply "Add" in 
// order to keep it easy to find the correct function in blueprints.

//------------------------------------------------------------------

/**
 * Base class for all rewards/penalties. Rewards are used during reinforcement learning to encourage/discourage
 * certain behaviors from occurring.
 */
UCLASS(Abstract, BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsReward : public UObject
{
	GENERATED_BODY()

public:

	/** Reference to the Trainer this reward is associated with. */
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

	/** Get the number of times a reward has been set for the given agent id. */
	uint64 GetAgentIteration(const int32 AgentId) const;

public:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Color used to draw this reward in the visual log */
	FLinearColor VisualLogColor = FColor::Green;

	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const {}
#endif

protected:

	/** Number of times this reward has been set for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> AgentIteration;
};

//------------------------------------------------------------------

/** A simple float reward. Used as a catch-all for situations where a more type-specific reward does not exist yet. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UFloatReward : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new float reward to the given trainer. Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this reward to.
	 * @param Name The name of this new reward. Used for debugging.
	 * @param Weight Multiplier for this reward when being summed up for the total reward.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UFloatReward* AddFloatReward(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Weight = 1.0f);

	/**
	 * Sets the data for this reward. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Reward The value currently being rewarded.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetFloatReward(const int32 AgentId, const float Reward);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatReward> RewardObject;
};

//------------------------------------------------------------------

/** A simple conditional reward that gives some constant reward value when a condition is true. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UConditionalReward : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new conditional reward to the given trainer. Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this reward to.
	 * @param Name The name of this new reward. Used for debugging.
	 * @param Value The amount of reward to give the agent when the provided condition is true.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UConditionalReward* AddConditionalReward(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Value = 1.0f);

	/**
	 * Sets if the agent should receive a reward. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param bCondition If the agent should receive a reward this iteration.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetConditionalReward(const int32 AgentId, const bool bCondition);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FConditionalConstantReward> RewardObject;
};

//------------------------------------------------------------------

/** A reward for maximizing speed. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UScalarVelocityReward : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new scalar velocity reward to the given trainer. Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this reward to.
	 * @param Name The name of this new reward. Used for debugging.
	 * @param Weight Multiplier for this reward when being summed up for the total reward.
	 * @param Scale Used to normalize the data for the reward.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UScalarVelocityReward* AddScalarVelocityReward(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Weight = 1.0f,
		const float Scale = 200.0f);

	/**
	 * Sets the data for this reward. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The current scalar velocity.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetScalarVelocityReward(const int32 AgentId, const float Velocity);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FScalarVelocityReward> RewardObject;
};

//------------------------------------------------------------------

/** A reward for maximizing velocity along a given local axis. */
UCLASS()
class LEARNINGAGENTSTRAINING_API ULocalDirectionalVelocityReward : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new directional velocity reward to the given trainer. Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this reward to.
	 * @param Name The name of this new reward. Used for debugging.
	 * @param Weight Multiplier for this reward when being summed up for the total reward.
	 * @param Scale Used to normalize the data for the reward.
	 * @param Axis The local direction we want to maximize velocity in.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static ULocalDirectionalVelocityReward* AddLocalDirectionalVelocityReward(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Weight = 1.0f,
		const float Scale = 200.0f,
		const FVector Axis = FVector::ForwardVector);

	/**
	 * Sets the data for this reward. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The current velocity.
	 * @param RelativeRotation The frame of reference rotation.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetLocalDirectionalVelocityReward(
		const int32 AgentId,
		const FVector Velocity,
		const FRotator RelativeRotation = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FLocalDirectionalVelocityReward> RewardObject;
};

//------------------------------------------------------------------

/** A penalty for being far from a goal position in a plane. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UPlanarPositionDifferencePenalty : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar difference penalty to the given trainer. The axis parameters define the plane.
	 * Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this penalty to.
	 * @param Name The name of this new penalty. Used for debugging.
	 * @param Weight Multiplier for this penalty when being summed up for the total reward.
	 * @param Scale Used to normalize the data for the penalty.
	 * @param Threshold Minimal distance to apply this penalty.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UPlanarPositionDifferencePenalty* AddPlanarPositionDifferencePenalty(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const float Weight = 1.0f,
		const float Scale = 100.0f,
		const float Threshold = 0.0f,
		const FVector Axis0 = FVector::ForwardVector,
		const FVector Axis1 = FVector::RightVector);

	/**
	 * Sets the data for this penalty. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Position0 The current position.
	 * @param Position1 The goal position.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarPositionDifferencePenalty(const int32 AgentId, const FVector Position0, const FVector Position1);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarPositionDifferencePenalty> RewardObject;
};

/** A reward for minimizing the distances of positions in the given arrays. */
UCLASS()
class LEARNINGAGENTSTRAINING_API UPositionArraySimilarityReward : public ULearningAgentsReward
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new position array similarity reward to the given trainer. Call during ULearningAgentsTrainer::SetupRewards event.
	 * @param InAgentTrainer The trainer to add this reward to.
	 * @param Name The name of this new reward. Used for debugging.
	 * @param PositionNum The number of positions in the array.
	 * @param Scale Used to normalize the data for the reward.
	 * @param Weight Multiplier for this reward when being summed up for the total reward.
	 * @return The newly created reward.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InAgentTrainer"))
	static UPositionArraySimilarityReward* AddPositionArraySimilarityReward(
		ULearningAgentsTrainer* InAgentTrainer,
		const FName Name = NAME_None,
		const int32 PositionNum = 0,
		const float Scale = 100.0f,
		const float Weight = 1.0f);

	/**
	 * Sets the data for this reward. Call during ULearningAgentsTrainer::SetRewards event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Positions0 The current positions.
	 * @param Positions1 The goal positions.
	 * @param RelativePosition0 The vector Positions0 will be offset from.
	 * @param RelativePosition1 The vector Positions1 will be offset from.
	 * @param RelativeRotation0 The frame of reference rotation for Positions0.
	 * @param RelativeRotation1 The frame of reference rotation for Positions1.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPositionArraySimilarityReward(
		const int32 AgentId,
		const TArray<FVector>& Positions0, 
		const TArray<FVector>& Positions1, 
		const FVector RelativePosition0 = FVector::ZeroVector,
		const FVector RelativePosition1 = FVector::ZeroVector,
		const FRotator RelativeRotation0 = FRotator::ZeroRotator,
		const FRotator RelativeRotation1 = FRotator::ZeroRotator);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this reward to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPositionArraySimilarityReward> RewardObject;
};
