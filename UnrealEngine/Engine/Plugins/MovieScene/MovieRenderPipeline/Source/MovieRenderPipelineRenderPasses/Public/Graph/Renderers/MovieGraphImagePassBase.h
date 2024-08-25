// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "SceneView.h"

// Forward Declares
class UMovieGraphImagePassBaseNode;

namespace UE::MovieGraph::Rendering
{
	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphRenderDataAccumulationArgs
	{
	public:
		TWeakPtr<FImageOverlappedAccumulator, ESPMode::ThreadSafe> ImageAccumulator;
		TWeakPtr<IMovieGraphOutputMerger, ESPMode::ThreadSafe> OutputMerger;

		// If it's the first sample then we will reset the accumulator to a clean slate before accumulating into it.
		bool bIsFirstSample;
		// If it's the last sample, then we will trigger moving the data to the output merger. It can be both the first and last sample at the same time.
		bool bIsLastSample;
	};

	// Forward Declare
	void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const ::UE::MovieGraph::FMovieGraphSampleState InSampleState, const FMovieGraphRenderDataAccumulationArgs& InAccumulationParams);

	struct MOVIERENDERPIPELINERENDERPASSES_API FViewFamilyInitData
	{
		FViewFamilyInitData()
			: RenderTarget(nullptr)
			, World(nullptr)
			, SceneCaptureSource(ESceneCaptureSource::SCS_MAX)
			, bWorldIsPaused(false)
			, FrameIndex(-1)
			, AntiAliasingMethod(EAntiAliasingMethod::AAM_None)
			, ShowFlags(ESFIM_Game)
			, ViewModeIndex(VMI_Lit)
		{
		}

		class FRenderTarget* RenderTarget;
		class UWorld* World;
		FMovieGraphTimeStepData TimeData;
		ESceneCaptureSource SceneCaptureSource;
		bool bWorldIsPaused;
		int32 FrameIndex;
		EAntiAliasingMethod AntiAliasingMethod;
		FEngineShowFlags ShowFlags;
		EViewModeIndex ViewModeIndex;
	};
	


	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphImagePassBase
	{
		FMovieGraphImagePassBase() = default;
		virtual ~FMovieGraphImagePassBase() = default;

		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer);
		virtual void Teardown() {}
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) {}
		virtual void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const {}
		virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
		virtual FName GetBranchName() const { return NAME_None; }
		virtual TWeakObjectPtr<UMovieGraphDefaultRenderer> GetRenderer() const { return WeakGraphRenderer; }

	protected:
		/** Utility function for calculating a Projection Matrix (Orthographic or Perspective), and modify it based on the overscan percentage, aspect ratio, etc. */
		virtual FMatrix CalculateProjectionMatrix(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		/** Utility function for modifying the projection matrix to match the given tiling parameters. */
		virtual void ModifyProjectionMatrixForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams, const bool bInOrthographic, FMatrix& InOutProjectionMatrix, float& OutDoFSensorScale) const;
		/** Utility function for calculating the PrinciplePointOffset with the given tiling parameters. Used to make some effects (like vignette) work with tiling. */
		virtual FVector4f CalculatePrinciplePointOffsetForTiling(const UE::MovieGraph::DefaultRenderer::FMovieGraphTilingParams& InTilingParams) const;
		/** Utility function for creating FSceneViewInitOptions based on the specified camera info. */
		virtual FSceneViewInitOptions CreateViewInitOptions(const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo, FSceneViewFamilyContext* InViewFamily, FSceneViewStateReference& InViewStateRef) const;
		/** Utility function for creating a FSceneView for the given InitOptions, Family, and Camera. */
		virtual FSceneView* CreateSceneView(const FSceneViewInitOptions& InInitOptions, TSharedRef<FSceneViewFamilyContext> InViewFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;

		virtual void ApplyCameraManagerPostProcessBlends(FSceneView* InView) const;
		virtual TSharedRef<FSceneViewFamilyContext> CreateSceneViewFamily(const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		virtual void ApplyMovieGraphOverridesToSceneView(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const;
		virtual void ApplyMovieGraphOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData) const;
		virtual void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo);
		virtual TFunction<void(TUniquePtr<FImagePixelData>&&)> MakeForwardingEndpoint(const FMovieGraphSampleState& InSampleState, const FMovieGraphTimeStepData& InTimeData);
		virtual bool ShouldDiscardOutput(const TSharedRef<FSceneViewFamilyContext>& InFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const { return false; }
		/** For this image pass, look up the associated node by type for the given config. */
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const { return nullptr; }

	protected:
		TWeakObjectPtr<UMovieGraphDefaultRenderer> WeakGraphRenderer;
	};
}