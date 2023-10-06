// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsTrainer.h"

#include "LearningAgentsInteractor.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsRewards.h"
#include "LearningAgentsCompletions.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsHelpers.h"
#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningExperience.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningNeuralNetworkObject.h"
#include "LearningPPOTrainer.h"
#include "LearningRewardObject.h"
#include "LearningCompletion.h"
#include "LearningCompletionObject.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "GameFramework/GameUserSettings.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/GameViewportClient.h"
#include "EngineDefines.h"

namespace UE::Learning::Agents
{
	ELearningAgentsCompletion GetLearningAgentsCompletion(const ECompletionMode CompletionMode)
	{
		switch (CompletionMode)
		{
		case ECompletionMode::Running: UE_LOG(LogLearning, Error, TEXT("Cannot convert from ECompletionMode::Running to ELearningAgentsCompletion")); return ELearningAgentsCompletion::Termination;
		case ECompletionMode::Terminated: return ELearningAgentsCompletion::Termination;
		case ECompletionMode::Truncated: return ELearningAgentsCompletion::Truncation;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Completion Mode.")); return ELearningAgentsCompletion::Termination;
		}
	}

	ECompletionMode GetCompletionMode(const ELearningAgentsCompletion Completion)
	{
		switch (Completion)
		{
		case ELearningAgentsCompletion::Termination: return ECompletionMode::Terminated;
		case ELearningAgentsCompletion::Truncation: return ECompletionMode::Truncated;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Completion.")); return ECompletionMode::Running;
		}
	}
}

namespace UE::Learning::Agents
{
	ELearningAgentsTrainerDevice GetLearningAgentsTrainerDevice(const ETrainerDevice Device)
	{
		switch (Device)
		{
		case ETrainerDevice::CPU: return ELearningAgentsTrainerDevice::CPU;
		case ETrainerDevice::GPU: return ELearningAgentsTrainerDevice::GPU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Trainer Device.")); return ELearningAgentsTrainerDevice::CPU;
		}
	}

	ETrainerDevice GetTrainerDevice(const ELearningAgentsTrainerDevice Device)
	{
		switch (Device)
		{
		case ELearningAgentsTrainerDevice::CPU: return ETrainerDevice::CPU;
		case ELearningAgentsTrainerDevice::GPU: return ETrainerDevice::GPU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Trainer Device.")); return ETrainerDevice::CPU;
		}
	}
}

FLearningAgentsTrainerPathSettings::FLearningAgentsTrainerPathSettings()
{
	EditorEngineRelativePath.Path = FPaths::EngineDir();
	IntermediateRelativePath.Path = FPaths::ProjectIntermediateDir();
}

FString FLearningAgentsTrainerPathSettings::GetEditorEnginePath() const
{
#if WITH_EDITOR
	return EditorEngineRelativePath.Path;
#else
	if (NonEditorEngineRelativePath.IsEmpty())
	{
		UE_LOG(LogLearning, Warning, TEXT("GetEditorEnginePath: NonEditorEngineRelativePath not set"));
	}

	return NonEditorEngineRelativePath;
#endif
}

FString FLearningAgentsTrainerPathSettings::GetIntermediatePath() const
{
	return IntermediateRelativePath.Path;
}

