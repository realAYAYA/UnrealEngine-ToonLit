// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Renderers/MovieGraphImagePassBase.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphPipeline.h"
#include "SceneView.h"
#include "MoviePipelineUtils.h"
#include "LegacyScreenPercentageDriver.h"
#include "MoviePipelineSurfaceReader.h"
#include "CanvasTypes.h"
#include "MovieRenderOverlappedImage.h"
#include "Engine/RendererSettings.h"
#include "UnrealClient.h"
#include "SceneViewExtensionContext.h"
#include "SceneViewExtension.h"
#include "Engine/Engine.h"

namespace UE::MovieGraph::Rendering
{

void FMovieGraphImagePassBase::Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer)
{
	// This is a pointer to the UMovieGraphPipeline's renderer which is valid throughout the entire render.
	WeakGraphRenderer = InRenderer;
}

FSceneViewInitOptions FMovieGraphImagePassBase::CreateViewInitOptions(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo, FSceneViewFamilyContext* InViewFamily, FSceneViewStateReference& InViewStateRef) const
{
	check(InViewFamily);

	FIntPoint RenderResolution = InViewFamily->RenderTarget->GetSizeXY();

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = InViewFamily;
	ViewInitOptions.ViewOrigin = InCameraInfo.ViewInfo.Location;
	ViewInitOptions.ViewLocation = InCameraInfo.ViewInfo.Location;
	ViewInitOptions.ViewRotation = InCameraInfo.ViewInfo.Rotation;
	ViewInitOptions.SetViewRectangle(FIntRect(FIntPoint(0, 0), RenderResolution));
	ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(InCameraInfo.ViewInfo.Rotation);
	ViewInitOptions.ViewActor = InCameraInfo.ViewActor;
	ViewInitOptions.ProjectionMatrix = InCameraInfo.ProjectionMatrix;

	// Rotate the view 90 degrees to match the rest of the engine.
	ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	
	float ViewFOV = InCameraInfo.ViewInfo.FOV;

	// Inflate our FOV to support the overscan 
	// ToDo: This is a duplicate of the logic in CalculateProjectionMatrix, should combine.
	ViewFOV = 2.0f * FMath::RadiansToDegrees(FMath::Atan((1.0f + InCameraInfo.OverscanFraction) * FMath::Tan(FMath::DegreesToRadians( ViewFOV * 0.5f ))));

	ViewInitOptions.SceneViewStateInterface = InViewStateRef.GetReference();
	ViewInitOptions.FOV = ViewFOV;
	ViewInitOptions.DesiredFOV = ViewFOV;
	
	return ViewInitOptions;
}



