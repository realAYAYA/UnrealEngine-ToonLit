// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "MovieSceneSequenceID.h"

#include "TakeRecorderCameraCutSource.generated.h"

class UWorld;
class UTakeRecorderActorSource;

/** A recording source that detects camera switching and creates a camera cut track */
UCLASS(Category="Other", meta = (TakeRecorderDisplayName = "Camera Cuts"))
class UTakeRecorderCameraCutSource : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderCameraCutSource(const FObjectInitializer& ObjInit);

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void TickRecording(const FQualifiedFrameTime& CurrentTime) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled) override;
	virtual FText GetDisplayTextImpl() const override;

	// This source does not support subscenes
	virtual bool SupportsSubscenes() const override { return false; }

private:

	UPROPERTY()
	TObjectPtr<UWorld> World;

	/** The master or uppermost level sequence that this source is being recorded into. Set during PreRecording, null after PostRecording. */
	UPROPERTY()
	TObjectPtr<ULevelSequence> MasterLevelSequence;


	struct FCameraCutData
	{
		FCameraCutData(FGuid InGuid, FMovieSceneSequenceID InSequenceID, FQualifiedFrameTime InTime) 
		: Guid(InGuid)
		, SequenceID(InSequenceID)
		, Time(InTime) {}

		FGuid Guid;
		FMovieSceneSequenceID SequenceID;
		FQualifiedFrameTime Time;
	};

	TArray<FCameraCutData> CameraCutData;

	/** Spawned actor sources to be removed at the end of recording */
	TArray<TWeakObjectPtr<UTakeRecorderActorSource> > NewActorSources;
};
