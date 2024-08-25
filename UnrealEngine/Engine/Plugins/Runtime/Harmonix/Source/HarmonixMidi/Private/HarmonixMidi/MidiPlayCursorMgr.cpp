// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include "HarmonixMidi/MidiPlayCursor.h"

#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiReader.h"
#include "HarmonixMidi/MidiTrack.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/TempoMap.h"

FMidiPlayCursorMgr::FMidiPlayCursorMgr()
	: SongMaps(&DefaultMaps)
	, LengthMs(0.f)
	, LengthTicks(0)
	, DirectMappedTimeFollower(false)
	, MsSinceLowResUpdate(0.0f)
	, HiResLoopedSinceLastLoResUpdate(false)
	, InMidiChangeLock(false)
{
	// setup the default tempo map to have one entry...
	DefaultMaps.GetTempoMap().AddTempoInfoPoint(Harmonix::Midi::Constants::BPMToMidiTempo(120.0f), 0);
	DefaultMaps.GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, 4, 4);
}

void FMidiPlayCursorMgr::Reset()
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	UnregisterAllPlayCursors();
	ResetTrackers();
	MidiFileData = nullptr;
	SongMaps = &DefaultMaps;
	LengthMs = 0.f;
	LengthTicks = 0;
	DirectMappedTimeFollower = false;

	for (FMidiPlayCursorTracker& Tracker : Trackers)
	{
		Tracker.CurrentAdvanceRate = 1.f;
		Tracker.LoopOffsetTick = 0.f;
		Tracker.LoopStartMs = 0.f;
		Tracker.LoopStartTick = 0;
		Tracker.LoopEndMs = 0.f;
		Tracker.LoopEndTick = 0;
		Tracker.Loop = false;
		Tracker.LoopIgnoringLookAhead = false;
	}

	MsSinceLowResUpdate = 0.0f;
	HiResLoopedSinceLastLoResUpdate = false;
	InMidiChangeLock = false;
}

void FMidiPlayCursorMgr::AttachToTimeAuthority(const TSharedPtr<FMidiPlayCursorMgr>& InTimeAuthority)
{
	TimeAuthority = InTimeAuthority;
}

void FMidiPlayCursorMgr::AttachToMidiResource(TSharedPtr<FMidiFileData> InMidiFileData, bool ResetTrackersToStart, int32 PreRollBars)
{
	LockForMidiDataChanges();
	MidiFileData = InMidiFileData;
	if (MidiFileData)
	{
		SongMaps = &MidiFileData->SongMaps;
	}
	else
	{
		SongMaps = &DefaultMaps;
	}
	if (ResetTrackersToStart)
	{
		ResetTrackers();
	}
	MidiDataChangeComplete(EMidiChangePositionCorrectMode::MaintainTick, PreRollBars);
}

void FMidiPlayCursorMgr::DetachFromMidiResource()
{
	LockForMidiDataChanges();
	MidiFileData = nullptr;
	SongMaps = &DefaultMaps;
	ResetTrackers();
	MidiDataChangeComplete(EMidiChangePositionCorrectMode::MaintainTick);
}

FMidiSongPos FMidiPlayCursorMgr::CalculateSongPosRelativeToCurrentMs(float AbsoluteMs, bool IsLowRes) const
{
	FMidiSongPos OutSongPos;
	if (DoesLoop(IsLowRes))
	{
		float LoopStartMs = GetLoopStartMs(IsLowRes), LoopEndMs = GetLoopEndMs(IsLowRes);
		float LoopLengthMs = LoopEndMs - LoopStartMs;
		float MappedMs = LoopStartMs + FMath::Fmod(AbsoluteMs - LoopStartMs, LoopLengthMs);
		OutSongPos.SetByTime(MappedMs, GetSongMaps());
	}
	else
	{
		OutSongPos.SetByTime(AbsoluteMs, GetSongMaps());
	}

	// currently only use the time authority for the tempo
	if (TSharedPtr<FMidiPlayCursorMgr> TimeAuthorityPtr = TimeAuthority.Pin())
	{
		float DeltaMs = AbsoluteMs - (IsLowRes ? GetCurrentLowResMs() : GetCurrentHiResMs());

		FMidiSongPos AuthoritySongPos = TimeAuthorityPtr->CalculateSongPosWithOffsetMs(DeltaMs, IsLowRes);

		OutSongPos.Tempo = AuthoritySongPos.Tempo;
	}

	return OutSongPos;
}

