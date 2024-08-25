// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphDefaultAudioRenderer.h"

#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformNonRealtime.h"
#include "AudioMixerSubmix.h"
#include "DSP/BufferVectorOperations.h"
#include "Engine/Engine.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphUtils.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "HAL/IConsoleManager.h"
#include "MoviePipelineQueue.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Graph/MovieGraphPipeline.h"

void UMovieGraphDefaultAudioRenderer::StartAudioRecording()
{
	AudioState.bIsRecordingAudio = true;

	if (Audio::FMixerDevice* MixerDevice = UE::MovieGraph::Audio::GetAudioMixerDeviceFromWorldContext(this))
	{
		const TWeakPtr<Audio::FMixerSubmix> MasterSubmix = MixerDevice->GetMasterSubmix();
		if (MasterSubmix.Pin())
		{
			AudioState.ActiveSubmixes.Add(MasterSubmix);
		}

		for (TWeakPtr<Audio::FMixerSubmix>& WeakSubmix : AudioState.ActiveSubmixes)
		{
			constexpr float ExpectedDuration = 30.f;
			WeakSubmix.Pin()->OnStartRecordingOutput(ExpectedDuration);
		}
	}
}

void UMovieGraphDefaultAudioRenderer::StopAudioRecording()
{
	AudioState.bIsRecordingAudio = false;

	for (TWeakPtr<Audio::FMixerSubmix>& WeakSubmix : AudioState.ActiveSubmixes)
	{
		if (const TSharedPtr<Audio::FMixerSubmix> Submix = WeakSubmix.Pin())
		{
			MoviePipeline::FAudioState::FAudioSegment& NewSegment = AudioState.FinishedSegments.AddDefaulted_GetRef();

			// Copy the data returned by the submix
			const Audio::AlignedFloatBuffer& AudioRecording = Submix->OnStopRecordingOutput(NewSegment.NumChannels, NewSegment.SampleRate);
			NewSegment.SegmentData = AudioRecording; 
		}
	}

	AudioState.ActiveSubmixes.Reset();
}

void UMovieGraphDefaultAudioRenderer::ProcessAudioTick()
{
	// Only supported on the new audio mixer (with the non-realtime device, windows only).
	Audio::FMixerDevice* MixerDevice = UE::MovieGraph::Audio::GetAudioMixerDeviceFromWorldContext(this);
	if (!MixerDevice)
	{
		return;
	}

	const Audio::IAudioMixerPlatformInterface* AudioMixerPlatform = MixerDevice->GetAudioMixerPlatform();
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

	const TArray<TObjectPtr<UMoviePipelineExecutorShot>>& ActiveShotList = GetOwningGraph()->GetActiveShotList();
	const int32 CurrentShotIndex = GetOwningGraph()->GetCurrentShotIndex();
	const TObjectPtr<UMoviePipelineExecutorShot> CurrentShot = ActiveShotList[CurrentShotIndex];

	// Start capturing any produced samples on the same frame we start submitting samples that will make it to disk.
	// This comes before we process samples for this frame (below).
	if ((CurrentShot->ShotInfo.State == EMovieRenderShotState::Rendering) && !AudioState.bIsRecordingAudio)
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

	if (CurrentShot->ShotInfo.State == EMovieRenderShotState::Rendering)
	{
		const UMovieGraphTimeStepBase* TimeStepInstance = GetOwningGraph()->GetTimeStepInstance();
		const UMovieGraphDataSourceBase* DataSourceInstance = GetOwningGraph()->GetDataSourceInstance();
		
		constexpr bool bIncludeCDOs = true;
		constexpr bool bExactMatch = true;
		const TObjectPtr<UMovieGraphEvaluatedConfig> EvaluatedGraph = TimeStepInstance->GetCalculatedTimeData().EvaluatedConfig;
		UMovieGraphGlobalOutputSettingNode* OutputSettingNode = EvaluatedGraph->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs, bExactMatch);

		const FFrameRate SourceFrameRate = DataSourceInstance->GetDisplayRate();
		const FFrameRate EffectiveFrameRate = UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(OutputSettingNode, SourceFrameRate);
		
		// The non-real time audio renderer desires even engine time steps. Unfortunately, when using temporal sampling
		// we don't have an even time-step. However, because it's non-real time, and we're accumulating the results into
		// a single frame anyways, we can bunch up the audio work and then process it when we've reached the end of a frame.
		bCanRenderAudio = TimeStepInstance->GetCalculatedTimeData().bIsLastTemporalSampleForFrame;
		
		// Process work that has been submitted from the game thread to the audio thread over the temporal samples of this frame.
		AudioDeltaTime = EffectiveFrameRate.AsInterval();
	}

	// Handle any game logic that changed Audio State.
	MixerDevice->Update(true);

	// Process work that has been submitted from the game thread to the audio thread over the temporal samples of this frame.
	if (bCanRenderAudio)
	{
		NRTPlatform->RenderAudio(AudioDeltaTime);
	}
}

void UMovieGraphDefaultAudioRenderer::SetupAudioRendering()
{
	// Ensure that we try to play audio at full volume, even if we're unfocused.
	AudioState.PrevUnfocusedAudioMultiplier = FApp::GetUnfocusedVolumeMultiplier();
	FApp::SetUnfocusedVolumeMultiplier(1.f);

	// Will be null if the NRT module wasn't loaded
	if (IConsoleVariable* AudioRenderEveryTickCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick")))
	{
		AudioState.PrevRenderEveryTickValue = AudioRenderEveryTickCvar->GetInt();
		
		// Override it to prevent it from automatically ticking, we'll control this below
		AudioRenderEveryTickCvar->SetWithCurrentPriority(0);
	}

	// Ensure that the NRT audio doesn't get muted
	if (IConsoleVariable* NeverMuteNRTAudioCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverMuteNonRealtimeAudioDevices")))
	{
		AudioState.PrevNeverMuteNRTAudioValue = NeverMuteNRTAudioCvar->GetInt();
		
		NeverMuteNRTAudioCvar->SetWithCurrentPriority(1);
	}
}

void UMovieGraphDefaultAudioRenderer::TeardownAudioRendering() const
{
	// Restore previous unfocused audio multiplier, to no longer force audio when unfocused
	FApp::SetUnfocusedVolumeMultiplier(AudioState.PrevUnfocusedAudioMultiplier);

	// Restore our cached CVar values
	
	// This will be null if the NRT wasn't used (module not loaded)
	if (IConsoleVariable* AudioRenderEveryTickCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.nrt.RenderEveryTick")))
	{
		AudioRenderEveryTickCvar->Set(AudioState.PrevRenderEveryTickValue, ECVF_SetByConstructor);
	}

	if (IConsoleVariable* NeverMuteNRTAudioCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.NeverMuteNonRealtimeAudioDevices")))
	{
		NeverMuteNRTAudioCvar->Set(AudioState.PrevNeverMuteNRTAudioValue, ECVF_SetByConstructor);
	}
}
