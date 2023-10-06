// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerComponent.h"
#include "LearningAgentsCritic.h" // Included for FLearningAgentsCriticSettings()

#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "Engine/EngineTypes.h"

#include "LearningAgentsTrainer.generated.h"

namespace UE::Learning
{
	struct FAnyCompletion;
	struct FArrayMap;
	struct FEpisodeBuffer;
	struct FReplayBuffer;
	struct FResetInstanceBuffer;
	struct FRewardObject;
	struct FSharedMemoryPPOTrainer;
	struct FSumReward;
	struct FCompletionObject;
	enum class ECompletionMode : uint8;
	enum class ETrainerDevice : uint8;
}

class ALearningAgentsManager;
class ULearningAgentsInteractor;
class ULearningAgentsCompletion;
class ULearningAgentsReward;
class ULearningAgentsPolicy;
class ULearningAgentsCritic;

/** Completion modes for episodes. */
UENUM(BlueprintType, Category = "LearningAgents", meta = (ScriptName = "LearningAgentsCompletionEnum"))
enum class ELearningAgentsCompletion : uint8
{
	/** Episode ended early but was still in progress. Critic will be used to estimate final return. */
	Truncation	UMETA(DisplayName = "Truncation"),

	/** Episode ended early and zero reward was expected for all future steps. */
	Termination	UMETA(DisplayName = "Termination"),
};

namespace UE::Learning::Agents
{
	/** Get the learning agents completion from the UE::Learning completion. */
	LEARNINGAGENTSTRAINING_API ELearningAgentsCompletion GetLearningAgentsCompletion(const ECompletionMode CompletionMode);

	/** Get the UE::Learning completion from the learning agents completion. */
	LEARNINGAGENTSTRAINING_API ECompletionMode GetCompletionMode(const ELearningAgentsCompletion Completion);
}

/** The configurable settings for a ULearningAgentsTrainer. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerSettings
{
	GENERATED_BODY()

public:

	/** Completion type to use when the maximum number of steps for an episode is reached. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	ELearningAgentsCompletion MaxStepsCompletion = ELearningAgentsCompletion::Truncation;

	/** Max number of steps to take while training before episode automatically completes. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxStepNum = 300;

	/** Maximum number of episodes to record before running a training iteration. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumRecordedEpisodesPerIteration = 1000;

	/** Maximum number of steps to record before running a training iteration. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaximumRecordedStepsPerIteration = 10000;

	/** Time in seconds to wait for the training subprocess before timing out. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TrainerCommunicationTimeout = 20.0f;
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

	/** If true, VSync will be disabled; Otherwise, it will not. Disabling VSync can speed up the game simulation. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bDisableVSync = true;

	/** If true, the viewport rendering will be unlit; Otherwise, it will not. Disabling lighting can speed up the game simulation. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bUseUnlitViewportRendering = false;
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
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumberOfIterations = 1000000;

	/** Learning rate of the policy network. Typical values are between 0.001 and 0.0001. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRatePolicy = 0.0001f;

	/**
	 * Learning rate of the critic network. To avoid instability generally the critic should have a larger learning 
	 * rate than the policy. Typically this can be set to 10x the rate of the policy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LearningRateCritic = 0.001f;

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
	 * The initial scaling for the weights of the output layer of the neural network. Typically, you would use this to
	 * scale down the initial actions as it can stabilize the training and speed up convergence.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float InitialActionScale = 0.1f;

	/**
	 * Batch size to use for training. Smaller values tend to produce better results at the cost of slowing down 
	 * training. Large batch sizes are much more computationally efficient when training on the GPU.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0", ClampMax = "4096", UIMin = "0", UIMax = "4096"))
	int32 BatchSize = 128;

	/**
	 * Clipping ratio to apply to policy updates. Keeps the training "on-policy". Larger values may speed up training at 
	 * the cost of stability. Conversely, too small values will keep the policy from being able to learn an 
	 * optimal policy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EpsilonClip = 0.2f;

	/**
	 * Weight used to regularize actions. Larger values will encourage smaller actions but too large will cause actions 
	 * to become always zero.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ActionRegularizationWeight = 0.001f;

	/**
	 * Weighting used for the entropy bonus. Larger values encourage larger action noise and therefore greater 
	 * exploration but can make actions very noisy.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EntropyWeight = 0.01f;

	/**
	 * This is used in the Generalized Advantage Estimation as what is essentially an exponential smoothing/decay. 
	 * Typical values should be between 0.9 and 1.0.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GaeLambda = 0.9f;

	/**
	 * When true, very large or small advantages will be clipped. This has few downsides and helps with numerical 
	 * stability.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bClipAdvantages = true;

	/** When true, advantages are normalized. This tends to makes training more robust to adjustments of the scale of rewards. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	bool bAdvantageNormalization = true;

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
	FDirectoryPath IntermediateRelativePath;

public:

	/** Gets the Relative Editor Engine Path accounting for if this is an editor build or not  */
	FString GetEditorEnginePath() const;

	/** Gets the Relative Intermediate Path  */
	FString GetIntermediatePath() const;
};

