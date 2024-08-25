// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/AudioAnalysis/WaveformAnalyzer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Harmonix::Dsp::AudioAnalysis::WaveformAnalyzer::Tests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FHarmonixWaveformAnalyzerBasicTest,
		"Harmonix.Dsp.AudioAnalysis.Waveform.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FHarmonixWaveformAnalyzerBasicTest::RunTest(const FString&)
	{
		constexpr int32 NumChannels = 2;
		constexpr int32 NumFramesPerBlock = 256;
		constexpr float SampleRate = 48000;
		
		FWaveformAnalyzer Analyzer{ SampleRate };
		constexpr FHarmonixWaveformAnalyzerSettings Settings;
		Analyzer.SetSettings(Settings);
		
		FHarmonixWaveformAnalyzerResults Results;

		// process a few times so we have some data
		TAudioBuffer<float> Buffer{ NumChannels, NumFramesPerBlock, EAudioBufferCleanupMode::Delete };
		constexpr float Frequency = 440;
		float Phase = 0;
		constexpr int32 NumBlocks = 10;
		for (int i = 0; i < NumBlocks; ++i)
		{
			// Fill the buffer with a sine
			HarmonixDsp::GenerateSine(Buffer.GetValidChannelData(0), NumFramesPerBlock, Frequency, SampleRate, Phase);
			for (int32 c = 1; c < NumChannels; ++c)
			{
				FMemory::Memcpy(Buffer.GetValidChannelData(c), Buffer.GetValidChannelData(0), NumFramesPerBlock * sizeof(float));
			}
			Analyzer.Process(Buffer, Results);
		}
		
		// make sure we got some results
		UTEST_EQUAL("Got the right number of raw bins", Results.WaveformRaw.Num(), Settings.NumBinsHeld);
		UTEST_EQUAL("Got the right number of smoothed bins", Results.WaveformSmoothed.Num(), Settings.NumBinsHeld);

		bool ABinWasNonZero = false;
		
		for (int32 BinIdx = 0; BinIdx < Settings.NumBinsHeld; ++BinIdx)
		{
			UTEST_EQUAL(
				FString::Printf(TEXT("Samples match at bin index %i"), BinIdx),
				Results.WaveformRaw[BinIdx],
				Results.WaveformSmoothed[BinIdx]);
			if (!FMath::IsNearlyZero(Results.WaveformRaw[BinIdx]))
			{
				ABinWasNonZero = true;
			}
		}

		UTEST_TRUE("A bin was non-zero", ABinWasNonZero);
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FHarmonixWaveformAnalyzerCorrectRawValuesTest,
		"Harmonix.Dsp.AudioAnalysis.Waveform.CorrectRawValues",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FHarmonixWaveformAnalyzerCorrectRawValuesTest::RunTest(const FString&)
	{
		constexpr int32 NumFramesPerBlock = 16;
		constexpr float SampleRate = 48000;
		
		FWaveformAnalyzer Analyzer{ SampleRate };
		FHarmonixWaveformAnalyzerSettings Settings;
		Settings.NumBinsPerSecond = SampleRate; // this makes it so we get the waveform out the other side
		Settings.NumBinsHeld = NumFramesPerBlock; // this makes it so we just hold a block of samples
		Analyzer.SetSettings(Settings);
		
		FHarmonixWaveformAnalyzerResults Results;
		
		// Process a block with a sine
		TAudioBuffer<float> Buffer{ 1, NumFramesPerBlock, EAudioBufferCleanupMode::Delete };
		constexpr float Frequency = 440;
		float Phase = 0;
		HarmonixDsp::GenerateSine(Buffer.GetValidChannelData(0), NumFramesPerBlock, Frequency, SampleRate, Phase);
		Analyzer.Process(Buffer, Results);
		
		// make sure we got some results
		UTEST_EQUAL("Got the right number of raw bins", Results.WaveformRaw.Num(), Settings.NumBinsHeld);
		UTEST_EQUAL("Got the right number of smoothed bins", Results.WaveformSmoothed.Num(), Settings.NumBinsHeld);

		TDynamicStridePtr<float> InputPtr = Buffer.GetStridingChannelDataPointer(0);
		
		for (int32 BinIdx = 0; BinIdx < Settings.NumBinsHeld; ++BinIdx)
		{
			// no smoothing, so we should have the same values in both arrays
			UTEST_EQUAL(
				FString::Printf(TEXT("Samples match at bin index %i"), BinIdx),
				Results.WaveformRaw[BinIdx],
				Results.WaveformSmoothed[BinIdx]);
			// make sure the input buffer matches the output buffer
			// The history is in reverse, so make sure to check opposite sides of the buffer
			const int32 InputIdx = (Settings.NumBinsHeld - BinIdx) - 1;
			UTEST_EQUAL(
				FString::Printf(TEXT("Output == Input at index %i"), BinIdx),
				Results.WaveformRaw[BinIdx],
				InputPtr[InputIdx]);
		}
		
		return true;
	}
}

#endif
