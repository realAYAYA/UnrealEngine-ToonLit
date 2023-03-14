// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelinePanoramicPass.h"
#include "MoviePipelineOutputBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneView.h"
#include "MovieRenderPipelineDataTypes.h"
#include "GameFramework/PlayerController.h"
#include "MoviePipelineRenderPass.h"
#include "EngineModule.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget.h"
#include "MoviePipeline.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineShotConfig.h"
#include "MovieRenderOverlappedImage.h"
#include "MovieRenderPipelineCoreModule.h"
#include "ImagePixelData.h"
#include "MoviePipelineOutputBuilder.h"
#include "BufferVisualizationData.h"
#include "Containers/Array.h"
#include "FinalPostProcessSettings.h"
#include "Materials/Material.h"
#include "MoviePipelineCameraSetting.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/RendererSettings.h"
#include "ImageUtils.h"
#include "Math/Quat.h"
#include "Engine/LocalPlayer.h"
#include "MoviePipelinePanoramicBlender.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelinePanoramicPass)

UMoviePipelinePanoramicPass::UMoviePipelinePanoramicPass() 
	: UMoviePipelineImagePassBase()
	, NumHorizontalSteps(8)
	, NumVerticalSteps(3)
	, bStereo(false)
	, EyeSeparation(6.5f)
	, EyeConvergenceDistance(EyeSeparation * 30.f)
	, bAllocateHistoryPerPane(false)
	, bHasWarnedSettings(false)
{
	PassIdentifier = FMoviePipelinePassIdentifier("Panoramic");
}

namespace MoviePipeline
{
	namespace Panoramic
	{
		static TArray<float> DistributeValuesInInterval(float InMin, float InMax, int32 InNumDivisions, bool bInInclusiveMax)
		{
			TArray<float> Results;
			Results.Reserve(bInInclusiveMax);

			float Delta = (InMax - InMin) / static_cast<float>(FMath::Max(bInInclusiveMax ? InNumDivisions - 1 : InNumDivisions, 1));
			float CurrentValue = InMin;
			for (int32 Index = 0; Index < InNumDivisions; Index++)
			{
				Results.Add(CurrentValue);

				CurrentValue += Delta;
			}

			return Results;
		};

		void GetCameraOrientationForStereo(FVector& OutLocation, FRotator& OutRotation, const FPanoPane& InPane, const int32 InStereoIndex, const bool bInPrevPosition)
		{
			// ToDo: This 110 (-55, 55) comes from TwinMotion who uses a hard-coded number of v-steps, may need adjusting.
			const TArray<float> PitchValues = MoviePipeline::Panoramic::DistributeValuesInInterval(-55, 55, InPane.NumVerticalSteps, /*Inclusive Max*/true);
			const TArray<float> YawValues = MoviePipeline::Panoramic::DistributeValuesInInterval(0, 360, InPane.NumHorizontalSteps, /*Inclusive Max*/false);

			const float HorizontalRotationDeg = YawValues[InPane.HorizontalStepIndex];
			const float VerticalRotationDeg = PitchValues[InPane.VerticalStepIndex];

			const FQuat HorizontalRotQuat = FQuat(FVector::UnitZ(), FMath::DegreesToRadians(HorizontalRotationDeg));
			const FQuat VerticalRotQuat = FQuat(FVector::UnitY(), FMath::DegreesToRadians(VerticalRotationDeg));

			const FRotator SourceRot = bInPrevPosition ? InPane.PrevOriginalCameraRotation : InPane.OriginalCameraRotation;
			FQuat RotationResult = FQuat(SourceRot) * HorizontalRotQuat * VerticalRotQuat;
			OutRotation = FRotator(RotationResult);

			// If not using stereo rendering then the eye is just the camera location
			if (InStereoIndex < 0)
			{
				OutLocation = bInPrevPosition ? InPane.PrevOriginalCameraLocation : InPane.OriginalCameraLocation;
			}
			else
			{
				check(InStereoIndex == 0 || InStereoIndex == 1);

				float EyeOffset = InStereoIndex == 0 ? (-InPane.EyeSeparation / 2.f) : (InPane.EyeSeparation / 2.f);
				OutLocation = bInPrevPosition ? InPane.PrevOriginalCameraLocation : InPane.OriginalCameraLocation;

				// Translate the eye either left or right of the target rotation.
				OutLocation += RotationResult.RotateVector(FVector(0.f, EyeOffset, 0.f));
			}
		}
	}
}

