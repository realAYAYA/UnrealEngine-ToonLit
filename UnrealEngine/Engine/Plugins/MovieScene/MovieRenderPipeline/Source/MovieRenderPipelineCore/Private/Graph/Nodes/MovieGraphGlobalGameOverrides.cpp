// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphGlobalGameOverrides.h"

#include "Graph/MovieGraphPipeline.h"
#include "MoviePipelineUtils.h"
#include "Styling/AppStyle.h"

UMovieGraphGlobalGameOverridesNode::UMovieGraphGlobalGameOverridesNode()
	: GameModeOverride(AMoviePipelineGameMode::StaticClass())
	, ScalabilityQualityLevel(EMovieGraphScalabilityQualityLevel::Cinematic)
	, bDisableTextureStreaming(false)
	, bDisableLODs(false)
	, bDisableHLODs(false)
	, bFlushLevelStreaming(false)
	, bFlushAssetCompiler(false)
	, bFlushShaderCompiler(false)
	, bFlushGrassStreaming(false)
	, bFlushStreamingManagers(false)
	, VirtualTextureFeedbackFactor(1)
{
}

void UMovieGraphGlobalGameOverridesNode::BuildNewProcessCommandLineArgsImpl(TArray<FString>& InOutUnrealURLParams, TArray<FString>& InOutCommandLineArgs, TArray<FString>& InOutDeviceProfileCvars, TArray<FString>& InOutExecCmds) const
{
	// We don't provide the GameMode on the command line argument as we expect NewProcess to boot into an empty map and then it will
	// transition into the correct map which will then use the GameModeOverride setting.
	
	{
		Scalability::FQualityLevels QualityLevels;
		QualityLevels.SetFromSingleQualityLevel(static_cast<int32>(ScalabilityQualityLevel));

		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.ViewDistanceQuality={0}"), {QualityLevels.ViewDistanceQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.AntiAliasingQuality={0}"), {QualityLevels.AntiAliasingQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.ShadowQuality={0}"), {QualityLevels.ShadowQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.GlobalIlluminationQuality={0}"), {QualityLevels.GlobalIlluminationQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.ReflectionQuality={0}"), {QualityLevels.ReflectionQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.PostProcessQuality={0}"), {QualityLevels.PostProcessQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.TextureQuality={0}"), {QualityLevels.TextureQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.EffectsQuality={0}"), {QualityLevels.EffectsQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.FoliageQuality={0}"), {QualityLevels.FoliageQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.ShadingQuality={0}"), {QualityLevels.ShadingQuality}));
		InOutDeviceProfileCvars.Add(FString::Format(TEXT("sg.LandscapeQuality={0}"), {QualityLevels.LandscapeQuality}));
	}

	if (bDisableTextureStreaming)
	{
		InOutDeviceProfileCvars.Add(TEXT("r.TextureStreaming=0"));
	}

	if (bDisableLODs)
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

	if (bFlushStreamingManagers)
	{
		InOutDeviceProfileCvars.Add(TEXT("r.Streaming.SyncStatesWhenBlocking=1"));
	}

	// Like the extra cvars applied in ApplySettings(), the below are applied to allow MRQ to function correctly.

#if WITH_EDITOR
	{
		InOutDeviceProfileCvars.Add(TEXT("GeometryCache.Streamer.BlockTillFinishStreaming=1"));
		InOutDeviceProfileCvars.Add(TEXT("GeometryCache.Streamer.ShowNotification=0"));
	}
#endif

	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("a.URO.Enable=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.VolumetricRenderTarget=%d"), 1));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.VolumetricRenderTarget.Mode=%d"), 3));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("wp.Runtime.BlockOnSlowStreaming=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("p.Chaos.ImmPhys.MinStepTime=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("r.SkipRedundantTransformUpdate=%d"), 0));
	InOutDeviceProfileCvars.Add(FString::Printf(TEXT("p.ChaosCloth.UseTimeStepSmoothing=%d"), 0));
}

