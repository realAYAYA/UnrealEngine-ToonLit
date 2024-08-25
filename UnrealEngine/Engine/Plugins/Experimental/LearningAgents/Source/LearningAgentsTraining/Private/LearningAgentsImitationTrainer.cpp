// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsImitationTrainer.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningExperience.h"
#include "LearningLog.h"
#include "LearningImitationTrainer.h"
#include "LearningAgentsRecording.h"
#include "LearningAgentsPolicy.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer() : Super(FObjectInitializer::Get()) {}
ULearningAgentsImitationTrainer::ULearningAgentsImitationTrainer(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsImitationTrainer::~ULearningAgentsImitationTrainer() = default;

void ULearningAgentsImitationTrainer::BeginDestroy()
{
	if (IsTraining())
	{
		EndTraining();
	}

	Super::BeginDestroy();
}

ULearningAgentsImitationTrainer* ULearningAgentsImitationTrainer::MakeImitationTrainer(
	ULearningAgentsManager* InManager,
	ULearningAgentsInteractor* InInteractor,
	ULearningAgentsPolicy* InPolicy,
	TSubclassOf<ULearningAgentsImitationTrainer> Class)
{
	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeImitationTrainer: InManager is nullptr."));
		return nullptr;
	}

	if (!Class)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeImitationTrainer: Class is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManager, Class, TEXT("ImitationTrainer"), EUniqueObjectNameOptions::GloballyUnique);

	ULearningAgentsImitationTrainer* ImitationTrainer = NewObject<ULearningAgentsImitationTrainer>(InManager, Class, UniqueName);
	if (!ImitationTrainer) { return nullptr; }

	ImitationTrainer->SetupImitationTrainer(
		InManager,
		InInteractor,
		InPolicy);

	return ImitationTrainer->IsSetup() ? ImitationTrainer : nullptr;
}

void ULearningAgentsImitationTrainer::SetupImitationTrainer(
	ULearningAgentsManager* InManager,
	ULearningAgentsInteractor* InInteractor,
	ULearningAgentsPolicy* InPolicy)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already performed!"), *GetName());
		return;
	}

	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InManager is nullptr."), *GetName());
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

	Manager = InManager;
	Interactor = InInteractor;
	Policy = InPolicy;

	bIsSetup = true;

	Manager->AddListener(this);
}

