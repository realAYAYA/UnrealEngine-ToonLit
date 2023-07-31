// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "SDL.h"
#include "SDL_audio.h"
THIRD_PARTY_INCLUDES_END

class FWebMAudioBackendSDL
{
public:

	enum class EStreamState
	{
		NewMovie,
		ResumeAfterPause
	};

	FWebMAudioBackendSDL();
	virtual ~FWebMAudioBackendSDL();

	//~ IWebMAudioBackend interface
	virtual bool InitializePlatform();
	virtual void ShutdownPlatform();
	virtual bool StartStreaming(int32 SampleRate, int32 NumOfChannels, EStreamState StreamState);
	virtual void StopStreaming();
	virtual bool SendAudio(const FTimespan& Timespan, const uint8* Buffer, size_t BufferSize);

	virtual void Pause(bool bPause);
	virtual bool IsPaused() const;
	virtual void Tick(float DeltaTime);
	virtual FString GetDefaultDeviceName();

protected:
	SDL_AudioDeviceID AudioDevice;
	bool bSDLInitialized;

	/** Whether we're pausing playing back sound. */
	bool bPaused;
};
