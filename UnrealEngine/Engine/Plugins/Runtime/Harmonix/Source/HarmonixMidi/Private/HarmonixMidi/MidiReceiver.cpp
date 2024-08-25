// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiReceiver.h"
#include "HarmonixMidi/MidiReader.h"
#include "HarmonixMidi/MusicTimeSpecifier.h"
#include "HarmonixMidi/MidiConstants.h"

void IMidiReceiver::SkipCurrentTrack()
{
	check(Reader);
	Reader->SkipCurrentTrack();
}
