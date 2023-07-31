// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformFeatures.h"
#include "WmfPrivate.h"
#include "WindowsVideoRecordingSystem.h"
#include "Misc/CommandLine.h"

IMPLEMENT_MODULE(FWindowsPlatformFeaturesModule, WindowsPlatformFeatures);

WINDOWSPLATFORMFEATURES_START

FWindowsPlatformFeaturesModule::FWindowsPlatformFeaturesModule()
{
}

IVideoRecordingSystem* FWindowsPlatformFeaturesModule::GetVideoRecordingSystem()
{
	static FWindowsVideoRecordingSystem VideoRecordingSystem;
	return &VideoRecordingSystem;
}

void FWindowsPlatformFeaturesModule::StartupModule()
{
	FModuleManager::Get().LoadModule(TEXT("GameplayMediaEncoder"));
}

WINDOWSPLATFORMFEATURES_END
