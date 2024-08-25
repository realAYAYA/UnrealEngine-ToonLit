// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundMusicClockDriver.h"
#include "MetasoundGeneratorHandle.h"
#include "Components/AudioComponent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundGenerator.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "Harmonix.h"

bool FMetasoundMusicClockDriver::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const
{
	check(IsInGameThread());

	// if we have an owner, ask them directly
	if (CursorOwner)
	{
		switch (Timebase)
		{
		case ECalibratedMusicTimebase::AudioRenderTime:
			OutResult = CursorOwner->CalculateLowResSongPosRelativeToCurrentMs((Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset);
			break;
		case ECalibratedMusicTimebase::ExperiencedTime:
			OutResult = CursorOwner->CalculateLowResSongPosRelativeToCurrentMs((Clock->CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset);
			break;
		case ECalibratedMusicTimebase::VideoRenderTime:
		default:
			OutResult = CursorOwner->CalculateLowResSongPosRelativeToCurrentMs((Clock->CurrentVideoRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset);
			break;
		}
		return true;
	}
	return false;
}

bool FMetasoundMusicClockDriver::RefreshCurrentSongPos()
{
	//	Only for use when on the game thread.
	if (ensureMsgf(
			IsInGameThread(),
			TEXT("%hs called from non-game thread.  This is not supported"),
			__FUNCTION__) == false)
	{
		return false;
	}

	if (AudioComponentToWatch.IsValid())
	{
		if (!CurrentGeneratorHandle)
		{
			// cursor connection is not pending
			AttemptToConnectToAudioComponentsMetasound();
		}
	}

	if (CursorOwner)
	{
		// cursor is attached and has the current info
		RefreshCurrentSongPosFromCursor();
		return true;
	}
	else
	{
		// Cursor not attached so use wall clock
		if (!WasEverConnected || Clock->RunPastMusicEnd)
		{
			RefreshCurrentSongPosFromWallClock();
			return true;
		}
	}
	return false;
}

void FMetasoundMusicClockDriver::OnStart()
{
	check(IsInGameThread());

	SongPosOffsetMs = 0.0f;
	FreeRunStartTimeSecs = Clock ? Clock->GetWorld()->GetTimeSeconds() : 0.0;
}

void FMetasoundMusicClockDriver::OnContinue()
{
	if (!CursorOwner)
	{
		RefreshCurrentSongPosFromWallClock();
	}
}

void FMetasoundMusicClockDriver::Disconnect()
{
	if (CursorOwner)
	{
		CursorOwner->UnregisterPlayCursor(&Cursor);
		CursorOwner.Reset();
	}
	DetachAllCallbacks();
	AudioComponentToWatch.Reset();
	CurrentGeneratorHandle.Reset();
}

const FSongMaps* FMetasoundMusicClockDriver::GetCurrentSongMaps() const
{
	check(IsInGameThread());
	if (CursorOwner)
	{
		return &CursorOwner->GetSongMaps();
	}
	return &Clock->DefaultMaps;
}

bool FMetasoundMusicClockDriver::ConnectToAudioComponentsMetasound(UAudioComponent* InAudioComponent, FName MetasoundOuputPinName)
{
	AudioComponentToWatch = InAudioComponent;
	MetasoundOutputName = MetasoundOuputPinName;
	return AttemptToConnectToAudioComponentsMetasound();
}

void FMetasoundMusicClockDriver::ResetCursorOwner(TSharedPtr<FMidiPlayCursorMgr> MidiPlayCursorMgr)
{
	check(IsInGameThread());

	// Verify that the cursor owner is changing.
	if (CursorOwner != MidiPlayCursorMgr)
	{
		if (MidiPlayCursorMgr)
		{
			// Register with the new cursor owner and broadcast the clock connection event.
			MidiPlayCursorMgr->RegisterLowResPlayCursor(&Cursor);
			CursorOwner = MoveTemp(MidiPlayCursorMgr);
			WasEverConnected = true;

			check(Clock);
			Clock->MusicClockConnectedEvent.Broadcast();
		}
		else
		{
			// Unregister the old cursor owner and broadcast the clock disconnection event.
			CursorOwner->UnregisterPlayCursor(&Cursor);
			if (Clock->GetState() != EMusicClockState::Stopped)
			{
				Clock->DefaultMaps.Copy(CursorOwner->GetSongMaps(), 0, Cursor.GetCurrentTick());
				SongPosOffsetMs = Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f;
				FreeRunStartTimeSecs = Clock->GetWorld()->GetTimeSeconds();
			}
			CursorOwner.Reset();

			check(Clock);
			Clock->MusicClockDisconnectedEvent.Broadcast();
		}
	}
}

bool FMetasoundMusicClockDriver::AttemptToConnectToAudioComponentsMetasound()
{
	check(IsInGameThread());
	if (!AudioComponentToWatch.IsValid() || MetasoundOutputName.IsNone())
	{
		return false;
	}

	DetachAllCallbacks();

	CurrentGeneratorHandle.Reset(UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(AudioComponentToWatch.Get()));
	if (!CurrentGeneratorHandle)
	{
		return false;
	}
	GeneratorAttachedCallbackHandle = CurrentGeneratorHandle->OnGeneratorHandleAttached.AddLambda([this](){OnGeneratorAttached();});
	GeneratorDetachedCallbackHandle = CurrentGeneratorHandle->OnGeneratorHandleDetached.AddLambda([this](){OnGeneratorDetached();});
	GeneratorIOUpdatedCallbackHandle = CurrentGeneratorHandle->OnIOUpdated.AddLambda([this](){OnGeneratorIOUpdated();});
	UMetasoundGeneratorHandle::FOnSetGraph::FDelegate OnSetGraph;
	OnSetGraph.BindLambda([this](){OnGraphSet();});
	GraphChangedCallbackHandle = CurrentGeneratorHandle->AddGraphSetCallback(MoveTemp(OnSetGraph));
	OnGeneratorAttached();
	return true;
}

void FMetasoundMusicClockDriver::DetachAllCallbacks()
{
	if (CurrentGeneratorHandle)
	{
		CurrentGeneratorHandle->OnGeneratorHandleAttached.Remove(GeneratorAttachedCallbackHandle);
		GeneratorAttachedCallbackHandle.Reset();
		CurrentGeneratorHandle->OnGeneratorHandleDetached.Remove(GeneratorDetachedCallbackHandle);
		GeneratorDetachedCallbackHandle.Reset();
		CurrentGeneratorHandle->OnIOUpdated.Remove(GeneratorIOUpdatedCallbackHandle);
		GeneratorIOUpdatedCallbackHandle.Reset();
		CurrentGeneratorHandle->RemoveGraphSetCallback(GraphChangedCallbackHandle);
		GraphChangedCallbackHandle.Reset();
	}
}

void FMetasoundMusicClockDriver::OnGeneratorAttached()
{
	TryToRegisterPlayCursor();
}

void FMetasoundMusicClockDriver::OnGraphSet()
{
	TryToRegisterPlayCursor();
}

void FMetasoundMusicClockDriver::OnGeneratorIOUpdated()
{
	// An output vertex update may have destroyed our Clock, reattach to the new one if it's there
	// FUTURE: There isn't a way to listen to vertex changes on a specific Metasound output, so until then this will be called every time IO changes
	TryToRegisterPlayCursor();
}

void FMetasoundMusicClockDriver::OnGeneratorDetached()
{
	ResetCursorOwner();
}

void FMetasoundMusicClockDriver::TryToRegisterPlayCursor()
{
	check(IsInGameThread());
	check(Clock);

	if (!CurrentGeneratorHandle || MetasoundOutputName.IsNone())
	{
		return;
	}

	if (TSharedPtr<Metasound::FMetasoundGenerator> LowLevelGenerator = CurrentGeneratorHandle->GetGenerator())
	{
		// Send a command to the OnGenerateAudio thread, where it is safe to interact with the low level generator's output read references.
		LowLevelGenerator->OnNextBuffer([MetasoundOutputName = MetasoundOutputName, ClockWeakPtr = TWeakObjectPtr<UMusicClockComponent>(Clock), ClockDriverWeakPtr = AsWeak()](Metasound::FMetasoundGenerator& LowLevelGenerator) mutable
			{
				// Try to get a cursor manager from the named midi clock output.
				TSharedPtr<FMidiPlayCursorMgr> MidiPlayCursorMgr;
				const TOptional<Metasound::TDataReadReference<HarmonixMetasound::FMidiClock>> MidiClockRef = LowLevelGenerator.GetOutputReadReference<HarmonixMetasound::FMidiClock>(MetasoundOutputName);
				if (const Metasound::TDataReadReference<HarmonixMetasound::FMidiClock>* MidiClock = MidiClockRef.GetPtrOrNull())
				{
					MidiPlayCursorMgr = (*MidiClock)->GetDrivingMidiPlayCursorMgr();
				}
				else
				{
					UE_LOG(LogMusicClock, Verbose, TEXT("Didn't find MIDI Clock output named \"%s\" in the Metasound!"), *MetasoundOutputName.ToString());
				}

				// Send a command to the game thread, where it is safe to modify clock component state.
				AsyncTask(ENamedThreads::GameThread, [ClockWeakPtr = MoveTemp(ClockWeakPtr), ClockDriverWeakPtr = MoveTemp(ClockDriverWeakPtr), MidiPlayCursorMgr = MoveTemp(MidiPlayCursorMgr)]() mutable
					{
						if (UMusicClockComponent* Clock = ClockWeakPtr.Get())
						{
							if (TSharedPtr<FMetasoundMusicClockDriver> ClockDriver = StaticCastSharedPtr<FMetasoundMusicClockDriver>(ClockDriverWeakPtr.Pin()))
							{
								// Verify that the music clock component still references the clock driver.
								if (ClockDriver == Clock->ClockDriver)
								{
									ClockDriver->ResetCursorOwner(MoveTemp(MidiPlayCursorMgr));
								}
							}
						}
					});
			});
	}
}

void FMetasoundMusicClockDriver::RefreshCurrentSongPosFromWallClock()
{
	check(Clock);

	bool TempoChanged = Clock->CurrentSmoothedAudioRenderSongPos.Tempo != Clock->Tempo;

	double FreeRunTime = (Clock->GetWorld()->GetTimeSeconds() - FreeRunStartTimeSecs) * Clock->CurrentClockAdvanceRate;

	Clock->CurrentSmoothedAudioRenderSongPos.SetByTime(((float)FreeRunTime * 1000.0) + SongPosOffsetMs, Clock->DefaultMaps);
	Clock->CurrentPlayerExperiencedSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), Clock->DefaultMaps);
	Clock->CurrentVideoRenderSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs(), Clock->DefaultMaps);
	if (Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn > Clock->RawUnsmoothedAudioRenderPos.SecondsIncludingCountIn)
	{
		Clock->RawUnsmoothedAudioRenderPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f, Clock->DefaultMaps);
	}

	if (TempoChanged)
	{
		Clock->Tempo = Clock->CurrentSmoothedAudioRenderSongPos.Tempo;
		Clock->CurrentBeatDurationSec = (60.0f / Clock->Tempo) / Clock->CurrentClockAdvanceRate;
		Clock->CurrentBarDurationSec = ((Clock->TimeSignatureNum * Clock->CurrentBeatDurationSec) / (Clock->TimeSignatureDenom / 4.0f)) / Clock->CurrentClockAdvanceRate;
	}
}

