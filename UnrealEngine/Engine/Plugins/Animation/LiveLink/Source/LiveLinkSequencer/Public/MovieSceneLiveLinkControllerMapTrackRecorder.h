// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "TrackRecorders/MovieSceneTrackRecorder.h"

#include "MovieSceneLiveLinkControllerTrackRecorder.h"

#include "MovieSceneLiveLinkControllerMapTrackRecorder.generated.h"

/** Movie Scene track recorder factory for the LiveLink Component's Controller Map */
class LIVELINKSEQUENCER_API FMovieSceneLiveLinkControllerMapTrackRecorderFactory : public IMovieSceneTrackRecorderFactory
{
public:
	virtual ~FMovieSceneLiveLinkControllerMapTrackRecorderFactory() = default;

	//~ Begin IMovieSceneTrackRecorderFactory interface
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override { return false; }
	virtual bool CanRecordProperty(class UObject* InObjectToRecord, class FProperty* InPropertyToRecord) const override;

	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForObject() const override;
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const override { return nullptr; }

	virtual FText GetDisplayName() const override { return NSLOCTEXT("MovieSceneLiveLinkControllerTrackRecorderFactory", "DisplayName", "LiveLink ControllerMap Track Recorder"); }
	//~ End IMovieSceneTrackRecorderFactory interface
};

/** Movie Scene track recorder for LiveLink Component's Controller Map */
UCLASS(BlueprintType)
class LIVELINKSEQUENCER_API UMovieSceneLiveLinkControllerMapTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()
public:
	virtual ~UMovieSceneLiveLinkControllerMapTrackRecorder() = default;

	//~ Begin UMovieSceneTrackRecorder Interface
	virtual void CreateTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentTime) override;
	virtual void StopRecordingImpl() override;
	virtual void FinalizeTrackImpl() override;
	//~ End UMovieSceneTrackRecorder Interface

private:
	/** Return the desired track recorder class that supports the input LiveLink controller class */
	TSubclassOf<UMovieSceneLiveLinkControllerTrackRecorder> GetRecorderClassForLiveLinkController(const TSubclassOf<ULiveLinkControllerBase> ControllerClass);

private:
	/** Array of track recorders that will record each of the LiveLink controller's in the Controller Map */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMovieSceneLiveLinkControllerTrackRecorder>> ControllerRecorders;
};
