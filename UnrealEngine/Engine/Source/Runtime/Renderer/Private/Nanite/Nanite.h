// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteCullRaster.h"
#include "NaniteDrawList.h"
#include "NaniteMaterials.h"
#include "NaniteVisualize.h"
#if WITH_EDITOR
#include "NaniteEditor.h"
#endif

namespace Nanite
{

void ExtractRasterStats(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FCullingContext& CullingContext,
	const FBinningData& MainPassBinning,
	const FBinningData& PostPassBinning,
	bool bVirtualTextureTarget
);

void ExtractShadingStats(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGBufferRef MaterialIndirectArgs,
	uint32 NumShadingBins
);

void PrintStats(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View
);

void ExtractResults(
	FRDGBuilder& GraphBuilder,
	const FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	FRasterResults& RasterResults
);

void EmitShadowMap(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FRasterContext& RasterContext,
	const FRDGTextureRef DepthBuffer,
	const FIntRect& SourceRect,
	const FIntPoint DestOrigin,
	const FMatrix& ProjectionMatrix,
	float DepthBias,
	bool bOrtho
);

void EmitCubemapShadow(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FRasterContext& RasterContext,
	const FRDGTextureRef CubemapDepthBuffer,
	const FIntRect& ViewRect,
	uint32 CubemapFaceIndex,
	bool bUseGeometryShader
);

} // namespace Nanite