ULearningAgentsTrainer::ULearningAgentsTrainer() : Super(FObjectInitializer::Get()) {}
ULearningAgentsTrainer::ULearningAgentsTrainer(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsTrainer::~ULearningAgentsTrainer() = default;

void ULearningAgentsTrainer::SetupTrainer(
	ULearningAgentsInteractor* InInteractor,
	ULearningAgentsPolicy* InPolicy,
	ULearningAgentsCritic* InCritic,
	const FLearningAgentsTrainerSettings& TrainerSettings)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already run!"), *GetName());
		return;
	}

	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		return;
	}

	if (!InInteractor)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InInteractor is nullptr."), *GetName());
		return;
	}

	if (!InInteractor->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InInteractor->GetName());
		return;
	}

	if (!InPolicy)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InPolicy is nullptr."), *GetName());
		return;
	}

	if (!InPolicy->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InPolicy->GetName());
		return;
	}

	// The critic is optional unlike the other components
	if (InCritic && !InCritic->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InCritic->GetName());
		return;
	}

	Interactor = InInteractor;
	Policy = InPolicy;
	Critic = InCritic;

	// Setup Rewards
	RewardObjects.Empty();
	RewardFeatures.Empty();
	SetupRewards();

	if (RewardObjects.Num() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: No rewards added to Trainer during SetupRewards."), *GetName());
		return;
	}

	Rewards = MakeShared<UE::Learning::FSumReward>(TEXT("Rewards"),
		TLearningArrayView<1, const TSharedRef<UE::Learning::FRewardObject>>(RewardFeatures),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum());

	// Setup Completions
	CompletionObjects.Empty();
	CompletionFeatures.Empty();
	SetupCompletions();

	if (CompletionObjects.Num() == 0)
	{
		// Not an error or warning because it's fine to run training without any completions.
		UE_LOG(LogLearning, Display, TEXT("%s: No completions added to Trainer during SetupCompletions."), *GetName());
	}

	Completions = MakeShared<UE::Learning::FAnyCompletion>(TEXT("Completions"),
		TLearningArrayView<1, const TSharedRef<UE::Learning::FCompletionObject>>(CompletionFeatures),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum());

	// Create Episode Buffer
	EpisodeBuffer = MakeUnique<UE::Learning::FEpisodeBuffer>();
	EpisodeBuffer->Resize(
		Manager->GetMaxAgentNum(),
		TrainerSettings.MaxStepNum,
		Interactor->GetObservationFeature().DimNum(),
		Interactor->GetActionFeature().DimNum());

	MaxStepsCompletion = TrainerSettings.MaxStepsCompletion;

	// Create Replay Buffer
	ReplayBuffer = MakeUnique<UE::Learning::FReplayBuffer>();
	ReplayBuffer->Resize(
		Interactor->GetObservationFeature().DimNum(),
		Interactor->GetActionFeature().DimNum(),
		TrainerSettings.MaximumRecordedEpisodesPerIteration,
		TrainerSettings.MaximumRecordedStepsPerIteration);

	// Create Reset Buffer
	ResetBuffer = MakeUnique<UE::Learning::FResetInstanceBuffer>();
	ResetBuffer->Resize(Manager->GetMaxAgentNum());

	// Record Timeout Setting
	TrainerTimeout = TrainerSettings.TrainerCommunicationTimeout;

	// Reset Agent iteration
	RewardEvaluatedAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	CompletionEvaluatedAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(RewardEvaluatedAgentIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(CompletionEvaluatedAgentIteration, INDEX_NONE);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsTrainer::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		ResetEpisodes(AgentIds);
		EpisodeBuffer->Reset(AgentIds);

		UE::Learning::Array::Set<1, uint64>(RewardEvaluatedAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<1, uint64>(CompletionEvaluatedAgentIteration, 0, AgentIds);

		for (ULearningAgentsReward* RewardObject : RewardObjects)
		{
			RewardObject->OnAgentsAdded(AgentIds);
		}

		for (ULearningAgentsCompletion* CompletionObject : CompletionObjects)
		{
			CompletionObject->OnAgentsAdded(AgentIds);
		}

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsTrainer::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(RewardEvaluatedAgentIteration, INDEX_NONE, AgentIds);
		UE::Learning::Array::Set<1, uint64>(CompletionEvaluatedAgentIteration, INDEX_NONE, AgentIds);

		for (ULearningAgentsReward* RewardObject : RewardObjects)
		{
			RewardObject->OnAgentsRemoved(AgentIds);
		}

		for (ULearningAgentsCompletion* CompletionObject : CompletionObjects)
		{
			CompletionObject->OnAgentsRemoved(AgentIds);
		}

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsTrainer::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		ResetEpisodes(AgentIds);
		EpisodeBuffer->Reset(AgentIds);

		UE::Learning::Array::Set<1, uint64>(RewardEvaluatedAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<1, uint64>(CompletionEvaluatedAgentIteration, 0, AgentIds);

		for (ULearningAgentsReward* RewardObject : RewardObjects)
		{
			RewardObject->OnAgentsReset(AgentIds);
		}

		for (ULearningAgentsCompletion* CompletionObject : CompletionObjects)
		{
			CompletionObject->OnAgentsReset(AgentIds);
		}

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

void ULearningAgentsTrainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsTraining)
	{
		EndTraining();
	}

	Super::EndPlay(EndPlayReason);
}

void ULearningAgentsTrainer::SetupRewards_Implementation()
{
	// Can be overridden to setup rewards without blueprints
}

void ULearningAgentsTrainer::SetRewards_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden to set rewards without blueprints
}

