// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDPlaybackViewport.h"

#include "ChaosVDCommands.h"
#include "ChaosVDEditorMode.h"
#include "ChaosVDEditorModeTools.h"
#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportClient.h"
#include "ChaosVDScene.h"
#include "EditorModeManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Widgets/ChaosVDPlaybackControlsHelper.h"
#include "Widgets/SChaosVDTimelineWidget.h"
#include "Widgets/SChaosVDViewportToolbar.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

namespace Chaos::VisualDebugger::Cvars
{
	static bool bBroadcastGameFrameUpdateEvenIfNotChanged = false;
	static FAutoConsoleVariableRef CVarChaosVDBroadcastGameFrameUpdateEvenIfNotChanged(
		TEXT("p.Chaos.VD.Tool.BroadcastGameFrameUpdateEvenIfNotChanged"),
		bBroadcastGameFrameUpdateEvenIfNotChanged,
		TEXT("If true, each time we get a controller data updated event, a game frame update will be triggered even if the frame didn't change..."));
}

SChaosVDPlaybackViewport::~SChaosVDPlaybackViewport()
{
	PlaybackViewportClient->Viewport = nullptr;
	PlaybackViewportClient.Reset();
}

void SChaosVDPlaybackViewport::Construct(const FArguments& InArgs, TWeakPtr<FChaosVDScene> InScene, TWeakPtr<FChaosVDPlaybackController> InPlaybackController, TSharedPtr<FEditorModeTools> InEditorModeTools)
{
	Extender = MakeShared<FExtender>();

	EditorModeTools = InEditorModeTools;
	EditorModeTools->SetWidgetMode(UE::Widget::WM_Translate);
	EditorModeTools->SetDefaultMode(UChaosVDEditorMode::EM_ChaosVisualDebugger);
	EditorModeTools->ActivateDefaultMode();

	SEditorViewport::Construct(SEditorViewport::FArguments());

	CVDSceneWeakPtr = InScene;
	TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin();
	ensure(ScenePtr.IsValid());
	ensure(InPlaybackController.IsValid());

	PlaybackViewportClient = StaticCastSharedPtr<FChaosVDPlaybackViewportClient>(GetViewportClient());

	// TODO: Add a way to gracefully shutdown (close) the tool when a no recoverable situation like this happens (UE-191876)
	check(PlaybackViewportClient.IsValid());
	
	PlaybackViewportClient->SetScene(InScene);
	
	if (UChaosVDEditorMode* CVDEdMode = Cast<UChaosVDEditorMode>(EditorModeTools->GetActiveScriptableMode(UChaosVDEditorMode::EM_ChaosVisualDebugger)))
	{
		if (ScenePtr.IsValid())
		{
			CVDEdMode->SetWorld(ScenePtr->GetUnderlyingWorld());
		}
	}

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
				.IsEnabled_Raw(this, &SChaosVDPlaybackViewport::CanPlayback)
				.ButtonVisibilityFlags(static_cast<uint16>(EChaosVDTimelineElementIDFlags::AllPlayback))
				.OnFrameChanged_Raw(this, &SChaosVDPlaybackViewport::OnFrameSelectionUpdated)
				.OnButtonClicked(this, &SChaosVDPlaybackViewport::HandlePlaybackButtonClicked)
				.MaxFrames(0)
			]
		]	
	];

	RegisterNewController(InPlaybackController);
}

TSharedRef<SEditorViewport> SChaosVDPlaybackViewport::GetViewportWidget()
{
	return StaticCastSharedRef<SEditorViewport>(AsShared());
}

TSharedPtr<FExtender> SChaosVDPlaybackViewport::GetExtenders() const
{
	return Extender;
}

void SChaosVDPlaybackViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FChaosVDCommands& Commands = FChaosVDCommands::Get();

	if (ensure(Client))
	{
		const TSharedRef<FChaosVDPlaybackViewportClient> ViewportClientRef = StaticCastSharedRef<FChaosVDPlaybackViewportClient>(Client.ToSharedRef());
		CommandList->MapAction(
			Commands.TrackUntrackSelectedObject,
			FExecuteAction::CreateSP(ViewportClientRef, &FChaosVDPlaybackViewportClient::ToggleObjectTrackingIfSelected));
	}
}

EVisibility SChaosVDPlaybackViewport::GetTransformToolbarVisibility() const
{
	// We want to always show the transform tool bar. We disable each action that is not supported for a selected actor individually.
	// Without doing this, if you select an unsupported mode, the entire toolbar disappears
	return EVisibility::Visible;
}

TSharedRef<FEditorViewportClient> SChaosVDPlaybackViewport::MakeEditorViewportClient()
{
	TSharedPtr<FChaosVDPlaybackViewportClient> NewViewport = MakeShared<FChaosVDPlaybackViewportClient>(EditorModeTools, GetViewportWidget());

	NewViewport->SetAllowCinematicControl(false);
	
	NewViewport->bSetListenerPosition = false;
	NewViewport->EngineShowFlags = FEngineShowFlags(ESFIM_Editor);
	NewViewport->LastEngineShowFlags = FEngineShowFlags(ESFIM_Editor);
	NewViewport->ViewportType = LVT_Perspective;
	NewViewport->bDrawAxes = true;
	NewViewport->bDisableInput = false;
	NewViewport->VisibilityDelegate.BindLambda([] {return true; });

	NewViewport->EngineShowFlags.DisableAdvancedFeatures();
	NewViewport->EngineShowFlags.SetSelectionOutline(true);
	NewViewport->EngineShowFlags.SetSnap(false);
	NewViewport->EngineShowFlags.SetBillboardSprites(true);

	return StaticCastSharedRef<FEditorViewportClient>(NewViewport.ToSharedRef());
}

