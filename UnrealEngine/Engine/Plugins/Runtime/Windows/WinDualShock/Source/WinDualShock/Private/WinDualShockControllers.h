// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WinDualShock.h"
#include <pad.h>
#include <pad_audio.h>

class FWinDualShockControllers : public FPlatformControllers
{
public:

	FWinDualShockControllers()
		: FPlatformControllers()
	{
	}

	virtual ~FWinDualShockControllers()
	{
	}

	void SetAudioGain(float InPadSpeakerGain, float InHeadphonesGain, float InMicrophoneGain, float InOutputGain)
	{
		PadSpeakerGain = InPadSpeakerGain;
		HeadphonesGain = InHeadphonesGain;
		MicrophoneGain = InMicrophoneGain;
		OutputGain = InOutputGain;
		for (int32 UserIndex = 0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; UserIndex++)
		{
			bGainChanged[UserIndex] = true;
		}
	}

	float GetOutputGain()
	{
		return OutputGain;
	}

	bool GetSupportsAudio(int32 UserIndex) const
	{
		return bSupportsAudio[UserIndex];
	}

private:
	float OutputGain = 1.0f;
};
