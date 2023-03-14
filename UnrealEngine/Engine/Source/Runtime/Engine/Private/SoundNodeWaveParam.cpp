// Copyright Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeWaveParam.h"
#include "Audio.h"
#include "ActiveSound.h"
#include "Sound/SoundWave.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundNodeWaveParam)

/*-----------------------------------------------------------------------------
	USoundNodeWaveParam implementation
-----------------------------------------------------------------------------*/
USoundNodeWaveParam::USoundNodeWaveParam(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float USoundNodeWaveParam::GetDuration()
{
	// Since we can't know how long this node will be we say it is indefinitely looping
	return INDEFINITELY_LOOPING_DURATION;
}

void USoundNodeWaveParam::ParseNodes(FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances)
{
	FAudioParameter ParamValue;
	ActiveSound.GetTransmitter()->GetParameter(WaveParameterName, ParamValue);

	if (USoundWave* NewWave = Cast<USoundWave>(ParamValue.ObjectParam))
	{
		NewWave->Parse(AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, (UPTRINT)NewWave, 0), ActiveSound, ParseParams, WaveInstances);
	}
	else
	{
		// use the default node linked to us, if any
		Super::ParseNodes(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
	}
}


