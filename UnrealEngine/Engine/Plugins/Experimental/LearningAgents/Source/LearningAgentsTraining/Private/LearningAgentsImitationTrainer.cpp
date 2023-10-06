// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsImitationTrainer.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningArrayMap.h"
#include "LearningExperience.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningImitationTrainer.h"
#include "LearningAgentsRecording.h"
#include "LearningAgentsPolicy.h"
#include "LearningNeuralNetworkObject.h"
#include "Misc/Paths.h"

ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer() : Super(FObjectInitializer::Get()) {}
ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsImitationTrainer::~ULearningAgentsImitationTrainer() = default;

void ULearningAgentsImitationTrainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsTraining())
	{
		EndTraining();
	}

	Super::EndPlay(EndPlayReason);
}

void ULearningAgentsImitationTrainer::BeginTraining(
	ULearningAgentsPolicy* InPolicy, 
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings,
	const bool bReinitializePolicyNetwork)
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot begin training as we are already training!"), *GetName());
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

	if (!Recording)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Recording is nullptr."), *GetName());
		return;
	}

	if (Recording->Records.IsEmpty())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Recording is empty!"), *GetName());
		return;
	}

	Policy = InPolicy;

	// Record Timeout Setting

	TrainerTimeout = ImitationTrainerSettings.TrainerCommunicationTimeout;

	// Check Paths

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::FileExists(PythonExecutablePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python executable \"%s\"."), *GetName(), *PythonExecutablePath);
		return;
	}
	const FString PythonContentPath = UE::Learning::Trainer::GetPythonContentPath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(PythonContentPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find LearningAgents plugin Content \"%s\"."), *GetName(), *PythonContentPath);
		return;
	}

	const FString SitePackagesPath = UE::Learning::Trainer::GetSitePackagesPath(ImitationTrainerPathSettings.GetEditorEnginePath());

	if (!FPaths::DirectoryExists(SitePackagesPath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Can't find Python site-packages \"%s\"."), *GetName(), *SitePackagesPath);
		return;
	}

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(ImitationTrainerPathSettings.GetIntermediatePath());

	// Sizes

	const int32 PolicyInputNum = Policy->GetPolicyNetwork().GetInputNum();
	const int32 PolicyOutputNum = Policy->GetPolicyNetwork().GetOutputNum();

	// Get Number of Steps

	int32 TotalSampleNum = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != PolicyInputNum)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for observations, got %i, policy expected %i."), *GetName(), Record.ObservationDimNum, PolicyInputNum);
			continue;
		}

		if (Record.ActionDimNum != PolicyOutputNum / 2)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for actions, got %i, policy expected %i."), *GetName(), Record.ActionDimNum, PolicyOutputNum / 2);
			continue;
		}

		TotalSampleNum += Record.SampleNum;
	}

	if (TotalSampleNum == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Recording contains no valid training data."), *GetName());
		return;
	}

	// Copy into Flat Arrays

	RecordedObservations.SetNumUninitialized({ TotalSampleNum, PolicyInputNum });
	RecordedActions.SetNumUninitialized({ TotalSampleNum, PolicyOutputNum / 2 });

	int32 SampleIdx = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != PolicyInputNum) { continue; }
		if (Record.ActionDimNum != PolicyOutputNum / 2) { continue; }

		UE::Learning::Array::Copy(RecordedObservations.Slice(SampleIdx, Record.SampleNum), Record.Observations);
		UE::Learning::Array::Copy(RecordedActions.Slice(SampleIdx, Record.SampleNum), Record.Actions);
		SampleIdx += Record.SampleNum;
	}

	UE_LEARNING_CHECK(SampleIdx == TotalSampleNum);

	// Begin Training Properly

	UE_LOG(LogLearning, Display, TEXT("%s: Imitation Training Started"), *GetName());


	UE::Learning::FImitationTrainerTrainingSettings ImitationTrainingSettings;
	ImitationTrainingSettings.IterationNum = ImitationTrainerTrainingSettings.NumberOfIterations;
	ImitationTrainingSettings.LearningRateActor = ImitationTrainerTrainingSettings.LearningRate;
	ImitationTrainingSettings.LearningRateDecay = ImitationTrainerTrainingSettings.LearningRateDecay;
	ImitationTrainingSettings.WeightDecay = ImitationTrainerTrainingSettings.WeightDecay;
	ImitationTrainingSettings.BatchSize = ImitationTrainerTrainingSettings.BatchSize;
	ImitationTrainingSettings.Seed = ImitationTrainerTrainingSettings.RandomSeed;
	ImitationTrainingSettings.Device = UE::Learning::Agents::GetTrainerDevice(ImitationTrainerTrainingSettings.Device);
	ImitationTrainingSettings.bUseTensorboard = ImitationTrainerTrainingSettings.bUseTensorboard;

	const UE::Learning::EImitationTrainerFlags TrainerFlags = 
		bReinitializePolicyNetwork ? 
		UE::Learning::EImitationTrainerFlags::None : 
		UE::Learning::EImitationTrainerFlags::UseInitialPolicyNetwork;

	ImitationTrainer = MakeUnique<UE::Learning::FSharedMemoryImitationTrainer>(
		GetName(),
		PythonExecutablePath,
		SitePackagesPath,
		PythonContentPath,
		IntermediatePath,
		RecordedObservations.Num<0>(),
		RecordedObservations.Num<1>(),
		RecordedActions.Num<1>(),
		ImitationTrainingSettings);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending / Receiving initial policy..."), *GetName());

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	if (bReinitializePolicyNetwork)
	{
		Response = ImitationTrainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
		Policy->GetNetworkAsset()->ForceMarkDirty();
	}
	else
	{
		Response = ImitationTrainer->SendPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
	}

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending or receiving policy from trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		ImitationTrainer->Terminate();
		return;
	}

	UE_LOG(LogLearning, Display, TEXT("%s: Sending Experience..."), *GetName());

	// Send Experience

	Response = ImitationTrainer->SendExperience(RecordedObservations, RecordedActions, TrainerTimeout);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending experience to trainer: %s. Check log for errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		ImitationTrainer->Terminate();
		return;
	}

	bIsTraining = true;
}

