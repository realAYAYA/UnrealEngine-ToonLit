// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiPlayCursorTracker.h"
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMidi/MidiSongPos.h"

class FMidiPlayCursor;

class HARMONIXMIDI_API FMidiPlayCursorMgr
{
public:
	FMidiPlayCursorMgr();
	virtual ~FMidiPlayCursorMgr();

	void Reset();

	void AttachToTimeAuthority(const TSharedPtr<FMidiPlayCursorMgr>& InTimeAuthority);
	void DetachFromTimeAuthority() { TimeAuthority = nullptr; }

	void AttachToMidiResource(TSharedPtr<FMidiFileData> MidiDataProxy, bool ResetTrackersToStart = true, int32 PreRollBars = 0);
	void DetachFromMidiResource();

	FMidiSongPos CalculateLowResSongPosWithOffsetMs(float DeltaMs) const
	{
		return CalculateSongPosWithOffsetMs(DeltaMs, true);
	}

	FMidiSongPos CalculateHiResSongPosWithOffsetMs(float DeltaMs) const
	{
		return CalculateSongPosWithOffsetMs(DeltaMs, false);
	}

	FMidiSongPos CalculateSongPosWithOffsetMs(float Ms, bool IsLowRes) const;

	FMidiSongPos CalculateLowResSongPosRelativeToCurrentMs(float AbsoluteMs) const
	{
		return CalculateSongPosRelativeToCurrentMs(AbsoluteMs, true);
	}

	FMidiSongPos CalculateHiResSongPosRelativeToCurrentMs(float AbsoluteMs) const
	{
		return CalculateSongPosRelativeToCurrentMs(AbsoluteMs, false);
	}

	FMidiSongPos CalculateSongPosRelativeToCurrentMs(float AbsoluteMs, bool IsLowRes) const;