void FMetasoundMusicClockDriver::RefreshCurrentSongPosFromCursor()
{
	check(IsInGameThread());
	check(Clock);
	Clock->CurrentSmoothedAudioRenderSongPos = Cursor.GetCurrentSongPos();

	float PrevClockAdvanceRate = Clock->CurrentClockAdvanceRate;
	if (CursorOwner)
	{
		Clock->CurrentClockAdvanceRate = CursorOwner->GetLowResAdvanceRate();
		Clock->CurrentPlayerExperiencedSongPos = CursorOwner->CalculateLowResSongPosRelativeToCurrentMs(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs());
		Clock->CurrentVideoRenderSongPos = CursorOwner->CalculateLowResSongPosRelativeToCurrentMs(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs());
		Clock->RawUnsmoothedAudioRenderPos = CursorOwner->CalculateLowResSongPosWithOffsetMs(0.0f);
	}
	else
	{
		Clock->CurrentPlayerExperiencedSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::Get().GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), Clock->DefaultMaps);
		Clock->CurrentVideoRenderSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::Get().GetMeasuredVideoToAudioRenderOffsetMs(), Clock->DefaultMaps);
		if (Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn > Clock->RawUnsmoothedAudioRenderPos.SecondsIncludingCountIn)
		{
			Clock->RawUnsmoothedAudioRenderPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f, Clock->DefaultMaps);
		}
	}

	Clock->TimeSignatureNum = Clock->CurrentSmoothedAudioRenderSongPos.TimeSigNumerator;
	Clock->TimeSignatureDenom = Clock->CurrentSmoothedAudioRenderSongPos.TimeSigDenominator;

	if (Clock->Tempo != Clock->CurrentSmoothedAudioRenderSongPos.Tempo || PrevClockAdvanceRate != Clock->CurrentClockAdvanceRate)
	{
		Clock->Tempo = Clock->CurrentSmoothedAudioRenderSongPos.Tempo;
		Clock->CurrentBeatDurationSec = (60.0f / Clock->Tempo) / Clock->CurrentClockAdvanceRate;
		Clock->CurrentBarDurationSec = ((Clock->TimeSignatureNum * Clock->CurrentBeatDurationSec) / (Clock->TimeSignatureDenom / 4.0f)) / Clock->CurrentClockAdvanceRate;
	}
}