void UMovieGraphGlobalGameOverridesNode::ApplySettings(const bool bOverrideValues, UWorld* InWorld)
{
	// Apply the scalability settings
	if (bOverrideValues)
	{
		// Store the current scalability settings so we can revert back to them
		PreviousQualityLevels = Scalability::GetQualityLevels();

		// Set to the chosen scalability quality level
		Scalability::FQualityLevels QualityLevels;
		QualityLevels.SetFromSingleQualityLevel(static_cast<int32>(ScalabilityQualityLevel));

		// Apply
		Scalability::SetQualityLevels(QualityLevels);
	}
	else
	{
		// We re-apply old scalability settings at the end of the function during teardown
		// so that any values that are also specified in Scalability don't get overwritten
		// with the wrong values from the ones below restoring.
	}

	if (bDisableTextureStreaming)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousTextureStreaming, TEXT("r.TextureStreaming"), 0, bOverrideValues);
	}
	
	if (bDisableLODs)
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
		if (GEngine && InWorld)
		{
			GEngine->Exec(InWorld, TEXT("r.HLOD 0"));
		}
	}

	if (bFlushStreamingManagers)
	{
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousStreamingManagerSyncState, TEXT("r.Streaming.SyncStatesWhenBlocking"), 1, bOverrideValues);
	}

	// Set cvars that allow MRQ to run correctly. These are not exposed as properties on the node, but the chance of a
	// user needing to customize these (or be aware of them) is very low.
	{
#if WITH_EDITOR
		// To make sure the GeometryCache streamer doesn't skip frames and doesn't pop up notification during rendering
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousGeoCacheStreamerBlockTillFinish, TEXT("GeometryCache.Streamer.BlockTillFinishStreaming"), 1, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousGeoCacheStreamerShowNotification, TEXT("GeometryCache.Streamer.ShowNotification"), 0, bOverrideValues);
#endif

		// Disable systems that try to preserve performance in runtime games.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousAnimationUROEnabled, TEXT("a.URO.Enable"), 0, bOverrideValues);

		// To make sure that the skylight is always valid and consistent across capture sessions, we enforce a full capture each frame, accepting a small GPU cost.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousSkyLightRealTimeReflectionCaptureTimeSlice, TEXT("r.SkyLight.RealTimeReflectionCapture.TimeSlice"), 0, bOverrideValues);

		// Cloud are rendered using high quality volumetric render target mode 3: per pixel tracing and composition on screen, while supporting cloud on translucent.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousVolumetricRenderTarget, TEXT("r.VolumetricRenderTarget"), 1, bOverrideValues);
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousVolumetricRenderTargetMode, TEXT("r.VolumetricRenderTarget.Mode"), 3, bOverrideValues);

		// To make sure that the world partition streaming doesn't end up in critical streaming performances and stops streaming low priority cells.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousIgnoreStreamingPerformance, TEXT("wp.Runtime.BlockOnSlowStreaming"), 0, bOverrideValues);

		// Remove any minimum delta time requirements from Chaos Physics to ensure accuracy at high Temporal Sample counts
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_FLOAT(PreviousChaosImmPhysicsMinStepTime, TEXT("p.Chaos.ImmPhys.MinStepTime"), 0, bOverrideValues);

		// MRQ's 0 -> 0.99 -> 0 evaluation for motion blur emulation can occasionally cause it to be detected as a redundant update and thus never updated
		// which causes objects to render in the wrong position on the first frame (and without motion blur). This disables an optimization that detects
		// the redundant updates so the update will get sent through anyways even though it thinks it's a duplicate (but it's not).
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousSkipRedundantTransformUpdate, TEXT("r.SkipRedundantTransformUpdate"), 0, bOverrideValues);

		// Cloth's time step smoothing messes up the change in number of simulation substeps that fixes the cloth simulation behavior when using Temporal Samples.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT(PreviousChaosClothUseTimeStepSmoothing, TEXT("p.ChaosCloth.UseTimeStepSmoothing"), 0, bOverrideValues);

		// Water skips water info texture when the world's game viewport rendering is disabled so we need to prevent this from happening.
		MOVIEPIPELINE_STORE_AND_OVERRIDE_CVAR_INT_IF_EXIST(PreviousSkipWaterInfoTextureRenderWhenWorldRenderingDisabled, TEXT("r.Water.SkipWaterInfoTextureRenderWhenWorldRenderingDisabled"), 0, bOverrideValues);

	}
	
	// Must come after the above cvars so that if one of those cvars is also specified by the scalability level, then we
	// restore to the value in the original scalability level, not the value we cached in the scalability level (if applied).
	if (!bOverrideValues)
	{
		Scalability::SetQualityLevels(PreviousQualityLevels);
	}
}

