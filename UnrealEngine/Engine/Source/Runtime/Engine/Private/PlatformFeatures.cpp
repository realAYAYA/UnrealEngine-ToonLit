// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "DVRStreaming.h"
#include "VideoRecordingSystem.h"

#if PLATFORM_ANDROID
extern bool AndroidThunkCpp_IsScreenCaptureDisabled();
extern void AndroidThunkCpp_DisableScreenCapture(bool bDisable);
#endif

ISaveGameSystem* IPlatformFeaturesModule::GetSaveGameSystem()
{
	static FGenericSaveGameSystem GenericSaveGame;
	return &GenericSaveGame;
}


IDVRStreamingSystem* IPlatformFeaturesModule::GetStreamingSystem()
{
	static FGenericDVRStreamingSystem GenericStreamingSystem;
	return &GenericStreamingSystem;
}

FString IPlatformFeaturesModule::GetUniqueAppId()
{
	return FString();
}

IVideoRecordingSystem* IPlatformFeaturesModule::GetVideoRecordingSystem()
{
	static FGenericVideoRecordingSystem GenericVideoRecordingSystem;
	return &GenericVideoRecordingSystem;
}

void IPlatformFeaturesModule::SetScreenshotEnableState(bool bEnabled)
{
#if PLATFORM_ANDROID
	if (AndroidThunkCpp_IsScreenCaptureDisabled() != !bEnabled)
	{
		AndroidThunkCpp_DisableScreenCapture(!bEnabled);
	}
#endif
}