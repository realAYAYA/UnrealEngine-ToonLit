// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/AudioComponent.h"

class FWaveformEditorTransportController
{
public:
	explicit FWaveformEditorTransportController(UAudioComponent* InAudioComponent);
	~FWaveformEditorTransportController();

	void Play();
	void Play(const float StartTime);
	void Pause();
	void Stop();
	void TogglePlayback();
	bool CanPlay() const;
	bool CanStop() const;
	bool IsPaused() const;
	bool IsPlaying() const;
	void CacheStartTime(const float StartTime);
	void Seek(const float SeekTime);

private:
	const bool SoundBaseIsValid() const;

	UAudioComponent* AudioComponent = nullptr;
	float CachedAudioStartTime = 0.f;
	bool bCachedTimeDuringPause = false;

	FDelegateHandle PlayStateChangeDelegateHandle;
};