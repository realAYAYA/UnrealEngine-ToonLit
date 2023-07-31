// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationTrimFade.h"
#include "Sound/SoundWave.h"

namespace WaveformTransformationTrimFadeNames
{
	static FLazyName StartTimeName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartTime));
	static FLazyName EndTimeName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndTime));
}

static void ApplyFadeIn(Audio::FAlignedFloatBuffer& InputAudio, const float FadeLength, const float FadeCurve, const int32 NumChannels, const float SampleRate)
{
	check(NumChannels > 0);

	if(InputAudio.Num() < NumChannels || FadeLength < SMALL_NUMBER)
	{
		return;
	}
	
	const int32 FadeNumFrames = FMath::Min(FadeLength * SampleRate / NumChannels, InputAudio.Num() / NumChannels);
	float* InputPtr = InputAudio.GetData();
	
	for(int32 FrameIndex = 0;FrameIndex < FadeNumFrames; ++FrameIndex)
	{
		const float FadeFraction = (float)FrameIndex / FadeNumFrames;
		const float EnvValue = FMath::Pow(FadeFraction, FadeCurve);
		
		for(int32 ChannelIt = 0; ChannelIt < NumChannels; ++ChannelIt)
		{
			*(InputPtr + ChannelIt) *= EnvValue;
		}

		InputPtr += NumChannels;
	}
}

static void ApplyFadeOut(Audio::FAlignedFloatBuffer& InputAudio, const float FadeLength, const float FadeCurve, const int32 NumChannels, const float SampleRate)
{
	check(NumChannels > 0);

	if(InputAudio.Num() < NumChannels || FadeLength < SMALL_NUMBER)
	{
		return;
	}
	
	const int32 FadeNumFrames = FMath::Min(FadeLength * SampleRate / NumChannels, InputAudio.Num() / NumChannels);
	const int32 StartSampleIndex = InputAudio.Num() - (FadeNumFrames * NumChannels);
	float* InputPtr = &InputAudio[StartSampleIndex];
	
	for(int32 FrameIndex = 0;FrameIndex < FadeNumFrames; ++FrameIndex)
	{
		const float FadeFraction = (float)FrameIndex / FadeNumFrames;
		const float EnvValue = 1.f - FMath::Pow(FadeFraction, FadeCurve);
		
		for(int32 ChannelIt = 0; ChannelIt < NumChannels; ++ChannelIt)
		{
			*(InputPtr + ChannelIt) *= EnvValue;
		}

		InputPtr += NumChannels;
	}
}

FWaveTransformationTrimFade::FWaveTransformationTrimFade(double InStartTime, double InEndTime, float InStartFadeTime, float InStartFadeCurve, float InEndFadeTime, float InEndFadeCurve)
	: StartTime(InStartTime)
	, EndTime(InEndTime)
	, StartFadeTime(InStartFadeTime)
	, StartFadeCurve(FMath::Max(InStartFadeCurve, 0.f))
	, EndFadeTime(InEndFadeTime)
	, EndFadeCurve(FMath::Max(InEndFadeCurve, 0.f)) {}

void FWaveTransformationTrimFade::ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const
{
	check(InOutWaveInfo.SampleRate > 0.f && InOutWaveInfo.Audio != nullptr);

	Audio::FAlignedFloatBuffer& InputAudio = *InOutWaveInfo.Audio;

	if(InputAudio.Num() == 0)
	{
		return;
	}

	const int32 StartSample = FMath::RoundToInt32(StartTime * InOutWaveInfo.SampleRate) * InOutWaveInfo.NumChannels;
	int32 EndSample = InputAudio.Num() - 1;
	
	if(EndTime > 0.f)
	{	
		const int32 EndFrame = FMath::RoundToInt32(EndTime * InOutWaveInfo.SampleRate);
		EndSample = EndFrame * InOutWaveInfo.NumChannels - 1;
		EndSample = FMath::Min(EndSample, InputAudio.Num() - 1);
	}

	const int32 FinalSize = EndSample - StartSample + 1;

	InOutWaveInfo.StartFrameOffset = StartSample - (StartSample % InOutWaveInfo.NumChannels);
	InOutWaveInfo.NumEditedSamples = FinalSize;
	
	if (FinalSize > InputAudio.Num() || FinalSize <= 0)
	{
		return;
	}

	const bool bProcessFades = StartFadeTime > 0.f || EndFadeTime > 0.f;

	if (!bProcessFades && FinalSize == InputAudio.Num())
	{
		return;
	}

	TArray<float> TempBuffer;
	TempBuffer.AddUninitialized(FinalSize);

	FMemory::Memcpy(TempBuffer.GetData(), &InputAudio[StartSample], FinalSize * sizeof(float));

	InputAudio.Empty();
	InputAudio.AddUninitialized(FinalSize);

	FMemory::Memcpy(InputAudio.GetData(), TempBuffer.GetData(), FinalSize * sizeof(float));


	if(StartFadeTime > 0.f)
	{
		ApplyFadeIn(InputAudio, StartFadeTime, StartFadeCurve, InOutWaveInfo.NumChannels, InOutWaveInfo.SampleRate);
	}
	
	if(EndFadeTime > 0.f)
	{
		ApplyFadeOut(InputAudio, EndFadeTime, EndFadeCurve, InOutWaveInfo.NumChannels, InOutWaveInfo.SampleRate);
	}
}

Audio::FTransformationPtr UWaveformTransformationTrimFade::CreateTransformation() const
{
	return MakeUnique<FWaveTransformationTrimFade>(StartTime, EndTime, StartFadeTime, StartFadeCurve, EndFadeTime, EndFadeCurve);
}

void UWaveformTransformationTrimFade::UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration)
{
	UpdateDurationProperties(InOutConfiguration.EndTime - InOutConfiguration.StartTime);

	InOutConfiguration.StartTime = StartTime;
	InOutConfiguration.EndTime = EndTime;
}

void UWaveformTransformationTrimFade::UpdateDurationProperties(const float InAvailableDuration)
{
	check(InAvailableDuration > 0)
	AvailableWaveformDuration = InAvailableDuration;
	StartTime = FMath::Clamp(StartTime, 0.f, AvailableWaveformDuration - UE_KINDA_SMALL_NUMBER);
	EndTime = EndTime < 0 ? EndTime = AvailableWaveformDuration : FMath::Clamp(EndTime, StartTime + UE_KINDA_SMALL_NUMBER, AvailableWaveformDuration);
}