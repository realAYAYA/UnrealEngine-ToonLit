// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeVREditorMode.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IVREditorModule.h"
#include "IXRTrackingSystem.h"
#include "LevelEditorActions.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "OpenXRInputFunctionLibrary.h"
#include "PlayerMappableInputConfig.h"
#include "SLevelViewport.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "XRCreativeAvatar.h"
#include "XRCreativeEditorModule.h"
#include "XRCreativeToolset.h"


#define LOCTEXT_NAMESPACE "XRCreativeVREditorMode"


UXRCreativeVREditorMode::UXRCreativeVREditorMode()
{
}


void UXRCreativeVREditorMode::Enter()
{
	FEditorScriptExecutionGuard ScriptGuard;

	bWantsToExitMode = false;
	bFirstTick = true;

	if (!ValidateSettings())
	{
		IVREditorModule::Get().EnableVREditor(false);
		return;
	}

	UXRCreativeToolset* Toolset = ToolsetClass.LoadSynchronous();
	Avatar = Cast<AXRCreativeAvatar>(SpawnTransientSceneActor(Toolset->Avatar, "XRCreativeAvatar"));
	Avatar->RegisterObjectForInput(this);
	Avatar->ConfigureToolset(Toolset);

	Super::Enter();

	BP_OnEnter();
	Avatar->BP_OnVRInitialize();
}


void UXRCreativeVREditorMode::Exit(bool bInShouldDisableStereo)
{
	FEditorScriptExecutionGuard ScriptGuard;

	BP_OnExit();

	if (bActuallyUsingVR && Avatar)
	{
		// Set viewport camera to head location (minus roll).
		FTransform HeadTransform = Avatar->GetHeadTransform();
		FRotator HeadRotator = HeadTransform.Rotator();
		HeadRotator.Roll = 0.0;
		HeadTransform.SetRotation(HeadRotator.Quaternion());
		SetRoomTransform(HeadTransform);
	}

	if (Avatar)
	{
		Avatar->UnregisterObjectForInput(this);
		DestroyTransientActor(Avatar);
		Avatar = nullptr;
	}

	Super::Exit(bInShouldDisableStereo);
}


void UXRCreativeVREditorMode::Tick(float InDeltaSeconds)
{
	FEditorScriptExecutionGuard ScriptGuard;

	Super::Tick(InDeltaSeconds);

	Avatar->Tick(InDeltaSeconds);
	
	// It's too early to get the head pose during Enter(), so finish setup here.
	if (UNLIKELY(bFirstTick))
	{
		SetHeadTransform(GetRoomTransform());
		Avatar->SetActorTransform(GetRoomTransform());
		bFirstTick = false;
	}

	SetRoomTransform(Avatar->GetActorTransform());

	BP_Tick(InDeltaSeconds);
}


bool UXRCreativeVREditorMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	if (InEvent == IE_Pressed)
	{
		const FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
		auto ChordMatched = [InKey, Modifiers](const FInputChord& Chord) -> bool
		{
			if (!Chord.IsValidChord())
			{
				return false;
			}

			return Chord.Key == InKey &&
				Chord.bAlt == Modifiers.IsAltDown() &&
				Chord.bCmd == Modifiers.IsCommandDown() &&
				Chord.bCtrl == Modifiers.IsControlDown() &&
				Chord.bShift == Modifiers.IsShiftDown();
		};

		const FLevelEditorCommands& LevelEditorCommands = FLevelEditorCommands::Get();
		if (FUICommandInfo* ToggleVrCommand = LevelEditorCommands.ToggleVR.Get())
		{
			if (ChordMatched(ToggleVrCommand->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get()) ||
				ChordMatched(ToggleVrCommand->GetActiveChord(EMultipleKeyBindingIndex::Secondary).Get()))
			{
				bWantsToExitMode = true;
				return true;
			}
		}
	}

	return Super::InputKey(InViewportClient, InViewport, InKey, InEvent);
}


FTransform UXRCreativeVREditorMode::GetRoomTransform() const
{
	if (TSharedPtr<const SLevelViewport> Viewport = GetVrLevelViewport())
	{
		const FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
		return FTransform(
			ViewportClient.GetViewRotation().Quaternion(),
			ViewportClient.GetViewLocation(),
			FVector(1.0f));
	}

	return FTransform();
}


FTransform UXRCreativeVREditorMode::GetHeadTransform() const
{
	if (Avatar)
	{
		return Avatar->GetHeadTransform();
	}

	return GetRoomTransform();
}


bool UXRCreativeVREditorMode::GetLaserForHand(EControllerHand InHand, FVector& OutLaserStart, FVector& OutLaserEnd) const
{
	if (Avatar)
	{
		return Avatar->GetLaserForHand(InHand, OutLaserStart, OutLaserEnd);
	}

	return false;
}


void UXRCreativeVREditorMode::SetRoomTransform(const FTransform& RoomToWorld)
{
	if (TSharedPtr<SLevelViewport> Viewport = GetVrLevelViewport())
	{
		FLevelEditorViewportClient& ViewportClient = Viewport->GetLevelViewportClient();
		ViewportClient.SetViewLocation(RoomToWorld.GetLocation());
		ViewportClient.SetViewRotation(RoomToWorld.GetRotation().Rotator());

		// Forcibly dirty the viewport camera location
		const bool bDollyCamera = false;
		ViewportClient.MoveViewportCamera(FVector::ZeroVector, FRotator::ZeroRotator, bDollyCamera);
	}
}


