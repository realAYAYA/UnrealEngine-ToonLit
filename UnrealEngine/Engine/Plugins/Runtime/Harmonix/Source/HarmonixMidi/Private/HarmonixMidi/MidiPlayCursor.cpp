// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiPlayCursor.h"
#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include <functional>

const float FMidiPlayCursor::kSmallMs = 0.1f;

FMidiPlayCursor::FMidiPlayCursor()
	: CurrentMs(0.0f)
	, CurrentTick(0)
	, LoopCount(0)
	, UnregisterASAP(false)
	, Owner(nullptr)
	, Tracker(nullptr)
	, LookaheadType(ELookaheadType::Ticks)
	, SyncOpts(ESyncOptions::NoBroadcastNoPreRoll)
	, LookaheadTicks(0)
	, LookaheadMs(0.0f)
	, WatchTrack(-1)
	, FilterPassFlags(EFilterPassFlags::All)
{

}

FMidiPlayCursor::~FMidiPlayCursor()
{
	if (GetOwner())
	{
		Owner->UnregisterPlayCursor(this);
		Owner = nullptr;
		Tracker = nullptr;
	}
}

void FMidiPlayCursor::SetOwner(FMidiPlayCursorMgr* NewOwner, FMidiPlayCursorTracker* NewTracker, float PreRollMs)
{
	check(GetOwner() == nullptr || NewOwner == nullptr);

	Owner = NewOwner;
	Tracker = NewTracker;

	if (GetOwner())
	{
		TrackNextEventIndexs.SetNumUninitialized(Owner->Tracks().Num());
		Reset(PreRollMs > 0.0f);

		if (Tracker && PreRollMs > 0.0f)
		{
			if (PreRollMs > Tracker->CurrentMs)
			{
				PreRollMs = Tracker->CurrentMs;
			}
			int32 Tick = (int32)Owner->GetTempoMap().MsToTick(Tracker->CurrentMs - PreRollMs);
			float Ms = Owner->GetTempoMap().TickToMs(Tick);
			Tracker->ResetNewCursor(this, Tick, Ms, false);
		}
	}
}

void FMidiPlayCursor::Reset(bool ForceNoBroadcast)
{
	CurrentTick = -1;
	LoopCount = 0;
	if (GetOwner())
	{
		CurrentMs = Owner->GetTempoMap().TickToMs(CurrentTick);
		TrackNextEventIndexs.SetNumUninitialized(Owner->Tracks().Num());

		for (int32 i = 0; i < TrackNextEventIndexs.Num(); i++)
		{
			TrackNextEventIndexs[i] = 0;
		}

		if (LookaheadType == ELookaheadType::Ticks)
		{
			PrepareLookAheadTicks(ForceNoBroadcast);
		}
		else
		{
			PrepareLookAheadMs(ForceNoBroadcast);
		}
	}
	else
	{
		TrackNextEventIndexs.Empty();
		CurrentMs = 0.0f;
	}
}

void FMidiPlayCursor::SetupTickLookahead(int32 Ticks, ESyncOptions Opts)
{
	LookaheadType = ELookaheadType::Ticks;
	LookaheadTicks = Ticks;
	SyncOpts = Opts;

	// may be called before or after we are attached to a play cursor manager!
	if (GetOwner())
	{
		PrepareLookAheadTicks();
	}
}

#if HARMONIX_MIDIPLAYCURSOR_ENABLE_ENSURE_OWNER
FMidiPlayCursorMgr* FMidiPlayCursor::GetOwner()
{
	if (Tracker)
	{
		int32 RecursionCount = Tracker->CursorListCS.GetRecursionCountIfOwned();
		if (!RecursionCount)
		{
			UE_LOG(LogMIDI, Warning, TEXT("Play cursor owner accessed with cursor list unlocked."));
		}
	}
	return Owner;
}
#endif

