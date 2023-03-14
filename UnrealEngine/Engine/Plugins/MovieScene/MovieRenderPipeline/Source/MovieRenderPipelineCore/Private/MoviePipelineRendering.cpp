// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipeline.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineOutputBase.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineShotConfig.h"
#include "MoviePipelineRenderPass.h"
#include "MoviePipelineOutputBuilder.h"
#include "RenderingThread.h"
#include "MoviePipelineDebugSettings.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineConfigBase.h"
#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "Math/Halton.h"
#include "ImageWriteTask.h"
#include "ImageWriteQueue.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MoviePipelineHighResSetting.h"
#include "Modules/ModuleManager.h"
#include "MoviePipelineCameraSetting.h"
#include "Engine/GameViewportClient.h"
#include "LegacyScreenPercentageDriver.h"
#include "RenderCaptureInterface.h"
#include "MoviePipelineGameOverrideSetting.h"

// For flushing async systems
#include "RendererInterface.h"
#include "LandscapeProxy.h"
#include "EngineModule.h"
#include "DistanceFieldAtlas.h"
#include "MeshCardRepresentation.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ContentStreaming.h"


#define LOCTEXT_NAMESPACE "MoviePipeline"

void UMoviePipeline::SetupRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot)
{
	/*
	* To support tiled rendering we take the final effective resolution and divide
	* it by the number of tiles to find the resolution of each render target. To 
	* handle non-evenly divisible numbers/resolutions we may oversize the targets
	* by a few pixels and then take the center of the resulting image when interlacing
	* to produce the final image at the right resolution. For example:
	*
	* 1920x1080 in 7x7 tiles gives you 274.29x154.29. We ceiling this to set the resolution
	* of the render pass to 275x155 which will give us a final interleaved image size of
	* 1925x1085. To ensure that the image matches a non-scaled one we take the center out.
	* LeftOffset = floor((1925-1920)/2) = 2
	* RightOffset = (1925-1920-LeftOffset)
	*/
	UMoviePipelineAntiAliasingSetting* AccumulationSettings = FindOrAddSettingForShot<UMoviePipelineAntiAliasingSetting>(InShot);
	UMoviePipelineHighResSetting* HighResSettings = FindOrAddSettingForShot<UMoviePipelineHighResSetting>(InShot);
	UMoviePipelineOutputSetting* OutputSettings = GetPipelineMasterConfig()->FindSetting<UMoviePipelineOutputSetting>();
	check(OutputSettings);


	FIntPoint BackbufferTileCount = FIntPoint(HighResSettings->TileCount, HighResSettings->TileCount);
	FIntPoint OutputResolution = UMoviePipelineBlueprintLibrary::GetEffectiveOutputResolution(GetPipelineMasterConfig(), InShot);

	// Figure out how big each sub-region (tile) is.
	FIntPoint BackbufferResolution = FIntPoint(
		FMath::CeilToInt((float)OutputResolution.X / (float)HighResSettings->TileCount),
		FMath::CeilToInt((float)OutputResolution.Y / (float)HighResSettings->TileCount));

	// Then increase each sub-region by the overlap amount.
	BackbufferResolution = HighResSettings->CalculatePaddedBackbufferSize(BackbufferResolution);

	{
		int32 MaxResolution = GetMax2DTextureDimension();
		if (BackbufferResolution.X > MaxResolution || BackbufferResolution.Y > MaxResolution)
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Resolution %dx%d exceeds maximum allowed by GPU (%dx%d). Consider using the HighRes setting and increasing the tile count."), BackbufferResolution.X, BackbufferResolution.Y, MaxResolution, MaxResolution);
			Shutdown(true);
			return;
		}
	}

	// Note how many tiles we wish to render with.
	BackbufferTileCount = FIntPoint(HighResSettings->TileCount, HighResSettings->TileCount);

	const ERHIFeatureLevel::Type FeatureLevel = GetWorld()->FeatureLevel;

	// Initialize our render pass. This is a copy of the settings to make this less coupled to the Settings UI.
	const MoviePipeline::FMoviePipelineRenderPassInitSettings RenderPassInitSettings(FeatureLevel, BackbufferResolution, BackbufferTileCount);

	// Code expects at least a 1x1 tile.
	ensure(RenderPassInitSettings.TileCount.X > 0 && RenderPassInitSettings.TileCount.Y > 0);

	// Initialize out output passes
	int32 NumOutputPasses = 0;
	for (UMoviePipelineRenderPass* RenderPass : FindSettingsForShot<UMoviePipelineRenderPass>(InShot))
	{
		RenderPass->Setup(RenderPassInitSettings);
		NumOutputPasses++;
	}

	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Finished setting up rendering for shot. Shot has %d Passes. Total resolution: (%dx%d) Individual tile resolution: (%dx%d). Tile count: (%dx%d)"), NumOutputPasses, OutputResolution.X, OutputResolution.Y, BackbufferResolution.X, BackbufferResolution.Y, BackbufferTileCount.X, BackbufferTileCount.Y);
}

