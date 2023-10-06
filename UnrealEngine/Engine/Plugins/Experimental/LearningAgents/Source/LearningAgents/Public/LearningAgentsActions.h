// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAgentsDebug.h"

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "LearningAgentsActions.generated.h"

namespace UE::Learning
{
	struct FFeatureObject;
	struct FFloatFeature;
	struct FPlanarVelocityFeature;
	struct FRotationVectorFeature;
}

class ULearningAgentsInteractor;

// For functions in this file, we are favoring having more verbose names such as "AddFloatAction" vs simply "Add" in 
// order to keep it easy to find the correct function in blueprints.

//------------------------------------------------------------------

/**
 * The base class for all actions. Actions define the outputs from your agents. Action getters are marked non-pure by
 * convention as many of them do non-trivial amounts of work that can cause performance issues when marked pure in 
 * blueprints.
 */
UCLASS(Abstract, BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsAction : public UObject
{
	GENERATED_BODY()

public:

	/** Reference to the Interactor this action is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

public:

	/** Initialize the internal state for a given maximum number of agents */
	void Init(const int32 MaxAgentNum);

	/**
	 * Called whenever agents are added to the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been added
	 */
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are removed from the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been removed
	 */
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are reset on the associated ULearningAgentsInteractor object.
	 * @param AgentIds Array of agent ids which have been reset
	 */
	virtual void OnAgentsReset(const TArray<int32>& AgentIds);

	/** Get the number of times an action has been got for the given agent id. */
	uint64 GetAgentGetIteration(const int32 AgentId) const;

	/** Get the number of times an action has been set for the given agent id. */
	uint64 GetAgentSetIteration(const int32 AgentId) const;

public:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Color used to draw this action in the visual log */
	FLinearColor VisualLogColor = FColor::Blue;

	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const {}
#endif

protected:

	/** Number of times this action has been got for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> AgentGetIteration;

	/** Number of times this action has been set for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> AgentSetIteration;
};

//------------------------------------------------------------------

/** A simple float action. Used as a catch-all for situations where a more type-specific action does not exist yet. */
UCLASS()
class LEARNINGAGENTS_API UFloatAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new float action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UFloatAction* AddFloatAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Gets the data for this action. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action value.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetFloatAction(const int32 AgentId);

	/**
	 * Sets the data for this action. Call during ULearningAgentsController::SetActions event.
	 * @param AgentId The agent id to set data for.
	 * @param Value The current action value.
	 */	
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetFloatAction(const int32 AgentId, const float Value);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

/** A simple float array action. Used as a catch-all for situations where a more type-specific action does not exist yet. */
UCLASS()
class LEARNINGAGENTS_API UFloatArrayAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new float array action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Num The number of floats in the array
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UFloatArrayAction* AddFloatArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 Num = 1, const float Scale = 1.0f);

	/**
	 * Gets the data for this action. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @param OutValues The output array of floats
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetFloatArrayAction(const int32 AgentId, TArray<float>& OutValues);

	/**
	 * Sets the data for this action. Call during ULearningAgentsController::SetActions event.
	 * @param AgentId The agent id to set data for.
	 * @param Values The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetFloatArrayAction(const int32 AgentId, const TArray<float>& Values);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

//------------------------------------------------------------------

/** A simple FVector action. */
UCLASS()
class LEARNINGAGENTS_API UVectorAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new vector action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVectorAction* AddVectorAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 1.0f);

	/**
	 * Gets the data for this action. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetVectorAction(const int32 AgentId);

	/**
	 * Sets the data for this action. Call during ULearningAgentsController::SetActions event.
	 * @param AgentId The agent id to set data for.
	 * @param Value The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVectorAction(const int32 AgentId, const FVector Value);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

/** A simple array of FVector action. */
UCLASS()
class LEARNINGAGENTS_API UVectorArrayAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new vector action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Num The number of vectors in the array
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UVectorArrayAction* AddVectorArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 Num = 1, const float Scale = 1.0f);

	/**
	 * Gets the data for this action. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @param OutVectors The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetVectorArrayAction(const int32 AgentId, TArray<FVector>& OutVectors);

	/**
	 * Sets the data for this action. Call during ULearningAgentsController::SetActions event.
	 * @param AgentId The agent id to set data for.
	 * @param Vectors The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetVectorArrayAction(const int32 AgentId, const TArray<FVector>& Vectors);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FFloatFeature> FeatureObject;
};

//------------------------------------------------------------------

/** A planar velocity action. */
UCLASS()
class LEARNINGAGENTS_API UPlanarVelocityAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new planar velocity action to the given agent interactor. The axis parameters define the plane.
	 * Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Scale Used to normalize the data for the action.
	 * @param Axis0 The forward axis of the plane.
	 * @param Axis1 The right axis of the plane.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static UPlanarVelocityAction* AddPlanarVelocityAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 200.0f, const FVector Axis0 = FVector::ForwardVector, const FVector Axis1 = FVector::RightVector);

	/**
	 * Gets the data for this action. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action value.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetPlanarVelocityAction(const int32 AgentId);

	/**
	 * Sets the data for this action. Call during ULearningAgentsController::SetActions event.
	 * @param AgentId The agent id this data corresponds to.
	 * @param Velocity The velocity currently being observed.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetPlanarVelocityAction(const int32 AgentId, const FVector Velocity);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FPlanarVelocityFeature> FeatureObject;
};

/** A rotation action. */
UCLASS()
class LEARNINGAGENTS_API URotationAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new rotation action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static URotationAction* AddRotationAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const float Scale = 180.0f);

	/**
	 * Gets the data for this action as a rotator. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action value.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	FRotator GetRotationAction(const int32 AgentId);

	/**
	 * Gets the data for this action as a rotation vector. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action value.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetRotationActionAsRotationVector(const int32 AgentId);

	/**
	 * Gets the data for this action as a quaternion. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @return The current action value.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	FQuat GetRotationActionAsQuat(const int32 AgentId);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FRotationVectorFeature> FeatureObject;
};

/** An array of rotation actions. */
UCLASS()
class LEARNINGAGENTS_API URotationArrayAction : public ULearningAgentsAction
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new rotation array action to the given agent interactor. Call during ULearningAgentsInteractor::SetupActions event.
	 * @param InInteractor The agent interactor to add this action to.
	 * @param Name The name of this new action. Used for debugging.
	 * @param RotationNum The number of rotations in the array.
	 * @param Scale Used to normalize the data for the action.
	 * @return The newly created action.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InInteractor"))
	static URotationArrayAction* AddRotationArrayAction(ULearningAgentsInteractor* InInteractor, const FName Name = NAME_None, const int32 RotationNum = 1, const float Scale = 180.0f);

	/**
	 * Gets the data for this action as rotators. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @param OutRotations The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetRotationArrayAction(const int32 AgentId, TArray<FRotator>& OutRotations);

	/**
	 * Gets the data for this action as rotation vectors. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @param OutRotationVectors The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetRotationArrayActionAsRotationVectors(const int32 AgentId, TArray<FVector>& OutRotationVectors);

	/**
	 * Gets the data for this action as quaternions. Call during ULearningAgentsInteractor::GetActions event.
	 * @param AgentId The agent id to get data for.
	 * @param OutRotations The current action values.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetRotationArrayActionAsQuats(const int32 AgentId, TArray<FQuat>& OutRotations);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Describes this action to the visual logger for debugging purposes. */
	virtual void VisualLog(const UE::Learning::FIndexSet Instances) const override;
#endif

	TSharedPtr<UE::Learning::FRotationVectorFeature> FeatureObject;
};
