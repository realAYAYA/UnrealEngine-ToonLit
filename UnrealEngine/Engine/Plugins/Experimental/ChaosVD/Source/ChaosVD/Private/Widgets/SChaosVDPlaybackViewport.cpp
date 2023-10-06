// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDPlaybackViewport.h"

#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportClient.h"
#include "ChaosVDScene.h"
#include "Framework/Application/SlateApplication.h"
#include "LevelEditorViewport.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SChaosVDTimelineWidget.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

SChaosVDPlaybackViewport::~SChaosVDPlaybackViewport()
{
	PlaybackViewportClient->Viewport = nullptr;
	PlaybackViewportClient.Reset();
}

TSharedPtr<FChaosVDPlaybackViewportClient> SChaosVDPlaybackViewport::CreateViewportClient() const
{
	TSharedPtr<FChaosVDPlaybackViewportClient> NewViewport = MakeShared<FChaosVDPlaybackViewportClient>();

	NewViewport->SetAllowCinematicControl(false);
	
	NewViewport->bSetListenerPosition = false;
	NewViewport->EngineShowFlags = FEngineShowFlags(ESFIM_Editor);
	NewViewport->LastEngineShowFlags = FEngineShowFlags(ESFIM_Editor);
	NewViewport->ViewportType = LVT_Perspective;
	NewViewport->bDrawAxes = true;
	NewViewport->bDisableInput = false;
	NewViewport->VisibilityDelegate.BindLambda([] {return true; });
	NewViewport->EngineShowFlags.SetSelectionOutline(true);

	return NewViewport;
}

void SChaosVDPlaybackViewport::Construct(const FArguments& InArgs, TWeakPtr<FChaosVDScene> InScene, TWeakPtr<FChaosVDPlaybackController> InPlaybackController)
{
	TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin();
	ensure(ScenePtr.IsValid());
	ensure(InPlaybackController.IsValid());

	PlaybackViewportClient = CreateViewportClient();

	ViewportWidget = SNew(SViewport)
		.RenderDirectlyToWindow(false)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		.EnableGammaCorrection(false)
		.EnableBlending(false);

	SceneViewport = MakeShareable(new FSceneViewport(PlaybackViewportClient.Get(), ViewportWidget));

	PlaybackViewportClient->Viewport = SceneViewport.Get();

	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	
	// Default to the base map
	PlaybackViewportClient->SetScene(InScene);

	ChildSlot
	[
		// 3D Viewport
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(0.9f)
		[
			ViewportWidget.ToSharedRef()
		]
		// Playback controls
		// TODO: Now that the tool is In-Editor, see if we can/is worth use the Sequencer widgets
		// instead of these custom ones
		+SVerticalBox::Slot()
		.Padding(16.0f, 16.0f, 16.0f, 16.0f)
		.FillHeight(0.1f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(LOCTEXT("PlaybackViewportWidgetGameFramesLabel", "Game Frames" ))
			]
			+SVerticalBox::Slot()
			[
				SAssignNew(GameFramesTimelineWidget, SChaosVDTimelineWidget)
				.HidePlayStopButtons(false)
				.HideLockButton(true)
				.OnFrameChanged_Raw(this, &SChaosVDPlaybackViewport::OnFrameSelectionUpdated)
				.MaxFrames(0)
			]
		]	
	];

	RegisterNewController(InPlaybackController);
}

void SChaosVDPlaybackViewport::HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InController)
{
	if (PlaybackController != InController)
	{
		RegisterNewController(InController);
	}

	const TSharedPtr<FChaosVDPlaybackController> ControllerSharedPtr = PlaybackController.Pin();
	if (ControllerSharedPtr.IsValid() && ControllerSharedPtr->IsRecordingLoaded())
	{
		if (const FChaosVDTrackInfo* TrackInfo = ControllerSharedPtr->GetTrackInfo(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID))
		{
			// Max is inclusive and we use this to request as the index on the recorded frames/steps arrays so we need to -1 to the available frames/steps
			GameFramesTimelineWidget->UpdateMinMaxValue(0, TrackInfo->MaxFrames != INDEX_NONE ? TrackInfo->MaxFrames -1  : 0);
		}
	}
	else
	{
		GameFramesTimelineWidget->UpdateMinMaxValue(0, 0);
		GameFramesTimelineWidget->ResetTimeline();
	}

	PlaybackViewportClient->bNeedsRedraw = true;
}

void SChaosVDPlaybackViewport::HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid)
{
	if (InstigatorGuid == GetInstigatorID())
	{
		// Ignore the update if we initiated it
		return;
	}

	if (TSharedPtr<FChaosVDPlaybackController> ControllerSharedPtr = InController.Pin())
	{
		// The frame number we receive could be from a Solver track, so make sure it is converted to the correct game track frame number 
		int32 GameTrackFrame = ControllerSharedPtr->ConvertCurrentFrameToOtherTrackFrame(UpdatedTrackInfo, ControllerSharedPtr->GetTrackInfo(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID));
		GameFramesTimelineWidget->SetCurrentTimelineFrame(GameTrackFrame, EChaosVDSetTimelineFrameFlags::None);
	}
}

void SChaosVDPlaybackViewport::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet)
{
	PlaybackViewportClient->bNeedsRedraw = true;
}

void SChaosVDPlaybackViewport::OnPlaybackSceneUpdated()
{
	PlaybackViewportClient->bNeedsRedraw = true;
}

void SChaosVDPlaybackViewport::RegisterNewController(TWeakPtr<FChaosVDPlaybackController> NewController)
{
	if (PlaybackController != NewController)
	{
		if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = PlaybackController.Pin())
		{
			if (TSharedPtr<FChaosVDScene> ScenePtr = CurrentPlaybackControllerPtr->GetControllerScene().Pin())
			{
				ScenePtr->OnSceneUpdated().RemoveAll(this);
			}
		}

		FChaosVDPlaybackControllerObserver::RegisterNewController(NewController);

		if (const TSharedPtr<FChaosVDPlaybackController> NewPlaybackControllerPtr = PlaybackController.Pin())
		{
			if (TSharedPtr<FChaosVDScene> ScenePtr = NewPlaybackControllerPtr->GetControllerScene().Pin())
			{
				ScenePtr->OnSceneUpdated().AddRaw(this, &SChaosVDPlaybackViewport::OnPlaybackSceneUpdated);
			}
		}
	}
}

void SChaosVDPlaybackViewport::OnFrameSelectionUpdated(int32 NewFrameIndex) const
{
	if (const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = PlaybackController.Pin())
	{
		constexpr int32 StepNumber = 0;
		PlaybackControllerPtr->GoToTrackFrame(GetInstigatorID(), EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID, NewFrameIndex, StepNumber);

		PlaybackViewportClient->bNeedsRedraw = true;
	}
}

#undef LOCTEXT_NAMESPACE
