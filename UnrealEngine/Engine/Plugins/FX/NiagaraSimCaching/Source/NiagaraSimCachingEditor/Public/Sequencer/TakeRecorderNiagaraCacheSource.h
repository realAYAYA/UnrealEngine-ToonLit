// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraActor.h"
#include "NiagaraSimCache.h"
#include "TakeRecorderSource.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "TakeRecorderNiagaraCacheSource.generated.h"

class ULevelSequence;
class UMovieSceneFolder;
class UMovieSceneNiagaraCacheTrack;
class UMovieSceneNiagaraTrackRecorder;
class ANiagaraCacheManager;

/** A recording source selector for the Niagara integration into take recorder */

UCLASS(Category = "Niagara", meta = (TakeRecorderDisplayName = "Niagara Cache"))
class UTakeRecorderNiagaraCacheSource : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderNiagaraCacheSource(const FObjectInitializer& ObjInit);

	/** Niagara system to use as take recorder source */
	UPROPERTY(EditAnywhere, Category = "NiagaraCache")
	TSoftObjectPtr<ANiagaraActor> SystemToRecord;

	UPROPERTY(EditAnywhere, Category = "NiagaraCache")
	FNiagaraSimCacheCreateParameters CacheParameters;
	
	// ~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// ~End UObject Interface
	
protected:
	// ~Begin UTakeRecorderSource Interface
	virtual bool SupportsSubscenes() const override { return false; }
	// ~End UTakeRecorderSource Interface

private:
	// ~Begin UTakeRecorderSource Interface
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InParentSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence) override;
	virtual void StopRecording(class ULevelSequence* InSequence) override;
	virtual void TickRecording(const FQualifiedFrameTime& CurrentTime) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(class ULevelSequence* InSequence, ULevelSequence* InParentSequence, const bool bCancelled) override;
	virtual void AddContentsToFolder(UMovieSceneFolder* InFolder) override;
	virtual FText GetDisplayTextImpl() const override;
	// ~End UTakeRecorderSource
	
	/** Niagara cache track recorder used by this source */
	UPROPERTY()
	TObjectPtr<UMovieSceneNiagaraTrackRecorder> TrackRecorder;

	/**
	 * Stores an existing Niagara cache track track in the Sequence to be recorded or
	 * a new one created for recording. Set during PreRecording.
	 */
	TWeakObjectPtr<UMovieSceneNiagaraCacheTrack> CachedNiagaraCacheTrack;
};
