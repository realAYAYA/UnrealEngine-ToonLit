// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/SmoothedMidiPlayCursor.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSmoothedMidiPlayCursor, Log, Log);

namespace SmoothedPlayCursor
{
	static constexpr float kMassiveJump = 60.0f;
	static constexpr float kMinorErrorThreshold = 5.0f;
	static constexpr float kMinorCorrectionFactor = 0.002;
	static constexpr float kMajorErrorThreshold = 20.0f;
	static constexpr float kMajorCorrectionFactor = 2.0 * kMinorCorrectionFactor;
}

FSmoothedMidiPlayCursor::FSmoothedMidiPlayCursor()
{
	SetupMsLookahead(-SmoothingLatencyMs, FMidiPlayCursor::ESyncOptions::NoBroadcastNoPreRoll);
	SmoothingTimer.SetSpeed(1.0);
	SmoothingTimer.Start();
	SmoothingTimer.Reset(0.0);
}

void FSmoothedMidiPlayCursor::SetSpeed(float NewSpeed)
{
	if (BaseSpeed == NewSpeed)
		return;
	BaseSpeed = NewSpeed;
	SetupMsLookahead(-SmoothingLatencyMs * BaseSpeed, FMidiPlayCursor::ESyncOptions::NoBroadcastNoPreRoll);
	if (Tracker)
		Tracker->RecalculateExtents();
	SmoothingTimer.SetSpeed(BaseSpeed);
}

void FSmoothedMidiPlayCursor::Reset(bool ForceNoBroadcast)
{
	FMidiPlayCursor::Reset(ForceNoBroadcast);

	ErrorTracker.Reset();
	SmoothingLatencyMs = 30.0f;
	SmoothedMs = 0.0f;
	SmoothedTick = 0.0f;
	LoopedThisPass = false;
	BaseSpeed = 1.0f;
	CurrentSongPos.Reset();
}

bool FSmoothedMidiPlayCursor::UpdateWithTrackerUnchanged()
{
	if (!FMidiPlayCursor::UpdateWithTrackerUnchanged())
	{
		return false;
	}

	SyncSmoothingTimer(false);
	UpdateSongPosition();
	return true;
}

void FSmoothedMidiPlayCursor::AdvanceByTicks(bool processLoops /*= true*/, bool broadcast /*= true*/, bool isPreRoll /*= false*/)
{
	FMidiPlayCursor::AdvanceByTicks(processLoops, broadcast, isPreRoll);
	SyncSmoothingTimer(true);
	UpdateSongPosition();
}

void FSmoothedMidiPlayCursor::AdvanceByMs(bool processLoops /*= true*/, bool broadcast /*= true*/, bool isPreRoll /*= false*/)
{
	FMidiPlayCursor::AdvanceByMs(processLoops, broadcast, isPreRoll);
	SyncSmoothingTimer(true);
	UpdateSongPosition();
}

void FSmoothedMidiPlayCursor::OnLoop(int loopStartTick, int loopEndTick)
{
	LoopedThisPass = true;
}

