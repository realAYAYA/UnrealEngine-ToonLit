// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteCullRaster.h"

struct FNaniteMaterialPassCommand;
struct FScreenMessageWriter;

namespace Nanite
{

void AddVisualizationPasses(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextures& SceneTextures,
	const FEngineShowFlags& EngineShowFlags,
	TArrayView<const FViewInfo> Views,
	TArrayView<Nanite::FRasterResults> Results,
	FNanitePickingFeedback& PickingFeedback,
	FVirtualShadowMapArray&	VirtualShadowMapArray
);

void DisplayPicking(
	const FScene* Scene,
	const FNanitePickingFeedback& PickingFeedback,
	uint32 RenderFlags,
	FScreenMessageWriter& ScreenMessageWriter
);

#if WITH_DEBUG_VIEW_MODES

enum class EDebugViewMode : uint8
{
	None = 0,
	Wireframe = 1,
	ShaderComplexity = 2,
	LightmapDensity = 3,
	PrimitiveColor = 4,
};

void RenderDebugViewMode(
	FRDGBuilder& GraphBuilder,
	EDebugViewMode DebugViewMode,
	const FScene& Scene,
	const FViewInfo& View,
	const FSceneViewFamily& ViewFamily,
	const FRasterResults& RasterResults,
	FRDGTextureRef OutputColorTexture,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef QuadOverdrawTexture
);

#endif

} // namespace Nanite
