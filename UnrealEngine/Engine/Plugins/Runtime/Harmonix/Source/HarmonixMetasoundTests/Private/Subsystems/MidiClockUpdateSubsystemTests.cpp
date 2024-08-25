// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/Engine.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiClockUpdateSubsystem
{
	namespace Helpers
	{
		HarmonixMetasound::FMidiClock MakeAndStartClock(
			const Metasound::FOperatorSettings& OperatorSettings,
			float Tempo,
			int32 TimeSigNum,
			int32 TimeSigDenom)
		{
			const TSharedPtr<FMidiFileData> MidiData = MakeShared<FMidiFileData>();
			check(MidiData);
			
			MidiData->Tracks.Add(FMidiTrack(TEXT("conductor")));
			MidiData->Tracks[0].AddEvent(FMidiEvent(0, FMidiMsg(static_cast<uint8>(TimeSigNum), static_cast<uint8>(TimeSigDenom))));
			FBarMap& BarMap = MidiData->SongMaps.GetBarMap();
			BarMap.AddTimeSignatureAtBarIncludingCountIn(0, TimeSigNum, TimeSigDenom);
			const int32 MidiTempo = Harmonix::Midi::Constants::BPMToMidiTempo(Tempo);
			MidiData->Tracks[0].AddEvent(FMidiEvent(0, FMidiMsg(MidiTempo)));
			FTempoMap& TempoMap = MidiData->SongMaps.GetTempoMap();
			TempoMap.AddTempoInfoPoint(MidiTempo, 0);
			MidiData->Tracks[0].Sort();
			MidiData->ConformToLength(std::numeric_limits<int32>::max());

			HarmonixMetasound::FMidiClock Clock{ OperatorSettings };
			Clock.AttachToMidiResource(MidiData);
			Clock.ResetAndStart(0);

			return Clock;
		}	
	}
	
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMidiClockUpdateSubsystemBasicTest,
	"Harmonix.Metasound.Subsystems.MidiClockUpdateSubsystem.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FMidiClockUpdateSubsystemBasicTest::RunTest(const FString&)
	{
		using namespace HarmonixMetasound;

		UTEST_NOT_NULL("GEngine exists", GEngine);
		UMidiClockUpdateSubsystem* Subsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>();
		UTEST_NOT_NULL("Subsystem exists", Subsystem);

		constexpr float Tempo = 95;
		constexpr int32 TimeSigNum = 3;
		constexpr int32 TimeSigDenom = 4;
		Metasound::FOperatorSettings OperatorSettings { 48000, 100 };
		
		FMidiClock Clock = Helpers::MakeAndStartClock(OperatorSettings, Tempo, TimeSigNum, TimeSigDenom);

		// Update the clock and tick the subsystem a few times to make sure we're updating the low-r clock
		constexpr int32 NumIterations = 100;
		const int32 NumSamples = OperatorSettings.GetNumFramesPerBlock();
		int32 SampleRemainder = 0;
		int32 SampleCount = 0;
		
		for (int32 i = 0; i < NumIterations; ++i)
		{
			// Advance the high-resolution clock
			SampleRemainder += NumSamples;
			constexpr int32 MidiGranularity = 128;
			while (SampleRemainder >= MidiGranularity)
			{
				SampleCount += MidiGranularity;
				SampleRemainder -= MidiGranularity;
				const float AdvanceToMs = static_cast<float>(SampleCount) * 1000.0f / OperatorSettings.GetSampleRate();
				Clock.AdvanceHiResToMs(0, AdvanceToMs, true);
			}

			// Tick the subsystem (low-resolution clocks)
			Subsystem->TickForTesting();

			// Check that the high- and low-resolution clocks are at the same place
			UTEST_EQUAL(
				FString::Printf(TEXT("High- and low-res are at the same tick: iteration %i"), i),
				Clock.GetCurrentHiResTick(),
				Clock.GetCurrentLowResTick());
		}

		return true;
	}
}

#endif