FSceneView* FMovieGraphImagePassBase::CreateSceneView(const FSceneViewInitOptions& InInitOptions, TSharedRef<FSceneViewFamilyContext> InViewFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const
{
	// Create our view based on the Init Options
	FSceneView* View = new FSceneView(InInitOptions);
	InViewFamily->Views.Add(View);

	View->StartFinalPostprocessSettings(InInitOptions.ViewLocation);
	ApplyCameraManagerPostProcessBlends(View);

	// Scaling sensor size inversely with the the projection matrix [0][0] should physically
	// cause the circle of confusion to be unchanged.
	View->FinalPostProcessSettings.DepthOfFieldSensorWidth *= InCameraInfo.DoFSensorScale;
	// Modify the 'center' of the lens to be offset for high-res tiling, helps some effects (vignette) etc. still work.
	View->LensPrincipalPointOffsetScale = CalculatePrinciplePointOffsetForTiling(InCameraInfo.TilingParams);
	View->EndFinalPostprocessSettings(InInitOptions);

	return View;
}

void FMovieGraphImagePassBase::ApplyCameraManagerPostProcessBlends(FSceneView* InView) const
{
	check(InView);

	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (!GraphRenderer)
	{
		return;
	}

	APlayerController* LocalPlayerController = GraphRenderer->GetWorld()->GetFirstPlayerController();
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

TSharedRef<FSceneViewFamilyContext> FMovieGraphImagePassBase::CreateSceneViewFamily(const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const
{

	EViewModeIndex ViewModeIndex = InInitData.ViewModeIndex;
	FEngineShowFlags ShowFlags = InInitData.ShowFlags;

	const bool bIsPerspective = InCameraInfo.ViewInfo.ProjectionMode == ECameraProjectionMode::Type::Perspective;

	// Allow the Engine Showflag system to override our engine showflags, based on our view mode index.
	// This is required for certain debug view modes (to have matching show flags set for rendering).
	ApplyViewMode(/*In*/ ViewModeIndex, bIsPerspective, /*InOut*/ShowFlags);

	// And then we have to let another system override them again (based on cvars, etc.)
	EngineShowFlagOverride(ESFIM_Game, ViewModeIndex, ShowFlags, false);

	TSharedRef<FSceneViewFamilyContext> OutViewFamily = MakeShared<FSceneViewFamilyContext>(FSceneViewFamily::ConstructionValues(
		InInitData.RenderTarget,
		InInitData.World->Scene,
		ShowFlags)
		.SetTime(FGameTime::CreateUndilated(InInitData.TimeData.WorldSeconds, InInitData.TimeData.FrameDeltaTime))
		.SetRealtimeUpdate(true));

	// Need to add the engine-wide view extensions, as rendering code may depend on them (ie: landscapes)
	OutViewFamily->ViewExtensions.Append(GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(InInitData.World->Scene)));

	return OutViewFamily;
}

void FMovieGraphImagePassBase::ApplyMovieGraphOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData) const
{
	// Used to specify if the Tone Curve is being applied or not to our Linear Output data
	InOutFamily->SceneCaptureSource = InInitData.SceneCaptureSource;
	InOutFamily->bWorldIsPaused = InInitData.bWorldIsPaused;
	// InOutFamily->ViewMode = ViewModeIndex;
	InOutFamily->bOverrideVirtualTextureThrottle = true;
	
	// We need to check if this is the first FSceneView being submitted to the renderer module, and set some flags on the ViewFamily for ensuring some
	// parts of the renderer only get updated once per frame. Kept as an if/else statement to avoid the confusion with setting all of these values to 
	// some permutation of !/!!bHasRenderedFirstViewThisFrame.
	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (GraphRenderer)
	{
		if (!GraphRenderer->GetHasRenderedFirstViewThisFrame())
		{
			// Update our renderer
			GraphRenderer->SetHasRenderedFirstViewThisFrame(true);

			InOutFamily->bIsFirstViewInMultipleViewFamily = true;
			InOutFamily->bAdditionalViewFamily = false;
		}
		else
		{
			InOutFamily->bIsFirstViewInMultipleViewFamily = false;
			InOutFamily->bAdditionalViewFamily = true;
		}

		InOutFamily->bIsMultipleViewFamily = true;
	}
	
	// Skip the whole pass if they don't want motion blur.
	if (FMath::IsNearlyZero(InInitData.TimeData.MotionBlurFraction))
	{
		InOutFamily->EngineShowFlags.SetMotionBlur(false);
	}
	

	// ToDo: Let settings modify the ScreenPercentageInterface so third party screen percentages are supported.

	// If UMoviePipelineViewFamilySetting never set a Screen percentage interface we fallback to default.
	if (InOutFamily->GetScreenPercentageInterface() == nullptr)
	{
		InOutFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(*InOutFamily, FLegacyScreenPercentageDriver::GetCVarResolutionFraction()));
	}
}

