// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDSolverPlaybackControls.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDPlaybackController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/SChaosVDTimelineWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDSolverPlaybackControls::Construct(const FArguments& InArgs, int32 InSolverID, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController)
{
	SolverID = InSolverID;

	ChildSlot
	[
		SNew(SVerticalBox)
		// Playback controls
		// TODO: Now that the tool is In-Editor, see if we can/is worth use the Sequencer widgets
		// instead of these custom ones
		+SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.7f)
			[
			SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Text(LOCTEXT("PlaybackViewportWidgetPhysicsFramesLabel", "Solver Frames" ))
				]
				+SVerticalBox::Slot()
				[
					SAssignNew(FramesTimelineWidget, SChaosVDTimelineWidget)
						.HidePlayStopButtons(false)
						.HideLockButton(true)
						.OnFrameChanged_Raw(this, &SChaosVDSolverPlaybackControls::OnFrameSelectionUpdated)
						.MaxFrames(0)
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(0.3f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Text_Lambda([this]()->FText{ return FText::Format(LOCTEXT("PlaybackViewportWidgetStepsLabel","Step {0}"), FText::AsCultureInvariant(CurrentStepName));})
				]
				+SVerticalBox::Slot()
				[
					SAssignNew(StepsTimelineWidget, SChaosVDTimelineWidget)
					.HidePlayStopButtons(true)
					.OnFrameLockStateChanged_Raw(this, &SChaosVDSolverPlaybackControls::HandleLockStateChanged)
					.OnFrameChanged_Raw(this, &SChaosVDSolverPlaybackControls::OnStepSelectionUpdated)
					.MaxFrames(0)
				]
			]
		]
	];

	RegisterNewController(InPlaybackController);

	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InPlaybackController.Pin())
	{
		if (const FChaosVDTrackInfo* SolverTrackInfo = CurrentPlaybackControllerPtr->GetTrackInfo(EChaosVDTrackType::Solver, SolverID))
		{
			HandleControllerTrackFrameUpdated(InPlaybackController, SolverTrackInfo, InvalidGuid);
		}
	}
}

void SChaosVDSolverPlaybackControls::HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController)
{
	if (PlaybackController != InController)
	{
		RegisterNewController(InController);
	}

	const TSharedPtr<FChaosVDPlaybackController> ControllerSharedPtr = PlaybackController.Pin();
	if (ControllerSharedPtr.IsValid() && ControllerSharedPtr->IsRecordingLoaded())
	{	
		const int32 AvailableFrames = ControllerSharedPtr->GetTrackFramesNumber(EChaosVDTrackType::Solver, SolverID);

		int32 AvailableSteps = INDEX_NONE;
		if (const FChaosVDStepsContainer* StepData = ControllerSharedPtr->GetTrackStepsDataAtFrame(EChaosVDTrackType::Solver, SolverID, ControllerSharedPtr->GetTrackCurrentFrame(EChaosVDTrackType::Solver, SolverID)))
		{
			AvailableSteps = StepData->Num() > 0 ? StepData->Num() : INDEX_NONE;
			CurrentStepName =  StepData->Num() > 0 ? (*StepData)[0].StepName : TEXT("NONE");
		}

		// Max is inclusive and we use this to request as the index on the recorded frames/steps arrays so we need to -1 to the available frames/steps
		FramesTimelineWidget->UpdateMinMaxValue(0, AvailableFrames != INDEX_NONE ? AvailableFrames -1 : 0);

		//TODO: This will show steps 0/0 if only one step is recorded, we need to add a way to override that functionality
		// or just set the slider to start from 1 and handle the offset later 
		StepsTimelineWidget->UpdateMinMaxValue(0,AvailableSteps != INDEX_NONE ? AvailableSteps -1 : 0);
	}
	else
	{
		FramesTimelineWidget->UpdateMinMaxValue(0, 0);
		FramesTimelineWidget->ResetTimeline();
		StepsTimelineWidget->UpdateMinMaxValue(0,0);
		StepsTimelineWidget->ResetTimeline();
	}
}