void ULearningAgentsImitationTrainer::BeginTraining(
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings)
{
	if (!PLATFORM_WINDOWS)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Training currently only supported on Windows."), *GetName());
		return;
	}

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (IsTraining())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot begin training as we are already training!"), *GetName());
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

	// Record Timeout Setting

	TrainerTimeout = ImitationTrainerSettings.TrainerCommunicationTimeout;

	// Check Paths

	const FString PythonExecutablePath = UE::Learning::Trainer::GetPythonExecutablePath(ImitationTrainerPathSettings.GetIntermediatePath());

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

	const FString IntermediatePath = UE::Learning::Trainer::GetIntermediatePath(ImitationTrainerPathSettings.GetIntermediatePath());

	// Sizes

	const int32 ObservationNum = Interactor->GetObservationVectorSize();
	const int32 ActionNum = Interactor->GetActionVectorSize();
	const int32 MemoryStateNum = Policy->GetMemoryStateSize();

	// Get Number of Steps

	int32 TotalEpisodeNum = 0;
	int32 TotalStepNum = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != ObservationNum)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for observations, got %i, policy expected %i."), *GetName(), Record.ObservationDimNum, ObservationNum);
			continue;
		}

		if (Record.ActionDimNum != ActionNum)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Record has wrong dimensionality for actions, got %i, policy expected %i."), *GetName(), Record.ActionDimNum, ActionNum);
			continue;
		}

		TotalEpisodeNum++;
		TotalStepNum += Record.StepNum;
	}

	if (TotalStepNum == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Recording contains no valid training data."), *GetName());
		return;
	}

	// Copy into Flat Arrays

	TLearningArray<1, int32> RecordedEpisodeStarts;
	TLearningArray<1, int32> RecordedEpisodeLengths;
	TLearningArray<2, float> RecordedObservations;
	TLearningArray<2, float> RecordedActions;

	RecordedEpisodeStarts.SetNumUninitialized({ TotalEpisodeNum });
	RecordedEpisodeLengths.SetNumUninitialized({ TotalEpisodeNum });
	RecordedObservations.SetNumUninitialized({ TotalStepNum, ObservationNum });
	RecordedActions.SetNumUninitialized({ TotalStepNum, ActionNum });

	int32 EpisodeIdx = 0;
	int32 StepIdx = 0;
	for (const FLearningAgentsRecord& Record : Recording->Records)
	{
		if (Record.ObservationDimNum != ObservationNum) { continue; }
		if (Record.ActionDimNum != ActionNum) { continue; }

		TLearningArrayView<2, const float> ObservationsView = TLearningArrayView<2, const float>(Record.ObservationData.GetData(), { Record.StepNum, Record.ObservationDimNum });
		TLearningArrayView<2, const float> ActionsView = TLearningArrayView<2, const float>(Record.ActionData.GetData(), { Record.StepNum, Record.ActionDimNum });

		RecordedEpisodeStarts[EpisodeIdx] = StepIdx;
		RecordedEpisodeLengths[EpisodeIdx] = Record.StepNum;
		UE::Learning::Array::Copy(RecordedObservations.Slice(StepIdx, Record.StepNum), ObservationsView);
		UE::Learning::Array::Copy(RecordedActions.Slice(StepIdx, Record.StepNum), ActionsView);
		EpisodeIdx++;
		StepIdx += Record.StepNum;
	}

	UE_LEARNING_CHECK(EpisodeIdx == TotalEpisodeNum);
	UE_LEARNING_CHECK(StepIdx == TotalStepNum);

	// Begin Training Properly

	UE_LOG(LogLearning, Display, TEXT("%s: Imitation Training Started"), *GetName());


	UE::Learning::FImitationTrainerTrainingSettings ImitationTrainingSettings;
	ImitationTrainingSettings.IterationNum = ImitationTrainerTrainingSettings.NumberOfIterations;
	ImitationTrainingSettings.LearningRate = ImitationTrainerTrainingSettings.LearningRate;
	ImitationTrainingSettings.LearningRateDecay = ImitationTrainerTrainingSettings.LearningRateDecay;
	ImitationTrainingSettings.WeightDecay = ImitationTrainerTrainingSettings.WeightDecay;
	ImitationTrainingSettings.BatchSize = ImitationTrainerTrainingSettings.BatchSize;
	ImitationTrainingSettings.Window = ImitationTrainerTrainingSettings.Window;
	ImitationTrainingSettings.ActionRegularizationWeight = ImitationTrainerTrainingSettings.ActionRegularizationWeight;
	ImitationTrainingSettings.ActionEntropyWeight = ImitationTrainerTrainingSettings.ActionEntropyWeight;
	ImitationTrainingSettings.Seed = ImitationTrainerTrainingSettings.RandomSeed;
	ImitationTrainingSettings.Device = UE::Learning::Agents::GetTrainerDevice(ImitationTrainerTrainingSettings.Device);
	ImitationTrainingSettings.bUseTensorboard = ImitationTrainerTrainingSettings.bUseTensorboard;
	ImitationTrainingSettings.bSaveSnapshots = ImitationTrainerTrainingSettings.bSaveSnapshots;

	ImitationTrainer = MakeUnique<UE::Learning::FSharedMemoryImitationTrainer>(
		GetName(),
		PythonExecutablePath,
		TEXT(""),
		PythonContentPath,
		IntermediatePath,
		TotalEpisodeNum,
		TotalStepNum,
		ObservationNum,
		ActionNum,
		MemoryStateNum,
		*Policy->GetPolicyNetworkAsset()->NeuralNetworkData,
		*Policy->GetEncoderNetworkAsset()->NeuralNetworkData,
		*Policy->GetDecoderNetworkAsset()->NeuralNetworkData,
		Interactor->GetObservationSchema(),
		Interactor->GetObservationSchemaElement(),
		Interactor->GetActionSchema(),
		Interactor->GetActionSchemaElement(),
		ImitationTrainingSettings);

	UE_LOG(LogLearning, Display, TEXT("%s: Sending / Receiving initial policy..."), *GetName());

	UE::Learning::ETrainerResponse Response = UE::Learning::ETrainerResponse::Success;

	Response = ImitationTrainer->SendPolicy(*Policy->GetPolicyNetworkAsset()->NeuralNetworkData, TrainerTimeout);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending policy to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		ImitationTrainer->Terminate();
		bHasTrainingFailed = true;
		return;
	}

	Response = ImitationTrainer->SendEncoder(*Policy->GetEncoderNetworkAsset()->NeuralNetworkData, TrainerTimeout);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending encoder to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		ImitationTrainer->Terminate();
		bHasTrainingFailed = true;
		return;
	}

	Response = ImitationTrainer->SendDecoder(*Policy->GetDecoderNetworkAsset()->NeuralNetworkData, TrainerTimeout);
	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending decoder to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		ImitationTrainer->Terminate();
		bHasTrainingFailed = true;
		return;
	}

	UE_LOG(LogLearning, Display, TEXT("%s: Sending Experience..."), *GetName());

	// Send Experience

	Response = ImitationTrainer->SendExperience(
		RecordedEpisodeStarts,
		RecordedEpisodeLengths,
		RecordedObservations, 
		RecordedActions, 
		TrainerTimeout);

	if (Response != UE::Learning::ETrainerResponse::Success)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Error sending experience to trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
		bHasTrainingFailed = true;
		ImitationTrainer->Terminate();
		return;
	}

	bIsTraining = true;
}

