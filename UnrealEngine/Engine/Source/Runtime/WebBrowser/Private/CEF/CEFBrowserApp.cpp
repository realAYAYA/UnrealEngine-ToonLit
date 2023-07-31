// Copyright Epic Games, Inc. All Rights Reserved.

#include "CEF/CEFBrowserApp.h"
#include "HAL/IConsoleManager.h"

#if WITH_CEF3
#include "WebBrowserLog.h"

//#define DEBUG_CEFMESSAGELOOP_FRAMERATE 1 // uncomment this to have debug spew about the FPS we call the CefDoMessageLoopWork function

DEFINE_LOG_CATEGORY(LogCEFBrowser);

static bool bCEFGPUAcceleration = true;
static FAutoConsoleVariableRef CVarCEFGPUAcceleration(
	TEXT("r.CEFGPUAcceleration"),
	bCEFGPUAcceleration,
	TEXT("Enables GPU acceleration in CEF\n"),
	ECVF_Default);

FCEFBrowserApp::FCEFBrowserApp()
	: MessagePumpCountdown(0)
{
}

void FCEFBrowserApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine)
{
}

void FCEFBrowserApp::OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine)
{
	if (bCEFGPUAcceleration)
	{
		UE_LOG(LogCEFBrowser, Log, TEXT("CEF GPU acceleration enabled"));
		CommandLine->AppendSwitch("enable-gpu");
		CommandLine->AppendSwitch("enable-gpu-compositing");
	}
	else
	{
		UE_LOG(LogCEFBrowser, Log, TEXT("CEF GPU acceleration disabled"));
		CommandLine->AppendSwitch("disable-gpu");
		CommandLine->AppendSwitch("disable-gpu-compositing");
	}

#if PLATFORM_LINUX
	CommandLine->AppendSwitchWithValue("ozone-platform", "headless");
	CommandLine->AppendSwitchWithValue("use-gl", "angle");
	CommandLine->AppendSwitchWithValue("use-angle", "vulkan");
	CommandLine->AppendSwitch("use-vulkan");
#endif

	CommandLine->AppendSwitch("enable-begin-frame-scheduling");
	CommandLine->AppendSwitch("disable-pinch"); // the web pages we have don't expect zoom to work right now so disable touchpad pinch zoom
	CommandLine->AppendSwitch("disable-gpu-shader-disk-cache"); // Don't create a "GPUCache" directory when cache-path is unspecified.
#if PLATFORM_MAC
	CommandLine->AppendSwitch("use-mock-keychain"); // Disable the toolchain prompt on macOS.
#endif

	// Uncomment these to lines to create a FULL network log from chrome, which can then be inspected using https://netlog-viewer.appspot.com/
	//CommandLine->AppendSwitchWithValue("log-net-log", "c:\\temp\\cef_net_log.json");
	//CommandLine->AppendSwitchWithValue("net-log-capture-mode", "IncludeCookiesAndCredentials");
}


void FCEFBrowserApp::OnScheduleMessagePumpWork(int64 delay_ms)
{
	FScopeLock Lock(&MessagePumpCountdownCS);

	// As per CEF documentation, if delay_ms is <= 0, then the call to CefDoMessageLoopWork should happen reasonably soon.  If delay_ms is > 0, then the call
	//  to CefDoMessageLoopWork should be scheduled to happen after the specified delay and any currently pending scheduled call should be canceled.
	if(delay_ms < 0)
	{
		delay_ms = 0;
	}
	MessagePumpCountdown = delay_ms;
}

bool FCEFBrowserApp::TickMessagePump(float DeltaTime, bool bForce)
{
	bool bPump = false;
	{
		FScopeLock Lock(&MessagePumpCountdownCS);
		
		// count down in order to call message pump
		if (MessagePumpCountdown >= 0)
		{
			MessagePumpCountdown -= (DeltaTime * 1000);
			if (MessagePumpCountdown <= 0)
			{
				bPump = true;
			}
			
			if (bPump || bForce)
			{
				// -1 indicates that no countdown is currently happening
				MessagePumpCountdown = -1;
			}
		}
	}
	
#ifdef  DEBUG_CEFMESSAGELOOP_FRAMERATE
	static float SecondsFrameRate = 0;
	static int NumFrames = 0;
	SecondsFrameRate += DeltaTime;
#endif
	if (bPump || bForce)
	{
#ifdef DEBUG_CEFMESSAGELOOP_FRAMERATE
		++NumFrames;
		if (NumFrames % 100 == 0 || SecondsFrameRate > 5.0f)
		{
			UE_LOG(LogWebBrowser, Error, TEXT("CefDoMessageLoopWork call Frame Rate %0.2f"), NumFrames / SecondsFrameRate);
			SecondsFrameRate = 0;
			NumFrames = 0;
		}
#endif

		CefDoMessageLoopWork();
		return true;
	}
	return false;
}

#endif
