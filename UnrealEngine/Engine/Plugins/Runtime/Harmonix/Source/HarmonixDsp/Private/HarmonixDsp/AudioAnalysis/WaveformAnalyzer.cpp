// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/AudioAnalysis/WaveformAnalyzer.h"

namespace Harmonix::Dsp::AudioAnalysis
{
	FWaveformAnalyzer::FWaveformAnalyzer(const float InSampleRate) : SampleRate(InSampleRate)
	{
		Reset();
	}

	void FWaveformAnalyzer::SetSettings(const FHarmonixWaveformAnalyzerSettings& InSettings)
	{
		FScopeLock Lock{ &SettingsGuard };
		Settings = InSettings;
	}

	void FWaveformAnalyzer::Reset()
	{
		State = FState{};
	}

	void FWaveformAnalyzer::Process(const TAudioBuffer<float>& InBuffer, FHarmonixWaveformAnalyzerResults& InOutResults)
	{
		if (SampleRate <= 0)
		{
			return;
		}
		
		const int32 NumChannels = InBuffer.GetNumValidChannels();
		const int32 NumFrames = InBuffer.GetNumValidFrames();

		if (NumChannels < 1 || NumFrames < 1)
		{
			return;
		}

		FHarmonixWaveformAnalyzerSettings SettingsCopy;
		{
			FScopeLock Lock{ &SettingsGuard };
			SettingsCopy = Settings;
		}

		if (SettingsCopy.NumBinsPerSecond < 1)
		{
			return;
		}

		const int32 NumFramesPerBin = FMath::FloorToInt32(SampleRate / SettingsCopy.NumBinsPerSecond);
		const int32 NumFramesWithLeftover = NumFrames + State.NumLeftoverFrames;
		const int32 NumBinsThisBlock = FMath::FloorToInt32(static_cast<float>(NumFramesWithLeftover) / NumFramesPerBin);

		// Fix settings if necessary
		if (NumBinsThisBlock > SettingsCopy.NumBinsHeld)
		{
			SettingsCopy.NumBinsHeld = NumBinsThisBlock;
		}

		// Update the waveform arrays if needed
		InOutResults.WaveformRaw.SetNumZeroed(SettingsCopy.NumBinsHeld);
		InOutResults.WaveformSmoothed.SetNumZeroed(SettingsCopy.NumBinsHeld);

		// Shift the old bins (oldest last)
		if (const int32 NumBinsToShift = SettingsCopy.NumBinsHeld - NumBinsThisBlock; NumBinsToShift > 0)
		{
			FMemory::Memmove(
				InOutResults.WaveformRaw.GetData() + NumBinsThisBlock,
				InOutResults.WaveformRaw.GetData(),
				NumBinsToShift * sizeof(float));
			FMemory::Memmove(
				InOutResults.WaveformSmoothed.GetData() + NumBinsThisBlock,
				InOutResults.WaveformSmoothed.GetData(),
				NumBinsToShift * sizeof(float));
		}
		
		// If there were some leftover frames from the last block, finish that bin before moving on.
		int32 BinIdx = NumBinsThisBlock - 1;
		int32 FrameIdx = 0;
		
		if (State.NumLeftoverFrames > 0)
		{
			InOutResults.WaveformRaw[BinIdx] = State.LeftoverBinSumSquared;
			const int32 NumFramesToFill = NumFramesPerBin - State.NumLeftoverFrames;
			const int32 EndFrame = FrameIdx + NumFramesToFill;

			// Reset the leftover counts
			State = FState{};

			for (int32 c = 0; c < NumChannels; ++c)
			{
				TDynamicStridePtr<float> ChannelPtr = InBuffer.GetStridingChannelDataPointer(c);

				for (int32 f = FrameIdx; f < EndFrame; ++f)
				{
					const float Sample = ChannelPtr[f];
					InOutResults.WaveformRaw[BinIdx] += Sample * Sample;
				}
			}

			InOutResults.WaveformRaw[BinIdx] = FMath::Sqrt(InOutResults.WaveformRaw[BinIdx] / NumFramesPerBin);
			
			--BinIdx;
			FrameIdx = EndFrame;
		}
		

		// Get the un-smoothed, binned waveform which is an average of all channels for a chunk of frames
		while (BinIdx >= 0)
		{
			InOutResults.WaveformRaw[BinIdx] = 0;
			const int32 StartFrame = FrameIdx;
			const int32 EndFrame = StartFrame + NumFramesPerBin;
			
			for (int32 c = 0; c < NumChannels; ++c)
			{
				TDynamicStridePtr<float> ChannelPtr = InBuffer.GetStridingChannelDataPointer(c);

				for (int32 f = StartFrame; f < EndFrame; ++f)
				{
					const float Sample = ChannelPtr[f];
					InOutResults.WaveformRaw[BinIdx] += Sample * Sample;
				}
			}

			InOutResults.WaveformRaw[BinIdx] = FMath::Sqrt(InOutResults.WaveformRaw[BinIdx] / NumFramesPerBin);
			
			FrameIdx = EndFrame;
			--BinIdx;
		}

		// If there is an incomplete bin, record what we have and save it for next time
		State.NumLeftoverFrames = NumFramesWithLeftover % NumFramesPerBin;

		if (State.NumLeftoverFrames > 0)
		{
			State.LeftoverBinSumSquared = 0;
			const int32 StartFrame = FrameIdx;
			
			for (int32 c = 0; c < NumChannels; ++c)
			{
				TDynamicStridePtr<float> ChannelPtr = InBuffer.GetStridingChannelDataPointer(c);

				for (int32 f = StartFrame; f < NumFrames; ++f)
				{
					const float Sample = ChannelPtr[f];
					State.LeftoverBinSumSquared += Sample * Sample;
				}
			}
		}

		// Calculate the smoothed waveform
		// Smoothing has a distance in bins in both directions and a factor which is a weight for the incoming raw values
		if (SettingsCopy.SmoothingDistance > 0 && SettingsCopy.SmoothingFactor > 0)
		{
			for (int32 SmoothedIdx = 0; SmoothedIdx <= NumBinsThisBlock + SettingsCopy.SmoothingDistance; ++SmoothedIdx)
			{
				if (SmoothedIdx < 0 || SmoothedIdx >= InOutResults.WaveformSmoothed.Num())
				{
					continue;
				}
			
				const int32 RawStartIdx = SmoothedIdx - SettingsCopy.SmoothingDistance;
				const int32 RawEndIdx = SmoothedIdx + SettingsCopy.SmoothingDistance;
				float Sum = 0;
				float NumSmoothed = 0;

				for (int32 RawIdx = RawStartIdx; RawIdx <= RawEndIdx; ++RawIdx)
				{
					if (RawIdx < 0 || RawIdx >= InOutResults.WaveformRaw.Num())
					{
						continue;
					}

					const float RawSample = InOutResults.WaveformRaw[RawIdx];

					if (SmoothedIdx == RawIdx)
					{
						Sum += RawSample;
						NumSmoothed += 1;
					}
					else
					{
						Sum += RawSample * SettingsCopy.SmoothingFactor;
						NumSmoothed += SettingsCopy.SmoothingFactor;
					}
				}

				InOutResults.WaveformSmoothed[SmoothedIdx] = NumSmoothed > 0.0f ? Sum / NumSmoothed : Sum;
			}
		}
		// If the settings are such that there would be no smoothing, just copy the raw buffer
		else
		{
			FMemory::Memcpy(
				InOutResults.WaveformSmoothed.GetData(),
				InOutResults.WaveformRaw.GetData(),
				InOutResults.WaveformSmoothed.Num() * sizeof(float));
		}
	}
}
