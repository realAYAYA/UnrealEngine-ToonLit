// Copyright Epic Games, Inc. All Rights Reserved.

// Note: This application is still in very early development

// This translation unit is windows specific

#include "ChaosVisualDebuggerWindows.h"
#include "ChaosVisualDebuggerMain.h"
#include "HAL/ExceptionHandling.h"
#include "Windows/WindowsHWrapper.h"
#include "LaunchEngineLoop.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceError.h"

/**
 * The main application entry point for the Visual debugger on Windows platforms.
 *
 * @param hInInstance Handle to the current instance of the application.
 * @param hPrevInstance Handle to the previous instance of the application (always NULL).
 * @param lpCmdLine Command line for the application.
 * @param nShowCmd Specifies how the window is to be shown.
 * @return Application's exit value.
 */

int WINAPI WinMain(_In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	hInstance = hInInstance;

	const TCHAR* CmdLine = ::GetCommandLineW();
	CmdLine = FCommandLine::RemoveExeName(CmdLine);


	int32 ErrorLevel = 0;

#if UE_BUILD_DEBUG
	if (!GAlwaysReportCrash)
#else
	if (FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash)
#endif
	{
		ErrorLevel = ChaosVisualDebuggerMain(CmdLine);
	}
	else
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif
		{
			GIsGuarded = 1;
			ErrorLevel = ChaosVisualDebuggerMain(CmdLine);
			GIsGuarded = 0;
		}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except (ReportCrash(GetExceptionInformation()))
		{
			ErrorLevel = 1;
			GError->HandleError();
			FPlatformMisc::RequestExit(true);
		}
#endif
	}

	FEngineLoop::AppExit();

	return ErrorLevel;
}