void SChaosVDSolverPlaybackControls::UpdateStepsWidgetForFrame(const FChaosVDPlaybackController& InCurrentPlaybackController, int32 FrameNumber, int32 StepNumber, EChaosVDStepsWidgetUpdateFlags OptionsFlags)
{
	if (const FChaosVDStepsContainer* StepsData = InCurrentPlaybackController.GetTrackStepsDataAtFrame(EChaosVDTrackType::Solver, SolverID, FrameNumber))
	{
		constexpr TCHAR const* UnknownStepName = TEXT("Unknown");

		if (StepsData->Num() > 0 && StepNumber < StepsData->Num())
		{
			const FChaosVDStepData& StepData = (*StepsData)[StepNumber];

			if (EnumHasAnyFlags(OptionsFlags, EChaosVDStepsWidgetUpdateFlags::UpdateText))
			{
				CurrentStepName = StepData.StepName;
			}

			if (EnumHasAnyFlags(OptionsFlags, EChaosVDStepsWidgetUpdateFlags::SetTimelineStep))
			{
				int32 AvailableSteps = StepsData->Num() > 0 ? StepsData->Num() : INDEX_NONE;
				StepsTimelineWidget->UpdateMinMaxValue(0,AvailableSteps != INDEX_NONE ? AvailableSteps -1 : 0);

				// On Frame updates, always use Step 0
				StepsTimelineWidget->SetCurrentTimelineFrame(StepNumber, EChaosVDSetTimelineFrameFlags::None);
			}
		}
		else
		{
			StepsTimelineWidget->UpdateMinMaxValue(0,0);
			CurrentStepName = UnknownStepName;
		}
	}
}

void SChaosVDSolverPlaybackControls::HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid)
{
	if (InstigatorGuid == GetInstigatorID())
	{
		// Ignore the update if we initiated it
		return;
	}

	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InController.Pin())
	{
		if (const FChaosVDTrackInfo* SolverTrackInfo = CurrentPlaybackControllerPtr->GetTrackInfo(EChaosVDTrackType::Solver, SolverID))
		{
			FramesTimelineWidget->SetCurrentTimelineFrame(SolverTrackInfo->CurrentFrame, EChaosVDSetTimelineFrameFlags::None);

			UpdateStepsWidgetForFrame(*CurrentPlaybackControllerPtr.Get(), SolverTrackInfo->CurrentFrame, SolverTrackInfo->CurrentStep);
		}
	}
}

void SChaosVDSolverPlaybackControls::HandleLockStateChanged(bool NewIsLocked)
{
	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = PlaybackController.Pin())
	{
		if (NewIsLocked)
		{
			CurrentPlaybackControllerPtr->LockTrackInCurrentStep(EChaosVDTrackType::Solver, SolverID);
		}
		else
		{
			CurrentPlaybackControllerPtr->UnlockTrackStep(EChaosVDTrackType::Solver, SolverID);
		}
	}
}

void SChaosVDSolverPlaybackControls::OnFrameSelectionUpdated(int32 NewFrameIndex)
{
	if (const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = PlaybackController.Pin())
	{
		const int32 LastStepNumber = PlaybackControllerPtr->GetTrackLastStepAtFrame(EChaosVDTrackType::Solver, SolverID, NewFrameIndex);
		const int32 CurrentStep = PlaybackControllerPtr->GetTrackCurrentStep(EChaosVDTrackType::Solver, SolverID);

		// If the steps control is unlocked, each time we go to a new frame we should start at step 0.
		const int32 StepNumber = StepsTimelineWidget->IsUnlocked() ? LastStepNumber : CurrentStep;

		UpdateStepsWidgetForFrame(*PlaybackControllerPtr.Get(), NewFrameIndex, StepNumber);

		PlaybackControllerPtr->GoToTrackFrame(GetInstigatorID(), EChaosVDTrackType::Solver, SolverID, NewFrameIndex, StepNumber);
	}
}

void SChaosVDSolverPlaybackControls::OnStepSelectionUpdated(int32 NewStepIndex)
{
	if (const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = PlaybackController.Pin())
	{
		// On Steps updates. Always use the current Frame
		int32 CurrentFrame = PlaybackControllerPtr->GetTrackCurrentFrame(EChaosVDTrackType::Solver, SolverID);

		// Only Update the text as if we are here it means manually set the step number already
		UpdateStepsWidgetForFrame(*PlaybackControllerPtr.Get(), CurrentFrame, NewStepIndex, EChaosVDStepsWidgetUpdateFlags::UpdateText);

		PlaybackControllerPtr->GoToTrackFrame(GetInstigatorID(), EChaosVDTrackType::Solver, SolverID, CurrentFrame, NewStepIndex);
	}
}

#undef LOCTEXT_NAMESPACE