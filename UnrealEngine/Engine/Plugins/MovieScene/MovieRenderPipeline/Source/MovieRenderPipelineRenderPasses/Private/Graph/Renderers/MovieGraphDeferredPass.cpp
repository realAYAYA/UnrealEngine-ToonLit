// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Renderers/MovieGraphDeferredPass.h"
#include "Graph/Nodes/MovieGraphDeferredPassNode.h"
#include "Graph/Nodes/MovieGraphCameraNode.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphRenderLayerSubsystem.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "MovieRenderOverlappedImage.h"
#include "MoviePipelineSurfaceReader.h"

#include "EngineModule.h"
#include "SceneManagement.h"
#include "CanvasTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LegacyScreenPercentageDriver.h"
#include "Materials/MaterialInterface.h"
#include "OpenColorIODisplayExtension.h"
#include "TextureResource.h"
#include "Tasks/Task.h"
#include "SceneView.h"

// For the 1D Weight table for accumulation
#include "MovieRenderPipelineDataTypes.h"
// For Sub-Pixel Jitter
#include "MoviePipelineUtils.h"

namespace UE::MovieGraph::Rendering
{

void FMovieGraphDeferredPass::Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer)
{
	FMovieGraphImagePassBase::Setup(InRenderer, InRenderPassNode, InLayer);
	
	LayerData = InLayer;


	RenderDataIdentifier.RootBranchName = LayerData.BranchName;
	RenderDataIdentifier.LayerName = LayerData.LayerName;
	RenderDataIdentifier.RendererName = InRenderPassNode->GetRendererName();
	RenderDataIdentifier.SubResourceName = TEXT("beauty");
	
	UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo = InRenderer->GetCameraInfo(LayerData.CameraIdentifier);
	RenderDataIdentifier.CameraName =  CameraInfo.CameraName;

	SceneViewState.Allocate(InRenderer->GetWorld()->GetFeatureLevel());
}

void FMovieGraphDeferredPass::Teardown()
{
	FSceneViewStateInterface* SceneViewRef = SceneViewState.GetReference();
	if (SceneViewRef)
	{
		SceneViewRef->ClearMIDPool();
	}
	SceneViewState.Destroy();
}

void FMovieGraphDeferredPass::GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const
{
	FMovieGraphImagePassBase::GatherOutputPasses(InConfig,OutExpectedPasses);
	
	// Add our pre-calculated identifier
	OutExpectedPasses.Add(RenderDataIdentifier);

	if (const UMovieGraphImagePassBaseNode* ParentNode = GetParentNode(InConfig))
	{
		for (const FMoviePipelinePostProcessPass& AdditionalPass : ParentNode->GetAdditionalPostProcessMaterials())
		{
			if (AdditionalPass.bEnabled)
			{
				UMaterialInterface* Material = AdditionalPass.Material.LoadSynchronous();
				if (Material)
				{
					FMovieGraphRenderDataIdentifier Identifier = RenderDataIdentifier;
					Identifier.SubResourceName = Material->GetName();
					OutExpectedPasses.Add(Identifier);
				}
			}
		}
	}
}

void FMovieGraphDeferredPass::AddReferencedObjects(FReferenceCollector& Collector)
{
	FSceneViewStateInterface* SceneViewRef = SceneViewState.GetReference();
	if (SceneViewRef)
	{
		SceneViewRef->AddReferencedObjects(Collector);
	}
}

FName FMovieGraphDeferredPass::GetBranchName() const
{
	return LayerData.BranchName;
}

UMovieGraphImagePassBaseNode* FMovieGraphDeferredPass::GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const
{
	// This is a bit of a workaround for the fact that the pass doesn't have a strong pointer to the node it's supposed to be associated with,
	// since that instance changes every frame. So instead we have a virtual function here so the node can look it up by type, and then we can
	// call a bunch of virtual functions on the right instance to fetch values.
	const bool bIncludeCDOs = true;
	UMovieGraphDeferredRenderPassNode* ParentNode = InConfig->GetSettingForBranch<UMovieGraphDeferredRenderPassNode>(GetBranchName(), bIncludeCDOs);
	if (!ensureMsgf(ParentNode, TEXT("DeferredPass should not exist without parent node in graph.")))
	{
		return nullptr;
	}

	return ParentNode;
}