void UMoviePipelinePanoramicPass::MoviePipelineRenderShowFlagOverride(FEngineShowFlags& OutShowFlag)
{
	// Panoramics can't support any of these.
	OutShowFlag.SetVignette(false);
	OutShowFlag.SetSceneColorFringe(false);
	OutShowFlag.SetPhysicalMaterialMasks(false);

	/*if(bPathTracer)
	{
		OutShowFlag.SetPathTracing(true);
		OutViewModeIndex = EViewModeIndex::VMI_PathTracing;
	}*/
	// OutShowFlag.SetBloom(false); ToDo: Does bloom work?
}

void UMoviePipelinePanoramicPass::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);
	// LLM_SCOPE_BYNAME(TEXT("MoviePipeline/PanoBlendSetup"));

	const FIntPoint PaneResolution = GetPaneResolution(InPassInitSettings.BackbufferResolution);

	// Re-initialize the render target and surface queue
	GetOrCreateViewRenderTarget(PaneResolution);
	GetOrCreateSurfaceQueue(PaneResolution);

	int32 StereoMultiplier = bStereo ? 2 : 1;
	int32 NumPanes = NumHorizontalSteps * NumVerticalSteps;
	int32 NumPanoramicPanes = NumPanes * StereoMultiplier;
	if (bAllocateHistoryPerPane)
	{
		OptionalPaneViewStates.SetNum(NumPanoramicPanes);

		for (int32 Index = 0; Index < OptionalPaneViewStates.Num(); Index++)
		{
			OptionalPaneViewStates[Index].Allocate(InPassInitSettings.FeatureLevel);
		}
	}

	// We need one accumulator per pano tile if using accumulation.
	AccumulatorPool = MakeShared<TAccumulatorPool<FImageOverlappedAccumulator>, ESPMode::ThreadSafe>(NumPanoramicPanes);

	// Create a class to blend the Panoramic Panes into equirectangular maps. When all of the samples for a given
	// Panorama are provided to the Blender and it is blended, it will pass the data onto the normal OutputBuilder
	// who is none the wiser that we're handing it complex blended images instead of normal stills.
	PanoramicOutputBlender = MakeShared<FMoviePipelinePanoramicBlender>(GetPipeline()->OutputBuilder, InPassInitSettings.BackbufferResolution);

	// Allocate an OCIO extension to do color grading if needed.
	OCIOSceneViewExtension = FSceneViewExtensions::NewExtension<FOpenColorIODisplayExtension>();
	bHasWarnedSettings = false;
}

void UMoviePipelinePanoramicPass::TeardownImpl()
{
	PanoramicOutputBlender.Reset();
	AccumulatorPool.Reset();

	for (int32 Index = 0; Index < OptionalPaneViewStates.Num(); Index++)
	{
		FSceneViewStateInterface* Ref = OptionalPaneViewStates[Index].GetReference();
		if (Ref)
		{
			Ref->ClearMIDPool();
		}
		OptionalPaneViewStates[Index].Destroy();
	}
	OptionalPaneViewStates.Reset();
	
	OCIOSceneViewExtension.Reset();
	OCIOSceneViewExtension = nullptr;

	Super::TeardownImpl();
}

void UMoviePipelinePanoramicPass::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMoviePipelinePanoramicPass& This = *CastChecked<UMoviePipelinePanoramicPass>(InThis);
	for (int32 Index = 0; Index < This.OptionalPaneViewStates.Num(); Index++)
	{
		FSceneViewStateInterface* Ref = This.OptionalPaneViewStates[Index].GetReference();
		if (Ref)
		{
			Ref->AddReferencedObjects(Collector);
		}
	}
}