void FMovieGraphImagePassBase::ApplyMovieGraphOverridesToSceneView(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const
{
	// Override the view's FrameIndex to be based on our progress through the sequence. This greatly increases
	// determinism with things like TAA.
	FSceneView* View = const_cast<FSceneView*>(InOutFamily->Views[0]);
	View->OverrideFrameIndexValue = InInitData.FrameIndex;
	// Each shot should initialize a scene history from scratch so there should be no need to do an extra camera cut flag.
	View->bCameraCut = false; 
	View->AntiAliasingMethod = InInitData.AntiAliasingMethod;
	View->bIsOfflineRender = true;

	// Override the Motion Blur settings since these are controlled by the movie pipeline.
	{
		// We need to inversly scale the target FPS by time dilation to counteract slowmo. If scaling isn't applied then motion blur length
		// stays the same length despite the smaller delta time and the blur ends up too long.
		View->FinalPostProcessSettings.MotionBlurTargetFPS = FMath::RoundToInt(InInitData.TimeData.FrameRate.AsDecimal() / FMath::Max(SMALL_NUMBER, InInitData.TimeData.WorldTimeDilation));
		View->FinalPostProcessSettings.MotionBlurAmount = InInitData.TimeData.MotionBlurFraction;
		View->FinalPostProcessSettings.MotionBlurMax = 100.f;
		View->FinalPostProcessSettings.bOverride_MotionBlurAmount = true;
		View->FinalPostProcessSettings.bOverride_MotionBlurTargetFPS = true;
		View->FinalPostProcessSettings.bOverride_MotionBlurMax = true;

	}

	// Warn the user for invalid setting combinations / enforce hardware limitations
	// Locked Exposure
	const bool bAutoExposureAllowed = true; // IsAutoExposureAllowed(InOutSampleState);
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
		bool bIsPathTracer = InOutFamily->EngineShowFlags.PathTracing;
		bool bWarnJitters = InCameraInfo.ProjectionMatrixJitterAmount.SquaredLength() > SMALL_NUMBER;
		if ((!bIsPathTracer && !bIsSupportedAAMethod) || bWarnJitters)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Orthographic Cameras are only supported with PathTracer or Deferred with FXAA Anti-Aliasing"));
		}
	}

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
		// If we're not using TAA, TSR, or Path Tracing we will apply the View Matrix projection jitter. Normally TAA sets this
		// inside FSceneRenderer::PreVisibilityFrameSetup. Path Tracing does its own anti-aliasing internally.
		bool bApplyProjectionJitter = !bIsOrthographicCamera
			&& !InOutFamily->EngineShowFlags.PathTracing
			&& !IsTemporalAccumulationBasedMethod(View->AntiAliasingMethod);
		if (bApplyProjectionJitter)
		{
			View->ViewMatrices.HackAddTemporalAAProjectionJitter(InCameraInfo.ProjectionMatrixJitterAmount);
		}
	}
}

