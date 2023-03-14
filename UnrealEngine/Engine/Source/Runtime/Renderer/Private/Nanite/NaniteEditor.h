// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteCullRaster.h"

DECLARE_GPU_STAT_NAMED_EXTERN(NaniteEditor, TEXT("Nanite Editor"));

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteVisualizeLevelInstanceParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FVector2f, OutputToInputScale)
	SHADER_PARAMETER(FVector2f, OutputToInputBias)
	SHADER_PARAMETER(uint32, MaxVisibleClusters)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVisibleCluster>, VisibleClustersSWHW)
	SHADER_PARAMETER(FIntVector4, PageConstants)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, VisBuffer64)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, MaterialResolve)

	SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteSelectionOutlineParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FVector2f, OutputToInputScale)
	SHADER_PARAMETER(FVector2f, OutputToInputBias)
	SHADER_PARAMETER(uint32, MaxVisibleClusters)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVisibleCluster>, VisibleClustersSWHW)
	SHADER_PARAMETER(FIntVector4, PageConstants)
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )

	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, VisBuffer64)

	SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
END_SHADER_PARAMETER_STRUCT()

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

void GetEditorSelectionPassParameters(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FRasterResults* NaniteRasterResults,
	FNaniteSelectionOutlineParameters* OutPassParameters
);

void DrawEditorSelection(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FNaniteSelectionOutlineParameters& PassParameters
);

void GetEditorVisualizeLevelInstancePassParameters(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FRasterResults* NaniteRasterResults,
	FNaniteVisualizeLevelInstanceParameters* OutPassParameters
);

void DrawEditorVisualizeLevelInstance(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FIntRect ViewportRect,
	const FNaniteVisualizeLevelInstanceParameters& PassParameters
);

#endif

} // namespace Nanite
