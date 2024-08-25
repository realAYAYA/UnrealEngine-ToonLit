// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMetasound/DataTypes/MidiClock.h"

#include "MetasoundDataTypeRegistrationMacro.h"
#include "Engine/Engine.h"
#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHarmonixMidiClock, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace Metasound
{
	DEFINE_METASOUND_ENUM_BEGIN(EMidiClockSubdivisionQuantization, FEnumMidiClockSubdivisionQuantizationType, "SubdivisionQuantizationType")
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::None, "NoneDesc", "None", "NoneTT", "None"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::Bar, "BarDesc", "Bar", "BarTT", "Bar"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::Beat, "BeatDesc", "Beat", "BeatTT", "Beat"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::ThirtySecondNote, "ThirtySecondNoteDesc", "1/32", "ThirtySecondNoteTT", "1/32"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::SixteenthNote, "SixteenthNoteDesc", "1/16", "SixteenthNoteTT", "1/16"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::EighthNote, "EighthNoteDesc", "1/8", "EighthNoteTT", "1/8"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::QuarterNote, "QuarterNoteDesc", "1/4", "QuarterNoteTT", "1/4"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::HalfNote, "HalfNoteDesc", "Half", "HalfNoteTT", "Half"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::WholeNote, "WholeNoteDesc", "Whole", "WholeNoteTT", "Whole"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedSixteenthNote, "DottedSixteenthNoteDesc", "(dotted) 1/16", "DottedSixteenthNoteTT", "(dotted) 1/16"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedEighthNote, "DottedEighthNoteDesc", "(dotted) 1/8", "DottedEighthNoteTT", "(dotted) 1/8"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedQuarterNote, "DottedQuarterNoteDesc", "(dotted) 1/4", "DottedQuarterNoteTT", "(dotted) 1/4"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedHalfNote, "DottedHalfNoteDesc", "(dotted) Half", "DottedHalfNoteTT", "(dotted) Half"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::DottedWholeNote, "DottedWholeNoteDesc", "(dotted) Whole", "DottedWholeNoteTT", "(dotted) Whole"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::SixteenthNoteTriplet, "SixteenthNoteTripletDesc", "1/16 (triplet)", "SixteenthNoteTripletTT", "1/16 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::EighthNoteTriplet, "EighthNoteTripletDesc", "1/8 (triplet)", "EighthNoteTripletTT", "1/8 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::QuarterNoteTriplet, "QuarterNoteTripletDesc", "1/4 (triplet)", "QuarterNoteTripletTT", "1/4 (triplet)"),
		DEFINE_METASOUND_ENUM_ENTRY(EMidiClockSubdivisionQuantization::HalfNoteTriplet, "HalfNoteTripletDesc", "1/2 (triplet)", "HalfNoteTripletTT", "1/2 (triplet)"),
	DEFINE_METASOUND_ENUM_END()
}

REGISTER_METASOUND_DATATYPE(HarmonixMetasound::FMidiClock, "MIDIClock")

namespace HarmonixMetasound
{
	using namespace Metasound;
	
	FMidiClock::FMidiClock(const FOperatorSettings& InSettings)
		: MidiClockEventCursor(this)
		, BlockSize(InSettings.GetNumFramesPerBlock())
		, CurrentBlockFrameIndex(0)
		, SampleRate(InSettings.GetSampleRate())
		, HasSpeedChangeInBlock(false)
		, HasTempoChangeInBlock(false)
		, DrivingMidiPlayCursorMgr(MakeShared<FMidiPlayCursorMgr>())
	{
		SpeedChangesInBlock.Add({0, 0.0f, 1.0f});
		TempoChangesInBlock.Add({0, 0.0f, 120.0f});
		
		DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&MidiClockEventCursor);

