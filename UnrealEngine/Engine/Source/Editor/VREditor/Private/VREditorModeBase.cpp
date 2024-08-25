// Copyright Epic Games, Inc. All Rights Reserved.

#include "VREditorModeBase.h"
#include "CameraController.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "IHeadMountedDisplay.h"
#include "ILevelEditor.h"
#include "IXRTrackingSystem.h"
#include "LevelEditor.h"
#include "LevelViewportActions.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#include "RenderCore.h"


namespace VREd
{
	extern FAutoConsoleVariable DefaultVRNearClipPlane;
	extern FAutoConsoleVariable DefaultWorldToMeters;
}


void UVREditorModeBase::Init()
{
	Super::Init();

	// @todo vreditor urgent: Turn on global editor hacks for VR Editor mode
	GEnableVREditorHacks = true;

	if (!SavedEditorStatePtr)
	{
		SavedEditorStatePtr = CreateSavedState();
	}
}

void UVREditorModeBase::Shutdown()
{
	// @todo vreditor urgent: Disable global editor hacks for VR Editor mode
	GEnableVREditorHacks = false;

	Super::Shutdown();
}


namespace UE::VREditor::Private
{
	TSharedPtr<SLevelViewport> TryGetActiveViewport()
	{
		const TSharedRef<ILevelEditor>& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
		if (!ActiveViewport)
		{
			return nullptr;
		}

		return StaticCastSharedRef<SLevelViewport>(ActiveViewport->AsWidget());
	}
}


void UVREditorModeBase::Enter()
{
	using namespace UE::VREditor::Private;

	TSharedPtr<SLevelViewport> ActiveLevelViewport = TryGetActiveViewport();

	if (ActiveLevelViewport && ActiveLevelViewport->GetCommandList() && FLevelViewportCommands::Get().SetDefaultViewportType)
	{
		ActiveLevelViewport->GetCommandList()->TryExecuteAction(
			FLevelViewportCommands::Get().SetDefaultViewportType.ToSharedRef());

		// If the active viewport was e.g. a cinematic viewport, changing it
		// back to default recreated it and our pointer might be stale.
		ActiveLevelViewport = TryGetActiveViewport();
	}

	if (!ensure(ActiveLevelViewport))
	{
		return;
	}

	ActiveLevelViewport->RemoveAllPreviews(true);
	StartViewport(ActiveLevelViewport);

	OnVRModeEntryCompleteEvent.Broadcast();
}


void UVREditorModeBase::Exit(bool bShouldDisableStereo)
{
	CloseViewport(bShouldDisableStereo);
	VREditorLevelViewportWeakPtr.Reset();
}


const SLevelViewport& UVREditorModeBase::GetLevelViewportPossessedForVR() const
{
	check(VREditorLevelViewportWeakPtr.IsValid());
	return *VREditorLevelViewportWeakPtr.Pin();
}


SLevelViewport& UVREditorModeBase::GetLevelViewportPossessedForVR()
{
	check(VREditorLevelViewportWeakPtr.IsValid());
	return *VREditorLevelViewportWeakPtr.Pin();
}