FSceneViewStateInterface* UMoviePipelinePanoramicPass::GetSceneViewStateInterface(IViewCalcPayload* OptPayload)
{
	check(OptPayload);
	FPanoPane* PanoPane = (FPanoPane*)OptPayload;

	if (bAllocateHistoryPerPane)
	{
		return OptionalPaneViewStates[PanoPane->GetAbsoluteIndex()].GetReference();
	}
	else
	{
		return nullptr; // Super::GetSceneViewStateInterface(OptPayload);
	}
}

void UMoviePipelinePanoramicPass::GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
{
	// Despite rendering many views, we only output one image total, which is covered by the base.
	Super::GatherOutputPassesImpl(ExpectedRenderPasses);
}

void UMoviePipelinePanoramicPass::AddViewExtensions(FSceneViewFamilyContext& InContext, FMoviePipelineRenderPassMetrics& InOutSampleState)
{
	// OCIO Scene View Extension is a special case and won't be registered like other view extensions.
	if (InOutSampleState.OCIOConfiguration && InOutSampleState.OCIOConfiguration->bIsEnabled)
	{
		FOpenColorIODisplayConfiguration* OCIOConfigNew = const_cast<FMoviePipelineRenderPassMetrics&>(InOutSampleState).OCIOConfiguration;
		FOpenColorIODisplayConfiguration& OCIOConfigCurrent = OCIOSceneViewExtension->GetDisplayConfiguration();

		// We only need to set this once per render sequence.
		if (OCIOConfigNew->ColorConfiguration.ConfigurationSource && OCIOConfigNew->ColorConfiguration.ConfigurationSource != OCIOConfigCurrent.ColorConfiguration.ConfigurationSource)
		{
			OCIOSceneViewExtension->SetDisplayConfiguration(*OCIOConfigNew);
		}

		InContext.ViewExtensions.Add(OCIOSceneViewExtension.ToSharedRef());
	}
}

FIntPoint UMoviePipelinePanoramicPass::GetPaneResolution(const FIntPoint& InSize) const
{
	// We calculate a different resolution than the final output resolution.
	float HorizontalFoV;
	float VerticalFoV;
	GetFieldOfView(HorizontalFoV, VerticalFoV, bStereo);

	// Horizontal FoV is a proportion of the global horizontal resolution
	// ToDo: We might have to check which is higher, if numVerticalPanes > numHorizontalPanes this math might be backwards.
	float HorizontalRes = (HorizontalFoV / 360.0f) * InSize.X;
	float Intermediate = FMath::Tan(FMath::DegreesToRadians(VerticalFoV) * 0.5f) / FMath::Tan(FMath::DegreesToRadians(HorizontalFoV) * 0.5f);
	float VerticalRes = HorizontalRes * Intermediate;

	//float VerticalRes = HorizontalRes * FMath::Tan(FMath::DegreesToRadians(VerticalFoV) * 0.5f) / FMath::Tan(FMath::DegreesToRadians(HorizontalFoV) * 0.5f);
	// 
	// ToDo: I think this is just aspect ratio based on FoVs?

	return FIntPoint(FMath::CeilToInt(HorizontalRes), FMath::CeilToInt(VerticalRes));
}

void UMoviePipelinePanoramicPass::GetFieldOfView(float& OutHorizontal, float& OutVertical, const bool bInStereo) const
{
	// ToDo: These should probably be mathematically derived based on numSteps
	OutHorizontal = bInStereo ? 30.f : HorzFieldOfView > 0 ? HorzFieldOfView : 90.f;
	OutVertical = bInStereo ? 80.f : VertFieldOfView > 0 ? VertFieldOfView : 90.f;
}

