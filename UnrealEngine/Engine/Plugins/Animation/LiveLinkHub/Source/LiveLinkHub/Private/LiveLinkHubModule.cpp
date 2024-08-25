// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubModule.h"

#include "Clients/LiveLinkHubProvider.h"
#include "LiveLinkHubApplication.h"
#include "LiveLinkHubLog.h"
#include "Modules/ModuleManager.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Recording/LiveLinkHubRecordingController.h"

#if !WITH_LIVELINK_HUB
#include "HAL/FileManager.h"
#include "Misc/AsyncTaskNotification.h"
#include "ToolMenus.h"
#endif

#define LOCTEXT_NAMESPACE "LiveLinkHubModule"

void FLiveLinkHubModule::StartLiveLinkHub()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StartLiveLinkHub);
	// Nothing will get executed after this, so put everything before.
	LiveLinkHub = MakeShared<FLiveLinkHub>();
	LiveLinkHub->Initialize();

	LiveLinkHubLoop(LiveLinkHub);

	// If we got to this point, the app is shutdown so we should release our references.
	LiveLinkHub.Reset();
}

void FLiveLinkHubModule::StartupModule()
{
#if !WITH_LIVELINK_HUB  // When running in the editor
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->AddSection("VirtualProductionSection", LOCTEXT("VirtualProductionSection", "Virtual Production"));

	Section.AddMenuEntry("LiveLinkHub",
		LOCTEXT("LiveLinkHubLabel", "LiveLink Hub"),
		LOCTEXT("LiveLinkHubTooltip", "Launch the LiveLink Hub app."),
		FSlateIcon("LiveLinkStyle", "LiveLinkClient.Common.Icon.Small"),
		FUIAction(FExecuteAction::CreateRaw(this, &FLiveLinkHubModule::OpenLiveLinkHub)));
#endif
}

void FLiveLinkHubModule::ShutdownModule()
{
#if !WITH_LIVELINK_HUB
	UToolMenus::UnregisterOwner(this);
#endif
}

TSharedPtr<FLiveLinkHub> FLiveLinkHubModule::GetLiveLinkHub() const
{
	return LiveLinkHub;
}

TSharedPtr<FLiveLinkHubProvider> FLiveLinkHubModule::GetLiveLinkProvider() const
{
	return LiveLinkHub ? LiveLinkHub->LiveLinkProvider : nullptr;
}

TSharedPtr<FLiveLinkHubRecordingController> FLiveLinkHubModule::GetRecordingController() const
{
	return LiveLinkHub ? LiveLinkHub->RecordingController : nullptr;
}

TSharedPtr<FLiveLinkHubRecordingListController> FLiveLinkHubModule::GetRecordingListController() const
{
	return LiveLinkHub ? LiveLinkHub->RecordingListController : nullptr;
}

TSharedPtr<FLiveLinkHubPlaybackController> FLiveLinkHubModule::GetPlaybackController() const
{
	return LiveLinkHub ? LiveLinkHub->PlaybackController : nullptr;
}

#if !WITH_LIVELINK_HUB
void FLiveLinkHubModule::OpenLiveLinkHub() const
{
	FAsyncTaskNotificationConfig NotificationConfig;
	NotificationConfig.bKeepOpenOnFailure = true;
	NotificationConfig.TitleText = LOCTEXT("LaunchingLiveLinkHub", "Launching LiveLink Hub...");
	NotificationConfig.LogCategory = &LogLiveLinkHub;

	FAsyncTaskNotification Notification(NotificationConfig);

	// Find livelink hub executable location for our build configuration
	FString LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), FApp::GetBuildConfiguration());

	// Validate it exists and fall back to development if it doesn't.
	if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
	{
		LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), EBuildConfiguration::Development);

		// If it still doesn't exist, fall back to the shipping executable.
		if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
		{
			LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), EBuildConfiguration::Shipping);
		}
	}

	const FText LaunchLiveLinkHubErrorTitle = LOCTEXT("LaunchLiveLinkHubErrorTitle", "Failed to Launch LiveLinkhub.");
	if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_ExecutableMissing", "Could not find the executable. Have you compiled the LiveLink Hub app?"),
			false
			);

		return;
	}

	// Validate we do not have it running locally
	const FString AppName = FPaths::GetCleanFilename(LiveLinkHubPath);
	if (FPlatformProcess::IsApplicationRunning(*AppName))
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_AlreadyRunning", "A LiveLinkHub instance is already running."),
			false
			);
		return;
	}

	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;

	const FProcHandle ProcHandle = FPlatformProcess::CreateProc(*LiveLinkHubPath, TEXT(""), bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, nullptr, nullptr, nullptr);
	if (ProcHandle.IsValid())
	{
		Notification.SetComplete(
			LOCTEXT("LaunchedLiveLinkHub", "Launched LiveLink Hub"), FText(), true);

		return;
	}
	else // Very unlikely in practice, but possible in theory.
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_InvalidHandle", "Failed to create the LiveLink Hub process."),
			false);
	}
}
#endif /** WITH_LIVELINK_HUB */

TSharedPtr<FLiveLinkHubSubjectController> FLiveLinkHubModule::GetSubjectController() const
{
	return LiveLinkHub ? LiveLinkHub->SubjectController : nullptr;
}

TSharedPtr<ILiveLinkHubSessionManager> FLiveLinkHubModule::GetSessionManager() const
{
	return LiveLinkHub ? LiveLinkHub->SessionManager : nullptr;
}

IMPLEMENT_MODULE(FLiveLinkHubModule, LiveLinkHub);
#undef LOCTEXT_NAMESPACE /* LiveLinkHubModule */
