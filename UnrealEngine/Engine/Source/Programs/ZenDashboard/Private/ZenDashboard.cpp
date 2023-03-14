// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenDashboard.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Framework/Application/SlateApplication.h"
#include "StandaloneRenderer.h"
#include "ZenDashboardStyle.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Windows/WindowsHWrapper.h"
#include "Misc/MonitoredProcess.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/CompilationResult.h"
#include "Misc/MessageDialog.h"
#include "SZenCacheStatistics.h"
#include "ZenServerInterface.h"

#if PLATFORM_WINDOWS
#include "Runtime/Launch/Resources/Windows/Resource.h"
#include "Windows/WindowsApplication.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/PreWindowsApi.h"
#include <shellapi.h>
#include "Windows/PostWindowsApi.h"
#endif

#define LOCTEXT_NAMESPACE "ZenDashboard"

IMPLEMENT_APPLICATION(ZenDashboard, "ZenDashboard");

static void OnRequestExit()
{
	RequestEngineExit(TEXT("ZenDashboard Closed"));
}

#if PLATFORM_WINDOWS
#define WM_TRAYICON (WM_USER + 1000)
#endif

class FZenDashboardApp
#if PLATFORM_WINDOWS
	: IWindowsMessageHandler
#endif
{
private:
	FCriticalSection CriticalSection;
	FSlateApplication& Slate;
	TSharedPtr<SWindow> Window;
	TSharedPtr<SNotificationItem> CompileNotification;
	TArray<FSimpleDelegate> MainThreadTasks;
	bool bRequestCancel;
	bool bDisableLimit;
	bool bHasReinstancingProcess;
	bool bWarnOnRestart;
	FDateTime LastPatchTime;
	FDateTime NextPatchStartTime;
	UE::Zen::FScopeZenService ZenService;
	UE::Zen::FZenStats ZenStats;


	bool ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam, int32& OutResult) override
	{
		if (msg == WM_TRAYICON && LOWORD(lParam) == WM_LBUTTONDBLCLK)
		{
			Window->ShowWindow();
			return true;
		}
		return false;
	}

public:
	FZenDashboardApp(FSlateApplication& InSlate)
		: Slate(InSlate)
		, bRequestCancel(false)
		, bDisableLimit(false)
		, bHasReinstancingProcess(false)
		, bWarnOnRestart(false)
		, LastPatchTime(FDateTime::MinValue())
		, NextPatchStartTime(FDateTime::MinValue())
	{
	}

	virtual ~FZenDashboardApp()
	{
	}

	void Run()
	{
		Window =
			SNew(SWindow)
			.Title(GetWindowTitle())
			.ClientSize(FVector2D(384.0f, 128.0f))
			.ActivationPolicy(EWindowActivationPolicy::Always)
			.SizingRule(ESizingRule::Autosized)
			.IsTopmostWindow(true)
			.FocusWhenFirstShown(false)
			.SupportsMaximize(false)
			.SupportsMinimize(true)
			.HasCloseButton(true)
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Top)
					[
						SNew(SZenCacheStatisticsDialog)
					]
				]
			];

		/*Window->RegisterActiveTimer(0.2f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
			{
				ZenService.GetInstance().GetStats(ZenStats);
				return EActiveTimerReturnType::Continue;
			}));*/

		// Add the window without showing it
		Slate.AddWindow(Window.ToSharedRef(), true);

		// Show the window without stealling focus
/*		if (!FParse::Param(FCommandLine::Get(), TEXT("Hidden")))
		{
			HWND ForegroundWindow = GetForegroundWindow();
			if (ForegroundWindow != nullptr)
			{
				::SetWindowPos((HWND)Window->GetNativeWindow()->GetOSWindowHandle(), ForegroundWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			Window->ShowWindow();
		}
		*/
		// Setting focus seems to have to happen after the Window has been added
		Slate.ClearKeyboardFocus(EFocusCause::Cleared);

		// 
