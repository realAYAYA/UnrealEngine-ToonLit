// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerListener.h"
#include "LearningAgentsCompletions.h"

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "Engine/EngineTypes.h"

#include "LearningAgentsTrainer.generated.h"

namespace UE::Learning
{
	struct FEpisodeBuffer;
	struct FReplayBuffer;
	struct FResetInstanceBuffer;
	struct FSharedMemoryPPOTrainer;
	enum class ETrainerDevice : uint8;
}

class ALearningAgentsManager;
class ULearningAgentsInteractor;
class ULearningAgentsPolicy;
class ULearningAgentsCritic;

/** The configurable settings for a ULearningAgentsTrainer. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerSettings
{
	GENERATED_BODY()

public:

	/**
	 * Maximum number of steps recorded in an episode before it is added to the replay buffer. This can generally be left at the default value and 
	 * does not have a large impact on training.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxEpisodeStepNum = 512;

	/**
	 * Maximum number of episodes to record before running a training iteration. An iteration of training will be run when either this or
	 * MaximumRecordedEpisodesPerIteration is reached. Typical values for this should be around 1000. Setting this too small means there is not 
	 * enough data each iteration for the system to train. Setting it too large means training will be very slow. 
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumRecordedEpisodesPerIteration = 1000;

	/**
	 * Maximum number of steps to record before running a training iteration. An iteration of training will be run when either this or
	 * MaximumRecordedEpisodesPerIteration is reached. Typical values for this should be around 10000. Setting this too small means there is not
	 * enough data each iteration for the system to train. Setting it too large means training will be very slow.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumRecordedStepsPerIteration = 10000;

	/** Time in seconds to wait for the training subprocess before timing out. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TrainerCommunicationTimeout = 10.0f;
};

/**
 * The configurable game settings for a ULearningAgentsTrainer. These allow the timestep and physics tick to be fixed
 * during training, which can enable ticking faster than real-time.
 */
