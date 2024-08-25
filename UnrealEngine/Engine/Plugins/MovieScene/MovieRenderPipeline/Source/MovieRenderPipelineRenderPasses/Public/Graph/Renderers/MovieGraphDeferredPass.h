// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/Renderers/MovieGraphImagePassBase.h"

namespace UE::MovieGraph::Rendering
{
	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphDeferredPass : public FMovieGraphImagePassBase
	{
		// FMovieGraphImagePassBase Interface
		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer) override;
		virtual void Teardown() override;
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
		virtual void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FName GetBranchName() const override;
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const override;
		// End FMovieGraphImagePassBase
			
	protected:
		virtual void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) override;
	protected:
		FMovieGraphRenderPassLayerData LayerData;

		/** Unique identifier passed in GatherOutputPasses and with each render that identifies the data produced by this renderer. */
		FMovieGraphRenderDataIdentifier RenderDataIdentifier;

		// Scene View history used by the renderer 
		FSceneViewStateReference SceneViewState;
	}; 
}