// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiVoiceId.h"


FCriticalSection FMidiVoiceGeneratorBase::GeneratorIdLock;
uint32 FMidiVoiceGeneratorBase::NextGeneratorId = 1;

FMidiVoiceGeneratorBase::FMidiVoiceGeneratorBase()
{
	FScopeLock Lock(&GeneratorIdLock);
	IdBits = NextGeneratorId++;
	if (NextGeneratorId >= (1L << IdWidth))
	{
		NextGeneratorId = 1;
	}
	IdBits <<= (32 - IdWidth);
}

