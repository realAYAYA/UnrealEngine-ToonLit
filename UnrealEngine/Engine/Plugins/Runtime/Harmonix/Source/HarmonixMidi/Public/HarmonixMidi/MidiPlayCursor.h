// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/IntrusiveDoubleLinkedList.h"
#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiReceiver.h"
#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include "Misc/EnumClassFlags.h"

struct FMidiPlayCursorTracker;

class  UMidiFile;

class HARMONIXMIDI_API FMidiPlayCursor : public TIntrusiveDoubleLinkedListNode<FMidiPlayCursor>
{
public:
	using FMidiPlayCurorListType = TIntrusiveDoubleLinkedList<FMidiPlayCursor>;

	FMidiPlayCursor();
	virtual ~FMidiPlayCursor();

	virtual void Reset(bool ForceNoBroadcast = false);
	virtual bool IsDone() const;

	virtual bool UpdateWithTrackerUnchanged() { return !UnregisterASAP; }

	bool Advance(bool IsLowRes = true);
	bool AdvanceAsPreRoll();

	virtual void AdvanceByTicks(bool ProcessLoops = true, bool Broadcast = true, bool IsPreRoll = false);
	virtual void AdvanceByMs(bool ProcessLoops = true, bool Broadcast = true, bool IsPreRoll = false);

	int32 GetLookaheadTicks() { return LookaheadTicks; }
	float GetLookaheadMs() { return LookaheadMs; }

	enum class EFilterPassFlags : uint32
	{
		None = 0,
		MidiMessage = 1 << 0,
		Text = 1 << 1,
		Tempo = 1 << 2,
		TimeSig = 1 << 3,
		Reset = 1 << 4,
		Loop = 1 << 5,
		PreRollNoteOn = 1 << 6,
		All = 0xFFFFFFFF
	};

	enum class ELookaheadType
	{
		Ticks = 0,
		Time = 1
	};

	enum class ESyncOptions
	{
		NoBroadcastNoPreRoll,
		BroadcastImmediately,
		PreRollIfAtStartOrBroadcast,
		PreRollIfAtStartOrNoBroadcast
	};

	void SetupMsLookahead(float Ms, ESyncOptions Opts);
	void SetupTickLookahead(int32 Ticks, ESyncOptions Opts);

	ELookaheadType GetLookaheadType() { return LookaheadType; }

	int32  GetCurrentTick() { return CurrentTick; }

	void SetMessageFilter(EFilterPassFlags InFilterPassFlags) { FilterPassFlags = InFilterPassFlags; }
	void SetWatchTrack(int32 TrackIndex) { WatchTrack = TrackIndex; }
	virtual void ManagerIsDetaching() {}

#if HARMONIX_MIDIPLAYCURSOR_ENABLE_ENSURE_OWNER
	FMidiPlayCursorMgr* GetOwner();
#else
	FMidiPlayCursorMgr* GetOwner() { return Owner; }
#endif
	const FMidiPlayCursorMgr* GetOwner() const { return const_cast<FMidiPlayCursor*>(this)->GetOwner(); }

	static const float kSmallMs;

protected:
	//////////////////////////////////////////////////////////////////////////
	// Implement the following in your subclass to process callbacks
	//////////////////////////////////////////////////////////////////////////
	// Standard 1- or 2-byte MIDI message:
	virtual void OnMidiMessage(int32 TrackIndex, int32 Tick, uint8 Status, uint8 Data1, uint8 Data2, bool IsPreroll = false) {}
	// Text, Copyright, TrackName, InstName, Lyric, Marker, CuePoint meta-event:
	//  "type" is the type of meta-event (constants defined in
	//  MidiFileConstants.h)
	virtual void OnText(int32 TrackIndex, int32 Tick, int32 TextIndex, const FString& Str, uint8 Type, bool IsPreroll = false) {}
	// Tempo Change meta-event:
	//  tempo is in microseconds per quarter-note
	virtual void OnTempo(int32 TrackIndex, int32 Tick, int32 Tempo, bool IsPreroll = false) {}
	// Time Signature meta-event:
	//   time signature is numer/denom
	virtual void OnTimeSig(int32 TrackIndex, int32 Tick, int32 Numerator, int32 Denominator, bool IsPreroll = false) {}
	// Called when the look ahead amount changes or midi playback resets...
	virtual void OnReset() {}
	// Called when the cursor is about to loop back (or forward) from the loop end point to the loop start point
	virtual void OnLoop(int32 LoopStartTick, int32 LoopEndTick) {}
	// Called when a noteOn happens during a pre-roll. Example usage...
	// A cursor playing a synth that supports note-ons with start offsets into playback may
	// pass this note on to the synth with the preRollMs as the start offset.
	virtual void OnPreRollNoteOn(int32 TrackIndex, int32 EventTick, int32 Tick, float PreRollMs, uint8 Status, uint8 Data1, uint8 Data2) {}

protected:
	float CurrentMs = 0.0f; // We've broadcast all events up through this Ms
	int32 CurrentTick = 0;  // We've broadcast all events up to and including this tick
	int32 LoopCount = 0;
	bool  UnregisterASAP = false;
private:
	FMidiPlayCursorMgr* Owner = nullptr;
protected:
	FMidiPlayCursorTracker* Tracker = nullptr;

protected:
	virtual void SeekToTick(int32 Tick);
	virtual void SeekThruTick(int32 Tick);
	virtual void AdvanceThruTick(int32 Tick, bool IsPreRoll);

private:
	friend struct FMidiPlayCursorTracker;

	void MoveToLoopStartIfAtNotOffset(int32 NewThruTick, float NewThruMs);

	void PrepareLookAheadTicks(bool ForceNoBroadcast = false);
	void PrepareLookAheadMs(bool ForceNoBroadcast = false);

	void DoAdvanceForLaggingTickCursor(bool Broadcast, bool IsPreRoll = false);
	void DoAdvanceForLeadingTickCursor(bool Broadcast, bool ProcessLoops, bool IsPreRoll = false);
	void DoAdvanceForLaggingMsCursor(bool Broadcast, bool IsPreRoll = false);
	void DoAdvanceForLeadingMsCursor(bool Broadcast, bool ProcessLoops, bool IsPreRoll = false);

	TArray<int32> TrackNextEventIndexs;
	ELookaheadType LookaheadType;
	ESyncOptions   SyncOpts;
	int32  LookaheadTicks;             // positive is ahead, negative is behind
	float  LookaheadMs;
	int32  WatchTrack;
	EFilterPassFlags FilterPassFlags;

	friend class FMidiPlayCursorMgr;
	void SetOwner(FMidiPlayCursorMgr* NewOwner, FMidiPlayCursorTracker* NewTracker, float PreRollMs = -1.0f);

	void BroadcastEvent(int32 TrackIndex, const FMidiEvent& MidiEvent, bool IsPreroll = false);

	void RecalcNextEventsDueToMidiChanges(FMidiPlayCursorMgr::EMidiChangePositionCorrectMode PositionMode);
};

ENUM_CLASS_FLAGS(FMidiPlayCursor::EFilterPassFlags)
