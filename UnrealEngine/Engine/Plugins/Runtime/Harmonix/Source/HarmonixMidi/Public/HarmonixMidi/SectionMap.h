// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMidi/MusicMapBase.h"
#include "HarmonixMidi/MidiConstants.h"

#include "SectionMap.generated.h"

/**
 * A section in a piece of music has a name, a starting point, and a length
 */
USTRUCT(BlueprintType)
struct HARMONIXMIDI_API FSongSection : public FMusicMapTimespanBase
{
	GENERATED_BODY()

public:
	static constexpr bool DefinedAsRegions = true;

	FSongSection() {}

	FSongSection(const FString& InName, int32 InStartTick, int32 InLengthTicks = 1)
		: FMusicMapTimespanBase(InStartTick, InLengthTicks)
		, Name(InName)
	{}

	bool operator==(const FSongSection& Other) const
	{
		return Name == Other.Name;
	}

	UPROPERTY(BlueprintReadOnly, Category = "SongSection")
	FString Name;
};

/**
 * A map of sections in a piece of music
 */
USTRUCT()
struct HARMONIXMIDI_API FSectionMap
{
	GENERATED_BODY()

public:
	FSectionMap()
		: TicksPerQuarterNote(Harmonix::Midi::Constants::GTicksPerQuarterNoteInt)
	{}
	bool operator==(const FSectionMap& Other) const;

	void Finalize(int32 LastTick);

	void Empty();
	void Copy(const FSectionMap& Other, int32 StartTick = 0, int32 EndTick = -1);
	bool IsEmpty() const;

	/** Called by the midi file importer before map points are added to this map */
	void SetTicksPerQuarterNote(int32 InTicksPerQuarterNote)
	{
		TicksPerQuarterNote = InTicksPerQuarterNote;
	}

	bool AddSection(const FString& Name, int32 StartTick, int32 LengthTicks, bool SortNow = true);

	int32 TickToSectionIndex(int32 Tick) const;
	const FSongSection* TickToSection(int32 Tick) const;
	int32 GetSectionStartTick(const FString& Name) const;
	int32 GetSectionStartTick(int32 SectionIndex) const;

	const TArray<FSongSection>& GetSections() const { return Points; }
	int32 GetNumSections() const { return Points.Num(); }
	const FSongSection* GetSection(int32 SectionIndex) const;
	FString GetSectionName(int32 SectionIndex) const;
	FString GetSectionNameAtTick(int32 Tick) const;
	void  GetSectionNames(TArray<FString>& Names) const;
	int32   FindSectionIndex(const FString& Name) const;
	const FSongSection* FindSectionInfo(const FString& Name) const;

protected:
	UPROPERTY()
	int32 TicksPerQuarterNote;
	UPROPERTY()
	TArray<FSongSection> Points;
};

