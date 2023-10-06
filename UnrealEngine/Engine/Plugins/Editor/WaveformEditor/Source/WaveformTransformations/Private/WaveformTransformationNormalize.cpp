// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationNormalize.h"

#include "DSP/FloatArrayMath.h"
#include "AudioAnalyzer.h"
#include "IAudioAnalyzerInterface.h"
#include "LoudnessFactory.h"
#include "Algo/MaxElement.h"
#include "MeterFactory.h"

namespace WaveformTransformNormalizeHelpers
{
	float GetRMSPeak(const Audio::FAlignedFloatBuffer& InputAudio, float SampleRate, int32 NumChannels)
	{
		const float AudioLengthSeconds = InputAudio.Num() / NumChannels / SampleRate;
		const float RMSAnalysisPeriod = 0.3f;
		const bool bAnalyzeAsSingleBuffer = RMSAnalysisPeriod > AudioLengthSeconds;

		Audio::FMeterSettings Settings;
		Settings.AnalysisPeriod = bAnalyzeAsSingleBuffer ? AudioLengthSeconds : RMSAnalysisPeriod;

		if (bAnalyzeAsSingleBuffer)
		{
			Audio::FMeterAnalyzer Analyzer(SampleRate, NumChannels, Settings);
			Audio::FMeterAnalyzerResults Results = Analyzer.ProcessAudio(InputAudio);
			const float* MeterValue = Algo::MaxElement(Results.MeterValues, [](const float A, const float B)
				{
					return A < B;
				});

			if (MeterValue)
			{
				return *MeterValue;
			}
		}
		else
		{
			Audio::FMeterFactory Analyzer;
			TUniquePtr<Audio::IAnalyzerResult> Result = Analyzer.NewResult();
			TUniquePtr<Audio::IAnalyzerWorker> AnalyzerWorker = Analyzer.NewWorker({ (int32)SampleRate, NumChannels }, &Settings);

			if (AnalyzerWorker == nullptr)
			{
				return 0.f;
			}

			AnalyzerWorker->Analyze(MakeArrayView(InputAudio), Result.Get());

			Audio::FMeterResult* MeterResult = static_cast<Audio::FMeterResult*>(Result.Get());

			if (MeterResult)
			{
				const TArray<Audio::FMeterEntry>& LoudnessArray = MeterResult->GetMeterArray();
				auto MeterEntry = Algo::MaxElement(LoudnessArray, [](const Audio::FMeterEntry& A, const Audio::FMeterEntry& B)
					{
						return A.MeterValue < B.MeterValue;
					});

				if (MeterEntry)
				{
					return MeterEntry->MeterValue;
				}
			}
		}
		
		return 0.f;
	}

	float GetLoudnessPeak(const Audio::FAlignedFloatBuffer& InputAudio, float SampleRate, int32 NumChannels)
	{
		Audio::FLoudnessFactory Analyzer;
		Audio::FLoudnessSettings Settings;
		Settings.AnalysisPeriod = 1.f;

		TUniquePtr<Audio::IAnalyzerResult> Result = Analyzer.NewResult();
		TUniquePtr<Audio::IAnalyzerWorker> AnalyzerWorker = Analyzer.NewWorker({ (int32)SampleRate, NumChannels }, &Settings);

		if (AnalyzerWorker == nullptr)
		{
			return 0.f;
		}

		AnalyzerWorker->Analyze(MakeArrayView(InputAudio), Result.Get());

		Audio::FLoudnessResult* LoudnessResult = static_cast<Audio::FLoudnessResult*>(Result.Get());

		if (LoudnessResult)
		{
			const TArray<Audio::FLoudnessEntry>& LoudnessArray = LoudnessResult->GetLoudnessArray();
			auto LoudestEntry = Algo::MaxElement(LoudnessArray, [](const Audio::FLoudnessEntry& A, const Audio::FLoudnessEntry& B)
				{
					return A.Loudness < B.Loudness;
				});

			if (LoudestEntry)
			{
				return LoudestEntry->Loudness;
			}
		}

		return 0.f;
	}
}


FWaveTransformationNormalize::FWaveTransformationNormalize(float InTarget, float InMaxGain, ENormalizationMode InMode)
	: Target(InTarget)
	, MaxGain(InMaxGain)
	, Mode(InMode) {}


 void FWaveTransformationNormalize::ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const
{
	check(InOutWaveInfo.Audio != nullptr);
	
	Audio::FAlignedFloatBuffer& InputAudio = *InOutWaveInfo.Audio;

	float PeakDecibelValue = 1.f;
	
	switch (Mode)
	{
	case ENormalizationMode::Peak:
		{
			const float PeakLinearValue = Audio::ArrayMaxAbsValue(InputAudio);
			PeakDecibelValue = Audio::ConvertToDecibels(PeakLinearValue);
			break;
		}
	case ENormalizationMode::RMS:
		PeakDecibelValue = WaveformTransformNormalizeHelpers::GetRMSPeak(InputAudio, InOutWaveInfo.SampleRate, InOutWaveInfo.NumChannels);
		PeakDecibelValue = Audio::ConvertToDecibels(PeakDecibelValue);
		break;
	case ENormalizationMode::DWeightedLoudness:
		PeakDecibelValue = WaveformTransformNormalizeHelpers::GetLoudnessPeak(InputAudio,  InOutWaveInfo.SampleRate, InOutWaveInfo.NumChannels);
		break;
	case ENormalizationMode::COUNT:
	default:
		return;
	}

	const float TargetOffset = Target - PeakDecibelValue;
	const float MakeupGain = FMath::Clamp(TargetOffset, -MaxGain, MaxGain);

	if (MakeupGain != 0.f)
	{
		Audio::ArrayMultiplyByConstantInPlace(InputAudio, Audio::ConvertToLinear(MakeupGain));
	}
}

Audio::FTransformationPtr UWaveformTransformationNormalize::CreateTransformation() const
{
	return MakeUnique<FWaveTransformationNormalize>(Target, MaxGain, Mode);
}
