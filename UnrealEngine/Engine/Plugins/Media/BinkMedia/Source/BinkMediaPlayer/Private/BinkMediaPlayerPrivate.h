// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 
#pragma once

#include "binkplugin.h"

#include "HAL/PlatformFileManager.h"
#include "Modules/ModuleManager.h"
#include "Rendering/RenderingCommon.h"
#include "TickableObjectRenderThread.h"
#include "RHI.h"
#include "RHIUtilities.h"
#include "RHIDefinitions.h"

#if PLATFORM_ANDROID
#include <android/log.h>
#include <android_native_app_glue.h>
#include "Android/AndroidPlatformFile.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif

#include "BinkMovieStreamer.h"
#include "BinkMoviePlayerSettings.h"

#include "binkplugin_ue4.h"

#include "AudioDeviceManager.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"

extern BINKMEDIAPLAYER_API unsigned bink_gpu_api;
extern BINKMEDIAPLAYER_API unsigned bink_gpu_api_hdr;
extern BINKMEDIAPLAYER_API EPixelFormat bink_force_pixel_format;
extern BINKMEDIAPLAYER_API FString BinkUE4CookOnTheFlyPath(FString path, const TCHAR *filename);
extern BINKMEDIAPLAYER_API TArray< FTexture2DRHIRef > BinkActiveTextureRefs;

static int GetNumSpeakers() 
{
	if (!GEngine || !GEngine->GetAudioDeviceManager())
	{
		return 2;
	}
	FAudioDevice *dev = FAudioDevice::GetMainAudioDevice().GetAudioDevice();
	if (dev && dev->IsAudioMixerEnabled()) {
		Audio::FMixerDevice *mix = static_cast<Audio::FMixerDevice*>(dev);
		if (mix) {
			return mix->GetNumDeviceChannels();
		}
	}
	return 2; // default case... unknown just assume 2 speakers stereo setup.
}

DECLARE_LOG_CATEGORY_EXTERN(LogBink, Log, All);