void UXRCreativeVREditorMode::SetHeadTransform(const FTransform& HeadToWorld)
{
	if (!Avatar)
	{
		return;
	}

	FRotator UseRotation(HeadToWorld.GetRotation().Rotator());

	const FTransform HeadTransform = Avatar->GetHeadTransformRoomSpace() * GetRoomTransform(); // NB: Based on viewport, not actor
	float YawDifference = HeadTransform.GetRotation().Rotator().Yaw - UseRotation.Yaw;
	UseRotation.Yaw = GetRoomTransform().GetRotation().Rotator().Yaw - YawDifference;
	UseRotation.Pitch = 0.0f;
	UseRotation.Roll = 0.0f;

	FVector Location = HeadToWorld.GetLocation();
	FVector HMDLocationOffset = Avatar->GetHeadTransformRoomSpace().GetLocation();
	HMDLocationOffset = UseRotation.RotateVector(HMDLocationOffset);
	Location -= HMDLocationOffset;

	SetRoomTransform(FTransform(UseRotation.Quaternion(), Location));
}


void UXRCreativeVREditorMode::EnableStereo()
{
	if (TSharedPtr<SLevelViewport> Viewport = GetVrLevelViewport())
	{
		Viewport->EnableStereoRendering(true);
		Viewport->SetRenderDirectlyToWindow(true);
	}
	
	TSet<UInputMappingContext*> Contexts;
	for (const TObjectPtr<UXRCreativeTool>& Tool : Avatar->GetTools())
	{
		if (UXRCreativeBlueprintableTool* BPTool = Cast<UXRCreativeBlueprintableTool>(Tool))
		{
			if (UInputMappingContext* ToolIMC = BPTool->GetToolInputMappingContext())
			{
				Contexts.Add(ToolIMC);
			}
		}
	}

	if (const UXRCreativeToolset* Toolset = Avatar->GetToolset())
	{
		if (Toolset->RightHandedInputMappingContext)
		{
			Contexts.Add(Toolset->RightHandedInputMappingContext);
		}

		if (Toolset->LeftHandedInputMappingContext)
		{
			Contexts.Add(Toolset->LeftHandedInputMappingContext);
		}
	}
	
	if (Contexts.IsEmpty())
	{
		UE_LOG(LogXRCreativeEditor, Warning, TEXT("No UInputMappingContexts provided in the UXRCreativeToolset. Action bindings will not be visible to the OpenXR runtime."));
	}

	Contexts.Empty(); // TODO this is a temporary fix because OpenXRInput.cpp does not respect input priorities. So instead we force it to fall back to legacy bindings because these actually work. 

	UOpenXRInputFunctionLibrary::BeginXRSession(Contexts);
}


void UXRCreativeVREditorMode::DisableStereo()
{
	UOpenXRInputFunctionLibrary::EndXRSession();

	if (TSharedPtr<SLevelViewport> VREditorLevelViewport = GetVrLevelViewport())
	{
		VREditorLevelViewport->EnableStereoRendering(false);
		VREditorLevelViewport->SetRenderDirectlyToWindow(false);
	}
}


bool UXRCreativeVREditorMode::ValidateSettings()
{
	bool bSettingsValid = true;

	UXRCreativeToolset* Toolset = ToolsetClass.LoadSynchronous();
	if (!Toolset)
	{
		InvalidSettingNotification(FText::Format(LOCTEXT("InvalidToolsetClass", "Unable to load toolset class {0}."), FText::FromString(ToolsetClass.GetLongPackageName())));
		bSettingsValid = false;
	}
	else if (!Toolset->Avatar)
	{
		InvalidSettingNotification(FText::Format(LOCTEXT("InvalidToolsetAvatar", "Unable to load avatar class from toolset {0}."), FText::FromString(ToolsetClass.GetLongPackageName())));
		bSettingsValid = false;
	}

	IConsoleManager& ConsoleMgr = IConsoleManager::Get();
	if (IConsoleVariable* PropagateAlpha = ConsoleMgr.FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha")))
	{
		if (PropagateAlpha->GetInt() != 0)
		{
			InvalidSettingNotification(LOCTEXT("InvalidCvarPropagateAlpha", "r.PostProcessing.PropagateAlpha must be set to 0 (and requires an engine restart)"));
			bSettingsValid = false;
		}
	}

	return bSettingsValid;
}


void UXRCreativeVREditorMode::InvalidSettingNotification(const FText& ErrorDetails)
{
	FText NotificationHeading = LOCTEXT("InvalidSettingNotificationTitle", "Unable to initialize XR Creative mode");

	UE_LOG(LogXRCreativeEditor, Error, TEXT("%s: %s"), *NotificationHeading.ToString(), *ErrorDetails.ToString());

	FNotificationInfo Info(NotificationHeading);
	Info.SubText = ErrorDetails;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification)
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}


#undef LOCTEXT_NAMESPACE
