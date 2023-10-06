// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineQueue.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineSetting.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MovieRenderPipelineCoreModule.h"

#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineQueue)

UMoviePipelineQueue::UMoviePipelineQueue()
	: QueueSerialNumber(0)
	, bIsDirty(false)
{
	// Ensure instances are always transactional
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetFlags(RF_Transactional);
	}

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UMoviePipelineQueue::OnAnyObjectModified);
#endif
}

UMoviePipelineExecutorJob* UMoviePipelineQueue::AllocateNewJob(TSubclassOf<UMoviePipelineExecutorJob> InJobType)
{
	if (!ensureAlwaysMsgf(InJobType, TEXT("Failed to specify a Job Type. Use the default in project setting or UMoviePipelineExecutorJob.")))
	{
		InJobType = UMoviePipelineExecutorJob::StaticClass();
	}

#if WITH_EDITOR
	Modify();
#endif

	UMoviePipelineExecutorJob* NewJob = NewObject<UMoviePipelineExecutorJob>(this, InJobType);
	NewJob->SetFlags(RF_Transactional);

	Jobs.Add(NewJob);
	QueueSerialNumber++;

	return NewJob;
}

void UMoviePipelineQueue::DeleteJob(UMoviePipelineExecutorJob* InJob)
{
	if (!InJob)
	{
		return;
	}

#if WITH_EDITOR
	Modify();
#endif

	Jobs.Remove(InJob);
	QueueSerialNumber++;
}

void UMoviePipelineQueue::DeleteAllJobs()
{
#if WITH_EDITOR
	Modify();
#endif

	Jobs.Empty();
	QueueSerialNumber++;
}

UMoviePipelineExecutorJob* UMoviePipelineQueue::DuplicateJob(UMoviePipelineExecutorJob* InJob)
{
	if (!InJob)
	{
		return nullptr;
	}

#if WITH_EDITOR
	Modify();
#endif

	UMoviePipelineExecutorJob* NewJob = CastChecked<UMoviePipelineExecutorJob>(StaticDuplicateObject(InJob, this));

	// Duplication causes the DisplayNames to get renamed to the asset name (required to support both duplicating
	// and renaming objects in the Content Browser). So after using the normal duplication, we run our custom
	// CopyFrom code to transfer the configurations over (which do a display name fixup).
	NewJob->GetConfiguration()->CopyFrom(InJob->GetConfiguration());
	for (int32 Index = 0; Index < NewJob->ShotInfo.Num(); Index++)
	{
		if (InJob->ShotInfo[Index]->GetShotOverrideConfiguration())
		{
			NewJob->ShotInfo[Index]->GetShotOverrideConfiguration()->CopyFrom(InJob->ShotInfo[Index]->GetShotOverrideConfiguration());
		}
	}

	NewJob->OnDuplicated();
	Jobs.Add(NewJob);

	QueueSerialNumber++;
	return NewJob;
}

void UMoviePipelineQueue::CopyFrom(UMoviePipelineQueue* InQueue)
{
	if (!InQueue)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Cannot copy the contents of a null queue."));
		return;
	}

	// The copy should reflect the input queue's origin (ie, the queue asset it was originally based off of). Setting
	// the origin to the input queue would change the meaning of what the origin is.
	QueueOrigin = InQueue->QueueOrigin;

#if WITH_EDITOR
	Modify();
#endif

	Jobs.Empty();
	for (UMoviePipelineExecutorJob* Job : InQueue->GetJobs())
	{
		DuplicateJob(Job);
	}

	// Ensure the serial number gets bumped at least once so the UI refreshes in case
	// the queue we are copying from was empty.
	QueueSerialNumber++;
}

void UMoviePipelineQueue::SetJobIndex(UMoviePipelineExecutorJob* InJob, int32 Index)
{
#if WITH_EDITOR
	Modify();
#endif

	int32 CurrentIndex = INDEX_NONE;
	Jobs.Find(InJob, CurrentIndex);

	if (CurrentIndex == INDEX_NONE)
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Cannot find Job %s in queue"), *InJob->JobName), ELogVerbosity::Error);
		return;
	}

	if (Index > CurrentIndex)
	{
		--Index;
	}

	Jobs.Remove(InJob);
	Jobs.Insert(InJob, Index);

	// Ensure the serial number gets bumped at least once so the UI refreshes in case
	// the queue we are copying from was empty.
	QueueSerialNumber++;
}

void UMoviePipelineQueue::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	// Reset dirty state after saving
	bIsDirty = false;
}

#if WITH_EDITOR
void UMoviePipelineQueue::OnAnyObjectModified(UObject* InModifiedObject)
{
	// Mark as dirty if this queue or any of its owned objects have been modified
	if (InModifiedObject)
	{
		if (InModifiedObject->IsIn(this) || (InModifiedObject == this))
		{
			bIsDirty = true;
		}
	}
}

void UMoviePipelineExecutorJob::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMoviePipelineExecutorJob, Sequence))
	{
		// Call our Set function so that we rebuild the shot mask.
		SetSequence(Sequence);
	}

	// We save the config on this object after each property change. This makes the variables flagged as config
	// save even though we're editing them through a normal details panel. This is a nicer user experience for
	// fields that don't change often but do need to be per job.
	SaveConfig();
}
#endif