FMidiSongPos FMidiPlayCursorMgr::CalculateSongPosWithOffsetMs(float DeltaMs, bool IsLowRes) const
{
	float AbsoluteMs = (IsLowRes ? GetCurrentLowResMs() : GetCurrentHiResMs()) + DeltaMs;
	return CalculateSongPosRelativeToCurrentMs(AbsoluteMs, IsLowRes);
}

void FMidiPlayCursorMgr::DetermineLength()
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	if (!MidiFileData)
	{
		LengthMs = 0.0f;
		LengthTicks = 0;
		for (FMidiPlayCursorTracker& Tracker : Trackers)
		{
			Tracker.LoopEndMs = 0.0f;
			Tracker.LoopEndTick = 0;
			Tracker.LoopStartMs = 0.0f;
			Tracker.LoopStartTick = 0;
		}
	}
	else
	{
		//Determine file length in ms and ticks
		LengthTicks = 0;
		const TArray<FMidiTrack>& Tracks = MidiFileData->Tracks;
		int32 NumTracks = Tracks.Num();
		for (int32 i = 0; i < NumTracks; ++i)
		{
			LengthTicks = FMath::Max(LengthTicks, Tracks[i].GetEvents().Last().GetTick());
		}
		//Round file length up to the nearest bar
		int32 Bar = FMath::CeilToInt32(SongMaps->GetBarMap().TickToFractionalBarIncludingCountIn(LengthTicks));
		LengthTicks = SongMaps->GetBarMap().BarIncludingCountInToTick(Bar);
		LengthMs = SongMaps->GetTempoMap().TickToMs(LengthTicks);

		for (FMidiPlayCursorTracker& Tracker : Trackers)
		{
			Tracker.LoopEndMs = LengthMs;
			Tracker.LoopEndTick = LengthTicks;
			Tracker.LoopStartMs = 0.0f;
			Tracker.LoopStartTick = 0;
		}
	}
}

FMidiPlayCursorMgr::~FMidiPlayCursorMgr()
{
	UnregisterAllPlayCursors();
}

void FMidiPlayCursorMgr::RegisterHiResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs)
{
	if (PlayCursor->GetOwner())
	{
		PlayCursor->Owner->UnregisterPlayCursor(PlayCursor);
	}
	PlayCursor->UnregisterASAP = false;
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	PlayCursor->SetOwner(this, &GetHiResTracker(), PreRollMs);
	GetHiResTracker().AddCursor(PlayCursor);
}

void FMidiPlayCursorMgr::RegisterLowResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs)
{
	if (PlayCursor->GetOwner())
	{
		PlayCursor->Owner->UnregisterPlayCursor(PlayCursor);
	}
	PlayCursor->UnregisterASAP = false;
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	PlayCursor->SetOwner(this, &GetLowResTracker(), PreRollMs);
	GetLowResTracker().AddCursor(PlayCursor);
}

void FMidiPlayCursorMgr::RecalculatePreRollDueToCursorPosition(FMidiPlayCursor* PlayCursor)
{
	float PreRollMs = 0.0f;
	if (PlayCursor->GetLookaheadType() == FMidiPlayCursor::ELookaheadType::Ticks)
	{
		PreRollMs = -GetTempoMap().TickToMs(-PlayCursor->GetLookaheadTicks());
	}
	else
	{
		PreRollMs = PlayCursor->GetLookaheadMs();
	}
	if (-PreRollMs < GetHiResTracker().CurrentMs)
	{
		// yup... earliest look ahead!
		int32 NewTick = GetTempoMap().MsToTick(-PreRollMs);

		// back up one tick...
		NewTick--;
		PreRollMs = GetTempoMap().TickToMs(NewTick);
		for (FMidiPlayCursorTracker& Tracker : Trackers)
		{
			Tracker.Reset(NewTick, PreRollMs, false);
		}
	}
}

