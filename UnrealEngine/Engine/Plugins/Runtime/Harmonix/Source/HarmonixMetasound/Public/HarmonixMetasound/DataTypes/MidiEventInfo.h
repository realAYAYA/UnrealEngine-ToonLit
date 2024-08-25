// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReferenceMacro.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/MidiMsg.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MidiEventInfo.generated.h"

USTRUCT(BlueprintType, Meta = (DisplayName = "MIDI Event Info"))
struct HARMONIXMETASOUND_API FMidiEventInfo
{
	GENERATED_BODY()

	UPROPERTY(Category = "Music", EditAnywhere, BlueprintReadWrite)
	FMusicTimestamp Timestamp{ 0, 0 };

	UPROPERTY(Category = "Music", EditAnywhere, BlueprintReadWrite)
	int32 TrackIndex{ 0 };

	FMidiMsg MidiMessage{ 0, 0, 0 };

	uint8 GetChannel() const;
	
	bool IsNote() const;
	bool IsNoteOn() const;
	bool IsNoteOff() const;
	uint8 GetNoteNumber() const;
	uint8 GetVelocity() const;
};

UCLASS()
class UMidiEventInfoBlueprintLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category="MetaSoundOutput")
	static bool IsMidiEventInfo(const FMetaSoundOutput& Output);
	
	UFUNCTION(BlueprintCallable, Category="MetaSoundOutput")
	static FMidiEventInfo GetMidiEventInfo(const FMetaSoundOutput& Output, bool& Success);

	UFUNCTION(BlueprintCallable, Category = "Music")
	static int32 GetChannel(const FMidiEventInfo& Event);
	
	UFUNCTION(BlueprintCallable, Category = "Music")
	static bool IsNote(const FMidiEventInfo& Event);

	UFUNCTION(BlueprintCallable, Category = "Music")
	static bool IsNoteOn(const FMidiEventInfo& Event);

	UFUNCTION(BlueprintCallable, Category = "Music")
	static bool IsNoteOff(const FMidiEventInfo& Event);

	UFUNCTION(BlueprintCallable, Category = "Music")
	static int32 GetNoteNumber(const FMidiEventInfo& Event);

	UFUNCTION(BlueprintCallable, Category = "Music")
	static int32 GetVelocity(const FMidiEventInfo& Event);
};

DECLARE_METASOUND_DATA_REFERENCE_TYPES(FMidiEventInfo, HARMONIXMETASOUND_API, FMidiEventInfoTypeInfo, FMidiEventInfoReadRef, FMidiEventInfoWriteRef);
