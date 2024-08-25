// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/Effects/DistortionV1.h"
#include "HarmonixDsp/AudioUtility.h"
#include "HAL/PlatformMath.h"
#include "Math/UnrealMath.h"

namespace Harmonix::Dsp::Effects
{

/*

FIR filter designed with
 http://t-filter.appspot.com

sampling frequency: 192000 Hz

* 0 Hz - 14000 Hz
  gain = 1
  desired ripple = 1 dB
  actual ripple = 0.7796156360240859 dB

* 23500 Hz - 96000 Hz
  gain = 0
  desired attenuation = -40 dB
  actual attenuation = -39.687516944147845 dB

*/
const float FDistortionV1::kOversamplingFilterTaps[FDistortionV1::kNumFilterTaps] = {
   0.00418482115493418f,   0.013551254941047723f,   0.012686975005637062f,   0.014699130648569597f,
   0.008901898051062396f, -0.001262265643319461f,  -0.01519076102473826f,   -0.02789823550531095f,
  -0.03390582823662936f,  -0.02768594317135351f,   -0.0060992166725150065f,  0.030271528968730574f,
   0.0765228538488631f,    0.12425289453921509f,    0.16358980718702476f,    0.18580517521235113f,
   0.18580517521235113f,   0.16358980718702476f,    0.12425289453921509f,    0.0765228538488631f,
   0.030271528968730574f, -0.0060992166725150065f, -0.02768594317135351f,   -0.03390582823662936f,
   -0.02789823550531095f, -0.015190761024738271f,  -0.0012622656433194607f,  0.008901898051062396f,
   0.014699130648569597f,  0.012686975005637062f,   0.013551254941047723f,   0.00418482115493418f
};

FDistortionV1::FDistortionV1(int32 InSampleRate)
	: Type(EDistortionTypeV1::Clean)
	, InputGain(1)
	, OutputGain(1)
	, DCAdjust(0)
	, DoOversampling(false)
	, SampleRate(InSampleRate)
{
	for (int32 i = 0; i < kNumFilters; i++)
	{
		FilterPreClip[i] = false;
		FilterEnabled[i] = false;
	}

	for (int32 i = 0; i < kMaxChannels; ++i)
	{
		OversampleFilterUp[i].Init(kOversamplingFilterTaps, kNumFilterTaps, false);
		OversampleFilterDown[i].Init(kOversamplingFilterTaps, kNumFilterTaps, false);
	}
}

void FDistortionV1::SetInputGainDb(float InGainDb)
{
	InputGain = FMath::Clamp(HarmonixDsp::DBToLinear(InGainDb), 0.0f, 1.0f);
}

void FDistortionV1::SetOutputGainDb(float InGainDb)
{
	OutputGain = FMath::Clamp(HarmonixDsp::DBToLinear(InGainDb), 0.0f, 1.0f);
}

void FDistortionV1::SetType(EDistortionTypeV1 InType)
{
	Type = InType;
}

void FDistortionV1::SetupFilter(int32 Index, const FBiquadFilterSettings& InSettings)
{
	check(0 <= Index && Index < kNumFilters);
	FilterEnabled[Index] = InSettings.IsEnabled;
	if (InSettings.IsEnabled)
	{
		FilterCoefs[Index].MakeFromSettings(InSettings, (float)SampleRate);
		for (int32 i = 0; i < kMaxChannels; ++i)
		{
			Filter[Index][i].ResetState();
		}
	}
}

void FDistortionV1::SetupFilter(int32 Index, const FDistortionFilterSettings& InSettings)
{
	check(0 <= Index && Index < kNumFilters);
	SetupFilter(Index, InSettings.Filter);
}

void FDistortionV1::SetOversample(bool InOversample, int32 InRenderBufferSizeFrames)
{
	if (InOversample && !DoOversampling)
	{
		for (int32 i = 0; i < kMaxChannels; ++i)
		{
			OversampleFilterUp[i].Reset();
			OversampleFilterDown[i].Reset();
		}
		int32 NumFrames = InRenderBufferSizeFrames * 4; // 4x oversampling
		if (UpsampleBuffer.GetLengthInFrames() != NumFrames)
		{
			UpsampleBuffer.Configure(1, NumFrames, EAudioBufferCleanupMode::Delete, (float)SampleRate);
		}
	}
	DoOversampling = InOversample;
}

void FDistortionV1::SetSampleRate(int32 InSampleRate)
{
	SampleRate = InSampleRate;
}

void FDistortionV1::ProcessChannel(int32 InChannel, float* InOutSamples, int32 InNumFrames)
{
	// process "pre-clip" filters...
	for (int32 i = 0; i < kNumFilters; ++i)
	{
		if (FilterEnabled[i] && FilterPreClip[i])
		{
			Filter[i][InChannel].Process(InOutSamples, InOutSamples, InNumFrames, FilterCoefs[i]);
		}
	}

	float* ClipBuffer = InOutSamples;
	int32    NumClipFrames = InNumFrames;

	if (DoOversampling)
	{
		// up sample...
		check(InNumFrames * 4 <= UpsampleBuffer.GetLengthInFrames());
		float* filter_out = UpsampleBuffer.GetValidChannelData(0);
		for (int32 i = 0; i < InNumFrames; ++i)
		{
			OversampleFilterUp[InChannel].Upsample4x(InOutSamples[i], filter_out);
			filter_out += 4;
		}
		ClipBuffer = UpsampleBuffer.GetValidChannelData(0);
		NumClipFrames = InNumFrames * 4;
	}

	// clip...
	switch (Type)
	{
	case EDistortionTypeV1::Dirty:
	{
		for (int32 i = 0; i < NumClipFrames; ++i)
		{
			ClipBuffer[i] = FMath::Clamp((ClipBuffer[i] + DCAdjust) * InputGain, -1.0f, 1.0f) * OutputGain;
		}
	}
	break;
	case EDistortionTypeV1::Clean:
	{
		for (int32 i = 0; i < NumClipFrames; ++i)
		{
			ClipBuffer[i] = tanhf((ClipBuffer[i] + DCAdjust) * InputGain) * OutputGain;
		}
	}
	break;
	case EDistortionTypeV1::Warm:
	{
		for (int32 i = 0; i < NumClipFrames; ++i)
		{
			ClipBuffer[i] = FMath::Sin((ClipBuffer[i] + DCAdjust) * InputGain) * OutputGain;
		}
	}
	break;
	case EDistortionTypeV1::Soft:   //soft and asymmetric from http://www.music.mcgill.ca/~gary/courses/projects/618_2009/NickDonaldson/#Distortion
	{
		for (int32 FrameIdx = 0; FrameIdx < NumClipFrames; ++FrameIdx)
		{
			float OutSample = (ClipBuffer[FrameIdx] + DCAdjust) * InputGain;
			if (OutSample > 1)
			{
				OutSample = .66666f;
			}
			else if (OutSample < -1)
			{
				OutSample = -.66666f;
			}
			else
			{
				OutSample = OutSample - (OutSample * OutSample * OutSample) / 3.0f;
			}
			ClipBuffer[FrameIdx] = OutSample * OutputGain;
		}
	}
	break;
	case EDistortionTypeV1::Asymmetric:
	{
		for (int32 i = 0; i < NumClipFrames; ++i)
		{
			float OutSample = (ClipBuffer[i] * .5f + DCAdjust) * InputGain;
			if (OutSample >= .320018f)
			{
				OutSample = .630035f;
			}
			else if (OutSample >= -.08905f)
			{
				OutSample = -6.153f * OutSample * OutSample + 3.9375f * OutSample;
			}
			else if (OutSample >= -1)
			{
				OutSample = -.75f * (1 - FMath::Pow(1 - (FMath::Abs(OutSample) - .032847f), 12) + .333f * (FMath::Abs(OutSample) - .032847f)) + .01f;
			}
			else
			{
				OutSample = -.9818f;
			}
			ClipBuffer[i] = OutSample * OutputGain;
		}
	}
	break;
	}

	if (DoOversampling)
	{
		// down sample...
		float* filter_in = UpsampleBuffer.GetValidChannelData(0);
		for (int32 i = 0; i < InNumFrames; ++i)
		{
			OversampleFilterDown[InChannel].AddData(filter_in, 4);
			filter_in += 4;
			InOutSamples[i] = OversampleFilterDown[InChannel].GetSample();
		}
	}

	// process "post-clip" filters...
	for (int32 FilterIdx = 0; FilterIdx < kNumFilters; ++FilterIdx)
	{
		if (FilterEnabled[FilterIdx] && !FilterPreClip[FilterIdx])
		{
			Filter[FilterIdx][InChannel].Process(InOutSamples, InOutSamples, InNumFrames, FilterCoefs[FilterIdx]);
		}
	}
}

void FDistortionV1::Process(TAudioBuffer<float>& InOutBuffer)
{
	// process each channel separately
	for (int32 Ch = 0; Ch < InOutBuffer.GetNumValidChannels(); ++Ch)
	{
		ProcessChannel(Ch, InOutBuffer.GetValidChannelData(Ch), InOutBuffer.GetNumValidFrames());
	}
}

};