void FMidiPlayCursorMgr::UnregisterPlayCursor(FMidiPlayCursor* PlayCursor, bool WarnOnFail)
{
	// search the hi-res list... then the low res...
	for (int32 Index = 0; Index != 2; ++Index)
	{
		FMidiPlayCursorListLock CursorListLock(Trackers[Index].CursorListCS);
		if (Trackers[Index].TraversingCursors)
		{
			if (PlayCursor->Tracker == &Trackers[Index])
			{
				UE_LOG(LogMIDI, Error, TEXT("You cannot unregister a MidiPlayCursor while the cursor's manager is traversing its cursor list!"));
				return;
			}
			continue;
		}
		if (Trackers[Index].RemoveCursor(PlayCursor))
		{
			// it was removed from one or the other lists...
			PlayCursor->SetOwner(nullptr, nullptr);
			return;
		}
	}
	if (WarnOnFail)
	{
		UE_LOG(LogMIDI, Warning, TEXT("Attempt to remove MidiPlayCursor from a manager that doesn't own it!"));
	}
}

void FMidiPlayCursorMgr::UnregisterAllPlayCursors()
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

	if (GetHiResTracker().TraversingCursors || GetLowResTracker().TraversingCursors)
	{
		UE_LOG(LogMIDI, Error, TEXT("You cannot unregister all MidiPlayCursors while this manager is currently traversing it's cursor list!"));
		return;
	}

	GetHiResTracker().TraversingCursors = true;
	for (auto it = GetHiResTracker().Cursors.begin(); it != GetHiResTracker().Cursors.end(); ++it)
	{
		it.GetNode()->ManagerIsDetaching();
		it.GetNode()->SetOwner(nullptr, nullptr);
	}
	GetHiResTracker().Clear();
	GetHiResTracker().TraversingCursors = false;
	GetLowResTracker().TraversingCursors = true;
	for (auto it = GetLowResTracker().Cursors.begin(); it != GetLowResTracker().Cursors.end(); ++it)
	{
		it.GetNode()->ManagerIsDetaching();
		it.GetNode()->SetOwner(nullptr, nullptr);
	}
	GetLowResTracker().Clear();
	GetLowResTracker().TraversingCursors = false;
}

bool FMidiPlayCursorMgr::HasLowResCursors() const
{
	return !GetLowResTracker().Cursors.IsEmpty();
}

void FMidiPlayCursorMgr::ResetTrackers()
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	float Ms = GetTempoMap().TickToMs(-1);
	for (FMidiPlayCursorTracker& Tracker : Trackers)
	{
		Tracker.Reset(-1, Ms, false);
	}
	MsSinceLowResUpdate = 0.0f;
	HiResLoopedSinceLastLoResUpdate = false;
}

bool FMidiPlayCursorMgr::HasMidiFile() const
{
	return MidiFileData.IsValid();
}

int32 FMidiPlayCursorMgr::FindTrackIndexByName(const FString& name) const
{
	return MidiFileData.IsValid() ? MidiFileData->FindTrackIndexByName(name) : INDEX_NONE;
}

const FString* FMidiPlayCursorMgr::GetMidiFileName() const
{
	if (!MidiFileData)
	{
		return nullptr;
	}

	return &MidiFileData->MidiFileName;
}

const UMidiFile::FMidiTrackList& FMidiPlayCursorMgr::Tracks() const
{
	return MidiFileData.IsValid() ? MidiFileData->Tracks : DefaultTracks;
}

const FSongMaps& FMidiPlayCursorMgr::GetSongMaps() const
{
	check(SongMaps);
	return *SongMaps;
}

const FTempoMap& FMidiPlayCursorMgr::GetTempoMap() const
{
	check(SongMaps);
	return SongMaps->GetTempoMap();
}

const FBarMap& FMidiPlayCursorMgr::GetBarMap() const
{
	check(SongMaps);
	return SongMaps->GetBarMap();
}

void FMidiPlayCursorMgr::GetCursorExtentsMs(float& Earliest, float& Latest) const
{
	Earliest = (GetHiResTracker().EarliestCursorMs > GetLowResTracker().EarliestCursorMs) ?
		GetHiResTracker().EarliestCursorMs : GetLowResTracker().EarliestCursorMs;
	Latest = (GetHiResTracker().LatestCursorMs < GetLowResTracker().LatestCursorMs) ?
		GetHiResTracker().LatestCursorMs : GetLowResTracker().LatestCursorMs;
}

