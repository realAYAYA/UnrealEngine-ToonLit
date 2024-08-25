// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMidi/TempoMap.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/BeatMap.h"
#include "HarmonixMidi/ChordMap.h"
#include "HarmonixMidi/SectionMap.h"
#include "Sound/QuartzQuantizationUtilities.h"

#include "SongMaps.generated.h"

class IMidiReader;
class FStdMidiFileReader;
class FSongMapReceiver;
class UMidiFile;

UENUM()
enum class EMidiFileQuantizeDirection : uint8
{
	Nearest,
	Up,
	Down
};

UENUM(BlueprintType)
enum class EMidiClockSubdivisionQuantization : uint8
{
	Bar = static_cast<uint8>(EQuartzCommandQuantization::Bar),
	Beat = static_cast<uint8>(EQuartzCommandQuantization::Beat),
	ThirtySecondNote = static_cast<uint8>(EQuartzCommandQuantization::ThirtySecondNote),
	SixteenthNote = static_cast<uint8>(EQuartzCommandQuantization::SixteenthNote),
	EighthNote = static_cast<uint8>(EQuartzCommandQuantization::EighthNote),
	QuarterNote = static_cast<uint8>(EQuartzCommandQuantization::QuarterNote),
	HalfNote = static_cast<uint8>(EQuartzCommandQuantization::HalfNote),
	WholeNote = static_cast<uint8>(EQuartzCommandQuantization::WholeNote),
	DottedSixteenthNote = static_cast<uint8>(EQuartzCommandQuantization::DottedSixteenthNote),
	DottedEighthNote = static_cast<uint8>(EQuartzCommandQuantization::DottedEighthNote),
	DottedQuarterNote = static_cast<uint8>(EQuartzCommandQuantization::DottedQuarterNote),
	DottedHalfNote = static_cast<uint8>(EQuartzCommandQuantization::DottedHalfNote),
	DottedWholeNote = static_cast<uint8>(EQuartzCommandQuantization::DottedWholeNote),
	SixteenthNoteTriplet = static_cast<uint8>(EQuartzCommandQuantization::SixteenthNoteTriplet),
	EighthNoteTriplet = static_cast<uint8>(EQuartzCommandQuantization::EighthNoteTriplet),
	QuarterNoteTriplet = static_cast<uint8>(EQuartzCommandQuantization::QuarterNoteTriplet),
	HalfNoteTriplet = static_cast<uint8>(EQuartzCommandQuantization::HalfNoteTriplet),
	None = static_cast<uint8>(EQuartzCommandQuantization::None)
};

USTRUCT()
struct FSongLengthData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 LengthTicks = 0;
	UPROPERTY()
	float LengthFractionalBars = 0.0f;
	UPROPERTY()
	int32 LastTick = 0;

	bool operator==(const FSongLengthData& Other) const
	{
		return	LengthTicks == Other.LengthTicks &&
				LengthFractionalBars == Other.LengthFractionalBars && 
				LastTick == Other.LastTick;
	}
};

/**
 * FSongMaps encapsulates a number of other musical/midi map types
 * that are very useful for musical gameplay and interactivity. 
 * 
 * With this class and the current playback position of a piece of music you can
 * do things like determine the current Bar | Beat | Tick, song section, tempo,
 * chord, etc.
 */
USTRUCT(BlueprintType)
struct HARMONIXMIDI_API FSongMaps
{
	GENERATED_BODY()

public:
	FSongMaps();
	bool operator==(const FSongMaps& Other) const;

	void Init(int32 InTicksPerQuarterNote);
	void Copy(const FSongMaps& Other, int32 StartTick = 0, int32 EndTick = -1);

	// For importing...
	bool LoadFromStdMidiFile(const FString& FilePath);
	bool LoadFromStdMidiFile(void* Buffer, int32 BufferSize, const FString& Filename);
	bool LoadFromStdMidiFile(TSharedPtr<FArchive> Archive, const FString& Filename);

	float TickToMs(float Tick) const;
	float MsToTick(float Ms) const;
	float GetCountInSeconds() const;

	// tracks
	TArray<FString>&       GetTrackNames() { return TrackNames; }
	const TArray<FString>& GetTrackNames() const { return TrackNames; }
	FString                GetTrackName(int32 Index) const;
	bool                   TrackNamesIsEmpty() const { return TrackNames.IsEmpty(); }
	void                   EmptyTrackNames() { TrackNames.Empty(); }

	// tempo
	const FTempoInfoPoint* GetTempoInfoForMs(float Ms) const;
	const FTempoInfoPoint* GetTempoInfoForTick(int32 Tick) const;
	float                  GetTempoAtMs(float Ms) const;
	float                  GetTempoAtTick(int32 Tick) const;
	FTempoMap&             GetTempoMap() { return TempoMap; }
	const FTempoMap&       GetTempoMap() const { return TempoMap; }
	bool                   TempoMapIsEmpty() { return TempoMap.GetNumTempoChangePoints() == 0; }
	void                   EmptyTempoMap() { TempoMap.Empty(); }

