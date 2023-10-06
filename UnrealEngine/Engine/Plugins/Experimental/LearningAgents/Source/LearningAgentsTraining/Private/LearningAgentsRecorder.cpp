// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsRecorder.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsRecording.h"
#include "LearningAgentsHelpers.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"
#include "LearningTrainer.h"

#include "UObject/Package.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

FLearningAgentsRecorderPathSettings::FLearningAgentsRecorderPathSettings()
{
	IntermediateRelativePath.Path = FPaths::ProjectIntermediateDir();
}

TLearningArrayView<1, float> ULearningAgentsRecorder::FAgentRecordBuffer::GetObservation(const int32 SampleIdx)
{
	return Observations[SampleIdx / ChunkSize][SampleIdx % ChunkSize];
}

TLearningArrayView<1, float> ULearningAgentsRecorder::FAgentRecordBuffer::GetAction(const int32 SampleIdx)
{
	return Actions[SampleIdx / ChunkSize][SampleIdx % ChunkSize];
}

TLearningArrayView<1, const float> ULearningAgentsRecorder::FAgentRecordBuffer::GetObservation(const int32 SampleIdx) const
{
	return Observations[SampleIdx / ChunkSize][SampleIdx % ChunkSize];
}

TLearningArrayView<1, const float> ULearningAgentsRecorder::FAgentRecordBuffer::GetAction(const int32 SampleIdx) const
{
	return Actions[SampleIdx / ChunkSize][SampleIdx % ChunkSize];
}

void ULearningAgentsRecorder::FAgentRecordBuffer::Push(
	const TLearningArrayView<1, const float> Observation,
	const TLearningArrayView<1, const float> Action)
{
	if (SampleNum / ChunkSize <= Observations.Num())
	{
		Observations.AddDefaulted_GetRef().SetNumUninitialized({ ChunkSize, Observation.Num() });
		Actions.AddDefaulted_GetRef().SetNumUninitialized({ ChunkSize, Action.Num() });
	}

	UE::Learning::Array::Copy(GetObservation(SampleNum), Observation);
	UE::Learning::Array::Copy(GetAction(SampleNum), Action);
	SampleNum++;
}

bool ULearningAgentsRecorder::FAgentRecordBuffer::IsEmpty() const
{
	return SampleNum == 0;
}

void ULearningAgentsRecorder::FAgentRecordBuffer::Empty()
{
	SampleNum = 0;
	Observations.Empty();
	Actions.Empty();
}

void ULearningAgentsRecorder::FAgentRecordBuffer::CopyToRecord(FLearningAgentsRecord& Record) const
{
	UE_LEARNING_CHECK(SampleNum > 0);

	Record.SampleNum = SampleNum;
	Record.ObservationDimNum = GetObservation(0).Num();
	Record.ActionDimNum = GetAction(0).Num();
	Record.Observations.SetNumUninitialized({ SampleNum, Observations[0].Num<1>() });
	Record.Actions.SetNumUninitialized({ SampleNum, Actions[0].Num<1>() });

	for (int32 SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		UE::Learning::Array::Copy(Record.Observations[SampleIdx], GetObservation(SampleIdx));
		UE::Learning::Array::Copy(Record.Actions[SampleIdx], GetAction(SampleIdx));
	}
}

ULearningAgentsRecorder::ULearningAgentsRecorder() : Super(FObjectInitializer::Get()) {}
ULearningAgentsRecorder::ULearningAgentsRecorder(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsRecorder::~ULearningAgentsRecorder() = default;

void ULearningAgentsRecorder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsRecording())
	{
		EndRecording();
	}

	Super::EndPlay(EndPlayReason);
}

