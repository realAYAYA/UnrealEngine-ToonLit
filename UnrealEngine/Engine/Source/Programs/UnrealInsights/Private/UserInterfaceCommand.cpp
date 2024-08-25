// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterfaceCommand.h"

#include "Async/TaskGraphInterfaces.h"
//#include "Brushes/SlateImageBrush.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "ISlateReflectorModule.h"
#include "ISourceCodeAccessModule.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "StandaloneRenderer.h"
#include "Widgets/Docking/SDockTab.h"

// Insights
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/Version.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#if PLATFORM_UNIX
#include <sys/file.h>
#include <errno.h>
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

#define IDEAL_FRAMERATE 60
#define BACKGROUND_FRAMERATE 4
#define IDLE_INPUT_SECONDS 5.0f

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UserInterfaceCommand
{
	TSharedRef<FWorkspaceItem> DeveloperTools = FWorkspaceItem::NewGroup(NSLOCTEXT("UnrealInsights", "DeveloperToolsMenu", "Developer Tools"));

	bool IsApplicationBackground()
	{
		return !FPlatformApplicationMisc::IsThisApplicationForeground() && (FPlatformTime::Seconds() - FSlateApplication::Get().GetLastUserInteractionTime()) > IDLE_INPUT_SECONDS;
	}

	void AdaptiveSleep(float Seconds)
	{
		const double IdealFrameTime = 1.0 / IDEAL_FRAMERATE;
		if (Seconds > IdealFrameTime)
		{
			// While in background, pump message at ideal frame time and get out of background as soon as input is received
			const double WakeupTime = FPlatformTime::Seconds() + Seconds;
			while (IsApplicationBackground() && FPlatformTime::Seconds() < WakeupTime)
			{
				FSlateApplication::Get().PumpMessages();
				FPlatformProcess::Sleep((float)FMath::Clamp(WakeupTime - FPlatformTime::Seconds(), 0.0, IdealFrameTime));
			}
		}
		else
		{
			FPlatformProcess::Sleep(Seconds);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool CheckSessionBrowserSingleInstance()
{
#if PLATFORM_WINDOWS
	// Create a named event that other processes can detect.
	// It allows only a single instance of Unreal Insights (Browser Mode).
	HANDLE SessionBrowserEvent = CreateEvent(NULL, true, false, TEXT("Local\\UnrealInsightsBrowser"));
	if (SessionBrowserEvent == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// Another Session Browser process is already running.

		if (SessionBrowserEvent != NULL)
		{
			CloseHandle(SessionBrowserEvent);
		}

		// Activate the respective window.
		HWND Window = FindWindowW(0, L"Unreal Insights Session Browser");
		if (Window)
		{
			ShowWindow(Window, SW_SHOW);
			SetForegroundWindow(Window);

			FLASHWINFO FlashInfo;
			FlashInfo.cbSize = sizeof(FLASHWINFO);
			FlashInfo.hwnd = Window;
			FlashInfo.dwFlags = FLASHW_ALL;
			FlashInfo.uCount = 3;
			FlashInfo.dwTimeout = 0;
			FlashWindowEx(&FlashInfo);
		}

		return false;
	}
#endif // PLATFORM_WINDOWS

#if PLATFORM_UNIX
	int FileHandle = open("/var/run/UnrealInsightsBrowser.pid", O_CREAT | O_RDWR, 0666);
	int Ret = flock(FileHandle, LOCK_EX | LOCK_NB);
	if (Ret && EWOULDBLOCK == errno)
	{
		// Another Session Browser process is already running.

		// Activate the respective window.
		//TODO: "wmctrl -a Insights"

		return false;
	}
#endif

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FUserInterfaceCommand::Run()
{
	const uint32 MaxPath = FPlatformMisc::GetMaxPathLength();
	TCHAR* TraceFile = new TCHAR[MaxPath + 1];
	TraceFile[0] = 0;
	bool bOpenTraceFile = false;

	// Only a single instance of Session Browser window/process is allowed.
	{
		bool bBrowserMode = true;

		if (bBrowserMode)
		{
			bBrowserMode = FCString::Strifind(FCommandLine::Get(), TEXT("-OpenTraceId=")) == nullptr;
		}
		if (bBrowserMode)
		{
			bOpenTraceFile = GetTraceFileFromCmdLine(TraceFile, MaxPath);
			bBrowserMode = !bOpenTraceFile;
		}

		if (bBrowserMode && !CheckSessionBrowserSingleInstance())
		{
			return;
		}
	}

	//FCoreStyle::ResetToDefault();

	// Crank up a normal Slate application using the platform's standalone renderer.
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Load required modules.
	FModuleManager::Get().LoadModuleChecked("TraceInsights");

	// Load plug-ins.
	// @todo: allow for better plug-in support in standalone Slate applications
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default);

	// Load optional modules.
	if (FModuleManager::Get().ModuleExists(TEXT("SettingsEditor")))
	{
		FModuleManager::Get().LoadModule("SettingsEditor");
	}

	InitializeSlateApplication(bOpenTraceFile, TraceFile);

	delete[] TraceFile;
	TraceFile = nullptr;

	// Initialize source code access.
	// Load the source code access module.
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>(FName("SourceCodeAccess"));

	// Manually load in the source code access plugins, as standalone programs don't currently support plugins.
#if PLATFORM_MAC
	IModuleInterface& XCodeSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>(FName("XCodeSourceCodeAccess"));
	SourceCodeAccessModule.SetAccessor(FName("XCodeSourceCodeAccess"));
#elif PLATFORM_WINDOWS
	IModuleInterface& VisualStudioSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>(FName("VisualStudioSourceCodeAccess"));
	SourceCodeAccessModule.SetAccessor(FName("VisualStudioSourceCodeAccess"));
#endif

#if WITH_SHARED_POINTER_TESTS
	SharedPointerTesting::TestSharedPointer<ESPMode::NotThreadSafe>();
	SharedPointerTesting::TestSharedPointer<ESPMode::ThreadSafe>();
#endif

	// Enter main loop.
	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / IDEAL_FRAMERATE;
	const float BackgroundFrameTime = 1.0f / BACKGROUND_FRAMERATE;

	while (!IsEngineExitRequested())
	{
		// Save the state of the tabs here rather than after close of application (the tabs are undesirably saved out with ClosedTab state on application close).
		//UserInterfaceCommand::UserConfiguredNewLayout = FGlobalTabmanager::Get()->PersistLayout();

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FTSTicker::GetCoreTicker().Tick(static_cast<float>(DeltaTime));

		// Throttle frame rate.
		const float FrameTime = UserInterfaceCommand::IsApplicationBackground() ? BackgroundFrameTime : IdealFrameTime;
		UserInterfaceCommand::AdaptiveSleep(FMath::Max<float>(0.0f, FrameTime - static_cast<float>(FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime =  CurrentTime - LastTime;
		LastTime = CurrentTime;

		FStats::AdvanceFrame(false);

		FCoreDelegates::OnEndFrame.Broadcast();
		GLog->FlushThreadedLogs(); //im: ???

		GFrameCounter++;
	}

	//im: ??? FCoreDelegates::OnExit.Broadcast();

	ShutdownSlateApplication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FUserInterfaceCommand::InitializeSlateApplication(bool bOpenTraceFile, const TCHAR* TraceFile)
{
	FSlateApplication::InitHighDPI(true);

	//const FSlateBrush* AppIcon = new FSlateImageBrush(FPaths::EngineContentDir() / "Editor/Slate/Icons/Insights/AppIcon_24x.png", FVector2D(24.0f, 24.0f));
	//FSlateApplication::Get().SetAppIcon(AppIcon);

	// Set the application name.
	const FText ApplicationTitle = FText::Format(NSLOCTEXT("UnrealInsights", "AppTitle", "Unreal Insights {0}"), FText::FromString(TEXT(UNREAL_INSIGHTS_VERSION_STRING_EX)));
	FGlobalTabmanager::Get()->SetApplicationTitle(ApplicationTitle);

	// Load widget reflector.
	const bool bAllowDebugTools = FParse::Param(FCommandLine::Get(), TEXT("DebugTools"));
	if (bAllowDebugTools)
	{
		FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(UserInterfaceCommand::DeveloperTools);
	}

	IUnrealInsightsModule& TraceInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");

	uint32 TraceId = 0;
	FString TraceIdString;
	bool bUseTraceId = FParse::Value(FCommandLine::Get(), TEXT("-OpenTraceId="), TraceIdString);
	if (TraceIdString.StartsWith(TEXT("0x")))
	{
		TCHAR* End;
		TraceId = FCString::Strtoi(*TraceIdString + 2, &End, 16);
	}
	else
	{
		TCHAR* End;
		TraceId = FCString::Strtoi(*TraceIdString, &End, 10);
	}

	FString StoreHost = TEXT("127.0.0.1");
	uint32 StorePort = 0;
	bool bUseCustomStoreAddress = false;

	if (FParse::Value(FCommandLine::Get(), TEXT("-Store="), StoreHost, true))
	{
		int32 Index = INDEX_NONE;
		if (StoreHost.FindChar(TEXT(':'), Index))
		{
			StorePort = FCString::Atoi(*StoreHost.RightChop(Index + 1));
			StoreHost.LeftInline(Index);
		}
		bUseCustomStoreAddress = true;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("-StoreHost="), StoreHost, true))
	{
		bUseCustomStoreAddress = true;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("-StorePort="), StorePort))
	{
		bUseCustomStoreAddress = true;
	}

	// This parameter will cause the application to close when analysis fails to start or completes successfully.
	const bool bAutoQuit = FParse::Param(FCommandLine::Get(), TEXT("AutoQuit"));

	const bool bInitializeTesting = FParse::Param(FCommandLine::Get(), TEXT("InsightsTest"));
	if (bInitializeTesting)
	{
		const bool bInitAutomationModules = true;
		TraceInsightsModule.InitializeTesting(bInitAutomationModules, bAutoQuit);
	}

	if (bUseTraceId || bOpenTraceFile) // viewer mode
	{
		FString Cmd;
		bool bExecuteCommand = false;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ExecOnAnalysisCompleteCmd="), Cmd, false))
		{
			bExecuteCommand = true;
		}
		if (bExecuteCommand)
		{
			TraceInsightsModule.ScheduleCommand(Cmd);
		}

		const bool bNoUI = FParse::Param(FCommandLine::Get(), TEXT("NoUI"));
		if (!bNoUI)
		{
			TraceInsightsModule.CreateSessionViewer(bAllowDebugTools);
		}

		if (bUseTraceId)
		{
			TraceInsightsModule.ConnectToStore(*StoreHost, StorePort);
			TraceInsightsModule.StartAnalysisForTrace(TraceId, bAutoQuit);
		}
		else
		{
			TraceInsightsModule.StartAnalysisForTraceFile(TraceFile, bAutoQuit);
		}
	}
	else // browser mode
	{
		FString Cmd;
		bool bExecuteCommand = false;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ExecBrowserAutomationTest="), Cmd, false))
		{
			bExecuteCommand = true;
		}

		if (bUseCustomStoreAddress)
		{
			TraceInsightsModule.ConnectToStore(*StoreHost, StorePort);
		}
		else
		{
			TraceInsightsModule.CreateDefaultStore();
		}

		FCreateSessionBrowserParams Params;
		Params.bAllowDebugTools = bAllowDebugTools;
		Params.bInitializeTesting = bInitializeTesting;
		Params.bStartProcessWithStompMalloc = FParse::Param(FCommandLine::Get(), TEXT("stompmalloc"));
		TraceInsightsModule.CreateSessionBrowser(Params);

		if (bExecuteCommand)
		{
			TraceInsightsModule.RunAutomationTest(Cmd);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FUserInterfaceCommand::ShutdownSlateApplication()
{
	IUnrealInsightsModule& TraceInsightsModule = FModuleManager::LoadModuleChecked<IUnrealInsightsModule>("TraceInsights");
	TraceInsightsModule.ShutdownUserInterface();

	// Shut down application.
	FSlateApplication::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FUserInterfaceCommand::GetTraceFileFromCmdLine(TCHAR* OutTraceFile, uint32 MaxPath)
{
	// Try getting the trace file from the -OpenTraceFile= paramter first.
	bool bUseTraceFile = FParse::Value(FCommandLine::Get(), TEXT("-OpenTraceFile="), OutTraceFile, MaxPath, true);

	if (bUseTraceFile)
	{
		return true;
	}

	// Support opening a trace file by double clicking a .utrace file.
	// In this case, the app will receive as the first parameter a utrace file path.

	const TCHAR* CmdLine = FCommandLine::Get();
	bool HasToken = FParse::Token(CmdLine, OutTraceFile, MaxPath, false);

	if (HasToken)
	{
		FString Token = OutTraceFile;
		if (Token.EndsWith(TEXT(".utrace")))
		{
			bUseTraceFile = true;
		}
	}

	return bUseTraceFile;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
