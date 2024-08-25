// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace Audio::SoundFileUtils
{
	bool AUDIOMIXER_API InitSoundFileIOManager();
	bool AUDIOMIXER_API ShutdownSoundFileIOManager();
	uint32 AUDIOMIXER_API GetNumSamples(const TArray<uint8>& InAudioData);
	bool AUDIOMIXER_API ConvertAudioToWav(const TArray<uint8>& InAudioData, TArray<uint8>& OutWaveData);
}
