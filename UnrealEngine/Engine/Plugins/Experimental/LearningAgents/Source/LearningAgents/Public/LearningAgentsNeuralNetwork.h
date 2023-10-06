// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "LearningAgentsNeuralNetwork.generated.h"

namespace UE::Learning
{
	struct FNeuralNetwork;
	enum class EActivationFunction : uint8;
}

/** Activation functions for neural networks. */
UENUM(BlueprintType, Category = "LearningAgents")
enum class ELearningAgentsActivationFunction : uint8
{
	/** ReLU Activation - Fast to train and evaluate but occasionally causes gradient collapse and untrainable networks. */
	ReLU	UMETA(DisplayName = "ReLU"),

	/** ELU Activation - Generally performs better than ReLU and is not prone to gradient collapse but slower to evaluate. */
	ELU		UMETA(DisplayName = "ELU"),

	/** TanH Activation - Smooth activation function that is slower to train and evaluate but sometimes more stable for certain tasks. */
	TanH	UMETA(DisplayName = "TanH"),
};

namespace UE::Learning::Agents
{
	/** Get the learning agents activation function from the UE::Learning activation function. */
	LEARNINGAGENTS_API ELearningAgentsActivationFunction GetLearningAgentsActivationFunction(const EActivationFunction ActivationFunction);

	/** Get the UE::Learning activation function from the learning agents activation function. */
	LEARNINGAGENTS_API EActivationFunction GetActivationFunction(const ELearningAgentsActivationFunction ActivationFunction);
}

/** A neural network data asset. */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsNeuralNetwork : public UDataAsset
{
	GENERATED_BODY()

public:
	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsNeuralNetwork();
	ULearningAgentsNeuralNetwork(FVTableHelper& Helper);
	virtual ~ULearningAgentsNeuralNetwork();

	/** Serialize this neural network to/from the given archive. */
	virtual void Serialize(FArchive& Ar) override;

	/**
	 * Resets this network asset to be empty.
	 */
	UFUNCTION(CallInEditor, Category = "LearningAgents")
	void ResetNetwork();

	/**
	 * Load this network from a snapshot.
	 * @param File The snapshot file.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void LoadNetworkFromSnapshot(const FFilePath& File);

	/**
	 * Save this network into a snapshot.
	 * @param File The snapshot file.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void SaveNetworkToSnapshot(const FFilePath& File);

	/**
	 * Copy another asset's network weights into this network. Network sizes must match.
	 * @param NeuralNetworkAsset The asset to load from.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void LoadNetworkFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset);

	/**
	 * Copy this network's weights to another asset. Network sizes must match.
	 * @param NeuralNetworkAsset The asset to save to.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", Meta = (DevelopmentOnly))
	void SaveNetworkToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset);

public:

	/** Marks this asset as modified even during PIE */
	void ForceMarkDirty();

public:

	/** Pointer to the internal Neural Network Data */
	TSharedPtr<UE::Learning::FNeuralNetwork> NeuralNetwork;
};