FMatrix FMovieGraphImagePassBase::CalculateProjectionMatrix(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const
{
	// Calculate a Projection Matrix. This code unfortunately ends up similar to, but not quite the same as FMinimalViewInfo::CalculateProjectionMatrixGivenView
	FMatrix BaseProjMatrix;
	
	// TileSize should respect the actual backbuffer size being used by the render.
	float ViewRectWidth = InCameraInfo.TilingParams.TileSize.X;
	float ViewRectHeight = InCameraInfo.TilingParams.TileSize.Y;

	const float DestAspectRatio = ViewRectWidth / ViewRectHeight;
	const float CameraAspectRatio = InCameraInfo.bAllowCameraAspectRatio ? InCameraInfo.ViewInfo.AspectRatio : DestAspectRatio;
	
	float ViewFOV = InCameraInfo.ViewInfo.FOV;

	// Inflate our FOV to support the overscan 
	ViewFOV = 2.0f * FMath::RadiansToDegrees(FMath::Atan((1.0f + InCameraInfo.OverscanFraction) * FMath::Tan(FMath::DegreesToRadians(ViewFOV * 0.5f))));
	
	if (InCameraInfo.ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		const float YScale = 1.0f / InCameraInfo.ViewInfo.AspectRatio;
		const float OverscanScale = 1.0f + (InCameraInfo.OverscanFraction);

		const float HalfOrthoWidth = (InCameraInfo.ViewInfo.OrthoWidth / 2.0f) * OverscanScale;
		const float ScaledOrthoHeight = (InCameraInfo.ViewInfo.OrthoWidth / 2.0f) * OverscanScale * YScale;

		const float NearPlane = InCameraInfo.ViewInfo.OrthoNearClipPlane;
		const float FarPlane = InCameraInfo.ViewInfo.OrthoFarClipPlane;

		const float ZScale = 1.0f / (FarPlane - NearPlane);
		const float ZOffset = -NearPlane;

		BaseProjMatrix = FReversedZOrthoMatrix(
			HalfOrthoWidth,
			ScaledOrthoHeight,
			ZScale,
			ZOffset
		);
	}
	else
	{
		float XAxisMultiplier;
		float YAxisMultiplier;

		if (InCameraInfo.ViewInfo.bConstrainAspectRatio)
		{
			// If the camera's aspect ratio has a thinner width, then stretch the horizontal fov more than usual to 
			// account for the extra with of (before constraining - after constraining)
			if (InCameraInfo.ViewInfo.AspectRatio < DestAspectRatio)
			{
				const float ConstrainedWidth = ViewRectHeight * InCameraInfo.ViewInfo.AspectRatio;
				XAxisMultiplier = ConstrainedWidth / (float)ViewRectWidth;
				YAxisMultiplier = InCameraInfo.ViewInfo.AspectRatio;
			}
			// Simplified some math here but effectively functions similarly to the above, the unsimplified code would look like:
			// const float ConstrainedHeight = ViewRectWidth / CameraCache.AspectRatio;
			// YAxisMultiplier = (ConstrainedHeight / ViewInitOptions.GetViewRect.Height()) * CameraCache.AspectRatio;
			else
			{
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = ViewRectWidth / ViewRectHeight;
			}
		}
		else
		{
			const EAspectRatioAxisConstraint AspectRatioAxisConstraint = GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint;
			if (((ViewRectWidth > ViewRectHeight) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
			{
				//if the viewport is wider than it is tall
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = ViewRectWidth / ViewRectHeight;
			}
			else
			{
				//if the viewport is taller than it is wide
				XAxisMultiplier = ViewRectHeight / ViewRectWidth;
				YAxisMultiplier = 1.0f;
			}
		}

		const float MinZ = InCameraInfo.ViewInfo.GetFinalPerspectiveNearClipPlane();
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
	}
	
	return BaseProjMatrix;
}

FVector4f FMovieGraphImagePassBase::CalculatePrinciplePointOffsetForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams) const 
{
	// We need our final view parameters to be in the space of [-1,1], including all the tiles.
	// Starting with a single tile, the middle of the tile in offset screen space is:
	FVector2f TilePrincipalPointOffset;

	TilePrincipalPointOffset.X = (float(InTilingParams.TileIndexes.X) + 0.5f - (0.5f * float(InTilingParams.TileCount.X))) * 2.0f;
	TilePrincipalPointOffset.Y = (float(InTilingParams.TileIndexes.Y) + 0.5f - (0.5f * float(InTilingParams.TileCount.Y))) * 2.0f;

	// For the tile size ratio, we have to multiply by (1.0 + overlap) and then divide by tile num
	FVector2D OverlapScale;
	OverlapScale.X = (1.0f + float(2 * InTilingParams.OverlapPad.X) / float(InTilingParams.TileSize.X));
	OverlapScale.Y = (1.0f + float(2 * InTilingParams.OverlapPad.Y) / float(InTilingParams.TileSize.Y));

	TilePrincipalPointOffset.X /= OverlapScale.X;
	TilePrincipalPointOffset.Y /= OverlapScale.Y;

	FVector2D TilePrincipalPointScale;
	TilePrincipalPointScale.X = OverlapScale.X / float(InTilingParams.TileCount.X);
	TilePrincipalPointScale.Y = OverlapScale.Y / float(InTilingParams.TileCount.Y);

	TilePrincipalPointOffset.X *= TilePrincipalPointScale.X;
	TilePrincipalPointOffset.Y *= TilePrincipalPointScale.Y;

	return FVector4f(TilePrincipalPointOffset.X, -TilePrincipalPointOffset.Y, TilePrincipalPointScale.X, TilePrincipalPointScale.Y);
}

void FMovieGraphImagePassBase::ModifyProjectionMatrixForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams, const bool bInOrthographic, FMatrix& InOutProjectionMatrix, float& OutDoFSensorScale) const
{
	float PadRatioX = 1.0f;
	float PadRatioY = 1.0f;

	if (InTilingParams.OverlapPad.X > 0 && InTilingParams.OverlapPad.Y > 0)
	{
		PadRatioX = float(InTilingParams.OverlapPad.X * 2 + InTilingParams.TileSize.X) / float(InTilingParams.TileSize.X);
		PadRatioY = float(InTilingParams.OverlapPad.Y * 2 + InTilingParams.TileSize.Y) / float(InTilingParams.TileSize.Y);
	}

	float ScaleX = PadRatioX / float(InTilingParams.TileCount.X);
	float ScaleY = PadRatioY / float(InTilingParams.TileCount.Y);

	InOutProjectionMatrix.M[0][0] /= ScaleX;
	InOutProjectionMatrix.M[1][1] /= ScaleY;
	OutDoFSensorScale = ScaleX;

	// this offset would be correct with no pad
	float OffsetX = -((float(InTilingParams.TileIndexes.X) + 0.5f - float(InTilingParams.TileCount.X) / 2.0f) * 2.0f);
	float OffsetY = ((float(InTilingParams.TileIndexes.Y) + 0.5f - float(InTilingParams.TileCount.Y) / 2.0f) * 2.0f);

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


void FMovieGraphImagePassBase::PostRendererSubmission(
	const UE::MovieGraph::FMovieGraphSampleState& InSampleState,
	const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo)
{
	// We have a pool of accumulators - we multi-thread the accumulation on the task graph, and for each frame,
	// the task has the previous samples as pre-reqs to keep the accumulation in order. However, each accumulator
	// can only work on one frame at a time, so we create a pool of them to work concurrently. This needs a limit
	// as large accumulations (16k) can take a lot of system RAM.
	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (!GraphRenderer)
	{
		return;
	}

	FMoviePipelineAccumulatorPoolPtr SampleAccumulatorPool = GraphRenderer->GetOrCreateAccumulatorPool<FImageOverlappedAccumulator>();
	UE::MovieGraph::DefaultRenderer::FSurfaceAccumulatorPool::FInstancePtr AccumulatorInstance = SampleAccumulatorPool->GetAccumulatorInstance_GameThread<FImageOverlappedAccumulator>(InSampleState.TraversalContext.Time.OutputFrameNumber, InSampleState.TraversalContext.RenderDataIdentifier);
	
	FMoviePipelineSurfaceQueuePtr LocalSurfaceQueue = GraphRenderer->GetOrCreateSurfaceQueue(InRenderTargetInitParams);
	LocalSurfaceQueue->BlockUntilAnyAvailable();

	FMovieGraphRenderDataAccumulationArgs AccumulationArgs;
	{
		AccumulationArgs.OutputMerger = GraphRenderer->GetOwningGraph()->GetOutputMerger();
		AccumulationArgs.ImageAccumulator = StaticCastSharedPtr<FImageOverlappedAccumulator>(AccumulatorInstance->Accumulator);
		AccumulationArgs.bIsFirstSample = InSampleState.TraversalContext.Time.bIsFirstTemporalSampleForFrame;
		AccumulationArgs.bIsLastSample = InSampleState.TraversalContext.Time.bIsLastTemporalSampleForFrame;
	}

	auto OnSurfaceReadbackFinished = [this, InSampleState, AccumulationArgs, AccumulatorInstance](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		UE::Tasks::TTask<void> Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [PixelData = MoveTemp(InPixelData), InSampleState, AccumulationArgs, AccumulatorInstance]() mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			AccumulateSample_TaskThread(MoveTemp(PixelData), InSampleState, AccumulationArgs);

			// We have to defer clearing the accumulator until after sample accumulation has finished
			if (AccumulationArgs.bIsLastSample)
			{
				// Final sample has now been executed, free the accumulator for reuse.
				AccumulatorInstance->SetIsActive(false);
			}
		}, AccumulatorInstance->TaskPrereq);

		// Make the next accumulation task that uses this accumulator use the task we just created as a pre-req.
		AccumulatorInstance->TaskPrereq = Task;

		// Because we're run on a separate thread, we need to check validity differently. The standard
		// TWeakObjectPtr will report non-valid during GC (even if the object it's pointing to isn't being
		// GC'd).
		const bool bEvenIfPendingKill = false;
		const bool bThreadsafeTest = true;
		const bool bValid = this->WeakGraphRenderer.IsValid(bEvenIfPendingKill, bThreadsafeTest);
		if (ensureMsgf(bValid, TEXT("Renderer was garbage collected while outstanding tasks existed, outstanding tasks were not flushed properly during shutdown!")))
		{
			// The regular Get() will fail during GC so we use the above check to see if it's valid
			// before ignoring it and directly getting the object.
			this->WeakGraphRenderer.GetEvenIfUnreachable()->AddOutstandingRenderTask_AnyThread(Task);
		}
	};

	FRenderTarget* RenderTarget = InCanvas.GetRenderTarget();

	ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
		[LocalSurfaceQueue, OnSurfaceReadbackFinished, RenderTarget](FRHICommandListImmediate& RHICmdList) mutable
		{
			// The legacy surface reader takes the payload just so it can shuffle it into our callback, but we can just include the data
			// directly in the callback, so this is just a dummy payload.
			TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
			LocalSurfaceQueue->OnRenderTargetReady_RenderThread(RenderTarget->GetRenderTargetTexture(), FramePayload, MoveTemp(OnSurfaceReadbackFinished));
		});
}

