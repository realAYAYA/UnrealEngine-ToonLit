#pragma once

#ifdef __UNREAL__
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#if !__UNREAL__ || (PLATFORM_HOLOLENS || PLATFORM_WINDOWS)
#include <Windows.h>
#include <spatialaudioclient.h>
#include <stdint.h>
#endif

#ifdef __UNREAL__
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#ifdef SPATIAL_AUDIO_EXPORTS
#define SPATIAL_AUDIO_INTEROP_API __declspec(dllexport)
#else
#define SPATIAL_AUDIO_INTEROP_API __declspec(dllimport)
#endif

class SpatialAudioClientRenderer;

/** Singleton that performs spatial audio rendering */
class SPATIAL_AUDIO_INTEROP_API SpatialAudioClient
{
public:
	static SpatialAudioClient* CreateSpatialAudioClient()
	{
		return new SpatialAudioClient();
	}

	void Release();

	// Starts the spatial audio client rendering
	bool Start(UINT32 InNumSources, UINT32 InSampleRate);

	// Stops the spatial audio client rendering
	bool Stop();

	// Returns whether or not the spatial audio client is active
	bool IsActive();

	// Returns the number of dynamic objects supported by the audio client renderer
	UINT32 GetMaxDynamicObjects() const;

	// Activates and returns a dynamic object handle
	ISpatialAudioObject* ActivatDynamicSpatialAudioObject();

	// Begins the update loop
	bool BeginUpdating(UINT32* OutAvailableDynamicObjectCount, UINT32* OutFrameCountPerBuffer);

	// Ends the update loop
	bool EndUpdating();

	// Pause the thread until buffer completion event
	bool WaitTillBufferCompletionEvent();

private:
	SpatialAudioClient();
	~SpatialAudioClient();

	int32_t sacId;
};
