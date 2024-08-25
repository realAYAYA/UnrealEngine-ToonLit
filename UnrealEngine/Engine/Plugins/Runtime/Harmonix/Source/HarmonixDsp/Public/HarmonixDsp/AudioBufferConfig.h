// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// Harmonix
#include "HarmonixDsp/AudioBufferConstants.h"


// UE
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "HAL/Platform.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAudioBufferConfig, Log, All);

struct HARMONIXDSP_API FAudioBufferConfig
{
public:
	static const uint32 kMaxAudioBufferChannels = 8;

	FAudioBufferConfig();
	FAudioBufferConfig(int32 InNumChannels, int32 InNumFrames, float InSampleRate = 0.0f, bool InInterleaved = false);
	FAudioBufferConfig(EAudioBufferChannelLayout InChannelLayout, int32 InNumFrames, float InSampleRate = 0.0f, bool InInterleaved = false);
	FAudioBufferConfig(EAudioBufferChannelLayout InChannelLayout, int32 InNumChannels, int32 InNumFrames, float InSampleRate = 0.0f, bool InInterleaved = false);

	int32 GetNumTotalSamples() const;
	float GetSampleRate() const;
	void  SetSampleRate(float InSamplesPerSecond);
	
	FORCEINLINE int32 GetNumChannels() const { return NumChannels; }
	void SetNumChannels(int32 InNumChannels);

	FORCEINLINE int32 GetNumFrames() const { return NumFrames; }
	void SetNumFrames(int32 InNumFrames);

	FAudioBufferConfig GetInterleavedConfig() const;

	FAudioBufferConfig GetDeinterleavedConfig() const;

	FORCEINLINE bool GetIsInterleaved() const { return Interleaved; }

	FORCEINLINE EAudioBufferChannelLayout GetChannelLayout() const { return ChannelLayout; }
	void SetChannelLayout(EAudioBufferChannelLayout InChannelLayout);

	FORCEINLINE uint32 GetChannelMask() const { return ChannelMask; }
	void SetChannelMask(uint32 InChannelMask);

	bool operator==(const FAudioBufferConfig&) const;

private:
	float SampleRate;
	int32 NumChannels;
	int32 NumFrames;
	uint32 ChannelMask;
	bool Interleaved;
	EAudioBufferChannelLayout ChannelLayout;
};