#if PLATFORM_WINDOWS
		((FWindowsApplication*)Slate.Get().GetPlatformApplication().Get())->AddMessageHandler(*this);

		NOTIFYICONDATAW NotifyIconData;
		memset(&NotifyIconData, 0, sizeof(NotifyIconData));
		NotifyIconData.cbSize = sizeof(NotifyIconData);
		NotifyIconData.hWnd = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
		NotifyIconData.uID = 0;
		NotifyIconData.uCallbackMessage = WM_USER + 1000;
		NotifyIconData.uFlags = NIF_ICON | NIF_MESSAGE;
		NotifyIconData.hIcon = ::LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDICON_UEGame));
		Shell_NotifyIcon(NIM_ADD, &NotifyIconData);
#endif

		// loop until the app is ready to quit
		while (!IsEngineExitRequested())
		{
			BeginExitIfRequested();

			Slate.PumpMessages();
			Slate.Tick();

			FPlatformProcess::Sleep(1.0f / 30.0f);

			// Execute all the main thread tasks
			FScopeLock Lock(&CriticalSection);
			for (FSimpleDelegate& MainThreadTask : MainThreadTasks)
			{
				MainThreadTask.Execute();
			}
			MainThreadTasks.Empty();
		}

#if PLATFORM_WINDOWS
		memset(&NotifyIconData, 0, sizeof(NotifyIconData));
		NotifyIconData.cbSize = sizeof(NotifyIconData);
		NotifyIconData.uID = 0;
		NotifyIconData.hWnd = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
		Shell_NotifyIcon(NIM_DELETE, &NotifyIconData);
#endif

		// Make sure the window is hidden, because it might take a while for the background thread to finish.
		Window->HideWindow();
	}

private:
	FText GetWindowTitle()
	{
		return LOCTEXT("WindowTitle", "Zen Dashboard");
	}

	void BringToFrontAsync()
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateRaw(this, &FZenDashboardApp::BringToFront));
	}

	void BringToFront()
	{
		HWND WindowHandle = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
		if (IsIconic(WindowHandle))
		{
			ShowWindow(WindowHandle, SW_RESTORE);
		}
		::SetWindowPos(WindowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		::SetWindowPos(WindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	void SetVisibleAsync(bool bVisible)
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateLambda([this, bVisible](){ SetVisible(bVisible); }));
	}

	void SetVisible(bool bVisible)
	{
		if (bVisible)
		{
			if (!Window->IsVisible())
			{
				Window->ShowWindow();
			}
		}
		else
		{
			if (Window->IsVisible())
			{
				Window->HideWindow();
			}
		}
	}

	void ShowConsole()
	{
		SetVisible(true);
		BringToFront();
	}
};

bool ZenDashboardMain(const TCHAR* CmdLine)
{
	// start up the main loop
	GEngineLoop.PreInit(CmdLine);
	check(GConfig && GConfig->IsReadyForUse());

	// Initialize high DPI mode
	FSlateApplication::InitHighDPI(true);

	{
		// Create the platform slate application (what FSlateApplication::Get() returns)
		TSharedRef<FSlateApplication> Slate = FSlateApplication::Create(MakeShareable(FPlatformApplicationMisc::CreateApplication()));

		{
			// Initialize renderer
			TSharedRef<FSlateRenderer> SlateRenderer = GetStandardStandaloneRenderer();

			// Try to initialize the renderer. It's possible that we launched when the driver crashed so try a few times before giving up.
			bool bRendererInitialized = Slate->InitializeRenderer(SlateRenderer, true);
			if (!bRendererInitialized)
			{
				// Close down the Slate application
				FSlateApplication::Shutdown();
				return false;
			}

			// Set the normal UE IsEngineExitRequested() when outer frame is closed
			Slate->SetExitRequestedHandler(FSimpleDelegate::CreateStatic(&OnRequestExit));

			// Prepare the custom Slate styles
			FZenDashboardStyle::Initialize();

			// Set the icon
			FAppStyle::SetAppStyleSet(FZenDashboardStyle::Get());

			// Run the inner application loop
			FZenDashboardApp App(Slate.Get());
			App.Run();

			// Clean up the custom styles
			FZenDashboardStyle::Shutdown();
		}

		// Close down the Slate application
		FSlateApplication::Shutdown();
	}

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();
	return true;
}

int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	hInstance = hCurrInstance;
	return ZenDashboardMain(GetCommandLineW())? 0 : 1;
}

#undef LOCTEXT_NAMESPACE
