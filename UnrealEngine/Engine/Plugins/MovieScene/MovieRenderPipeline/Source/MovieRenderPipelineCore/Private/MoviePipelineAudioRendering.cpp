// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipeline.h"
#include "HAL/IConsoleManager.h"
#include "AudioDeviceManager.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Engine/Engine.h"
#include "AudioMixerBlueprintLibrary.h"
#include "LevelSequence.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "MoviePipelineMasterConfig.h"
#include "MovieRenderPipelineDataTypes.h"
#include "AudioMixerSubmix.h"
#include "DSP/BufferVectorOperations.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "AudioMixerPlatformNonRealtime.h"
#include "MoviePipelineQueue.h"

static FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* WorldContextObject)
{
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!ThisWorld || !ThisWorld->bAllowAudioPlayback || ThisWorld->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	return ThisWorld->GetAudioDeviceRaw();
}

static Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* WorldContextObject)
{
	if (FAudioDevice* AudioDevice = GetAudioDeviceFromWorldContext(WorldContextObject))
	{
		if (!AudioDevice->IsAudioMixerEnabled())
		{
			return nullptr;
		}
		else
		{
			return static_cast<Audio::FMixerDevice*>(AudioDevice);
		}
	}
	return nullptr;
}

void UMoviePipeline::SetupAudioRendering()
{
	// Ensure that we try to play audio at full volume, even if we're unfocused.
	AudioState.PrevUnfocusedAudioMultiplier = FApp::GetUnfocusedVolumeMultiplier();
	FApp::SetUnfocusedVolumeMultiplier(1.f);

	IConsoleVariable* AudioRenderEveryTickCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick"));
	// Will be null if the NRT module wasn't loaded
	if (AudioRenderEveryTickCvar)
	{
		AudioState.PrevRenderEveryTickValue = AudioRenderEveryTickCvar->GetInt();
		// Override it to prevent it from automatically ticking, we'll control this below.
		AudioRenderEveryTickCvar->Set(0, ECVF_SetByConstructor);
	}
}

void UMoviePipeline::TeardownAudioRendering()
{
	// Restore previous unfocused audio multiplier, to no longer force audio when unfocused
	FApp::SetUnfocusedVolumeMultiplier(AudioState.PrevUnfocusedAudioMultiplier);

	// Restore our cached CVar value.
	IConsoleVariable* AudioRenderEveryTickCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick"));

	// This will be null if the NRT wasn't used (module not loaded).
	if (AudioRenderEveryTickCvar)
	{
		AudioRenderEveryTickCvar->Set(AudioState.PrevRenderEveryTickValue, ECVF_SetByConstructor);

	}
}

void UMoviePipeline::StartAudioRecording()
{
	AudioState.bIsRecordingAudio = true;

	Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(this);
	if (MixerDevice)
	{
		TWeakPtr<Audio::FMixerSubmix, ESPMode::ThreadSafe> MasterSubmix = MixerDevice->GetMasterSubmix();
		if (MasterSubmix.Pin())
		{
			AudioState.ActiveSubmixes.Add(MasterSubmix);
		}

		for (TWeakPtr<Audio::FMixerSubmix, ESPMode::ThreadSafe> WeakSubmix : AudioState.ActiveSubmixes)
		{
			const float ExpectedDuration = 30.f;
			WeakSubmix.Pin()->OnStartRecordingOutput(ExpectedDuration);
		}
	}
}

void UMoviePipeline::StopAudioRecording()
{
	AudioState.bIsRecordingAudio = false;

	for (TWeakPtr<Audio::FMixerSubmix, ESPMode::ThreadSafe> WeakSubmix : AudioState.ActiveSubmixes)
	{
		TSharedPtr<Audio::FMixerSubmix, ESPMode::ThreadSafe> Submix = WeakSubmix.Pin();
		if (Submix)
		{
			MoviePipeline::FAudioState::FAudioSegment& NewSegment = AudioState.FinishedSegments.AddDefaulted_GetRef();
			NewSegment.OutputState = CachedOutputState;

			// Copy the data returned by the Submix.
			Audio::AlignedFloatBuffer& AudioRecording = Submix->OnStopRecordingOutput(NewSegment.NumChannels, NewSegment.SampleRate);
			NewSegment.SegmentData = AudioRecording; 
		}
	}

	AudioState.ActiveSubmixes.Reset();
}

void UMoviePipeline::ProcessAudioTick()
{
	// Only supported on the new audio mixer (with the non-realtime device, windows only).
	Audio::FMixerDevice* MixerDevice = GetAudioMixerDeviceFromWorldContext(this);
	if (!MixerDevice)
	{
		return;
	}

	Audio::IAudioMixerPlatformInterface* AudioMixerPlatform = MixerDevice->GetAudioMixerPlatform();
	Audio::FMixerPlatformNonRealtime* NRTPlatform = nullptr;
	if (AudioMixerPlatform && AudioMixerPlatform->IsNonRealtime())
	{
		// There is only one non-realtime audio platform at this time, so we can safely static_cast this due to the IsNonRealtime check.
		NRTPlatform = static_cast<Audio::FMixerPlatformNonRealtime*>(MixerDevice->GetAudioMixerPlatform());
	}

	if (!NRTPlatform)
	{
		return;
	}


	UMoviePipelineExecutorShot* CurrentShot = ActiveShotList[CurrentShotIndex];

	// Start capturing any produced samples on the same frame we start submitting samples that will make it to disk.
	// This comes before we process samples for this frame (below).
	if (CurrentShot->ShotInfo.State == EMovieRenderShotState::Rendering && !AudioState.bIsRecordingAudio)
	{
		// There can be time in the Unreal world that passes when MRQ is not yet active. When MRQ isn't active,
		// then the NRT audio platform doesn't process anything and it builds up for when we do start to actually
		// process it for a shot. This causes our actual audio to become offset from the sequence once lined up
		// afterwards. So for now we'll just flush out anything that needed to be processed before we start capturing
			
		MixerDevice->Update(true);
		
		// We don't have a great way of determining how much outstanding audio there is, so we're just going to 
		// process an arbitrary amount of time.
		NRTPlatform->RenderAudio(30.f);
		
		StartAudioRecording();
	}

	// This needs to tick every frame so we process and clear audio that happens between shots while the world is running.
	// We have special ticking logic while rendering due to temporal sampling, and otherwise just fall back to normal.
	bool bCanRenderAudio = true;
	double AudioDeltaTime = FApp::GetDeltaTime();
	
	if(CurrentShot->ShotInfo.State == EMovieRenderShotState::Rendering)
	{
		// The non-real time audio renderer desires even engine time steps. Unfortunately, when using temporal sampling
		// we don't have an even timestep. However, because it's non-real time, and we're accumulating the results into
		// a single frame anyways, we can bunch up the audio work and then process it when we've reached the end of a frame.
		bCanRenderAudio = CachedOutputState.IsLastTemporalSample();
		
		// Process work that has been submitted from the game thread to the audio thread over the temporal samples of this frame.
		AudioDeltaTime = GetPipelineMasterConfig()->GetEffectiveFrameRate(TargetSequence).AsInterval();
	}

	{
		// Handle any game logic that changed Audio State.
		MixerDevice->Update(true);
	}

	{
		// Process work that has been submitted from the game thread to the audio thread over the temporal samples of this frame.
		FFrameRate OutputFrameRate = GetPipelineMasterConfig()->GetEffectiveFrameRate(TargetSequence);
		NRTPlatform->RenderAudio(OutputFrameRate.AsInterval());
	}
}
