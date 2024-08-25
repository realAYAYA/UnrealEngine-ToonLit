// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMidi/MidiFile.h"
#include "UObject/StrongObjectPtr.h"

struct FWallClockMusicClockDriver : public FMusicClockDriverBase
{
public:
	FWallClockMusicClockDriver(UMusicClockComponent* InClock, UMidiFile* InTempoMap)
		: FMusicClockDriverBase(InClock)
		, TempoMap(InTempoMap)
	{}

	virtual bool CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const override;

	virtual void Disconnect() override;
	virtual bool RefreshCurrentSongPos() override;
	virtual void OnStart() override;
	virtual void OnPause() override;
	virtual void OnContinue() override;
	virtual void OnStop() override {}
	virtual const FSongMaps* GetCurrentSongMaps() const override;

private:
	TWeakObjectPtr<UMidiFile> TempoMap;

	double StartTimeSecs = 0.0;
	double PauseTimeSecs = 0.0f;
};