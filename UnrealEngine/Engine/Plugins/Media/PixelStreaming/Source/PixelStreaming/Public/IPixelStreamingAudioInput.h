// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class PIXELSTREAMING_API IPixelStreamingAudioInput
{
public:
	IPixelStreamingAudioInput(){};
	virtual ~IPixelStreamingAudioInput(){};

    /* Pushes audio, which must match the underlying sample rate/num channels of the mixer. */
    virtual void PushAudio(const float* InBuffer, int32 NumSamples, int32 NumChannels, int32 SampleRate) = 0;
};
