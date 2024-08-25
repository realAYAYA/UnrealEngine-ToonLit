// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MovieGraphDeferredPass.h"

namespace UE::MovieGraph::Rendering
{
	struct MOVIERENDERPIPELINERENDERPASSES_API FMovieGraphPathTracerPass : public FMovieGraphDeferredPass
	{
		virtual UMovieGraphImagePassBaseNode* GetParentNode(UMovieGraphEvaluatedConfig* InConfig) const override;
		virtual void ApplyMovieGraphOverridesToSceneView(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const override;
		virtual bool ShouldDiscardOutput(const TSharedRef<FSceneViewFamilyContext>& InFamily, const UE::MovieGraph::DefaultRenderer::FCameraInfo& InCameraInfo) const override;
	}; 
}