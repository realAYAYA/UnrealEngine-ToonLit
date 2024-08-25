// Copyright Epic Games, Inc. All Rights Reserved.
#include "WallClockMusicClockDriver.h"
#include "Harmonix.h"
#include "Engine/World.h"

bool FWallClockMusicClockDriver::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const
{
	check(IsInGameThread());
	if (!TempoMap.IsValid())
	{
		return false;
	}

	const FSongMaps* Maps = TempoMap->GetSongMaps();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		OutResult.SetByTime((Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::ExperiencedTime:
		OutResult.SetByTime((Clock->CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:
		OutResult.SetByTime((Clock->CurrentVideoRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	}

	return true;
}

void FWallClockMusicClockDriver::Disconnect()
{
	TempoMap = nullptr;
}

bool FWallClockMusicClockDriver::RefreshCurrentSongPos()
{
	check(IsInGameThread());
	check(Clock);
	check(Clock->GetWorld());

	bool TempoChanged = Clock->CurrentSmoothedAudioRenderSongPos.Tempo != Clock->Tempo;

	double RunTime = Clock->GetWorld()->GetTimeSeconds() - StartTimeSecs;

	const FSongMaps* Maps = GetCurrentSongMaps();
	check(Maps);

	Clock->CurrentSmoothedAudioRenderSongPos.SetByTime((float)(RunTime * 1000.0), *Maps);
	Clock->CurrentPlayerExperiencedSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), *Maps);
	Clock->CurrentVideoRenderSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs(), *Maps);

	if (TempoChanged)
	{
		Clock->Tempo = Clock->CurrentSmoothedAudioRenderSongPos.Tempo;
		Clock->CurrentBeatDurationSec = (60.0f / Clock->Tempo) / Clock->CurrentClockAdvanceRate;
		Clock->CurrentBarDurationSec = ((Clock->TimeSignatureNum * Clock->CurrentBeatDurationSec) / (Clock->TimeSignatureDenom / 4.0f)) / Clock->CurrentClockAdvanceRate;
	}

	return true;

}

void FWallClockMusicClockDriver::OnStart()
{
	check(IsInGameThread());
	check(Clock);
	StartTimeSecs = Clock->GetWorld()->GetTimeSeconds();
	PauseTimeSecs = 0.0;
}

void FWallClockMusicClockDriver::OnPause()
{
	check(IsInGameThread());
	check(Clock);
	PauseTimeSecs = Clock->GetWorld()->GetTimeSeconds();
}

void FWallClockMusicClockDriver::OnContinue()
{
	check(IsInGameThread());
	check(Clock);
	double CurrentTime = Clock->GetWorld()->GetTimeSeconds();
	StartTimeSecs += (CurrentTime - PauseTimeSecs);
	PauseTimeSecs = 0.0;
	RefreshCurrentSongPos();
}

const FSongMaps* FWallClockMusicClockDriver::GetCurrentSongMaps() const
{
	if (TempoMap.IsValid())
	{
		return TempoMap->GetSongMaps();
	}
	check(IsInGameThread());
	check(Clock);
	return &Clock->DefaultMaps;
}
