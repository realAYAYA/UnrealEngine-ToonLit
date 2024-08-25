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

class ULearningAgentsInteractor;
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
	float TrainerCommunicationTimeout = 10.0f;
};

/** The configurable settings for the training process. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsImitationTrainerTrainingSettings
{
	GENERATED_BODY()

public:

	/** The number of iterations to run before ending training. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 NumberOfIterations = 1000000;

	/** Learning rate of the policy network. Typical values are between 0.001 and 0.0001. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRate = 0.001f;

	/** Amount by which to multiply the learning rate every 1000 iterations. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRateDecay = 1.0f;

	/**
	 * Amount of weight decay to apply to the network. Larger values encourage network weights to be smaller but too
	 * large a value can cause the network weights to collapse to all zeros.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float WeightDecay = 0.0001f;

	/**
	 * Batch size to use for training. Smaller values tend to produce better results at the cost of slowing down
	 * training. Large batch sizes are much more computationally efficient when training on the GPU.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "4096"))
	uint32 BatchSize = 128;

	/**
	 * The number of consecutive steps of observations and actions over which to train the policy. Increasing this value will encourage the policy to use its memory 
	 * effectively. Too large and training can become unstable. Given we don't know the memory state during imitation learning it is better this is 
	 * slightly larger than when we are doing reinforcement learning.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "512"))
	uint32 Window = 64;

	/**
	 * Weight used to regularize actions. Larger values will encourage smaller actions but too large will cause actions to become always zero.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActionRegularizationWeight = 0.001f;

	/**
	 * Weighting used for the entropy bonus. Larger values encourage larger action noise and therefore greater exploration but can make actions very 
	 * noisy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ActionEntropyWeight = 0.0f;

	/** The seed used for any random sampling the trainer will perform, e.g. for weight initialization. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 RandomSeed = 1234;

	/** The device to train on. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	ELearningAgentsTrainerDevice Device = ELearningAgentsTrainerDevice::CPU;

	/** If true, TensorBoard logs will be emitted to the intermediate directory. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseTensorboard = false;

	/** If true, snapshots of the trained networks will be emitted to the intermediate directory. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bSaveSnapshots = false;
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
class LEARNINGAGENTSTRAINING_API ULearningAgentsImitationTrainer : public ULearningAgentsManagerListener
{
	GENERATED_BODY()

// ----- Setup -----
public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsImitationTrainer();
	ULearningAgentsImitationTrainer(FVTableHelper& Helper);
	virtual ~ULearningAgentsImitationTrainer();

	/** Will automatically call EndTraining if training is still in-progress when the object is destroyed. */
	virtual void BeginDestroy() override;

	/**
	 * Constructs the imitation trainer and runs the setup functions.
	 * 
	 * @param InManager			The agent manager we are using.
	 * @param InInteractor		The agent interactor we are recording with.
	 * @param InPolicy			The policy we are using.
	 * @param Class				The imitation trainer class
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (Class = "/Script/LearningAgentsTraining.LearningAgentsImitationTrainer", DeterminesOutputType = "Class"))
	static ULearningAgentsImitationTrainer* MakeImitationTrainer(
		ULearningAgentsManager* InManager,
		ULearningAgentsInteractor* InInteractor,
		ULearningAgentsPolicy* InPolicy,
		TSubclassOf<ULearningAgentsImitationTrainer> Class);

	/**
	 * Initializes the imitation trainer and runs the setup functions.
	 * 
	 * @param InManager			The agent manager we are using.
	 * @param InInteractor		The agent interactor we are recording with.
	 * @param InPolicy			The policy we are using.
	 */
	void SetupImitationTrainer(
		ULearningAgentsManager* InManager,
		ULearningAgentsInteractor* InInteractor,
		ULearningAgentsPolicy* InPolicy);

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
	 * 
	 * @param Recording							The data to train on.
	 * @param ImitationTrainerSettings			The settings for this trainer.
	 * @param ImitationTrainerTrainingSettings	The training settings for this network.
	 * @param ImitationTrainerPathSettings		The path settings used by the imitation trainer.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AutoCreateRefTerm = "ImitationTrainerSettings,ImitationTrainerTrainingSettings,ImitationTrainerPathSettings"))
	void BeginTraining(
		const ULearningAgentsRecording* Recording,
		const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings = FLearningAgentsImitationTrainerSettings(),
		const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings = FLearningAgentsImitationTrainerTrainingSettings(),
		const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings = FLearningAgentsTrainerPathSettings());

	/** Stops the training process. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EndTraining();

	/** Iterates the training process and gets the updated policy network. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void IterateTraining();

	/**
	 * Convenience function that runs a basic training loop. If training has not been started, it will start it. On 
	 * each following call to this function, it will call IterateTraining.
	 * 
	 * @param Recording							The data to train on.
	 * @param ImitationTrainerSettings			The settings for this trainer.
	 * @param ImitationTrainerTrainingSettings	The training settings for this network.
	 * @param ImitationTrainerPathSettings		The path settings used by the imitation trainer.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AutoCreateRefTerm = "ImitationTrainerSettings,ImitationTrainerTrainingSettings,ImitationTrainerPathSettings"))
	void RunTraining(
		const ULearningAgentsRecording* Recording,
		const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings = FLearningAgentsImitationTrainerSettings(),
		const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings = FLearningAgentsImitationTrainerTrainingSettings(),
		const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings = FLearningAgentsTrainerPathSettings());

// ----- Private Data -----
private:

	/** The interactor being trained. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

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

	TUniquePtr<UE::Learning::FSharedMemoryImitationTrainer> ImitationTrainer;
};
