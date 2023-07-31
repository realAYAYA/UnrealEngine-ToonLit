// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineGameOverrideSetting.h"
#include "Scalability.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MoviePipelineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineGameOverrideSetting)

void UMoviePipelineGameOverrideSetting::SetupForPipelineImpl(UMoviePipeline* InPipeline)
{
	// Store the cvar values and apply the ones from this setting
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Applying Game Override quality settings and cvars."));
	ApplyCVarSettings(true);
}

void UMoviePipelineGameOverrideSetting::TeardownForPipelineImpl(UMoviePipeline* InPipeline)
{
	// Restore the previous cvar values the user had
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Restoring Game Override quality settings and cvars."));
	ApplyCVarSettings(false);
}

void UMoviePipelineGameOverrideSetting::ApplyCVarSettings(const bool bOverrideValues)
{
	if (bCinematicQualitySettings)
	{
		if (bOverrideValues)
		{
			// Store their previous Scalability settings so we can revert back to them
			PreviousQualityLevels = Scalability::GetQualityLevels();

			// Create a copy and override to the maximum level for each Scalability category
			Scalability::FQualityLevels QualityLevels = PreviousQualityLevels;
			QualityLevels.SetFromSingleQualityLevelRelativeToMax(0);

			// Apply
			Scalability::SetQualityLevels(QualityLevels);
		}
		else
		{
			Scalability::SetQualityLevels(PreviousQualityLevels);
		}
	}

	switch (TextureStreaming)
	{
	case EMoviePipelineTextureStreamingMethod::FullyLoad:
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousFramesForFullUpdate, TEXT("r.Streaming.FramesForFullUpdate"), 0, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousFullyLoadUsedTextures, TEXT("r.Streaming.FullyLoadUsedTextures"), 1, bOverrideValues);
		break;
	case EMoviePipelineTextureStreamingMethod::Disabled:
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousTextureStreaming, TEXT("r.TextureStreaming"), 0, bOverrideValues);
		break;
	default:
		// We don't change their texture streaming settings.
		break;
	}

	if (bUseLODZero)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousForceLOD, TEXT("r.ForceLOD"), 0, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousSkeletalMeshBias, TEXT("r.SkeletalMeshLODBias"), -10, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousParticleLODBias, TEXT("r.ParticleLODBias"), -10, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousFoliageDitheredLOD, TEXT("foliage.DitheredLOD"), 0, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousFoliageForceLOD, TEXT("foliage.ForceLOD"), 0, bOverrideValues);
	}

	if (bDisableHLODs)
	{
		// It's a command and not an integer cvar (despite taking 1/0), so we can't cache it 
		if(GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("r.HLOD 0"));
		}
	}

	if (bUseHighQualityShadows)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousShadowDistanceScale, TEXT("r.Shadow.DistanceScale"), ShadowDistanceScale, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousShadowQuality, TEXT("r.ShadowQuality"), 5, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_FLOAT(PreviousShadowRadiusThreshold, TEXT("r.Shadow.RadiusThreshold"), ShadowRadiusThreshold, bOverrideValues);
	}

	if (bOverrideViewDistanceScale)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousViewDistanceScale, TEXT("r.ViewDistanceScale"), ViewDistanceScale, bOverrideValues);
	}

	if (bDisableGPUTimeout)
	{
		// This CVAR only exists if the D3D12RHI module is loaded
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousGPUTimeout, TEXT("r.D3D12.GPUTimeout"), 0, bOverrideValues);
	}

	if (bFlushStreamingManagers)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousStreamingManagerSyncState, TEXT("r.Streaming.SyncStatesWhenBlocking"), 1, bOverrideValues);
	}
	
#if WITH_EDITOR
	// To make sure the GeometryCache streamer doesn't skip frames and doesn't pop up notification during rendering
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousGeoCacheStreamerBlockTillFinish, TEXT("GeometryCache.Streamer.BlockTillFinishStreaming"), 1, bOverrideValues);
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousGeoCacheStreamerShowNotification, TEXT("GeometryCache.Streamer.ShowNotification"), 0, bOverrideValues);
#endif

	{
		// Disable systems that try to preserve performance in runtime games.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousAnimationUROEnabled, TEXT("a.URO.Enable"), 0, bOverrideValues);
	}

	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousNeverMuteNonRealtimeAudio, TEXT("au.NeverMuteNonRealtimeAudioDevices"), 1, bOverrideValues);

	// To make sure that the skylight is always valid and consistent accross capture sessions, we enforce a full capture each frame, accepting a small GPU cost.
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousSkyLightRealTimeReflectionCaptureTimeSlice, TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice"), 0, bOverrideValues);

	// To make sure that the skylight is always valid and consistent accross capture sessions, we enforce a full capture each frame, accepting a small GPU cost.
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousVolumetricRenderTarget, TEXT("r.VolumetricRenderTarget"), 0, bOverrideValues);

	// To make sure that the world partition streaming doesn't end up in critical streaming performances and stops streaming low priority cells.
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousIgnoreStreamingPerformance, TEXT("wp.Runtime.BlockOnSlowStreaming"), 0, bOverrideValues);

	// Remove any minimum delta time requirements from Chaos Physics to ensure accuracy at high Temporal Sample counts
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_FLOAT(PreviousChaosImmPhysicsMinStepTime, TEXT("p.Chaos.ImmPhys.MinStepTime"), 0, bOverrideValues);

	// MRQ's 0 -> 0.99 -> 0 evaluation for motion blur emulation can occasionally cause it to be detected as a redundant update and thus never updated
	// which causes objects to render in the wrong position on the first frame (and without motion blur). This disables an optimization that detects
	// the redundant updates so the update will get sent through anyways even though it thinks it's a duplicate (but it's not).
	MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousSkipRedundantTransformUpdate, TEXT("r.SkipRedundantTransformUpdate"), 0, bOverrideValues);
}

