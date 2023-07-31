// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/TrackInstance/MovieSceneTrackInstance.h"
#include "MovieSceneCinePrestreamingTrackInstance.generated.h"

struct FStreamableHandle;
class UCinePrestreamingData;

UCLASS()
class UMovieSceneCinePrestreamingTrackInstance : public UMovieSceneTrackInstance
{
	GENERATED_BODY()

protected:
	/** UMovieSceneTrackInstance interface */
	void OnInputAdded(const FMovieSceneTrackInstanceInput& InInput) override;
	void OnInputRemoved(const FMovieSceneTrackInstanceInput& InInput) override;
	void OnAnimate() override;

private:
	/** Map of loaded asset references. */
	UPROPERTY(Transient)
	TMap< FMovieSceneTrackInstanceInput, TObjectPtr<UCinePrestreamingData> > PrestreamingAssetMap;

	/** Map of outstanding async loading handles. */
	TMap< FMovieSceneTrackInstanceInput, TSharedPtr<FStreamableHandle> > LoadHandleMap;
};
