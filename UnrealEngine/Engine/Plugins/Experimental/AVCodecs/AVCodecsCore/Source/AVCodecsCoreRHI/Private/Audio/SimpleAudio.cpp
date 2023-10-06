// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/SimpleAudio.h"

#include "Audio/Encoders/Configs/AudioEncoderConfigAAC.h"

ESimpleAudioCodec USimpleAudioHelper::GuessCodec(TSharedRef<FAVInstance> const& Instance)
{
	if (Instance->Has<FAudioEncoderConfigAAC>())
	{
		return ESimpleAudioCodec::AAC;
	}

	return ESimpleAudioCodec::AAC;
}
