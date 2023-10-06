// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteCullRaster.h"

DECLARE_GPU_STAT_NAMED_EXTERN(NaniteEditor, TEXT("Nanite Editor"));

namespace Nanite
{

void DrawHitProxies(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FRasterResults& RasterResults,
	FRDGTextureRef HitProxyTexture,
	FRDGTextureRef HitProxyDeptTexture
);

#if WITH_EDITOR

void DrawEditorSelection(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef DepthTarget,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults* NaniteRasterResults
);

void DrawEditorVisualizeLevelInstance(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef DepthTarget,
	FScene& Scene,
	const FViewInfo& SceneView,
	const FViewInfo& EditorView,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FRasterResults* NaniteRasterResults
);

#endif

} // namespace Nanite