void ULearningAgentsRecorder::SetupRecorder(
	ULearningAgentsInteractor* InInteractor,
	const FLearningAgentsRecorderPathSettings& RecorderPathSettings,
	ULearningAgentsRecording* RecordingAsset)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already performed!"), *GetName());
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

	Interactor = InInteractor;

	if (RecordingAsset)
	{
		Recording = RecordingAsset;
	}
	else
	{
		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsRecording::StaticClass(), TEXT("Recording"), EUniqueObjectNameOptions::GloballyUnique);

		Recording = NewObject<ULearningAgentsRecording>(this, UniqueName);
	}

	RecordingDirectory = RecorderPathSettings.IntermediateRelativePath.Path / TEXT("LearningAgents") / RecorderPathSettings.RecordingsSubdirectory;

	RecordBuffers.Empty();
	RecordBuffers.SetNum(Manager->GetMaxAgentNum());

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsRecorder::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		for (const int32 AgentId : AgentIds)
		{
			if (!RecordBuffers[AgentId].IsEmpty())
			{
				RecordBuffers[AgentId].CopyToRecord(Recording->Records.AddDefaulted_GetRef());
				RecordBuffers[AgentId].Empty();
			}
		}

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsRecorder::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		for (const int32 AgentId : AgentIds)
		{
			if (!RecordBuffers[AgentId].IsEmpty())
			{
				RecordBuffers[AgentId].CopyToRecord(Recording->Records.AddDefaulted_GetRef());
				RecordBuffers[AgentId].Empty();
			}
		}

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

void ULearningAgentsRecorder::AddExperience()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!IsRecording())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Trying to add experience but we aren't currently recording. Call BeginRecording before AddExperience."), *GetName());
		return;
	}

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0 ||
			Interactor->GetActionEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not made observations and taken actions so experience will not be recorded for it."), *GetName(), AgentId);
			continue;
		}

		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] !=
			Interactor->GetActionEncodingAgentIteration()[AgentId])
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i does not have matching iteration numbers for observations and actions so experience will not be recorded for it."), *GetName(), AgentId);
			continue;
		}

		RecordBuffers[AgentId].Push(
			Interactor->GetObservationFeature().FeatureBuffer()[AgentId],
			Interactor->GetActionFeature().FeatureBuffer()[AgentId]);
	}
}

bool ULearningAgentsRecorder::IsRecording() const
{
	return bIsRecording;
}

void ULearningAgentsRecorder::EndRecording()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!IsRecording())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot end recording as we are not currently recording!"), *GetName());
		return;
	}

	// Write to Recording

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (!RecordBuffers[AgentId].IsEmpty())
		{
			RecordBuffers[AgentId].CopyToRecord(Recording->Records.AddDefaulted_GetRef());
			RecordBuffers[AgentId].Empty();
		}
	}

	bIsRecording = false;
}

void ULearningAgentsRecorder::LoadRecordingFromFile(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->LoadRecordingFromFile(File);
}

void ULearningAgentsRecorder::SaveRecordingToFile(const FFilePath& File) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->SaveRecordingToFile(File);
}

void ULearningAgentsRecorder::AppendRecordingFromFile(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->AppendRecordingFromFile(File);
}

void ULearningAgentsRecorder::UseRecordingAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!RecordingAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is nullptr."), *GetName());
		return;
	}

	if (RecordingAsset == Recording)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current recording."), *GetName());
		return;
	}

	Recording = RecordingAsset;
}

void ULearningAgentsRecorder::LoadRecordingFromAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->LoadRecordingFromAsset(RecordingAsset);
}

void ULearningAgentsRecorder::SaveRecordingToAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->SaveRecordingToAsset(RecordingAsset);
}

void ULearningAgentsRecorder::AppendRecordingToAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Recording->AppendRecordingToAsset(RecordingAsset);
}

void ULearningAgentsRecorder::BeginRecording(bool bReinitializeRecording)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (IsRecording())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot begin recording as we are already Recording!"), *GetName());
		return;
	}

	if (bReinitializeRecording)
	{
		Recording->Records.Empty();
		Recording->ForceMarkDirty();
	}

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		RecordBuffers[AgentId].Empty();
	}

	bIsRecording = true;
}

const ULearningAgentsRecording* ULearningAgentsRecorder::GetCurrentRecording() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return nullptr;
	}

	return Recording;
}