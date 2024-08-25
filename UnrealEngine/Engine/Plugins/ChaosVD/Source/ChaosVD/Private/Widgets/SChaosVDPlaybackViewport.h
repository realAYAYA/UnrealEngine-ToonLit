// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDPlaybackControllerObserver.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

enum class EChaosVDPlaybackButtonsID : uint8;
class FChaosVDEditorModeTools;
enum class EChaosVDActorTrackingMode;
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
class SChaosVDPlaybackViewport : public SEditorViewport, public FChaosVDPlaybackControllerObserver, public IChaosVDPlaybackControllerInstigator, public ICommonEditorViewportToolbarInfoProvider
{
public:

	SLATE_BEGIN_ARGS(SChaosVDPlaybackViewport) {}
	SLATE_END_ARGS()

	virtual ~SChaosVDPlaybackViewport() override;

	void Construct(const FArguments& InArgs, TWeakPtr<FChaosVDScene> InScene, TWeakPtr<FChaosVDPlaybackController> InPlaybackController, TSharedPtr<FEditorModeTools> InEditorModeTools);

	// BEING ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override {};
	// END ICommonEditorViewportToolbarInfoProvider interface

	virtual void BindCommands() override;

	virtual EVisibility GetTransformToolbarVisibility() const override;

protected:

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

	virtual void RegisterNewController(TWeakPtr<FChaosVDPlaybackController> NewController )override;
	virtual void HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController) override;
	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid) override;
	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	void OnPlaybackSceneUpdated();
	void OnSolverVisibilityUpdated(int32 SolverID, bool bNewVisibility);

	void OnFrameSelectionUpdated(int32 NewFrameIndex) const;

	void HandlePlaybackButtonClicked(EChaosVDPlaybackButtonsID ButtonID);

	bool CanPlayback() const;

	TSharedPtr<SChaosVDTimelineWidget> GameFramesTimelineWidget;

	TSharedPtr<FChaosVDPlaybackViewportClient> PlaybackViewportClient;
	
	TWeakPtr<FChaosVDScene> CVDSceneWeakPtr;

	TSharedPtr<FExtender> Extender;

	TSharedPtr<FEditorModeTools> EditorModeTools;
};