/**
 * The ULearningAgentsTrainer is the core class for reinforcement learning training. It has a few responsibilities:
 *   1) It keeps track of which agents are gathering training data.
 *   2) It defines how those agents' rewards, completions, and resets are implemented.
 *   3) It provides methods for orchestrating the training process.
 *
 * To use this class, you need to implement the SetupRewards and SetupCompletions functions (as well as their
 * corresponding SetRewards and SetCompletions functions), which will define the rewards and penalties the agent
 * receives and what conditions cause an episode to end. Before you can begin training, you need to call SetupTrainer,
 * which will initialize the underlying data structures, and you need to call AddAgent for each agent you want to gather
 * training data from.
 *
 * @see ULearningAgentsInteractor to understand how observations and actions work.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ULearningAgentsTrainer : public ULearningAgentsManagerComponent
{
	GENERATED_BODY()

// ----- Setup -----
public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsTrainer();
	ULearningAgentsTrainer(FVTableHelper& Helper);
	virtual ~ULearningAgentsTrainer();

	/** Will automatically call EndTraining if training is still in-progress when play is ending. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Initializes this object and runs the setup functions for rewards and completions.
	 * @param InInteractor The agent interactor we are training with.
	 * @param InPolicy The policy to be trained.
	 * @param InCritic Optional - only needs to be provided if we want the critic to be accessible at runtime.
	 * @param TrainerSettings The trainer settings to use.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetupTrainer(
		ULearningAgentsInteractor* InInteractor,
		ULearningAgentsPolicy* InPolicy,
		ULearningAgentsCritic* InCritic = nullptr,
		const FLearningAgentsTrainerSettings& TrainerSettings = FLearningAgentsTrainerSettings());

public: 

	//~ Begin ULearningAgentsManagerComponent Interface
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsReset(const TArray<int32>& AgentIds) override;
	//~ End ULearningAgentsManagerComponent Interface

// ----- Rewards -----
public:
	
	/**
	 * During this event, all rewards/penalties should be added to this trainer.
	 * @see LearningAgentsRewards.h for the list of available rewards/penalties.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void SetupRewards();

	/**
	 * During this event, all rewards/penalties should be set for each agent.
	 * @param AgentIds The list of agent ids to set rewards/penalties for.
	 * @see LearningAgentsRewards.h for the list of available rewards/penalties.
	 * @see GetAgent to get the agent corresponding to each id.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void SetRewards(const TArray<int32>& AgentIds);

	/**
	 * Used by objects derived from ULearningAgentsReward to add themselves to this trainer during their creation.
	 * You shouldn't need to call this directly.
	 */
	void AddReward(TObjectPtr<ULearningAgentsReward> Object, const TSharedRef<UE::Learning::FRewardObject>& Reward);

// ----- Completions ----- 
public:

	/**
	 * During this event, all completions should be added to this trainer.
	 * @see LearningAgentsCompletions.h for the list of available completions.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void SetupCompletions();

	/**
	 * During this event, all completions should be set for each agent.
	 * @param AgentIds The list of agent ids to set completions for.
	 * @see LearningAgentsCompletions.h for the list of available completions.
	 * @see GetAgent to get the agent corresponding to each id.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void SetCompletions(const TArray<int32>& AgentIds);

	/**
	 * Used by objects derived from ULearningAgentsCompletion to add themselves to this trainer during their creation.
	 * You shouldn't need to call this directly.
	 */
	void AddCompletion(TObjectPtr<ULearningAgentsCompletion> Object, const TSharedRef<UE::Learning::FCompletionObject>& Completion);