		RegisterForGameThreadUpdates();
	}

	FMidiClock::FMidiClock(const FMidiClock& Other)
		: MidiClockEventCursor(this)
		, BlockSize(Other.BlockSize)
		, CurrentBlockFrameIndex(Other.CurrentBlockFrameIndex)
		, SampleRate(Other.SampleRate)
		, SampleCount(Other.SampleCount)
		, FramesUntilNextProcess(Other.FramesUntilNextProcess)
		, CurrentTransportState(Other.CurrentTransportState)
		, TransportChangesInBlock(Other.TransportChangesInBlock)
		, HasSpeedChangeInBlock(Other.HasSpeedChangeInBlock)
		, SpeedChangesInBlock(Other.SpeedChangesInBlock)
		, HasTempoChangeInBlock(Other.HasTempoChangeInBlock)
		, TempoChangesInBlock(Other.TempoChangesInBlock)
		, MidiClockEventsInBlock(Other.MidiClockEventsInBlock)
		, SmoothingEnabled(Other.SmoothingEnabled)
		, DrivingMidiPlayCursorMgr(Other.DrivingMidiPlayCursorMgr)
	{
		DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&MidiClockEventCursor);

		RegisterForGameThreadUpdates();
	}

	FMidiClock& FMidiClock::operator=(const FMidiClock& Other)
	{
		if (this != &Other)
		{
			UnregisterForGameThreadUpdates();

			if (DrivingMidiPlayCursorMgr != Other.DrivingMidiPlayCursorMgr)
			{
				DrivingMidiPlayCursorMgr->UnregisterPlayCursor(&MidiClockEventCursor);
			}

			BlockSize = Other.BlockSize;
			CurrentBlockFrameIndex = Other.CurrentBlockFrameIndex;
			SampleRate = Other.SampleRate;
			SampleCount = Other.SampleCount;
			FramesUntilNextProcess = Other.FramesUntilNextProcess;
			CurrentTransportState = Other.CurrentTransportState;
			TransportChangesInBlock = Other.TransportChangesInBlock;
			HasSpeedChangeInBlock = Other.HasSpeedChangeInBlock;
			SpeedChangesInBlock = Other.SpeedChangesInBlock;
			HasTempoChangeInBlock = Other.HasTempoChangeInBlock;
			TempoChangesInBlock = Other.TempoChangesInBlock;
			MidiClockEventsInBlock = Other.MidiClockEventsInBlock;
			SmoothingEnabled = Other.SmoothingEnabled;

			if (DrivingMidiPlayCursorMgr != Other.DrivingMidiPlayCursorMgr)
			{
				DrivingMidiPlayCursorMgr = Other.DrivingMidiPlayCursorMgr;
				DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(&MidiClockEventCursor);
			}

			RegisterForGameThreadUpdates();
		}

		return *this;
	}

	FMidiClock::~FMidiClock()
	{
		UnregisterForGameThreadUpdates();
	
		DrivingMidiPlayCursorMgr->UnregisterPlayCursor(&MidiClockEventCursor, false);
	}

	void FMidiClock::RegisterForGameThreadUpdates()
	{
		UMidiClockUpdateSubsystem::TrackMidiClock(this);
	}

	void FMidiClock::UnregisterForGameThreadUpdates()
	{
		UMidiClockUpdateSubsystem::StopTrackingMidiClock(this);
	}

	void FMidiClock::ResetAndStart(int32 FrameIndex, bool SeekToStart)
	{
		if (SeekToStart)
		{
			SampleCount = 0;
			DrivingMidiPlayCursorMgr->SeekTo(0, 0, true, false);
		}
	
		AddTransportStateChangeToBlock({FrameIndex,0.0f, EMusicPlayerTransportState::Playing});
		
		HasSpeedChangeInBlock = false;
		SpeedChangesInBlock.SetNum(1);
		SpeedChangesInBlock[0] = {0, 0.0f, 1.0f};

		HasTempoChangeInBlock = false;
		TempoChangesInBlock.SetNum(1);
		TempoChangesInBlock[0] = { 0, 0.0f, 120.0f };

		CurrentBlockFrameIndex = FrameIndex;
		FramesUntilNextProcess = 0;
	}

	void FMidiClock::PrepareBlock()
	{
		TransportChangesInBlock.Empty(4);
		if (SpeedChangesInBlock.Num() > 1)
		{
			SpeedChangesInBlock[0].Speed = SpeedChangesInBlock.Last().Speed;
			SpeedChangesInBlock.SetNum(1, EAllowShrinking::No);
		}
		HasSpeedChangeInBlock = false;
		if (TempoChangesInBlock.Num() > 1)
		{
			TempoChangesInBlock[0].Tempo = TempoChangesInBlock.Last().Tempo;
			TempoChangesInBlock.SetNum(1, EAllowShrinking::No);
		}
		HasTempoChangeInBlock = false;
		CurrentBlockFrameIndex = 0;

		MidiClockEventsInBlock.Reset();
	}

	bool FMidiClock::HasLowResCursors() const
	{
		return DrivingMidiPlayCursorMgr->HasLowResCursors();
	}

	void FMidiClock::UpdateLowResCursors()
	{
		DrivingMidiPlayCursorMgr->AdvanceLowResCursors();
	}

	void FMidiClock::AddTransportStateChangeToBlock(const FMidiTimestampTransportState& NewTransportState)
	{
		if (TransportChangesInBlock.IsEmpty() || TransportChangesInBlock.Last().BlockSampleFrameIndex <= NewTransportState.BlockSampleFrameIndex)
		{
			CurrentTransportState = NewTransportState;
			TransportChangesInBlock.Add(NewTransportState);
		}
	}

	void FMidiClock::AddSpeedChangeToBlock(const FMidiTimestampSpeed& NewSpeed)
	{
		check(SpeedChangesInBlock.IsEmpty() || SpeedChangesInBlock.Last().BlockSampleFrameIndex <= NewSpeed.BlockSampleFrameIndex);
		if (SpeedChangesInBlock.Last().BlockSampleFrameIndex == NewSpeed.BlockSampleFrameIndex)
		{
			SpeedChangesInBlock.Last().Speed = NewSpeed.Speed;
		}
		else
		{
			SpeedChangesInBlock.Add(NewSpeed);
		}
		HasSpeedChangeInBlock = true;
		DrivingMidiPlayCursorMgr->InformOfHiResAdvanceRate(NewSpeed.Speed);
	}

	const TArray<FMidiClockEvent>& FMidiClock::GetMidiClockEventsInBlock() const
	{
		return MidiClockEventsInBlock;
	}

	EMusicPlayerTransportState FMidiClock::GetTransportStateAtBlockSampleFrame(int32 FrameIndex) const
	{
		return GetTransportTimestampForBlockSampleFrame(FrameIndex).TransportState;
	}

	EMusicPlayerTransportState FMidiClock::GetTransportStateAtEndOfBlock() const
	{
		return CurrentTransportState.TransportState;
	}

	const FMidiTimestampTransportState& FMidiClock::GetTransportTimestampForBlockSampleFrame(int32 FrameIndex) const
	{
		if (TransportChangesInBlock.IsEmpty())
		{
			return CurrentTransportState;
		}
		int32 Index = Algo::UpperBoundBy(TransportChangesInBlock, FrameIndex, [](const FMidiTimestampTransportState& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			return CurrentTransportState;
		}
		return TransportChangesInBlock[Index];
	}

	float FMidiClock::GetSpeedAtBlockSampleFrame(int32 FrameIndex) const
	{
		return GetSpeedTimestampForBlockSampleFrame(FrameIndex).Speed;
	}

	float FMidiClock::GetSpeedAtEndOfBlock() const
	{
		return SpeedChangesInBlock.Last().Speed;
	}

	const FMidiTimestampSpeed& FMidiClock::GetSpeedTimestampForBlockSampleFrame(int32 FrameIndex) const
	{
		int32 Index = Algo::UpperBoundBy(SpeedChangesInBlock, FrameIndex, [](const FMidiTimestampSpeed& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			Index = 0;
		}
		return SpeedChangesInBlock[Index];
	}

	float FMidiClock::GetTempoAtBlockSampleFrame(int32 FrameIndex) const
	{
		int32 Index = Algo::UpperBoundBy(TempoChangesInBlock, FrameIndex, [](const FMidiTimestampTempo& t) { return t.BlockSampleFrameIndex; }) - 1;
		if (Index < 0)
		{
			Index = 0;
		}
		return TempoChangesInBlock[Index].Tempo;
	}

	float FMidiClock::GetTempoAtEndOfBlock() const
	{
		return TempoChangesInBlock.Last().Tempo;
	}

	int32 FMidiClock::GetNumTempoChangesInBlock() const
	{
		return TempoChangesInBlock.Num();
	}

	FMidiTimestampTempo FMidiClock::GetTempoChangeByIndex(int32 Index) const
	{
		if (Index >= 0 && Index < TempoChangesInBlock.Num())
		{
			return TempoChangesInBlock[Index]; // intentional copy
		}

		return InvalidMidiTimestampTempo;
	}

	int32 FMidiClock::GetCurrentMidiTick() const
	{
		return DrivingMidiPlayCursorMgr->GetCurrentHiResTick();
	}

	int32 FMidiClock::GetCurrentBlockFrameIndex() const
	{
		return CurrentBlockFrameIndex;
	}

	void FMidiClock::AdvanceHiResToMs(int32 BlockFrameIndex, float Ms, bool Broadcast)
	{
		CurrentBlockFrameIndex = BlockFrameIndex;
		DrivingMidiPlayCursorMgr->AdvanceHiResToMs(Ms, Broadcast);
	}

	void FMidiClock::AttachToTimeAuthority(const FMidiClock& MidiClockRef)
	{
		DrivingMidiPlayCursorMgr->AttachToTimeAuthority(MidiClockRef.DrivingMidiPlayCursorMgr);
	}

	void FMidiClock::DetachFromTimeAuthority()
	{
		DrivingMidiPlayCursorMgr->DetachFromTimeAuthority();
	}
	
	float FMidiClock::GetQuarterNoteIncludingCountIn() const
	{
		int32 Tick = DrivingMidiPlayCursorMgr->GetCurrentHiResTick();
		return (float)Tick / (float)DrivingMidiPlayCursorMgr->GetSongMaps().GetTicksPerQuarterNote();
	}

	FMusicTimestamp FMidiClock::GetCurrentMusicTimestamp() const
	{
		return DrivingMidiPlayCursorMgr->GetMusicTimestampAtMs(GetCurrentHiResMs());
	}

	FMusicTimestamp FMidiClock::GetMusicTimestampAtBlockOffset(const int32 Offset) const
	{
		const float MsAtOffset = GetMsAtBlockOffset(Offset);
		return DrivingMidiPlayCursorMgr->GetMusicTimestampAtMs(MsAtOffset);
	}

	float FMidiClock::GetMsAtBlockOffset(int32 Offset) const
	{
		const float EndMs = GetCurrentHiResMs();
		const float MsPerFrame = 1000 / SampleRate;
		const int32 InvOffset = BlockSize - Offset - 1;
		return EndMs - InvOffset * MsPerFrame;
	}

	void FMidiClock::LockForMidiDataChanges()
	{

		DrivingMidiPlayCursorMgr->LockForMidiDataChanges();
	}

	void FMidiClock::MidiDataChangesComplete(FMidiPlayCursorMgr::EMidiChangePositionCorrectMode Mode /*= FMidiPlayCursorMgr::EMidiChangePositionCorrectMode::MaintainTick*/)
	{
		DrivingMidiPlayCursorMgr->MidiDataChangeComplete(Mode);
	}

	void FMidiClock::SeekTo(const FMusicSeekTarget& Target, int32 PreRollBars)
	{
		int32 Tick = 0;
		switch (Target.Type)
		{
		case ESeekPointType::BarBeat:
			Tick = DrivingMidiPlayCursorMgr->GetBarMap().MusicTimestampToTick(Target.BarBeat);
			break;
		default:
		case ESeekPointType::Millisecond:
			Tick = DrivingMidiPlayCursorMgr->GetSongMaps().MsToTick(Target.Ms);
			break;
		}
		DrivingMidiPlayCursorMgr->SeekTo(Tick, PreRollBars,true,false);
	}

	void FMidiClock::SeekTo(int32 Tick, int32 PreRollBars)
	{
		DrivingMidiPlayCursorMgr->SeekTo(Tick, PreRollBars, true, false);
	}

	void FMidiClock::InformOfCurrentAdvanceRate(float AdvanceRate)
	{
		DrivingMidiPlayCursorMgr->InformOfHiResAdvanceRate(AdvanceRate);
	}

	void FMidiClock::CopySpeedAndTempoChanges(const FMidiClock* InClock, float InSpeedMult)
	{
		HasTempoChangeInBlock = InClock->HasTempoChangeInBlock;
		TempoChangesInBlock = InClock->TempoChangesInBlock;

		HasSpeedChangeInBlock = InClock->HasSpeedChangeInBlock;
		SpeedChangesInBlock = InClock->SpeedChangesInBlock;

		for (FMidiTimestampSpeed& SpeedChange : SpeedChangesInBlock)
		{
			SpeedChange.Speed *= InSpeedMult;
		}
	}

	void FMidiClock::HandleTransportChange(int32 BlockFrameIndex, EMusicPlayerTransportState TransportState)
	{
		switch (TransportState)
		{
		case EMusicPlayerTransportState::Prepared:
		case EMusicPlayerTransportState::Paused:
			AddTransportStateChangeToBlock({ BlockFrameIndex, 0.0f, TransportState });
			break;
		case EMusicPlayerTransportState::Playing:
			AddTransportStateChangeToBlock({ BlockFrameIndex, 0.0f, TransportState });
			break;
		}
	}

	void FMidiClock::Process(int32 StartFrame, int32 NumFrames, int32 PrerollBars, float Speed)
	{
		int32 EndFrame = StartFrame + NumFrames;
		switch (CurrentTransportState.TransportState)
		{
		case EMusicPlayerTransportState::Playing:
		case EMusicPlayerTransportState::Continuing:
			WriteAdvance(StartFrame, EndFrame, Speed);
			break;
		}
	}

	void FMidiClock::Process(const FMidiClock& DrivingClock, int32 StartFrame, int32 NumFrames, int32 PrerollBars, float Speed)
	{
		int32 EndFrame = StartFrame + NumFrames;
		const TArray<FMidiClockEvent>& ClockEvents = DrivingClock.GetMidiClockEventsInBlock();
		int32 Index = Algo::LowerBoundBy(ClockEvents, StartFrame, &FMidiClockEvent::BlockFrameIndex);
		while (ClockEvents.IsValidIndex(Index))
		{
			const FMidiClockEvent& Event = ClockEvents[Index];
			if (Event.BlockFrameIndex >= EndFrame)
			{
				return;
			}
			
			HandleClockEvent(DrivingClock, Event, PrerollBars, Speed);
			++Index;
		}
	}
	
	void FMidiClock::HandleClockEvent(const FMidiClock& DrivingClock, const FMidiClockEvent& Event, int32 PrerollBars, float Speed)
	{
		switch (Event.Msg.Type)
		{
		case FMidiClockMsg::EType::Reset:
			{
				int32 Tick = CalculateMappedTick(Event.Msg.ToTick());
				SeekTo(Event.BlockFrameIndex, Tick + 1, PrerollBars);
				break;
			}
		case FMidiClockMsg::EType::Loop:
			// ignore loops since the driving clock will loop us by seeking
			break;
		case FMidiClockMsg::EType::SeekTo:
			{
				int32 Tick = CalculateMappedTick(Event.Msg.ToTick());
				SeekTo(Event.BlockFrameIndex, Tick, PrerollBars);
				break;
			}
		case FMidiClockMsg::EType::SeekThru:
			{
				int32 Tick = CalculateMappedTick(Event.Msg.ThruTick());
				SeekTo(Event.BlockFrameIndex, Tick + 1, PrerollBars);
				break;
			}
		case FMidiClockMsg::EType::AdvanceThru:
			{
				if (Event.Msg.AsAdvanceThru().IsPreRoll)
				{
					int32 Tick = CalculateMappedTick(Event.Msg.ThruTick());
					SeekTo(Event.BlockFrameIndex, Tick + 1, PrerollBars);
					break;
				}

				// Advance based on the delta ticks, and not based on the absolute tick
				int32 Tick = GetCurrentMidiTick() + (Event.Msg.ThruTick() - Event.Msg.FromTick());
				float Ms = GetSongMaps().TickToMs(Tick);

				float ClockInSpeed = DrivingClock.GetSpeedAtBlockSampleFrame(Event.BlockFrameIndex);
				float AdvanceRatio = DrivingClock.GetSongMaps().GetTempoAtTick(Event.Msg.FromTick())
								   / GetSongMaps().GetTempoAtTick(GetCurrentMidiTick());
				InformOfCurrentAdvanceRate(ClockInSpeed * Speed * AdvanceRatio);
				AdvanceHiResToMs(Event.BlockFrameIndex, Ms, true);
				break;
			}
		}
	}

	void FMidiClock::WriteAdvance(int32 StartFrameIndex, int32 EndFrameIndex, float InSpeed /*= 1.0f*/)
	{
		int32 FramesToProcess = EndFrameIndex - StartFrameIndex;
		const FTempoMap& TempoMap = DrivingMidiPlayCursorMgr->GetSongMaps().GetTempoMap();
		while (FramesToProcess > FramesUntilNextProcess)
		{
			StartFrameIndex += FramesUntilNextProcess;
			FSampleCount AdvanceToFrame = SampleCount + (FSampleCount)((float)kMidiGranularity * InSpeed);
			FramesUntilNextProcess = kMidiGranularity;
			float AdvanceToMs = ((float)AdvanceToFrame * 1000.0f) / SampleRate;
			DrivingMidiPlayCursorMgr->InformOfHiResAdvanceRate(InSpeed);
			AdvanceHiResToMs(StartFrameIndex, AdvanceToMs, true);

			
			// Account for looping
			// recalculate sampleCount based on our new time, if clock looped back, this will be different from AdvanceToFrame
			if (GetCurrentHiResMs() < AdvanceToMs)
			{
				SampleCount = FMath::Max<FSampleCount>(FSampleCount(GetCurrentHiResMs() / 1000.0f * SampleRate), 0);
			}
			else
			{
				SampleCount = AdvanceToFrame;
			}
			

			FramesToProcess = EndFrameIndex - StartFrameIndex;
		}
		FramesUntilNextProcess -= FramesToProcess;
	}

	void FMidiClock::SeekTo(int32 BlockFrameIndex, const FMusicSeekTarget& InTarget, int32 InPrerollBars)
	{
		CurrentBlockFrameIndex = BlockFrameIndex;
		SeekTo(InTarget, InPrerollBars);
		SampleCount = FMath::Max<FSampleCount>(FSampleCount(GetCurrentHiResMs() / 1000.0f * SampleRate), 0);
		FramesUntilNextProcess = 0;
	}

	void FMidiClock::SeekTo(int32 BlockFrameIndex, int32 Tick, int32 InPrerollBars)
	{
		CurrentBlockFrameIndex = BlockFrameIndex;
		DrivingMidiPlayCursorMgr->SeekTo(Tick, InPrerollBars, true, false);
		SampleCount = FMath::Max<FSampleCount>(FSampleCount(GetCurrentHiResMs() / 1000.0f * SampleRate), 0);
		FramesUntilNextProcess = 0;
	}

	int32 FMidiClock::CalculateMappedTick(int32 Tick) const
	{
		if (DoesLoop())
		{
			const int32 LoopStartTick = GetLoopStartTick();
			const int32 LoopEndTick = GetLoopEndTick();
			const int32 LoopLengthTicks = LoopEndTick - LoopStartTick;
			// only wrap the tick if it we're passed the loop end tick
			if (LoopLengthTicks > 0 && Tick >= LoopEndTick)
			{
				return LoopStartTick + (Tick - LoopStartTick) % LoopLengthTicks;
			}
		}
		return Tick;
	}
	
	TSharedPtr<FMidiFileData> FMidiClock::MakeClockConductorMidiData(float InTempoBPM, int32 InTimeSigNum, int32 InTimeSigDen)
	{
		TSharedPtr<FMidiFileData> OutMidiData = MakeShared<FMidiFileData>();

		// clear it all out for good measure...
		FTempoMap& TempoMap = OutMidiData->SongMaps.GetTempoMap();
		TempoMap.Empty();
		FBarMap& BarMap = OutMidiData->SongMaps.GetBarMap();
		BarMap.Empty();
		OutMidiData->Tracks.Empty();

		// create conductor track
		FMidiTrack& Track = OutMidiData->Tracks.Add_GetRef(FMidiTrack(TEXT("conductor")));
		// max out song length data so the midi can play indefinitely
		OutMidiData->ConformToLength(std::numeric_limits<int32>::max());

		// add time sig info
		int32 TimeSigNum = FMath::Clamp(InTimeSigNum, 1, 64);
		int32 TimeSigDen = FMath::Clamp(InTimeSigDen, 1, 64);
		Track.AddEvent(FMidiEvent(0, FMidiMsg((uint8)TimeSigNum, (uint8)TimeSigDen)));
		BarMap.AddTimeSignatureAtBarIncludingCountIn(0, TimeSigNum, TimeSigDen);

		// add tempo info
		float TempoBPM = FMath::Max(1.0f, InTempoBPM);
		int32 MidiTempo = Harmonix::Midi::Constants::BPMToMidiTempo(TempoBPM);
		Track.AddEvent(FMidiEvent(0, FMidiMsg(MidiTempo)));
		TempoMap.AddTempoInfoPoint(MidiTempo, 0);

		Track.Sort();

		return OutMidiData;
	}

	FMidiClock::FMidiClockEventCursor::FMidiClockEventCursor(FMidiClock* MidiClock)
		: MyMidiClock(MidiClock)
	{
		check(MidiClock);
	}

	void FMidiClock::FMidiClockEventCursor::Reset(bool ForceNoBroadcast /*= false*/)
	{
		const int32 FromTick = CurrentTick;
		FMidiPlayCursor::Reset(ForceNoBroadcast);
		AddEvent(FMidiClockEvent(MyMidiClock->GetCurrentBlockFrameIndex(), FMidiClockMsg::FReset(FromTick, CurrentTick, ForceNoBroadcast)));
	}

	void FMidiClock::FMidiClockEventCursor::OnLoop(int32 LoopStartTick, int32 LoopEndTick)
	{
		FMidiPlayCursor::OnLoop(LoopStartTick, LoopEndTick);
		AddEvent(FMidiClockEvent(MyMidiClock->GetCurrentBlockFrameIndex(), FMidiClockMsg::FLoop(LoopStartTick, LoopEndTick)));
	}

	void FMidiClock::FMidiClockEventCursor::SeekToTick(int32 Tick) 
	{
		const int32 FromTick = CurrentTick;
		FMidiPlayCursor::SeekToTick(Tick);
		AddEvent(FMidiClockEvent(MyMidiClock->GetCurrentBlockFrameIndex(), FMidiClockMsg::FSeekTo(FromTick, Tick)));
	}

	void FMidiClock::FMidiClockEventCursor::SeekThruTick(int32 Tick)
	{
		const int32 FromTick = CurrentTick;
		FMidiPlayCursor::SeekThruTick(Tick);
		AddEvent(FMidiClockEvent(MyMidiClock->GetCurrentBlockFrameIndex(), FMidiClockMsg::FSeekThru(FromTick, Tick)));
	}

	void FMidiClock::FMidiClockEventCursor::AdvanceThruTick(int32 Tick, bool IsPreRoll)
	{
		const int32 FromTick = CurrentTick;
		FMidiPlayCursor::AdvanceThruTick(Tick, IsPreRoll);
		AddEvent(FMidiClockEvent(MyMidiClock->GetCurrentBlockFrameIndex(), FMidiClockMsg::FAdvanceThru(FromTick, Tick, IsPreRoll)));
	}

	void FMidiClock::FMidiClockEventCursor::OnTempo(int32 TrackIndex, int32 Tick, int32 Tempo, bool IsPreroll)
	{
		int32 BlockFrameIndex = MyMidiClock->CurrentBlockFrameIndex;

		check(BlockFrameIndex >= MyMidiClock->TempoChangesInBlock.Last().BlockSampleFrameIndex);
		MyMidiClock->HasTempoChangeInBlock = true;
		float Bpm = Harmonix::Midi::Constants::MidiTempoToBPM(Tempo);
		if (MyMidiClock->TempoChangesInBlock.Last().BlockSampleFrameIndex == BlockFrameIndex)
		{
			MyMidiClock->TempoChangesInBlock.Last().Tempo = Bpm;
		}
		else
		{ 
			MyMidiClock->TempoChangesInBlock.Add({BlockFrameIndex, 0.0f, Bpm});
		}
	}

	void FMidiClock::FMidiClockEventCursor::AddEvent(const FMidiClockEvent& InEvent)
	{
		TArray<FMidiClockEvent>& Events = MyMidiClock->MidiClockEventsInBlock;
		if (!Events.IsEmpty())
		{
			check(Events.Last().BlockFrameIndex <= InEvent.BlockFrameIndex);
		}

		Events.Add(InEvent);
	}

}

#undef LOCTEXT_NAMESPACE
