// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMidi/Blueprint/MidiNote.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "MidiFunctionLibrary.generated.h"

/**
* Function library for FMidiNote and various midi note constants
*/
UCLASS()
class HARMONIXMIDI_API UMidiNoteFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Meta = (DisplayName = "FMidiNote To int64", CompactNodeTitle = "->", BlueprintAutocast), Category = "Midi")
	static int MidiNoteToInt(const FMidiNote& InMidiNote) { return InMidiNote.NoteNumber; }

	UFUNCTION(BlueprintPure, Meta = (DisplayName = "int64 to FMidiNote", CompactNodeTitle = "->", BlueprintAutocast), Category = "Midi")
	static FMidiNote IntToMidiNote(int InInt) { return FMidiNote(InInt); }

	UFUNCTION(BlueprintPure, Meta = (DisplayName = "FMidiNote To byte", CompactNodeTitle = "->", BlueprintAutocast), Category = "Midi")
	static uint8 MidiNoteToByte(const FMidiNote& InMidiNote) { return InMidiNote.NoteNumber; }

	UFUNCTION(BlueprintPure, Meta = (DisplayName = "byte to FMidiNote", CompactNodeTitle = "->", BlueprintAutocast), Category = "Midi")
	static FMidiNote ByteToMidiNote(uint8 InByte) { return FMidiNote(InByte); }

	UFUNCTION(BlueprintPure, Category = "Midi")
	static FMidiNote MakeLiteralMidiNote(FMidiNote Value) { return Value; }

	UFUNCTION(BlueprintPure, Category = "Midi")
	static FMidiNote GetMinMidiNote();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static FMidiNote GetMaxMidiNote();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static uint8 GetMinNoteNumber();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static uint8 GetMaxNoteNumber();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static int GetMaxNumNotes();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static uint8 GetMinNoteVelocity();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static uint8 GetMaxNoteVelocity();
};

/**
* Function library for converting Ticks to Beats and Beats to Ticks and other midi constants
*/
UCLASS()
class HARMONIXMIDI_API UMusicalTickFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Midi")
	static float GetTicksPerQuarterNote();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static int GetTicksPerQuarterNoteInt();
	 
	UFUNCTION(BlueprintPure, Category = "Midi")
	static float GetQuarterNotesPerTick();

	UFUNCTION(BlueprintPure, Category = "Midi")
	static float TickToQuarterNote(float InTick);

	UFUNCTION(BlueprintPure, Category = "Midi")
	static float QuarterNoteToTick(float InQuarterNote);
};
