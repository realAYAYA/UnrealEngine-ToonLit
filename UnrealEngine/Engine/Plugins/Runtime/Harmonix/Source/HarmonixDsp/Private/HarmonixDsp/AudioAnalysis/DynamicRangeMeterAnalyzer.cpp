// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/AudioAnalysis/DynamicRangeMeterAnalyzer.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Harmonix::Dsp::AudioAnalysis
{
	FDynamicRangeMeterAnalyzer::FDynamicRangeMeterAnalyzer()
	{
		Reset();
	}

	void FDynamicRangeMeterAnalyzer::SetSettings(const FSettings& InSettings)
	{
		FScopeLock Lock{ &SettingsGuard };
		Settings = InSettings;
	}

	void FDynamicRangeMeterAnalyzer::Reset()
	{
		State = FState{};
	}

	void FDynamicRangeMeterAnalyzer::Process(const TAudioBuffer<float>& InBuffer, FResults& InOutResults)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Harmonix::Dsp::AudioAnalysis::FDynamicRangeMeterAnalyzer::Process);
		
		// copy the settings so we don't hold up another thread trying to set them
		FSettings SettingsCopy;
		{
			FScopeLock Lock{ &SettingsGuard };
			SettingsCopy = Settings;
		}

		// If the window size changed, or the user requested a reset, reset the state
		if (State.RmsWindow.Num() != SettingsCopy.WindowSize || InOutResults.RequestReset)
		{
			State.RmsWindow.Reset();
			State.RmsWindow.AddZeroed(SettingsCopy.WindowSize);
			State.RmsSum = 0.0f;
			State.RmsNextWrite = 0;
			InOutResults.HighEnvelope = 0.0f;
			InOutResults.LowEnvelope = 0.0f;
			InOutResults.LevelDecibels = 0.0f;
			InOutResults.RequestReset = false;
			InOutResults.MonoPeakDecibels = -96.0;
			InOutResults.MonoPeakHighEnvelopeDecibels = -96.0f;
			State.Filter.ResetState();
		}

		// If the filter settings have changed, re-generate the coefficients
		if (State.FilterEnabled != SettingsCopy.FilterResult || State.FilterCutoff != SettingsCopy.FilterCutoff)
		{
			State.FilterCoefs.MakeFromSettings(
					FBiquadFilterSettings{ EBiquadFilterType::LowPass, SettingsCopy.FilterCutoff, 1.0f },
					SettingsCopy.SampleRate);
			State.Filter.ResetState();
			State.FilterEnabled = SettingsCopy.FilterResult;
			State.FilterCutoff = SettingsCopy.FilterCutoff;
		}

		const int32 NumChannels = InBuffer.GetNumValidChannels();
		State.ChannelPointers.Reset();
		for (int32 c = 0; c < NumChannels; ++c)
		{
			State.ChannelPointers.Emplace(InBuffer.GetStridingChannelDataPointer(c));
		}
		
		int32 NumFrames = InBuffer.GetNumValidFrames();
		float MonoPeak = -96.0f;
		float MonoPeakHighEnv = 0.0f;

		// process in chunks
		while (NumFrames > 0)
		{
			const int32 NumFramesThisPass = FMath::Min(NumFrames, AudioRendering::kFramesPerRenderBuffer);

			// Update the RMS
			for (int32 f = 0; f < NumFramesThisPass; ++f)
			{
				float Sum = 0.0f;
				for (int c = 0; c < NumChannels; ++c)
				{
					Sum += *State.ChannelPointers[c]++;
				}
				Sum /= NumChannels;
				Sum *= Sum;

				State.RmsSum -= State.RmsWindow[State.RmsNextWrite];
				State.RmsSum += Sum;
				State.RmsWindow[State.RmsNextWrite] = Sum;
				State.RmsNextWrite = (State.RmsNextWrite + 1) % SettingsCopy.WindowSize;
			}

			// Once per chunk, update the rest of the state
			if (NumFramesThisPass == AudioRendering::kFramesPerRenderBuffer)
			{
				if (State.RmsSum < 0.0f)
				{
					State.RmsSum = 0.0f;
				}
				
				State.EnvLast = FMath::Sqrt(State.RmsSum / SettingsCopy.WindowSize);
				check(FMath::IsFinite(State.EnvLast));

				if (State.HighLast > State.EnvLast)
				{
					State.HighLast = State.HighLast * (1.0f - SettingsCopy.HighFallingAlpha) + State.EnvLast * SettingsCopy.HighFallingAlpha;
				}
				else
				{
					State.HighLast = State.HighLast * (1.0f - SettingsCopy.HighRisingAlpha) + State.EnvLast * SettingsCopy.HighRisingAlpha;
				}

				const float FallingAlpha = FMath::Clamp(SettingsCopy.LowFallingAlpha * State.LowLast * 2.0f, 0.0f, 1.0f);

				if (!State.HasHadFirstSettle && State.LowLast < State.EnvLast)
				{
					State.LowLast = State.EnvLast;
				}
				else if (State.LowLast > State.EnvLast)
				{
					State.LowLast = State.LowLast * (1 - FallingAlpha) + State.EnvLast * FallingAlpha;
					State.HasHadFirstSettle = true;
				}
				else
				{
					State.LowLast = State.LowLast * (1 - SettingsCopy.LowRisingAlpha) + State.EnvLast * SettingsCopy.LowRisingAlpha;
				}

				InOutResults.HighEnvelope = State.HighLast;
				InOutResults.LowEnvelope = State.LowLast;

				float Top = State.HighLast;
				float Bottom = State.LowLast;
				if (Bottom <= 0.0f)
				{
					Top = Top - Bottom;
					Bottom = 0.0001;
				}

				if (SettingsCopy.FilterResult)
				{
					float NewSample = (Top < Bottom) ? 0.0f : FMath::Clamp(Top / Bottom / 1000.0f, 0.0f, 1.0f);
					State.Filter.Process(&NewSample, &NewSample, 1, State.FilterCoefs);
					NewSample = FMath::Clamp(NewSample * 1000.0f, 1.0f, 1000.0f);
					InOutResults.LevelDecibels = HarmonixDsp::LinearToDB(NewSample);
				}
				else
				{
					if (Top < Bottom)
					{
						InOutResults.LevelDecibels = 0.0f;
					}
					else
					{
						InOutResults.LevelDecibels = HarmonixDsp::LinearToDB(Top / Bottom);
					}
				}

				InOutResults.LevelDecibels = FMath::Clamp(InOutResults.LevelDecibels, 0.0f, 60.0f);
				MonoPeak = FMath::Max(MonoPeak, InOutResults.LevelDecibels);
				MonoPeakHighEnv = FMath::Max(MonoPeakHighEnv, InOutResults.HighEnvelope);
			}

			NumFrames -= NumFramesThisPass;
		}

		InOutResults.MonoPeakDecibels = FMath::Max(MonoPeak, InOutResults.MonoPeakDecibels);
		InOutResults.MonoPeakHighEnvelopeDecibels = FMath::Max(HarmonixDsp::LinearToDB(MonoPeakHighEnv), InOutResults.MonoPeakHighEnvelopeDecibels);
	}
}