void FMovieGraphDeferredPass::Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData)
{
	// ToDo: InFrameTraversalContext includes a copy of TimeData, but may be the one cached at the first temporal sample,
	// maybe we can combine, maybe we can't?
	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (!GraphRenderer)
	{
		return;
	}

	UMovieGraphImagePassBaseNode* ParentNodeThisFrame = GetParentNode(InTimeData.EvaluatedConfig);
	const bool bWriteAllSamples = ParentNodeThisFrame->GetWriteAllSamples();
	const bool bIsRenderingState = InFrameTraversalContext.Shot->ShotInfo.State == EMovieRenderShotState::Rendering;
	int32 NumSpatialSamples = FMath::Max(1, bIsRenderingState ? ParentNodeThisFrame->GetNumSpatialSamples() : ParentNodeThisFrame->GetNumSpatialSamplesDuringWarmUp());

	const ESceneCaptureSource SceneCaptureSource = ParentNodeThisFrame->GetDisableToneCurve() ? ESceneCaptureSource::SCS_FinalColorHDR : ESceneCaptureSource::SCS_FinalToneCurveHDR;
	const EAntiAliasingMethod AntiAliasingMethod = ParentNodeThisFrame->GetAntiAliasingMethod();
	float OverscanFraction = 0.f;
	const float TileOverlapPadRatio = 0.0f; // No tiling support right now

	// Camera nodes are optional
	const bool bIncludeCDOs = false;
	const UMovieGraphCameraSettingNode* CameraNode = InTimeData.EvaluatedConfig->GetSettingForBranch<UMovieGraphCameraSettingNode>(LayerData.BranchName, bIncludeCDOs);
	if (CameraNode)
	{
		OverscanFraction = FMath::Clamp(CameraNode->OverscanPercentage / 100.f, 0.f, 1.f);
	}
	
	FIntPoint AccumulatorResolution = UMovieGraphBlueprintLibrary::GetEffectiveOutputResolution(InTimeData.EvaluatedConfig);
	// ToDo: When tiling is used, this should be the size of the per-tile backbuffer
	FIntPoint BackbufferResolution = AccumulatorResolution;
	// ToDo: This math probably needs the per-tile, pre-overlapped size? 
	FIntPoint OverlappedPad = FIntPoint(FMath::CeilToInt(BackbufferResolution.X * TileOverlapPadRatio), FMath::CeilToInt(BackbufferResolution.Y * TileOverlapPadRatio));
	// Calculate a backbuffer
	UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams RenderTargetInitParams;
	{
		RenderTargetInitParams.Size = BackbufferResolution;

		// OCIO: Since this is a manually created Render target we don't need Gamma to be applied.
		// We use this render target to render to via a display extension that utilizes Display Gamma
		// which has a default value of 2.2 (DefaultDisplayGamma), therefore we need to set Gamma on this render target to 2.2 to cancel out any unwanted effects.
		RenderTargetInitParams.TargetGamma = FOpenColorIORendering::DefaultDisplayGamma;
		RenderTargetInitParams.PixelFormat = EPixelFormat::PF_FloatRGBA;
	}

	UTextureRenderTarget2D* RenderTarget = GraphRenderer->GetOrCreateViewRenderTarget(RenderTargetInitParams, RenderDataIdentifier);
	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	check(RenderTargetResource);

	// Now we can actually construct our ViewFamily, SceneView, and submit it for Rendering + Readback
	for(int32 SpatialIndex = 0; SpatialIndex < NumSpatialSamples; SpatialIndex++)
	{
		// World should be paused for every spatial sample except the last one, so that
		// the view doesn't update histories until the end, allowing us to render the same
		// scene multiple times.
		const bool bWorldIsPaused = SpatialIndex < (NumSpatialSamples - 1);
		const int32 FrameIndex = InTimeData.RenderedFrameNumber * ((InTimeData.TemporalSampleCount * NumSpatialSamples) + (InTimeData.TemporalSampleIndex * NumSpatialSamples)) + SpatialIndex;

		// We only allow a spatial jitter if we have more than one sample
		FVector2f SpatialShiftAmount = FVector2f(0.f, 0.f);
		const bool bAntiAliasingAllowsJitter = AntiAliasingMethod == EAntiAliasingMethod::AAM_None;
		const bool bSampleCountsAllowsJitter = NumSpatialSamples > 1 || InTimeData.TemporalSampleCount > 1;
		if (bAntiAliasingAllowsJitter && bSampleCountsAllowsJitter)
		{
			const int32 NumSamplesPerOutputFrame = NumSpatialSamples * InTimeData.TemporalSampleCount;
			SpatialShiftAmount = UE::MoviePipeline::GetSubPixelJitter(FrameIndex, NumSamplesPerOutputFrame);
		}
		 
		// These are the parameters of our camera 
		UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo;

		// ToDo: Get this from the renderer based on LayerData.CameraIdentifier for eventual multi-camera support
		APlayerController* LocalPlayerController = GraphRenderer->GetWorld()->GetFirstPlayerController();
		// CameraAnim override
		if (LocalPlayerController->PlayerCameraManager)
		{
			CameraInfo.ViewInfo = LocalPlayerController->PlayerCameraManager->GetCameraCacheView();
			CameraInfo.ViewActor = LocalPlayerController->GetViewTarget();
		}


		CameraInfo.bAllowCameraAspectRatio = true;
		CameraInfo.TilingParams.TileSize = BackbufferResolution;
		CameraInfo.TilingParams.OverlapPad = FVector2f(0.f, 0.f); // No tile support
		CameraInfo.TilingParams.TileCount = FIntPoint(1, 1); // No tile support
		CameraInfo.TilingParams.TileIndexes = FIntPoint(0, 0); // No tile support
		CameraInfo.SamplingParams.TemporalSampleIndex = InTimeData.TemporalSampleIndex;
		CameraInfo.SamplingParams.TemporalSampleCount = InTimeData.TemporalSampleCount;
		CameraInfo.SamplingParams.SpatialSampleIndex = SpatialIndex;
		CameraInfo.SamplingParams.SpatialSampleCount = NumSpatialSamples;
		CameraInfo.OverscanFraction = OverscanFraction;
		CameraInfo.ProjectionMatrixJitterAmount = FVector2D((SpatialShiftAmount.X) * 2.0f / (float)BackbufferResolution.X, SpatialShiftAmount.Y * -2.0f / (float)BackbufferResolution.Y);

		// For this particular tile, what is the offset into the output image
		FIntPoint OverlappedOffset = FIntPoint(CameraInfo.TilingParams.TileIndexes.X * BackbufferResolution.X - OverlappedPad.X, CameraInfo.TilingParams.TileIndexes.Y * BackbufferResolution.Y - OverlappedPad.Y);
		
		// Move the final render by this much in the accumulator to counteract the offset put into the view matrix.
		// Note that when bAllowSpatialJitter is false, SpatialShiftX/Y will always be zero.
		FVector2D OverlappedSubpixelShift = FVector2D(0.5f - SpatialShiftAmount.X, 0.5f - SpatialShiftAmount.Y);
		
		FMatrix ProjectionMatrix = CalculateProjectionMatrix(CameraInfo);
		float DoFSensorScale = 1.f;

		// Modify the perspective matrix to do an off center projection, with overlap for high-res tiling
		const bool bOrthographic = CameraInfo.ViewInfo.ProjectionMode == ECameraProjectionMode::Type::Orthographic;
		ModifyProjectionMatrixForTiling(CameraInfo.TilingParams, bOrthographic, ProjectionMatrix, DoFSensorScale);
		
		CameraInfo.ProjectionMatrix = ProjectionMatrix;
		CameraInfo.DoFSensorScale = DoFSensorScale;

		// The Scene View Family must be constructed first as the FSceneView needs it to be constructed
		UE::MovieGraph::Rendering::FViewFamilyInitData ViewFamilyInitData;
		ViewFamilyInitData.RenderTarget = RenderTargetResource;
		ViewFamilyInitData.World = GraphRenderer->GetWorld();
		ViewFamilyInitData.TimeData = InTimeData;
		ViewFamilyInitData.SceneCaptureSource = SceneCaptureSource;
		ViewFamilyInitData.bWorldIsPaused = bWorldIsPaused;
		ViewFamilyInitData.FrameIndex = FrameIndex;
		ViewFamilyInitData.AntiAliasingMethod = AntiAliasingMethod;
		ViewFamilyInitData.ShowFlags = ParentNodeThisFrame->GetShowFlags();
		ViewFamilyInitData.ViewModeIndex = ParentNodeThisFrame->GetViewModeIndex();
		
		TSharedRef<FSceneViewFamilyContext> ViewFamily = CreateSceneViewFamily(ViewFamilyInitData, CameraInfo);
		 
		// Now we can construct a View to go within this family.
		FSceneViewInitOptions SceneViewInitOptions = CreateViewInitOptions(CameraInfo, ViewFamily.ToSharedPtr().Get(), SceneViewState);
		FSceneView* NewView = CreateSceneView(SceneViewInitOptions, ViewFamily, CameraInfo);
		
		// Then apply Movie Render Queue specific overrides to the ViewFamily, and then to the SceneView.
		ApplyMovieGraphOverridesToViewFamily(ViewFamily, ViewFamilyInitData);
		
		// ToDo: This really only needs access to the ViewFamily for path tracer related things,
		// and would rather just take a FSceneView* 
		ApplyMovieGraphOverridesToSceneView(ViewFamily, ViewFamilyInitData, CameraInfo);
		
		FHitProxyConsumer* HitProxyConsumer = nullptr;
		const float DPIScale = 1.0f;
		FCanvas Canvas = FCanvas(RenderTargetResource, HitProxyConsumer, GraphRenderer->GetWorld(), GraphRenderer->GetWorld()->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, DPIScale);
		
		// Construct the sample state that reflects the current render sample
		UE::MovieGraph::FMovieGraphSampleState SampleState;
		{
			// Take our per-frame Traversal Context and update it with context specific to this sample.
			FMovieGraphTraversalContext UpdatedTraversalContext = InFrameTraversalContext;
			UpdatedTraversalContext.Time = InTimeData;
			UpdatedTraversalContext.RenderDataIdentifier = RenderDataIdentifier;

			SampleState.TraversalContext = MoveTemp(UpdatedTraversalContext);
			SampleState.BackbufferResolution = BackbufferResolution;
			SampleState.AccumulatorResolution = AccumulatorResolution;
			SampleState.bWriteSampleToDisk = bWriteAllSamples;
			SampleState.bRequiresAccumulator = InTimeData.bRequiresAccumulator || (NumSpatialSamples > 1);
			SampleState.bFetchFromAccumulator = InTimeData.bIsLastTemporalSampleForFrame && (SpatialIndex == (NumSpatialSamples - 1));
			SampleState.OverlappedPad = OverlappedPad;
			SampleState.OverlappedOffset = OverlappedOffset;
			SampleState.OverlappedSubpixelShift = OverlappedSubpixelShift;
			SampleState.OverscanFraction = OverscanFraction;
			SampleState.bAllowOCIO = ParentNodeThisFrame->GetAllowOCIO();
			SampleState.SceneCaptureSource = SceneCaptureSource;
			SampleState.CompositingSortOrder = 10;
		}

		if (UMovieGraphImagePassBaseNode* ParentNode = GetParentNode(InFrameTraversalContext.Time.EvaluatedConfig))
		{
			for (const FMoviePipelinePostProcessPass& PostProcessPass : ParentNode->GetAdditionalPostProcessMaterials())
			{
				if (PostProcessPass.bEnabled)
				{
					UMaterialInterface* Material = PostProcessPass.Material.LoadSynchronous();
					if (Material)
					{
						NewView->FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(Material);
					}
				}
			}

			for (UMaterialInterface* VisMaterial : NewView->FinalPostProcessSettings.BufferVisualizationOverviewMaterials)
			{
				auto BufferPipe = MakeShared<FImagePixelPipe, ESPMode::ThreadSafe>();
				
				FMovieGraphRenderDataIdentifier Identifier = RenderDataIdentifier;
				Identifier.SubResourceName = VisMaterial->GetName();
				
				UE::MovieGraph::FMovieGraphSampleState PassSampleState = SampleState;
				PassSampleState.TraversalContext.RenderDataIdentifier = Identifier;
				
				// Give a lower priority to materials so they show up after the main pass in multi-layer exrs.
				PassSampleState.CompositingSortOrder = SampleState.CompositingSortOrder + 1;
				BufferPipe->AddEndpoint(MakeForwardingEndpoint(PassSampleState, InTimeData));

				NewView->FinalPostProcessSettings.BufferVisualizationPipes.Add(VisMaterial->GetFName(), BufferPipe);
			}
		}

		int32 NumValidMaterials = NewView->FinalPostProcessSettings.BufferVisualizationPipes.Num();
		NewView->FinalPostProcessSettings.bBufferVisualizationDumpRequired = NumValidMaterials > 0;
		NewView->FinalPostProcessSettings.bOverride_PathTracingEnableDenoiser = true;

		// The denoiser is disabled during warm-up frames.
		NewView->FinalPostProcessSettings.PathTracingEnableDenoiser = bIsRenderingState && ParentNodeThisFrame->GetAllowDenoiser();

		// Submit the renderer to be rendered
		GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.ToSharedPtr().Get());

		// If this was just to contribute to the history buffer, no need to go any further.
		bool bDiscardOutput = InTimeData.bDiscardOutput || ShouldDiscardOutput(ViewFamily, CameraInfo);
		if (bDiscardOutput)
		{
			continue;
		}
		
		// Readback + Accumulate.
		PostRendererSubmission(SampleState, RenderTargetInitParams, Canvas, CameraInfo);
	}
}