void ULearningAgentsImitationTrainer::DoneTraining()
{
	if (IsTraining())
	{
		// Wait for Trainer to finish
		ImitationTrainer->Wait(1.0f);

		// If not finished in time, terminate
		ImitationTrainer->Terminate();

		bIsTraining = false;
	}
}

void ULearningAgentsImitationTrainer::EndTraining()
{
	if (IsTraining())
	{
		UE_LOG(LogLearning, Display, TEXT("%s: Stopping training..."), *GetName());
		ImitationTrainer->SendStop();
		DoneTraining();
	}
}

void ULearningAgentsImitationTrainer::IterateTraining()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsImitationTrainer::IterateTraining);

	if (!IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training not running."), *GetName());
		return;
	}

	if (ImitationTrainer->HasPolicyOrCompleted())
	{
		UE::Learning::ETrainerResponse Response = ImitationTrainer->RecvPolicy(Policy->GetPolicyNetwork(), TrainerTimeout);
		Policy->GetNetworkAsset()->ForceMarkDirty();

		if (Response == UE::Learning::ETrainerResponse::Completed)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Trainer completed training."), *GetName());
			DoneTraining();
			return;
		}
		else if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving policy from trainer. Check log for errors."), *GetName());
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}
	}
}

void ULearningAgentsImitationTrainer::RunTraining(
	ULearningAgentsPolicy* InPolicy,
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings,
	const bool bReinitializePolicyNetwork)
{
	if (bHasTrainingFailed)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training has failed. Check log for errors."), *GetName());
		return;
	}

	// If we aren't training yet, then start training and do the first inference step.
	if (!IsTraining())
	{
		BeginTraining(
			InPolicy,
			Recording,
			ImitationTrainerSettings,
			ImitationTrainerTrainingSettings,
			ImitationTrainerPathSettings,
			bReinitializePolicyNetwork);

		if (!IsTraining())
		{
			// If IsTraining is false, then BeginTraining must have failed and we can't continue.
			return;
		}
	}

	// Otherwise, do the regular training process.
	IterateTraining();
}

bool ULearningAgentsImitationTrainer::IsTraining() const
{
	return bIsTraining;
}

bool ULearningAgentsImitationTrainer::HasTrainingFailed() const
{
	return bHasTrainingFailed;
}
