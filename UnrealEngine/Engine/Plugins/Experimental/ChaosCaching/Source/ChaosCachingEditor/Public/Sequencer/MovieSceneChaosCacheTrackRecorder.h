// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/MovieSceneTrackRecorder.h"
#include "CoreMinimal.h"
#include "MovieScene.h"
#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Chaos/CacheManagerActor.h"

#include "MovieSceneChaosCacheTrackRecorder.generated.h"

class  UMovieSceneChaosCacheTrack;
class  UMovieSceneChaosCacheSection;

struct FFrameNumber;
class  UMovieSceneSection;
class  UMovieSceneTrackRecorderSettings;

class CHAOSCACHINGEDITOR_API FMovieSceneChaosCacheTrackRecorderFactory : public IMovieSceneTrackRecorderFactory
{
public:
	virtual ~FMovieSceneChaosCacheTrackRecorderFactory() {}

	// ~Begin IMovieSceneTrackRecorderFactory Interface
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForObject() const override;
	virtual bool CanRecordProperty(class UObject* InObjectToRecord, class FProperty* InPropertyToRecord) const override { return false; }
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const override { return nullptr; }
	virtual FText GetDisplayName() const override { return NSLOCTEXT("MovieSceneChaosCacheTrackRecorderFactory", "DisplayName", "Chaos Cache Track"); }
	// ~End IMovieSceneTrackRecorderFactory Interface
};

/**
* Track recorder implementation for the chaos cache
*/
UCLASS(BlueprintType)
class CHAOSCACHINGEDITOR_API UMovieSceneChaosCacheTrackRecorder
	: public UMovieSceneTrackRecorder
{
	GENERATED_BODY()

public:
	// ~Begin UMovieSceneTrackRecorder Interface
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentFrameTime) override;
	virtual void FinalizeTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual UMovieSceneSection* GetMovieSceneSection() const override;
	virtual void StopRecordingImpl() override;
	virtual bool LoadRecordedFile(const FString& InFileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback) override;
	virtual void CreateTrackImpl() override;
	// ~End UMovieSceneTrackRecorder Interface

	/** Returns the chaos cache track on which the cache manager will be recorded */
	TWeakObjectPtr<UMovieSceneChaosCacheTrack> GetChaosCacheTrack() const {return ChaosCacheTrack;}
	
private:
	
	/** The ChaosCache Track to record onto */
	TWeakObjectPtr<UMovieSceneChaosCacheTrack> ChaosCacheTrack;

	/** Sections to record to on each track*/
	TWeakObjectPtr<UMovieSceneChaosCacheSection> ChaosCacheSection;

	/** Chaos cache that will be used to record the simulation */
	AChaosCacheManager* ChaosCacheManager;

	/** Stored cahce mode that will be set back onto the manager once the recording will be finished */
	ECacheMode StoredCacheMode;

	/** The time at the start of this recording section */
	double RecordStartTime;

	/** The frame at the start of this recording section */
	FFrameNumber RecordStartFrame;
};

