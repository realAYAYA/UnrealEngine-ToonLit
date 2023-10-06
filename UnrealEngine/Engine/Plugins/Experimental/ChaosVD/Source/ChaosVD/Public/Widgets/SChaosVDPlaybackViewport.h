// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDPlaybackControllerObserver.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FChaosVDPlaybackViewportClient;
class SChaosVDSolverPlaybackControls;
class FChaosVDPlaybackController;
class SChaosVDTimelineWidget;
struct FChaosVDRecording;
class FChaosVDScene;
class FLevelEditorViewportClient;
class FSceneViewport;
class SViewport;

/* Widget that contains the 3D viewport and playback controls */
class SChaosVDPlaybackViewport : public SCompoundWidget, public FChaosVDPlaybackControllerObserver, public IChaosVDPlaybackControllerInstigator
{
public:

	SLATE_BEGIN_ARGS( SChaosVDPlaybackViewport ){}
	SLATE_END_ARGS()

	virtual ~SChaosVDPlaybackViewport() override;

	void Construct(const FArguments& InArgs, TWeakPtr<FChaosVDScene> InScene, TWeakPtr<FChaosVDPlaybackController> InPlaybackController);

protected:

	TSharedPtr<FChaosVDPlaybackViewportClient> CreateViewportClient() const;

	virtual void RegisterNewController(TWeakPtr<FChaosVDPlaybackController> NewController )override;
	virtual void HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController) override;
	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid) override;
	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	void OnPlaybackSceneUpdated();

	void OnFrameSelectionUpdated(int32 NewFrameIndex) const;

	TSharedPtr<SChaosVDTimelineWidget> GameFramesTimelineWidget;

	TSharedPtr<FChaosVDPlaybackViewportClient> PlaybackViewportClient;
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FSceneViewport> SceneViewport;
};