TSubclassOf<AGameModeBase> UMovieGraphGlobalGameOverridesNode::GetGameModeOverride(const UMoviePipelineExecutorJob* InJob)
{
	UMovieGraphConfig* GraphConfig = InJob->GetGraphPreset();
	if (InJob->IsUsingGraphConfiguration() && GraphConfig)
	{
		// There is most likely no pipeline to fetch a traversal context from at this point, so a temporary context
		// is generated instead (which will not be fully filled in, but should be enough to generate a correctly flattened
		// Globals branch).
		FMovieGraphTraversalContext TraversalContext;
		TraversalContext.Job = const_cast<UMoviePipelineExecutorJob*>(InJob);
		
		FString OutTraversalError;
		if (const UMovieGraphEvaluatedConfig* EvaluatedGraph = GraphConfig->CreateFlattenedGraph(TraversalContext, OutTraversalError))
		{
			// Note that the CDO is not fetched here. Users need to explicitly include the Global Game Overrides node and specify the game mode override.
			constexpr bool bIncludeCDOs = false;
			constexpr bool bExactMatch = true;
			UMovieGraphGlobalGameOverridesNode* GameOverridesNode =
				EvaluatedGraph->GetSettingForBranch<UMovieGraphGlobalGameOverridesNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);

			return GameOverridesNode ? GameOverridesNode->GameModeOverride : nullptr;
		}
	}
	else
	{
		TArray<UMoviePipelineSetting*> AllSettings = InJob->GetConfiguration()->GetAllSettings();
		UMoviePipelineSetting** GameOverridesPtr = AllSettings.FindByPredicate([](const UMoviePipelineSetting* InSetting)
		{
			return InSetting->GetClass() == UMoviePipelineGameOverrideSetting::StaticClass();
		});
		
		if (GameOverridesPtr)
		{
			if (UMoviePipelineSetting* Setting = *GameOverridesPtr)
			{
				return CastChecked<UMoviePipelineGameOverrideSetting>(Setting)->GameModeOverride;
			}
		}
	}

	return nullptr;
}

#if WITH_EDITOR
FText UMovieGraphGlobalGameOverridesNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText GlobalGameOverridesNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_GlobalGameOverrides", "Global Game Overrides");
	return GlobalGameOverridesNodeName;
}

FText UMovieGraphGlobalGameOverridesNode::GetMenuCategory() const
{
	static const FText NodeCategory_Globals = NSLOCTEXT("MovieGraphNodes", "NodeCategory_Globals", "Globals");
	return NodeCategory_Globals;
}

FLinearColor UMovieGraphGlobalGameOverridesNode::GetNodeTitleColor() const
{
	static const FLinearColor GlobalGameOverridesNodeColor = FLinearColor(0.549f, 0.f, 0.250f);
	return GlobalGameOverridesNodeColor;
}

FSlateIcon UMovieGraphGlobalGameOverridesNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon GlobalGameOverridesIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Launcher.TabIcon");

	OutColor = FLinearColor::White;
	return GlobalGameOverridesIcon;
}
#endif // WITH_EDITOR