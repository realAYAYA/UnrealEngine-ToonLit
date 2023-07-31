// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace Audio
{
	bool AUDIOMIXER_API InitSoundFileIOManager();
	bool AUDIOMIXER_API ShutdownSoundFileIOManager();
	bool AUDIOMIXER_API ConvertAudioToWav(const TArray<uint8>& InAudioData, TArray<uint8>& OutWaveData);
}
