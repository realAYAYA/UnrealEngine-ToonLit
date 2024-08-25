// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/Generate.h"
#include "HarmonixDsp/AudioAnalysis/VuMeterAnalyzer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Harmonix::Dsp::AudioAnalysis::VuMeter::Tests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FHarmonixVuMeterBasicTest,
		"Harmonix.Dsp.AudioAnalysis.VuMeter.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FHarmonixVuMeterBasicTest::RunTest(const FString&)
	{
		constexpr int32 NumChannels = 2;
		constexpr int32 NumFramesPerBlock = 256;
		constexpr float SampleRate = 48000;
		
		FVuMeterAnalyzer VuMeterAnalyzer(SampleRate);

		// fill the buffer with a sine wave
		TAudioBuffer<float> Buffer{ NumChannels, NumFramesPerBlock, EAudioBufferCleanupMode::Delete };
		{
			constexpr float Frequency = 440;
			float Phase = 0;
			UTEST_TRUE("Test buffer must have at least one channel", Buffer.GetNumValidChannels() > 0);
			HarmonixDsp::GenerateSine(Buffer.GetValidChannelData(0), NumFramesPerBlock, Frequency, SampleRate, Phase);
			for (int32 c = 1; c < NumChannels; ++c)
			{
				FMemory::Memcpy(Buffer.GetValidChannelData(c), Buffer.GetValidChannelData(0), NumFramesPerBlock * sizeof(float));
			}
		}

		FHarmonixVuMeterAnalyzerResults Results;

		// results should be zero
		UTEST_EQUAL("Mono level is zero", Results.MonoValues.LevelMeanSquared, 0.0f);
		UTEST_EQUAL("Mono peak is zero", Results.MonoValues.PeakSquared, 0.0f);
		UTEST_EQUAL("There are no channel values", Results.ChannelValues.Num(), 0);
		
		VuMeterAnalyzer.Process(Buffer, Results);

		// make sure we got some results
		UTEST_TRUE("Mono level is greater than zero", Results.MonoValues.LevelMeanSquared > 0.0f);
		UTEST_TRUE("Mono peak is greater than zero", Results.MonoValues.PeakSquared > 0.0f);
		UTEST_EQUAL("Results have the same number of channels as the input audio", Results.ChannelValues.Num(), NumChannels);
		for (int32 c = 0; c < NumChannels; ++c)
		{
			UTEST_TRUE("Channel level is greater than zero", Results.ChannelValues[c].LevelMeanSquared > 0.0f);
			UTEST_TRUE("Channel peak is greater than zero", Results.ChannelValues[c].PeakSquared > 0.0f);
		}
		
		return true;
	}

}

#endif