void ULearningAgentsTrainer::AddReward(TObjectPtr<ULearningAgentsReward> Object, const TSharedRef<UE::Learning::FRewardObject>& Reward)
{
	UE_LEARNING_CHECK(!IsSetup());
	UE_LEARNING_CHECK(Object);
	RewardObjects.Add(Object);
	RewardFeatures.Add(Reward);
}

void ULearningAgentsTrainer::SetupCompletions_Implementation()
{
	// Can be overridden to setup completions without blueprints
}

void ULearningAgentsTrainer::SetCompletions_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden to evaluate completions without blueprints
}

void ULearningAgentsTrainer::AddCompletion(TObjectPtr<ULearningAgentsCompletion> Object, const TSharedRef<UE::Learning::FCompletionObject>& Completion)
{
	UE_LEARNING_CHECK(!IsSetup());
	UE_LEARNING_CHECK(Object);
	CompletionObjects.Add(Object);
	CompletionFeatures.Add(Completion);
}

const bool ULearningAgentsTrainer::IsTraining() const
{
	return bIsTraining;
}

void ULearningAgentsTrainer::BeginTraining(
	const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings,
	const FLearningAgentsTrainerGameSettings& TrainerGameSettings,
	const FLearningAgentsTrainerPathSettings& TrainerPathSettings,
	const FLearningAgentsCriticSettings& CriticSettings,
	const bool bReinitializePolicyNetwork,
	const bool bReinitializeCriticNetwork,
	const bool bResetAgentsOnBegin)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Already Training!"), *GetName());
		return;
	}

	// Check Paths

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(TrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python executable \"%s\"."), *GetName(), *PythonExecutablePath);
		return;
	}

	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(TrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find LearningAgents plugin Content \"%s\"."), *GetName(), *PythonContentPath);
		return;
	}

	const FString SitePackagesPath = UE::Learning::Trainer::GetSitePackagesPath(TrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(SitePackagesPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python site-packages \"%s\"."), *GetName(), *SitePackagesPath);
		return;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(TrainerPathSettings.GetIntermediatePath());

	// Record GameState Settings

	bFixedTimestepUsed = FApp::UseFixedTimeStep();
	FixedTimeStepDeltaTime = FApp::GetFixedDeltaTime();

	UGameUserSettings* GameSettings = UGameUserSettings::GetGameUserSettings();
	if (GameSettings)
	{
		bVSyncEnabled = GameSettings->IsVSyncEnabled();
	}

	UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
	if (PhysicsSettings)
	{
		MaxPhysicsStep = PhysicsSettings->MaxPhysicsDeltaTime;
	}

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient)
	{
		ViewModeIndex = ViewportClient->ViewModeIndex;
	}

	// Apply Training GameState Settings

	FApp::SetUseFixedTimeStep(TrainerGameSettings.bUseFixedTimeStep);

	if (TrainerGameSettings.FixedTimeStepFrequency > UE_SMALL_NUMBER)
	{
		FApp::SetFixedDeltaTime(1.0f / TrainerGameSettings.FixedTimeStepFrequency);
		if (TrainerGameSettings.bSetMaxPhysicsStepToFixedTimeStep && PhysicsSettings)
		{
			PhysicsSettings->MaxPhysicsDeltaTime = 1.0f / TrainerGameSettings.FixedTimeStepFrequency;
		}
	}
	else
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Provided invalid FixedTimeStepFrequency: %0.5f"), *GetName(), TrainerGameSettings.FixedTimeStepFrequency);
	}

	if (TrainerGameSettings.bDisableVSync && GameSettings)
	{
		GameSettings->SetVSyncEnabled(false);
		GameSettings->ApplySettings(false);
	}

	if (TrainerGameSettings.bUseUnlitViewportRendering && ViewportClient)
	{
		ViewportClient->ViewModeIndex = EViewModeIndex::VMI_Unlit;
	}

	// Start Trainer

	UE::Learning::FPPOTrainerTrainingSettings PPOTrainingSettings;
	PPOTrainingSettings.IterationNum = TrainerTrainingSettings.NumberOfIterations;
	PPOTrainingSettings.LearningRatePolicy = TrainerTrainingSettings.LearningRatePolicy;
	PPOTrainingSettings.LearningRateCritic = TrainerTrainingSettings.LearningRateCritic;
	PPOTrainingSettings.LearningRateDecay = TrainerTrainingSettings.LearningRateDecay;
	PPOTrainingSettings.WeightDecay = TrainerTrainingSettings.WeightDecay;
	PPOTrainingSettings.InitialActionScale = TrainerTrainingSettings.InitialActionScale;
	PPOTrainingSettings.BatchSize = TrainerTrainingSettings.BatchSize;
	PPOTrainingSettings.EpsilonClip = TrainerTrainingSettings.EpsilonClip;
	PPOTrainingSettings.ActionRegularizationWeight = TrainerTrainingSettings.ActionRegularizationWeight;
	PPOTrainingSettings.EntropyWeight = TrainerTrainingSettings.EntropyWeight;
	PPOTrainingSettings.GaeLambda = TrainerTrainingSettings.GaeLambda;
	PPOTrainingSettings.bClipAdvantages = TrainerTrainingSettings.bClipAdvantages;
	PPOTrainingSettings.bAdvantageNormalization = TrainerTrainingSettings.bAdvantageNormalization;
	PPOTrainingSettings.TrimEpisodeStartStepNum = TrainerTrainingSettings.NumberOfStepsToTrimAtStartOfEpisode;
	PPOTrainingSettings.TrimEpisodeEndStepNum = TrainerTrainingSettings.NumberOfStepsToTrimAtEndOfEpisode;
	PPOTrainingSettings.Seed = TrainerTrainingSettings.RandomSeed;
	PPOTrainingSettings.DiscountFactor = TrainerTrainingSettings.DiscountFactor;
	PPOTrainingSettings.Device = UE::Learning::Agents::GetTrainerDevice(TrainerTrainingSettings.Device);
	PPOTrainingSettings.bUseTensorboard = TrainerTrainingSettings.bUseTensorboard;

	UE::Learning::FPPOTrainerNetworkSettings PPONetworkSettings;
	PPONetworkSettings.PolicyActionNoiseMin = Policy->GetPolicyObject().Settings.ActionNoiseMin;
	PPONetworkSettings.PolicyActionNoiseMax = Policy->GetPolicyObject().Settings.ActionNoiseMax;
	PPONetworkSettings.PolicyActivationFunction = Policy->GetPolicyNetwork().ActivationFunction;
	PPONetworkSettings.PolicyHiddenLayerSize = Policy->GetPolicyNetwork().GetHiddenNum();
	PPONetworkSettings.PolicyLayerNum = Policy->GetPolicyNetwork().GetLayerNum();

	if (Critic)
	{
		if (CriticSettings.HiddenLayerSize != Critic->GetCriticNetwork().GetHiddenNum() ||
			CriticSettings.LayerNum != Critic->GetCriticNetwork().GetLayerNum() ||
			UE::Learning::Agents::GetActivationFunction(CriticSettings.ActivationFunction) != Critic->GetCriticNetwork().ActivationFunction)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: BeginTraining got different Critic Network Settings to those provided to SetupCritic."), *GetName());
		}

		PPONetworkSettings.CriticHiddenLayerSize = Critic->GetCriticNetwork().GetHiddenNum();
		PPONetworkSettings.CriticLayerNum = Critic->GetCriticNetwork().GetLayerNum();
		PPONetworkSettings.CriticActivationFunction = Critic->GetCriticNetwork().ActivationFunction;
	}
	else
	{
		PPONetworkSettings.CriticHiddenLayerSize = CriticSettings.HiddenLayerSize;
		PPONetworkSettings.CriticLayerNum = CriticSettings.LayerNum;
		PPONetworkSettings.CriticActivationFunction = UE::Learning::Agents::GetActivationFunction(CriticSettings.ActivationFunction);
	}

	// We assume that if the critic has been setup on the agent interactor, then
	// the user wants the critic network to be synced during training.
	UE::Learning::EPPOTrainerFlags TrainerFlags = 
		Critic ?
		UE::Learning::EPPOTrainerFlags::SynchronizeCriticNetwork :
		UE::Learning::EPPOTrainerFlags::None;

	if (!bReinitializePolicyNetwork) { TrainerFlags |= UE::Learning::EPPOTrainerFlags::UseInitialPolicyNetwork; }
	if (!bReinitializeCriticNetwork && Critic) { TrainerFlags |= UE::Learning::EPPOTrainerFlags::UseInitialCriticNetwork; }

	// Start Python Training Process (this must be done on game thread)
	Trainer = MakeUnique<UE::Learning::FSharedMemoryPPOTrainer>(
		GetName(),
		PythonExecutablePath,
		SitePackagesPath,
		PythonContentPath,
		IntermediatePath,
		*ReplayBuffer,
		PPOTrainingSettings,
		PPONetworkSettings,
		TrainerFlags);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending / Receiving initial policy..."), *GetName());

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	if ((bool)(TrainerFlags & UE::Learning::EPPOTrainerFlags::UseInitialPolicyNetwork))
	{
		Response = Trainer->SendPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
	}
	else
	{
		Response = Trainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
		Policy->GetNetworkAsset()->ForceMarkDirty();
	}

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending or receiving policy from trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		Trainer->Terminate();
		return;
	}

	if (Critic)
	{
		if ((bool)(TrainerFlags & UE::Learning::EPPOTrainerFlags::UseInitialCriticNetwork))
		{
			Response = Trainer->SendCritic(Critic->GetCriticNetwork(), TrainerTimeout);
		}
		else if ((bool)(TrainerFlags & UE::Learning::EPPOTrainerFlags::SynchronizeCriticNetwork))
		{
			Response = Trainer->RecvCritic(Critic->GetCriticNetwork(), TrainerTimeout);
			Critic->GetNetworkAsset()->ForceMarkDirty();
		}
		else
		{
			Response = UE::Learning::ETrainerResponse::Success;
		}

		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error sending or receiving critic from trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			Trainer->Terminate();
			return;
		}
	}

	// Reset Agents, Replay Buffer

	if (bResetAgentsOnBegin)
	{
		Manager->ResetAllAgents();
	}

	ReplayBuffer->Reset();

	bIsTraining = true;
}

