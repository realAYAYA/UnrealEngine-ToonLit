// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FLinuxPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static void PreInit();
	static void Init();
	static bool InitSDL();
	static void TearDown();
	static void LoadPreInitModules();
	static void LoadStartupModules();
	static uint32 WindowStyle();
	static class FOutputDeviceConsole* CreateConsoleOutputDevice();
	static class FOutputDeviceError* GetErrorOutputDevice();
	static class FFeedbackContext* GetFeedbackContext();
	static class GenericApplication* CreateApplication();
	static bool IsThisApplicationForeground();
	static void PumpMessages(bool bFromMainLoop);
	static bool IsScreensaverEnabled();
	static bool ControlScreensaver(EScreenSaverAction Action);
	static float GetDPIScaleFactorAtPoint(float X, float Y);
	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);
	static bool FullscreenSameAsWindowedFullscreen() { return true; }

	// Unix specific
	static void EarlyUnixInitialization(class FString& OutCommandLine);
	static bool ShouldIncreaseProcessLimits() { return true; }

	// Linux specific
	/** Informs ApplicationCore that it needs to create Vulkan-compatible windows (mutually exclusive with OpenGL) */
	static void UsingVulkan();
	/** Informs ApplicationCore that it needs to create OpenGL-compatible windows (mutually exclusive with Vulkan) */
	static void UsingOpenGL();
};

typedef FLinuxPlatformApplicationMisc FPlatformApplicationMisc;