FSceneView* UMoviePipelinePanoramicPass::GetSceneViewForSampleState(FSceneViewFamily* ViewFamily, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	check(OptPayload);
	FPanoPane* PanoPane = (FPanoPane*)OptPayload;

	//We skip calling the Super::GetSceneViewForSampleState entirely, as this has highly customized logic for Panos.
	APlayerController* LocalPlayerController = GetPipeline()->GetWorld()->GetFirstPlayerController();

	// We ignore the resolution that comes from the main render job as that will be our output resolution.
	int32 PaneSizeX = PanoPane->Resolution.X;
	int32 PaneSizeY = PanoPane->Resolution.Y;

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.ViewOrigin = PanoPane->CameraLocation;
	ViewInitOptions.SetViewRectangle(FIntRect(FIntPoint(0, 0), FIntPoint(PaneSizeX, PaneSizeY)));
	ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(FRotator(PanoPane->CameraRotation));
	ViewInitOptions.ViewActor = LocalPlayerController ? LocalPlayerController->GetViewTarget() : nullptr;

	// Rotate the view 90 degrees (reason: unknown)
	ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	float ViewFOV = PanoPane->HorizontalFieldOfView;
	// Inflate our FOV to support the overscan  
	// Overscan not supported right now.
	// ViewFOV = 2.0f * FMath::RadiansToDegrees(FMath::Atan((1.0f + InOutSampleState.OverscanPercentage) * FMath::Tan(FMath::DegreesToRadians(ViewFOV * 0.5f))));

	float DofSensorScale = 1.0f;

	// Calculate a Projection Matrix
	{
		float MinZ = GNearClippingPlane;
		if (LocalPlayerController && LocalPlayerController->PlayerCameraManager)
		{
			float NearClipPlane = LocalPlayerController->PlayerCameraManager->GetCameraCacheView().PerspectiveNearClipPlane;
			MinZ = NearClipPlane > 0 ? NearClipPlane : MinZ;
		}
		PanoPane->NearClippingPlane = MinZ;
		const float MaxZ = MinZ;
		// Avoid zero ViewFOV's which cause divide by zero's in projection matrix
		const float MatrixFOV = FMath::Max(0.001f, ViewFOV) * (float)PI / 360.0f;
		// ToDo: I think this is a FMath::DegreesToRadians, easier to read that way than PI/360

		static_assert((int32)ERHIZBuffer::IsInverted != 0, "ZBuffer should be inverted");

		float XAxisMultiplier = 1.f;
		float YAxisMultiplier = 1.f;
		if (PaneSizeX > PaneSizeY)
		{
			// Viewport is wider than it is tall
			YAxisMultiplier = PaneSizeX / (float)PaneSizeY;
		}
		else
		{
			// Viewport is taller than wide
			XAxisMultiplier = PaneSizeY / (float)PaneSizeX;
		}
		FMatrix BaseProjMatrix = FReversedZPerspectiveMatrix(
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				MinZ,
				MaxZ
			);
		
		// If doing a stereo render, the matrix needs to be offset.
		if (PanoPane->EyeIndex >= 0)
		{
			float HalfEyeOffset = PanoPane->EyeSeparation * 0.5f;

			// ToDo: Left eye uses a positive offset and right eye uses a negative? Seems backwards.
			float ProjectionOffset = PanoPane->EyeIndex == 0
				? HalfEyeOffset / PanoPane->EyeConvergenceDistance
				: -HalfEyeOffset / PanoPane->EyeConvergenceDistance;
			BaseProjMatrix.M[2][0] = ProjectionOffset;

		}

		// Modify the perspective matrix to do an off center projection, with overlap for high-res tiling
		// ToDo: High-res not supported, use more horizontal/vertical segments instead.
		// ModifyProjectionMatrixForTiling(InOutSampleState, /*InOut*/ BaseProjMatrix, /*Out*/DofSensorScale);

		ViewInitOptions.ProjectionMatrix = BaseProjMatrix;
	}

	ViewInitOptions.SceneViewStateInterface = GetSceneViewStateInterface(OptPayload);
	ViewInitOptions.FOV = ViewFOV;

	FSceneView* View = new FSceneView(ViewInitOptions);
	ViewFamily->Views.Add(View);
	View->ViewLocation = ViewInitOptions.ViewOrigin;
	View->ViewRotation = PanoPane->CameraRotation;
	// Override previous/current view transforms so that tiled renders don't use the wrong occlusion/motion blur information.
	View->PreviousViewTransform = FTransform(PanoPane->PrevCameraRotation, PanoPane->PrevCameraLocation);

	View->StartFinalPostprocessSettings(View->ViewLocation);
	BlendPostProcessSettings(View, InOutSampleState, OptPayload);

	// Scaling sensor size inversely with the the projection matrix [0][0] should physically
	// cause the circle of confusion to be unchanged.
	View->FinalPostProcessSettings.DepthOfFieldSensorWidth *= DofSensorScale;

	// Modify the 'center' of the lens to be offset for high-res tiling, helps some effects (vignette) etc. still work.
	// ToDo: Highres Tiling support
	// View->LensPrincipalPointOffsetScale = CalculatePrinciplePointOffsetForTiling(InOutSampleState);

	View->EndFinalPostprocessSettings(ViewInitOptions);

	// ToDo: Re-inject metadata

	return View;
}

