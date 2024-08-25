// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPScoutingMode.h"
#include "VirtualScoutingEditorModule.h"
#include "VirtualScoutingOpenXR.h"
#include "VirtualScoutingOpenXRModule.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Framework/Notifications/NotificationManager.h"
#include "IOpenXRHMDModule.h"
#include "IVREditorModule.h"
#include "IXRTrackingSystem.h"
#include "Widgets/Notifications/SNotificationList.h"


#define LOCTEXT_NAMESPACE "VirtualScouting"


static const FName OpenXRSystemName = FName(TEXT("OpenXR"));


UVPScoutingMode::UVPScoutingMode()
{
	InteractorClass = FSoftObjectPath("/VirtualProductionUtilities/VR/VirtualScoutingInteractor.VirtualScoutingInteractor_C");
	TeleporterClass = FSoftObjectPath("/VirtualProductionUtilities/VR/VirtualScoutingTeleporter.VirtualScoutingTeleporter_C");
}


bool UVPScoutingMode::NeedsSyntheticDpad()
{
	if (!GEngine->XRSystem.IsValid() || GEngine->XRSystem->GetSystemName() != OpenXRSystemName)
	{
		return Super::NeedsSyntheticDpad();
	}

	return !IOpenXRHMDModule::Get().IsExtensionEnabled("XR_EXT_dpad_binding");
}


void UVPScoutingMode::Enter()
{
	if (!GEngine->XRSystem.IsValid() || GEngine->XRSystem->GetSystemName() != OpenXRSystemName)
	{
		Super::Enter();
		return;
	}

	TSharedPtr<FVirtualScoutingOpenXRExtension> XrExt = FVirtualScoutingOpenXRModule::Get().GetOpenXRExt();
	if (!XrExt)
	{
		UE_LOG(LogVirtualScoutingEditor, Error, TEXT("OpenXR extension plugin invalid"));
		IVREditorModule::Get().EnableVREditor(false);
		return;
	}

	if (!ValidateSettings())
	{
		IVREditorModule::Get().EnableVREditor(false);
		return;
	}

#if WITH_EDITOR
	// This causes FOpenXRInput to rebuild and reattach actions.
	FEditorDelegates::OnActionAxisMappingsChanged.Broadcast();
#endif

	// Split the mode entry into two phases. This is necessary because we have to poll OpenXR for
	// the active interaction profile and translate it into a legacy plugin name, but OpenXR may
	// not return the correct interaction profile for several frames after the OpenXR session
	// (stereo rendering) has started, and we need to defer creation of the interactors, etc.
	BeginEntry();

	XrExt->GetHmdDeviceTypeFuture() = XrExt->GetHmdDeviceTypeFuture().Next(
		[WeakThis = TWeakObjectPtr<UVPScoutingMode>(this)]
		(FName DeviceType)
		{
			UVPScoutingMode* This = WeakThis.Get();
			if (!This || !This->bIsFullyInitialized)
			{
				UE_LOG(LogVirtualScoutingEditor, Warning, TEXT("Stale UVPScoutingMode; ignoring HmdDeviceType future"));
				return DeviceType;
			}

			if (DeviceType == NAME_None)
			{
				UE_LOG(LogVirtualScoutingEditor, Error, TEXT("Unable to map legacy HMD device type"));
				return DeviceType;
			}

			This->SetHMDDeviceTypeOverride(DeviceType);
			This->SetupSubsystems();
			This->FinishEntry();

			return DeviceType;
		});
}


bool UVPScoutingMode::ValidateSettings()
{
	bool bSettingsValid = true;

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


void UVPScoutingMode::InvalidSettingNotification(const FText& ErrorDetails)
{
	FText NotificationHeading = LOCTEXT("InvalidSettingNotificationTitle", "Unable to initialize Virtual Scouting");

	UE_LOG(LogVirtualScoutingEditor, Error, TEXT("%s: %s"), *NotificationHeading.ToString(), *ErrorDetails.ToString());

	FNotificationInfo Info(NotificationHeading);
	Info.SubText = ErrorDetails;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification)
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}


#undef LOCTEXT_NAMESPACE
