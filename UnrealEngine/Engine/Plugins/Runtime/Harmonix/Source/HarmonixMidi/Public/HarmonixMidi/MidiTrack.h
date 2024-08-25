// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMidi/MidiEvent.h"
#include "HarmonixMidi/MidiMsg.h"

#include "MidiTrack.generated.h"

using FMidiEventList = TArray<FMidiEvent>;
class FMidiWriter;

/**
	* An FMidiTrack is a collection of FMidiEvents in chronological order.  
	* 
	* It can be created dynamically or be the end result of importing a 
	* standard midi file.
	*/
USTRUCT(BlueprintType, Meta = (DisplayName = "MIDI Track"))
struct HARMONIXMIDI_API FMidiTrack
{
	GENERATED_BODY()

public:
	FMidiTrack();
	FMidiTrack(const FString& name);

	bool operator==(const FMidiTrack& Other) const;

	const FMidiEventList& GetEvents() const;  		// this will ASSERT if events aren't sorted!
	const FMidiEventList& GetUnsortedEvents() const; // this won't sort

	/** Do not call unless you know what you're doing! */
	FMidiEventList& GetRawEvents();

	int32 GetNumEvents() const { return (int32)Events.Num(); }
	const FMidiEvent* GetEvent(int32 index) const { return &Events[index]; }

	void SetName(const FString& InName);
	const FString* GetName() const;

	void AddEvent(const FMidiEvent& Event);

	/**
		* This will not move the event's location in the list so that iterators into
		* Events do note get screwed up, BUT it CAN result in the midi events not being 
		* sorted. So if you call this function one or more times you should call Sort()
		* after doing so.
		* @see Sort
		*/
	void ChangeTick(FMidiEventList::TIterator Iterator, int32 NewTick);

	void WriteStdMidi(FMidiWriter& writer) const;

	void Sort();

	void Empty() { Events.Empty(); }
	void ClearEventsAfter(int32 Tick, bool IncludeTick);
	void ClearEventsBefore(int32 Tick, bool IncludeTick);

	int32 GetPrimaryMidiChannel() const { return PrimaryMidiChannel; }

	int32 CopyEvents(FMidiTrack& SourceTrack, int32 FromTick, int32 ThruTick, int32 TickOffset = 0,
		bool Clear = true, int32 MinNote = 0, int32 MaxNote = 127, int32 NoteTranspose = 0, bool FilterTrackName = true);

	const FString& GetTextAtIndex(int32 Index) const
	{
		return Strings[Index];
	}

	uint16 AddText(const FString& Str) { return (uint16)Strings.AddUnique(Str); }

	FMidiTextRepository* GetTextRepository() { return &Strings; }
	const FMidiTextRepository* GetTextRepository() const { return &Strings; }
	FString GetTextForMsg(const FMidiMsg& Message) const { check(Message.MsgType() == FMidiMsg::EType::Text); return GetTextAtIndex(Message.GetTextIndex()); }

	SIZE_T GetAllocatedSize() const;

private:
	UPROPERTY()
	TArray<FMidiEvent> Events; // All the midi events
	UPROPERTY()
	bool Sorted;               // Are the events sorted.
	UPROPERTY()
	int32 PrimaryMidiChannel;  // The midi channel of the first event on the track!
	UPROPERTY()
	TArray<FString> Strings;
};