void UMoviePipeline::TeardownRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot)
{
	for (UMoviePipelineRenderPass* RenderPass : FindSettingsForShot<UMoviePipelineRenderPass>(InShot))
	{
		RenderPass->Teardown();
	}

	if (OutputBuilder->GetNumOutstandingFrames() > 1)
	{
		// The intention behind this warning is to catch when you've created a render pass that doesn't submit as many render passes as you expect. Unfortunately,
		// it also catches the fact that temporal sampling tends to render an extra frame. When we are submitting frames we only check if the actual evaluation point
		// surpasses the upper bound, at which point we don't submit anything more. We could check a whole frame in advance and never submit any temporal samples for
		// the extra frame, but then this would not work with slow-motion. Instead, we will just comprimise here and only warn if there's multiple frames that are missing.
		// This is going to be true if you have set up your rendering wrong (and are rendering more than one frame) so it will catch enough of the cases to be worth it.
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Not all frames were fully submitted by the time rendering was torn down! Frames will be missing from output!"));
	}
}

void UMoviePipeline::RenderFrame()
{
	// Flush built in systems before we render anything. This maximizes the likelihood that the data is prepared for when
	// the render thread uses it.
	FlushAsyncEngineSystems();

	// Send any output frames that have been completed since the last render.
	ProcessOutstandingFinishedFrames();

	FMoviePipelineCameraCutInfo& CurrentCameraCut = ActiveShotList[CurrentShotIndex]->ShotInfo;
	APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController();


	// If we don't want to render this frame, then we will skip processing - engine warmup frames,
	// render every nTh frame, etc. In other cases, we may wish to render the frame but discard the
	// result and not send it to the output merger (motion blur frames, gpu feedback loops, etc.)
	if (CachedOutputState.bSkipRendering)
	{
		return;
	}
	
	// Hide the progress widget before we render anything. This allows widget captures to not include the progress bar.
	SetProgressWidgetVisible(false);

	// To produce a frame from the movie pipeline we may render many frames over a period of time, additively collecting the results
	// together before submitting it for writing on the last result - this is referred to as an "output frame". The 1 (or more) samples
	// that make up each output frame are referred to as "sample frames". Within each sample frame, we may need to render the scene many
	// times. In order to support ultra-high-resolution rendering (>16k) movie pipelines support building an output frame out of 'tiles'. 
	// Each tile renders the entire viewport with a small offset which causes different samples to be picked for each final pixel. These
	// 'tiles' are then interleaved together (on the CPU) to produce a higher resolution result. For each tile, we can render a number
	// of jitters that get added together to produce a higher quality single frame. This is useful for cases where you may not want any 
	// motion (such as trees fluttering in the wind) but you do want high quality anti-aliasing on the edges of the pixels. Finally,
	// the outermost loop (which is not represented here) is accumulation over time which happens over multiple engine ticks.
	// 
	// In short, for each output frame, for each accumulation frame, for each tile X/Y, for each jitter, we render a pass. This setup is
	// designed to maximize the likely hood of deterministic rendering and that different passes line up with each other.
	UMoviePipelineAntiAliasingSetting* AntiAliasingSettings = FindOrAddSettingForShot<UMoviePipelineAntiAliasingSetting>(ActiveShotList[CurrentShotIndex]);
	UMoviePipelineCameraSetting* CameraSettings = FindOrAddSettingForShot<UMoviePipelineCameraSetting>(ActiveShotList[CurrentShotIndex]);
	UMoviePipelineHighResSetting* HighResSettings = FindOrAddSettingForShot<UMoviePipelineHighResSetting>(ActiveShotList[CurrentShotIndex]);
	UMoviePipelineOutputSetting* OutputSettings = GetPipelineMasterConfig()->FindSetting<UMoviePipelineOutputSetting>();
	UMoviePipelineDebugSettings* DebugSettings = FindOrAddSettingForShot<UMoviePipelineDebugSettings>(ActiveShotList[CurrentShotIndex]);
	
	// Color settings are optional, so we don't need to do any assertion checks.
	UMoviePipelineColorSetting* ColorSettings = GetPipelineMasterConfig()->FindSetting<UMoviePipelineColorSetting>();
	check(AntiAliasingSettings);
	check(CameraSettings);
	check(HighResSettings);
	check(OutputSettings);

	FIntPoint TileCount = FIntPoint(HighResSettings->TileCount, HighResSettings->TileCount);
	FIntPoint OriginalTileCount = TileCount;
	FIntPoint OutputResolution = UMoviePipelineBlueprintLibrary::GetEffectiveOutputResolution(GetPipelineMasterConfig(), ActiveShotList[CurrentShotIndex]);

	int32 NumSpatialSamples = AntiAliasingSettings->SpatialSampleCount;
	int32 NumTemporalSamples = AntiAliasingSettings->TemporalSampleCount;
	if (!ensureAlways(TileCount.X > 0 && TileCount.Y > 0 && NumSpatialSamples > 0 && NumTemporalSamples > 0))
	{
		return;
	}
	
	{
		// Sidecar Cameras get updated below after rendering, they're still separate for backwards compat reasons
		FrameInfo.PrevViewLocation = FrameInfo.CurrViewLocation;
		FrameInfo.PrevViewRotation = FrameInfo.CurrViewRotation;

		// Update the Sidecar Cameras
		FrameInfo.PrevSidecarViewLocations = FrameInfo.CurrSidecarViewLocations;
		FrameInfo.PrevSidecarViewRotations = FrameInfo.CurrSidecarViewRotations;

		// Update our current view location
		LocalPlayerController->GetPlayerViewPoint(FrameInfo.CurrViewLocation, FrameInfo.CurrViewRotation);
		GetSidecarCameraViewPoints(ActiveShotList[CurrentShotIndex], FrameInfo.CurrSidecarViewLocations, FrameInfo.CurrSidecarViewRotations);
	}

	bool bWriteAllSamples = DebugSettings ? DebugSettings->bWriteAllSamples : false;

	// Add appropriate metadata here that is shared by all passes.
	{
		// Add hardware stats such as total memory, cpu vendor, etc.
		FString ResolvedOutputDirectory;
		TMap<FString, FString> FormatOverrides;
		FMoviePipelineFormatArgs FinalFormatArgs;

		// We really only need the output disk path for disk size info, but we'll try to resolve as much as possible anyways
		ResolveFilenameFormatArguments(OutputSettings->OutputDirectory.Path, FormatOverrides, ResolvedOutputDirectory, FinalFormatArgs);
		// Strip .{ext}
		ResolvedOutputDirectory.LeftChopInline(6);

		UE::MoviePipeline::GetHardwareUsageMetadata(CachedOutputState.FileMetadata, ResolvedOutputDirectory);


		// We'll leave these in for legacy, when this tracks the 'Main' camera (of the player), render passes that support
		// multiple cameras will have to write each camera name into their metadata.
		UE::MoviePipeline::GetMetadataFromCameraLocRot(TEXT("camera"), TEXT(""), FrameInfo.CurrViewLocation, FrameInfo.CurrViewRotation, FrameInfo.PrevViewLocation, FrameInfo.PrevViewRotation, CachedOutputState.FileMetadata);

		// This is still global regardless, individual cameras don't get their own motion blur amount because the engine tick is tied to it.
		CachedOutputState.FileMetadata.Add(TEXT("unreal/camera/shutterAngle"), FString::SanitizeFloat(CachedOutputState.TimeData.MotionBlurFraction * 360.0f));
	}

	if (CurrentCameraCut.State != EMovieRenderShotState::Rendering)
	{
		// We can optimize some of the settings for 'special' frames we may be rendering, ie: we render once for motion vectors, but
		// we don't need that per-tile so we can set the tile count to 1, and spatial sample count to 1 for that particular frame.
		{
			// Spatial Samples aren't needed when not producing frames (caveat: Render Warmup Frame, handled below)
			NumSpatialSamples = 1;
		}
	}

	int32 NumWarmupSamples = 0;
	if (CurrentCameraCut.State == EMovieRenderShotState::WarmingUp)
	{
		// We sometimes render the actual warmup frames, and in this case we only want to render one warmup sample each frame,
		// and save any RenderWarmUp frames until the last one.
		if (CurrentCameraCut.NumEngineWarmUpFramesRemaining > 0)
		{
			NumWarmupSamples = 1;
		}
		else
		{
			NumWarmupSamples = AntiAliasingSettings->RenderWarmUpCount;
		}
	}

	TArray<UMoviePipelineRenderPass*> InputBuffers = FindSettingsForShot<UMoviePipelineRenderPass>(ActiveShotList[CurrentShotIndex]);

	// If this is the first sample for a new frame, we want to notify the output builder that it should expect data to accumulate for this frame.
	if (CachedOutputState.IsFirstTemporalSample())
	{
		// This happens before any data is queued for this frame.
		FMoviePipelineMergerOutputFrame& OutputFrame = OutputBuilder->QueueOutputFrame_GameThread(CachedOutputState);

		// Now we need to go through all passes and get any identifiers from them of what this output frame should expect.
		for (UMoviePipelineRenderPass* RenderPass : InputBuffers)
		{
			RenderPass->GatherOutputPasses(OutputFrame.ExpectedRenderPasses);
		}

		FRenderTimeStatistics& TimeStats = RenderTimeFrameStatistics.FindOrAdd(CachedOutputState.OutputFrameNumber);
		TimeStats.StartTime = FDateTime::UtcNow();
	}

	// Support for RenderDoc captures of just the MRQ work
#if WITH_EDITOR && !UE_BUILD_SHIPPING
	TUniquePtr<RenderCaptureInterface::FScopedCapture> ScopedGPUCapture;
	if (CachedOutputState.bCaptureRendering)
	{
		ScopedGPUCapture = MakeUnique<RenderCaptureInterface::FScopedCapture>(true, *FString::Printf(TEXT("MRQ Frame: %d"), CachedOutputState.SourceFrameNumber));
	}
#endif

	for (int32 TileY = 0; TileY < TileCount.Y; TileY++)
	{
		for (int32 TileX = 0; TileX < TileCount.X; TileX++)
		{
			int NumSamplesToRender = (CurrentCameraCut.State == EMovieRenderShotState::WarmingUp) ? NumWarmupSamples : NumSpatialSamples;

			// Now we want to render a user-configured number of spatial jitters to come up with the final output for this tile. 
			for (int32 RenderSampleIndex = 0; RenderSampleIndex < NumSamplesToRender; RenderSampleIndex++)
			{
				int32 SpatialSampleIndex = (CurrentCameraCut.State == EMovieRenderShotState::WarmingUp) ? 0 : RenderSampleIndex;

				if (CurrentCameraCut.State == EMovieRenderShotState::Rendering)
				{
					// Count this as a sample rendered for the current work.
					CurrentCameraCut.WorkMetrics.OutputSubSampleIndex++;
				}

				// We freeze views for all spatial samples except the last so that nothing in the FSceneView tries to update.
				// Our spatial samples need to be different positional takes on the same world, thus pausing it.
				const bool bAllowPause = CurrentCameraCut.State == EMovieRenderShotState::Rendering;
				const bool bIsLastTile = FIntPoint(TileX, TileY) == FIntPoint(TileCount.X - 1, TileCount.Y - 1);
				const bool bWorldIsPaused = bAllowPause && !(bIsLastTile && (RenderSampleIndex == (NumSamplesToRender - 1)));

				// We need to pass camera cut flag on the first sample that gets rendered for a given camera cut. If you don't have any render
				// warm up frames, we do this on the first render sample because we no longer render the motion blur frame (just evaluate it).
				const bool bCameraCut = CachedOutputState.ShotSamplesRendered == 0;
				CachedOutputState.ShotSamplesRendered++;

				EAntiAliasingMethod AntiAliasingMethod = UE::MovieRenderPipeline::GetEffectiveAntiAliasingMethod(AntiAliasingSettings);

				// Now to check if we have to force it off (at which point we warn the user).
				bool bMultipleTiles = (TileCount.X > 1) || (TileCount.Y > 1);
				if (bMultipleTiles && IsTemporalAccumulationBasedMethod(AntiAliasingMethod))
				{
					// Temporal Anti-Aliasing isn't supported when using tiled rendering because it relies on having history, and
					// the tiles use the previous tile as the history which is incorrect. 
					UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Temporal AntiAliasing is not supported when using tiling!"));
					AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
				}


				// We Abs this so that negative numbers on the first frame of a cut (warm ups) don't go into Halton which will assign 0.
				int32 ClampedFrameNumber = FMath::Max(0, CachedOutputState.OutputFrameNumber);
				int32 ClampedTemporalSampleIndex = FMath::Max(0, CachedOutputState.TemporalSampleIndex);
				int32 FrameIndex = FMath::Abs((ClampedFrameNumber * (NumTemporalSamples * NumSpatialSamples)) + (ClampedTemporalSampleIndex * NumSpatialSamples) + SpatialSampleIndex);

				// if we are warming up, we will just use the RenderSampleIndex as the FrameIndex so the samples jump around a bit.
				if (CurrentCameraCut.State == EMovieRenderShotState::WarmingUp)
				{
					FrameIndex = RenderSampleIndex;
				}
				

				// Repeat the Halton Offset equally on each output frame so non-moving objects don't have any chance to crawl between frames.
				int32 HaltonIndex = (FrameIndex % (NumSpatialSamples*NumTemporalSamples)) + 1;
				float HaltonOffsetX = Halton(HaltonIndex, 2);
				float HaltonOffsetY = Halton(HaltonIndex, 3);

				// only allow a spatial jitter if we have more than one sample
				bool bAllowSpatialJitter = !(NumSpatialSamples == 1 && NumTemporalSamples == 1);

				UE_LOG(LogTemp, VeryVerbose, TEXT("FrameIndex: %d HaltonIndex: %d Offset: (%f,%f)"), FrameIndex, HaltonIndex, HaltonOffsetX, HaltonOffsetY);
				float SpatialShiftX = 0.0f;
				float SpatialShiftY = 0.0f;

				if (bAllowSpatialJitter)
				{
					static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAAFilterSize"));
					float FilterSize = CVar->GetFloat();

					// Scale distribution to set non-unit variance
					// Variance = Sigma^2
					float Sigma = 0.47f * FilterSize;

					// Window to [-0.5, 0.5] output
					// Without windowing we could generate samples far away on the infinite tails.
					float OutWindow = 0.5f;
					float InWindow = FMath::Exp(-0.5 * FMath::Square(OutWindow / Sigma));

					// Box-Muller transform
					float Theta = 2.0f * PI * HaltonOffsetY;
					float r = Sigma * FMath::Sqrt(-2.0f * FMath::Loge((1.0f - HaltonOffsetX) * InWindow + HaltonOffsetX));

					SpatialShiftX = r * FMath::Cos(Theta);
					SpatialShiftY = r * FMath::Sin(Theta);
				}

				// We take all of the information needed to render a single sample and package it into a struct.
				FMoviePipelineRenderPassMetrics SampleState;
				SampleState.FrameIndex = FrameIndex;
				SampleState.bWorldIsPaused = bWorldIsPaused;
				SampleState.bCameraCut = bCameraCut;
				SampleState.AntiAliasingMethod = AntiAliasingMethod;
				SampleState.SceneCaptureSource = (ColorSettings && ColorSettings->bDisableToneCurve) ? ESceneCaptureSource::SCS_FinalColorHDR : ESceneCaptureSource::SCS_FinalToneCurveHDR;
				SampleState.OutputState = CachedOutputState;
				SampleState.OutputState.CameraIndex = 0; // Initialize to a sane default for non multi-cam passes.
				SampleState.TileIndexes = FIntPoint(TileX, TileY);
				SampleState.TileCounts = TileCount;
				SampleState.OriginalTileCounts = OriginalTileCount;
				SampleState.SpatialShiftX = SpatialShiftX;
				SampleState.SpatialShiftY = SpatialShiftY;
				SampleState.bDiscardResult = CachedOutputState.bDiscardRenderResult;
				SampleState.SpatialSampleIndex = SpatialSampleIndex;
				SampleState.SpatialSampleCount = NumSpatialSamples;
				SampleState.TemporalSampleIndex = CachedOutputState.TemporalSampleIndex;
				SampleState.TemporalSampleCount = AntiAliasingSettings->TemporalSampleCount;
				SampleState.AccumulationGamma = AntiAliasingSettings->AccumulationGamma;
				SampleState.FrameInfo = FrameInfo;
				SampleState.bWriteSampleToDisk = bWriteAllSamples;
				SampleState.TextureSharpnessBias = HighResSettings->TextureSharpnessBias;
				SampleState.OCIOConfiguration = ColorSettings ? &ColorSettings->OCIOConfiguration : nullptr;
				SampleState.GlobalScreenPercentageFraction = FLegacyScreenPercentageDriver::GetCVarResolutionFraction();
				SampleState.OverscanPercentage = FMath::Clamp(CameraSettings->OverscanPercentage, 0.0f, 1.0f);

				// Render each output pass
				FMoviePipelineRenderPassMetrics SampleStateForCurrentResolution = UE::MoviePipeline::GetRenderPassMetrics(GetPipelineMasterConfig(), ActiveShotList[CurrentShotIndex], SampleState, OutputResolution);
				for (UMoviePipelineRenderPass* RenderPass : InputBuffers)
				{
					RenderPass->RenderSample_GameThread(SampleStateForCurrentResolution);
				}
			}
		}
	}

	// Re-enable the progress widget so when the player viewport is drawn to the preview window, it shows.
	SetProgressWidgetVisible(true);
}

