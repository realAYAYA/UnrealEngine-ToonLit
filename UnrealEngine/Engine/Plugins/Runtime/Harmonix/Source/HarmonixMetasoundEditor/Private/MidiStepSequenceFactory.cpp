// Copyright Epic Games, Inc. All Rights Reserved.

#include "MidiStepSequenceFactory.h"
#include "HarmonixMetasound/DataTypes/MidiStepSequence.h"

UMidiStepSequenceFactory::UMidiStepSequenceFactory()
{
	SupportedClass = UMidiStepSequence::StaticClass();
	bCreateNew = true;
}

FText UMidiStepSequenceFactory::GetDisplayName() const
{
	return NSLOCTEXT("MIDI", "MIDIStepSequenceFactoryName", "MIDI Step Sequence");
}

UObject* UMidiStepSequenceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMidiStepSequence>(InParent, Class, Name, Flags, Context);
}