// ----- Resets ----- 
public:

	/**
	 * During this event, all episodes should be reset for each agent.
	 * @param AgentIds The ids of the agents that need resetting.
	 * @see GetAgent to get the agent corresponding to each id.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void ResetEpisodes(const TArray<int32>& AgentIds);

// ----- Training Process -----
public:

	/** Returns true if the trainer is currently training; Otherwise, false. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	const bool IsTraining() const;

	/**
	 * Begins the training process with the provided settings.
	 * @param TrainerTrainingSettings The settings for this training run.
	 * @param TrainerGameSettings The settings that will affect the game's simulation.
	 * @param TrainerPathSettings The path settings used by the trainer.
	 * @param CriticSettings The settings for the critic (if we are using one).
	 * @param bReinitializePolicyNetwork If true, reinitialize the policy. Set this to false if your policy is pre-trained, e.g. with imitation learning.
	 * @param bReinitializeCriticNetwork If true, reinitialize the critic. Set this to false if your critic is pre-trained.
	 * @param bResetAgentsOnBegin If true, reset all agents at the beginning of training.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void BeginTraining(
		const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings = FLearningAgentsTrainerTrainingSettings(),
		const FLearningAgentsTrainerGameSettings& TrainerGameSettings = FLearningAgentsTrainerGameSettings(),
		const FLearningAgentsTrainerPathSettings& TrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const FLearningAgentsCriticSettings& CriticSettings = FLearningAgentsCriticSettings(),
		const bool bReinitializePolicyNetwork = true,
		const bool bReinitializeCriticNetwork = true,
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
	void EvaluateRewards();

	/**
	 * Call this function when it is time to evaluate the completions for your agents. This should be done at the beginning
	 * of each iteration of your training loop after the initial step, i.e. after taking an action, you want to get into
	 * the next state before evaluating the completions.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EvaluateCompletions();

	/**
	 * Call this function at the end of each step of your training loop. This takes the current observations/actions/
	 * rewards and moves them into the episode experience buffer. All agents with full episode buffers or those which
	 * have been signaled complete will be reset. If enough experience is gathered, it will be sent to the training 
	 * process and an iteration of training will be run and the updated policy will be synced back.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void ProcessExperience();

	/**
	 * Convenience function that runs a basic training loop. If training has not been started, it will start it, and 
	 * then call RunInference. On each following call to this function, it will call EvaluateRewards, 
	 * EvaluateCompletions, and ProcessExperience, followed by RunInference.
	 * @param TrainerTrainingSettings The settings for this training run.
	 * @param TrainerGameSettings The settings that will affect the game's simulation.
	 * @param TrainerPathSettings The path settings used by the trainer.
	 * @param CriticSettings The settings for the critic (if we are using one).
	 * @param bReinitializePolicyNetwork If true, reinitialize the policy. Set this to false if your policy is pre-trained, e.g. with imitation learning.
	 * @param bReinitializeCriticNetwork If true, reinitialize the critic. Set this to false if your critic is pre-trained.
	 * @param bResetAgentsOnBegin If true, reset all agents at the beginning of training.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void RunTraining(
		const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings = FLearningAgentsTrainerTrainingSettings(),
		const FLearningAgentsTrainerGameSettings& TrainerGameSettings = FLearningAgentsTrainerGameSettings(),
		const FLearningAgentsTrainerPathSettings& TrainerPathSettings = FLearningAgentsTrainerPathSettings(),
		const FLearningAgentsCriticSettings& CriticSettings = FLearningAgentsCriticSettings(),
		const bool bReinitializePolicyNetwork = true,
		const bool bReinitializeCriticNetwork = true,
		const bool bResetAgentsOnBegin = true);

	/**
	 * Gets the current reward for an agent according to the critic. Should be called only after EvaluateRewards.
	 *
	 * @param AgentId	The AgentId to look-up the reward for
	 * @returns			The reward
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetReward(const int32 AgentId) const;

	/**
	 * Gets if the agent will complete the episode or not according to the given set of completions. Should be called 
	 * only after EvaluateCompletions.
	 *
	 * @param AgentId		The AgentId to look-up the completion for
	 * @param OutCompletion	The completion type if the agent will complete the episode
	 * @returns				If the agent will complete the episode
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1"))
	bool IsCompleted(const int32 AgentId, ELearningAgentsCompletion& OutCompletion) const;

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

	/** The list of current reward objects. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TArray<TObjectPtr<ULearningAgentsReward>> RewardObjects;

	/** The list of current completion objects. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TArray<TObjectPtr<ULearningAgentsCompletion>> CompletionObjects;

	TArray<TSharedRef<UE::Learning::FRewardObject>, TInlineAllocator<16>> RewardFeatures;
	TArray<TSharedRef<UE::Learning::FCompletionObject>, TInlineAllocator<16>> CompletionFeatures;

	TSharedPtr<UE::Learning::FSumReward> Rewards;
	TSharedPtr<UE::Learning::FAnyCompletion> Completions;

	TUniquePtr<UE::Learning::FEpisodeBuffer> EpisodeBuffer;
	TUniquePtr<UE::Learning::FReplayBuffer> ReplayBuffer;
	TUniquePtr<UE::Learning::FResetInstanceBuffer> ResetBuffer;
	TUniquePtr<UE::Learning::FSharedMemoryPPOTrainer> Trainer;
	
	ELearningAgentsCompletion MaxStepsCompletion = ELearningAgentsCompletion::Truncation;

	float TrainerTimeout = 10.0f;

	void DoneTraining();

// ----- Private Iteration Checks ----- 
private:

	/** Number of times rewards have been evaluated for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> RewardEvaluatedAgentIteration;

	/** Number of times completions have been evaluated for all agents */
	TLearningArray<1, uint64, TInlineAllocator<32>> CompletionEvaluatedAgentIteration;

	/** Temp buffers used to record the set of agents that are valid for training */
	TArray<int32> ValidAgentIds;
	TArray<int32> FinalValidAgentIds;
	UE::Learning::FIndexSet ValidAgentSet;
	UE::Learning::FIndexSet FinalValidAgentSet;
	TBitArray<TInlineAllocator<32>> ValidAgentStatus;

// ----- Private Recording of GameSettings ----- 
private:

	bool bFixedTimestepUsed = false;
	float FixedTimeStepDeltaTime = -1.0f;
	bool bVSyncEnabled = true;
	float MaxPhysicsStep = -1.0f;
	int32 ViewModeIndex = -1;
};
