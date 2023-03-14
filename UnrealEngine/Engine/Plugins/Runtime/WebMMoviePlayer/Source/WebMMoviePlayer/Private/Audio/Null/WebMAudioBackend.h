// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FWebMAudioBackendNull
{
public:

	enum class EStreamState
	{
		NewMovie,
		ResumeAfterPause
	};

	bool InitializePlatform() { return true;  }
	void ShutdownPlatform() {}
	bool StartStreaming(int32 SampleRate, int32 NumOfChannels, EStreamState StreamState) { return true; }
	void StopStreaming() {}
	bool SendAudio(const FTimespan& Timespan, const uint8* Buffer, size_t BufferSize) { return true; }

	void Pause(bool bPause) {}
	bool IsPaused() const { return false; }
	void Tick(float DeltaTime) {}
};
