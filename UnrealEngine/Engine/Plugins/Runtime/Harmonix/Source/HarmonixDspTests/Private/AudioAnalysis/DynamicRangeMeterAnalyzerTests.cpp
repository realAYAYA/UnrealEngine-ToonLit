// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/Generate.h"
#include "HarmonixDsp/AudioAnalysis/DynamicRangeMeterAnalyzer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Harmonix::Dsp::AudioAnalysis::VuMeter::Tests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FHarmonixDynamicRangeMeterBasicTest,
		"Harmonix.Dsp.AudioAnalysis.DynamicRangeMeter.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FHarmonixDynamicRangeMeterBasicTest::RunTest(const FString&)
	{
		constexpr int32 NumChannels = 2;
		constexpr int32 NumFramesPerBlock = 256;
		constexpr float SampleRate = 48000;
		
		FDynamicRangeMeterAnalyzer Analyzer;
		FDynamicRangeMeterAnalyzer::FSettings Settings;
		Settings.SampleRate = SampleRate;
		Analyzer.SetSettings(Settings);

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

		FDynamicRangeMeterAnalyzer::FResults Results;
		
		Analyzer.Process(Buffer, Results);

		// make sure we got some results
		UTEST_TRUE("High envelope is greater than zero", Results.HighEnvelope > 0.0f);
		UTEST_TRUE("Low envelope is greater than zero", Results.LowEnvelope > 0.0f);
		UTEST_TRUE("Level is greater than -96Db", Results.LevelDecibels > -96.0f);
		UTEST_TRUE("Mono peak is greater than -96Db", Results.MonoPeakDecibels > -96.0f);
		UTEST_TRUE("Mono peak high envelope is greater than -96Db", Results.MonoPeakHighEnvelopeDecibels > -96.0f);
		
		return true;
	}

}

#endif
