// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorTransportController.h"

FWaveformEditorTransportController::FWaveformEditorTransportController(UAudioComponent* InAudioComponent)
	: AudioComponent(InAudioComponent)
{}

FWaveformEditorTransportController::~FWaveformEditorTransportController()
{
	Stop();
}

void FWaveformEditorTransportController::Play()
{
	if (!CanPlay())
	{
		return;
	}

	if (IsPaused())
	{
		AudioComponent->SetPaused(false);

		if (!bCachedTimeDuringPause)
		{
			return;
		}
		
	}

	AudioComponent->Play(CachedAudioStartTime);
	
}

void FWaveformEditorTransportController::Play(const float StartTime)
{
	if (!CanPlay())
	{
		return;
	}

	if (IsPaused())
	{
		AudioComponent->SetPaused(false);
	}

	CacheStartTime(StartTime);
	AudioComponent->Play(CachedAudioStartTime);
}

void FWaveformEditorTransportController::Pause()
{
	AudioComponent->SetPaused(true);
}


void FWaveformEditorTransportController::Stop()
{
	if (!CanStop())
	{
		return;
	}

	Play(0.f);
	AudioComponent->StopDelayed(0.1f);

	if (IsPaused())
	{
		AudioComponent->SetPaused(false);
	}

	bCachedTimeDuringPause = false;
}

void FWaveformEditorTransportController::TogglePlayback()
{
	if (IsPlaying())
	{
		Pause();
	}
	else
	{
		Play();
	}
}

bool FWaveformEditorTransportController::CanPlay() const
{
	return SoundBaseIsValid();
}

bool FWaveformEditorTransportController::CanStop() const
{
	return (IsPlaying() || IsPaused());
}

bool FWaveformEditorTransportController::IsPaused() const
{
	return AudioComponent->GetPlayState() == EAudioComponentPlayState::Paused;
}

bool FWaveformEditorTransportController::IsPlaying() const
{
	return AudioComponent->GetPlayState() == EAudioComponentPlayState::Playing;
}

void FWaveformEditorTransportController::CacheStartTime(const float StartTime)
{
	CachedAudioStartTime = StartTime;

	if (IsPaused())
	{
		bCachedTimeDuringPause = true;
	}
	else
	{
		bCachedTimeDuringPause = false;
	}
}

void FWaveformEditorTransportController::Seek(const float SeekTime)
{
	AudioComponent->Play(SeekTime);
}

const bool FWaveformEditorTransportController::SoundBaseIsValid() const
{
	return AudioComponent->GetSound() != nullptr;
}