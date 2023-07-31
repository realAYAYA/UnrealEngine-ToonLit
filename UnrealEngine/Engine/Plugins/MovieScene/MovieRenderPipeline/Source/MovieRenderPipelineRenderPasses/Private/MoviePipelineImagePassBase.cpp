// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineImagePassBase.h"

// For Cine Camera Variables in Metadata
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MoviePipeline.h"
#include "GameFramework/PlayerController.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineViewFamilySetting.h"
#include "MoviePipelineQueue.h"
#include "LegacyScreenPercentageDriver.h"
#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineGameOverrideSetting.h"
#include "EngineModule.h"
#include "Engine/LocalPlayer.h"
#include "Engine/RendererSettings.h"
#include "MovieRenderOverlappedImage.h"
#include "ImageUtils.h"

// For Cine Camera Variables in Metadata
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "MoviePipelineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineImagePassBase)


DECLARE_CYCLE_STAT(TEXT("STAT_MoviePipeline_AccumulateSample_TT"), STAT_AccumulateSample_TaskThread, STATGROUP_MoviePipeline);

void UMoviePipelineImagePassBase::GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const
{
	OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	OutViewModeIndex = EViewModeIndex::VMI_Lit;
}

void UMoviePipelineImagePassBase::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);

	// Allocate 
	ViewState.Allocate(InPassInitSettings.FeatureLevel);
}

void UMoviePipelineImagePassBase::WaitUntilTasksComplete()
{
	GetPipeline()->SetPreviewTexture(nullptr);

	// This may call FlushRenderingCommands if there are outstanding readbacks that need to happen.
	for (TPair<FIntPoint, TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>> SurfaceQueueIt : SurfaceQueues)
	{
		if (SurfaceQueueIt.Value.IsValid())
		{
			SurfaceQueueIt.Value->Shutdown();
		}
	}

	// Stall until the task graph has completed any pending accumulations.
	FTaskGraphInterface::Get().WaitUntilTasksComplete(OutstandingTasks, ENamedThreads::GameThread);
	OutstandingTasks.Reset();
};

void UMoviePipelineImagePassBase::TeardownImpl()
{
	for (TPair<FIntPoint, TWeakObjectPtr<UTextureRenderTarget2D>>& TileRenderTargetIt : TileRenderTargets)
	{
		if (!TileRenderTargetIt.Value.IsValid())
		{
			TileRenderTargetIt.Value->RemoveFromRoot();
		}
	}

	SurfaceQueues.Empty();
	TileRenderTargets.Empty();

	FSceneViewStateInterface* Ref = ViewState.GetReference();
	if (Ref)
	{
		Ref->ClearMIDPool();
	}
	ViewState.Destroy();

	Super::TeardownImpl();
}

void UMoviePipelineImagePassBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMoviePipelineImagePassBase& This = *CastChecked<UMoviePipelineImagePassBase>(InThis);
	FSceneViewStateInterface* Ref = This.ViewState.GetReference();
	if (Ref)
	{
		Ref->AddReferencedObjects(Collector);
	}
}

void UMoviePipelineImagePassBase::RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState)
{
	Super::RenderSample_GameThreadImpl(InSampleState);

	// Wait for a all surfaces to be available to write to. This will stall the game thread while the RHI/Render Thread catch up.
	SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_WaitForAvailableSurface);
	for(TPair<FIntPoint, TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>> SurfaceQueueIt : SurfaceQueues)
	{
		if (SurfaceQueueIt.Value.IsValid())
		{
			SurfaceQueueIt.Value->BlockUntilAnyAvailable();
		}
	}
}

TWeakObjectPtr<UTextureRenderTarget2D> UMoviePipelineImagePassBase::GetOrCreateViewRenderTarget(const FIntPoint& InSize, IViewCalcPayload* OptPayload)
{
	if (const TWeakObjectPtr<UTextureRenderTarget2D>* ExistViewRenderTarget = TileRenderTargets.Find(InSize))
	{
		return *ExistViewRenderTarget;
	}

	const TWeakObjectPtr<UTextureRenderTarget2D> NewViewRenderTarget = CreateViewRenderTargetImpl(InSize, OptPayload);
	TileRenderTargets.Emplace(InSize, NewViewRenderTarget);

	return NewViewRenderTarget;
}

TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> UMoviePipelineImagePassBase::GetOrCreateSurfaceQueue(const FIntPoint& InSize, IViewCalcPayload* OptPayload)
{
	if (const TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>* ExistSurfaceQueue = SurfaceQueues.Find(InSize))
	{
		return *ExistSurfaceQueue;
	}

	const TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> NewSurfaceQueue = CreateSurfaceQueueImpl(InSize, OptPayload);
	SurfaceQueues.Emplace(InSize, NewSurfaceQueue);

	return NewSurfaceQueue;
}

TWeakObjectPtr<UTextureRenderTarget2D> UMoviePipelineImagePassBase::CreateViewRenderTargetImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload) const
{
	TWeakObjectPtr<UTextureRenderTarget2D> NewTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	NewTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// OCIO: Since this is a manually created Render target we don't need Gamma to be applied.
	// We use this render target to render to via a display extension that utilizes Display Gamma
	// which has a default value of 2.2 (DefaultDisplayGamma), therefore we need to set Gamma on this render target to 2.2 to cancel out any unwanted effects.
	NewTarget->TargetGamma = FOpenColorIODisplayExtension::DefaultDisplayGamma;

	// Initialize to the tile size (not final size) and use a 16 bit back buffer to avoid precision issues when accumulating later
	NewTarget->InitCustomFormat(InSize.X, InSize.Y, EPixelFormat::PF_FloatRGBA, false);
	NewTarget->AddToRoot();

	if (GetPipeline()->GetPreviewTexture() == nullptr)
	{
		GetPipeline()->SetPreviewTexture(NewTarget.Get());
	}

	return NewTarget;
}

TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> UMoviePipelineImagePassBase::CreateSurfaceQueueImpl(const FIntPoint& InSize, IViewCalcPayload* OptPayload) const
{
	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(InSize, EPixelFormat::PF_FloatRGBA, 3, true);

	return SurfaceQueue;
}

TSharedPtr<FSceneViewFamilyContext> UMoviePipelineImagePassBase::CalculateViewFamily(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	const FMoviePipelineFrameOutputState::FTimeData& TimeData = InOutSampleState.OutputState.TimeData;

	FEngineShowFlags ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	EViewModeIndex  ViewModeIndex;
	GetViewShowFlags(ShowFlags, ViewModeIndex);
	MoviePipelineRenderShowFlagOverride(ShowFlags);
	TWeakObjectPtr<UTextureRenderTarget2D> ViewRenderTarget = GetOrCreateViewRenderTarget(InOutSampleState.BackbufferSize, OptPayload);
	check(ViewRenderTarget.IsValid());

	FRenderTarget* RenderTarget = ViewRenderTarget->GameThread_GetRenderTargetResource();

	TSharedPtr<FSceneViewFamilyContext> OutViewFamily = MakeShared<FSceneViewFamilyContext>(FSceneViewFamily::ConstructionValues(
		RenderTarget,
		GetPipeline()->GetWorld()->Scene,
		ShowFlags)
		.SetTime(FGameTime::CreateUndilated(TimeData.WorldSeconds, TimeData.FrameDeltaTime))
		.SetRealtimeUpdate(true));

	OutViewFamily->SceneCaptureSource = InOutSampleState.SceneCaptureSource;
	OutViewFamily->bWorldIsPaused = InOutSampleState.bWorldIsPaused;
	OutViewFamily->ViewMode = ViewModeIndex;
	OutViewFamily->bOverrideVirtualTextureThrottle = true;

	const bool bIsPerspective = true;
	ApplyViewMode(OutViewFamily->ViewMode, bIsPerspective, OutViewFamily->EngineShowFlags);

	EngineShowFlagOverride(ESFIM_Game, OutViewFamily->ViewMode, OutViewFamily->EngineShowFlags, false);
	
	const UMoviePipelineExecutorShot* Shot = GetPipeline()->GetActiveShotList()[InOutSampleState.OutputState.ShotIndex];

	for (UMoviePipelineGameOverrideSetting* OverrideSetting : GetPipeline()->FindSettingsForShot<UMoviePipelineGameOverrideSetting>(Shot))
	{
		if (OverrideSetting->bOverrideVirtualTextureFeedbackFactor)
		{
			OutViewFamily->VirtualTextureFeedbackFactor = OverrideSetting->VirtualTextureFeedbackFactor;
		}
	}

	// No need to do anything if screen percentage is not supported. 
	if (IsScreenPercentageSupported())
	{
		// Allows all Output Settings to have an access to View Family. This allows to modify rendering output settings.
		for (UMoviePipelineViewFamilySetting* Setting : GetPipeline()->FindSettingsForShot<UMoviePipelineViewFamilySetting>(Shot))
		{
			Setting->SetupViewFamily(*OutViewFamily);
		}
	}

	// If UMoviePipelineViewFamilySetting never set a Screen percentage interface we fallback to default.
	if (OutViewFamily->GetScreenPercentageInterface() == nullptr)
	{
		OutViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*OutViewFamily, IsScreenPercentageSupported() ? InOutSampleState.GlobalScreenPercentageFraction : 1.f));
	}


	// View is added as a child of the OutViewFamily-> 
	FSceneView* View = GetSceneViewForSampleState(OutViewFamily.Get(), /*InOut*/ InOutSampleState, OptPayload);
	
	SetupViewForViewModeOverride(View);

	// Override the view's FrameIndex to be based on our progress through the sequence. This greatly increases
	// determinism with things like TAA.
	View->OverrideFrameIndexValue = InOutSampleState.FrameIndex;
	View->bCameraCut = InOutSampleState.bCameraCut;
	View->bIsOfflineRender = true;
	View->AntiAliasingMethod = IsAntiAliasingSupported() ? InOutSampleState.AntiAliasingMethod : EAntiAliasingMethod::AAM_None;

	// Override the Motion Blur settings since these are controlled by the movie pipeline.
	{
		FFrameRate OutputFrameRate = GetPipeline()->GetPipelineMasterConfig()->GetEffectiveFrameRate(GetPipeline()->GetTargetSequence());

		// We need to inversly scale the target FPS by time dilation to counteract slowmo. If scaling isn't applied then motion blur length
		// stays the same length despite the smaller delta time and the blur ends up too long.
		View->FinalPostProcessSettings.MotionBlurTargetFPS = FMath::RoundToInt(OutputFrameRate.AsDecimal() / FMath::Max(SMALL_NUMBER, InOutSampleState.OutputState.TimeData.TimeDilation));
		View->FinalPostProcessSettings.MotionBlurAmount = InOutSampleState.OutputState.TimeData.MotionBlurFraction;
		View->FinalPostProcessSettings.MotionBlurMax = 100.f;
		View->FinalPostProcessSettings.bOverride_MotionBlurAmount = true;
		View->FinalPostProcessSettings.bOverride_MotionBlurTargetFPS = true;
		View->FinalPostProcessSettings.bOverride_MotionBlurMax = true;

		// Skip the whole pass if they don't want motion blur.
		if (FMath::IsNearlyZero(InOutSampleState.OutputState.TimeData.MotionBlurFraction))
		{
			OutViewFamily->EngineShowFlags.SetMotionBlur(false);
		}
	}

	// Locked Exposure
	const bool bAutoExposureAllowed = IsAutoExposureAllowed(InOutSampleState);
	{
		// If the rendering pass doesn't allow autoexposure and they dont' have manual exposure set up, warn.
		if (!bAutoExposureAllowed && (View->FinalPostProcessSettings.AutoExposureMethod != EAutoExposureMethod::AEM_Manual))
		{
			// Skip warning if the project setting is disabled though, as exposure will be forced off in the renderer anyways.
			const URendererSettings* RenderSettings = GetDefault<URendererSettings>();
			if (RenderSettings->bDefaultFeatureAutoExposure != false)
			{
				UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Camera Auto Exposure Method not supported by one or more render passes. Change the Auto Exposure Method to Manual!"));
				View->FinalPostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
			}
		}
	}

	// Orthographic cameras don't support anti-aliasing outside the path tracer (other than FXAA)
	const bool bIsOrthographicCamera = !View->IsPerspectiveProjection();
	if (bIsOrthographicCamera)
	{
		bool bIsSupportedAAMethod = View->AntiAliasingMethod == EAntiAliasingMethod::AAM_FXAA;
		bool bIsPathTracer = OutViewFamily->EngineShowFlags.PathTracing;
		bool bWarnJitters = InOutSampleState.ProjectionMatrixJitterAmount.SquaredLength() > SMALL_NUMBER;
		if ((!bIsPathTracer && !bIsSupportedAAMethod) || bWarnJitters)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Orthographic Cameras are only supported with PathTracer or Deferred with FXAA Anti-Aliasing"));
		}
	}

	OutViewFamily->ViewExtensions.Append(GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(GetWorld()->Scene)));

	AddViewExtensions(*OutViewFamily, InOutSampleState);

	for (auto ViewExt : OutViewFamily->ViewExtensions)
	{
		ViewExt->SetupViewFamily(*OutViewFamily.Get());
	}

	for (int ViewExt = 0; ViewExt < OutViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		OutViewFamily->ViewExtensions[ViewExt]->SetupView(*OutViewFamily.Get(), *View);
	}

	// The requested configuration may not be supported, warn user and fall back. We can't call
	// FSceneView::SetupAntiAliasingMethod because it reads the value from the cvar which would
	// cause the value set by the MoviePipeline UI to be ignored.
	{
		bool bMethodWasUnsupported = false;
		if (View->AntiAliasingMethod == AAM_TemporalAA && !SupportsGen4TAA(View->GetShaderPlatform()))
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("TAA was requested but this hardware does not support it."));
			bMethodWasUnsupported = true;
		}
		else if (View->AntiAliasingMethod == AAM_TSR && !SupportsTSR(View->GetShaderPlatform()))
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("TSR was requested but this hardware does not support it."));
			bMethodWasUnsupported = true;
		}

		if (bMethodWasUnsupported)
		{
			View->AntiAliasingMethod = AAM_None;
		}
	}

	// Anti Aliasing
	{
		// If we're not using Temporal Anti-Aliasing or Path Tracing we will apply the View Matrix projection jitter. Normally TAA sets this
		// inside FSceneRenderer::PreVisibilityFrameSetup. Path Tracing does its own anti-aliasing internally.
		bool bApplyProjectionJitter = !bIsOrthographicCamera
									&& !OutViewFamily->EngineShowFlags.PathTracing 
									&& !IsTemporalAccumulationBasedMethod(View->AntiAliasingMethod);
		if (bApplyProjectionJitter)
		{
			View->ViewMatrices.HackAddTemporalAAProjectionJitter(InOutSampleState.ProjectionMatrixJitterAmount);
		}
	}

	// Path Tracer Sampling
	if (OutViewFamily->EngineShowFlags.PathTracing)
	{
		// override whatever settings came from PostProcessVolume or Camera

		// If motion blur is enabled:
		//    blend all spatial samples together while leaving the handling of temporal samples up to MRQ
		//    each temporal sample will include denoising and post-process effects
		// If motion blur is NOT enabled:
		//    blend all temporal+spatial samples within the path tracer and only apply denoising on the last temporal sample
		//    this way we minimize denoising cost and also allow a much higher number of temporal samples to be used which
		//    can help reduce strobing

		// NOTE: Tiling is not compatible with the reference motion blur mode because it changes the order of the loops over the image.
		const bool bAccumulateSpatialSamplesOnly = OutViewFamily->EngineShowFlags.MotionBlur || InOutSampleState.GetTileCount() > 1;

		const int32 SampleCount = bAccumulateSpatialSamplesOnly ? InOutSampleState.SpatialSampleCount : InOutSampleState.TemporalSampleCount * InOutSampleState.SpatialSampleCount;
		const int32 SampleIndex = bAccumulateSpatialSamplesOnly ? InOutSampleState.SpatialSampleIndex : InOutSampleState.TemporalSampleIndex * InOutSampleState.SpatialSampleCount + InOutSampleState.SpatialSampleIndex;

		// TODO: pass along FrameIndex (which includes SampleIndex) to make sure sampling is fully deterministic

		// Overwrite whatever sampling count came from the PostProcessVolume
		View->FinalPostProcessSettings.bOverride_PathTracingSamplesPerPixel = true;
		View->FinalPostProcessSettings.PathTracingSamplesPerPixel = SampleCount;

		// reset path tracer's accumulation at the start of each sample
		View->bForcePathTracerReset = SampleIndex == 0;

		// discard the result, unless its the last sample
		InOutSampleState.bDiscardResult |= !(SampleIndex == SampleCount - 1);
	}

	// Object Occlusion/Histories
	{
		// If we're using tiling, we force the reset of histories each frame so that we don't use the previous tile's
		// object occlusion queries, as that causes things to disappear from some views.
		if (InOutSampleState.GetTileCount() > 1)
		{
			View->bForceCameraVisibilityReset = true;
		}
	}

	// Bias all mip-mapping to pretend to be working at our target resolution and not our tile resolution
	// so that the images don't end up soft.
	{
		float EffectivePrimaryResolutionFraction = 1.f / InOutSampleState.TileCounts.X;
		View->MaterialTextureMipBias = FMath::Log2(EffectivePrimaryResolutionFraction);

		// Add an additional bias per user settings. This allows them to choose to make the textures sharper if it
		// looks better with their particular settings.
		View->MaterialTextureMipBias += InOutSampleState.TextureSharpnessBias;
	}

	return OutViewFamily;
}