void ULearningAgentsImitationTrainer::DoneTraining()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

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
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

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

	if (ImitationTrainer->HasPolicyOrCompleted())
	{
		UE::Learning::ETrainerResponse Response = ImitationTrainer->RecvPolicy(*Policy->GetPolicyNetworkAsset()->NeuralNetworkData, TrainerTimeout);
		if (Response == UE::Learning::ETrainerResponse::Completed)
		{
			UE_LOG(LogLearning, Display, TEXT("%s: Trainer completed training."), *GetName());
			DoneTraining();
			return;
		}
		else if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving policy from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}
		Policy->GetPolicyNetworkAsset()->ForceMarkDirty();

		Response = ImitationTrainer->RecvEncoder(*Policy->GetEncoderNetworkAsset()->NeuralNetworkData, TrainerTimeout);
		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving encoder from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}
		Policy->GetEncoderNetworkAsset()->ForceMarkDirty();

		Response = ImitationTrainer->RecvDecoder(*Policy->GetDecoderNetworkAsset()->NeuralNetworkData, TrainerTimeout);
		if (Response != UE::Learning::ETrainerResponse::Success)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Error receiving decoder from trainer: %s. Check log for additional errors."), *GetName(), UE::Learning::Trainer::GetResponseString(Response));
			bHasTrainingFailed = true;
			EndTraining();
			return;
		}
		Policy->GetDecoderNetworkAsset()->ForceMarkDirty();
	}
}

void ULearningAgentsImitationTrainer::RunTraining(
	const ULearningAgentsRecording* Recording,
	const FLearningAgentsImitationTrainerSettings& ImitationTrainerSettings,
	const FLearningAgentsImitationTrainerTrainingSettings& ImitationTrainerTrainingSettings,
	const FLearningAgentsTrainerPathSettings& ImitationTrainerPathSettings)
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
			Recording,
			ImitationTrainerSettings,
			ImitationTrainerTrainingSettings,
			ImitationTrainerPathSettings);

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
