// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsTrainer.h" // Included for ELearningAgentsTrainerDevice and FLearningAgentsTrainerPathSettings
#include "LearningArray.h"

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

#include "LearningAgentsImitationTrainer.generated.h"

namespace UE::Learning
{
	struct FSharedMemoryImitationTrainer;
}

class ULearningAgentsPolicy;
class ULearningAgentsRecording;

/** The configurable settings for a ULearningAgentsImitationTrainer. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsImitationTrainerSettings
{
	GENERATED_BODY()

public:

	/** Time in seconds to wait for the training process before timing out. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TrainerCommunicationTimeout = 20.0f;
};

/** The configurable settings for the training process. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsImitationTrainerTrainingSettings
{
	GENERATED_BODY()

public:

	/** The number of iterations to run before ending training. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumberOfIterations = 1000000;

	/** Learning rate of the policy network. Typical values are between 0.001 and 0.0001. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRate = 0.0001f;

	/** Ratio by which to decay the learning rate every 1000 iterations. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRateDecay = 0.99f;

	/**
	 * Amount of weight decay to apply to the network. Larger values encourage network weights to be smaller but too
	 * large a value can cause the network weights to collapse to all zeros.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WeightDecay = 0.001f;

	/**
	 * Batch size to use for training. Smaller values tend to produce better results at the cost of slowing down
	 * training. Large batch sizes are much more computationally efficient when training on the GPU.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", ClampMax = "4096", UIMin = "0", UIMax = "4096"))
	uint32 BatchSize = 128;

	/** The seed used for any random sampling the trainer will perform, e.g. for weight initialization. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 RandomSeed = 1234;

	/** The device to train on. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	ELearningAgentsTrainerDevice Device = ELearningAgentsTrainerDevice::CPU;

	/** If true, TensorBoard logs will be emitted to the intermediate directory. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseTensorboard = false;
};

/**
 * The ULearningAgentsImitationTrainer enable imitation learning, i.e. learning from human/AI demonstrations.
 * Imitation training is typically much faster than reinforcement learning, but requires gathering large amounts of
 * data in order to generalize. This can be used to initialize a reinforcement learning policy to speed up initial
 * exploration.
 * @see ULearningAgentsInteractor to understand how observations and actions work.
 * @see ULearningAgentsController to understand how we can manually perform actions via a human or AI.
 * @see ULearningAgentsRecorder to understand how to make new recordings.
 */
UCLASS(BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ULearningAgentsImitationTrainer : public UActorComponent
{
	GENERATED_BODY()

// ----- Setup -----
public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsImitationTrainer();
	ULearningAgentsImitationTrainer(FVTableHelper& Helper);
	virtual ~ULearningAgentsImitationTrainer();

	/** Will automatically call EndTraining if training is still in-progress when play is ending. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns true if the trainer is currently training; Otherwise, false. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool IsTraining() const;

	/**
	 * Returns true if the trainer has failed to communicate with the external training process. This can be used in
	 * combination with RunTraining to avoid filling the logs with errors.
	 *
	 * @returns				True if the training has failed. Otherwise, false.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool HasTrainingFailed() const;

	/**
	 * Begins the training process with the provided settings.
	 * @param InPolicy The policy to train.
	 * @param Recording The data to train on.
	 * @param ImitationTrainerSettings The settings for this trainer.
	 * @param ImitationTrainerTrainingSettings The training settings for this network.
	 * @param ImitationTrainerPathSettings The path settings used by the imitation trainer.
	 * @param bReinitializePolicyNetwork If true, reinitialize the policy. Set this to false if your policy is pre-trained.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void BeginTraining(
		ULearningAgentsPolicy* InPolicy,
		const ULearningAgentsRecording* Recording,
		const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings = FLearningAgentsImitationTrainerSettings(),
		const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings = FLearningAgentsImitationTrainerTrainingSettings(),
		const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const bool bReinitializePolicyNetwork = true);

	/** Stops the training process. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EndTraining();

	/** Iterates the training process and gets the updated policy network. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void IterateTraining();

	/**
	 * Convenience function that runs a basic training loop. If training has not been started, it will start it. On 
	 * each following call to this function, it will call IterateTraining.
	 * @param InPolicy The policy to train.
	 * @param Recording The data to train on.
	 * @param ImitationTrainerSettings The settings for this trainer.
	 * @param ImitationTrainerTrainingSettings The training settings for this network.
	 * @param ImitationTrainerPathSettings The path settings used by the imitation trainer.
	 * @param bReinitializePolicyNetwork If true, reinitialize the policy. Set this to false if your policy is pre-trained.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void RunTraining(
		ULearningAgentsPolicy* InPolicy,
		const ULearningAgentsRecording* Recording,
		const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings = FLearningAgentsImitationTrainerSettings(),
		const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings = FLearningAgentsImitationTrainerTrainingSettings(),
		const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const bool bReinitializePolicyNetwork = true);

// ----- Private Data -----
private:

	/** The policy being trained. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsPolicy> Policy;

	/** True if training is currently in-progress. Otherwise, false. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bIsTraining = false;

	/**
	 * True if trainer encountered an unrecoverable error during training (e.g. the trainer process timed out). Otherwise, false.
	 * This exists mainly to keep the editor from locking up if something goes wrong during training.
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bHasTrainingFailed = false;

	float TrainerTimeout = 10.0f;

	void DoneTraining();

	TLearningArray<2, float> RecordedObservations;
	TLearningArray<2, float> RecordedActions;

	TUniquePtr<UE::Learning::FSharedMemoryImitationTrainer> ImitationTrainer;
};