void UVREditorModeBase::StartViewport(TSharedPtr<SLevelViewport> Viewport)
{
	check(Viewport);

	VREditorLevelViewportWeakPtr = Viewport;

	FBaseSavedEditorState& SavedEditorState = SavedEditorStateChecked();
	FLevelEditorViewportClient& VRViewportClient = Viewport->GetLevelViewportClient();

	// Make sure we are in perspective mode
	// @todo vreditor: We should never allow ortho switching while in VR
	SavedEditorState.ViewportType = VRViewportClient.GetViewportType();
	VRViewportClient.SetViewportType(LVT_Perspective);

	// Set the initial camera location
	// @todo vreditor: This should instead be calculated using the currently active perspective camera's
	// location and orientation, compensating for the current HMD offset from the tracking space origin.
	// Perhaps, we also want to teleport the original viewport's camera back when we exit this mode, too!
	// @todo vreditor: Should save and restore camera position and any other settings we change (viewport type, pitch locking, etc.)
	SavedEditorState.ViewLocation = VRViewportClient.GetViewLocation();
	SavedEditorState.ViewRotation = VRViewportClient.GetViewRotation();

	// Don't allow the tracking space to pitch up or down.  People hate that in VR.
	// @todo vreditor: This doesn't seem to prevent people from pitching the camera with RMB drag
	SavedEditorState.bLockedPitch = VRViewportClient.GetCameraController()->GetConfig().bLockedPitch;
	if (bActuallyUsingVR)
	{
		VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = true;
	}

	// Set "game mode" to be enabled, to get better performance.  Also hit proxies won't work in VR, anyway
	VRViewportClient.SetVREditView(true);

	SavedEditorState.bRealTime = VRViewportClient.IsRealtime();
	VRViewportClient.SetRealtime(true);

	SavedEditorState.ShowFlags = VRViewportClient.EngineShowFlags;

	// Make sure the mode widgets don't come back when users click on things
	VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = false;

	// Force tiny near clip plane distance, because user can scale themselves to be very small.
	SavedEditorState.NearClipPlane = GNearClippingPlane;

	SetNearClipPlaneGlobals(VREd::DefaultVRNearClipPlane->GetFloat());

	SavedEditorState.bOnScreenMessages = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;

	// Save the world to meters scale
	const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
	const float SavedWorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : VRViewportClient.GetWorld()->GetWorldSettings()->WorldToMeters;
	SavedEditorState.WorldToMetersScale = SavedWorldToMeters;

	if (bActuallyUsingVR)
	{
		SavedEditorState.TrackingOrigin = GEngine->XRSystem->GetTrackingOrigin();
		GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
	}

	// Make the new viewport the active level editing viewport right away
	GCurrentLevelEditingViewportClient = &VRViewportClient;

	// Change viewport settings to more VR-friendly sequencer settings
	SavedEditorState.bCinematicControlViewport = VRViewportClient.AllowsCinematicControl();
	VRViewportClient.SetAllowCinematicControl(false);
	// Need to force fading and color scaling off in case we enter VR editing mode with a sequence open
	VRViewportClient.bEnableFading = false;
	VRViewportClient.bEnableColorScaling = false;
	VRViewportClient.Invalidate(true);

	if (bActuallyUsingVR)
	{
		if (!Viewport->IsImmersive())
		{
			// Switch to immersive mode
			const bool bWantImmersive = true;
			const bool bAllowAnimation = false;
			Viewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}

		EnableStereo();
	}
}

void UVREditorModeBase::CloseViewport( const bool bShouldDisableStereo )
{
	FBaseSavedEditorState& SavedEditorState = SavedEditorStateChecked();

	if (bActuallyUsingVR && bShouldDisableStereo)
	{
		DisableStereo();
	}

	if (TSharedPtr<SLevelViewport> VREditorLevelViewport = GetVrLevelViewport())
	{
		if ( bShouldDisableStereo && bActuallyUsingVR )
		{
			// Leave immersive mode
			const bool bWantImmersive = false;
			const bool bAllowAnimation = false;
			VREditorLevelViewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}

		FLevelEditorViewportClient& VRViewportClient = VREditorLevelViewport->GetLevelViewportClient();

		// Restore settings that we changed on the viewport
		VRViewportClient.SetViewportType(SavedEditorState.ViewportType);
		VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = SavedEditorState.bLockedPitch;
		VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = SavedEditorState.bAlwaysShowModeWidgetAfterSelectionChanges;
		VRViewportClient.EngineShowFlags = SavedEditorState.ShowFlags;
		VRViewportClient.SetVREditView(false);
		VRViewportClient.SetAllowCinematicControl(SavedEditorState.bCinematicControlViewport);
		VRViewportClient.bEnableFading = true;
		VRViewportClient.bEnableColorScaling = true;
		VRViewportClient.Invalidate(true);

		VRViewportClient.SetRealtime(SavedEditorState.bRealTime);

		SetNearClipPlaneGlobals(SavedEditorState.NearClipPlane);

		GAreScreenMessagesEnabled = SavedEditorState.bOnScreenMessages;

		if (bActuallyUsingVR)
		{
			GEngine->XRSystem->SetTrackingOrigin(SavedEditorState.TrackingOrigin);
		}

		// Restore WorldToMeters
		const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
		GetWorld()->GetWorldSettings()->WorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : SavedEditorState.WorldToMetersScale;
		ENGINE_API extern float GNewWorldToMetersScale;
		GNewWorldToMetersScale = 0.0f;
	}
}


void UVREditorModeBase::EnableStereo()
{
	if (TSharedPtr<SLevelViewport> Viewport = GetVrLevelViewport())
	{
		Viewport->EnableStereoRendering(true);
		Viewport->SetRenderDirectlyToWindow(true);
	}

	if (GEngine->XRSystem)
	{
		GEngine->StereoRenderingDevice->EnableStereo(true);
	}
}


void UVREditorModeBase::DisableStereo()
{
	if (GEngine->XRSystem)
	{
		GEngine->StereoRenderingDevice->EnableStereo(false);
	}

	if (TSharedPtr<SLevelViewport> Viewport = GetVrLevelViewport())
	{
		Viewport->EnableStereoRendering(false);
		Viewport->SetRenderDirectlyToWindow(false);
	}
}
