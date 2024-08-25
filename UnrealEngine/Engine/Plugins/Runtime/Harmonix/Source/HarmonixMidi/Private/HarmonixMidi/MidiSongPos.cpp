// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiSongPos.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/MusicTimeSpecifier.h"
#include "HarmonixMidi/MidiConstants.h"
#include "HarmonixMidi/SongMaps.h"

// -1 = less than, 1 = greater than, 0 = equal to
int32 SongPosCmp(const FMidiSongPos& lhs, const FMidiSongPos& rhs)
{
	// can't use bar alone because of floating point. 
	// so use bar first and then "rounded" beat.
		
	// bar
	if (lhs.Timestamp.Bar < rhs.Timestamp.Bar)
	{
		return -1;
	}
	else if (lhs.Timestamp.Bar > rhs.Timestamp.Bar)
	{
		return 1;
	}

	// beat
	if (Harmonix::Midi::Constants::RoundToStandardBeatPrecision(lhs.Timestamp.Beat, lhs.TimeSigDenominator) ==
		Harmonix::Midi::Constants::RoundToStandardBeatPrecision(rhs.Timestamp.Beat, rhs.TimeSigDenominator))
	{
		return 0;
	}
	if (lhs.Timestamp.Beat < rhs.Timestamp.Beat)
	{
		return -1;
	}
	return 1;
}

// operators
bool FMidiSongPos::operator<(const FMidiSongPos& rhs) const
{
	return SongPosCmp(*this, rhs) == -1;
}

bool FMidiSongPos::operator<=(const FMidiSongPos& rhs) const
{
	int32 cmp = SongPosCmp(*this, rhs);
	return cmp == -1 || cmp == 0;
}

bool FMidiSongPos::operator>(const FMidiSongPos& rhs) const
{
	return SongPosCmp(*this, rhs) == 1;
}

bool FMidiSongPos::operator>=(const FMidiSongPos& rhs) const
{
	int32 cmp = SongPosCmp(*this, rhs);
	return cmp == 1 || cmp == 0;
}

bool FMidiSongPos::operator==(const FMidiSongPos& rhs) const
{
	return SongPosCmp(*this, rhs) == 0;
}

void FMidiSongPos::SetByTime(float InElapsedMs, float InBpm, int32 InTimeSigNumerator, int32 InTimeSigDenominator, int32 StartBar)
{
	// first some numbers we will need...
	float QuarterNotesPerSecond = InBpm / 60.0f;
	float BeatsPerSecond       = QuarterNotesPerSecond * (InTimeSigDenominator / 4);
	float TotalBeats           = BeatsPerSecond * (InElapsedMs / 1000.0f);
	float BeatsPerBar          = (float)InTimeSigNumerator; // simple assumption given lack of SongMaps!
	int32 BeatsOfCountIn       = (1 - StartBar) * InTimeSigNumerator;
	float CountInSeconds       = (float)BeatsOfCountIn / BeatsPerSecond;

	SecondsIncludingCountIn = InElapsedMs / 1000.0f;
	SecondsFromBarOne       = SecondsIncludingCountIn - CountInSeconds;
	TimeSigNumerator        = InTimeSigNumerator;
	TimeSigDenominator      = InTimeSigDenominator;
	Tempo                   = InBpm;
	BarsIncludingCountIn    = TotalBeats / TimeSigNumerator;
	BeatsIncludingCountIn   = TotalBeats;
	Timestamp.Bar = FMath::FloorToInt32(BarsIncludingCountIn) + StartBar;
	Timestamp.Beat = FMath::Fractional(BarsIncludingCountIn) * InTimeSigNumerator + 1.0f;
}

void FMidiSongPos::SetByTime(float InMs, const FSongMaps& Maps)
{
	float Tick = Maps.GetTempoMap().MsToTick(InMs);
	SetByTimeAndTick(InMs, Tick, Maps);
}

void FMidiSongPos::SetByTick(float InTick, const FSongMaps& Maps)
{
	float Ms = Maps.GetTempoMap().TickToMs(InTick);
	SetByTimeAndTick(Ms, InTick, Maps);
}

void FMidiSongPos::SetByTimeAndTick(float InMs, float InTick, const FSongMaps& Maps)
{
	const FTimeSignature* TimeSig = Maps.GetTimeSignatureAtTick(int32(InTick));
	SecondsIncludingCountIn = InMs / 1000.0f;
	SecondsFromBarOne       = SecondsIncludingCountIn - Maps.GetCountInSeconds();
	TimeSigNumerator        = TimeSig ? TimeSig->Numerator : 4;
	TimeSigDenominator      = TimeSig ? TimeSig->Denominator : 4;
	Tempo                   = Maps.GetTempoAtTick(int32(InTick));
	BarsIncludingCountIn    = Maps.GetBarIncludingCountInAtTick(InTick);

	const FSongSection* SongSection = !Maps.GetSectionMap().IsEmpty() ? Maps.GetSectionAtTick(int32(InTick)) : nullptr;
	CurrentSongSection		= SongSection ? FSongSection(SongSection->Name, SongSection->StartTick, SongSection->LengthTicks) : FSongSection();
	
	const FBeatMap& BeatMap = Maps.GetBeatMap();
	int32 BeatPointIndex     = BeatMap.GetPointIndexForTick(int32(InTick));
	if (BeatPointIndex >= 0)
	{
		const FBeatMapPoint& BeatPoint = BeatMap.GetBeatPointInfo(BeatPointIndex);
		float TickInBeat     = InTick - BeatPoint.StartTick;
		float FractionalPart = TickInBeat / BeatPoint.LengthTicks;
		BeatsIncludingCountIn     = BeatPointIndex + FractionalPart;
		BeatType             = BeatPoint.Type;
	}
	else
	{
		// use basic time signature based info from bar map...
		const FBarMap& BarMap = Maps.GetBarMap();
		int32 BarPointIndex = BarMap.GetPointIndexForTick(int32(InTick));
		if (BarPointIndex == INDEX_NONE)
		{
			ensure(InTick < 0.0f);
			BarPointIndex = 0;
		}
		const FTimeSignaturePoint& TimeSigPoint = BarMap.GetTimeSignaturePoint(BarPointIndex);
		float BarAtTimeSig = BarsIncludingCountIn - TimeSigPoint.BarIndex;
		float FractPart = FMath::Fractional(BarAtTimeSig);
		BeatsIncludingCountIn = TimeSigPoint.BeatIndex + (BarAtTimeSig * TimeSigPoint.TimeSignature.Numerator);
		BeatType = FMath::IsNearlyEqual(Timestamp.Beat, 1.0f) ? EMusicalBeatType::Downbeat : EMusicalBeatType::Normal;
	}
	Timestamp = Maps.GetBarMap().TickToMusicTimestamp(InTick);
}
