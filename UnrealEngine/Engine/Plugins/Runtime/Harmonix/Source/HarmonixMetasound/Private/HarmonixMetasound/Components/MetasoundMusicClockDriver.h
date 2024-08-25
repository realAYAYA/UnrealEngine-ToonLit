// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "MetasoundGeneratorHandle.h"
#include "UObject/StrongObjectPtr.h"

struct FMetasoundMusicClockDriver : public FMusicClockDriverBase
{
public:
	FMetasoundMusicClockDriver(UMusicClockComponent* InClock)
		: FMusicClockDriverBase(InClock)
	{}

	virtual bool CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const override;

	virtual void Disconnect() override;
	virtual bool RefreshCurrentSongPos() override;
	virtual void OnStart() override;
	virtual void OnPause() override {}
	virtual void OnContinue() override;
	virtual void OnStop() override {}
	virtual const FSongMaps* GetCurrentSongMaps() const override;


	bool ConnectToAudioComponentsMetasound(UAudioComponent* InAudioComponent, FName MetasoundOuputPinName = "MIDI Clock");

protected:
	void OnGeneratorAttached();
	void OnGeneratorDetached();
	void OnGraphSet();
	void OnGeneratorIOUpdated();

private:
	FName MetasoundOutputName;
	// We can keep a week reference to this because our "owner" is a UClass and
	// also has a reference to it...
	TWeakObjectPtr<UAudioComponent> AudioComponentToWatch;
	// We need a strong object ptr to this next thing since we will be the only one holding a reference to it...
	TStrongObjectPtr<UMetasoundGeneratorHandle> CurrentGeneratorHandle;

	FSmoothedMidiPlayCursor Cursor;
	TSharedPtr<FMidiPlayCursorMgr> CursorOwner;
	double FreeRunStartTimeSecs = 0.0;
	bool WasEverConnected = false;
	float SongPosOffsetMs = 0.0f;

	FDelegateHandle GeneratorAttachedCallbackHandle;
	FDelegateHandle GeneratorDetachedCallbackHandle;
	FDelegateHandle GeneratorIOUpdatedCallbackHandle;
	FDelegateHandle GraphChangedCallbackHandle;

	void ResetCursorOwner(TSharedPtr<FMidiPlayCursorMgr> MidiPlayCursorMgr = nullptr);

	bool AttemptToConnectToAudioComponentsMetasound();
	void DetachAllCallbacks();
	void TryToRegisterPlayCursor();
	void RefreshCurrentSongPosFromWallClock();
	void RefreshCurrentSongPosFromCursor();
};