void UMoviePipelineImagePassBase::SetupViewForViewModeOverride(FSceneView* View)
{
	if (View->Family->EngineShowFlags.Wireframe)
	{
		// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
		View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
		View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
	}
	else if (View->Family->EngineShowFlags.OverrideDiffuseAndSpecular)
	{
		View->DiffuseOverrideParameter = FVector4f(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
		View->SpecularOverrideParameter = FVector4f(.1f, .1f, .1f, 0.0f);
	}
	else if (View->Family->EngineShowFlags.LightingOnlyOverride)
	{
		View->DiffuseOverrideParameter = FVector4f(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
		View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
	}
	else if (View->Family->EngineShowFlags.ReflectionOverride)
	{
		View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
		View->SpecularOverrideParameter = FVector4f(1, 1, 1, 0.0f);
		View->NormalOverrideParameter = FVector4f(0, 0, 1, 0.0f);
		View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
	}

	if (!View->Family->EngineShowFlags.Diffuse)
	{
		View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
	}

	if (!View->Family->EngineShowFlags.Specular)
	{
		View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
	}
	FName BufferVisualizationMode = "WorldNormal";
	View->CurrentBufferVisualizationMode = BufferVisualizationMode;
}

void UMoviePipelineImagePassBase::GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
{
	Super::GatherOutputPassesImpl(ExpectedRenderPasses);
	ExpectedRenderPasses.Add(PassIdentifier);
}

FSceneView* UMoviePipelineImagePassBase::GetSceneViewForSampleState(FSceneViewFamily* ViewFamily, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	APlayerController* LocalPlayerController = GetPipeline()->GetWorld()->GetFirstPlayerController();

	int32 TileSizeX = InOutSampleState.BackbufferSize.X;
	int32 TileSizeY = InOutSampleState.BackbufferSize.Y;

	UE::MoviePipeline::FImagePassCameraViewData CameraInfo = GetCameraInfo(InOutSampleState, OptPayload);

	const float DestAspectRatio = InOutSampleState.BackbufferSize.X / (float)InOutSampleState.BackbufferSize.Y;
	const float CameraAspectRatio = bAllowCameraAspectRatio ? CameraInfo.ViewInfo.AspectRatio : DestAspectRatio;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.ViewOrigin = CameraInfo.ViewInfo.Location;
	ViewInitOptions.SetViewRectangle(FIntRect(FIntPoint(0, 0), FIntPoint(TileSizeX, TileSizeY)));
	ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(CameraInfo.ViewInfo.Rotation);
	ViewInitOptions.ViewActor = CameraInfo.ViewActor;

	// Rotate the view 90 degrees (reason: unknown)
	ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	float ViewFOV = CameraInfo.ViewInfo.FOV;

	// Inflate our FOV to support the overscan 
	ViewFOV = 2.0f * FMath::RadiansToDegrees(FMath::Atan((1.0f + InOutSampleState.OverscanPercentage) * FMath::Tan(FMath::DegreesToRadians( ViewFOV * 0.5f ))));

	float DofSensorScale = 1.0f;

	// Calculate a Projection Matrix. This code unfortunately ends up similar to, but not quite the same as FMinimalViewInfo::CalculateProjectionMatrixGivenView
	FMatrix BaseProjMatrix;

	if (CameraInfo.bUseCustomProjectionMatrix)
	{
		BaseProjMatrix = CameraInfo.CustomProjectionMatrix;

		// Modify the custom matrix to do an off center projection, with overlap for high-res tiling
		const bool bOrthographic = false;
		ModifyProjectionMatrixForTiling(InOutSampleState, bOrthographic, /*InOut*/ BaseProjMatrix, DofSensorScale);
	}
	else
	{
		if (CameraInfo.ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			const float YScale = 1.0f / CameraAspectRatio;
			const float OverscanScale = 1.0f + InOutSampleState.OverscanPercentage;

			const float HalfOrthoWidth = (CameraInfo.ViewInfo.OrthoWidth / 2.0f) * OverscanScale;
			const float ScaledOrthoHeight = (CameraInfo.ViewInfo.OrthoWidth / 2.0f) * OverscanScale * YScale;

			const float NearPlane = CameraInfo.ViewInfo.OrthoNearClipPlane;
			const float FarPlane = CameraInfo.ViewInfo.OrthoFarClipPlane;

			const float ZScale = 1.0f / (FarPlane - NearPlane);
			const float ZOffset = -NearPlane;

			BaseProjMatrix = FReversedZOrthoMatrix(
				HalfOrthoWidth,
				ScaledOrthoHeight,
				ZScale,
				ZOffset
			);
			ViewInitOptions.bUseFauxOrthoViewPos = true;
			
			// Modify the projection matrix to do an off center projection, with overlap for high-res tiling
			const bool bOrthographic = true;
			ModifyProjectionMatrixForTiling(InOutSampleState, bOrthographic, /*InOut*/ BaseProjMatrix, DofSensorScale);
		}
		else
		{
			float XAxisMultiplier;
			float YAxisMultiplier;

			if (CameraInfo.ViewInfo.bConstrainAspectRatio)
			{
				// If the camera's aspect ratio has a thinner width, then stretch the horizontal fov more than usual to 
				// account for the extra with of (before constraining - after constraining)
				if (CameraAspectRatio < DestAspectRatio)
				{
					const float ConstrainedWidth = ViewInitOptions.GetViewRect().Height() * CameraAspectRatio;
					XAxisMultiplier = ConstrainedWidth / (float)ViewInitOptions.GetViewRect().Width();
					YAxisMultiplier = CameraAspectRatio;
				}
				// Simplified some math here but effectively functions similarly to the above, the unsimplified code would look like:
				// const float ConstrainedHeight = ViewInitOptions.GetViewRect().Width() / CameraCache.AspectRatio;
				// YAxisMultiplier = (ConstrainedHeight / ViewInitOptions.GetViewRect.Height()) * CameraCache.AspectRatio;
				else
				{
					XAxisMultiplier = 1.0f;
					YAxisMultiplier = ViewInitOptions.GetViewRect().Width() / (float)ViewInitOptions.GetViewRect().Height();
				}
			}
			else
			{
				const int32 DestSizeX = ViewInitOptions.GetViewRect().Width();
				const int32 DestSizeY = ViewInitOptions.GetViewRect().Height();
				const EAspectRatioAxisConstraint AspectRatioAxisConstraint = GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint;
				if (((DestSizeX > DestSizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
				{
					//if the viewport is wider than it is tall
					XAxisMultiplier = 1.0f;
					YAxisMultiplier = ViewInitOptions.GetViewRect().Width() / (float)ViewInitOptions.GetViewRect().Height();
				}
				else
				{
					//if the viewport is taller than it is wide
					XAxisMultiplier = ViewInitOptions.GetViewRect().Height() / (float)ViewInitOptions.GetViewRect().Width();
					YAxisMultiplier = 1.0f;
				}
			}

			const float MinZ = CameraInfo.ViewInfo.GetFinalPerspectiveNearClipPlane();
			const float MaxZ = MinZ;
			// Avoid zero ViewFOV's which cause divide by zero's in projection matrix
			const float MatrixFOV = FMath::Max(0.001f, ViewFOV) * (float)PI / 360.0f;


			if ((bool)ERHIZBuffer::IsInverted)
			{
				BaseProjMatrix = FReversedZPerspectiveMatrix(
					MatrixFOV,
					MatrixFOV,
					XAxisMultiplier,
					YAxisMultiplier,
					MinZ,
					MaxZ
				);
			}
			else
			{
				BaseProjMatrix = FPerspectiveMatrix(
					MatrixFOV,
					MatrixFOV,
					XAxisMultiplier,
					YAxisMultiplier,
					MinZ,
					MaxZ
				);
			}

			// Modify the perspective matrix to do an off center projection, with overlap for high-res tiling
			const bool bOrthographic = false;
			ModifyProjectionMatrixForTiling(InOutSampleState, bOrthographic, /*InOut*/ BaseProjMatrix, DofSensorScale);
			// ToDo: Does orthographic support tiling in the same way or do I need to modify the values before creating the ortho view.
		}
	}
		// BaseProjMatrix may be perspective or orthographic.
		ViewInitOptions.ProjectionMatrix = BaseProjMatrix;

	ViewInitOptions.SceneViewStateInterface = GetSceneViewStateInterface(OptPayload);
	ViewInitOptions.FOV = ViewFOV;
	ViewInitOptions.DesiredFOV = ViewFOV;

	FSceneView* View = new FSceneView(ViewInitOptions);
	ViewFamily->Views.Add(View);

	
	View->ViewLocation = CameraInfo.ViewInfo.Location;
	View->ViewRotation = CameraInfo.ViewInfo.Rotation;
	// Override previous/current view transforms so that tiled renders don't use the wrong occlusion/motion blur information.
	View->PreviousViewTransform = CameraInfo.ViewInfo.PreviousViewTransform;

	View->StartFinalPostprocessSettings(View->ViewLocation);
	BlendPostProcessSettings(View, InOutSampleState, OptPayload);

	// Scaling sensor size inversely with the the projection matrix [0][0] should physically
	// cause the circle of confusion to be unchanged.
	View->FinalPostProcessSettings.DepthOfFieldSensorWidth *= DofSensorScale;
	// Modify the 'center' of the lens to be offset for high-res tiling, helps some effects (vignette) etc. still work.
	View->LensPrincipalPointOffsetScale = (FVector4f)CalculatePrinciplePointOffsetForTiling(InOutSampleState); // LWC_TODO: precision loss. CalculatePrinciplePointOffsetForTiling() could return float, it's normalized?
	View->EndFinalPostprocessSettings(ViewInitOptions);

	// This metadata is per-file and not per-view, but we need the blended result from the view to actually match what we rendered.
	// To solve this, we'll insert metadata per renderpass, separated by render pass name.
	InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("unreal/%s/%s/fstop"), *PassIdentifier.CameraName, *PassIdentifier.Name), FString::SanitizeFloat(View->FinalPostProcessSettings.DepthOfFieldFstop));
	InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("unreal/%s/%s/fov"), *PassIdentifier.CameraName, *PassIdentifier.Name), FString::SanitizeFloat(ViewInitOptions.FOV));
	InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("unreal/%s/%s/focalDistance"), *PassIdentifier.CameraName, *PassIdentifier.Name), FString::SanitizeFloat(View->FinalPostProcessSettings.DepthOfFieldFocalDistance));
	InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("unreal/%s/%s/sensorWidth"), *PassIdentifier.CameraName, *PassIdentifier.Name), FString::SanitizeFloat(View->FinalPostProcessSettings.DepthOfFieldSensorWidth));
	InOutSampleState.OutputState.FileMetadata.Add(FString::Printf(TEXT("unreal/%s/%s/overscanPercent"), *PassIdentifier.CameraName, *PassIdentifier.Name), FString::SanitizeFloat(InOutSampleState.OverscanPercentage));

	InOutSampleState.OutputState.FileMetadata.Append(CameraInfo.FileMetadata);
	return View;
}

