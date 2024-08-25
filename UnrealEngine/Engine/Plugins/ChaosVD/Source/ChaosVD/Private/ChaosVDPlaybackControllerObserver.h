// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneSelectionObserver.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

struct FChaosVDTrackInfo;
class FChaosVDPlaybackController;


/**
 * Class to be used as base to any object that needs to Process Player Controller changes,
 * and scene Selection Changes
 */
class FChaosVDPlaybackControllerObserver : public FChaosVDSceneSelectionObserver
{
public:

	virtual ~FChaosVDPlaybackControllerObserver() override;

	/** Returns a Weak Ptr to the Playback Controller we are currently observing */
	TWeakPtr<FChaosVDPlaybackController> GetObservedController();

protected:
	virtual void RegisterNewController(TWeakPtr<FChaosVDPlaybackController> NewController);
	virtual void HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController) {};
	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid){};
	
	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override{};
	
	TWeakPtr<FChaosVDPlaybackController> PlaybackController;
};
