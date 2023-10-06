// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct FLinuxPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static APPLICATIONCORE_API void PreInit();
	static APPLICATIONCORE_API void Init();
	static APPLICATIONCORE_API bool InitSDL();
	static APPLICATIONCORE_API void TearDown();
	static APPLICATIONCORE_API void LoadPreInitModules();
	static APPLICATIONCORE_API void LoadStartupModules();
	static APPLICATIONCORE_API uint32 WindowStyle();
	static APPLICATIONCORE_API class FOutputDeviceConsole* CreateConsoleOutputDevice();
	static APPLICATIONCORE_API class FOutputDeviceError* GetErrorOutputDevice();
	static APPLICATIONCORE_API class FFeedbackContext* GetFeedbackContext();
	static APPLICATIONCORE_API class GenericApplication* CreateApplication();
	static APPLICATIONCORE_API bool IsThisApplicationForeground();
	static APPLICATIONCORE_API void PumpMessages(bool bFromMainLoop);
	static APPLICATIONCORE_API bool IsScreensaverEnabled();
	static APPLICATIONCORE_API bool ControlScreensaver(EScreenSaverAction Action);
	static APPLICATIONCORE_API float GetDPIScaleFactorAtPoint(float X, float Y);
	static APPLICATIONCORE_API void ClipboardCopy(const TCHAR* Str);
	static APPLICATIONCORE_API void ClipboardPaste(class FString& Dest);
	static bool FullscreenSameAsWindowedFullscreen() { return true; }

	// Unix specific
	static APPLICATIONCORE_API void EarlyUnixInitialization(class FString& OutCommandLine);
	static bool ShouldIncreaseProcessLimits() { return true; }

	// Linux specific
	/** Informs ApplicationCore that it needs to create Vulkan-compatible windows (mutually exclusive with OpenGL) */
	static APPLICATIONCORE_API void UsingVulkan();
	/** Informs ApplicationCore that it needs to create OpenGL-compatible windows (mutually exclusive with Vulkan) */
	static APPLICATIONCORE_API void UsingOpenGL();
};

typedef FLinuxPlatformApplicationMisc FPlatformApplicationMisc;