	// pulses
	const FBeatMapPoint*  GetBeatAtMs(float Ms) const;
	float                 GetMsAtBeat(float Beat) const;
	const FBeatMapPoint*  GetBeatAtTick(int32 Tick) const;
	float                 GetMsPerBeatAtMs(float Ms) const;
	float                 GetMsPerBeatAtTick(int32 Tick) const;
	float                 GetFractionalBeatAtMs(float Ms) const;
	float                 GetFractionalBeatAtTick(float Tick) const;
	int32                 GetBeatIndexAtMs(float Ms) const;
	int32                 GetBeatIndexAtTick(int32 Tick) const;
	EMusicalBeatType      GetBeatTypeAtMs(float Ms) const;
	EMusicalBeatType      GetBeatTypeAtTick(int32 Tick) const;
	float                 GetBeatInPulseBarAtMs(float Ms) const;
	float                 GetBeatInPulseBarAtTick(float Tick) const;
	int32                 GetNumBeatsInPulseBarAtMs(float Ms) const ;
	int32                 GetNumBeatsInPulseBarAtTick(int32 Tick) const;
	FBeatMap&             GetBeatMap() { return BeatMap; }
	const FBeatMap&       GetBeatMap() const { return BeatMap; }
	bool                  BeatMapIsEmpty() { return BeatMap.GetNumMapPoints() == 0; }
	void                  EmptyBeatMap() { BeatMap.Empty(); }

	// bars
	const FTimeSignature* GetTimeSignatureAtMs(float Ms) const;
	const FTimeSignature* GetTimeSignatureAtTick(int32 Tick) const;
	const FTimeSignature* GetTimeSignatureAtBar(int32 Bar) const;
	float                 GetBarIncludingCountInAtMs(float Ms) const;
	float                 GetBarIncludingCountInAtTick(float Tick) const;
	float                 GetMsPerBarAtMs(float Ms) const;
	float                 GetMsPerBarAtTick(float Tick) const;
	FBarMap&              GetBarMap() { return BarMap; }
	const FBarMap&        GetBarMap() const { return BarMap; }
	bool                  BarMapIsEmpty() { return BarMap.GetNumTimeSignaturePoints() == 0; }
	void                  EmptyBarMap() { BarMap.Empty(); }
	void                  SetLengthTotalBars(int32 Bars);
	int32                 CalculateMidiTick(const FMusicTimestamp& Timestamp, const EMidiClockSubdivisionQuantization Quantize) const;
	int32                 SubdivisionToMidiTicks(const EMidiClockSubdivisionQuantization Division, const int32 AtTick) const;
	int32                 SubdivisionToMidiTicks(const EMidiClockSubdivisionQuantization Division, const FTimeSignature& TimeSignature) const;
	static float          SubdivisionToBeats(EMidiClockSubdivisionQuantization Subdivision, const FTimeSignature& TimeSignature);


	// sections
	float               GetSectionStartMsAtMs(float Ms) const;
	float               GetSectionEndMsAtMs(float Ms) const;
	const FSongSection* GetSectionAtMs(float Ms) const;
	const FSongSection* GetSectionAtTick(int32 Tick) const;
	const FSongSection* GetSectionWithName(const FString& Name) const;
	FString             GetSectionNameAtMs(float Ms) const;
	FString             GetSectionNameAtTick(int32 Tick) const;
	float               GetSectionLengthMsAtMs(float Ms) const;
	float               GetSectionLengthMsAtTick(int32 Tick) const;
	FSectionMap&        GetSectionMap() { return SectionMap; }
	const FSectionMap&  GetSectionMap() const { return SectionMap; }
	bool                SectionMapIsEmpty() { return SectionMap.GetNumSections() == 0; }
	void                EmptySectionMap() { SectionMap.Empty(); }

	// chords
	const FChordMapPoint* GetChordAtMs(float Ms) const;
	const FChordMapPoint* GetChordAtTick(int32 Tick) const;
	FName                 GetChordNameAtMs(float Ms) const;
	FName                 GetChordNameAtTick(int32 Tick) const;
	float                 GetChordLengthMsAtMs(float Ms) const;
	float                 GetChordLengthMsAtTick(int32 Tick) const;
	FChordProgressionMap& GetChordMap() { return ChordMap; }
	const FChordProgressionMap&  GetChordMap() const { return ChordMap; }
	bool                  ChordMapIsEmpty() const { return ChordMap.GetNumChords() == 0; }
	void                  EmptyChordMap() { ChordMap.Empty(); }

	void EmptyAllMaps();
	bool IsEmpty() const;

	FSongLengthData& GetSongLengthData() { return LengthData; }
	const FSongLengthData& GetSongLengthData() const { return LengthData; }
	float GetSongLengthMs() const;
	int32 GetSongLengthBeats() const;
	float GetSongLengthFractionalBars() const;

	int32 GetTicksPerQuarterNote() const { return TicksPerQuarterNote; }

	void SetSongLengthTicks(int32 NewLengthTicks);
	bool LengthIsAPerfectSubdivision() const;

	int32 QuantizeTickToAnyNearestSubdivision(int32 InTick, EMidiFileQuantizeDirection Direction, EMidiClockSubdivisionQuantization& Division) const;
	int32 QuantizeTickToNearestSubdivision(int32 InTick, EMidiFileQuantizeDirection Direction, EMidiClockSubdivisionQuantization Division) const;
	void GetTicksForNearestSubdivision(int32 InTick, EMidiClockSubdivisionQuantization Division, int32& LowerTick, int32& UpperTick) const;
	FString GetSongLengthString() const;

protected:
	friend class FSongMapReceiver;
	UPROPERTY()
	int32 TicksPerQuarterNote = Harmonix::Midi::Constants::GTicksPerQuarterNoteInt;
	UPROPERTY()
	FTempoMap TempoMap;
	UPROPERTY()
	FBarMap BarMap;
	UPROPERTY()
	FBeatMap BeatMap;
	UPROPERTY()
	FSectionMap SectionMap;
	UPROPERTY()
	FChordProgressionMap ChordMap;
	UPROPERTY()
	TArray<FString>	TrackNames;

private:
	UPROPERTY()
	FSongLengthData LengthData;

	void StringLengthToMT(const FString& LengthString, int32& OutBars, int32& OutTicks);
	bool ReadWithReader(FStdMidiFileReader& Reader);
	bool FinalizeRead(IMidiReader* Reader);
};

