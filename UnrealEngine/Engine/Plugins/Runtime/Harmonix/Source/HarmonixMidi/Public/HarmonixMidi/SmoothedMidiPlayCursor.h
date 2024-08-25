// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MidiPlayCursor.h"
#include "Harmonix/LocalMinimumMagnitudeTracker.h"
#include "Harmonix/VariableSpeedTimer.h"
#include "MidiSongPos.h"

class HARMONIXMIDI_API FSmoothedMidiPlayCursor : public FMidiPlayCursor
{
public:
	FSmoothedMidiPlayCursor();

	void  SetSmoothingLatencyMs(float ms) { SmoothingLatencyMs = ms; }
	float GetSmoothedMs() const           { return SmoothedMs;       }
	float GetSmoothedTick() const         { return SmoothedTick;     }
	const FMidiSongPos& GetCurrentSongPos() const { return CurrentSongPos; }

	//** BEGIN FMidiPlayCursor
	virtual void Reset(bool ForceNoBroadcast = false) override;
	virtual bool UpdateWithTrackerUnchanged() override;
	virtual void AdvanceByTicks(bool processLoops = true, bool broadcast = true, bool isPreRoll = false) override;
	virtual void AdvanceByMs(bool processLoops = true, bool broadcast = true, bool isPreRoll = false) override;
	virtual void OnLoop(int loopStartTick, int loopEndTick) override;
	//** END FMidiPlayCursor

protected:

private:
	static const int kFramesOfErrorHistory = 10;
	FLocalMinimumMagnitudeTracker<kFramesOfErrorHistory> ErrorTracker;
	FVariableSpeedTimer SmoothingTimer;

	float SmoothingLatencyMs = 30.0f;
	float SmoothedMsDelta    = 0.0f;
	float SmoothedMs         = 0.0f;
	float SmoothedTick       = 0.0f;
	bool  LoopedThisPass     = false;
	float BaseSpeed          = 1.0f;
	FMidiSongPos CurrentSongPos;

	void  SetSpeed(float NewSpeed);
	void  SyncSmoothingTimer(bool bEnableErrorCorrection);
	void  UpdateSongPosition();
};