void FMidiPlayCursorMgr::GetCursorExtentsTicks(int32& Earliest, int32& Latest) const
{
	Earliest = (GetHiResTracker().EarliestCursorTick > GetLowResTracker().EarliestCursorTick) ?
		GetHiResTracker().EarliestCursorTick : GetLowResTracker().EarliestCursorTick;
	Latest = (GetHiResTracker().LatestCursorTick < GetLowResTracker().LatestCursorTick) ?
		GetHiResTracker().LatestCursorTick : GetLowResTracker().LatestCursorTick;
}

bool FMidiPlayCursorMgr::CursorsAllInPhase() const
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

	return CursorsAllInPhaseImpl(false) && CursorsAllInPhaseImpl(true);
}

bool FMidiPlayCursorMgr::CursorsAllInPhaseImpl(bool IsLowRes) const
{
	const FMidiPlayCursorTracker& Tracker = Trackers[IsLowRes];

	if (!Tracker.Loop)
	{
		return true;
	}

	// Check tick driven...
	int32 EarliestTick = Tracker.EarliestCursorTick, LatestTick = Tracker.LatestCursorTick;
	if ((Tracker.CurrentTick < Tracker.LoopEndTick && Tracker.CurrentTick + EarliestTick > Tracker.LoopEndTick) ||
		(Tracker.CurrentTick > Tracker.LoopStartTick && Tracker.CurrentTick + LatestTick < Tracker.LoopStartTick))
	{
		return false;
	}

	// check time driven...
	float EarliestMs = Tracker.EarliestCursorMs, LatestMs = Tracker.LatestCursorMs;
	if ((Tracker.CurrentMs < Tracker.LoopEndMs && Tracker.CurrentMs + EarliestMs > Tracker.LoopEndMs) ||
		(Tracker.CurrentMs > Tracker.LoopStartMs && Tracker.CurrentMs + LatestMs < Tracker.LoopStartMs))
	{
		return false;
	}

	return true;
}

int32 FMidiPlayCursorMgr::GetFarthestAheadCursorTick() const
{
	int32 Hrt = GetHiResTracker().GetFarthestAheadCursorTick();
	int32 Lrt = GetLowResTracker().GetFarthestAheadCursorTick();
	return((Lrt > Hrt) ? Lrt : Hrt);
}

int32 FMidiPlayCursorMgr::GetFarthestBehindCursorTick() const
{
	int32 Hrt = GetHiResTracker().GetFarthestBehindCursorTick();
	int32 Lrt = GetLowResTracker().GetFarthestBehindCursorTick();
	return((Lrt < Hrt) ? Lrt : Hrt);
}

float FMidiPlayCursorMgr::GetBufferedMs() const
{
	return (GetHiResTracker().EarliestCursorMs > GetLowResTracker().EarliestCursorMs) ? GetHiResTracker().EarliestCursorMs : GetLowResTracker().EarliestCursorMs;
}

void FMidiPlayCursorMgr::SetLoop(int32 StartTick, int32 EndTick, bool IsDirectMappedFollower, bool IgnoringLookAhead)
{
	// NOTE:
	// TODO: There is a missing test here that would check to see if the proposed loop end
	// might fall in the middle of the current span of leading and lagging play cursors,
	// which would result in ugly behavior!

	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	DirectMappedTimeFollower = IsDirectMappedFollower;
	SetLoopImpl(StartTick, EndTick, IgnoringLookAhead, false);
}

void FMidiPlayCursorMgr::ClearLoop(bool IgnoringLookAhead)
{
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	ClearLoopImpl(IgnoringLookAhead, false);
}

