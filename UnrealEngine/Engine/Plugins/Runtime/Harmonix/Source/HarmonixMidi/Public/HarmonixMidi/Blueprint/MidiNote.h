// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MidiNote.generated.h"

/**
* Helper struct for getting and assigning MidiNote values
* Uses custom detail customization and custom pins for convenience in Editor
* 
* has a one property
* uint8 NoteNumber
* ranging from Midi::kMinNote to Midi::kMaxNote [0 - 127]
*/
USTRUCT(BlueprintType, Meta = (DisplayName = "MIDI Note"))
struct HARMONIXMIDI_API FMidiNote
{
	GENERATED_BODY()

	FMidiNote() : NoteNumber(60) { };

	FMidiNote(uint8 inValue) : NoteNumber(inValue) { };
	FMidiNote(int inValue) : NoteNumber(inValue) { };
	FMidiNote(int8 inValue) : NoteNumber(inValue) { };

	operator uint8() { return NoteNumber; }
	operator int8() { return NoteNumber; }
	operator int() { return NoteNumber; }

	bool operator==(const FMidiNote& rhs) const { return NoteNumber == rhs.NoteNumber; }
	bool operator!=(const FMidiNote& rhs) const { return NoteNumber != rhs.NoteNumber; }
	bool operator< (const FMidiNote& rhs) const { return NoteNumber <  rhs.NoteNumber; }
	bool operator> (const FMidiNote& rhs) const { return NoteNumber >  rhs.NoteNumber; }
	bool operator<=(const FMidiNote& rhs) const { return NoteNumber <= rhs.NoteNumber; }
	bool operator>=(const FMidiNote& rhs) const { return NoteNumber >= rhs.NoteNumber; }
	
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Midi")
	uint8 NoteNumber = 60;

	FText GetDisplayText() const;

	FName GetDisplayName() const;

	// note names formated enharmonic stile sharp with octave number (eg. C#4)
	FString ToString() const;

	// takes a note name (eg C#4) and makes a FMidiNote struct from it
	static FMidiNote FromString(const FString& NoteName);

	static uint8 NoteNumberFromString(const FString& NoteName);

#ifdef WITH_EDITOR

	FString ToEditorString() const;

	static FMidiNote FromEditorString(const FString& EditorName);

	static uint8 NoteNumberFromEditorString(const FString& EditorName);
#endif
};