void FMovieGraphDeferredPass::PostRendererSubmission(
	const UE::MovieGraph::FMovieGraphSampleState& InSampleState,
	const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo)
{
	TObjectPtr<UMovieGraphDefaultRenderer> GraphRenderer = GetRenderer().Get();
	if (!GraphRenderer)
	{
		return;
	}

	// Draw letterboxing
	// ToDo: Multi-camera support
	APlayerCameraManager* PlayerCameraManager = GraphRenderer->GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	if(PlayerCameraManager && PlayerCameraManager->GetCameraCacheView().bConstrainAspectRatio)
	{
		const FMinimalViewInfo CameraCache = PlayerCameraManager->GetCameraCacheView();
		
		// Taking overscan into account.
		const FIntPoint FullOutputSize = InSampleState.AccumulatorResolution;
	
		const float OutputSizeAspectRatio = FullOutputSize.X / (float)FullOutputSize.Y;
		const float CameraAspectRatio = InCameraInfo.bAllowCameraAspectRatio ? CameraCache.AspectRatio : OutputSizeAspectRatio;
	
		const FIntPoint ConstrainedFullSize = CameraAspectRatio > OutputSizeAspectRatio ?
			FIntPoint(FullOutputSize.X, FMath::CeilToInt((double)FullOutputSize.X / (double)CameraAspectRatio)) :
			FIntPoint(FMath::CeilToInt(CameraAspectRatio * FullOutputSize.Y), FullOutputSize.Y);
	
		const FIntPoint TileViewMin = InSampleState.OverlappedOffset;
		const FIntPoint TileViewMax = TileViewMin + InSampleState.BackbufferResolution;
	
		// Camera ratio constrained rect, clipped by the tile rect
		FIntPoint ConstrainedViewMin = (FullOutputSize - ConstrainedFullSize) / 2;
		FIntPoint ConstrainedViewMax = ConstrainedViewMin + ConstrainedFullSize;
		ConstrainedViewMin = FIntPoint(FMath::Clamp(ConstrainedViewMin.X, TileViewMin.X, TileViewMax.X),
			FMath::Clamp(ConstrainedViewMin.Y, TileViewMin.Y, TileViewMax.Y));
		ConstrainedViewMax = FIntPoint(FMath::Clamp(ConstrainedViewMax.X, TileViewMin.X, TileViewMax.X),
			FMath::Clamp(ConstrainedViewMax.Y, TileViewMin.Y, TileViewMax.Y));
	
		// Difference between the clipped constrained rect and the tile rect
		const FIntPoint OffsetMin = ConstrainedViewMin - TileViewMin;
		const FIntPoint OffsetMax = TileViewMax - ConstrainedViewMax;
	
		// Clear left
		if (OffsetMin.X > 0)
		{
			InCanvas.DrawTile(0, 0, OffsetMin.X, InSampleState.BackbufferResolution.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear right
		if (OffsetMax.X > 0)
		{
			InCanvas.DrawTile(InSampleState.BackbufferResolution.X - OffsetMax.X, 0, InSampleState.BackbufferResolution.X, InSampleState.BackbufferResolution.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear top
		if (OffsetMin.Y > 0)
		{
			InCanvas.DrawTile(0, 0, InSampleState.BackbufferResolution.X, OffsetMin.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear bottom
		if (OffsetMax.Y > 0)
		{
			InCanvas.DrawTile(0, InSampleState.BackbufferResolution.Y - OffsetMax.Y, InSampleState.BackbufferResolution.X, InSampleState.BackbufferResolution.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
	
		InCanvas.Flush_GameThread(true);
	}
	
	FMovieGraphImagePassBase::PostRendererSubmission(InSampleState, InRenderTargetInitParams, InCanvas, InCameraInfo);
}
} // namespace UE::MovieGraph::Rendering
