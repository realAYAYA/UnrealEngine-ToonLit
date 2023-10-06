// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerComponent.h"

#include "LearningAgentsNeuralNetwork.h" // Included for ELearningAgentsActivationFunction
#include "LearningAgentsDebug.h"
#include "LearningArray.h"
#include "UObject/ObjectPtr.h"

#include "LearningAgentsPolicy.generated.h"

namespace UE::Learning
{
	struct FNeuralNetwork;
	struct FNeuralNetworkPolicyFunction;
}

struct FFilePath;
class ULearningAgentsNeuralNetwork;

/** The configurable settings for a ULearningAgentsPolicy. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTS_API FLearningAgentsPolicySettings
{
	GENERATED_BODY()

public:

	/** Seed for the action noise used by the policy  */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 ActionNoiseSeed = 1234;

	/** Minimum action noise used by the policy */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActionNoiseMin = 0.25f;

	/** Maximum action noise used by the policy */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActionNoiseMax = 0.25f;

	/** Scale of the action noise used by the policy should be 1.0 during training. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActionNoiseScale = 1.0f;

	/** Total layers for policy network including input, hidden, and output layers */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "2", UIMin = "2"))
	int32 LayerNum = 3;

	/** Number of neurons in each hidden layer of the policy network */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 HiddenLayerSize = 128;

	/** Activation function to use on hidden layers of the policy network */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	ELearningAgentsActivationFunction ActivationFunction = ELearningAgentsActivationFunction::ELU;
};

/** A policy that maps from observations to actions for the managed agents. */
UCLASS(BlueprintType, Blueprintable)
class LEARNINGAGENTS_API ULearningAgentsPolicy : public ULearningAgentsManagerComponent
{
	GENERATED_BODY()

public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsPolicy();
	ULearningAgentsPolicy(FVTableHelper& Helper);
	virtual ~ULearningAgentsPolicy();

	/**
	 * Initializes this object to be used with the given agent interactor and policy settings.
	 * @param InInteractor The input Interactor component
	 * @param PolicySettings The policy settings to use
	 * @param NeuralNetworkAsset Optional Network Asset to use. If provided must match the given PolicySettings. If not
	 * provided or asset is empty then a new neural network object will be created according to the given 
	 * PolicySettings and used.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetupPolicy(ULearningAgentsInteractor* InInteractor, 
		const FLearningAgentsPolicySettings& PolicySettings = FLearningAgentsPolicySettings(),
		ULearningAgentsNeuralNetwork* NeuralNetworkAsset = nullptr);

// ----- Load / Save -----
public:

	/**
	 * Load a snapshot's weights into this policy.
	 * @param File The snapshot file.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void LoadPolicyFromSnapshot(const FFilePath& File);

	/**
	 * Save this policy's weights into a snapshot.
	 * @param File The snapshot file.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LearningAgents", meta = (RelativePath))
	void SavePolicyToSnapshot(const FFilePath& File) const;

	/**
	 * Use a ULearningAgentsNeuralNetwork asset directly for this critic rather than making a copy. If used
	 * during training then this asset's weights will be updated as training progresses.
	 * @param NeuralNetworkAsset The asset to use.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void UsePolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset);

	/**
	 * Load a ULearningAgentsNeuralNetwork asset's weights into this policy.
	 * @param NeuralNetworkAsset The asset to load from.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void LoadPolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset);

	/**
	 * Save this policy's weights to a ULearningAgentsNeuralNetwork asset.
	 * @param NeuralNetworkAsset The asset to save to.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", Meta = (DevelopmentOnly))
	void SavePolicyToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset);

// ----- Evaluation -----
public:

	/**
	 * Calling this function will run the underlying neural network on the previously buffered observations to populate
	 * the output action buffer. This should be called after the associated agent interactor's EncodeObservations and
	 * before its DecodeActions.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EvaluatePolicy();

	/**
	 * Calls EncodeObservations, followed by EvaluatePolicy, followed by DecodeActions
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void RunInference();

	/** Get the action noise scale used. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	float GetActionNoiseScale() const;

	/** Set the action noise scale used. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetActionNoiseScale(const float ActionNoiseScale);

// ----- Non-blueprint public interface -----
public:

	/** Gets the current Network Asset being used */
	ULearningAgentsNeuralNetwork* GetNetworkAsset();

	/** Get a reference to this policy's neural network. */
	UE::Learning::FNeuralNetwork& GetPolicyNetwork();

	/** Get a reference to this policy's policy function object. */
	UE::Learning::FNeuralNetworkPolicyFunction& GetPolicyObject();

// ----- Private Data -----
private:

	/** The agent interactor this policy is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

	/** The underlying neural network. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsNeuralNetwork> Network;

	/** Internal Policy Function Object */
	TSharedPtr<UE::Learning::FNeuralNetworkPolicyFunction> PolicyObject;

// ----- Private Iteration Checks ----- 
private:

	/** Temp buffers used to record the set of agents that are valid for evaluation */
	TArray<int32> ValidAgentIds;
	UE::Learning::FIndexSet ValidAgentSet;

private:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Color used to draw this action in the visual log */
	FLinearColor VisualLogColor = FColor::Purple;

	/** Describes this policy to the visual logger for debugging purposes. */
	void VisualLog(const UE::Learning::FIndexSet AgentSet) const;
#endif
};
