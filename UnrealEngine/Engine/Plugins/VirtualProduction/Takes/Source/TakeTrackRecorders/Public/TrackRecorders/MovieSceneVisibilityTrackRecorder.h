// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "IMovieSceneTrackRecorderFactory.h"
#include "MovieSceneTrackRecorder.h"
#include "Sections/MovieSceneBoolSection.h"
#include "MovieSceneVisibilityTrackRecorder.generated.h"

class UMovieSceneBoolTrack;

class TAKETRACKRECORDERS_API FMovieSceneVisibilityTrackRecorderFactory : public IMovieSceneTrackRecorderFactory
{
public:
	virtual ~FMovieSceneVisibilityTrackRecorderFactory() {}

	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForObject() const override;

	// Visibility is based on a different property for components and actors, and they're not marked as interp
	virtual bool CanRecordProperty(class UObject* InObjectToRecord, class FProperty* InPropertyToRecord) const override;
	virtual UMovieSceneTrackRecorder* CreateTrackRecorderForProperty(UObject* InObjectToRecord, const FName& InPropertyToRecord) const override { return nullptr; }

	virtual FText GetDisplayName() const override { return NSLOCTEXT("MovieSceneVisibilityTrackRecorderFactory", "DisplayName", "Visibility Track"); }
};

UCLASS(BlueprintType)
class TAKETRACKRECORDERS_API UMovieSceneVisibilityTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()
protected:
	// UMovieSceneTrackRecorder Interface
	virtual void CreateTrackImpl() override;
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentTime) override;
	virtual void FinalizeTrackImpl() override;
	virtual UMovieSceneSection* GetMovieSceneSection() const override { return Cast<UMovieSceneSection>(MovieSceneSection.Get()); }
	// UMovieSceneTrackRecorder Interface

	void RemoveRedundantTracks();
	bool IsObjectVisible() const;

private:
	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneBoolSection> MovieSceneSection;

	/** Flag used to track visibility state and add keys when this changes */
	bool bWasVisible;

	/** Flag used to determine whether the first key needs to be set */
	bool bSetFirstKey;
};
