// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/BeatMap.h"
#include "HarmonixMidi/MidiConstants.h"
#include "Math/UnrealMathUtility.h"
#include "SectionMap.h"

#include "MidiSongPos.generated.h"

struct FSongMaps;

/////////////////////////////////////////////////////////////////////////////
// Position within a song (midi info)
//
USTRUCT(BlueprintType, Meta = (DisplayName = "MIDI Song Position"))
struct HARMONIXMIDI_API FMidiSongPos
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	float SecondsFromBarOne = 0.0f; // total seconds from bar 1 beat 1. (negative for count-in and/or pickup bars)
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	float SecondsIncludingCountIn = 0.0f; // total seconds from the beginning of the musical content (ie. includes all count-in and pickup bars)
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	int32 TimeSigNumerator = 4;
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	int32 TimeSigDenominator = 4;
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	float Tempo = 120.0f;
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	float BarsIncludingCountIn = 0.0f; // total bars from the beginning of the song.
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	float BeatsIncludingCountIn = 0.0f; // total beats from the beginning of the song.
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	EMusicalBeatType BeatType = EMusicalBeatType::Normal;
	UPROPERTY(BlueprintReadOnly, Category = "MidiSongPosition")
	FMusicTimestamp Timestamp;

	FSongSection CurrentSongSection;

	// This version is for use when song maps are unavailable. StartBar can be used if you know the content
	// has, or will have, etc. a pickup or count-in. So for example... 
	// If tempo = 60 bpm, time signature = 4/4, and StartBar = -1 (two bars or count-in, bars -1 and 0), then...
	//     InElapsedMs = 0000.0 --> Seconds = -8, ElapsedBars = 0.00, MusicPosition.Bar = -1, MusicPosition.Beat = 1
	//     InElapsedMs = 1000.0 --> Seconds = -7, ElapsedBars = 0.25, MusicPosition.Bar = -1, MusicPosition.Beat = 2
	//     InElapsedMs = 2000.0 --> Seconds = -6, ElapsedBars = 0.50, MusicPosition.Bar = -1, MusicPosition.Beat = 3
	//     InElapsedMs = 3000.0 --> Seconds = -5, ElapsedBars = 0.75, MusicPosition.Bar = -1, MusicPosition.Beat = 4
	//     InElapsedMs = 4000.0 --> Seconds = -4, ElapsedBars = 1.00, MusicPosition.Bar =  0, MusicPosition.Beat = 1
	//     InElapsedMs = 5000.0 --> Seconds = -3, ElapsedBars = 1.25, MusicPosition.Bar =  0, MusicPosition.Beat = 2
	//     InElapsedMs = 6000.0 --> Seconds = -2, ElapsedBars = 1.50, MusicPosition.Bar =  0, MusicPosition.Beat = 3
	//     InElapsedMs = 7000.0 --> Seconds = -1, ElapsedBars = 1.75, MusicPosition.Bar =  0, MusicPosition.Beat = 4
	//     InElapsedMs = 8000.0 --> Seconds =  0, ElapsedBars = 2.00, MusicPosition.Bar =  1, MusicPosition.Beat = 1
	// etc.
	void SetByTime(float InElapsedMs, float Bpm, int32 TimeSigNum = 4, int32 TimeSigDenom = 4, int32 StartBar = 1);

	void SetByTime(float InElapsedMs, const FSongMaps& Maps);

	// Low-level version for midi players / parsers that use the low-level
	// "midi tick system" for advancing song position.
	void SetByTick(float Tick, const FSongMaps& Maps);

	void Reset()	
	{
		SecondsFromBarOne = 0.0f;
		SecondsIncludingCountIn = 0.0f;
		TimeSigNumerator = 4;
		TimeSigDenominator = 4;
		Tempo = 120.0f;
		Timestamp.Reset();
		BarsIncludingCountIn = 0.0f;
		BeatsIncludingCountIn = 0.0f;
		BeatType = EMusicalBeatType::Normal;
		CurrentSongSection = FSongSection();
	}

	bool IsZero() const	{ return FMath::IsNearlyZero(BeatsIncludingCountIn, UE_KINDA_SMALL_NUMBER); }

	// operators
	bool operator<(const FMidiSongPos& rhs) const;
	bool operator<=(const FMidiSongPos& rhs) const;
	bool operator>(const FMidiSongPos& rhs) const;
	bool operator>=(const FMidiSongPos& rhs) const;
	bool operator==(const FMidiSongPos& rhs) const;

private:
	void SetByTimeAndTick(float Ms, float Tick, const FSongMaps& Maps);
};