void UMoviePipelineImagePassBase::BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	check(InView);

	APlayerController* LocalPlayerController = GetPipeline()->GetWorld()->GetFirstPlayerController();
	// CameraAnim override
	if (LocalPlayerController->PlayerCameraManager)
	{
		TArray<FPostProcessSettings> const* CameraAnimPPSettings;
		TArray<float> const* CameraAnimPPBlendWeights;
		LocalPlayerController->PlayerCameraManager->GetCachedPostProcessBlends(CameraAnimPPSettings, CameraAnimPPBlendWeights);

		if (LocalPlayerController->PlayerCameraManager->bEnableFading)
		{
			InView->OverlayColor = LocalPlayerController->PlayerCameraManager->FadeColor;
			InView->OverlayColor.A = FMath::Clamp(LocalPlayerController->PlayerCameraManager->FadeAmount, 0.f, 1.f);
		}

		if (LocalPlayerController->PlayerCameraManager->bEnableColorScaling)
		{
			FVector ColorScale = LocalPlayerController->PlayerCameraManager->ColorScale;
			InView->ColorScale = FLinearColor(ColorScale.X, ColorScale.Y, ColorScale.Z);
		}

		FMinimalViewInfo ViewInfo = LocalPlayerController->PlayerCameraManager->GetCameraCacheView();
		for (int32 PPIdx = 0; PPIdx < CameraAnimPPBlendWeights->Num(); ++PPIdx)
		{
			InView->OverridePostProcessSettings((*CameraAnimPPSettings)[PPIdx], (*CameraAnimPPBlendWeights)[PPIdx]);
		}

		InView->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);
	}
}