void FMidiPlayCursorMgr::SetLoopImpl(int32 StartTick, int32 EndTick, bool IgnoringLookAhead, bool IsLowRes)
{
	// NOTE:
	// TODO: There is a missing test here that would check to see if the proposed loop end
	// might fall in the middle of the current span of leading and lagging play cursors,
	// which would result in ugly behavior!

	const FTempoMap& TempoMap = GetTempoMap();
	Trackers[IsLowRes].LoopStartTick = StartTick;
	Trackers[IsLowRes].LoopStartMs = TempoMap.TickToMs(StartTick);
	if (EndTick == kEndTick)
	{
		EndTick = LengthTicks;
	}
	Trackers[IsLowRes].LoopEndTick = EndTick;
	Trackers[IsLowRes].LoopEndMs = TempoMap.TickToMs(EndTick);
	Trackers[IsLowRes].LoopIgnoringLookAhead = IgnoringLookAhead;
	Trackers[IsLowRes].Loop = true;
	bool CursorsInPhase = CursorsAllInPhaseImpl(IsLowRes);
	if (!CursorsInPhase)
	{
		if (IgnoringLookAhead)
		{
			Trackers[IsLowRes].Reset(Trackers[IsLowRes].CurrentTick, Trackers[IsLowRes].CurrentMs, false);
		}
		else
		{
			UE_LOG(LogMIDI, Warning, TEXT("FMidiPlayCursorMgr::SetLoop : not ignoring look ahead, but cursors are not in phase!"));
		}
	}
}

void FMidiPlayCursorMgr::ClearLoopImpl(bool IgnoringLookAhead, bool IsLowRes)
{
	Trackers[IsLowRes].LoopIgnoringLookAhead = IgnoringLookAhead;
	Trackers[IsLowRes].Loop = false; // Bug?  CursorsAllInPhaseImpl will always return true.  Should this be set after calling?
	bool CursorsInPhase = CursorsAllInPhaseImpl(IsLowRes);
	if (!CursorsInPhase)
	{
		if (IgnoringLookAhead)
		{
			Trackers[IsLowRes].Reset(Trackers[IsLowRes].CurrentTick, Trackers[IsLowRes].CurrentMs, false);
		}
		else
		{
			UE_LOG(LogMIDI, Warning, TEXT("FMidiPlayCursorMgr::ClearLoop : not ignoring look ahead, but cursors are not in phase!"));
		}
	}
}

void FMidiPlayCursorMgr::SeekTo(int32 Tick, int32 PreRollBars, bool IsRenderThread, bool IsLoop)
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

	// Back up one tick, as we are going to be setting the cursors'
	// 'played through' position.
	Tick--;
	float NewPosMs = GetTempoMap().TickToMs(Tick);
	float PreRollStartMs = NewPosMs;
	int32 PreRollStartTick = Tick;
	if (PreRollBars > 0)
	{
		const FBarMap& Map = GetBarMap();
		float Bar = Map.TickToFractionalBarIncludingCountIn(Tick);
		Bar -= (float)PreRollBars;

		if (Bar < 0.0f)
		{
			PreRollStartTick = -1;
		}
		else
		{
			PreRollStartTick = Map.BarIncludingCountInToTick(Bar);
		}

		PreRollStartMs = GetTempoMap().TickToMs(PreRollStartTick);

		if (PreRollStartTick > Tick)
		{
			PreRollStartTick = Tick;
			PreRollStartMs = NewPosMs;
		}
	}

	if (IsLoop)
	{
		HiResLoopedSinceLastLoResUpdate = true;
	}

	// The hi-res cursors can just slam to the new position.
	// If this is a DirectMappedTimeFollower, the hi-res cursors
	// were already advanced to the end of the loop.
	GetHiResTracker().Reset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	if (!IsRenderThread)
	{
		GetLowResTracker().Reset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	}
	else
	{
		GetLowResTracker().QueueReset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	}
}

void FMidiPlayCursorMgr::MoveToLoopStart()
{
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	{
		if (ensureMsgf(GetHiResTracker().Loop, TEXT("That's odd. Asked to move to the beginning of the loop... but there is no loop!")))
		{
			const FTempoMap& TempoMap = GetTempoMap();

			int32 NewThruTick = GetHiResTracker().LoopStartTick - 1;
			float NewThruMs = TempoMap.TickToMs(NewThruTick);

			// Move the hi-res cursor to the loop start
			GetHiResTracker().MoveToLoopStart(NewThruTick, NewThruMs);

			HiResLoopedSinceLastLoResUpdate = true;

			// The low res cursor will need to advance to the end of the loop, and then
			// advance from loop-start to current position at some point in the future
			// (during a low-res frame poll)
			FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);
			MsSinceLowResUpdate += GetHiResTracker().LoopEndMs - GetLowResTracker().CurrentMs;
		}
	}
}