#if WITH_EDITOR
void UMoviePipeline::AddFrameToOutputMetadata(const FString& ClipName, const FString& ImageSequenceFileName, const FMoviePipelineFrameOutputState& FrameOutputState, const FString& Extension, const bool bHasAlpha)
{
	if (FrameOutputState.ShotIndex < 0 || FrameOutputState.ShotIndex >= ActiveShotList.Num())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("ShotIndex %d out of range"), FrameOutputState.ShotIndex);
		return;
	}

	FMovieSceneExportMetadataShot& ShotMetadata = OutputMetadata.Shots[FrameOutputState.ShotIndex];
	FMovieSceneExportMetadataClip& ClipMetadata = ShotMetadata.Clips.FindOrAdd(ClipName).FindOrAdd(Extension.ToUpper());

	if (!ClipMetadata.IsValid())
	{
		ClipMetadata.FileName = ImageSequenceFileName;
		ClipMetadata.bHasAlpha = bHasAlpha;
	}

	if (FrameOutputState.OutputFrameNumber < ClipMetadata.StartFrame)
	{
		ClipMetadata.StartFrame = FrameOutputState.OutputFrameNumber;
	}

	if (FrameOutputState.OutputFrameNumber > ClipMetadata.EndFrame)
	{
		ClipMetadata.EndFrame = FrameOutputState.OutputFrameNumber;
	}
}
#endif

