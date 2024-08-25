// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/MidiOps/PulseGenerator.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Harmonix::Midi::Ops::Tests
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiPulseGeneratorBasicTest,
		"Harmonix.Midi.Ops.PulseGenerator.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FMidiPulseGeneratorBasicTest::RunTest(const FString&)
	{
		FPulseGenerator PulseGenerator;
		
		Metasound::FOperatorSettings OperatorSettings{ 48000, 100 };

		constexpr float Tempo = 123;
		constexpr uint8 TimeSigNumerator = 4;
		constexpr uint8 TimeSigDenominator = 4;
		const auto Clock = MakeShared<HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe>(OperatorSettings);
		Clock->AttachToMidiResource(HarmonixMetasound::FMidiClock::MakeClockConductorMidiData(Tempo, TimeSigNumerator, TimeSigDenominator));
		
		HarmonixMetasound::FMidiStream OutputStream;

		PulseGenerator.SetClock(Clock);

		// Default: a pulse every beat
		{
			constexpr int32 NotesUntilWeAreSatisfiedThisWorks = 23;
			int32 NumNotesReceived = 0;

			Clock->ResetAndStart(0);

			FTimeSignature TimeSignature = Clock->GetBarMap().GetTimeSignatureAtTick(0);
			FMusicTimeInterval Interval = PulseGenerator.GetInterval();
			FMusicTimestamp NextPulse{ 1, 1 };
			IncrementTimestampByOffset(NextPulse, Interval, TimeSignature);

			int32 EndTick = 0;
			{
				FMusicTimestamp EndTimestamp{ NextPulse };
				for (int i = 0; i < NotesUntilWeAreSatisfiedThisWorks; ++i)
				{
					IncrementTimestampByInterval(EndTimestamp, Interval, TimeSignature);
				}
				EndTick = Clock->GetBarMap().MusicTimestampToTick(EndTimestamp);
			}
			
			while (Clock->GetCurrentMidiTick() < EndTick)
			{
				// Advance the clock, which will advance the play cursor in the pulse generator
				Clock->PrepareBlock();
				Clock->WriteAdvance(0, OperatorSettings.GetNumFramesPerBlock());

				// Process, which will pop the next notes
				OutputStream.PrepareBlock();
				PulseGenerator.Process(OutputStream);
				
				// If this is a block where we should get a pulse, check that we got it
				if (Clock->GetCurrentMidiTick() >= Clock->GetBarMap().MusicTimestampToTick(NextPulse))
				{
					const bool ShouldGetNoteOff = NumNotesReceived > 0;

					bool GotNoteOn = false;
					bool GotNoteOff = false;

					const TArray<HarmonixMetasound::FMidiStreamEvent> Events = OutputStream.GetEventsInBlock();

					if (Events.Num() != (ShouldGetNoteOff ? 2 : 1))
					{
						return false;
					}

					UTEST_EQUAL("Got the right number of events", Events.Num(), ShouldGetNoteOff ? 2 : 1);

					for (const HarmonixMetasound::FMidiStreamEvent& Event : Events)
					{
						if (Event.MidiMessage.IsNoteOn())
						{
							GotNoteOn = true;

							UTEST_EQUAL("Right track", Event.TrackIndex, PulseGenerator.Track);
							UTEST_EQUAL("Right channel", Event.MidiMessage.GetStdChannel(), PulseGenerator.Channel - 1);
							UTEST_EQUAL("Right note number", Event.MidiMessage.GetStdData1(), PulseGenerator.NoteNumber);
							UTEST_EQUAL("Right velocity", Event.MidiMessage.GetStdData2(), PulseGenerator.Velocity);

							++NumNotesReceived;
						}
						else if (Event.MidiMessage.IsNoteOff())
						{
							GotNoteOff = true;

							UTEST_EQUAL("Right track", Event.TrackIndex, PulseGenerator.Track);
							UTEST_EQUAL("Right channel", Event.MidiMessage.GetStdChannel(), PulseGenerator.Channel - 1);
							UTEST_EQUAL("Right note number", Event.MidiMessage.GetStdData1(), PulseGenerator.NoteNumber);
						}
						else
						{
							UTEST_TRUE("Unexpected event", false);
						}
					}

					UTEST_TRUE("Got note on", GotNoteOn);

					if (ShouldGetNoteOff)
					{
						UTEST_TRUE("Got note off", GotNoteOff);
					}
					else
					{
						UTEST_FALSE("Did not get note off", GotNoteOff);
					}

					IncrementTimestampByInterval(NextPulse, Interval, TimeSignature);
				}
			}
			
			UTEST_TRUE("Got all the notes at the right time", NumNotesReceived >= NotesUntilWeAreSatisfiedThisWorks);
		}

		return true;
	}
}

#endif