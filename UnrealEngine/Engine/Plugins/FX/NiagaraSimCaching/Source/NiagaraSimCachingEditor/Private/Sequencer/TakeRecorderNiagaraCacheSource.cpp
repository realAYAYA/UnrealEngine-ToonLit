// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/TakeRecorderNiagaraCacheSource.h"

#include "Niagara/Sequencer/MovieSceneNiagaraCacheTrack.h"
#include "Sequencer/MovieSceneNiagaraTrackRecorder.h"

#include "LevelSequence.h"
#include "MovieSceneFolder.h"
#include "NiagaraComponent.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Framework/Notifications/NotificationManager.h" 


#define LOCTEXT_NAMESPACE "TakeRecorderNiagaraCacheSource"

UTakeRecorderNiagaraCacheSource::UTakeRecorderNiagaraCacheSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	TrackTint = FColor(0.0f, 125.0f, 255.0f, 65.0f);
}

TArray<UTakeRecorderSource*> UTakeRecorderNiagaraCacheSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InParentSequence, FManifestSerializer* InManifestSerializer)
{
	if (SystemToRecord)
	{
		UMovieScene* MovieScene = InSequence->GetMovieScene();
		//FGuid ObjectGuid = MovieScene->AddSpawnable(SystemToRecord->GetActorLabel(), *SystemToRecord.Get());
		//InSequence->Bind(ObjectGuid, *SystemToRecord.Get(), SystemToRecord->GetWorld());
		TrackRecorder = NewObject<UMovieSceneNiagaraTrackRecorder>();

		// We only support possessable for now since the spawnable template is clearing out the component reference
		// Would be better to use a TSoftObjectPtr for the actor and have a mechanism similar to the FixUpPIE to go
		// from PIE to editor to have a correct soft object path
		//const FGuid ObjectGuid = MovieScene->AddPossessable(NiagaraCacheManager->GetActorLabel(), NiagaraCacheManager->GetClass());
		//InParentSequence->BindPossessableObject(ObjectGuid, *NiagaraCacheManager, NiagaraCacheManager->GetWorld());

		//InParentSequence->
		//TrackRecorder->CreateTrack(MovieScene, SystemToRecord->GetNiagaraComponent(), MovieScene, nullptr, ObjectGuid);
		CachedNiagaraCacheTrack = TrackRecorder->GetNiagaraCacheTrack();
	}
	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderNiagaraCacheSource::TickRecording(const FQualifiedFrameTime& CurrentTime)
{
	if (TrackRecorder)
	{
		TrackRecorder->RecordSample(CurrentTime);
	}
}

void UTakeRecorderNiagaraCacheSource::StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->SetSectionStartTimecode(InSectionStartTimecode, InSectionFirstFrame);
	}
}

void UTakeRecorderNiagaraCacheSource::StopRecording(class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->StopRecording();
	}
}

TArray<UTakeRecorderSource*> UTakeRecorderNiagaraCacheSource::PostRecording(class ULevelSequence* InSequence, ULevelSequence* InParentSequence, const bool bCancelled)
{
	if (TrackRecorder)
	{
		TrackRecorder->FinalizeTrack();
	}

	TrackRecorder = nullptr;
	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderNiagaraCacheSource::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	if (CachedNiagaraCacheTrack.IsValid())
	{
		InFolder->AddChildMasterTrack(CachedNiagaraCacheTrack.Get());
	}
}

FText UTakeRecorderNiagaraCacheSource::GetDisplayTextImpl() const
{
	/*
	if (NiagaraCacheManager != nullptr && NiagaraCacheManager->IsValidLowLevelFast())
	{
		return FText::FromString(NiagaraCacheManager->GetName());
	}*/

	return LOCTEXT("Display Text", "Niagara Cache");
}

void UTakeRecorderNiagaraCacheSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
}

void UTakeRecorderNiagaraCacheSource::PostLoad()
{
	Super::PostLoad();
}

#undef LOCTEXT_NAMESPACE