void UMoviePipeline::AddOutputFuture(TFuture<bool>&& OutputFuture, const MoviePipeline::FMoviePipelineOutputFutureData& InOutputData)
{
	OutputFutures.Add(
		TTuple<TFuture<bool>, MoviePipeline::FMoviePipelineOutputFutureData>(MoveTemp(OutputFuture), InOutputData)
	);
}

void UMoviePipeline::ProcessOutstandingFinishedFrames()
{
	while (!OutputBuilder->FinishedFrames.IsEmpty())
	{
		FMoviePipelineMergerOutputFrame OutputFrame;
		OutputBuilder->FinishedFrames.Dequeue(OutputFrame);

		FRenderTimeStatistics& TimeStats = RenderTimeFrameStatistics.FindOrAdd(OutputFrame.FrameOutputState.OutputFrameNumber);
		TimeStats.EndTime = FDateTime::UtcNow();
	
		for (UMoviePipelineOutputBase* OutputContainer : GetPipelineMasterConfig()->GetOutputContainers())
		{
			OutputContainer->OnReceiveImageData(&OutputFrame);
		}
	}
}


void UMoviePipeline::OnSampleRendered(TUniquePtr<FImagePixelData>&& OutputSample)
{
	// This function handles the "Write all Samples" feature which lets you inspect data
	// pre-accumulation.
	UMoviePipelineOutputSetting* OutputSettings = GetPipelineMasterConfig()->FindSetting<UMoviePipelineOutputSetting>();
	check(OutputSettings);

	// This is for debug output, writing every individual sample to disk that comes off of the GPU (that isn't discarded).
	TUniquePtr<FImageWriteTask> TileImageTask = MakeUnique<FImageWriteTask>();

	FImagePixelDataPayload* InFrameData = OutputSample->GetPayload<FImagePixelDataPayload>();
	TileImageTask->Format = EImageFormat::EXR;
	TileImageTask->CompressionQuality = (int32)EImageCompressionQuality::Default;

	FString OutputName = InFrameData->Debug_OverrideFilename.IsEmpty() ?
		FString::Printf(TEXT("/%s_SS_%d_TS_%d_TileX_%d_TileY_%d.%d"),
			*InFrameData->PassIdentifier.Name, InFrameData->SampleState.SpatialSampleIndex, InFrameData->SampleState.TemporalSampleIndex,
			InFrameData->SampleState.TileIndexes.X, InFrameData->SampleState.TileIndexes.Y, InFrameData->SampleState.OutputState.OutputFrameNumber)
		: InFrameData->Debug_OverrideFilename;
	
	FString OutputDirectory = OutputSettings->OutputDirectory.Path;
	FString FileNameFormatString = OutputDirectory + OutputName;

	TMap<FString, FString> FormatOverrides;
	FormatOverrides.Add(TEXT("ext"), TEXT("exr"));
	UMoviePipelineExecutorShot* Shot = nullptr;
	if (InFrameData->SampleState.OutputState.ShotIndex >= 0 && InFrameData->SampleState.OutputState.ShotIndex < ActiveShotList.Num())
	{
		Shot = ActiveShotList[InFrameData->SampleState.OutputState.ShotIndex];
	}

	if (Shot)
	{
		FormatOverrides.Add(TEXT("shot_name"), Shot->OuterName);
		FormatOverrides.Add(TEXT("camera_name"), Shot->GetCameraName(InFrameData->SampleState.OutputState.CameraIndex));
	}
	FMoviePipelineFormatArgs FinalFormatArgs;

	FString FinalFilePath;
	ResolveFilenameFormatArguments(FileNameFormatString, FormatOverrides, FinalFilePath, FinalFormatArgs);

	TileImageTask->Filename = FinalFilePath;

	// Duplicate the data so that the Image Task can own it.
	TileImageTask->PixelData = MoveTemp(OutputSample);
	ImageWriteQueue->Enqueue(MoveTemp(TileImageTask));
}