USTRUCT(BlueprintType, Category="LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerGameSettings
{
	GENERATED_BODY()

public:

	/**
	 * If true, the game will run in fixed time step mode (i.e the frame's delta times will always be the same
	 * regardless of how much wall time has passed). This can enable faster than real-time training if your game runs
	 * quickly. If false, the time steps will match real wall time.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseFixedTimeStep = true;

	/**
	 * Determines the amount of time for each frame when bUseFixedTimeStep is true; Ignored if false. You want this
	 * time step to match as closely as possible to the expected inference time steps, otherwise your training results
	 * may not generalize to your game.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (DisplayName = "Fixed Time Step Frequency (Hz)"), meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FixedTimeStepFrequency = 60.0f;

	/** If true, set the physics delta time to match the fixed time step. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bSetMaxPhysicsStepToFixedTimeStep = true;

	/** If true, the MaxFPS console variable will be set to a negative number during training; Otherwise, it will not. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bDisableMaxFPS = true;

	/** If true, VSync will be disabled; Otherwise, it will not. Disabling VSync can speed up the game simulation. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bDisableVSync = true;

	/** If true, the viewport rendering will be unlit; Otherwise, it will not. Disabling lighting can speed up the game simulation. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseUnlitViewportRendering = false;

#if WITH_EDITORONLY_DATA

	/** If true, the Use Less CPU In The Background editor setting will be disabled. This prevents the editor from running slowly when minimized. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bDisableUseLessCPUInTheBackground = true;

	/** If true, Editor VSync will be disabled; Otherwise, it will not. Disabling Editor VSync can speed up the game simulation. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bDisableEditorVSync = true;

#endif
};

/** Enumeration of the training devices. */
UENUM(BlueprintType, Category = "LearningAgents")
enum class ELearningAgentsTrainerDevice : uint8
{
	CPU,
	GPU,
};

namespace UE::Learning::Agents
{
	/** Get the learning agents trainer device from the UE::Learning trainer device. */
	LEARNINGAGENTSTRAINING_API ELearningAgentsTrainerDevice GetLearningAgentsTrainerDevice(const ETrainerDevice Device);

	/** Get the UE::Learning trainer device from the learning agents trainer device. */
	LEARNINGAGENTSTRAINING_API ETrainerDevice GetTrainerDevice(const ELearningAgentsTrainerDevice Device);
}

/** The configurable settings for the training process. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerTrainingSettings
{
	GENERATED_BODY()

public:

	/** The number of iterations to run before ending training. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 NumberOfIterations = 1000000;

	/** Learning rate of the policy network. Typical values are between 0.001 and 0.0001. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRatePolicy = 0.0001f;

	/**
	 * Learning rate of the critic network. To avoid instability generally the critic should have a larger learning 
	 * rate than the policy. Typically this can be set to 10x the rate of the policy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRateCritic = 0.001f;

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
	 * Batch size to use for training the policy. Large batch sizes are much more computationally efficient when training on the GPU.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "4096"))
	int32 PolicyBatchSize = 1024;

	/**
	 * Batch size to use for training the critic. Large batch sizes are much more computationally efficient when training on the GPU.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "4096"))
	int32 CriticBatchSize = 4096;

	/**
	 * The number of consecutive steps of observations and actions over which to train the policy. Increasing this value 
	 * will encourage the policy to use its memory effectively. Too large and training can become slow and unstable.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "128"))
	int32 PolicyWindowSize = 16;

	/**
	 * Number of training iterations to perform per buffer of experience gathered. This should be large enough for
	 * the critic and policy to be effectively updated, but too large and it will simply slow down training.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "1024"))
	int32 IterationsPerGather = 32;

	/**
	 * Number of iterations of training to perform to warm - up the Critic. This helps speed up and stabilize training
	 * at the beginning when the Critic may be producing predictions at the wrong order of magnitude.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1", UIMax = "128"))
	int32 CriticWarmupIterations = 8;

	/**
	 * Clipping ratio to apply to policy updates. Keeps the training "on-policy". Larger values may speed up training at 
	 * the cost of stability. Conversely, too small values will keep the policy from being able to learn an 
	 * optimal policy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EpsilonClip = 0.2f;

	/**
	 * Weight used to regularize returns. Encourages the critic not to over or under estimate returns.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float ReturnRegularizationWeight = 0.0001f;

	/**
	 * Weight used to regularize actions. Larger values will encourage exploration and smaller actions, but too large will cause 
	 * noisy actions centered around zero.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float ActionRegularizationWeight = 0.001f;

	/**
	 * Weighting used for the entropy bonus. Larger values encourage larger action noise and therefore greater 
	 * exploration but can make actions very noisy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float ActionEntropyWeight = 0.0f;

	/**
	 * This is used in the Generalized Advantage Estimation, where larger values will tend to assign more credit to recent actions. Typical
	 * values should be between 0.9 and 1.0.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GaeLambda = 0.95f;

	/** When true, advantages are normalized. This tends to makes training more robust to adjustments of the scale of rewards. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bAdvantageNormalization = true;

	/**
	 * The minimum advantage to allow. Setting this below zero will encourage the policy to move away from bad actions, 
	 * but can introduce instability.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (UIMin = "-10.0", UIMax = "0.0"))
	float MinimumAdvantage = 0.0f;

	/**
	 * The maximum advantage to allow. Making this smaller may increase training stability
	 * at the cost of some training speed.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (UIMin = "0.0", UIMax = "10.0"))
	float MaximumAdvantage = 10.0f;

	/**
	 * When true, gradient norm max clipping will be used on the policy, critic, encoder, and decoder. Set this as True if
	 * training is unstable (and adjust GradNormMax) or leave as False if unused.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseGradNormMaxClipping = false;

	/**
	 * The maximum gradient norm to clip updates to. Only used when bUseGradNormMaxClipping is set to true.
	 * 
	 * This needs to be carefully chosen based on the size of your gradients during training. Setting too low can make it
	 * difficult to learn an optimal policy, and too high will have no impact.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (UIMin = "0.0", UIMax = "10.0"))
	float GradNormMax = 0.5f;

	/**
	 * The number of steps to trim from the start of the episode, e.g. can be useful if some things are still getting
	 * setup at the start of the episode and you don't want them used for training.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumberOfStepsToTrimAtStartOfEpisode = 0;

	/**
	 * The number of steps to trim from the end of the episode. Can be useful if the end of the episode contains
	 * irrelevant data.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumberOfStepsToTrimAtEndOfEpisode = 0;

	/** The seed used for any random sampling the trainer will perform, e.g. for weight initialization. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 RandomSeed = 1234;

	/**
	 * The discount factor to use during training. This affects how much the agent cares about future rewards vs
	 * near-term rewards. Should typically be a value less than but near 1.0. 
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DiscountFactor = 0.99f;

	/** The device to train on. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	ELearningAgentsTrainerDevice Device = ELearningAgentsTrainerDevice::GPU;

	/** If true, TensorBoard logs will be emitted to the intermediate directory. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseTensorboard = false;

	/** If true, snapshots of the trained networks will be emitted to the intermediate directory. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bSaveSnapshots = false;
};

/** The path settings for the trainer. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerPathSettings
{
	GENERATED_BODY()

public:

	FLearningAgentsTrainerPathSettings();

	/** The relative path to the engine for editor builds. Defaults to FPaths::EngineDir. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FDirectoryPath EditorEngineRelativePath;

	/**
	 * The relative path to the editor engine folder for non-editor builds.
	 * 
	 * If we want to run training in cooked, non-editor builds, then by default we wont have access to python and the
	 * LearningAgents training scripts - these are editor-only things and are stripped during the cooking process.
	 *
	 * However, running training in non-editor builds can be very important - we probably want to disable rendering
	 * and sound while we are training to make experience gathering as fast as possible - and for any non-trivial game
	 * is simply may not be realistic to run it for a long time in play-in-editor.
	 *
	 * For this reason even in non-editor builds we let you provide the path where all of these editor-only things can 
	 * be found. This allows you to run training when these things actually exist somewhere accessible to the executable, 
	 * which will usually be the case on a normal development machine or cloud machine if it is set up that way.
	 *
	 * Since non-editor builds can be produced in a number of different ways, this is not set by default and cannot 
	 * use a directory picker since it is relative to the final location of where your cooked, non-editor executable 
	 * will exist rather than the current with-editor executable.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	FString NonEditorEngineRelativePath;

	/** The relative path to the Intermediate directory. Defaults to FPaths::ProjectIntermediateDir. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FDirectoryPath EditorIntermediateRelativePath;

	/** The relative path to the intermediate folder for non-editor builds. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	FString NonEditorIntermediateRelativePath;

public:

	/** Gets the Relative Editor Engine Path accounting for if this is an editor build or not  */
	FString GetEditorEnginePath() const;

	/** Gets the Relative Intermediate Path  */
	FString GetIntermediatePath() const;
};

/**
 * The ULearningAgentsTrainer is the core class for reinforcement learning training. It defines how agents rewards, completions, and resets are 
 * implemented and provides methods for orchestrating the training process.
 *
 * To use this class, you need to implement the GatherAgentRewards and GatherAgentCompletions functions, which will define the rewards and penalties 
 * the agent receives and what conditions cause an episode to end.
 *
 * @see ULearningAgentsInteractor to understand how observations and actions work.
 */
UCLASS(Abstract, HideDropdown, BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ULearningAgentsTrainer : public ULearningAgentsManagerListener
{
	GENERATED_BODY()

// ----- Setup -----
public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsTrainer();
	ULearningAgentsTrainer(FVTableHelper& Helper);
	virtual ~ULearningAgentsTrainer();

	/** Will automatically call EndTraining if training is still in-progress when the object is destroyed. */
	virtual void BeginDestroy() override;

	/**
	 * Constructs the trainer and runs the setup functions for rewards and completions.
	 * 
	 * @param InManager			The agent manager we are using.
	 * @param InInteractor		The agent interactor we are training with.
	 * @param InPolicy			The policy to be trained.
	 * @param InCritic			The critic to be trained.
	 * @param Class				The trainer class
	 * @param Name				The trainer name
	 * @param TrainerSettings	The trainer settings to use.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DeterminesOutputType = "Class", AutoCreateRefTerm = "TrainerSettings"))
	static ULearningAgentsTrainer* MakeTrainer(
		ULearningAgentsManager* InManager,
		ULearningAgentsInteractor* InInteractor,
		ULearningAgentsPolicy* InPolicy,
		ULearningAgentsCritic* InCritic,
		TSubclassOf<ULearningAgentsTrainer> Class,
		const FName Name = TEXT("Trainer"),
		const FLearningAgentsTrainerSettings& TrainerSettings = FLearningAgentsTrainerSettings());

	/**
	 * Initializes the trainer and runs the setup functions for rewards and completions.
	 * 
	 * @param InManager The agent manager we are using.
	 * @param InInteractor The agent interactor we are training with.
	 * @param InPolicy The policy to be trained.
	 * @param InCritic The critic to be trained.
	 * @param TrainerSettings The trainer settings to use.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AutoCreateRefTerm = "TrainerSettings"))
	void SetupTrainer(
		ULearningAgentsManager* InManager,
		ULearningAgentsInteractor* InInteractor,
		ULearningAgentsPolicy* InPolicy,
		ULearningAgentsCritic* InCritic,
		const FLearningAgentsTrainerSettings& TrainerSettings = FLearningAgentsTrainerSettings());

public: 

	//~ Begin ULearningAgentsManagerListener Interface
	virtual void OnAgentsAdded_Implementation(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsReset_Implementation(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsManagerTick_Implementation(const TArray<int32>& AgentIds, const float DeltaTime) override;
	//~ End ULearningAgentsManagerListener Interface

// ----- Rewards -----
public:

	/**
	 * This callback should be overridden by the Trainer and gathers the reward value for the given agent.
	 *
	 * @param OutReward			Output reward for the given agent.
	 * @param AgentId			Agent id to gather reward for.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void GatherAgentReward(float& OutReward, const int32 AgentId);


	/**
	 * This callback can be overridden by the Trainer and gathers all the reward values for the given set of agents. By default this will call 
	 * GatherAgentReward on each agent.
	 *
	 * @param OutRewards		Output rewards for each agent in AgentIds
	 * @param AgentIds			Agents to gather rewards for.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void GatherAgentRewards(TArray<float>& OutRewards, const TArray<int32>& AgentIds);

// ----- Completions ----- 
public:

	/**
	 * This callback should be overridden by the Trainer and gathers the completion for a given agent.
	 *
	 * @param OutCompletion		Output completion for the given agent.
	 * @param AgentId			Agent id to gather completion for.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void GatherAgentCompletion(ELearningAgentsCompletion& OutCompletion, const int32 AgentId);

	/**
	 * This callback can be overridden by the Trainer and gathers all the completions for the given set of agents. By default this will call 
	 * GatherAgentCompletion on each agent.
	 *
	 * @param OutCompletions	Output completions for each agent in AgentIds
	 * @param AgentIds			Agents to gather completions for.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void GatherAgentCompletions(TArray<ELearningAgentsCompletion>& OutCompletions, const TArray<int32>& AgentIds);

// ----- Resets ----- 
public:

	/**
	 * This callback should be overridden by the Trainer and resets the episode for the given agent.
	 *
	 * @param AgentId			The id of the agent that need resetting.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void ResetAgentEpisode(const int32 AgentId);

	/**
	 * This callback can be overridden by the Trainer and resets all episodes for each agent in the given set. By default this will call 
	 * ResetAgentEpisode on each agent.
	 * 
	 * @param AgentIds			The ids of the agents that need resetting.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void ResetAgentEpisodes(const TArray<int32>& AgentIds);

// ----- Training Process -----
public:

	/** Returns true if the trainer is currently training; Otherwise, false. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	const bool IsTraining() const;

	/**
	 * Begins the training process with the provided settings.
	 * 
	 * @param TrainerTrainingSettings	The settings for this training run.
	 * @param TrainerGameSettings		The settings that will affect the game's simulation.
	 * @param TrainerPathSettings		The path settings used by the trainer.
	 * @param bResetAgentsOnBegin		If true, reset all agents at the beginning of training.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AutoCreateRefTerm = "TrainerTrainingSettings,TrainerGameSettings,TrainerPathSettings"))
	void BeginTraining(
		const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings = FLearningAgentsTrainerTrainingSettings(),
		const FLearningAgentsTrainerGameSettings& TrainerGameSettings = FLearningAgentsTrainerGameSettings(),
		const FLearningAgentsTrainerPathSettings& TrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const bool bResetAgentsOnBegin = true);

	/** Stops the training process. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EndTraining();

	/**
	 * Call this function when it is time to evaluate the rewards for your agents. This should be done at the beginning
	 * of each iteration of your training loop after the initial step, i.e. after taking an action, you want to get into
	 * the next state before evaluating the rewards.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void GatherRewards();

	/**
	 * Call this function when it is time to evaluate the completions for your agents. This should be done at the beginning
	 * of each iteration of your training loop after the initial step, i.e. after taking an action, you want to get into
	 * the next state before evaluating the completions.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void GatherCompletions();

	/**
	 * Call this function at the end of each step of your training loop. This takes the current observations/actions/
	 * rewards and moves them into the episode experience buffer. All agents with full episode buffers or those which
	 * have been signaled complete will be reset. If enough experience is gathered, it will be sent to the training 
	 * process and an iteration of training will be run and the updated policy will be synced back.
	 *
	 * @param bResetAgentsOnUpdate				If true, reset all agents whenever an updated policy is received.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void ProcessExperience(const bool bResetAgentsOnUpdate = true);

	/**
	 * Convenience function that runs a basic training loop. If training has not been started, it will start it, and 
	 * then call RunInference. On each following call to this function, it will call GatherRewards, 
	 * GatherCompletions, and ProcessExperience, followed by RunInference.
	 * 
	 * @param TrainerTrainingSettings			The settings for this training run.
	 * @param TrainerGameSettings				The settings that will affect the game's simulation.
	 * @param TrainerPathSettings				The path settings used by the trainer.
	 * @param bResetAgentsOnBegin				If true, reset all agents at the beginning of training.
	 * @param bResetAgentsOnUpdate				If true, reset all agents whenever an updated policy is received.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AutoCreateRefTerm = "TrainerTrainingSettings,TrainerGameSettings,TrainerPathSettings"))
	void RunTraining(
		const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings = FLearningAgentsTrainerTrainingSettings(),
		const FLearningAgentsTrainerGameSettings& TrainerGameSettings = FLearningAgentsTrainerGameSettings(),
		const FLearningAgentsTrainerPathSettings& TrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const bool bResetAgentsOnBegin = true,
		const bool bResetAgentsOnUpdate = true);

	/**
	 * Returns true if GatherRewards has been called and the reward already set for the given agent.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	bool HasReward(const int32 AgentId) const;

	/**
	 * Returns true if GatherCompletions has been called and the completion already set for the given agent.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	bool HasCompletion(const int32 AgentId) const;

	/**
	 * Gets the current reward for an agent. Should be called only after GatherRewards.
	 *
	 * @param AgentId	The AgentId to look-up the reward for
	 * @returns			The reward
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetReward(const int32 AgentId) const;

	/**
	 * Gets the current completion for an agent. Should be called only after GatherCompletions.
	 *
	 * @param AgentId	The AgentId to look-up the completion for
	 * @returns			The completion type
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	ELearningAgentsCompletion GetCompletion(const int32 AgentId) const;

	/**
	 * Gets the current elapsed episode time for the given agent.
	 *
	 * @param AgentId	The AgentId to look-up the episode time for
	 * @returns			The elapsed episode time
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetEpisodeTime(const int32 AgentId) const;

	/**
	 * Gets the number of step recorded in an episode for the given agent.
	 *
	 * @param AgentId	The AgentId to look-up the number of recorded episode steps for
	 * @returns			The number of recorded episode steps
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	int32 GetEpisodeStepNum(const int32 AgentId) const;

	/**
	 * Returns true if the trainer has failed to communicate with the external training process. This can be used in
	 * combination with RunTraining to avoid filling the logs with errors.
	 *
	 * @returns				True if the training has failed. Otherwise, false.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool HasTrainingFailed() const;

// ----- Private Data ----- 
private:

	/** The agent interactor associated with this component. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

	/** The current policy for experience gathering. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsPolicy> Policy;

	/** The current critic. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsCritic> Critic;

	/** True if training is currently in-progress. Otherwise, false. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bIsTraining = false;

	/**
	 * True if trainer encountered an unrecoverable error during training (e.g. the trainer process timed out). Otherwise, false.
	 * This exists mainly to keep the editor from locking up if something goes wrong during training.
	 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bHasTrainingFailed = false;

	/** Callback Reward Output */
	TArray<float> RewardBuffer;

	/** Callback Completion Output */
	TArray<ELearningAgentsCompletion> CompletionBuffer;

	/** Reward Buffer */
	TLearningArray<1, float> Rewards;
	
	/** Agent Completions Buffer */
	TLearningArray<1, UE::Learning::ECompletionMode> AgentCompletions;

	/** Episode Completions Buffer */
	TLearningArray<1, UE::Learning::ECompletionMode> EpisodeCompletions;

	/** All Completions Buffer */
	TLearningArray<1, UE::Learning::ECompletionMode> AllCompletions;

	/** Agent episode times */
	TLearningArray<1, float> EpisodeTimes;

	TUniquePtr<UE::Learning::FEpisodeBuffer> EpisodeBuffer;
	TUniquePtr<UE::Learning::FReplayBuffer> ReplayBuffer;
	TUniquePtr<UE::Learning::FResetInstanceBuffer> ResetBuffer;
	TUniquePtr<UE::Learning::FSharedMemoryPPOTrainer> Trainer;
	
	float TrainerTimeout = 10.0f;

	void DoneTraining();

	/** Number of times rewards have been evaluated for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> RewardIteration;

	/** Number of times completions have been evaluated for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> CompletionIteration;

	/** Temp buffers used to record the set of agents that are valid for training */
	TArray<int32> ValidAgentIds;
	TArray<int32> FinalValidAgentIds;
	UE::Learning::FIndexSet ValidAgentSet;
	UE::Learning::FIndexSet FinalValidAgentSet;

// ----- Private Recording of GameSettings ----- 
private:

	bool bFixedTimestepUsed = false;
	float FixedTimeStepDeltaTime = -1.0f;
	float MaxPhysicsStep = -1.0f;
	int32 MaxFPS = 120;
	bool bVSyncEnabled = true;
	int32 ViewModeIndex = -1;
	
	bool bUseLessCPUInTheBackground = true;
	bool bEditorVSyncEnabled = true;
};
