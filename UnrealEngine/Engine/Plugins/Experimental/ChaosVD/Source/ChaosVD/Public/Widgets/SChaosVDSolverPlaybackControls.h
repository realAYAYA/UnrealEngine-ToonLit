// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDPlaybackControllerObserver.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

struct FChaosVDTrackInfo;
class FChaosVDPlaybackController;
class SChaosVDTimelineWidget;

/**
 * Options flags to control how the Step timeline widgets should be updated
 */
enum class EChaosVDStepsWidgetUpdateFlags
{
	UpdateText =  1 << 0,
	SetTimelineStep = 1 << 1,

	Default = UpdateText | SetTimelineStep
};
ENUM_CLASS_FLAGS(EChaosVDStepsWidgetUpdateFlags)

/** Widget that Generates playback controls for solvers
 * Which are two timelines, one for physics frames and other for steps
 */
class SChaosVDSolverPlaybackControls : public SCompoundWidget, public FChaosVDPlaybackControllerObserver, public IChaosVDPlaybackControllerInstigator
{
public:
	SLATE_BEGIN_ARGS( SChaosVDSolverPlaybackControls ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 InSolverID, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController);

private:

	void OnFrameSelectionUpdated(int32 NewFrameIndex);
	void OnStepSelectionUpdated(int32 NewStepIndex);

	virtual void HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController) override;
	void UpdateStepsWidgetForFrame(const FChaosVDPlaybackController& InCurrentPlaybackController, int32 FrameNumber, int32 StepNumber, EChaosVDStepsWidgetUpdateFlags OptionsFlags = EChaosVDStepsWidgetUpdateFlags::Default);
	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid) override;

	void HandleLockStateChanged(bool NewIsLocked);

	int32 SolverID = INDEX_NONE;
	FString CurrentStepName;
	TSharedPtr<SChaosVDTimelineWidget> FramesTimelineWidget;
	TSharedPtr<SChaosVDTimelineWidget> StepsTimelineWidget;
	bool bStepsLocked = false;
};