bool FMidiPlayCursorMgr::IsDone() const
{
	if (GetHiResTracker().Loop || GetLowResTracker().Loop)
	{
		return false;
	}

	GetHiResTracker().TraversingCursors = true;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstHiResCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(GetHiResTracker().Cursors);
	for (auto it = NonConstHiResCursorList.begin(); it != NonConstHiResCursorList.end(); it++)
	{
		if (!it.GetNode()->IsDone())
		{
			GetHiResTracker().TraversingCursors = false;
			return false;
		}
	}
	GetHiResTracker().TraversingCursors = false;
	GetLowResTracker().TraversingCursors = true;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstLowResCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(GetLowResTracker().Cursors);
	for (auto it = NonConstLowResCursorList.begin(); it != NonConstLowResCursorList.end(); it++)
	{
		if (!it.GetNode()->IsDone())
		{
			GetLowResTracker().TraversingCursors = false;
			return false;
		}
	}
	GetLowResTracker().TraversingCursors = false;
	return true;
}

void FMidiPlayCursorMgr::LockForMidiDataChanges()
{
	if (ensureAlways(!InMidiChangeLock))
	{
		InMidiChangeLock = true;
		GetLowResTracker().CursorListCS.Lock();
		GetHiResTracker().CursorListCS.Lock();
	}
}

void FMidiPlayCursorMgr::MidiDataChangeComplete(EMidiChangePositionCorrectMode PositionMode, int32 PreRollBars)
{
	if (ensureAlways(InMidiChangeLock))
	{
		// We will need to recalculate extents. Unfortunately, this will also
		// blow away and previously set up loop points, so cache those...
		int32 OriginalLoopStartTick = GetHiResTracker().LoopStartTick;
		int32 OriginalLoopEndTick = GetHiResTracker().LoopEndTick;
		// Now fix up for possible new lengths...
		DetermineLength();
		// Now fix up looping...
		int32 NewLoopStartTick = OriginalLoopStartTick;
		int32 NewLoopEndTick = FMath::Min(GetHiResTracker().LoopEndTick, OriginalLoopEndTick);
		float NewLoopStartMs = GetTempoMap().TickToMs(NewLoopStartTick);
		float NewLoopEndMs = GetTempoMap().TickToMs(NewLoopEndTick);
		for (FMidiPlayCursorTracker& Tracker : Trackers)
		{
			Tracker.LoopStartTick = NewLoopStartTick;
			Tracker.LoopEndTick = NewLoopEndTick;
			Tracker.LoopStartMs = NewLoopStartMs;
			Tracker.LoopEndMs = NewLoopEndMs;
		}

		if (PreRollBars == 0)
		{
			// Now fix up the hires cursors...
			GetHiResTracker().TraversingCursors = true;
			for (auto it = GetHiResTracker().Cursors.begin(); it != GetHiResTracker().Cursors.end(); ++it)
			{
				it.GetNode()->RecalcNextEventsDueToMidiChanges(PositionMode);
			}
			if (PositionMode == EMidiChangePositionCorrectMode::MaintainTick)
			{
				GetHiResTracker().CurrentMs = GetTempoMap().TickToMs(GetHiResTracker().CurrentTick);
			}
			else
			{
				GetHiResTracker().CurrentTick = GetTempoMap().MsToTick(GetHiResTracker().CurrentMs);
			}
			GetHiResTracker().TraversingCursors = false;
			// Now the low res cursors...
			GetLowResTracker().TraversingCursors = true;
			for (auto it = GetLowResTracker().Cursors.begin(); it != GetLowResTracker().Cursors.end(); ++it)
			{
				it.GetNode()->RecalcNextEventsDueToMidiChanges(PositionMode);
			}
			if (PositionMode == EMidiChangePositionCorrectMode::MaintainTick)
			{
				GetLowResTracker().CurrentMs = GetTempoMap().TickToMs(GetLowResTracker().CurrentTick);
			}
			else
			{
				GetLowResTracker().CurrentTick = GetTempoMap().MsToTick(GetLowResTracker().CurrentMs);
			}
			// Now deal with the possibility that the low res cursors might be "out of phase" with the hi res
			// cursors. This would happen if we are in the middle of a loop...
			if (HiResLoopedSinceLastLoResUpdate)
			{
				if (GetHiResTracker().CurrentMs < GetLowResTracker().CurrentMs)
				{
					MsSinceLowResUpdate = (GetHiResTracker().LoopEndMs - GetLowResTracker().CurrentMs) + (GetHiResTracker().CurrentMs - GetHiResTracker().LoopStartMs);
					check(MsSinceLowResUpdate >= 0.0f);
				}
				else
				{
					// yikes. how could this happen?
					checkNoEntry();
					HiResLoopedSinceLastLoResUpdate = false;
				}
			}
			GetLowResTracker().TraversingCursors = false;
		}
		else
		{
			int32 SeekTick = PositionMode == EMidiChangePositionCorrectMode::MaintainTick ? GetHiResTracker().CurrentTick : GetTempoMap().MsToTick(GetHiResTracker().CurrentMs);
			SeekTo(SeekTick, PreRollBars, true, false);
		}
		GetHiResTracker().CursorListCS.Unlock();
		GetLowResTracker().CursorListCS.Unlock();
		InMidiChangeLock = false;
	}
}

