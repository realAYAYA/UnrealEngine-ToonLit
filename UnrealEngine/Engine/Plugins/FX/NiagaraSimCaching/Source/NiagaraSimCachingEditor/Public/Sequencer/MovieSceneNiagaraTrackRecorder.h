// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/MovieSceneTrackRecorder.h"
#include "CoreMinimal.h"
#include "MovieScene.h"
#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "Channels/MovieSceneFloatChannel.h"

#include "MovieSceneNiagaraTrackRecorder.generated.h"

class UNiagaraComponent;
class  UMovieSceneNiagaraCacheTrack;
class  UMovieSceneNiagaraCacheSection;

struct FFrameNumber;
class  UMovieSceneSection;
class  UMovieSceneTrackRecorderSettings;

class NIAGARASIMCACHINGEDITOR_API FMovieSceneNiagaraTrackRecorderFactory : public IMovieSceneTrackRecorderFactory
{
public:
	virtual ~FMovieSceneNiagaraTrackRecorderFactory() {}

	// ~Begin IMovieSceneTrackRecorderFactory Interface
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForObject() const override;

	// Particle Systems are entire components and you can't animate them as a property
	virtual bool CanRecordProperty(class UObject* InObjectToRecord, class FProperty* InPropertyToRecord) const override { return false; }
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const override;
	virtual FText GetDisplayName() const override { return NSLOCTEXT("MovieSceneNiagaraTrackRecorderFactory", "DisplayName", "Niagara Cache Track"); }
	// ~End IMovieSceneTrackRecorderFactory Interface
};

UCLASS(BlueprintType)
class NIAGARASIMCACHINGEDITOR_API UMovieSceneNiagaraTrackRecorder
	: public UMovieSceneTrackRecorder
{
	GENERATED_BODY()

public:
	// ~Begin UMovieSceneTrackRecorder Interface
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentFrameTime) override;
	virtual void FinalizeTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual UMovieSceneSection* GetMovieSceneSection() const override;
	virtual void CreateTrackImpl() override;
	// ~End UMovieSceneTrackRecorder Interface

	/** Returns the Niagara cache track on which the cache manager will be recorded */
	TWeakObjectPtr<UMovieSceneNiagaraCacheTrack> GetNiagaraCacheTrack() const {return NiagaraCacheTrack;}
	
private:
	
	/** The NiagaraCache Track to record onto */
	TWeakObjectPtr<UMovieSceneNiagaraCacheTrack> NiagaraCacheTrack;

	/** Sections to record to on each track*/
	TWeakObjectPtr<UMovieSceneNiagaraCacheSection> NiagaraCacheSection;

	/** Object to record from */
	TLazyObjectPtr<UNiagaraComponent> SystemToRecord;

	/** The time at the start of this recording section */
	double RecordStartTime;

	/** The frame at the start of this recording section */
	FFrameNumber RecordStartFrame;
};

