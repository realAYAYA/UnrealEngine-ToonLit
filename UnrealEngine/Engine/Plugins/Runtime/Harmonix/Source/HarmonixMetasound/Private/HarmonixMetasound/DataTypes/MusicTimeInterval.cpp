// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MusicTimeInterval.h"
#include "HarmonixMidi/SongMaps.h"

namespace Harmonix
{
	float GetIntervalInBeats(const FMusicTimeInterval& Interval, const FTimeSignature& TimeSignature)
	{
		return FSongMaps::SubdivisionToBeats(Interval.Interval, TimeSignature) * Interval.IntervalMultiplier;
	}

	void IncrementTimestampByBeats(FMusicTimestamp& Timestamp, float Beats, const FTimeSignature& TimeSignature)
	{
		// Add the beats
		Timestamp.Beat += Beats;

		// Add bars until the beat is within the bar
		// Beats and bars are 1-indexed, so, for example, in 4/4, Beat 4.999... is within the bar.
		while (Timestamp.Beat >= static_cast<float>(TimeSignature.Numerator + 1))
		{
			++Timestamp.Bar;
			Timestamp.Beat -= TimeSignature.Numerator;
		}
	}
	
	void IncrementTimestampByInterval(
		FMusicTimestamp& Timestamp,
		const FMusicTimeInterval& Interval,
		const FTimeSignature& TimeSignature)
	{
		// Get the pulse interval in beats
		const float IntervalBeats = GetIntervalInBeats(Interval, TimeSignature);

		if (FMath::IsNearlyZero(IntervalBeats))
		{
			return;
		}

		IncrementTimestampByBeats(Timestamp, IntervalBeats, TimeSignature);
	}

	float GetOffsetInBeats(const FMusicTimeInterval& Interval, const FTimeSignature& TimeSignature)
	{
		const float IntervalBeats = GetIntervalInBeats(Interval, TimeSignature);
		const float OffsetBeats = FSongMaps::SubdivisionToBeats(Interval.Offset, TimeSignature) * Interval.OffsetMultiplier;
		return FMath::Fmod(OffsetBeats, IntervalBeats);
	}

	void IncrementTimestampByOffset(
		FMusicTimestamp& Timestamp,
		const FMusicTimeInterval& Interval,
		const FTimeSignature& TimeSignature)
	{
		// Get the offset in beats
		const float OffsetBeats = GetOffsetInBeats(Interval, TimeSignature);

		if (FMath::IsNearlyZero(OffsetBeats))
		{
			return;
		}

		IncrementTimestampByBeats(Timestamp, OffsetBeats, TimeSignature);
	}
}
