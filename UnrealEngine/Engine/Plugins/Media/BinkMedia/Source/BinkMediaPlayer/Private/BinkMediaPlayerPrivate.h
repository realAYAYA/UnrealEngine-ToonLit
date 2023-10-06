// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 
#pragma once

#include "Engine/Engine.h"
#include "Rendering/SlateRenderer.h"
#include "AudioMixerDevice.h"

#if PLATFORM_ANDROID
#include <android/log.h>
#include <android_native_app_glue.h>
#include "Android/AndroidPlatformFile.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif




enum EPixelFormat : uint8;

extern BINKMEDIAPLAYER_API unsigned bink_gpu_api;
extern BINKMEDIAPLAYER_API unsigned bink_gpu_api_hdr;
extern BINKMEDIAPLAYER_API EPixelFormat bink_force_pixel_format;
extern BINKMEDIAPLAYER_API FString BinkUE4CookOnTheFlyPath(FString path, const TCHAR *filename);
extern BINKMEDIAPLAYER_API TArray< FTexture2DRHIRef > BinkActiveTextureRefs;
extern BINKMEDIAPLAYER_API bool BinkInitialize();

static int GetNumSpeakers() 
{
	if (!GEngine || !GEngine->GetAudioDeviceManager())
	{
		return 2;
	}
	FAudioDevice *dev = FAudioDevice::GetMainAudioDevice().GetAudioDevice();
	if (dev) {
		Audio::FMixerDevice *mix = static_cast<Audio::FMixerDevice*>(dev);
		if (mix) {
			return mix->GetNumDeviceChannels();
		}
	}
	return 2; // default case... unknown just assume 2 speakers stereo setup.
}

DECLARE_LOG_CATEGORY_EXTERN(LogBink, Log, All);



