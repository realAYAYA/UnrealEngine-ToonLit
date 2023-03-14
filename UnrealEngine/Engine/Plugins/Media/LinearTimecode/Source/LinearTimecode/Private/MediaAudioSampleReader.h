// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaAudioSample.h"

/** Handles conversion from the different sample formats */

class MediaAudioSampleReader
{
public:
	MediaAudioSampleReader(TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& InAudioFrame)
		: AudioFrame(InAudioFrame)
	{
		SampleFormat = AudioFrame->GetFormat();
		SampleStream = reinterpret_cast<const uint8*>(AudioFrame->GetBuffer());
		SampleStep = AudioFrame->GetChannels() * GetSampleSize(SampleFormat);
		SampleEnd = SampleStream + AudioFrame->GetFrames() * SampleStep;
	}

	/** Get the next Sample from the Audio packet
	 * @param OutFloat Returned Sample, -1.0 to 1.0 range
	 * return true valid sample returned, false no more samples
	 */
	bool GetSample(float& OutFloat)
	{
		if (SampleStream == SampleEnd)
		{
			return false;
		}
		switch (SampleFormat)
		{
		case EMediaAudioSampleFormat::Double:
			OutFloat = static_cast<float>(*reinterpret_cast<const double*>(SampleStream));
			SampleStream += SampleStep;
			return true;
		case EMediaAudioSampleFormat::Float:
			OutFloat = *reinterpret_cast<const float*>(SampleStream);
			SampleStream += SampleStep;
			return true;
		case EMediaAudioSampleFormat::Int32:
			OutFloat = static_cast<float>(*reinterpret_cast<const int32*>(SampleStream) / 2147483647.0);
			SampleStream += SampleStep;
			return true;
		case EMediaAudioSampleFormat::Int16:
			OutFloat = static_cast<float>(*reinterpret_cast<const int16*>(SampleStream) / 32767.0f);
			SampleStream += SampleStep;
			return true;
		case EMediaAudioSampleFormat::Int8:
			OutFloat = static_cast<float>(*reinterpret_cast<const int8*>(SampleStream) / 127.0f);
			SampleStream += SampleStep;
			return true;
		}
		OutFloat = 0.0f;
		return false;
	}

	int32 GetSampleStep() const { return SampleStep; }
	const uint8* const GetCurrentSampleStream() const { return SampleStream; }


protected:
	static int GetSampleSize(EMediaAudioSampleFormat inFormat)
	{
		switch (inFormat)
		{
		case EMediaAudioSampleFormat::Double:
			return 8;
		case EMediaAudioSampleFormat::Float:
		case EMediaAudioSampleFormat::Int32:
			return 4;
		case EMediaAudioSampleFormat::Int16:
			return 2;
		case EMediaAudioSampleFormat::Int8:
			return 1;
		}
		return 0;
	}

protected:
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& AudioFrame;

	EMediaAudioSampleFormat SampleFormat;
	const uint8* SampleStream;
	const uint8* SampleEnd;
	int32 SampleStep;
};

