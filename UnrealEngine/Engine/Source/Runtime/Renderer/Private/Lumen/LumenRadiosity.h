// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneRendering.h"

namespace LumenRadiosity
{
	struct FFrameTemporaries
	{
		bool bIndirectLightingHistoryValid = false;
		bool bUseProbeOcclusion = false;

		int32 ProbeSpacing = 0;
		int32 HemisphereProbeResolution = 0;
		FIntPoint ProbeAtlasSize = FIntPoint(0, 0);
		FIntPoint ProbeTracingAtlasSize = FIntPoint(0, 0);

		FRDGTextureRef TraceRadianceAtlas = nullptr;
		FRDGTextureRef TraceHitDistanceAtlas = nullptr;
		FRDGTextureRef ProbeSHRedAtlas = nullptr;
		FRDGTextureRef ProbeSHGreenAtlas = nullptr;
		FRDGTextureRef ProbeSHBlueAtlas = nullptr;
	};

	bool IsEnabled(const FSceneViewFamily& ViewFamily);
	void InitFrameTemporaries(FRDGBuilder& GraphBuilder, const FLumenSceneData& LumenSceneData, const FSceneViewFamily& ViewFamily, const TArray<FViewInfo>& Views, LumenRadiosity::FFrameTemporaries& RadiosityFrameTemporaries);
	uint32 GetAtlasDownsampleFactor();
};