FMusicTimestamp FMidiPlayCursorMgr::GetMusicTimestampAtMs(float Ms) const
{
	const int32 TickAtOffset = GetSongMaps().MsToTick(Ms);
	return GetBarMap().TickToMusicTimestamp(TickAtOffset);
}

void FMidiPlayCursorMgr::AdvanceHiResToMs(float Ms, bool Broadcast)
{
	float DeltaMs = Ms - GetCurrentHiResMs();
	if (DeltaMs <= 0.0f)
	{
		return;
	}
	AdvanceHiResByDeltaMs(DeltaMs, Broadcast);
}

void FMidiPlayCursorMgr::AdvanceHiResByDeltaMs(float Ms, bool Broadcast)
{
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);
	HiResLoopedSinceLastLoResUpdate = AdvanceTrackerByDeltaMs(Ms, GetHiResTracker(), false, Broadcast) || HiResLoopedSinceLastLoResUpdate;
	MsSinceLowResUpdate += Ms;
}

void FMidiPlayCursorMgr::AdvanceHiResByDeltaTick(int32 NumTicks, bool Broadcast)
{
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

	int32 ThruTick = NumTicks + GetHiResTracker().CurrentTick;

	float StartMs = GetHiResTracker().CurrentMs;
	AdvanceTrackerThruTick(ThruTick, GetHiResTracker(), false, Broadcast);
	MsSinceLowResUpdate += GetHiResTracker().CurrentMs - StartMs;
}

void FMidiPlayCursorMgr::AdvanceHiResThruTick(int32 ThruTick, bool Broadcast, bool DontAdvancePastLoopEnd)
{
	FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

	// early out?...
	if (ThruTick == GetHiResTracker().CurrentTick)
	{
		return;
	}

	if (GetHiResTracker().Loop && DontAdvancePastLoopEnd && ThruTick >= GetHiResTracker().LoopEndTick - 1)
	{
		ThruTick = GetHiResTracker().LoopEndTick - 1;
	}

	if (ThruTick < GetHiResTracker().CurrentTick)
	{
		UE_LOG(LogMIDI, Warning, TEXT("Asked to go back in time without a seek!"));
		return;
	}

	float StartMs = GetHiResTracker().CurrentMs;
	AdvanceTrackerThruTick(ThruTick, GetHiResTracker(), false, Broadcast);
	MsSinceLowResUpdate += GetHiResTracker().CurrentMs - StartMs;
}