FVector4 UMoviePipelineImagePassBase::CalculatePrinciplePointOffsetForTiling(const FMoviePipelineRenderPassMetrics& InSampleState) const
{
	// We need our final view parameters to be in the space of [-1,1], including all the tiles.
		// Starting with a single tile, the middle of the tile in offset screen space is:
	FVector2D TilePrincipalPointOffset;

	TilePrincipalPointOffset.X = (float(InSampleState.TileIndexes.X) + 0.5f - (0.5f * float(InSampleState.TileCounts.X))) * 2.0f;
	TilePrincipalPointOffset.Y = (float(InSampleState.TileIndexes.Y) + 0.5f - (0.5f * float(InSampleState.TileCounts.Y))) * 2.0f;

	// For the tile size ratio, we have to multiply by (1.0 + overlap) and then divide by tile num
	FVector2D OverlapScale;
	OverlapScale.X = (1.0f + float(2 * InSampleState.OverlappedPad.X) / float(InSampleState.TileSize.X));
	OverlapScale.Y = (1.0f + float(2 * InSampleState.OverlappedPad.Y) / float(InSampleState.TileSize.Y));

	TilePrincipalPointOffset.X /= OverlapScale.X;
	TilePrincipalPointOffset.Y /= OverlapScale.Y;

	FVector2D TilePrincipalPointScale;
	TilePrincipalPointScale.X = OverlapScale.X / float(InSampleState.TileCounts.X);
	TilePrincipalPointScale.Y = OverlapScale.Y / float(InSampleState.TileCounts.Y);

	TilePrincipalPointOffset.X *= TilePrincipalPointScale.X;
	TilePrincipalPointOffset.Y *= TilePrincipalPointScale.Y;

	return FVector4(TilePrincipalPointOffset.X, -TilePrincipalPointOffset.Y, TilePrincipalPointScale.X, TilePrincipalPointScale.Y);
}