void ULearningAgentsTrainer::DoneTraining()
{
	if (IsTraining())
	{
		// Wait for Trainer to finish
		Trainer->Wait(1.0f);

		// If not finished in time, terminate
		Trainer->Terminate();

		// Apply back previous game settings
		FApp::SetUseFixedTimeStep(bFixedTimestepUsed);
		FApp::SetFixedDeltaTime(FixedTimeStepDeltaTime);
		UGameUserSettings* GameSettings = UGameUserSettings::GetGameUserSettings();
		if (GameSettings)
		{
			GameSettings->SetVSyncEnabled(bVSyncEnabled);
			GameSettings->ApplySettings(true);
		}

		UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
		if (PhysicsSettings)
		{
			PhysicsSettings->MaxPhysicsDeltaTime = MaxPhysicsStep;
		}

		UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (ViewportClient)
		{
			ViewportClient->ViewModeIndex = ViewModeIndex;
		}

		bIsTraining = false;
	}
}

void ULearningAgentsTrainer::EndTraining()
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Display, TEXT("%s: Stopping training..."), *GetName());
		Trainer->SendStop();
		DoneTraining();
	}
}

void ULearningAgentsTrainer::EvaluateRewards()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsTrainer::EvaluateRewards);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check agents have actually make observations and taken actions.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0 || 
			Interactor->GetActionEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Agent with id %i has not made observations and taken actions so rewards will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Set Rewards

	SetRewards(ValidAgentIds);

	// Check that all rewards have had their setter run

	ValidAgentStatus.SetNumUninitialized(Manager->GetMaxAgentNum());
	ValidAgentStatus.SetRange(0, Manager->GetMaxAgentNum(), true);

	for (ULearningAgentsReward* RewardObject : RewardObjects)
	{
		for (const int32 AgentId : ValidAgentSet)
		{
			if (RewardObject->GetAgentIteration(AgentId) == RewardEvaluatedAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Reward %s for agent with id %i has not been set (got iteration %i, expected iteration %i) and so agent will not have rewards evaluated."), *GetName(), *RewardObject->GetName(), AgentId, RewardObject->GetAgentIteration(AgentId), RewardEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (RewardObject->GetAgentIteration(AgentId) > RewardEvaluatedAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Reward %s for agent with id %i appears to have been set multiple times (got iteration %i, expected iteration %i) and so agent will not have rewards evaluated."), *GetName(), *RewardObject->GetName(), AgentId, RewardObject->GetAgentIteration(AgentId), RewardEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (RewardObject->GetAgentIteration(AgentId) != RewardEvaluatedAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Reward %s for agent with id %i does not have a matching iteration number (got iteration %i, expected iteration %i) and so agent will not have rewards evaluated."), *GetName(), *RewardObject->GetName(), AgentId, RewardObject->GetAgentIteration(AgentId), RewardEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}
		}
	}

	FinalValidAgentIds.Empty(ValidAgentSet.Num());

	for (const int32 AgentId : ValidAgentSet)
	{
		if (ValidAgentStatus[AgentId]) { FinalValidAgentIds.Add(AgentId); }
	}

	FinalValidAgentSet = FinalValidAgentIds;
	FinalValidAgentSet.TryMakeSlice();

	// Evaluate Rewards

	Rewards->Evaluate(FinalValidAgentSet);

	// Increment Reward Evaluation Iteration

	for (const int32 AgentId : FinalValidAgentSet)
	{
		RewardEvaluatedAgentIteration[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	for (const ULearningAgentsReward* RewardObject : RewardObjects)
	{
		RewardObject->VisualLog(FinalValidAgentSet);
	}
#endif
}

void ULearningAgentsTrainer::EvaluateCompletions()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsTrainer::EvaluateCompletions);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check agents have actually make observations and taken actions.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0 || 
			Interactor->GetActionEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Agent with id %i has not made observations and taken actions so completions will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}
		
	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Set Completions

	SetCompletions(ValidAgentIds);

	// Check that all completions have had their setter run

	ValidAgentStatus.SetNumUninitialized(Manager->GetMaxAgentNum());
	ValidAgentStatus.SetRange(0, Manager->GetMaxAgentNum(), true);

	for (ULearningAgentsCompletion* CompletionObject : CompletionObjects)
	{
		for (const int32 AgentId : ValidAgentSet)
		{
			if (CompletionObject->GetAgentIteration(AgentId) == CompletionEvaluatedAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Completion %s for agent with id %i has not been set (got iteration %i, expected iteration %i) and so agent will not have completions evaluated."), *GetName(), *CompletionObject->GetName(), AgentId, CompletionObject->GetAgentIteration(AgentId), CompletionEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (CompletionObject->GetAgentIteration(AgentId) > CompletionEvaluatedAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Completion %s for agent with id %i appears to have been set multiple times (got iteration %i, expected iteration %i) and so agent will not have completions evaluated."), *GetName(), *CompletionObject->GetName(), AgentId, CompletionObject->GetAgentIteration(AgentId), CompletionEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (CompletionObject->GetAgentIteration(AgentId) != CompletionEvaluatedAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Completion %s for agent with id %i does not have a matching iteration number (got iteration %i, expected iteration %i) and so agent will not have completions evaluated."), *GetName(), *CompletionObject->GetName(), AgentId, CompletionObject->GetAgentIteration(AgentId), CompletionEvaluatedAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}
		}
	}

	FinalValidAgentIds.Empty(ValidAgentSet.Num());

	for (const int32 AgentId : ValidAgentSet)
	{
		if (ValidAgentStatus[AgentId]) { FinalValidAgentIds.Add(AgentId); }
	}

	FinalValidAgentSet = FinalValidAgentIds;
	FinalValidAgentSet.TryMakeSlice();

	// Evaluate Completions

	Completions->Evaluate(FinalValidAgentSet);
	
	// Increment Completion Evaluation Iteration

	for (const int32 AgentId : FinalValidAgentSet)
	{
		CompletionEvaluatedAgentIteration[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	for (const ULearningAgentsCompletion* CompletionObject : CompletionObjects)
	{
		CompletionObject->VisualLog(FinalValidAgentSet);
	}
#endif
}

void ULearningAgentsTrainer::ProcessExperience()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsTrainer::ProcessExperience);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training not running."), *GetName());
		return;
	}

	if (Policy->GetActionNoiseScale() != 1.0f)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Policy Action Noise Scale should be set to 1.0 during training."), *GetName());
	}

	// Check Observations, Actions, Rewards, and Completions have been completed and have matching iteration number

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0 ||
			Interactor->GetActionEncodingAgentIteration()[AgentId] == 0 ||
			RewardEvaluatedAgentIteration[AgentId] == 0 ||
			CompletionEvaluatedAgentIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Agent with id %i has not completed a full step of observations, actions, rewards, completions and so experience will not be processed for it."), *GetName(), AgentId);
			continue;
		}

		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] != Interactor->GetActionEncodingAgentIteration()[AgentId] ||
			Interactor->GetObservationEncodingAgentIteration()[AgentId] != RewardEvaluatedAgentIteration[AgentId] ||
			Interactor->GetObservationEncodingAgentIteration()[AgentId] != CompletionEvaluatedAgentIteration[AgentId])
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has non-matching iteration numbers (observation: %i, action: %i, reward: %i, completion: %i). Experience will not be processed for it."), *GetName(), AgentId,
				Interactor->GetObservationEncodingAgentIteration()[AgentId],
				Interactor->GetActionEncodingAgentIteration()[AgentId],
				RewardEvaluatedAgentIteration[AgentId],
				CompletionEvaluatedAgentIteration[AgentId]);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;

	// Check for episodes that have been immediately completed

	for (const int32 AgentId : ValidAgentSet)
	{
		if (Completions->CompletionBuffer()[AgentId] != UE::Learning::ECompletionMode::Running && 
			EpisodeBuffer->GetEpisodeStepNums()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has completed episode and will be reset but has not generated any experience."), *GetName(), AgentId);
		}
	}

	// Get Feature Buffers

	UE::Learning::FFeatureObject& Observations = Interactor->GetObservationFeature();
	UE::Learning::FFeatureObject& Actions = Interactor->GetActionFeature();

	// Add Experience to Episode Buffer
	EpisodeBuffer->Push(
		Observations.FeatureBuffer(),
		Actions.FeatureBuffer(),
		Rewards->RewardBuffer(),
		ValidAgentSet);

	// Check for completion based on reaching the maximum episode length
	UE::Learning::Completion::EvaluateEndOfEpisodeCompletions(
		Completions->CompletionBuffer(),
		EpisodeBuffer->GetEpisodeStepNums(),
		EpisodeBuffer->GetMaxStepNum(),
		MaxStepsCompletion == ELearningAgentsCompletion::Truncation
		? UE::Learning::ECompletionMode::Truncated
		: UE::Learning::ECompletionMode::Terminated,
		ValidAgentSet);

	// Find the set of Instances that need to be reset
	ResetBuffer->SetResetInstancesFromCompletions(Completions->CompletionBuffer(), ValidAgentSet);

	if (ResetBuffer->GetResetInstanceNum() > 0)
	{
		// Encode Observations for completed Instances
		Interactor->EncodeObservations(ResetBuffer->GetResetInstances());

		const bool bReplayBufferFull = ReplayBuffer->AddEpisodes(
			Completions->CompletionBuffer(),
			Observations.FeatureBuffer(),
			*EpisodeBuffer,
			ResetBuffer->GetResetInstances());

		if (bReplayBufferFull)
		{
			UE::Learning::ETrainerResponse Response = Trainer->SendExperience(*ReplayBuffer, TrainerTimeout);

			if (Response != UE::Learning::ETrainerResponse::Success)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Error waiting to push experience to trainer. Check log for errors."), *GetName());
				bHasTrainingFailed = true;
				EndTraining();
				return;
			}

			ReplayBuffer->Reset();

			// Get Updated Policy
			Response = Trainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
			Policy->GetNetworkAsset()->ForceMarkDirty();

			if (Response == UE::Learning::ETrainerResponse::Completed)
			{
				UE_LOG(LogLearning, Display, TEXT("%s: Trainer completed training."), *GetName());
				DoneTraining();
				return;
			}
			else if (Response != UE::Learning::ETrainerResponse::Success)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for policy from trainer. Check log for errors."), *GetName());
				bHasTrainingFailed = true;
				EndTraining();
				return;
			}

			// Get Updated Critic
			if (Critic)
			{
				Response = Trainer->RecvCritic(Critic->GetCriticNetwork(), TrainerTimeout);
				Critic->GetNetworkAsset()->ForceMarkDirty();

				if (Response != UE::Learning::ETrainerResponse::Success)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Error waiting for critic from trainer. Check log for errors."), *GetName());
					bHasTrainingFailed = true;
					EndTraining();
					return;
				}
			}

			// Mark all agents for reset since we have a new policy
			ResetBuffer->SetResetInstances(Manager->GetAllAgentSet());
		}

		Manager->ResetAgents(ResetBuffer->GetResetInstances().ToArray());
	}
}