bool FMidiPlayCursorMgr::AdvanceLowResCursors()
{
	FMidiPlayCursorListLock LowResCursorListLock(GetLowResTracker().CursorListCS);

	GetLowResTracker().HandleQueuedReset();

	float MsDiff = 0.0f;
	bool  HiResLoopedSinceLastUpdate = false;
	float HiResCurrentMs = 0.0f;
	int32 HiResLoopStartTick = -1;
	int32 HiResLoopEndTick = -1;
	bool HiResLoop = false;
	bool HiResLoopIgnoringLookAhead = false;
	{
		FMidiPlayCursorListLock HiResCursorListLock(GetHiResTracker().CursorListCS);

		MsDiff = MsSinceLowResUpdate;
		HiResCurrentMs = GetHiResTracker().CurrentMs;
		HiResLoopedSinceLastUpdate = HiResLoopedSinceLastLoResUpdate;
		MsSinceLowResUpdate = 0.0f;
		HiResLoopedSinceLastLoResUpdate = false;

		GetLowResTracker().CurrentAdvanceRate = GetHiResTracker().CurrentAdvanceRate;

		HiResLoopStartTick = GetHiResTracker().LoopStartTick;
		HiResLoopEndTick = GetHiResTracker().LoopEndTick;
		HiResLoop = GetHiResTracker().Loop;
		HiResLoopIgnoringLookAhead = GetHiResTracker().LoopIgnoringLookAhead;
	}

	if (HiResLoop)
	{
		if (!GetLowResTracker().Loop || HiResLoopStartTick != GetLowResTracker().LoopStartTick || HiResLoopEndTick != GetLowResTracker().LoopEndTick)
		{
			SetLoopImpl(HiResLoopStartTick, HiResLoopEndTick, HiResLoopIgnoringLookAhead, true);
		}
	}
	else
	{
		if (GetLowResTracker().Loop)
		{
			ClearLoopImpl(HiResLoopIgnoringLookAhead, true);
		}
	}

	if (!HiResLoopedSinceLastUpdate)
	{
		MsDiff = HiResCurrentMs - GetLowResTracker().CurrentMs;
		if (MsDiff <= 0.0f)
		{
			// This would allow clocks to update independently of buffer sizes,
			// but also allows them to lose touch with reality, if the
			// high-res trackker actually stops updating for some reason.
			// Have to figure out how to distinguish those cases.
			// FORT-706568
			//UpdateLowResCursors(GetLowResTracker());
			return false;
		}
	}
	return AdvanceTrackerByDeltaMs(MsDiff, GetLowResTracker(), true, true);
}

void FMidiPlayCursorMgr::UpdateLowResCursors(FMidiPlayCursorTracker& Tracker)
{
	GetLowResTracker().TraversingCursors = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->UpdateWithTrackerUnchanged())
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	GetLowResTracker().TraversingCursors = false;
}

bool FMidiPlayCursorMgr::AdvanceTrackerByDeltaMs(float Ms, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast)
{
	Tracker.ElapsedMs += Ms;
	const FTempoMap& TempoMap = GetTempoMap();
	float NewMs = Tracker.CurrentMs + Ms;
	int32 NewTick = (int32)(TempoMap.MsToTick(NewMs) + 0.5f);
	bool  Looped = false;

	if (NewTick >= Tracker.LoopEndTick && Tracker.CurrentTick < Tracker.LoopEndTick && Tracker.Loop)
	{
		// loop the tick around...
		NewTick = (NewTick - Tracker.LoopEndTick) + Tracker.LoopStartTick;
		NewMs = TempoMap.TickToMs(NewTick);
		Tracker.CurrentMs = NewMs;
		Tracker.CurrentTick = NewTick;
		Looped = true;
	}
	else
	{
		Tracker.CurrentTick = NewTick;
		Tracker.CurrentMs = NewMs;
	}

	Tracker.TraversingCursors = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->Advance(IsLowRes))
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	Tracker.TraversingCursors = false;

	return Looped;
}

void FMidiPlayCursorMgr::AdvanceTrackerThruTick(int32 ToTick, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast)
{
	const FTempoMap& TempoMap = GetTempoMap();

	float NewMs = TempoMap.TickToMs(ToTick);
	Tracker.ElapsedMs += NewMs - Tracker.CurrentMs;
	int32   NewTick = ToTick;

	Tracker.CurrentTick = NewTick;
	Tracker.CurrentMs = NewMs;

	Tracker.TraversingCursors = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->Advance(IsLowRes))
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	Tracker.TraversingCursors = false;
}