void UMoviePipelineExecutorJob::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

#if WITH_EDITOR
	VariableAssignments->UpdateGraphVariableOverrides();
#endif
}

void UMoviePipelineExecutorJob::PostLoad()
{
	Super::PostLoad();
	
	if (VariableAssignments)
	{
		VariableAssignments->SetGraphConfig(GraphPreset.LoadSynchronous());
	}

	// Update the job's variable overrides whenever the graph's variables change
	if (UMovieGraphConfig* Config = GraphPreset.LoadSynchronous())
	{
#if WITH_EDITOR
		Config->OnGraphVariablesChangedDelegate.AddUObject(VariableAssignments, &UMovieJobVariableAssignmentContainer::UpdateGraphVariableOverrides);
#endif
	}

	// TODO: The variable assignments need to be updated here, but currently GraphPreset.LoadSynchronous() does not
	// provide a fully-loaded object (ie, GraphPreset will only be partially loaded at this point).
}

void UMoviePipelineExecutorJob::SetSequence(FSoftObjectPath InSequence)
{
	Sequence = InSequence;

	// Rebuild our shot mask.
	ShotInfo.Reset();

	ULevelSequence* LoadedSequence = Cast<ULevelSequence>(Sequence.TryLoad());
	if (!LoadedSequence)
	{
		return;
	}

	bool bShotsChanged = false;
	UMoviePipelineBlueprintLibrary::UpdateJobShotListFromSequence(LoadedSequence, this, bShotsChanged);
	
	if (UMoviePipelineQueue* OwningQueue = GetTypedOuter<UMoviePipelineQueue>())
	{
		OwningQueue->InvalidateSerialNumber();
	}
}

void UMoviePipelineExecutorJob::SetConfiguration(UMoviePipelinePrimaryConfig* InPreset)
{
	if (InPreset)
	{
		Configuration->CopyFrom(InPreset);
		PresetOrigin = nullptr;
	}
}

void UMoviePipelineExecutorJob::SetPresetOrigin(UMoviePipelinePrimaryConfig* InPreset)
{
	if (InPreset)
	{
		Configuration->CopyFrom(InPreset);
		PresetOrigin = TSoftObjectPtr<UMoviePipelinePrimaryConfig>(InPreset);
	}
}

void UMoviePipelineExecutorJob::OnDuplicated_Implementation()
{
	UserData = FString();
	StatusMessage = FString();
	StatusProgress = 0.f;
	SetConsumed(false);
}

UMoviePipelineShotConfig* UMoviePipelineExecutorShot::AllocateNewShotOverrideConfig(TSubclassOf<UMoviePipelineShotConfig> InConfigType)
{
	if (!ensureAlwaysMsgf(InConfigType, TEXT("Failed to specify a Config Type. Use the default in project setting or UMoviePipelineShotConfig.")))
	{
		InConfigType = UMoviePipelineShotConfig::StaticClass();
	}

#if WITH_EDITOR
	Modify();
#endif

	ShotOverrideConfig = NewObject<UMoviePipelineShotConfig>(this, InConfigType);
	ShotOverrideConfig->SetFlags(RF_Transactional);

	return ShotOverrideConfig;
}

void UMoviePipelineExecutorShot::SetShotOverrideConfiguration(UMoviePipelineShotConfig* InPreset)
{
	if (InPreset)
	{
		if (ShotOverrideConfig == nullptr)
		{
			AllocateNewShotOverrideConfig(UMoviePipelineShotConfig::StaticClass());
		}

		ShotOverrideConfig->CopyFrom(InPreset);
	}
	else
	{
		ShotOverrideConfig = nullptr;
	}

	ShotOverridePresetOrigin = nullptr;
}

void UMoviePipelineExecutorShot::SetShotOverridePresetOrigin(UMoviePipelineShotConfig* InPreset)
{
	if (InPreset)
	{
		if (ShotOverrideConfig == nullptr)
		{
			AllocateNewShotOverrideConfig(UMoviePipelineShotConfig::StaticClass());
		}

		ShotOverrideConfig->CopyFrom(InPreset);
		ShotOverridePresetOrigin = TSoftObjectPtr<UMoviePipelineShotConfig>(InPreset);
	}
	else
	{
		ShotOverridePresetOrigin.Reset();
	}
}

void UMoviePipelineExecutorShot::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

#if WITH_EDITOR
	VariableAssignments->UpdateGraphVariableOverrides();
#endif
}

void UMoviePipelineExecutorShot::PostLoad()
{
	Super::PostLoad();

	if (VariableAssignments)
	{
		VariableAssignments->SetGraphConfig(GraphPreset.LoadSynchronous());
	}

	// Update the job's variable overrides whenever the graph's variables change
	if (UMovieGraphConfig* Config = GraphPreset.LoadSynchronous())
	{
#if WITH_EDITOR
		Config->OnGraphVariablesChangedDelegate.AddUObject(VariableAssignments, &UMovieJobVariableAssignmentContainer::UpdateGraphVariableOverrides);
#endif
	}

	// TODO: The variable assignments need to be updated here, but currently GraphPreset.LoadSynchronous() does not
	// provide a fully-loaded object (ie, GraphPreset will only be partially loaded at this point).
}