void ULearningAgentsTrainer::RunTraining(
	const FLearningAgentsTrainerTrainingSettings& TrainerTrainingSettings,
	const FLearningAgentsTrainerGameSettings& TrainerGameSettings,
	const FLearningAgentsTrainerPathSettings& TrainerPathSettings,
	const FLearningAgentsCriticSettings& CriticSettings,
	const bool bReinitializePolicyNetwork,
	const bool bReinitializeCriticNetwork,
	const bool bResetAgentsOnBegin)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (bHasTrainingFailed)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training has failed. Check log for errors."), *GetName());
		return;
	}

	// If we aren't training yet, then start training and do the first inference step.
	if (!IsTraining())
	{
		BeginTraining(
			TrainerTrainingSettings,
			TrainerGameSettings, 
			TrainerPathSettings, 
			CriticSettings, 
			bReinitializePolicyNetwork, 
			bReinitializeCriticNetwork,
			bResetAgentsOnBegin);

		if (!IsTraining())
		{
			// If IsTraining is false, then BeginTraining must have failed and we can't continue.
			return;
		}

		Policy->RunInference();
	}

	// Otherwise, do the regular training process.
	EvaluateCompletions();
	EvaluateRewards();
	ProcessExperience();
	Policy->RunInference();
}

float ULearningAgentsTrainer::GetReward(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (RewardEvaluatedAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not evaluated rewards. Did you run EvaluateRewards?"), *GetName(), AgentId);
		return 0.0f;
	}

	return Rewards->RewardBuffer()[AgentId];
}

bool ULearningAgentsTrainer::IsCompleted(const int32 AgentId, ELearningAgentsCompletion& OutCompletion) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutCompletion = ELearningAgentsCompletion::Termination;
		return false;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutCompletion = ELearningAgentsCompletion::Termination;
		return false;
	}

	if (CompletionEvaluatedAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not evaluated completions. Did you run EvaluateCompletions?"), *GetName(), AgentId);
		OutCompletion = ELearningAgentsCompletion::Termination;
		return false;
	}

	const UE::Learning::ECompletionMode CompletionMode = Completions->CompletionBuffer()[AgentId];

	if (CompletionMode == UE::Learning::ECompletionMode::Running)
	{
		OutCompletion = ELearningAgentsCompletion::Termination;
		return false;
	}
	else
	{
		OutCompletion = UE::Learning::Agents::GetLearningAgentsCompletion(CompletionMode);
		return true;
	}
}

bool ULearningAgentsTrainer::HasTrainingFailed() const
{
	return bHasTrainingFailed;
}

void ULearningAgentsTrainer::ResetEpisodes_Implementation(const TArray<int32>& AgentId)
{
	// Can be overridden to reset agent without blueprints
}