void FSmoothedMidiPlayCursor::SyncSmoothingTimer(bool bEnableErrorCorrection)
{
	FMidiPlayCursorMgr* CursorOwner = GetOwner();
	if (!CursorOwner)
	{
		return;
	}

	if (!SmoothingTimer.IsRunning())
	{
		SmoothingTimer.Start();
	}

	SetSpeed(CursorOwner->GetCurrentAdvanceRate(Tracker->IsLowRes));

	float RawMs = CurrentMs;
	double SmoothMs = SmoothingTimer.Ms();

	if (bEnableErrorCorrection)
	{
		float Error = float((SmoothMs - RawMs) * (1.0 / SmoothingTimer.GetSpeed()));
		ErrorTracker.Push(Error);
		float MinRecentError = ErrorTracker.Min();
		float AbsError = FMath::Abs(MinRecentError);

		// if we have a massive jump just slam the thing to the current "raw" time and be done with it...
		if (FMath::Abs(Error) > SmoothedPlayCursor::kMassiveJump)
		{
			UE_LOG(LogSmoothedMidiPlayCursor, Verbose, TEXT("Smoothing: Massive Error = %fms. Slamming."), Error);
			SmoothingTimer.SetSpeed(BaseSpeed);
			SmoothingTimer.Reset(RawMs);
			SmoothedMs = RawMs;
			SmoothedMsDelta = 0.0f;
			SmoothedTick = CursorOwner->GetTempoMap().MsToTick(RawMs);
			ErrorTracker.Reset();
			return;
		}

		if (AbsError < SmoothedPlayCursor::kMinorErrorThreshold)
		{
			if (SmoothingTimer.GetSpeed() != BaseSpeed)
			{
				UE_LOG(LogSmoothedMidiPlayCursor, Verbose, TEXT("Smoothing: Error = %fms. Running normal speed."), MinRecentError);
				SmoothingTimer.SetSpeed(BaseSpeed);
			}
		}
		else if (AbsError < SmoothedPlayCursor::kMajorErrorThreshold)
		{
			float NewSpeedFactor = BaseSpeed + (MinRecentError < 0.0f ? SmoothedPlayCursor::kMinorCorrectionFactor : -SmoothedPlayCursor::kMinorCorrectionFactor);
			if (SmoothingTimer.GetSpeed() != NewSpeedFactor)
			{
				UE_LOG(LogSmoothedMidiPlayCursor, Verbose, TEXT("Smoothing: Small error = %f. Adjusting speedfactor to %f."), MinRecentError, NewSpeedFactor);
				SmoothingTimer.SetSpeed(NewSpeedFactor);
			}
		}
		else
		{
			float NewSpeedFactor = BaseSpeed + (MinRecentError < 0.0f ? SmoothedPlayCursor::kMajorCorrectionFactor : -SmoothedPlayCursor::kMajorCorrectionFactor);
			if (SmoothingTimer.GetSpeed() != NewSpeedFactor)
			{
				UE_LOG(LogSmoothedMidiPlayCursor, Verbose, TEXT("Smoothing: Significant error = %f. Adjusting speedfactor to %f."), MinRecentError, NewSpeedFactor);
				SmoothingTimer.SetSpeed(NewSpeedFactor);
			}
		}
	}

	// keep track of the difference in Ms between the current time and the smoothed time
	// update the MsDelta before applying any looping so that it's 
	SmoothedMsDelta = float(SmoothMs - RawMs);

	// we need to see if the smoothed position is a position that we have never played!
	float LoopStartMs = CursorOwner->GetLoopStartMs(Tracker->IsLowRes);
	float LoopEndMs = CursorOwner->GetLoopEndMs(Tracker->IsLowRes);
	if (LoopStartMs < LoopEndMs)
	{
		// possible loop back...
		if (SmoothMs > LoopEndMs)
		{
			// yikes, we smoothed past the end of the loop!
			SmoothMs = LoopStartMs + (SmoothMs - LoopEndMs);
		}
	}
	else if (LoopStartMs > LoopEndMs)
	{
		// possible jump forward...
		if (SmoothMs >= LoopEndMs && SmoothMs < LoopStartMs)
		{
			// yikes, we smoothed into the gap!
			SmoothMs = LoopStartMs + (SmoothMs - LoopEndMs);
		}
	}
	SmoothedMs = float(SmoothMs);
	SmoothedTick = CursorOwner->GetTempoMap().MsToTick(SmoothedMs);
	LoopedThisPass = false;
}

void FSmoothedMidiPlayCursor::UpdateSongPosition()
{
	FMidiPlayCursorMgr* CursorOwner = GetOwner();
	if (!CursorOwner)
	{
		return;
	}

	CurrentSongPos = CursorOwner->CalculateSongPosWithOffsetMs(SmoothedMsDelta, Tracker->IsLowRes);
}