void FMidiPlayCursor::RecalcNextEventsDueToMidiChanges(FMidiPlayCursorMgr::EMidiChangePositionCorrectMode PositionMode)
{
	// MUST ONLY BE CALLED BY THE PLAYCURSORMGR WITH CURSORS LOCKED!
	check(Owner);

	if (PositionMode == FMidiPlayCursorMgr::EMidiChangePositionCorrectMode::MaintainTick)
	{
		CurrentMs = Owner->GetTempoMap().TickToMs(CurrentTick);
	}
	else // if (positionMode == MidiChangePositionCorrectMode::MaintainTime)
	{
		CurrentTick = Owner->GetTempoMap().MsToTick(CurrentMs);
	}

	// changes to tracks might actually be more complicated...
	const UMidiFile::FMidiTrackList& Tracks = GetOwner()->Tracks();
	if (Tracks.Num() != TrackNextEventIndexs.Num())
	{
		int32 OldNum = TrackNextEventIndexs.Num();
		TrackNextEventIndexs.SetNumUninitialized(Tracks.Num());
	}

	for (int32 i = 0; i < Tracks.Num(); ++i)
	{
		const FMidiTrack& Track = Tracks[i];
		const FMidiEventList& Events = Track.GetEvents();
		if (Events.Num() == 0)
		{
			TrackNextEventIndexs[i] = -1;
			continue;
		}
		else
		{
			// search from the beginning...
			TrackNextEventIndexs[i] = 0;
			while (TrackNextEventIndexs[i] != -1 && CurrentTick >= Events[TrackNextEventIndexs[i]].GetTick())
			{
				TrackNextEventIndexs[i]++;
				if (TrackNextEventIndexs[i] == Events.Num())
				{
					TrackNextEventIndexs[i] = -1;
				}
			}
		}
	}
}

void FMidiPlayCursor::PrepareLookAheadTicks(bool ForceNoBroadcast)
{
	check(GetOwner());

	bool Broadcast = !ForceNoBroadcast &&
		(SyncOpts != ESyncOptions::NoBroadcastNoPreRoll) &&
		(SyncOpts == ESyncOptions::BroadcastImmediately || (SyncOpts == ESyncOptions::PreRollIfAtStartOrBroadcast && !Tracker->IsAtStart()));

	if ((FilterPassFlags & EFilterPassFlags::Reset) != EFilterPassFlags::None)
	{
		OnReset();
	}

	if (LookaheadTicks > 0 &&
		Tracker->IsAtStart() &&
		(SyncOpts == ESyncOptions::PreRollIfAtStartOrBroadcast || SyncOpts == ESyncOptions::PreRollIfAtStartOrNoBroadcast))
	{
		GetOwner()->RecalculatePreRollDueToCursorPosition(this);
		return;
	}

	if (Broadcast)
	{
		AdvanceByTicks(true);
	}
	else
	{
		SeekThruTick(Tracker->CurrentTick + LookaheadTicks);
	}
}

void FMidiPlayCursor::SetupMsLookahead(float Ms, ESyncOptions Opts)
{
	LookaheadType = ELookaheadType::Time;
	LookaheadMs = Ms;
	SyncOpts = Opts;

	// may be called before or after we are attached to a play cursor manager!
	if (GetOwner())
	{
		PrepareLookAheadMs();
	}
}

void FMidiPlayCursor::PrepareLookAheadMs(bool ForceNoBroadcast)
{
	check(GetOwner());

	bool Broadcast = !ForceNoBroadcast &&
		(SyncOpts != ESyncOptions::NoBroadcastNoPreRoll) &&
		(SyncOpts == ESyncOptions::BroadcastImmediately || (SyncOpts == ESyncOptions::PreRollIfAtStartOrBroadcast && !Tracker->IsAtStart()));

	if ((FilterPassFlags & EFilterPassFlags::Reset) != EFilterPassFlags::None)
	{
		OnReset();
	}

	if (LookaheadMs > 0.0f &&
		Tracker->IsAtStart() &&
		(SyncOpts == ESyncOptions::PreRollIfAtStartOrBroadcast || SyncOpts == ESyncOptions::PreRollIfAtStartOrNoBroadcast))
	{
		GetOwner()->RecalculatePreRollDueToCursorPosition(this);
		return;
	}

	if (Broadcast)
	{
		AdvanceByMs(true);
	}
	else
	{
		float NewMs = Tracker->CurrentMs + LookaheadMs;
		if (NewMs > 0.0f)
		{
			CurrentTick = (int32)(GetOwner()->GetTempoMap().MsToTick(NewMs) + 0.5f);
		}
		else
		{
			CurrentTick = (int32)(GetOwner()->GetTempoMap().MsToTick(NewMs) - 0.5f);
		}
		SeekThruTick(CurrentTick);
		CurrentMs = NewMs;
	}
}