void UMoviePipelineImagePassBase::ModifyProjectionMatrixForTiling(const FMoviePipelineRenderPassMetrics& InSampleState, const bool bInOrthographic, FMatrix& InOutProjectionMatrix, float& OutDoFSensorScale) const
{
	float PadRatioX = 1.0f;
	float PadRatioY = 1.0f;

	if (InSampleState.OverlappedPad.X > 0 && InSampleState.OverlappedPad.Y > 0)
	{
		PadRatioX = float(InSampleState.OverlappedPad.X * 2 + InSampleState.TileSize.X) / float(InSampleState.TileSize.X);
		PadRatioY = float(InSampleState.OverlappedPad.Y * 2 + InSampleState.TileSize.Y) / float(InSampleState.TileSize.Y);
	}

	float ScaleX = PadRatioX / float(InSampleState.TileCounts.X);
	float ScaleY = PadRatioY / float(InSampleState.TileCounts.Y);

	InOutProjectionMatrix.M[0][0] /= ScaleX;
	InOutProjectionMatrix.M[1][1] /= ScaleY;
	OutDoFSensorScale = ScaleX;

	// this offset would be correct with no pad
	float OffsetX = -((float(InSampleState.TileIndexes.X) + 0.5f - float(InSampleState.TileCounts.X) / 2.0f) * 2.0f);
	float OffsetY = ((float(InSampleState.TileIndexes.Y) + 0.5f - float(InSampleState.TileCounts.Y) / 2.0f) * 2.0f);

	if (bInOrthographic)
	{
		InOutProjectionMatrix.M[3][0] += OffsetX / PadRatioX;
		InOutProjectionMatrix.M[3][1] += OffsetY / PadRatioY;
	}
	else
	{
	InOutProjectionMatrix.M[2][0] += OffsetX / PadRatioX;
		InOutProjectionMatrix.M[2][1] += OffsetY / PadRatioY;
	}
}

UE::MoviePipeline::FImagePassCameraViewData UMoviePipelineImagePassBase::GetCameraInfo(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload) const
{
	UE::MoviePipeline::FImagePassCameraViewData OutCameraData;

	// Default implementation doesn't support multi-camera and always provides the information from the current PlayerCameraManager
	if (GetPipeline()->GetWorld()->GetFirstPlayerController()->PlayerCameraManager)
	{
		OutCameraData.ViewInfo = GetPipeline()->GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraCacheView();

		// Now override some of the properties with things that come from MRQ
		OutCameraData.ViewInfo.Location = InOutSampleState.FrameInfo.CurrViewLocation;
		OutCameraData.ViewInfo.Rotation = InOutSampleState.FrameInfo.CurrViewRotation;
		OutCameraData.ViewInfo.PreviousViewTransform = FTransform(InOutSampleState.FrameInfo.PrevViewRotation, InOutSampleState.FrameInfo.PrevViewLocation);

		// And some fields that aren't in FMinimalViewInfo
		OutCameraData.ViewActor = GetPipeline()->GetWorld()->GetFirstPlayerController()->GetViewTarget();

		// This only works if you use a Cine Camera (which is almost guranteed with Sequencer) and it's easier (and less human error prone) than re-deriving the information
		ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetViewTarget());
		if (CineCameraActor)
		{
			UCineCameraComponent* CineCameraComponent = CineCameraActor->GetCineCameraComponent();
			if (CineCameraComponent)
			{
				// Add camera-specific metadata
				UE::MoviePipeline::GetMetadataFromCineCamera(CineCameraComponent, PassIdentifier.CameraName, PassIdentifier.Name, OutCameraData.FileMetadata);
			}
		}
	}

	return OutCameraData;
}


