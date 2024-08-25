// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MidiClock.h"

namespace Harmonix
{
	struct HARMONIXMETASOUND_API FMusicTimeInterval
	{
		EMidiClockSubdivisionQuantization Interval = EMidiClockSubdivisionQuantization::Beat;
		EMidiClockSubdivisionQuantization Offset = EMidiClockSubdivisionQuantization::Beat;
		uint16 IntervalMultiplier = 1;
		uint16 OffsetMultiplier = 0;
	};

	HARMONIXMETASOUND_API void IncrementTimestampByInterval(
			FMusicTimestamp& Timestamp,
			const FMusicTimeInterval& Interval,
			const FTimeSignature& TimeSignature);

	HARMONIXMETASOUND_API void IncrementTimestampByOffset(
			FMusicTimestamp& Timestamp,
			const FMusicTimeInterval& Interval,
			const FTimeSignature& TimeSignature);
}