void UMoviePipelineGameOverrideSetting::BuildNewProcessCommandLineArgsImpl(TArray<FString>& InOutUnrealURLParams, TArray<FString>& InOutCommandLineArgs, TArray<FString>& InOutDeviceProfileCvars, TArray<FString>& InOutExecCmds) const
{
	if (!IsEnabled())
	{
		return;
	}

	// We don't provide the GameMode on the command line argument as we expect NewProcess to boot into an empty map and then it will
	// transition into the correct map which will then use the GameModeOverride setting.
	if (bCinematicQualitySettings)
	{
		InOutDeviceProfileCvars.Add(TEXT("sg.ViewDistanceQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.AntiAliasingQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.ShadowQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.GlobalIlluminationQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.ReflectionQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.PostProcessQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.TextureQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.EffectsQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.FoliageQuality=4"));
		InOutDeviceProfileCvars.Add(TEXT("sg.ShadingQuality=4"));
	}

	switch (TextureStreaming)
	{
	case EMoviePipelineTextureStreamingMethod::FullyLoad:
		InOutDeviceProfileCvars.Add(TEXT("r.Streaming.FramesForFullUpdate=0"));
		InOutDeviceProfileCvars.Add(TEXT("r.Streaming.FullyLoadUsedTextures=1"));
		break;
	case EMoviePipelineTextureStreamingMethod::Disabled:
		InOutDeviceProfileCvars.Add(TEXT("r.TextureStreaming=0"));
		break;
	default:
		// We don't change their texture streaming settings.
		break;
	}

	if (bUseLODZero)
	{
		InOutDeviceProfileCvars.Add(TEXT("r.ForceLOD=0"));
		InOutDeviceProfileCvars.Add(TEXT("r.SkeletalMeshLODBias=-10"));
		InOutDeviceProfileCvars.Add(TEXT("r.ParticleLODBias=-10"));
		InOutDeviceProfileCvars.Add(TEXT("foliage.DitheredLOD=0"));
		InOutDeviceProfileCvars.Add(TEXT("foliage.ForceLOD=0"));
	}

	if (bDisableHLODs)
	{
		// It's a command and not an integer cvar (despite taking 1/0)
		InOutExecCmds.Add(TEXT("r.HLOD 0"));
	}

	if (bUseHighQualityShadows)
	{
		InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.Shadow.DistanceScale=%d"), ShadowDistanceScale));
		InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.Shadow.RadiusThreshold=%f"), ShadowRadiusThreshold));
		InOutDeviceProfileCvars.Add(TEXT("r.ShadowQuality=5"));
	}

	if (bOverrideViewDistanceScale)
	{
		InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.ViewDistanceScale=%d"), ViewDistanceScale));
	}

	if (bDisableGPUTimeout)
	{
		InOutDeviceProfileCvars.Add(TEXT("r.D3D12.GPUTimeout=0"));
	}

	if (bFlushStreamingManagers)
	{
		InOutDeviceProfileCvars.Add(TEXT("r.Streaming.SyncStatesWhenBlocking=1"));
	}
	
#if WITH_EDITOR
	{
		InOutDeviceProfileCvars.Add(TEXT("GeometryCache.Streamer.BlockTillFinishStreaming=1"));
		InOutDeviceProfileCvars.Add(TEXT("GeometryCache.Streamer.ShowNotification=0"));
	}
#endif

	{
		InOutDeviceProfileCvars.Add(FString::Printf(TEXT("a.URO.Enable=%d"), 0));
	}

	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("au.NeverMuteNonRealtimeAudioDevices=%d"), 1));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.VolumetricRenderTarget=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("wp.Runtime.BlockOnSlowStreaming=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("p.Chaos.ImmPhys.MinStepTime=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.SkipRedundantTransformUpdate=%d"), 0));
}