TSharedPtr<FAccumulatorPool::FAccumulatorInstance, ESPMode::ThreadSafe> FAccumulatorPool::BlockAndGetAccumulator_GameThread(int32 InFrameNumber, const FMoviePipelinePassIdentifier& InPassIdentifier)
{
	FScopeLock ScopeLock(&CriticalSection);

	int32 AvailableIndex = INDEX_NONE;
	while (AvailableIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Accumulators.Num(); Index++)
		{
			if (InFrameNumber == Accumulators[Index]->ActiveFrameNumber && InPassIdentifier == Accumulators[Index]->ActivePassIdentifier)
			{
				AvailableIndex = Index;
				break;
			}
		}

		if (AvailableIndex == INDEX_NONE)
		{
			// If we don't have an accumulator already working on it let's look for a free one.
			for (int32 Index = 0; Index < Accumulators.Num(); Index++)
			{
				if (!Accumulators[Index]->IsActive())
				{
					// Found a free one, tie it to this output frame.
					Accumulators[Index]->ActiveFrameNumber = InFrameNumber;
					Accumulators[Index]->ActivePassIdentifier = InPassIdentifier;
					Accumulators[Index]->bIsActive = true;
					Accumulators[Index]->TaskPrereq = nullptr;
					AvailableIndex = Index;
					break;
				}
			}
		}
	}

	return Accumulators[AvailableIndex];
}

bool FAccumulatorPool::FAccumulatorInstance::IsActive() const
{
	return bIsActive;
}

void FAccumulatorPool::FAccumulatorInstance::SetIsActive(const bool bInIsActive)
{
	bIsActive = bInIsActive;
}

