// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/AudioAnalysis/VuMeterAnalyzer.h"

#include "HarmonixDsp/AudioUtility.h"
#include "HarmonixDsp/StridePointer.h"
#include "HarmonixDsp/AudioAnalysis/AnalysisUtilities.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Harmonix::Dsp::AudioAnalysis
{
	FVuMeterAnalyzer::FVuMeterAnalyzer(float InSampleRate) : SampleRate(InSampleRate)
	{
		Reset();
	}

	void FVuMeterAnalyzer::SetSettings(const FHarmonixVuMeterAnalyzerSettings& InSettings)
	{
		FScopeLock Lock{ &SettingsGuard };
		Settings = InSettings;
	}

	void FVuMeterAnalyzer::Reset()
	{
		SetSettings(FHarmonixVuMeterAnalyzerSettings{});
		
		ChannelStates.Reset();
		MonoState = FChannelState{};
	}

	void FVuMeterAnalyzer::Process(const TAudioBuffer<float>& InBuffer, FHarmonixVuMeterAnalyzerResults& InOutResults)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Harmonix::Dsp::AudioAnalysis::FVuMeterAnalyzer::Process);
		
		const int32 NumChannels = InBuffer.GetNumValidChannels();

		// copy the settings so we don't hold up another thread trying to set them
		FHarmonixVuMeterAnalyzerSettings SettingsCopy;
		{
			FScopeLock Lock{ &SettingsGuard };
			SettingsCopy = Settings;
		}
		
		if (NumChannels <= 0 || SampleRate <= 0)
		{
			InOutResults = FHarmonixVuMeterAnalyzerResults{};
			return;
		}

		// Update the channel values arrays if needed
		InOutResults.ChannelValues.SetNum(NumChannels);
		ChannelStates.SetNum(NumChannels);

		const int32 NumFrames = InBuffer.GetNumValidFrames();
		const float MsThisPass = 1000.0f * NumFrames / SampleRate;
		float MonoMeanSq = 0;
		float MonoPeakSq = 0;

		// weights for leaky integrator
		float InvWeight = 0;
		if (SettingsCopy.AvgWindowMs > 1.0e-9f)
		{
			InvWeight = expf(-1000.0f / (SampleRate * SettingsCopy.AvgWindowMs));
		}
		const float Weight = 1 - InvWeight;

		// update the state
		for (int ChannelIdx = 0; ChannelIdx < NumChannels; ++ChannelIdx)
		{
			float ChMeanSq = ChannelStates[ChannelIdx].LatestValues.LevelMeanSquared;
			float PeakSq = 0;

			// Find the mean squared and peak squared for the channel
			TDynamicStridePtr<float> InputPtr = InBuffer.GetStridingChannelDataPointer(ChannelIdx);
			for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
			{
				const float Sample = InputPtr[FrameIdx];
				const float SampleSq = Sample * Sample;
				ChMeanSq = ChMeanSq * InvWeight + SampleSq * Weight;
				PeakSq = FMath::Max(SampleSq, PeakSq);
			}

			// Map the peak to the range in decibels, but stay in linear
			PeakSq = HarmonixDsp::MapLinearToDecibelRange(
				PeakSq,
				SettingsCopy.OutputSettings.MaxDecibels,
				SettingsCopy.OutputSettings.RangeDecibels);

			// Update the channel state and results
			UpdateChannel(
				ChannelStates[ChannelIdx],
				InOutResults.ChannelValues[ChannelIdx],
				PeakSq,
				ChMeanSq,
				MsThisPass,
				SettingsCopy);

			// Track the mono values
			// peak of any channel...
			MonoPeakSq = FMath::Max(PeakSq, MonoPeakSq);
			// simply average across all channels...
			MonoMeanSq += PeakSq / NumChannels;
		}

		MonoState.LatestValues.LevelMeanSquared = MonoMeanSq;

		// NB: The peak in dB is a special case, it is not cleared after the reset time.
		InOutResults.MonoPeakDecibels = FMath::Max(HarmonixDsp::LinearToDB(FMath::Sqrt(MonoPeakSq)), InOutResults.MonoPeakDecibels);

		// Update the mono state
		UpdateChannel(
			MonoState,
			InOutResults.MonoValues,
			MonoPeakSq,
			MonoMeanSq,
			MsThisPass,
			SettingsCopy);
	}

	void FVuMeterAnalyzer::UpdateChannel(
		FChannelState& ChannelState,
		FHarmonixVuMeterAnalyzerChannelValues& ResultsValues,
		const float PeakSquared,
		float MeanSquared,
		const float ElapsedMs,
		const FHarmonixVuMeterAnalyzerSettings& SettingsToUse)
	{
		// reset the peak if needed
		ChannelState.PeakHoldRemainingMs -= ElapsedMs;
		if (ChannelState.PeakHoldRemainingMs <= 0.0f)
		{
			ChannelState.LatestValues.PeakSquared = 0.0f;
		}

		// update the peak
		if (PeakSquared > ChannelState.LatestValues.PeakSquared)
		{
			ChannelState.LatestValues.PeakSquared = ResultsValues.PeakSquared = PeakSquared;
			ChannelState.PeakHoldRemainingMs = SettingsToUse.PeakHoldMs;
		}

		// set the level with smoothing
		{
			MeanSquared = HarmonixDsp::MapLinearToDecibelRange(
				MeanSquared,
				SettingsToUse.OutputSettings.MaxDecibels,
				SettingsToUse.OutputSettings.RangeDecibels);
			ChannelState.LatestValues.LevelMeanSquared = MeanSquared;
			
			// smooth from the last result value, not the last actual value
			const float PreviousMeanSquared = ResultsValues.LevelMeanSquared;
			MeanSquared = FMath::Max(MeanSquared, ChannelState.RiseTargetSquared);
			ResultsValues.LevelMeanSquared = SmoothEnergy(
				MeanSquared,
				PreviousMeanSquared,
				ElapsedMs,
				SettingsToUse.OutputSettings);

			// If we didn't make it to the rise target, remember it for next time
			if (ResultsValues.LevelMeanSquared > PreviousMeanSquared)
			{
				ChannelState.RiseTargetSquared = MeanSquared > ResultsValues.LevelMeanSquared ? MeanSquared : -1;
			}
			else
			{
				ChannelState.RiseTargetSquared = -1;
			}
		}
	}
}