void UMoviePipeline::FlushAsyncEngineSystems()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_MoviePipelineFlushAsyncEngineSystems);

	// Flush Block until Level Streaming completes. This solves the problem where levels that are not controlled
	// by the Sequencer Level Visibility track are marked for Async Load by a gameplay system.
	// This will register any new actors/components that were spawned during this frame. This needs 
	// to be done before the shader compiler is flushed so that we compile shaders for any newly
	// spawned component materials.
	if (GetWorld())
	{
		GetWorld()->BlockTillLevelStreamingCompleted();
	}

	// Ensure we have complete shader maps for all materials used by primitives in the world.
	// This way we will never render with the default material.
	UMaterialInterface::SubmitRemainingJobsForWorld(GetWorld());

	// Flush all assets still being compiled asynchronously.
	// A progressbar is already in place so the user can get feedback while waiting for everything to settle.
	FAssetCompilingManager::Get().FinishAllCompilation();

	// Flush streaming managers
	{
		UMoviePipelineGameOverrideSetting* GameOverrideSettings = FindOrAddSettingForShot<UMoviePipelineGameOverrideSetting>(ActiveShotList[CurrentShotIndex]);
		if (GameOverrideSettings && GameOverrideSettings->bFlushStreamingManagers)
		{
			FStreamingManagerCollection& StreamingManagers = IStreamingManager::Get();
			StreamingManagers.UpdateResourceStreaming(GetWorld()->GetDeltaSeconds(), /* bProcessEverything */ true);
			StreamingManagers.BlockTillAllRequestsFinished();
		}
	}

	// Flush grass
	if (CurrentShotIndex < ActiveShotList.Num())
	{
		UMoviePipelineGameOverrideSetting* GameOverrides = FindOrAddSettingForShot<UMoviePipelineGameOverrideSetting>(ActiveShotList[CurrentShotIndex]);
		if (GameOverrides && GameOverrides->bFlushGrassStreaming)
		{
			for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
			{
				ALandscapeProxy* LandscapeProxy = (*It);
				if (LandscapeProxy)
				{
					TArray<FVector> CameraList;
					LandscapeProxy->UpdateGrass(CameraList, true);
				}
			}
		}
	}

	// Flush virtual texture tile calculations
	ERHIFeatureLevel::Type FeatureLevel = GetWorld()->FeatureLevel;
	ENQUEUE_RENDER_COMMAND(VirtualTextureSystemFlushCommand)(
		[FeatureLevel](FRHICommandListImmediate& RHICmdList)
	{
		GetRendererModule().LoadPendingVirtualTextureTiles(RHICmdList, FeatureLevel);
	});
}

#undef LOCTEXT_NAMESPACE // "MoviePipeline"
