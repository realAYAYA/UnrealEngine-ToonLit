// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiPlayCursorTracker.h"
#include "HarmonixMidi/MidiPlayCursor.h"

FMidiPlayCursorTracker::FMidiPlayCursorTracker(bool InIsLowRes)
	: CurrentTick(-1)
	, CurrentMs(-FMidiPlayCursor::kSmallMs)
	, ElapsedMs(0.0f)
	, LoopCount(0)
	, EarliestCursorTick(0)
	, LatestCursorTick(0)
	, EarliestCursorMs(0.0f)
	, LatestCursorMs(0.0f)
	, CurrentAdvanceRate(1.f)
	, LoopOffsetTick(0.f)
	, LoopStartMs(0.f)
	, LoopStartTick(0)
	, LoopEndMs(0.f)
	, LoopEndTick(0)
	, Loop(false)
	, LoopIgnoringLookAhead(false)
	, TraversingCursors(false)
	, IsLowRes(InIsLowRes)
{
}

bool FMidiPlayCursorTracker::IsAtStart()
{
	return CurrentTick <= 0 && CurrentMs <= 0.0 && LoopCount == 0;
}

void FMidiPlayCursorTracker::AddCursor(FMidiPlayCursor* Cursor)
{
	Cursors.AddTail(Cursor);
	RecalculateExtents();
}

bool FMidiPlayCursorTracker::ContainsCursor(FMidiPlayCursor* Cursor)
{
	for (auto it = Cursors.begin(); it != Cursors.end(); ++it)
	{
		if (it.GetNode() == Cursor)
		{
			return true;
		}
	}
	return false;
}

bool FMidiPlayCursorTracker::RemoveCursor(FMidiPlayCursor* Cursor)
{
	if (ContainsCursor(Cursor))
	{
		Cursors.Remove(Cursor);
		if ((Cursor->GetLookaheadType() == FMidiPlayCursor::ELookaheadType::Ticks &&
			(Cursor->GetLookaheadTicks() < LatestCursorTick || Cursor->GetLookaheadTicks() > EarliestCursorTick)) ||
			(Cursor->GetLookaheadMs() < LatestCursorMs || Cursor->GetLookaheadMs() > EarliestCursorMs))
		{
			RecalculateExtents();
		}
		return true;
	}
	return false;
}

void FMidiPlayCursorTracker::Clear()
{
	EarliestCursorTick = 0;
	LatestCursorTick = 0;
	EarliestCursorMs = 0.0f;
	LatestCursorMs = 0.0f;
	while (Cursors.PopTail());
}

void FMidiPlayCursorTracker::RecalculateExtents()
{
	EarliestCursorTick = 0;
	LatestCursorTick = 0;
	EarliestCursorMs = 0.0f;
	LatestCursorMs = 0.0f;
	for (auto it = Cursors.begin(); it != Cursors.end(); ++it)
	{
		FMidiPlayCursor* Cursor = it.GetNode();
		if (Cursor->GetLookaheadType() == FMidiPlayCursor::ELookaheadType::Ticks)
		{
			int32 LookAheadTicks = Cursor->GetLookaheadTicks();
			if (LookAheadTicks < 0 && LookAheadTicks < LatestCursorTick)
			{
				LatestCursorTick = LookAheadTicks;
			}
			else if (LookAheadTicks > 0 && LookAheadTicks > EarliestCursorTick)
			{
				EarliestCursorTick = LookAheadTicks;
			}
		}
		else
		{
			float LookAheadMs = Cursor->GetLookaheadMs();
			if (LookAheadMs < 0.0f && LookAheadMs < LatestCursorMs)
			{
				LatestCursorMs = LookAheadMs;
			}
			else if (LookAheadMs > 0 && LookAheadMs > EarliestCursorMs)
			{
				EarliestCursorMs = LookAheadMs;
			}
		}
	}
}

int32 FMidiPlayCursorTracker::GetFarthestAheadCursorTick() const
{
	int32 Result = 0;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(Cursors);
	for (auto it = NonConstCursorList.begin(); it != NonConstCursorList.end(); ++it)
	{
		int32 Tick = it.GetNode()->GetCurrentTick();
		if (Tick > Result)
		{
			Result = Tick;
		}
	}
	return Result;
}

int32 FMidiPlayCursorTracker::GetFarthestBehindCursorTick() const
{
	int32 Result = 0x7FFFFFFF;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(Cursors);
	for (auto it = NonConstCursorList.begin(); it != NonConstCursorList.end(); ++it)
	{
		int32 Tick = it.GetNode()->GetCurrentTick();
		if (Tick < Result)
		{
			Result = Tick;
		}
	}
	return Result;
}

void FMidiPlayCursorTracker::MoveToLoopStart(int32 NewThruTick, float NewThruMs)
{
	CurrentTick = NewThruTick;
	CurrentMs = NewThruMs;

	for (auto it = Cursors.begin(); it != Cursors.end(); ++it)
	{
		it.GetNode()->MoveToLoopStartIfAtNotOffset(NewThruTick, NewThruMs);
	}
}

void FMidiPlayCursorTracker::QueueReset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast)
{
	NewQueuedTick = NewCurrentTick;
	NewQueuedMs = NewCurrentMs;
	NewQueuedPreRollTick = PreRollStartTick;
	NewQueuedPreRollMs = PreRollStartMs;
	NewQueuedBroadcast = Broadcast;
	HasQueuedReset = true;
}

void FMidiPlayCursorTracker::HandleQueuedReset()
{
	if (!HasQueuedReset)
	{
		return;
	}
	Reset(NewQueuedTick, NewQueuedMs, NewQueuedPreRollTick, NewQueuedPreRollMs, NewQueuedBroadcast);
}

void FMidiPlayCursorTracker::Reset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast)
{
	HasQueuedReset = false;
	if (NewCurrentTick == PreRollStartTick)
	{
		Reset(NewCurrentTick, NewCurrentMs, Broadcast);
		return;
	}
	CurrentTick = PreRollStartTick;
	CurrentMs = PreRollStartMs;
	LoopCount = 0;
	for (auto it = Cursors.begin(); it != Cursors.end(); ++it)
	{
		it.GetNode()->Reset(!Broadcast);
	}
	CurrentTick = NewCurrentTick;
	CurrentMs = NewCurrentMs;
	for (auto it = Cursors.begin(); it != Cursors.end();)
	{
		if (!it.GetNode()->AdvanceAsPreRoll())
		{
			auto DeadIt = it;
			++it;
			Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
}

void FMidiPlayCursorTracker::Reset(int32 NewCurrentTick, float NewCurrentMs, bool Broadcast)
{
	HasQueuedReset = false;
	CurrentTick = NewCurrentTick;
	CurrentMs = NewCurrentMs;
	LoopCount = 0;
	for (auto it = Cursors.begin(); it != Cursors.end(); ++it)
	{
		it.GetNode()->Reset(!Broadcast);
	}
}

void FMidiPlayCursorTracker::ResetNewCursor(FMidiPlayCursor* Cursor, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast)
{
	HasQueuedReset = false;
	int32 CurrentTickSaver = CurrentTick;
	float CurrentMsSaver = CurrentMs;
	CurrentTick = PreRollStartTick;
	CurrentMs = PreRollStartMs;
	LoopCount = 0;
	Cursor->Reset(!Broadcast);
	CurrentTick = CurrentTickSaver;
	CurrentMs = CurrentMsSaver;
	Cursor->AdvanceAsPreRoll();
}