bool FMidiPlayCursor::Advance(bool IsLowRes)
{
	check(GetOwner());
	bool ProcessLoops = IsLowRes ||
		!(GetOwner()->IsDirectMappedTimeFollower() && LookaheadTicks == 0);
	switch (LookaheadType)
	{
	case ELookaheadType::Ticks: AdvanceByTicks(ProcessLoops); break;
	case ELookaheadType::Time:  AdvanceByMs(ProcessLoops); break;
	default: checkNoEntry(); break;
	}
	return !UnregisterASAP;
}

bool FMidiPlayCursor::AdvanceAsPreRoll()
{
	check(GetOwner());
	switch (LookaheadType)
	{
	case ELookaheadType::Ticks: AdvanceByTicks(true, true, true); break;
	case ELookaheadType::Time:  AdvanceByMs(true, true, true); break;
	default: checkNoEntry(); break;
	}
	return !UnregisterASAP;
}


void FMidiPlayCursor::AdvanceByTicks(bool ProcessLoops, bool Broadcast, bool IsPreRoll)
{
	if (LookaheadTicks < 0)
	{
		DoAdvanceForLaggingTickCursor(Broadcast, IsPreRoll);
	}
	else
	{
		DoAdvanceForLeadingTickCursor(Broadcast, ProcessLoops, IsPreRoll);
	}
}