namespace MoviePipeline
{
	void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const MoviePipeline::FImageSampleAccumulationArgs& InParams)
	{
		SCOPE_CYCLE_COUNTER(STAT_AccumulateSample_TaskThread);

		TUniquePtr<FImagePixelData> SamplePixelData = MoveTemp(InPixelData);
		const bool bIsWellFormed = SamplePixelData->IsDataWellFormed();

		if (!bIsWellFormed)
		{
			// figure out why it is not well formed, and print a warning.
			int64 RawSize = SamplePixelData->GetRawDataSizeInBytes();

			int64 SizeX = SamplePixelData->GetSize().X;
			int64 SizeY = SamplePixelData->GetSize().Y;
			int64 ByteDepth = int64(SamplePixelData->GetBitDepth() / 8);
			int64 NumChannels = int64(SamplePixelData->GetNumChannels());
			int64 ExpectedTotalSize = SizeX * SizeY * ByteDepth * NumChannels;
			int64 ActualTotalSize = SamplePixelData->GetRawDataSizeInBytes();

			UE_LOG(LogMovieRenderPipeline, Log, TEXT("AccumulateSample_RenderThread: Data is not well formed."));
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Image dimension: %lldx%lld, %lld, %lld"), SizeX, SizeY, ByteDepth, NumChannels);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Expected size: %lld"), ExpectedTotalSize);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Actual size:   %lld"), ActualTotalSize);
		}

		check(bIsWellFormed);

		FImagePixelDataPayload* OriginalFramePayload = SamplePixelData->GetPayload<FImagePixelDataPayload>();
		check(OriginalFramePayload);

		// We duplicate the payload for now because there are multiple cases where we need to create a new 
		// image payload and we can't transfer the existing payload over.
		TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> NewPayload = OriginalFramePayload->Copy();

		// Writing tiles can be useful for debug reasons. These get passed onto the output every frame.
		if (NewPayload->SampleState.bWriteSampleToDisk)
		{
			// Send the data to the Output Builder. This has to be a copy of the pixel data from the GPU, since
			// it enqueues it onto the game thread and won't be read/sent to write to disk for another frame. 
			// The extra copy is unfortunate, but is only the size of a single sample (ie: 1920x1080 -> 17mb)
			TUniquePtr<FImagePixelData> SampleData = SamplePixelData->CopyImageData();
			ensure(InParams.OutputMerger.IsValid());
			InParams.OutputMerger.Pin()->OnSingleSampleDataAvailable_AnyThread(MoveTemp(SampleData));
		}

		// Optimization! If we don't need the accumulator (no tiling, no supersampling) then we'll skip it and just send it straight to the output stage.
		// This significantly improves performance in the baseline case.
		const bool bOneTile = NewPayload->IsFirstTile() && NewPayload->IsLastTile();
		const bool bOneTS = NewPayload->IsFirstTemporalSample() && NewPayload->IsLastTemporalSample();
		const bool bOneSS = NewPayload->SampleState.SpatialSampleCount == 1;

		if (bOneTile && bOneTS && bOneSS)
		{
			// Send the data directly to the Output Builder and skip the accumulator.
			ensure(InParams.OutputMerger.IsValid());
			InParams.OutputMerger.Pin()->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(SamplePixelData));
			return;
		}

		// Allocate memory if the ImageAccumulator has not been initialized yet for this output
		// This usually happens on the first sample (regular case), or on the last spatial sample of the first temporal sample (path tracer)
		MoviePipeline::FTileWeight1D WeightFunctionX;
		MoviePipeline::FTileWeight1D WeightFunctionY;
		NewPayload->GetWeightFunctionParams(/*Out*/ WeightFunctionX, /*Out*/ WeightFunctionY);

		TSharedPtr<FImageOverlappedAccumulator> PinnedImageAccumulator = InParams.ImageAccumulator.Pin();
		TSharedPtr<IMoviePipelineOutputMerger> PinnedOutputMerger = InParams.OutputMerger.Pin();

		ensure(PinnedImageAccumulator.IsValid());
		ensure(PinnedOutputMerger.IsValid());
		if (PinnedImageAccumulator->NumChannels == 0)
		{
			LLM_SCOPE_BYNAME(TEXT("MoviePipeline/ImageAccumulatorInitMemory"));
			int32 ChannelCount = InParams.bAccumulateAlpha ? 4 : 3;
			PinnedImageAccumulator->InitMemory(NewPayload->GetAccumulatorSize(), ChannelCount); 
			PinnedImageAccumulator->ZeroPlanes();
			PinnedImageAccumulator->AccumulationGamma = NewPayload->SampleState.AccumulationGamma;
		}

		// Accumulate the new sample to our target
		{
			// Some samples can come back at a different size than expected (post process materials) which
			// creates numerous issues with the accumulators. To work around this issue for now, we will resize
			// the image to the expected resolution. 
			FIntPoint RawSize = SamplePixelData->GetSize();
			const bool bCorrectSize = NewPayload->GetOverlapPaddedSizeIsValid(RawSize);
			
			if (!bCorrectSize)
			{
				const double ResizeConvertBeginTime = FPlatformTime::Seconds();

				// Convert the incoming data to full floats (the accumulator would do this later normally anyways)
				TArray64<FLinearColor> FullSizeData;
				FullSizeData.AddUninitialized(RawSize.X * RawSize.Y);

				if (SamplePixelData->GetType() == EImagePixelType::Float32)
				{
					const void* RawDataPtr;
					int64 RawDataSize;
					SamplePixelData->GetRawData(RawDataPtr, RawDataSize);

					FMemory::Memcpy(FullSizeData.GetData(), RawDataPtr, RawDataSize);
				}
				else if (SamplePixelData->GetType() == EImagePixelType::Float16)
				{
					const void* RawDataPtr;
					int64 RawDataSize;
					SamplePixelData->GetRawData(RawDataPtr, RawDataSize);

					const FFloat16Color* DataAsColor = reinterpret_cast<const FFloat16Color*>(RawDataPtr);
					for (int64 Index = 0; Index < RawSize.X * RawSize.Y; Index++)
					{
						FullSizeData[Index] = FLinearColor(DataAsColor[Index]);
					}
				}
				else
				{
					check(0);
				}
				const double ResizeConvertEndTime = FPlatformTime::Seconds();

				// Now we can resize to our target size.
				FIntPoint TargetSize = NewPayload->GetOverlapPaddedSize();

				TArray64<FLinearColor> NewPixelData;
				NewPixelData.SetNumUninitialized(TargetSize.X* TargetSize.Y);

				FImageUtils::ImageResize(RawSize.X, RawSize.Y, MakeArrayView<FLinearColor>(FullSizeData.GetData(), FullSizeData.Num()), TargetSize.X, TargetSize.Y, MakeArrayView<FLinearColor>(NewPixelData.GetData(), NewPixelData.Num()));

				const float ElapsedConvertMs = float((ResizeConvertEndTime - ResizeConvertBeginTime) * 1000.0f);
				const float ElapsedResizeMs = float((FPlatformTime::Seconds() - ResizeConvertEndTime) * 1000.0f);

				UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Resize Convert Time: %8.2fms Resize Time: %8.2fms"), ElapsedConvertMs, ElapsedResizeMs);

				SamplePixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(TargetSize.X, TargetSize.Y), MoveTemp(NewPixelData), NewPayload);

				// Update the raw size to match our new size.
				RawSize = SamplePixelData->GetSize();
			}

			const double AccumulateBeginTime = FPlatformTime::Seconds();

			// This should have been rescaled now if needed, so we can just check again to validate.
			check(NewPayload->GetOverlapPaddedSizeIsValid(RawSize));

			// bool bSkip = NewPayload->SampleState.TileIndexes.X != 0 || NewPayload->SampleState.TileIndexes.Y != 1;
			// if (!bSkip)
			{
				PinnedImageAccumulator->AccumulatePixelData(*SamplePixelData, NewPayload->GetOverlappedOffset(), NewPayload->GetOverlappedSubpixelShift(), WeightFunctionX, WeightFunctionY);
			}

			const double AccumulateEndTime = FPlatformTime::Seconds();
			const float ElapsedMs = float((AccumulateEndTime - AccumulateBeginTime) * 1000.0f);

			UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Accumulation time: %8.2fms"), ElapsedMs);
			
		}

		if (NewPayload->IsLastTile() && NewPayload->IsLastTemporalSample())
		{
			int32 FullSizeX = PinnedImageAccumulator->PlaneSize.X;
			int32 FullSizeY = PinnedImageAccumulator->PlaneSize.Y;

			// Now that a tile is fully built and accumulated we can notify the output builder that the
			// data is ready so it can pass that onto the output containers (if needed).
			if (SamplePixelData->GetType() == EImagePixelType::Float32)
			{
				// 32 bit FLinearColor
				TUniquePtr<TImagePixelData<FLinearColor> > FinalPixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(FullSizeX, FullSizeY), NewPayload);
				PinnedImageAccumulator->FetchFinalPixelDataLinearColor(FinalPixelData->Pixels);

				// Send the data to the Output Builder
				PinnedOutputMerger->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
			}
			else if (SamplePixelData->GetType() == EImagePixelType::Float16)
			{
				// 16 bit FLinearColor
				TUniquePtr<TImagePixelData<FFloat16Color> > FinalPixelData = MakeUnique<TImagePixelData<FFloat16Color>>(FIntPoint(FullSizeX, FullSizeY), NewPayload);
				PinnedImageAccumulator->FetchFinalPixelDataHalfFloat(FinalPixelData->Pixels);

				// Send the data to the Output Builder
				PinnedOutputMerger->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
			}
			else if (SamplePixelData->GetType() == EImagePixelType::Color)
			{
				// 8bit FColors
				TUniquePtr<TImagePixelData<FColor>> FinalPixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint(FullSizeX, FullSizeY), NewPayload);
				PinnedImageAccumulator->FetchFinalPixelDataByte(FinalPixelData->Pixels);

				// Send the data to the Output Builder
				PinnedOutputMerger->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
			}
			else
			{
				check(0);
			}

			// Free the memory in the accumulator.
			PinnedImageAccumulator->Reset();
		}
	}
}


