// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "UObject/SoftObjectPtr.h"
#include "TakeRecorderLevelSequenceSource.generated.h"

class UTexture;
class ALevelSequenceActor;
class ULevelSequence;

/** Plays level sequence actors when recording starts */
UCLASS(Category="Other", meta = (TakeRecorderDisplayName = "Level Sequence"))
class UTakeRecorderLevelSequenceSource : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderLevelSequenceSource(const FObjectInitializer& ObjInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source")
	TArray<TObjectPtr<ULevelSequence>> LevelSequencesToTrigger;

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence) override;
	virtual void StopRecording(class ULevelSequence* InSequence) override;
	virtual FText GetDisplayTextImpl() const override;
	virtual FText GetDescriptionTextImpl() const override;

	// This source does not support subscenes since it's a playback source instead of a recording
	virtual bool SupportsSubscenes() const override { return false; }

	/** Transient level sequence actors to trigger, to be stopped and reset at the end of recording */
	TArray<TWeakObjectPtr<ALevelSequenceActor>> ActorsToTrigger;
};