TSharedPtr<SWidget> SChaosVDPlaybackViewport::MakeViewportToolbar()
{
	// Build our toolbar level toolbar
	TSharedRef< SChaosVDViewportToolbar > ToolBar = SNew(SChaosVDViewportToolbar, SharedThis(this));

	return 
		SNew(SVerticalBox)
		.Visibility( EVisibility::SelfHitTestInvisible )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 1.0f, 0, 0)
		.VAlign(VAlign_Top)
		[
			ToolBar
		];
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

			const bool bNeedsToBroadcastChange = Chaos::VisualDebugger::Cvars::bBroadcastGameFrameUpdateEvenIfNotChanged || (GameFramesTimelineWidget->GetCurrentFrame() == 0 || GameFramesTimelineWidget->GetCurrentFrame() != TrackInfo->CurrentFrame);
			GameFramesTimelineWidget->SetCurrentTimelineFrame(TrackInfo->CurrentFrame, bNeedsToBroadcastChange ? EChaosVDSetTimelineFrameFlags::BroadcastChange : EChaosVDSetTimelineFrameFlags::Silent);

			constexpr uint16 PlaybackElementDisabledDuringLiveSession = static_cast<uint16>(EChaosVDTimelineElementIDFlags::Stop | EChaosVDTimelineElementIDFlags::Next | EChaosVDTimelineElementIDFlags::Prev);
			if (ControllerSharedPtr->IsPlayingLiveSession())
			{
				if (!ControllerSharedPtr->HasPauseRequest())
				{
					GameFramesTimelineWidget->SetAutoStopEnabled(false);
					GameFramesTimelineWidget->Play();
				}

				uint16& CurrentEnabledFlags = GameFramesTimelineWidget->GetMutableElementEnabledFlagsRef();
				CurrentEnabledFlags = CurrentEnabledFlags &~ PlaybackElementDisabledDuringLiveSession;
			}
			else
			{
				const FGuid CurrentPlaybackInstigatorID = ControllerSharedPtr->GetPlaybackInstigatorWithExclusiveControlsID();
				const bool bUserCanControlPlayback = CurrentPlaybackInstigatorID == InvalidGuid || CurrentPlaybackInstigatorID == GetInstigatorID();

				// When it is not a live session, the Game Frames timeline follows the same rule as other timelines. The controls are locked unless we are who started a Play action
				GameFramesTimelineWidget->SetIsLocked(!bUserCanControlPlayback);

				GameFramesTimelineWidget->SetAutoStopEnabled(true);
			}
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
	if (const TSharedPtr<FChaosVDPlaybackController> ControllerSharedPtr = InController.Pin())
	{
		// The frame number we receive could be from a Solver track, so make sure it is converted to the correct game track frame number
		if (FChaosVDTrackInfo* GameTrackInfo = ControllerSharedPtr->GetMutableTrackInfo(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID))
		{
			const int32 GameTrackFrame = ControllerSharedPtr->ConvertCurrentFrameToOtherTrackFrame(UpdatedTrackInfo, GameTrackInfo);

			// Something other than us advanced the game track, so make sure the timeline widget is updated
			if (InstigatorGuid != GetInstigatorID())
			{
				GameFramesTimelineWidget->SetCurrentTimelineFrame(GameTrackFrame, EChaosVDSetTimelineFrameFlags::None);
				GameTrackInfo->CurrentFrame = GameTrackFrame;
			}

			GameFramesTimelineWidget->SetTargetFrameTime(ControllerSharedPtr->GetFrameTimeForTrack(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID, *GameTrackInfo));
		}
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

void SChaosVDPlaybackViewport::OnSolverVisibilityUpdated(int32 SolverID, bool bNewVisibility)
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
				ScenePtr->OnSolverVisibilityUpdated().RemoveAll(this);
			}
		}

		FChaosVDPlaybackControllerObserver::RegisterNewController(NewController);

		if (const TSharedPtr<FChaosVDPlaybackController> NewPlaybackControllerPtr = PlaybackController.Pin())
		{
			if (TSharedPtr<FChaosVDScene> ScenePtr = NewPlaybackControllerPtr->GetControllerScene().Pin())
			{
				ScenePtr->OnSceneUpdated().AddRaw(this, &SChaosVDPlaybackViewport::OnPlaybackSceneUpdated);
				ScenePtr->OnSolverVisibilityUpdated().AddRaw(this, &SChaosVDPlaybackViewport::OnSolverVisibilityUpdated);
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

void SChaosVDPlaybackViewport::HandlePlaybackButtonClicked(EChaosVDPlaybackButtonsID ButtonID)
{
	Chaos::VisualDebugger::HandleUserPlaybackInputControl(ButtonID, *this, PlaybackController);
}

bool SChaosVDPlaybackViewport::CanPlayback() const
{
	const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = PlaybackController.Pin();

	return PlaybackControllerPtr && PlaybackControllerPtr->IsRecordingLoaded();
}

#undef LOCTEXT_NAMESPACE
