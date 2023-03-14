// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCWebInterface.h"

#include "RCWebInterfacePrivate.h"
#include "RCWebInterfaceProcess.h"
#include "RemoteControlSettings.h"
#include "IWebRemoteControlModule.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "Editor.h"
#include "RCWebInterfaceCustomizations.h"
#endif

#define LOCTEXT_NAMESPACE "FRemoteControlWebInterfaceModule"

static TAutoConsoleVariable<int32> CVarRCWebInterfaceAutoStart(
	TEXT("RCWebInterface.AutoStart"),
	1,
	TEXT("Auto Start Remote Control Web App"));

namespace RCWebInterface
{
	bool IsWebInterfaceEnabled()
	{
		bool bIsEditor = false;

#if WITH_EDITOR
		bIsEditor = GIsEditor;
#endif
		// By default, remote control web interface is disabled in -game and packaged game.
		return (!IsRunningCommandlet() && bIsEditor) || FParse::Param(FCommandLine::Get(), TEXT("RCWebInterfaceEnable"));
	}
}

void FRemoteControlWebInterfaceModule::StartupModule()
{
	WebApp = MakeShared<FRemoteControlWebInterfaceProcess>();

	if (!RCWebInterface::IsWebInterfaceEnabled())
	{
		UE_LOG(LogRemoteControlWebInterface, Display, TEXT("Remote Control Web Interface is disabled by default when running outside the editor. Use the -RCWebInterfaceEnable flag when launching in order to use it."));
		return;
	}

	if (CVarRCWebInterfaceAutoStart.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogRemoteControlWebInterface, Display, TEXT("Remote Control Web Interface did not launch WebApp because CVar RCWebInterface.AutoStart is set to 0"));
		return;
	}
	
	TSharedPtr<FRemoteControlWebInterfaceProcess> WebAppLocal = WebApp;

	FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([WebAppLocal]()
		{
			WebAppLocal->Start();
		});

	if (IWebRemoteControlModule* WebRemoteControlModule = FModuleManager::GetModulePtr<IWebRemoteControlModule>("WebRemoteControl"))
	{
		WebSocketServerStartedDelegate = WebRemoteControlModule->OnWebSocketServerStarted().AddLambda([this](uint32)
			{
				WebApp->Shutdown();
				WebApp->Start();
			});
	}

#if WITH_EDITOR
	GetMutableDefault<URemoteControlSettings>()->OnSettingChanged().AddRaw(this, &FRemoteControlWebInterfaceModule::OnSettingsModified);

	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
		{
			Customizations = MakePimpl<FRCWebInterfaceCustomizations>(WebApp);
		});
#endif
}

void FRemoteControlWebInterfaceModule::ShutdownModule()
{
	if (!RCWebInterface::IsWebInterfaceEnabled())
	{
		return;
	}

	WebApp->Shutdown();

	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
#endif
	
	if (IWebRemoteControlModule* WebRemoteControlModule = FModuleManager::GetModulePtr<IWebRemoteControlModule>("WebRemoteControl"))
	{
		WebRemoteControlModule->OnWebSocketServerStarted().Remove(WebSocketServerStartedDelegate);
	}
}

bool FRemoteControlWebInterfaceModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (!FParse::Command(&Cmd, TEXT("RCWebInterface")))
	{
		return false;
	}

	FRemoteControlWebInterfaceModule& RCWebInf = FRemoteControlWebInterfaceModule::Get();
	const FRemoteControlWebInterfaceProcess::EStatus WebAppStatus = RCWebInf.WebApp->GetStatus();

	const bool bIsCurrentlyRunning = (
		WebAppStatus == FRemoteControlWebInterfaceProcess::EStatus::Running ||
		WebAppStatus == FRemoteControlWebInterfaceProcess::EStatus::Launching
	);

	if (FParse::Command(&Cmd, TEXT("Start")))
	{
		if (!bIsCurrentlyRunning) // Ignore if already running
		{
			Ar.Log(TEXT("RCWebInterface: Starting WebApp"));

			RCWebInf.WebApp->Start();
		}
		else
		{
			Ar.Log(TEXT("RCWebInterface: WebApp was already running"));
		}
	}
	else if (FParse::Command(&Cmd, TEXT("Stop")))
	{
		Ar.Log(TEXT("RCWebInterface: Stopping WebApp"));

		RCWebInf.WebApp->Shutdown();
	}
	else if (FParse::Command(&Cmd, TEXT("Restart")))
	{
		Ar.Log(TEXT("RCWebInterface: Restarting WebApp"));

		RCWebInf.WebApp->Shutdown();
		RCWebInf.WebApp->Start();
	}

	return true;
}

void FRemoteControlWebInterfaceModule::OnSettingsModified(UObject* Settings, FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(URemoteControlSettings, RemoteControlWebInterfacePort) || PropertyName == GET_MEMBER_NAME_CHECKED(URemoteControlSettings, bForceWebAppBuildAtStartup))
	{
		WebApp->Shutdown();
		WebApp->Start();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(URemoteControlSettings, bWebAppLogRequestDuration))
	{
		const URemoteControlSettings* RCSettings = CastChecked<URemoteControlSettings>(Settings);
		WebApp->SetExternalLoggerEnabled(RCSettings->bWebAppLogRequestDuration);
	}
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(LogRemoteControlWebInterface);

IMPLEMENT_MODULE(FRemoteControlWebInterfaceModule, RemoteControlWebInterface)