FIntPoint UMoviePipelinePanoramicPass::GetPayloadPaneResolution(const FIntPoint& InSize, IViewCalcPayload* OptPayload) const
{
	if (OptPayload)
	{
		FPanoPane* PanoPane = (FPanoPane*)OptPayload;
		return PanoPane->Resolution;
	}

	return InSize;
}

TWeakObjectPtr<UTextureRenderTarget2D> UMoviePipelinePanoramicPass::GetOrCreateViewRenderTarget(const FIntPoint& InSize, IViewCalcPayload* OptPayload)
{
	return Super::GetOrCreateViewRenderTarget(GetPayloadPaneResolution(InSize, OptPayload), OptPayload);
}

TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> UMoviePipelinePanoramicPass::GetOrCreateSurfaceQueue(const FIntPoint& InSize, IViewCalcPayload* OptPayload)
{
	return Super::GetOrCreateSurfaceQueue(GetPayloadPaneResolution(InSize, OptPayload), OptPayload);
}

void UMoviePipelinePanoramicPass::RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState)
{
	// Wait for a surface to be available to write to. This will stall the game thread while the RHI/Render Thread catch up.
	Super::RenderSample_GameThreadImpl(InSampleState);

	const FIntPoint PaneResolution = GetPaneResolution(InSampleState.BackbufferSize);
	
	// This function will be called for each sample of every tile. We'll then submit renders for each
	// Panoramic Pane. This effectively means that all panes will fill in their top left corners first.
	// The accumulation stage will sort out waiting for all the high-res tiles to come in, and once those
	// are all put together into one-image-per-pane, then we can blend them together.
	
	// For each vertical segment
	for(int32 VerticalStepIndex = 0; VerticalStepIndex < NumVerticalSteps; VerticalStepIndex++)
	{
		// For each horizontal segment
		for(int32 HorizontalStepIndex = 0; HorizontalStepIndex < NumHorizontalSteps; HorizontalStepIndex++)
		{
			// For each eye
			int32 NumEyeRenders = bStereo ? 2 : 1;
			for (int32 EyeLoopIndex = 0; EyeLoopIndex < NumEyeRenders; EyeLoopIndex++)
			{
				FMoviePipelineRenderPassMetrics InOutSampleState = InSampleState;

				// Construct a pane that will be attached to the render information
				FPanoPane Pane;
				Pane.OriginalCameraLocation = InSampleState.FrameInfo.CurrViewLocation;
				Pane.PrevOriginalCameraLocation = InSampleState.FrameInfo.PrevViewLocation;
				Pane.OriginalCameraRotation = InSampleState.FrameInfo.CurrViewRotation;
				Pane.PrevOriginalCameraRotation = InSampleState.FrameInfo.PrevViewRotation;

				int32 StereoIndex = bStereo ? EyeLoopIndex : -1;
				Pane.EyeIndex = StereoIndex;
				Pane.VerticalStepIndex = VerticalStepIndex;
				Pane.HorizontalStepIndex = HorizontalStepIndex;
				Pane.NumHorizontalSteps = NumHorizontalSteps;
				Pane.NumVerticalSteps = NumVerticalSteps;
				Pane.EyeSeparation = EyeSeparation;
				Pane.EyeConvergenceDistance = EyeConvergenceDistance;

				// Get the actual camera location/rotation for this particular pane, the above values are from the global camera.
				MoviePipeline::Panoramic::GetCameraOrientationForStereo(/*Out*/ Pane.CameraLocation, /*Out*/ Pane.CameraRotation, Pane, StereoIndex, /*bInPrevPos*/ false);
				MoviePipeline::Panoramic::GetCameraOrientationForStereo(/*Out*/ Pane.PrevCameraLocation, /*Out*/ Pane.PrevCameraRotation, Pane, StereoIndex, /*bInPrevPos*/ true);

				GetFieldOfView(Pane.HorizontalFieldOfView, Pane.VerticalFieldOfView, bStereo);

				// Copy the backbufffer size we actually allocated the texture at into the Pane, instead of using
				// the global output resolution, which is the final image size.
				Pane.Resolution = PaneResolution;

				// Create the View Family for this render. This will only contain one view to better fit into the existing
				// architecture of MRQ we have. Calculating the view family requires calculating the FSceneView itself which
				// is highly customized for panos. So we provide the FPanoPlane as 'raw' data that gets passed along so we can
				// use it when we calculate our individual view.
				TSharedPtr<FSceneViewFamilyContext> ViewFamily = CalculateViewFamily(InOutSampleState, &Pane);

				EAntiAliasingMethod AAMethod = ViewFamily->Views[0]->AntiAliasingMethod;
				const bool bRequiresHistory = (AAMethod == EAntiAliasingMethod::AAM_TemporalAA) || (AAMethod == EAntiAliasingMethod::AAM_TSR);
				if (!bAllocateHistoryPerPane && bRequiresHistory)
				{
					if (!bHasWarnedSettings)
					{
						bHasWarnedSettings = true;
						UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Panoramic Renders do not support TAA without enabling bAllocateHistoryPerPane! Forcing AntiAliasing off."));
					}
					
					FSceneView* NonConstView = const_cast<FSceneView*>(ViewFamily->Views[0]);
					NonConstView->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
				}

				// Submit the view to be rendered.
				TWeakObjectPtr<UTextureRenderTarget2D> ViewRenderTarget = GetOrCreateViewRenderTarget(PaneResolution);
				check(ViewRenderTarget.IsValid());

				FRenderTarget* RenderTarget = ViewRenderTarget->GameThread_GetRenderTargetResource();
				check(RenderTarget);

				FCanvas Canvas = FCanvas(RenderTarget, nullptr, GetPipeline()->GetWorld(), ViewFamily->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
				GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

				// Schedule a readback and then high-res accumulation.
				ScheduleReadbackAndAccumulation(InOutSampleState, Pane, Canvas);
			}
		}
	}
}

void UMoviePipelinePanoramicPass::ScheduleReadbackAndAccumulation(const FMoviePipelineRenderPassMetrics& InSampleState, const FPanoPane& InPane, FCanvas& InCanvas)
{
	// If this was just to contribute to the history buffer, no need to go any further.
	if (InSampleState.bDiscardResult)
	{
		return;
	}

	// We have a pool of accumulators - we multi-thread the accumulation on the task graph, and for each frame,
	// the task has the previous samples as pre-reqs to keep the accumulation in order. However, each accumulator
	// can only work on one frame at a time, so we create a pool of them to work concurrently. This needs a limit
	// as large accumulations (16k) can take a lot of system RAM.
	TSharedPtr<FAccumulatorPool::FAccumulatorInstance, ESPMode::ThreadSafe> SampleAccumulator = nullptr;
	{
		SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_WaitForAvailableAccumulator);
		
		// Generate a unique PassIdentifier for this Panoramic Pane. High Res tile isn't taken into account because
		// the same accumulator is used for all tiles.
		FMoviePipelinePassIdentifier PanePassIdentifier = FMoviePipelinePassIdentifier(FString::Printf(TEXT("%s_x%d_y%d"), *PassIdentifier.Name, InPane.VerticalStepIndex, InPane.HorizontalStepIndex));
		SampleAccumulator = AccumulatorPool->BlockAndGetAccumulator_GameThread(InSampleState.OutputState.OutputFrameNumber, PanePassIdentifier);
	}

	TSharedRef<FPanoramicImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FPanoramicImagePixelDataPayload, ESPMode::ThreadSafe>();
	FramePayload->PassIdentifier = PassIdentifier;
	FramePayload->SampleState = InSampleState;
	FramePayload->SortingOrder = GetOutputFileSortingOrder();
	FramePayload->Pane = InPane;

	if (FramePayload->Pane.EyeIndex >= 0)
	{
		FramePayload->Debug_OverrideFilename = FString::Printf(TEXT("/%s_SS_%d_TS_%d_TileX_%d_TileY_%d_PaneX_%d_PaneY_%d_Eye_%d.%d.exr"),
			*FramePayload->PassIdentifier.Name, FramePayload->SampleState.SpatialSampleIndex, FramePayload->SampleState.TemporalSampleIndex,
			FramePayload->SampleState.TileIndexes.X, FramePayload->SampleState.TileIndexes.Y, FramePayload->Pane.HorizontalStepIndex,
			FramePayload->Pane.VerticalStepIndex, FramePayload->Pane.EyeIndex, FramePayload->SampleState.OutputState.OutputFrameNumber);
	}
	else
	{
		FramePayload->Debug_OverrideFilename = FString::Printf(TEXT("/%s_SS_%d_TS_%d_TileX_%d_TileY_%d_PaneX_%d_PaneY_%d.%d.exr"),
			*FramePayload->PassIdentifier.Name, FramePayload->SampleState.SpatialSampleIndex, FramePayload->SampleState.TemporalSampleIndex,
			FramePayload->SampleState.TileIndexes.X, FramePayload->SampleState.TileIndexes.Y, FramePayload->Pane.HorizontalStepIndex,
			FramePayload->Pane.VerticalStepIndex, FramePayload->SampleState.OutputState.OutputFrameNumber);
	}
	
	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> LocalSurfaceQueue = GetOrCreateSurfaceQueue(InSampleState.BackbufferSize, (IViewCalcPayload*)(&FramePayload->Pane));

	MoviePipeline::FImageSampleAccumulationArgs AccumulationArgs;
	{
		AccumulationArgs.OutputMerger = PanoramicOutputBlender;
		AccumulationArgs.ImageAccumulator = StaticCastSharedPtr<FImageOverlappedAccumulator>(SampleAccumulator->Accumulator);
		AccumulationArgs.bAccumulateAlpha = bAccumulatorIncludesAlpha;
	}

	auto Callback = [this, FramePayload, AccumulationArgs, SampleAccumulator](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		bool bFinalSample = FramePayload->IsLastTile() && FramePayload->IsLastTemporalSample();

		FMoviePipelineBackgroundAccumulateTask Task;
		// There may be other accumulations for this accumulator which need to be processed first
		Task.LastCompletionEvent = SampleAccumulator->TaskPrereq;

		FGraphEventRef Event = Task.Execute([PixelData = MoveTemp(InPixelData), AccumulationArgs, bFinalSample, SampleAccumulator]() mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			MoviePipeline::AccumulateSample_TaskThread(MoveTemp(PixelData), AccumulationArgs);
			if (bFinalSample)
			{
				// Final sample has now been executed, break the pre-req chain and free the accumulator for reuse.
				SampleAccumulator->bIsActive = false;
				SampleAccumulator->TaskPrereq = nullptr;
			}
		});
		SampleAccumulator->TaskPrereq = Event;

		this->OutstandingTasks.Add(Event);
	};

	FRenderTarget* RenderTarget = InCanvas.GetRenderTarget();

	ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
		[LocalSurfaceQueue, FramePayload, Callback, RenderTarget](FRHICommandListImmediate& RHICmdList) mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			LocalSurfaceQueue->OnRenderTargetReady_RenderThread(RenderTarget->GetRenderTargetTexture(), FramePayload, MoveTemp(Callback));
		});

}
