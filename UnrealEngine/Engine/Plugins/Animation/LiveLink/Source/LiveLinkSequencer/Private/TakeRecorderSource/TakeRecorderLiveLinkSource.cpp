// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderLiveLinkSource.h"
#include "MovieSceneLiveLinkTrackRecorder.h"
#include "LevelSequence.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "SequenceRecorderUtils.h"
#include "Sound/SoundWave.h"
#include "MovieSceneFolder.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "TakeMetaData.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderLiveLinkSource)

UTakeRecorderLiveLinkSource::UTakeRecorderLiveLinkSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, bReduceKeys(false)
	, SubjectName(NAME_None)
	, bSaveSubjectSettings(true)
	, bUseSourceTimecode(true)
	, bDiscardSamplesBeforeStart(true)
{
	TrackTint = FColor(74, 108, 164);
}

TArray<UTakeRecorderSource*> UTakeRecorderLiveLinkSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) 
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	TrackRecorder = NewObject<UMovieSceneLiveLinkTrackRecorder>();
	TrackRecorder->CreateTrack(MovieScene, SubjectName, bSaveSubjectSettings, bUseSourceTimecode, bDiscardSamplesBeforeStart, nullptr);

	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderLiveLinkSource::StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->SetReduceKeys(bReduceKeys);
		TrackRecorder->SetSectionStartTimecode(InSectionStartTimecode, InSectionFirstFrame);
	}
}

void UTakeRecorderLiveLinkSource::TickRecording(const FQualifiedFrameTime& CurrentSequenceTime)
{
	if(TrackRecorder)
	{
		TrackRecorder->RecordSample(CurrentSequenceTime);
	}
}

void UTakeRecorderLiveLinkSource::StopRecording(class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->StopRecording();
	}
}

TArray<UTakeRecorderSource*> UTakeRecorderLiveLinkSource::PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled)
{
	if (TrackRecorder)
	{
		TrackRecorder->FinalizeTrack();
	}
	
	TrackRecorder = nullptr;
	return TArray<UTakeRecorderSource*>();
}

FText UTakeRecorderLiveLinkSource::GetDisplayTextImpl() const
{
	return FText::FromName(SubjectName);
}

void UTakeRecorderLiveLinkSource::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	TrackRecorder->AddContentsToFolder(InFolder);
}

bool UTakeRecorderLiveLinkSource::CanAddSource(UTakeRecorderSources* InSources) const
{
	for (UTakeRecorderSource* Source : InSources->GetSources())
	{
		if (Source->IsA<UTakeRecorderLiveLinkSource>() )
		{
			if (UTakeRecorderLiveLinkSource* OtherSource = Cast<UTakeRecorderLiveLinkSource>(Source) )
			{
				if (OtherSource->SubjectName == SubjectName)
				{
					return false;
				}
			}
		}
	}
	return true;
}

FString UTakeRecorderLiveLinkSource::GetSubsceneTrackName(ULevelSequence* InSequence) const
{
	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		return FString::Printf(TEXT("%s_%s"), *SubjectName.ToString(), *TakeMetaData->GenerateAssetPath("{slate}"));
	}
	else if (SubjectName != NAME_None)
	{
		return SubjectName.ToString();
	}

	return TEXT("LiveLink");
}

FString UTakeRecorderLiveLinkSource::GetSubsceneAssetName(ULevelSequence* InSequence) const
{
	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		return FString::Printf(TEXT("%s_%s"), *SubjectName.ToString(), *TakeMetaData->GenerateAssetPath("{slate}_{take}"));
	}
	else if (SubjectName != NAME_None)
	{
		return SubjectName.ToString();
	}

	return TEXT("LiveLink");
}

