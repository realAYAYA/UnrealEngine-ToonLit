// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDSolverPlaybackControls.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDScene.h"
#include "Widgets/ChaosVDPlaybackControlsHelper.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/SChaosVDTimelineWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDSolverPlaybackControls::Construct(const FArguments& InArgs, int32 InSolverID, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController)
{
	SolverID = InSolverID;

	static const FName NAME_VisibleNotHoveredBrush = TEXT("Level.VisibleIcon16x");
	static const FName NAME_NotVisibleNotHoveredBrush = TEXT("Level.NotVisibleIcon16x");

	SolverVisibleIconBrush = FAppStyle::Get().GetBrush(NAME_VisibleNotHoveredBrush);
	SolverHiddenIconBrush = FAppStyle::Get().GetBrush(NAME_NotVisibleNotHoveredBrush);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.FillWidth(0.8f)
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
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(0.9f)
				[
					SAssignNew(FramesTimelineWidget, SChaosVDTimelineWidget)
					.ButtonVisibilityFlags(static_cast<uint16>(EChaosVDTimelineElementIDFlags::AllPlayback))
					.OnFrameChanged_Raw(this, &SChaosVDSolverPlaybackControls::OnFrameSelectionUpdated)
					.OnButtonClicked_Raw(this, &SChaosVDSolverPlaybackControls::HandlePlaybackButtonClicked)
					.MaxFrames(0)
				]
				+SHorizontalBox::Slot()
				.Padding(6.0f,0.0f)
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage_Raw(this, &SChaosVDSolverPlaybackControls::GetFrameTypeBadgeBrush)
					.Padding(2.0f)
					.Content()
					[
						SNew(SBox)
						.Padding(4.0f,0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Justification(ETextJustify::Center)
							.Text_Lambda([this]()->FText{ return bIsReSimFrame ? LOCTEXT("PlaybackViewportWidgetPhysicsFramesResimLabel", "ReSim" ) : LOCTEXT("PlaybackViewportWidgetPhysicsFramesNormalLabel", "Normal" );})
						]
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.FillWidth(0.2f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text_Lambda([this]()->FText{ return FText::Format(LOCTEXT("PlaybackViewportWidgetStepsLabel","Solver Stage: {0}"), FText::AsCultureInvariant(CurrentStepName));})
			]
			+SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(0.9f)
				[
					SAssignNew(StepsTimelineWidget, SChaosVDTimelineWidget)
					.ButtonVisibilityFlags(static_cast<uint16>(EChaosVDTimelineElementIDFlags::AllManualStepping))
					.OnFrameLockStateChanged_Raw(this, &SChaosVDSolverPlaybackControls::HandleLockStateChanged)
					.OnFrameChanged_Raw(this, &SChaosVDSolverPlaybackControls::OnStepSelectionUpdated)
					.MaxFrames(0)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateVisibilityWidget().ToSharedRef()
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

SChaosVDSolverPlaybackControls::~SChaosVDSolverPlaybackControls()
{
	if (const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = PlaybackController.Pin())
	{
		// TODO: Doing this here to try to keep this change small enough for a 5.4.1 hotfix release
		// This should be handled by the playback controller, but currently we are not doing any tracking of what solver tracks are being unloaded/loaded there
		// This can be done as part of UE-192940 , which is a task to remove a similar dependency in the UI for the playback logic
		if (FChaosVDTrackInfo* SolverTrackInfo = PlaybackControllerPtr->GetMutableTrackInfo(EChaosVDTrackType::Solver, SolverID))
		{
			SolverTrackInfo->CurrentFrame = 0;
		}

		PlaybackControllerPtr->ReleaseExclusivePlaybackControls(*this);
	}
}

void SChaosVDSolverPlaybackControls::ConditionallyLockPlaybackControl(const TSharedRef<FChaosVDPlaybackController>& InControllerSharedRef)
{
	const FGuid CurrentPlaybackInstigatorID = InControllerSharedRef->GetPlaybackInstigatorWithExclusiveControlsID();
	const bool bUserCanControlPlayback = CurrentPlaybackInstigatorID == InvalidGuid || CurrentPlaybackInstigatorID == GetInstigatorID();

	// On Live Sessions, only the Game Frames timeline controls are allowed for now
	FramesTimelineWidget->SetIsLocked(InControllerSharedRef->IsPlayingLiveSession() || !bUserCanControlPlayback);
	StepsTimelineWidget->SetIsLocked(InControllerSharedRef->IsPlayingLiveSession() || !bUserCanControlPlayback);
}

void SChaosVDSolverPlaybackControls::HandleSolverVisibilityChanged(int32 InSolverID, bool bNewVisibility)
{
	if (SolverID != InSolverID)
	{
		return;
	}

	bIsVisible = bNewVisibility;
}

FReply SChaosVDSolverPlaybackControls::ToggleSolverVisibility() const
{
	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = PlaybackController.Pin())
	{
		CurrentPlaybackControllerPtr->UpdateTrackVisibility(EChaosVDTrackType::Solver, SolverID, !bIsVisible);
	}

	return FReply::Handled();
}

const FSlateBrush* SChaosVDSolverPlaybackControls::GetBrushForCurrentVisibility() const
{
	return bIsVisible ? SolverVisibleIconBrush : SolverHiddenIconBrush;
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
		if (const FChaosVDStepsContainer* StepData = ControllerSharedPtr->GetTrackStepsDataAtFrame_AssumesLocked(EChaosVDTrackType::Solver, SolverID, ControllerSharedPtr->GetTrackCurrentFrame(EChaosVDTrackType::Solver, SolverID)))
		{
			AvailableSteps = StepData->Num() > 0 ? StepData->Num() : INDEX_NONE;
			CurrentStepName =  StepData->Num() > 0 ? (*StepData)[0].StepName : TEXT("NONE");
		}

		// Max is inclusive and we use this to request as the index on the recorded frames/steps arrays so we need to -1 to the available frames/steps
		FramesTimelineWidget->UpdateMinMaxValue(0, AvailableFrames != INDEX_NONE ? AvailableFrames -1 : 0);

		//TODO: This will show steps 0/0 if only one step is recorded, we need to add a way to override that functionality
		// or just set the slider to start from 1 and handle the offset later 
		StepsTimelineWidget->UpdateMinMaxValue(0,AvailableSteps != INDEX_NONE ? AvailableSteps -1 : 0);

		ConditionallyLockPlaybackControl(ControllerSharedPtr.ToSharedRef());
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
	if (const FChaosVDStepsContainer* StepsData = InCurrentPlaybackController.GetTrackStepsDataAtFrame_AssumesLocked(EChaosVDTrackType::Solver, SolverID, FrameNumber))
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
	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InController.Pin())
	{
		if (const FChaosVDTrackInfo* SolverTrackInfo = CurrentPlaybackControllerPtr->GetTrackInfo(EChaosVDTrackType::Solver, SolverID))
		{
			if (InstigatorGuid != GetInstigatorID())
			{
				// No Need to manually update the widget state if the widget it-self instigated the update 
				FramesTimelineWidget->SetCurrentTimelineFrame(SolverTrackInfo->CurrentFrame, EChaosVDSetTimelineFrameFlags::None);
				UpdateStepsWidgetForFrame(*CurrentPlaybackControllerPtr.Get(), SolverTrackInfo->CurrentFrame, SolverTrackInfo->CurrentStep);
			}

			bIsReSimFrame = SolverTrackInfo->bIsReSimulated;

			FramesTimelineWidget->SetTargetFrameTime(CurrentPlaybackControllerPtr->GetFrameTimeForTrack(EChaosVDTrackType::Solver, SolverID, *SolverTrackInfo));
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

void SChaosVDSolverPlaybackControls::HandlePlaybackButtonClicked(EChaosVDPlaybackButtonsID ButtonID)
{
	Chaos::VisualDebugger::HandleUserPlaybackInputControl(ButtonID, *this, PlaybackController);
}

const FSlateBrush* SChaosVDSolverPlaybackControls::GetFrameTypeBadgeBrush() const
{
	const FButtonStyle& ButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Menu.Button");
	return bIsReSimFrame ? &ButtonStyle.Pressed : FCoreStyle::Get().GetBrush("Border");
}

TSharedPtr<SWidget> SChaosVDSolverPlaybackControls::CreateVisibilityWidget()
{
	return SNew(SButton)
			.OnClicked_Raw(this, &SChaosVDSolverPlaybackControls::ToggleSolverVisibility)
			[
				SNew(SImage)
				.Image_Raw(this,&SChaosVDSolverPlaybackControls::GetBrushForCurrentVisibility)
				.DesiredSizeOverride(FVector2D(16.0f,16.0f))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];	
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

void SChaosVDSolverPlaybackControls::RegisterNewController(TWeakPtr<FChaosVDPlaybackController> NewController)
{
	if (const TSharedPtr<FChaosVDPlaybackController> OldPlaybackControllerPtr = PlaybackController.Pin())
	{
		if (TSharedPtr<FChaosVDScene> Scene = OldPlaybackControllerPtr->GetControllerScene().Pin())
		{
			Scene->OnSolverVisibilityUpdated().RemoveAll(this);
		}
	}

	FChaosVDPlaybackControllerObserver::RegisterNewController(NewController);

	if (const TSharedPtr<FChaosVDPlaybackController> NewPlaybackControllerPtr = PlaybackController.Pin())
	{
		if (TSharedPtr<FChaosVDScene> Scene = NewPlaybackControllerPtr->GetControllerScene().Pin())
		{
			Scene->OnSolverVisibilityUpdated().AddRaw(this, &SChaosVDSolverPlaybackControls::HandleSolverVisibilityChanged);
		}
	}
}

#undef LOCTEXT_NAMESPACE
