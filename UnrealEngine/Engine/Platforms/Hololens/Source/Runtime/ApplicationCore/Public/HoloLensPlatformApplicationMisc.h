// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FHoloLensPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static class FOutputDeviceError* GetErrorOutputDevice();
	static class GenericApplication* CreateApplication();
	static void PumpMessages(bool bFromMainLoop);
	static float GetDPIScaleFactorAtPoint(float X, float Y);
	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);

	static bool AnchorWindowWindowPositionTopLeft()
	{
		// UE expects mouse coordinates in screen space. HoloLens provides in client space. 
		// Also note comments in FDisplayMetrics::GetDisplayMetrics for HoloLens.
		return true;
	}
	
	static bool RequiresVirtualKeyboard()
	{
#if PLATFORM_HOLOLENS
		return true;
#endif
		return false;
	}
};

typedef FHoloLensPlatformApplicationMisc FPlatformApplicationMisc;
