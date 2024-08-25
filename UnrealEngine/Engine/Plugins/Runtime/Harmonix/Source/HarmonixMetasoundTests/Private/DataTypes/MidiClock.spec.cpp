// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiClock.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiClock
{
	using namespace HarmonixMetasound;
	BEGIN_DEFINE_SPEC(
		FHarmonixMetasoundMidiClockSpec,
		"Harmonix.Metasound.DataTypes.MidiClock",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TUniquePtr<FMidiClock> TestClock;
	TUniquePtr<FMidiClock> DrivingClock;
	Metasound::FOperatorSettings OperatorSettings {48000, 100};

	void AddStateAtFrame(EMusicPlayerTransportState State, int32 Frame) const
	{
		const FMidiTimestampTransportState NewState
		{
			Frame,
			0,
			State
		};

		check(TestClock.IsValid());
		TestClock->AddTransportStateChangeToBlock(NewState);
	}

	void ExecuteWriteAdvance(int32 StartFrameIndex, int32 EndFrameIndex, float Speed)
	{
		TestClock->SeekTo(0, 1, 0);
		TestClock->PrepareBlock();

		TestEqual("Clock.GetCurrentHiResMs()", TestClock->GetCurrentHiResMs(), 0.0f);
		TestEqual("Clock.GetCurrentMidiTick()", TestClock->GetCurrentMidiTick(), 0);
		
		float OldMs = TestClock->GetCurrentHiResMs();
		int32 OldTick = TestClock->GetCurrentMidiTick();
		TestClock->WriteAdvance(StartFrameIndex, EndFrameIndex, Speed);
		float NewMs = TestClock->GetCurrentHiResMs();
		int32 NewTick = TestClock->GetCurrentMidiTick();

		TestTrue("Non looping clock advanced forward in time", NewMs > OldMs);
		TestTrue("Non looping Clock Advanced forward in ticks", NewTick > OldTick);

		int32 DeltaFrames = FMidiClock::kMidiGranularity * Speed;
		float DeltaMs = DeltaFrames * 1000.0f / OperatorSettings.GetSampleRate();
		float Ms = OldMs;

		int32 BlockFrameIndex = 0; 
		for (const FMidiClockEvent& Event : TestClock->GetMidiClockEventsInBlock())
		{
			int32 Tick1 = (int32)(TestClock->GetTempoMap().MsToTick(Ms) + 0.5f);
			Ms += DeltaMs;
			int32 Tick2 = (int32)(TestClock->GetTempoMap().MsToTick(Ms) + 0.5f);

			TestEqual(FString::Printf(TEXT("Frame-%d: Event.BlockFrameIndex"), Event.BlockFrameIndex), Event.BlockFrameIndex, BlockFrameIndex);
			TestEqual(FString::Printf(TEXT("Frame-%d: Event.Type"), Event.BlockFrameIndex), Event.Msg.Type, FMidiClockMsg::EType::AdvanceThru);
			TestEqual(FString::Printf(TEXT("Frame-%d: Event.FromTick"), Event.BlockFrameIndex), Event.Msg.FromTick(), Tick1);
			TestEqual(FString::Printf(TEXT("Frame-%d: Event.ThruTick"), Event.BlockFrameIndex), Event.Msg.ThruTick(), Tick2);
			TestEqual(FString::Printf(TEXT("Frame-%d: Event.IsPreRoll"), Event.BlockFrameIndex), Event.Msg.AsAdvanceThru().IsPreRoll, false);
			
			BlockFrameIndex += FMidiClock::kMidiGranularity;
		}

		int32 Tick = (int32)(TestClock->GetTempoMap().MsToTick(Ms) + 0.5f);
		TestEqual("Clock.GetCurrentMidiTick()", TestClock->GetCurrentMidiTick(), Tick);
		TestEqual("Clock.GetCurrentHiResMs()", TestClock->GetCurrentHiResMs(), Ms);
	};
	
	END_DEFINE_SPEC(FHarmonixMetasoundMidiClockSpec)

	void FHarmonixMetasoundMidiClockSpec::Define()
	{
		BeforeEach([this]()
		{
			TestClock = MakeUnique<FMidiClock>(OperatorSettings);
		});

		AfterEach([this]()
		{
			TestClock.Reset();
		});
		
		Describe("AddTransportStateChangeToBlock(NewState)", [this]()
		{
			It("should always add NewState if there are no changes in the block", [this]()
			{
				TestClock->PrepareBlock();

				TestFalse("No transport changes in block", TestClock->HasTransportStateChangeInBlock());

				constexpr EMusicPlayerTransportState NewState = EMusicPlayerTransportState::Playing;
				AddStateAtFrame(NewState, 0);

				TestTrue("There is a transport state change in block", TestClock->HasTransportStateChangeInBlock());
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("should add NewState if its frame is greater than the last one in the block", [this]()
			{
				AddStateAtFrame(EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr EMusicPlayerTransportState NewState = EMusicPlayerTransportState::Pausing;
				AddStateAtFrame(NewState, LastStateFrame + 1);

				TestEqual("There is another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates + 1);
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("should add NewState if it has the same frame as the last one in the block", [this]()
			{
				AddStateAtFrame(EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr EMusicPlayerTransportState NewState = EMusicPlayerTransportState::Paused;
				AddStateAtFrame(NewState, LastStateFrame );

				TestEqual("There is another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates + 1);
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("should not add NewState if the frame is less than the last one in the block", [this]()
			{
				AddStateAtFrame(EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const EMusicPlayerTransportState LastState = TestClock->GetTransportStateAtEndOfBlock();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr EMusicPlayerTransportState NewState = EMusicPlayerTransportState::Continuing;
				AddStateAtFrame(NewState, LastStateFrame - 1);

				TestEqual("There is not another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates);
				TestEqual("State at end of block matches the one that was already there", TestClock->GetTransportStateAtEndOfBlock(), LastState);
			});
		});

		Describe("PrepareBlock()", [this]()
		{
			It("should reset transport, speed, tempo, and clock states", [this]()
			{
				const float LastTempo = TestClock->GetTempoAtEndOfBlock();
				const float LastSpeed = TestClock->GetSpeedAtEndOfBlock();
				TestClock->PrepareBlock();
				TestEqual("Clock.TransportChangesInBlock.Num()", TestClock->GetTransportTimestampsInBlock().Num(), 0);
				TestEqual("Clock.SpeedChangesInBlock.Num()", TestClock->GetTempoSpeedTimestampsInBlock().Num(), 1);
				TestEqual("Clock.SpeedAtBlockSampleFrame(0)", TestClock->GetSpeedAtBlockSampleFrame(0), LastSpeed);
				TestEqual("Clock.SpeedAtEndOfBlock()", TestClock->GetSpeedAtEndOfBlock(), LastSpeed);
				TestFalse("Clock.HasSpeedChangesInBlock()", TestClock->HasSpeedChangesInBlock());
				TestEqual("Clock.TempoChangesInBlock.Num()", TestClock->GetNumTempoChangesInBlock(), 1);
				TestEqual("Clock.TempoAtBlockSampleFrame(0)", TestClock->GetTempoAtBlockSampleFrame(0), LastTempo);
				TestEqual("Clock.GetTempoAtEndOfBlock()", TestClock->GetTempoAtEndOfBlock(), LastTempo);
				TestFalse("Clock.HasTempoChangesInBlock()", TestClock->HasTempoChangesInBlock());
				TestEqual("Clock.GetMidiClockEventsInBlock().Num()", TestClock->GetMidiClockEventsInBlock().Num(), 0);
			});
		});

		Describe("ResetAndStart()", [this]()
		{
			It("should reset speed and tempo changes, and be \"playing\"", [this]()
			{
				const float LastTempo = TestClock->GetTempoAtEndOfBlock();
				const float LastSpeed = TestClock->GetSpeedAtEndOfBlock();
				TestClock->ResetAndStart(0, false);
				TestEqual("Clock.GetTransportStateAtEndOfBlock()", TestClock->GetTransportStateAtEndOfBlock(), EMusicPlayerTransportState::Playing);
				TestEqual("Clock.SpeedAtBlockSampleFrame(0)", TestClock->GetSpeedAtBlockSampleFrame(0), LastSpeed);
				TestEqual("Clock.SpeedAtEndOfBlock()", TestClock->GetSpeedAtEndOfBlock(), LastSpeed);
				TestFalse("Clock.HasSpeedChangesInBlock()", TestClock->HasSpeedChangesInBlock());
				TestEqual("Clock.TempoChangesInBlock.Num()", TestClock->GetNumTempoChangesInBlock(), 1);
				TestEqual("Clock.TempoAtBlockSampleFrame(0)", TestClock->GetTempoAtBlockSampleFrame(0), LastTempo);
				TestEqual("Clock.GetTempoAtEndOfBlock()", TestClock->GetTempoAtEndOfBlock(), LastTempo);
				TestFalse("Clock.HasTempoChangesInBlock()", TestClock->HasTempoChangesInBlock());
			});
			
			It("should be at the correct block frame index", [this]()
			{
				const float LastTempo = TestClock->GetTempoAtEndOfBlock();
				const float LastSpeed = TestClock->GetSpeedAtEndOfBlock();
				int32 BlockFrame = 100;
				TestClock->ResetAndStart(BlockFrame, false);
				TestEqual("Clock.Get", TestClock->GetCurrentBlockFrameIndex(), BlockFrame);
			});
			
			It("should seek to start when requested", [this]()
			{
				int32 BlockFrame = 100;
				FMusicSeekTarget SeekTarget;
				SeekTarget.Type = ESeekPointType::BarBeat;
				SeekTarget.BarBeat = FMusicTimestamp(2, 1.0f);
				TestClock->SeekTo(SeekTarget, 0);
				TestFalse("Clock seeked, Clock.GetCurrentMidiTick() == 0", TestClock->GetCurrentMidiTick() == 0);
				
				TestClock->ResetAndStart(BlockFrame, true);
				TestEqual("Clock.GetCurrentBlockFrameIndex()", TestClock->GetCurrentBlockFrameIndex(), BlockFrame);
				TestEqual("Clock.GetCurrentMidiTick()", TestClock->GetCurrentMidiTick(), -1);
			});

			It("should otherwise not seek to start", [this]()
			{
				int32 BlockFrame = 100;
				FMusicSeekTarget SeekTarget;
				SeekTarget.Type = ESeekPointType::BarBeat;
				SeekTarget.BarBeat = FMusicTimestamp(2, 1.0f);
				TestClock->SeekTo(SeekTarget, 0);
				int32 NewMidiTick = TestClock->GetCurrentMidiTick();
				TestFalse("Clock seeked, Clock.GetCurrentMidiTick() == 0", TestClock->GetCurrentMidiTick() == 0);
							
				TestClock->ResetAndStart(BlockFrame, false);
				TestEqual("Clock.GetCurrentBlockFrameIndex()", TestClock->GetCurrentBlockFrameIndex(), BlockFrame);
				TestEqual("Clock.GetCurrentMidiTick()", TestClock->GetCurrentMidiTick(), NewMidiTick);
			});
			
		});

		Describe("SetLoop()", [this]()
		{
			It("should correctly set the tempo", [this]()
			{
				const float TicksPerQuarterNote = Harmonix::Midi::Constants::GTicksPerQuarterNote;
				const float TempoBPM = 120.0f;
				int32 LoopStartTick = 0;
				int32 LoopEndTick = 1000;

				// initial clock tempo
				TestEqual("Clock.Tempo", TestClock->GetTempoAtEndOfBlock(), TempoBPM);
				TestEqual("Clock.Tempo", TestClock->GetTempoMap().GetTempoAtTick(0), TempoBPM);

				// microseconds per quarternote
				int32 MidiTempo = Harmonix::Midi::Constants::BPMToMidiTempo(TempoBPM);
							
				float MsPerTick = MidiTempo / TicksPerQuarterNote / 1000.0f;
				float LoopStartMs = MsPerTick * LoopStartTick;
				float LoopEndMs = MsPerTick * LoopEndTick;

				TestFalse("Initial -> Clock.DoesLoop()", TestClock->DoesLoop());
				TestClock->SetLoop(LoopStartTick, LoopEndTick);
				TestTrue("SetLoop -> Clock.DoesLoop()", TestClock->DoesLoop());
				TestEqual("Clock.LoopStartTick()", TestClock->GetLoopStartTick(), LoopStartTick);
				TestEqual("Clock.LoopEndTick()", TestClock->GetLoopEndTick(), LoopEndTick);
				TestEqual("Clock.LoopStartMs()", TestClock->GetLoopStartMs(), LoopStartMs);
				TestEqual("Clock.LoopEndMs()", TestClock->GetLoopEndMs(), LoopEndMs);

				TestClock->ClearLoop();
				TestFalse("Cleared -> Clock.DoesLoop()", TestClock->DoesLoop());
			});

		});

		Describe("WriteAdvance", [this]()
		{
			It("should advance one block correctly with speed 1", [this]
			{
				int32 StartFrame = 0;
				int32 EndFrame = StartFrame + OperatorSettings.GetNumFramesPerBlock();
				float Speed = 1.0f;
				ExecuteWriteAdvance(0, EndFrame, Speed);
			});

			It("should advance one block correctly with speed 2", [this]
			{
				int32 StartFrame = 0;
				int32 EndFrame = StartFrame + OperatorSettings.GetNumFramesPerBlock();
				float Speed = 2.0f;
				ExecuteWriteAdvance(0, EndFrame, Speed);
			});
			
			It("should advance one block correctly with speed 1/2", [this]
			{
				int32 StartFrame = 0;
				int32 EndFrame = StartFrame + OperatorSettings.GetNumFramesPerBlock();
				float Speed = 0.5f;
				ExecuteWriteAdvance(0, EndFrame, Speed);
			});
			
		});

		Describe("ProcessClockEvent(EventType)", [this]()
		{
			BeforeEach([&, this]
			{
				DrivingClock = MakeUnique<FMidiClock>(OperatorSettings);
				TestClock->AttachToTimeAuthority(*DrivingClock);
			});

			AfterEach([&, this]
			{
				TestClock->ClearLoop();
				TestClock->DetachFromTimeAuthority();
				DrivingClock.Reset();
			});

			It("EventType::AdvanceThru.NonLooping", [&, this]
			{
				float DrivingClockSpeed = 1.0f;
				int32 StartFrame = 0;
				int32 DeltaFrames = FMidiClock::kMidiGranularity * DrivingClockSpeed;
				float DeltaMs = DeltaFrames * 1000.0f / OperatorSettings.GetSampleRate();
				int32 Tick = (int32)(TestClock->GetTempoMap().MsToTick(DeltaMs) + 0.5f);

				float Speed = 1.0f;
				int32 PrerollBars = 8;
				FMidiClockEvent Event = FMidiClockEvent(StartFrame, FMidiClockMsg::FAdvanceThru(0, Tick, false));

				TestClock->PrepareBlock();
				int32 OldEventsNum = TestClock->GetMidiClockEventsInBlock().Num();
				TestClock->HandleClockEvent(*DrivingClock, Event, PrerollBars, Speed);

				if (!TestTrue("Clock has new clock events", TestClock->GetMidiClockEventsInBlock().Num() > OldEventsNum))
				{
					return;
				}
				TestEqual("Last Clock Event in block", TestClock->GetMidiClockEventsInBlock().Last().Msg.Type, FMidiClockMsg::EType::AdvanceThru);
				TestEqual("Clock Current Tick", TestClock->GetCurrentMidiTick(), Event.Msg.ThruTick() - 1);
			});

			It("EventType::AdvanceThru.Looping", [&, this]
			{
				FMusicTimestamp LoopEndTimestamp;
				LoopEndTimestamp.Bar = 2;
				LoopEndTimestamp.Beat = 1.0f;
				
				int32 LoopStartTick = 0;
				int32 LoopEndTick = TestClock->GetBarMap().MusicTimestampToTick(LoopEndTimestamp);
				TestClock->SetLoop(LoopStartTick, LoopEndTick);
				TestClock->SeekTo(0, LoopEndTick - 1, 0);
				
				float DrivingClockSpeed = 1.0f;
				int32 StartFrame = 0;
				int32 DeltaFrames = FMidiClock::kMidiGranularity * DrivingClockSpeed;
				float DeltaMs = DeltaFrames * 1000.0f / OperatorSettings.GetSampleRate();
				int32 DeltaTicks = (int32)(TestClock->GetTempoMap().MsToTick(DeltaMs) + 0.5f);
				int32 Tick = DeltaTicks;

				int32 ExpectedTick = TestClock->GetCurrentMidiTick() + DeltaTicks + 1;
				if (ExpectedTick > LoopEndTick)
				{
					ExpectedTick = ExpectedTick - LoopEndTick + LoopStartTick;
				}

				float Speed = 1.0f;
				int32 PrerollBars = 8;
				FMidiClockEvent Event = FMidiClockEvent(StartFrame, FMidiClockMsg::FAdvanceThru(0, Tick, false));

				TestClock->PrepareBlock();
				int32 OldEventsNum = TestClock->GetMidiClockEventsInBlock().Num();
				TestClock->HandleClockEvent(*DrivingClock, Event, PrerollBars, Speed);

				if (!TestTrue("Clock has new clock events", TestClock->GetMidiClockEventsInBlock().Num() > OldEventsNum))
				{
					return;
				}
				TestEqual("Last Clock Event in block", TestClock->GetMidiClockEventsInBlock().Last().Msg.Type, FMidiClockMsg::EType::AdvanceThru);
				TestEqual("Clock Current Tick", TestClock->GetCurrentMidiTick(), ExpectedTick - 1);
			});

			It("ProccessClockEvent(SeekTo)", [&, this]
			{
				int32 StartFrame = 0;
				int32 Tick = 1000;
				float Speed = 1.0f;
				int32 PrerollBars = 8;
				FMidiClockEvent Event = FMidiClockEvent(StartFrame, FMidiClockMsg::FSeekTo(0, Tick));

				TestClock->PrepareBlock();
				int32 OldEventsNum = TestClock->GetMidiClockEventsInBlock().Num();
				TestClock->HandleClockEvent(*DrivingClock, Event, PrerollBars, Speed);

				if (!TestTrue("Clock has new clock events", TestClock->GetMidiClockEventsInBlock().Num() > OldEventsNum))
				{
					return;
				}
				TestEqual("Last Clock Event in block", TestClock->GetMidiClockEventsInBlock()[0].Msg.Type, FMidiClockMsg::EType::SeekThru);
				TestEqual("Clock Current Tick", TestClock->GetCurrentMidiTick(), Tick - 1);
			});
			
		});

	}


}

#endif