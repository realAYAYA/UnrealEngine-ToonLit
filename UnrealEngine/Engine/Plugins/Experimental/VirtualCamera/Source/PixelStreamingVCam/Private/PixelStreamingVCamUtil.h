// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "PixelStreamingVCamLogging.h"

class FPixelStreamingVCamUtil
{
public:
	static bool GetSignallingServerBinaryAbsPath(FString& OutPath)
	{
		TSharedPtr<IPlugin> VCamPlugin = IPluginManager::Get().FindPlugin("PixelStreamingVCam");

		if(!VCamPlugin.IsValid())
		{
			return false;
		}

		FString PluginContentDir = FPaths::ConvertRelativePathToFull(VCamPlugin->GetContentDir());

#if PLATFORM_WINDOWS
		OutPath = PluginContentDir / TEXT("SignallingWebServer") / TEXT("cirrus.exe");
#elif PLATFORM_LINUX
		OutPath = PluginContentDir / TEXT("SignallingWebServer") / TEXT("cirrus");
#else
		UE_LOG(LogPixelStreamingVCam, Log, TEXT("Unsupported platform for Pixel Streaming VCam plugin - only Windows and Linux are supported."))
		return false;
#endif

		// Check if the out path we set is actually valid
		return FPaths::FileExists(OutPath);
	}
};