void FMidiPlayCursor::DoAdvanceForLaggingTickCursor(bool Broadcast, bool IsPreRoll)
{
	int32 NewTick = Tracker->CurrentTick + LookaheadTicks;
	if (GetOwner()->DoesLoop(Tracker->IsLowRes))
	{
		int32 LoopStartTick = Owner->GetLoopStartTick(Tracker->IsLowRes);
		int32 LoopEndTick = Owner->GetLoopEndTick(Tracker->IsLowRes);
		int32 TrackerTick = Tracker->CurrentTick;

		// loop or jump?
		if (LoopStartTick < LoopEndTick)
		{
			// possible loop back...
			if (CurrentTick < TrackerTick)
			{
				// cases 1,2 & 3...
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else if (NewTick < LoopStartTick)
			{
				// wrap new tick back to loop end...
				NewTick = LoopEndTick - (LoopStartTick - NewTick);
				// now advance from current position to new position...
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (Broadcast)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
		}
		else
		{
			// possible jump forward...
			if ((CurrentTick < LoopEndTick && TrackerTick < LoopEndTick) ||
				(CurrentTick > LoopStartTick && TrackerTick > LoopStartTick))
			{
				// easy
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (NewTick < LoopStartTick)
				{
					// wrap new tick back from loopEnd...
					NewTick = LoopEndTick - (LoopStartTick - NewTick);
					if (Broadcast)
					{
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
				else
				{
					if (Broadcast)
					{
						// advance from current position to loop end...
						AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
						// notify...
						if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
						{
							OnLoop(LoopStartTick, LoopEndTick);
						}
						// advance from loop start to new position...
						SeekToTick(LoopStartTick);
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
			}
		}
	}
	else
	{
		// simple... no loop...
		if (Broadcast)
		{
			AdvanceThruTick(NewTick, IsPreRoll);
		}
		else
		{
			SeekThruTick(NewTick);
		}
	}
}

void FMidiPlayCursor::DoAdvanceForLeadingTickCursor(bool Broadcast, bool ProcessLoops, bool IsPreRoll)
{
	int32 NewTick = Tracker->CurrentTick + LookaheadTicks;
	if (GetOwner()->DoesLoop(Tracker->IsLowRes) && ProcessLoops) 
	{
		int32 LoopStartTick = Owner->GetLoopStartTick(Tracker->IsLowRes);
		int32 LoopEndTick = Owner->GetLoopEndTick(Tracker->IsLowRes);
		int32 TrackerTick = Tracker->CurrentTick;

		// loop or jump?
		if (LoopStartTick < LoopEndTick)
		{
			// possible loop back...
			if (NewTick >= LoopEndTick)
			{
				// wrap NewTick...
				NewTick = LoopStartTick + (NewTick - LoopEndTick);
			}
			if (NewTick < CurrentTick)
			{
				if (Broadcast)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
		}
		else
		{
			// possible jump forward...
			if ((NewTick < LoopEndTick && CurrentTick < LoopEndTick) ||
				(NewTick >= LoopStartTick && CurrentTick >= LoopStartTick))
			{
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (NewTick > (LoopEndTick - 1) && NewTick < LoopStartTick)
				{
					// jump NewTick...
					NewTick = LoopStartTick + (NewTick - LoopEndTick);
				}

				if (CurrentTick < LoopEndTick)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					if (Broadcast)
					{
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
			}
		}
	}
	else
	{
		// simple... no loop...
		if (Broadcast)
		{
			AdvanceThruTick(NewTick, IsPreRoll);
		}
		else
		{
			SeekThruTick(NewTick);
		}
	}
}

void FMidiPlayCursor::AdvanceByMs(bool ProcessLoops, bool Broadcast, bool IsPreRoll)
{
	if (LookaheadMs < 0.0f)
	{
		DoAdvanceForLaggingMsCursor(Broadcast, IsPreRoll);
	}
	else
	{
		DoAdvanceForLeadingMsCursor(Broadcast, ProcessLoops, IsPreRoll);
	}
}

void FMidiPlayCursor::DoAdvanceForLaggingMsCursor(bool Broadcast, bool IsPreRoll)
{
	float NewMs = Tracker->CurrentMs + LookaheadMs;
	int32 NewTick = (int32)(GetOwner()->GetTempoMap().MsToTick(NewMs) + 0.5f);
	if (Owner->DoesLoop(Tracker->IsLowRes))
	{
		float LoopStartMs = Owner->GetLoopStartMs(Tracker->IsLowRes);
		float LoopEndMs = Owner->GetLoopEndMs(Tracker->IsLowRes);
		int32 LoopStartTick = Owner->GetLoopStartTick(Tracker->IsLowRes);
		int32 LoopEndTick = Owner->GetLoopEndTick(Tracker->IsLowRes);
		int32 TrackerTick = Tracker->CurrentTick;

		// loop or jump?
		if (LoopStartTick < LoopEndTick)
		{
			// possible loop back...
			if (CurrentTick < TrackerTick)
			{
				// cases 1,2 & 3...
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else if (NewMs < LoopStartMs)
			{
				// wrap new tick back to loop end...
				NewMs = LoopEndMs - (LoopStartMs - NewMs);
				NewTick = (int32)(Owner->GetTempoMap().MsToTick(NewMs) + 0.5f);
				// now advance from current position to new position...
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (Broadcast)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
		}
		else
		{
			// possible jump forward...
			if ((CurrentTick < LoopEndTick && TrackerTick < LoopEndTick) ||
				(CurrentTick > LoopStartTick && TrackerTick > LoopStartTick))
			{
				// easy
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (NewMs < LoopStartMs)
				{
					// wrap new tick back from loopEnd...
					NewMs = LoopEndMs - (LoopStartMs - NewMs);
					NewTick = (int32)(Owner->GetTempoMap().MsToTick(NewMs) + 0.5f);
					if (Broadcast)
					{
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
				else
				{
					if (Broadcast)
					{
						// advance from current position to loop end...
						AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
						// notify...
						if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
						{
							OnLoop(LoopStartTick, LoopEndTick);
						}
						// advance from loop start to new position...
						SeekToTick(LoopStartTick);
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
			}
		}
	}
	else
	{
		// simple... no loop...
		if (Broadcast)
		{
			AdvanceThruTick(NewTick, IsPreRoll);
		}
		else
		{
			SeekThruTick(NewTick);
		}
	}
	CurrentMs = NewMs;
}

void FMidiPlayCursor::DoAdvanceForLeadingMsCursor(bool Broadcast, bool ProcessLoops, bool IsPreRoll)
{
	float NewMs = Tracker->CurrentMs + LookaheadMs;
	int32 NewTick = (int32)(GetOwner()->GetTempoMap().MsToTick(NewMs) + 0.5f);
	if (Owner->DoesLoop(Tracker->IsLowRes) && ProcessLoops) 
	{
		float LoopStartMs = Owner->GetLoopStartMs(Tracker->IsLowRes);
		float LoopEndMs = Owner->GetLoopEndMs(Tracker->IsLowRes);
		int32 LoopStartTick = Owner->GetLoopStartTick(Tracker->IsLowRes);
		int32 LoopEndTick = Owner->GetLoopEndTick(Tracker->IsLowRes);
		int32 TrackerTick = Tracker->CurrentTick;

		// loop or jump?
		if (LoopStartTick < LoopEndTick)
		{
			// possible loop back...
			if (NewMs >= LoopEndMs)
			{
				// wrap NewTick...
				NewMs = LoopStartMs + (NewMs - LoopEndMs);
				NewTick = (int32)(Owner->GetTempoMap().MsToTick(NewMs) + 0.5f);
			}
			if (NewTick < CurrentTick)
			{
				if (Broadcast)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
		}
		else
		{
			// possible jump forward...
			if ((NewTick < LoopEndTick && CurrentTick < LoopEndTick) ||
				(NewTick >= LoopStartTick && CurrentTick >= LoopStartTick))
			{
				if (Broadcast)
				{
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					SeekThruTick(NewTick);
				}
			}
			else
			{
				if (NewTick > (LoopEndTick - 1) && NewTick < LoopStartTick)
				{
					// jump NewTick...
					NewMs = LoopStartMs + (NewMs - LoopEndMs);
					NewTick = (int32)(Owner->GetTempoMap().MsToTick(NewMs) + 0.5f);
				}

				if (CurrentTick < LoopEndTick)
				{
					// advance from current position to loop end...
					AdvanceThruTick(LoopEndTick - 1, IsPreRoll);
					// notify...
					if ((FilterPassFlags & EFilterPassFlags::Loop) != EFilterPassFlags::None)
					{
						OnLoop(LoopStartTick, LoopEndTick);
					}
					// advance from loop start to new position...
					SeekToTick(LoopStartTick);
					AdvanceThruTick(NewTick, IsPreRoll);
				}
				else
				{
					if (Broadcast)
					{
						AdvanceThruTick(NewTick, IsPreRoll);
					}
					else
					{
						SeekThruTick(NewTick);
					}
				}
			}
		}
	}
	else
	{
		// simple... no loop...
		if (Broadcast)
		{
			AdvanceThruTick(NewTick, IsPreRoll);
		}
		else
		{
			SeekThruTick(NewTick);
		}
	}
	CurrentMs = NewMs;
}

void FMidiPlayCursor::SeekToTick(int32 Tick)
{
	const UMidiFile::FMidiTrackList& Tracks = GetOwner()->Tracks();
	int32 StartIndex = (WatchTrack < 0) ? 0 : WatchTrack;
	int32 EndIndex = (WatchTrack < 0) ? Tracks.Num() : (WatchTrack + 1);
	if (EndIndex > Tracks.Num())
	{
		EndIndex = Tracks.Num();
	}
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		const FMidiTrack& Track = Tracks[i];
		const FMidiEventList& Events = Track.GetEvents();
		TrackNextEventIndexs[i] = 0;
		while (Events.Num() > TrackNextEventIndexs[i] && Events[TrackNextEventIndexs[i]].GetTick() < Tick)
		{
			TrackNextEventIndexs[i]++;
		}
		if (Events.Num() == TrackNextEventIndexs[i])
		{
			TrackNextEventIndexs[i] = -1;
		}
	}
	CurrentTick = Tick - 1;
	CurrentMs = Owner->GetTempoMap().TickToMs(CurrentTick);
}

void FMidiPlayCursor::SeekThruTick(int32 Tick)
{
	const UMidiFile::FMidiTrackList& Tracks = GetOwner()->Tracks();
	int32 StartIndex = (WatchTrack < 0) ? 0 : WatchTrack;
	int32 EndIndex = (WatchTrack < 0) ? Tracks.Num() : (WatchTrack + 1);
	if (EndIndex > Tracks.Num())
	{
		EndIndex = Tracks.Num();
	}
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		const FMidiTrack& Track = Tracks[i];
		const FMidiEventList& Events = Track.GetEvents();
		TrackNextEventIndexs[i] = 0;
		while (Events.Num() > TrackNextEventIndexs[i] && Events[TrackNextEventIndexs[i]].GetTick() <= Tick)
		{
			TrackNextEventIndexs[i]++;
		}
		if (Events.Num() == TrackNextEventIndexs[i])
		{
			TrackNextEventIndexs[i] = -1;
		}
	}
	CurrentTick = Tick;
	CurrentMs = Owner->GetTempoMap().TickToMs(Tick);
}

bool FMidiPlayCursor::IsDone() const
{
	const UMidiFile::FMidiTrackList& Tracks = GetOwner()->Tracks();
	int32 StartIndex = (WatchTrack < 0) ? 0 : WatchTrack;
	int32 EndIndex = (WatchTrack < 0) ? Tracks.Num() : (WatchTrack + 1);
	if (EndIndex > Tracks.Num())
	{
		EndIndex = Tracks.Num();
	}
	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		if (TrackNextEventIndexs[i] != -1)
		{
			return false;
		}
	}
	return true;
}

void FMidiPlayCursor::MoveToLoopStartIfAtNotOffset(int32 NewThruTick, float NewThruMs)
{
	if ((LookaheadType == ELookaheadType::Ticks && LookaheadTicks == 0) ||
		(LookaheadType == ELookaheadType::Time && LookaheadMs == 0.0f))
	{
		//mCurrentTick = newThruTick;
		//mCurrentMs   = newThruMs;

		// This will set both mCurrentTick and mCurrentMs
		SeekThruTick(NewThruTick);
	}
}

namespace
{
	class FMidiEventHashTable
	{
	private:
		struct Node
		{
			int32 TrackIndex = 0;
			const FMidiEvent* E = nullptr;
			Node* Next = nullptr;
			uint32 Key;
		};

	public:
		static FMidiEventHashTable& Get();

		FMidiEventHashTable(int32 Size)
			: NumNodes(Size)
		{
			Head = (Node*)FMemory::Malloc(sizeof(Node) * Size, 16);
			Clear();
		}

		bool Push(int32 TrackIndex, const FMidiEvent* E)
		{
			ensure(FreeNodes);
			if (!FreeNodes)
			{
				return false;
			}

			IsClear = false;

			int32 BucketIndex;
			uint32 Key;
			MakeHashAndKey(TrackIndex, E, BucketIndex, Key);
			// make sure this isn't in the table already!
			Pop(BucketIndex, Key);

			Node* N = FreeNodes;
			FreeNodes = N->Next;
			Count++;

			N->TrackIndex = TrackIndex;
			N->E = E;
			N->Key = Key;
			N->Next = Buckets[BucketIndex];
			Buckets[BucketIndex] = N;

			return true;
		}

		bool Pop(int32 TrackIndex, const FMidiEvent* E)
		{
			int32 BucketIndex;
			uint32 Key;
			MakeHashAndKey(TrackIndex, E, BucketIndex, Key);
			return Pop(BucketIndex, Key);
		}

		bool Pop(int32 BucketIndex, uint32 Key)
		{
			Node* List = Buckets[BucketIndex];
			if (!List)
			{
				return false;
			}

			Node* Freeable = nullptr;
			if (List->Key == Key)
			{
				Freeable = List;
				Buckets[BucketIndex] = List->Next;
			}
			else
			{
				while (List->Next && List->Next->Key != Key)
				{
					List = List->Next;
				}
				if (List->Next)
				{
					Freeable = List->Next;
					List->Next = List->Next->Next;
				}
			}
			if (Freeable)
			{
				Freeable->Next = FreeNodes;
				FreeNodes = Freeable;
				Count--;
				return true;
			}
			return false;
		}

		Node* FindKeyMatch(int32 BucketIndex, uint32 Key)
		{
			Node* N = Buckets[BucketIndex];
			while (N && N->Key != Key)
			{
				N = N->Next;
			}
			return N;
		}

		~FMidiEventHashTable()
		{
			FMemory::Free(Head);
		}

		using WorkFunction = std::function<void(int32, const FMidiEvent*)>;
		void DoOnAll(WorkFunction WorkFunc)
		{
			for (int32 i = 0; i < kNumBuckets; ++i)
			{
				Node* List = Buckets[i];
				while (List)
				{
					WorkFunc(List->TrackIndex, List->E);
					List = List->Next;
				}
			}
		}

		void Clear()
		{
			if (IsClear)
			{
				return;
			}
			FreeNodes = Head;
			for (int32 i = 0; i < NumNodes - 1; ++i)
			{
				FreeNodes[i].Next = &FreeNodes[i + 1];
			}
			FreeNodes[NumNodes - 1].Next = nullptr;
			for (int32 i = 0; i < kNumBuckets; ++i)
			{
				Buckets[i] = nullptr;
			}
			Count = 0;
			IsClear = true;
		}

	private:
		int32 NumNodes = 0;
		int32 Count = 0;
		bool  IsClear = false;
		Node* Head;
		Node* FreeNodes;
		static constexpr int32 kNumBuckets = 24;
		Node* Buckets[kNumBuckets];

		void MakeHashAndKey(int32 TrackIndex, const FMidiEvent* E, int32& BucketIndex, uint32& Key)
		{
			int32 Note = (int32)E->GetMsg().GetStdData1();
			int32 MidiCh = (int32)E->GetMsg().GetStdChannel();
			Key = (uint32)((TrackIndex << 16) | (MidiCh << 8) | Note);

			BucketIndex = ((Note % 13) + MidiCh + (13 * TrackIndex)) % kNumBuckets;
			checkf(BucketIndex >= 0 && BucketIndex < kNumBuckets, TEXT("What!? buckIndex = %d, numBuckets = %d, note = %d, midi ch = %d, trackIndex = %d"), BucketIndex, kNumBuckets, Note, MidiCh, TrackIndex);
			// Maybe we need this ?!? bucketIndex = Abs((note % 13) + midiCh + (13 * trackIndex)) % kNumBuckets;
			return;
		}
	};

	// This next little wrapper lets us lazily new up a FMidiEventHashTable
	// only on threads that actually need it. Otherwise the thread is only 
	// wasting sizeof(void*) bytes for the unused hash table. 
	class FThreadLocalHashTableHolder
	{
	public:
		FThreadLocalHashTableHolder() : ThreadsTable(nullptr) {}
		FMidiEventHashTable& Get() 
		{
			if (!ThreadsTable)
			{
				ThreadsTable = new FMidiEventHashTable(64);
			}
			ThreadsTable->Clear();
			return *ThreadsTable;
		}
		~FThreadLocalHashTableHolder() { delete ThreadsTable; }
	private:
		FMidiEventHashTable* ThreadsTable = nullptr;
	};

	static thread_local FThreadLocalHashTableHolder ThisThreadsMidiHashTable;
}

void FMidiPlayCursor::AdvanceThruTick(int32 Tick, bool IsPreRoll)
{
	const UMidiFile::FMidiTrackList& Tracks = GetOwner()->Tracks();
	FMidiEventHashTable& HeldNoteOnEvents = ThisThreadsMidiHashTable.Get();

	if (!IsPreRoll)
	{
		int32 StartIndex = (WatchTrack < 0) ? 0 : WatchTrack;
		int32 EndIndex = (WatchTrack < 0) ? Tracks.Num() : (WatchTrack + 1);
		if (EndIndex > Tracks.Num())
		{
			EndIndex = Tracks.Num();
		}
		for (int32 i = StartIndex; i < EndIndex && !UnregisterASAP; ++i)
		{
			const FMidiTrack& Track = Tracks[i];
			const FMidiEventList& Events = Track.GetEvents();
			while (TrackNextEventIndexs[i] != -1 && Tick >= Events[TrackNextEventIndexs[i]].GetTick() && !UnregisterASAP)
			{
				BroadcastEvent(i, Events[TrackNextEventIndexs[i]]);
				TrackNextEventIndexs[i]++;
				if (TrackNextEventIndexs[i] == Events.Num())
				{
					TrackNextEventIndexs[i] = -1;
				}
			}
		}
	}
	else
	{
		// This is a much more complicated case...
		// We only want to broadcast note-on messages that don't have a corresponding note-off message
		// between the preroll start tick and the current tick.
		int32 StartIndex = (WatchTrack < 0) ? 0 : WatchTrack;
		int32 EndIndex = (WatchTrack < 0) ? Tracks.Num() : (WatchTrack + 1);
		if (EndIndex > Tracks.Num())
		{
			EndIndex = Tracks.Num();
		}
		for (int32 i = StartIndex; i < EndIndex && !UnregisterASAP; ++i)
		{
			const FMidiTrack& Track = Tracks[i];
			const FMidiEventList& Events = Track.GetEvents();
			while (TrackNextEventIndexs[i] != -1 && Tick >= Events[TrackNextEventIndexs[i]].GetTick() && !UnregisterASAP)
			{
				const FMidiMsg& Msg = Events[TrackNextEventIndexs[i]].GetMsg();
				bool Swallow = false;
				if (Msg.MsgType() == FMidiMsg::EType::Std)
				{
					if (Msg.IsNoteOn())
					{
						if (Msg.GetStdData2() != 0)
						{
							HeldNoteOnEvents.Push(i, &Events[TrackNextEventIndexs[i]]);
						}
						else
						{
							HeldNoteOnEvents.Pop(i, &Events[TrackNextEventIndexs[i]]);
						}
						Swallow = true;
					}
					else if (Msg.IsNoteOff())
					{
						HeldNoteOnEvents.Pop(i, &Events[TrackNextEventIndexs[i]]);
						Swallow = true;
					}
				}
				if (!Swallow)
				{
					BroadcastEvent(i, Events[TrackNextEventIndexs[i]], true);
				}
				TrackNextEventIndexs[i]++;
				if (TrackNextEventIndexs[i] == Events.Num())
				{
					TrackNextEventIndexs[i] = -1;
				}
			}
		}
	}

	// A result of one of the broadcasts above may have resulted in this cursor being
	// unregistered. So... check for that possibility...
	if (UnregisterASAP)
	{
		return;
	}

	CurrentTick = Tick;
	const FTempoMap& TempoMap = Owner->GetTempoMap();
	CurrentMs = TempoMap.TickToMs(Tick);

	// now we MAY have note on events which still need to be broadcast...
	if ((FilterPassFlags & EFilterPassFlags::PreRollNoteOn) != EFilterPassFlags::None)
	{
		HeldNoteOnEvents.DoOnAll(
			[&](int32 TrackIndex, const FMidiEvent* Event)
			{
				const FMidiMsg& Msg = Event->GetMsg();
				int32 EventTick = Event->GetTick();
				float EventMs = TempoMap.TickToMs(EventTick);
				OnPreRollNoteOn(TrackIndex, EventTick, CurrentTick, CurrentMs - EventMs, Msg.GetStdStatus(), Msg.GetStdData1(), Msg.GetStdData2());
			});
	}
}

void FMidiPlayCursor::BroadcastEvent(int32 TrackIndex, const FMidiEvent& Event, bool IsPreroll)
{
	const FMidiMsg& Msg = Event.GetMsg();
	//Call callbacks for each event that the cursor is interested in
	switch (Msg.MsgType())
	{
	case FMidiMsg::EType::Std:
		if ((FilterPassFlags & EFilterPassFlags::MidiMessage) != EFilterPassFlags::None)
		{
			OnMidiMessage(TrackIndex, Event.GetTick(), Msg.GetStdStatus(), Msg.GetStdData1(), Msg.GetStdData2(), IsPreroll);
		}
		break;
	case FMidiMsg::EType::Tempo:
		if ((FilterPassFlags & EFilterPassFlags::Tempo) != EFilterPassFlags::None)
		{
			OnTempo(TrackIndex, Event.GetTick(), Msg.GetMicrosecPerQuarterNote(), IsPreroll);
		}
		break;
	case FMidiMsg::EType::Text:
		if ((FilterPassFlags & EFilterPassFlags::Text) != EFilterPassFlags::None)
		{
			const FMidiTrack& Track = GetOwner()->Tracks()[TrackIndex];
			OnText(TrackIndex, Event.GetTick(), Msg.GetTextIndex(), Track.GetTextAtIndex(Msg.GetTextIndex()), Msg.GetTextType(), IsPreroll);
		}
		break;
	case FMidiMsg::EType::TimeSig:
		if ((FilterPassFlags & EFilterPassFlags::TimeSig) != EFilterPassFlags::None)
		{
			OnTimeSig(TrackIndex, Event.GetTick(), Msg.GetTimeSigNumerator(), Msg.GetTimeSigDenominator(), IsPreroll);
		}
		break;
	default:
		UE_LOG(LogMIDI, Error, TEXT("Unknown MIDI message type %d on track %d at tick %d, file %s"), int(Msg.MsgType()), TrackIndex, Event.GetTick(), **GetOwner()->GetMidiFileName());
		break;
	}
}


/*****************************************************************************

Cursor situations...

C = mCurrentTick
N = newTick
T = tracker->mCurrentTick

LAGGING
=======

   Loop:         LS         LE
				  |          |
		1)  C  NT |          |
		2)    C  N|T         |
		3)      C | NT       |
		4)        |C  NT     |
		5)        |T     C  N|
		6)        | NT     C |

   Jump:         LE         LS
				  |          |
		1)  C  NT |          |
		2)    C  N|          |T
		3)      C |          | NT
		4)        |          |C  NT

LEADING
=======
   Loop:         LS         LE
				  |          |
		1)   T  CN|          |
		2)    T  C|N         |
		3)      T | CN       |
		4)        |T  CN     |
		5)        |N     T  C|
		6)        | CN     T |

   Jump:         LE         LS
				  |          |
		1)  T  CN |          |
		2)    T  C|          |N
		3)      T |          | CN
		4)        |          |T  CN

*****************************************************************************/
