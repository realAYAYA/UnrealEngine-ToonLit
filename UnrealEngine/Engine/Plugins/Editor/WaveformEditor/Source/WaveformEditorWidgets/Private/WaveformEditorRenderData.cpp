// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorRenderData.h"

#include "AudioExtensions/Public/IWaveformTransformation.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/FloatArrayMath.h"
#include "Sound/SoundWave.h"

TArrayView<const int16> FWaveformEditorRenderData::GetSampleData() const
{
	return SampleData;
}

const uint16 FWaveformEditorRenderData::GetNumChannels() const
{
	return NumChannels;
}

const uint32 FWaveformEditorRenderData::GetNumSamples() const
{
	return NumSamples;
}

const uint32 FWaveformEditorRenderData::GetSampleRate() const
{
	return SampleRate;
}

const float FWaveformEditorRenderData::GetOriginalWaveformDurationInSeconds() const
{
	return OriginalWaveformDurationInSeconds;
}

const float FWaveformEditorRenderData::GetTransformedWaveformDurationInSeconds() const
{
	return TransformedWaveformDurationInSeconds;
}

const uint32 FWaveformEditorRenderData::GetOriginalWaveformFrames() const
{
	return OriginalWaveformFrames;
}

const uint32 FWaveformEditorRenderData::GetTransformedWaveformFrames() const
{
	return TransformedWaveformFrames;
}

const TRange<float> FWaveformEditorRenderData::GetTransformedWaveformBounds() const
{
	return TransformedWaveformBounds;
}

void FWaveformEditorRenderData::UpdateRenderData(const uint8* InRawData, const uint32 InNumSamples, const uint32 InFirstEditedSample, const uint32 InLastEditedSample, const uint32 InSampleRate, const uint16 InNumChannels)
{
	NumSamples = InNumSamples;
	NumEditedSamples = InLastEditedSample - InFirstEditedSample;
	SampleRate = InSampleRate;
	NumChannels = InNumChannels;
	const int16* SampleDataPtr = reinterpret_cast<const int16*>(InRawData);
	SampleData = MakeArrayView(SampleDataPtr, NumSamples);

	OriginalWaveformFrames = NumSamples / NumChannels;
	OriginalWaveformDurationInSeconds = OriginalWaveformFrames / (float) SampleRate;
	
	TransformedWaveformFrames = (InLastEditedSample - InFirstEditedSample) / NumChannels;
	TransformedWaveformDurationInSeconds = TransformedWaveformFrames / (float) SampleRate;

	const float FirstEditedSampleOffset = InFirstEditedSample / (float) NumSamples;
	const float LastEditedSampleOffset = (NumSamples - InLastEditedSample) / (float) NumSamples;	
	
	TransformedWaveformBounds.SetLowerBoundValue(FirstEditedSampleOffset);
	TransformedWaveformBounds.SetUpperBoundValue(1 - LastEditedSampleOffset);

	if (OnRenderDataUpdated.IsBound())
	{
		OnRenderDataUpdated.Broadcast();
	}
}