	//////////////////////////////////////////////////////////////////////////
	// Play cursor management
	//////////////////////////////////////////////////////////////////////////
	enum class ESetupOptions
	{
		NoBroadcastNoPreRoll,
		BroadcastImmediately,
		PreRollIfAtStartOrBroadcast,
		PreRollIfAtStartOrNoBroadcast
	};
	void  RegisterHiResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f);
	void  RegisterLowResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f);
	void  UnregisterPlayCursor(FMidiPlayCursor* PlayCursor, bool WarnOnFail = true);
	void  UnregisterAllPlayCursors();
	bool  HasLowResCursors() const;
	void  RecalculatePreRollDueToCursorPosition(FMidiPlayCursor* PlayCursor);
	void  GetCursorExtentsMs(float& Earliest, float& Latest) const;
	void  GetCursorExtentsTicks(int32& Earliest, int32& Latest) const;
	bool  CursorsAllInPhase() const;
	int32 GetFarthestAheadCursorTick() const;
	int32 GetFarthestBehindCursorTick() const;
	float GetBufferedMs() const;

	// Do we loop when we hit the end of the file?
	// Will function incorrectly if you change this after passing the end
	static const int32 kEndTick = -1;
	void SetLoop(int32 StartTick, int32 EndTick, bool IsDirectMappedFollower, bool IgnoringLookAhead);
	void ClearLoop(bool IgnoringLookAhead);

	void SeekTo(int32 Tick, int32 PreRollBars, bool IsRenderThread, bool IsLoop);
	void MoveToLoopStart();

	// NOTE: This next function DOES NOT affect how quickly the play cursor manager advances!
	// The manager is advanced as desired with calls to the Advance___ functions. This simply 
	// sets a member to inform the manager and the manager's cursors how quickly the driver of
	// this manager is advancing time as this might be useful information for some cursors
	// (eg. smoothing cursors)
	void  InformOfHiResAdvanceRate(float Rate) { GetHiResTracker().CurrentAdvanceRate = Rate; }
	float GetCurrentAdvanceRate(bool IsLowRes) const { return Trackers[IsLowRes].CurrentAdvanceRate; }
	float GetHiResAdvanceRate() const { return GetCurrentAdvanceRate(false); }
	float GetLowResAdvanceRate() const { return GetCurrentAdvanceRate(true); }

	//If broadcast is true, call callbacks for each midi event that we pass
	//If broadcast is false, silently update the internals of each play cursor
	void AdvanceHiResToMs(float Ms, bool Broadcast);
	void AdvanceHiResByDeltaMs(float Ms, bool Broadcast);
	void AdvanceHiResThruTick(int32 ThruTick, bool Broadcast, bool DontAdvancePastLoopEnd = false);
	void AdvanceHiResByDeltaTick(int32 NumTicks, bool Broadcast);

	// returns whether the cursors looped
	bool AdvanceLowResCursors();
	void UpdateLowResCursors(FMidiPlayCursorTracker& Tracker);

	void ResetTrackers(); //reset to the beginning of the song

	float GetLoopStartMs(bool IsLowRes) const { return Trackers[IsLowRes].LoopStartMs; }
	int32 GetLoopStartTick(bool IsLowRes) const { return Trackers[IsLowRes].LoopStartTick; }
	float GetLoopEndMs(bool IsLowRes) const { return Trackers[IsLowRes].LoopEndMs; }
	int32 GetLoopEndTick(bool IsLowRes) const { return Trackers[IsLowRes].LoopEndTick; }
	bool  DoesLoop(bool IsLowRes) const { return Trackers[IsLowRes].Loop; }

	bool  IsDirectMappedTimeFollower() const { return DirectMappedTimeFollower; }
	int32 GetLengthTicks() const { return LengthTicks; }

	bool   HasMidiFile() const;
	int32  FindTrackIndexByName(const FString& name) const;
	const FString* GetMidiFileName() const;

	const UMidiFile::FMidiTrackList& Tracks() const;

	const FSongMaps& GetSongMaps() const;
	const FTempoMap& GetTempoMap() const;
	const FBarMap& GetBarMap() const;

	int32 GetCurrentHiResTick() const   { return GetHiResTracker().CurrentTick;  }
	float GetCurrentHiResMs() const     { return GetHiResTracker().CurrentMs;    }
	float GetElapsedHiResMs() const     { return GetHiResTracker().ElapsedMs;    }
	int32 GetCurrentLowResTick() const  { return GetLowResTracker().CurrentTick; }
	float GetCurrentLowResMs() const    { return GetLowResTracker().CurrentMs;   }
	float GetElapsedLowResMs() const    { return GetLowResTracker().ElapsedMs;   }

	bool IsDone() const;

	void LockForMidiDataChanges();

	enum class EMidiChangePositionCorrectMode
	{
		MaintainTick,
		MaintainTime
	};

	void MidiDataChangeComplete(EMidiChangePositionCorrectMode positionMode, int32 PreRollBars = 0);
	
	/**
	 * @brief Get the music timestamp at an absolute time in milliseconds
	 * @param Ms - The time in milliseconds
	 * @return The music timestamp
	 */
	FMusicTimestamp GetMusicTimestampAtMs(float Ms) const;

protected:

	friend class FMidiPlayCursor;

	FMidiPlayCursorTracker Trackers[2]{ FMidiPlayCursorTracker(false), FMidiPlayCursorTracker(true) };

	FMidiPlayCursorTracker& GetHiResTracker() { return Trackers[0]; }
	FMidiPlayCursorTracker& GetLowResTracker() { return Trackers[1]; }

	const FMidiPlayCursorTracker& GetHiResTracker() const { return Trackers[0]; }
	const FMidiPlayCursorTracker& GetLowResTracker() const { return Trackers[1]; }

private:
	bool AdvanceTrackerByDeltaMs(float Ms, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast = true);
	void AdvanceTrackerThruTick(int32 ToTick, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast);

	void SetLoopImpl(int32 StartTick, int32 EndTick, bool IgnoringLookAhead, bool IsLowRes);
	void ClearLoopImpl(bool IgnoringLookAhead, bool IsLowRes);

	bool CursorsAllInPhaseImpl(bool IsLowRes) const;

	void DetermineLength();

	FSongMaps                 DefaultMaps;
	UMidiFile::FMidiTrackList DefaultTracks;

	TWeakPtr<FMidiPlayCursorMgr> TimeAuthority;

	TSharedPtr<FMidiFileData> MidiFileData;
	const FSongMaps* SongMaps;     // <-- This is a pointer to the actual midi song map data... allows for fast access

	float LengthMs;
	int32 LengthTicks;
	bool  DirectMappedTimeFollower;

	float MsSinceLowResUpdate;
	bool  HiResLoopedSinceLastLoResUpdate;
	bool  InMidiChangeLock;
};
