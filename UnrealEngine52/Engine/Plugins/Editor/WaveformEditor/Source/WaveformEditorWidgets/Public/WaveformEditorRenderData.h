// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Math/Range.h"

class USoundWave;

DECLARE_MULTICAST_DELEGATE(FOnRenderDataUpdated)


/************************************************************************************************/
/* FWaveformEditorRenderData																	*/
/* The Waveform Editor Render Data generates a view of Raw PCM data through the 				*/
/* UpdateRenderData() method.																	*/
/* 																								*/
/* This class can be used to propagate efficiently waveform data to different renderers 		*/
/* while keeping it separate from them, but please not that it DOES NOT hold onto that data.	*/
/*																								*/
/* Users of this class should make sure that the data being shown by the view is valid. 		*/
/************************************************************************************************/

class WAVEFORMEDITORWIDGETS_API FWaveformEditorRenderData : public TSharedFromThis<FWaveformEditorRenderData>
{
public:
	FWaveformEditorRenderData() = default;

	TArrayView<const int16> GetSampleData() const;
	const uint16 GetNumChannels() const;
	const uint32 GetNumSamples() const;
	const uint32 GetSampleRate() const;
	const float GetOriginalWaveformDurationInSeconds() const;
	const float GetTransformedWaveformDurationInSeconds() const;
	const uint32 GetOriginalWaveformFrames() const;
	const uint32 GetTransformedWaveformFrames() const;
	const TRange<float> GetTransformedWaveformBounds() const;

	/** Call this to generate the view and the info shown by the render data */
	void UpdateRenderData(const uint8* InRawData, const uint32 InNumSamples, const uint32 InFirstEditedSample, const uint32 LastEditedSample, const uint32 InSampleRate, const uint16 InNumChannels);

	/** Called when the render data view is updated */
	FOnRenderDataUpdated OnRenderDataUpdated;
	
private:
	TArrayView<const int16> SampleData;

	uint16 NumChannels = 0;
	uint32 NumSamples = 0;
	uint32 NumEditedSamples = 0;
	uint32 SampleRate = 0;

	float OriginalWaveformDurationInSeconds = 0.f;
	float TransformedWaveformDurationInSeconds = 0.f;
	uint32 OriginalWaveformFrames = 0;
	uint32 TransformedWaveformFrames = 0;

	/* The bounds of the transformed waveform in relation to the original */
	TRange<float> TransformedWaveformBounds = TRange<float>::Inclusive(0.f, 1.f);
};