TFunction<void(TUniquePtr<FImagePixelData>&&)> FMovieGraphImagePassBase::MakeForwardingEndpoint(
	const FMovieGraphSampleState& InSampleState, const FMovieGraphTimeStepData& InTimeData)
{
	// We have a pool of accumulators - we multi-thread the accumulation on the task graph, and for each frame,
	// the task has the previous samples as pre-reqs to keep the accumulation in order. However, each accumulator
	// can only work on one frame at a time, so we create a pool of them to work concurrently. This needs a limit
	// as large accumulations (16k) can take a lot of system RAM.
	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (!GraphRenderer)
	{
		return nullptr;
	}
	
	FMoviePipelineAccumulatorPoolPtr SampleAccumulator = GraphRenderer->GetOrCreateAccumulatorPool<FImageOverlappedAccumulator>();
	UE::MovieGraph::DefaultRenderer::FSurfaceAccumulatorPool::FInstancePtr AccumulatorInstance =
		SampleAccumulator->GetAccumulatorInstance_GameThread<FImageOverlappedAccumulator>(
			InTimeData.RenderedFrameNumber, InSampleState.TraversalContext.RenderDataIdentifier);
	
	FMovieGraphRenderDataAccumulationArgs AccumulationArgs;
	{
		AccumulationArgs.OutputMerger = GraphRenderer->GetOwningGraph()->GetOutputMerger();
		AccumulationArgs.ImageAccumulator = StaticCastSharedPtr<FImageOverlappedAccumulator>(AccumulatorInstance->Accumulator);
		AccumulationArgs.bIsFirstSample = InTimeData.bIsFirstTemporalSampleForFrame;
		AccumulationArgs.bIsLastSample = InTimeData.bIsLastTemporalSampleForFrame;
	}

	// The legacy surface reader takes the payload just so it can shuffle it into our callback, but we can just include the data
	// directly in the callback, so this is just a dummy payload.
	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
	auto Callback = [this, InSampleState, FramePayload, AccumulationArgs, AccumulatorInstance](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		// Transfer the framePayload to the returned data
		TUniquePtr<FImagePixelData> PixelDataWithPayload = nullptr;
		switch (InPixelData->GetType())
		{
		case EImagePixelType::Color:
		{
			TImagePixelData<FColor>* SourceData = static_cast<TImagePixelData<FColor>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FColor>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		case EImagePixelType::Float16:
		{
			TImagePixelData<FFloat16Color>* SourceData = static_cast<TImagePixelData<FFloat16Color>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FFloat16Color>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		case EImagePixelType::Float32:
		{
			TImagePixelData<FLinearColor>* SourceData = static_cast<TImagePixelData<FLinearColor>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FLinearColor>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		default:
			checkNoEntry();
		}

		UE::Tasks::TTask<void> Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [PixelData = MoveTemp(PixelDataWithPayload), InSampleState, AccumulationArgs, AccumulatorInstance]() mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			AccumulateSample_TaskThread(MoveTemp(PixelData), InSampleState, AccumulationArgs);

			// We have to defer clearing the accumulator until after sample accumulation has finished
			if (AccumulationArgs.bIsLastSample)
			{
				// Final sample has now been executed, free the accumulator for reuse.
				AccumulatorInstance->SetIsActive(false);
			}
		}, AccumulatorInstance->TaskPrereq);

		// Make the next accumulation task that uses this accumulator use the task we just created as a pre-req.
		AccumulatorInstance->TaskPrereq = Task;
	};

	return Callback;
}

void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const ::UE::MovieGraph::FMovieGraphSampleState InSampleState, const FMovieGraphRenderDataAccumulationArgs& InAccumulationParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MoviePipeline_AccumulateSample);
		
	TUniquePtr<FImagePixelData> SamplePixelData = MoveTemp(InPixelData);

	// Associate the sample state with the image as payload data, this allows downstream systems to fetch the values without us having to store the data
	// separately and ensure they stay paired the whole way down.
	TSharedPtr<FMovieGraphSampleState> SampleStatePayload = MakeShared<FMovieGraphSampleState>(InSampleState);
	SamplePixelData->SetPayload(StaticCastSharedPtr<IImagePixelDataPayload>(SampleStatePayload));

	TSharedPtr<IMovieGraphOutputMerger, ESPMode::ThreadSafe> OutputMergerPin = InAccumulationParams.OutputMerger.Pin();
	if (!OutputMergerPin.IsValid())
	{
		return;
	}

	const bool bIsWellFormed = SamplePixelData->IsDataWellFormed();
	check(bIsWellFormed);

	if (SampleStatePayload->bWriteSampleToDisk)
	{
		// Debug Feature: Write the raw sample to disk for debugging purposes. We copy the data here,
		// as we don't want to disturb the memory flow below.
		TUniquePtr<FImagePixelData> SampleData = SamplePixelData->CopyImageData();
		OutputMergerPin->OnSingleSampleDataAvailable_AnyThread(MoveTemp(SampleData));
	}

	// Optimization! If we don't need the accumulator (no tiling, no sub-sampling) then we'll skip it and just send it straight to the output stage.
	// This reduces memory requirements and improves performance in the baseline case.
	if (!SampleStatePayload->bRequiresAccumulator)
	{
		OutputMergerPin->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(SamplePixelData));
		return;
	}

	TSharedPtr<FImageOverlappedAccumulator> AccumulatorPin = InAccumulationParams.ImageAccumulator.Pin();
	if (AccumulatorPin->NumChannels == 0)
	{
		LLM_SCOPE_BYNAME(TEXT("MoviePipeline/ImageAccumulatorInitMemory"));
		const int32 ChannelCount = 4;
		AccumulatorPin->InitMemory(SampleStatePayload->AccumulatorResolution, ChannelCount);
		AccumulatorPin->ZeroPlanes();
		AccumulatorPin->AccumulationGamma = 1.f;
	}

	// Accumulate the new sample to our target
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MoviePipeline_AccumulatePixelData);
				
		const FIntPoint TileSize = SampleStatePayload->BackbufferResolution;
		const FIntPoint OverlappedPad = SampleStatePayload->OverlappedPad;
		const FIntPoint OverlappedOffset = SampleStatePayload->OverlappedOffset;
		const FVector2D OverlappedSubpixelShift = SampleStatePayload->OverlappedSubpixelShift;
		::MoviePipeline::FTileWeight1D WeightFunctionX;
		::MoviePipeline::FTileWeight1D WeightFunctionY;
		WeightFunctionX.InitHelper(OverlappedPad.X, TileSize.X, OverlappedPad.X);
		WeightFunctionY.InitHelper(OverlappedPad.Y, TileSize.Y, OverlappedPad.Y);
				
		AccumulatorPin->AccumulatePixelData(*SamplePixelData, OverlappedOffset, OverlappedSubpixelShift, WeightFunctionX, WeightFunctionY);
	}

	// Finally on our last sample, we fetch the data out of the accumulator
	// and move it to the Output Merger.
	if (SampleStatePayload->bFetchFromAccumulator)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MoviePipeline_FetchAccumulatedPixelData);
		int32 FullSizeX = AccumulatorPin->PlaneSize.X;
		int32 FullSizeY = AccumulatorPin->PlaneSize.Y;

		// Now that a tile is fully built and accumulated we can notify the output builder that the
		// data is ready so it can pass that onto the output containers (if needed).
		if (SamplePixelData->GetType() == EImagePixelType::Float32)
		{
			// 32 bit FLinearColor
			TUniquePtr<TImagePixelData<FLinearColor> > FinalPixelData = MakeUnique<TImagePixelData<FLinearColor>>(FIntPoint(FullSizeX, FullSizeY), SampleStatePayload);
			AccumulatorPin->FetchFinalPixelDataLinearColor(FinalPixelData->Pixels);

			// Send the data to the Output Builder
			OutputMergerPin->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
		}
		else if (SamplePixelData->GetType() == EImagePixelType::Float16)
		{
			// 16 bit FLinearColor
			TUniquePtr<TImagePixelData<FFloat16Color> > FinalPixelData = MakeUnique<TImagePixelData<FFloat16Color>>(FIntPoint(FullSizeX, FullSizeY), SampleStatePayload);
			AccumulatorPin->FetchFinalPixelDataHalfFloat(FinalPixelData->Pixels);

			// Send the data to the Output Builder
			OutputMergerPin->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
		}
		else if (SamplePixelData->GetType() == EImagePixelType::Color)
		{
			// 8bit FColors
			TUniquePtr<TImagePixelData<FColor>> FinalPixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint(FullSizeX, FullSizeY), SampleStatePayload);
			AccumulatorPin->FetchFinalPixelDataByte(FinalPixelData->Pixels);

			// Send the data to the Output Builder
			OutputMergerPin->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(FinalPixelData));
		}
		else
		{
			check(0);
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MoviePipeline_FreeAccumulatedPixelData);
			// Free the memory in the accumulator.
			AccumulatorPin->Reset();
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReleaseSampleData);
		SamplePixelData.Reset();
	}
}

} // namespace UE::MovieGraph::Rendering

