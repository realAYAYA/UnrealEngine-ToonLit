// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderCameraCutSource.h"

#include "TakeRecorderSources.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSourcesUtils.h"
#include "TakesUtils.h"
#include "TakesCoreLog.h"

#include "LevelSequence.h"
#include "LevelEditorViewport.h"
#include "Engine/Engine.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderCameraCutSource)

#define LOCTEXT_NAMESPACE "UTakeRecorderCameraCutSource"

UTakeRecorderCameraCutSource::UTakeRecorderCameraCutSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	TrackTint = FColor(160, 160, 160);
}

TArray<UTakeRecorderSource*> UTakeRecorderCameraCutSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	World = TakeRecorderSourcesUtils::GetSourceWorld(InSequence);
	MasterLevelSequence = InMasterSequence;

	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderCameraCutSource::TickRecording(const FQualifiedFrameTime& CurrentTime)
{	
	AActor* Target = nullptr;
	if (!World)
	{
		return;
	}

	if (World->WorldType == EWorldType::Editor)
	{
		if (GCurrentLevelEditingViewportClient)
		{
			UCameraComponent* CameraComponent = GCurrentLevelEditingViewportClient->GetCameraComponentForView();
			if (CameraComponent)
			{
				Target = CameraComponent->GetOwner();
			}
		}
	}
	else 
	{
		APlayerController* PC = World->GetGameInstance() ? World->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		APlayerCameraManager* PlayerCameraManager = PC ? ToRawPtr(PC->PlayerCameraManager) : nullptr;
		if (PlayerCameraManager)
		{
			Target = PlayerCameraManager->ViewTarget.Target;
		}
	}

	if (!Target)
	{
		return;
	}

	if (!TakeRecorderSourcesUtils::IsActorBeingRecorded(this, Target))
	{
		UTakeRecorderSources* OwningSources = CastChecked<UTakeRecorderSources>(GetOuter());

		UTakeRecorderActorSource* ActorSource = OwningSources->AddSource<UTakeRecorderActorSource>();
		ActorSource->Target = Target;

		// Send a PropertyChangedEvent so the class catches the callback and rebuilds the property map. We can't rely on the Actor rebuilding the map on PreRecording
		// because that would wipe out any user adjustments from one added natively.
		FPropertyChangedEvent PropertyChangedEvent(UTakeRecorderActorSource::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)), EPropertyChangeType::ValueSet);
		ActorSource->PostEditChangeProperty(PropertyChangedEvent);

		// This has to be called after setting the Target and propagating the change event so that it has a chance to know what to record
		// about the actor.
		OwningSources->StartRecordingSource(TArray<UTakeRecorderSource*>({ ActorSource }), CurrentTime);

		NewActorSources.Add(ActorSource);
	}

	FGuid RecordedCameraGuid = TakeRecorderSourcesUtils::GetRecordedActorGuid(this, Target);
	FMovieSceneSequenceID RecordedCameraSequenceID = TakeRecorderSourcesUtils::GetLevelSequenceID(this, Target, MasterLevelSequence);

	// If this camera is already noted, skip it until it changes
	if (CameraCutData.Num() && CameraCutData.Last().Guid == RecordedCameraGuid && CameraCutData.Last().SequenceID == RecordedCameraSequenceID)
	{
		return;
	}

	CameraCutData.Add(FCameraCutData(RecordedCameraGuid, RecordedCameraSequenceID, CurrentTime));
}


TArray<UTakeRecorderSource*> UTakeRecorderCameraCutSource::PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled)
{
	// Build the camera cut track
	if (!bCancelled && CameraCutData.Num())
	{
		UMovieSceneTrack* CameraCutTrack = InMasterSequence->GetMovieScene()->GetCameraCutTrack();
		if (!CameraCutTrack)
		{
			CameraCutTrack = InMasterSequence->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
		}
		else
		{
			CameraCutTrack->RemoveAllAnimationData();
		}

 		FFrameRate TickResolution = InMasterSequence->GetMovieScene()->GetTickResolution();

		for (int32 CameraCutIndex = 0; CameraCutIndex < CameraCutData.Num(); ++CameraCutIndex)
		{
			FFrameNumber CameraCutTime = CameraCutData[CameraCutIndex].Time.ConvertTo(TickResolution).FloorToFrame();
			TRange<FFrameNumber> Range;

			if (CameraCutIndex == 0)
			{
				FFrameNumber NextCameraCutTime = 
					CameraCutIndex+1 < CameraCutData.Num() ? 
					CameraCutData[CameraCutIndex+1].Time.ConvertTo(TickResolution).FloorToFrame() : 
					InMasterSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue();
				Range = TRange<FFrameNumber>(InMasterSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue(), NextCameraCutTime);
			}
			else if (CameraCutIndex == CameraCutData.Num()-1)
			{
				Range = TRange<FFrameNumber>(CameraCutTime, InMasterSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue());
			}
			else
			{
				FFrameNumber NextCameraCutTime = CameraCutData[CameraCutIndex+1].Time.ConvertTo(TickResolution).FloorToFrame();
				Range = TRange<FFrameNumber>(CameraCutTime, NextCameraCutTime);
			}

			UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
			FMovieSceneObjectBindingID CameraCutBinding = UE::MovieScene::FRelativeObjectBindingID(CameraCutData[CameraCutIndex].Guid, CameraCutData[CameraCutIndex].SequenceID);
			CameraCutSection->SetCameraBindingID(CameraCutBinding);
			CameraCutSection->SetRange(Range);
			CameraCutTrack->AddSection(*CameraCutSection);
		}
	}

	TArray<UTakeRecorderSource*> SourcesToRemove;
	for (auto NewActorSource : NewActorSources)
	{
		if (NewActorSource.IsValid())
		{
			SourcesToRemove.Add(NewActorSource.Get());
		}
	}

	World = nullptr;
	MasterLevelSequence = nullptr;

	return SourcesToRemove;
}

FText UTakeRecorderCameraCutSource::GetDisplayTextImpl() const
{
	return LOCTEXT("Label", "Camera Cuts");
}

#undef LOCTEXT_NAMESPACE // "UTakeRecorderCameraCutSource"
