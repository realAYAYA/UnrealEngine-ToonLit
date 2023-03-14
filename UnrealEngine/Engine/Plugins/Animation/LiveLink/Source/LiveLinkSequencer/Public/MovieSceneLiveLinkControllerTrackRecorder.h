// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/MovieSceneTrackRecorder.h"

#include "MovieSceneLiveLinkControllerTrackRecorder.generated.h"

class ULiveLinkControllerBase;

/** Abstract based for movie scene track recorders that can record LiveLink Controllers */
UCLASS(Abstract)
class LIVELINKSEQUENCER_API UMovieSceneLiveLinkControllerTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()

public:
	/** Returns true if this track recorder class supports the input LiveLink controller class, false otherwise */
	virtual bool IsLiveLinkControllerSupported(const TSubclassOf<ULiveLinkControllerBase>& ControllerToSupport) const PURE_VIRTUAL(UMovieSceneLiveLinkControllerTrackRecorder::IsLiveLinkControllerSupported, return false;);

	/** Set the LiveLink controller object that this track recorder will record properties from */
	void SetLiveLinkController(ULiveLinkControllerBase* InLiveLinkController) { LiveLinkControllerToRecord = InLiveLinkController; }

protected:
	/** The LiveLink controller that this track record will record properties from */
	UPROPERTY(Transient)
	TObjectPtr<ULiveLinkControllerBase> LiveLinkControllerToRecord;
};
