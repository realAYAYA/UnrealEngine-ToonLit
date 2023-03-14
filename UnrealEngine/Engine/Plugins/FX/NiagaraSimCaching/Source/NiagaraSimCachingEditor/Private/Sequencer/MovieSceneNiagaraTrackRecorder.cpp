// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneNiagaraTrackRecorder.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheTrack.h"
#include "TakeRecorderSettings.h"
#include "Recorder/TakeRecorderParameters.h"
#include "Engine/TimecodeProvider.h"
#include "Misc/App.h"
#include "MovieSceneFolder.h"
#include "LevelSequence.h"
#include "NiagaraComponent.h"
#include "NiagaraSimCacheFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraTrackRecorder"

bool FMovieSceneNiagaraTrackRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<UNiagaraComponent>();
}

UMovieSceneTrackRecorder* FMovieSceneNiagaraTrackRecorderFactory::CreateTrackRecorderForObject() const
{
	return NewObject<UMovieSceneNiagaraTrackRecorder>();
}

UMovieSceneTrackRecorder* FMovieSceneNiagaraTrackRecorderFactory::CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const
{
	return nullptr;
}

void UMovieSceneNiagaraTrackRecorder::CreateTrackImpl()
{
	SystemToRecord = CastChecked<UNiagaraComponent>(ObjectToRecord.Get());

	NiagaraCacheTrack = MovieScene->FindTrack<UMovieSceneNiagaraCacheTrack>(ObjectGuid);
	if (!NiagaraCacheTrack.IsValid())
	{
		NiagaraCacheTrack = MovieScene->AddTrack<UMovieSceneNiagaraCacheTrack>(ObjectGuid);
	}
	else
	{
		NiagaraCacheTrack->RemoveAllAnimationData();
	}

	if (NiagaraCacheTrack.IsValid())
	{
		NiagaraCacheSection = CastChecked<UMovieSceneNiagaraCacheSection>(NiagaraCacheTrack->CreateNewSection());
		NiagaraCacheSection->SetIsActive(false);
		NiagaraCacheTrack->AddSection(*NiagaraCacheSection);

		// Resize the section to either it's remaining	keyframes range or 0
		NiagaraCacheSection->SetRange(NiagaraCacheSection->GetAutoSizeRange().Get(TRange<FFrameNumber>(0, 0)));

		// Make sure it starts at frame 0, in case Auto Size removed a piece of the start
		NiagaraCacheSection->ExpandToFrame(0);
	}
}

void UMovieSceneNiagaraTrackRecorder::SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame)
{
	if (NiagaraCacheSection.IsValid() && NiagaraCacheTrack.IsValid())
	{
		NiagaraCacheSection->TimecodeSource = FMovieSceneTimecodeSource(InSectionStartTimecode);

		FTakeRecorderParameters Parameters;
		Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
		Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

		RecordStartTime = FApp::GetCurrentTime();
		RecordStartFrame = Parameters.Project.bStartAtCurrentTimecode ?
			FFrameRate::TransformTime(FFrameTime(InSectionStartTimecode.ToFrameNumber(DisplayRate)),
				DisplayRate, TickResolution).FloorToFrame() : MovieScene->GetPlaybackRange().GetLowerBoundValue();

		if (SystemToRecord.IsValid())
		{
			// start simulation and writing to the sim cache 
			SystemToRecord->SetSimCache(nullptr);
			//SystemToRecord->SetAgeUpdateMode(ENiagaraAgeUpdateMode::TickDeltaTime);
			//SystemToRecord->ResetSystem();
			NiagaraCacheSection->Params.SimCache->BeginWrite(NiagaraCacheSection->Params.CacheParameters, SystemToRecord.Get());
		}
	}
}

UMovieSceneSection* UMovieSceneNiagaraTrackRecorder::GetMovieSceneSection() const
{
	return Cast<UMovieSceneSection>(NiagaraCacheSection.Get());
}

void UMovieSceneNiagaraTrackRecorder::FinalizeTrackImpl()
{
	if (NiagaraCacheSection.IsValid())
	{
		// finalize the sim cache
		NiagaraCacheSection->Params.SimCache->EndWrite();
		
		// Set the final range 
		TOptional<TRange<FFrameNumber> > DefaultSectionLength = NiagaraCacheSection->GetAutoSizeRange();
		if (DefaultSectionLength.IsSet())
		{
			NiagaraCacheSection->SetRange(DefaultSectionLength.GetValue());
		}

		// Activate the section
		NiagaraCacheSection->SetIsActive(true);
	}
}

void UMovieSceneNiagaraTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentFrameTime)
{
	// Expand the section to the new length
	const FFrameRate	 TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber CurrentFrame = CurrentFrameTime.ConvertTo(TickResolution).FloorToFrame();

	if (NiagaraCacheSection.IsValid() && SystemToRecord.IsValid())
	{
		NiagaraCacheSection->Params.SimCache->WriteFrame(SystemToRecord.Get());
		NiagaraCacheSection->SetEndFrame(CurrentFrame);
	}
}

#undef LOCTEXT_NAMESPACE
