// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiClockEvent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"


namespace HarmonixMetasound
{
	const FMidiClockMsg::FReset& FMidiClockMsg::AsReset() const
	{
		check(Type == EType::Reset);
		return Reset;
	}
	const FMidiClockMsg::FLoop& FMidiClockMsg::AsLoop() const
	{
		check(Type == EType::Loop);
		return Loop;
	}
	const FMidiClockMsg::FSeekTo& FMidiClockMsg::AsSeekTo() const
	{
		check(Type == EType::SeekTo);
		return SeekTo;
	}
	const FMidiClockMsg::FSeekThru& FMidiClockMsg::AsSeekThru() const
	{
		check(Type == EType::SeekThru);
		return SeekThru;
	}
	const FMidiClockMsg::FAdvanceThru& FMidiClockMsg::AsAdvanceThru() const
	{
		check(Type == EType::AdvanceThru);
		return AdvanceThru;
	}

	int32 FMidiClockMsg::FromTick() const
	{
		switch (Type)
		{
		case EType::Reset: return Reset.FromTick;
		case EType::SeekTo: return SeekTo.FromTick;
		case EType::SeekThru: return SeekThru.FromTick;
		case EType::AdvanceThru: return AdvanceThru.FromTick;
		default: checkNoEntry();
		}
		return -1;
	}
	
	int32 FMidiClockMsg::ToTick() const
	{
		switch (Type)
		{
		case EType::Reset: return Reset.ToTick;
		case EType::SeekTo: return SeekTo.ToTick;
		default: checkNoEntry();
		}
		return -1;
	}

	int32 FMidiClockMsg::ThruTick() const
	{
		switch (Type)
		{
		case EType::SeekThru: return SeekThru.ThruTick;
		case EType::AdvanceThru: return AdvanceThru.ThruTick;
		default: checkNoEntry();
		}
		return -1;
	}



}
