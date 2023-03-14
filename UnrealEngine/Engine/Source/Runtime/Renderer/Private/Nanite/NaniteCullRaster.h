// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"

class FVirtualShadowMapArray;

BEGIN_SHADER_PARAMETER_STRUCT(FRasterParameters,)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>,			OutDepthBuffer)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>,	OutDepthBufferArray)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UlongType>,	OutVisBuffer64)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UlongType>,	OutDbgBuffer64)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>,			OutDbgBuffer32)
END_SHADER_PARAMETER_STRUCT()

namespace Nanite
{

enum class ERasterScheduling : uint8
{
	// Only rasterize using fixed function hardware.
	HardwareOnly = 0,

	// Rasterize large triangles with hardware, small triangles with software (compute).
	HardwareThenSoftware = 1,

	// Rasterize large triangles with hardware, overlapped with rasterizing small triangles with software (compute).
	HardwareAndSoftwareOverlap = 2,
};

/**
 * Used to select raster mode when creating the context.
 */
enum class EOutputBufferMode : uint8
{
	// Default mode outputting both ID and depth
	VisBuffer,

	// Rasterize only depth to 32 bit buffer
	DepthOnly,
};

enum class EPipeline : uint8
{
	Primary,
	Shadows,
	Lumen,
	HitProxy
};

struct FSharedContext
{
	FGlobalShaderMap* ShaderMap;
	ERHIFeatureLevel::Type FeatureLevel;
	EPipeline Pipeline;
};

struct FCullingContext
{
	struct FConfiguration
	{
		uint32 bTwoPassOcclusion : 1;
		uint32 bUpdateStreaming : 1;
		uint32 bSupportsMultiplePasses : 1;
		uint32 bForceHWRaster : 1;
		uint32 bPrimaryContext : 1;
		uint32 bDrawOnlyVSMInvalidatingGeometry : 1;
		uint32 bIsSceneCapture : 1;
		uint32 bIsReflectionCapture : 1;
		uint32 bIsLumenCapture : 1;
		uint32 bIsGameView : 1;
		uint32 bEditorShowFlag : 1;
		uint32 bGameShowFlag : 1;
		uint32 bProgrammableRaster : 1;

		void SetViewFlags(const FViewInfo& View);
	}
	Configuration = { 0 };

	TRefCountPtr<IPooledRenderTarget> PrevHZB; // If non-null, HZB culling is enabled

	uint32			DrawPassIndex;
	uint32			NumInstancesPreCull;
	uint32			RenderFlags;
	uint32			DebugFlags;
	FIntRect		HZBBuildViewRect;

	FIntVector4		PageConstants;

	FRDGBufferRef	MainRasterizeArgsSWHW;
	FRDGBufferRef	PostRasterizeArgsSWHW;

	FRDGBufferRef	SafeMainRasterizeArgsSWHW;
	FRDGBufferRef	SafePostRasterizeArgsSWHW;

	FRDGBufferRef	ClusterCountSWHW;
	FRDGBufferRef	ClusterClassifyArgs;

	FRDGBufferRef	QueueState;
	FRDGBufferRef	VisibleClustersSWHW;
	FRDGBufferRef	OccludedInstances;
	FRDGBufferRef	OccludedInstancesArgs;
	FRDGBufferRef	TotalPrevDrawClustersBuffer;
	FRDGBufferRef	StreamingRequests;
	FRDGBufferRef	ViewsBuffer;
	FRDGBufferRef	InstanceDrawsBuffer;
	FRDGBufferRef	PrimitiveFilterBuffer;
	FRDGBufferRef	HiddenPrimitivesBuffer;
	FRDGBufferRef	ShowOnlyPrimitivesBuffer;
	FRDGBufferRef	StatsBuffer;
};

struct FRasterContext
{
	FVector2f			RcpViewSize;
	FIntPoint			TextureSize;
	EOutputBufferMode	RasterMode;
	ERasterScheduling	RasterScheduling;

	FRasterParameters	Parameters;

	FRDGTextureRef		DepthBuffer;
	FRDGTextureRef		VisBuffer64;
	FRDGTextureRef		DbgBuffer64;
	FRDGTextureRef		DbgBuffer32;

	uint32				VisualizeModeBitMask;
	bool				VisualizeActive;
};

struct FRasterResults
{
	FIntVector4		PageConstants;
	uint32			MaxVisibleClusters;
	uint32			MaxNodes;
	uint32			RenderFlags;

	FRDGBufferRef	ViewsBuffer{};
	FRDGBufferRef	VisibleClustersSWHW{};

	FRDGTextureRef	VisBuffer64{};
	FRDGTextureRef	DbgBuffer64{};
	FRDGTextureRef	DbgBuffer32{};

	FRDGTextureRef	MaterialDepth{};
	FRDGTextureRef	MaterialResolve{};

	FNaniteVisibilityResults VisibilityResults;

	TArray<FVisualizeResult, TInlineAllocator<32>> Visualizations;
};

FCullingContext	InitCullingContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FScene& Scene,
	const TRefCountPtr<IPooledRenderTarget> &PrevHZB,
	const FIntRect& HZBBuildViewRect,
	const FCullingContext::FConfiguration& Configuration
);

FRasterContext InitRasterContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	FIntPoint TextureSize,
	bool bVisualize,
	EOutputBufferMode RasterMode = EOutputBufferMode::VisBuffer,
	bool bClearTarget = true,
	FRDGBufferSRVRef RectMinMaxBufferSRV = nullptr,
	uint32 NumRects = 0,
	FRDGTextureRef ExternalDepthBuffer = nullptr
);

void CullRasterize(
	FRDGBuilder& GraphBuilder,
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityResults& VisibilityResults,
	const FScene& Scene,
	const FViewInfo& SceneView,
	const TArray<FPackedView, SceneRenderingAllocator>& Views,
	const FSharedContext& SharedContext,
	FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	const FRasterState& RasterState = FRasterState(),
	const TArray<FInstanceDraw, SceneRenderingAllocator>* OptionalInstanceDraws = nullptr,
	bool bExtractStats = false
);

/**
 * Rasterize to a virtual shadow map (set) defined by the Views array, each view must have a virtual shadow map index set and the 
 * virtual shadow map physical memory mapping must have been defined. Note that the physical backing is provided by the raster context.
 * parameter Views - One view per layer to rasterize, the 'TargetLayerIdX_AndMipLevelY.X' must be set to the correct layer.
 */
void CullRasterize(
	FRDGBuilder& GraphBuilder,
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityResults& VisibilityResults,
	const FScene& Scene,
	const FViewInfo& SceneView,
	const TArray<FPackedView, SceneRenderingAllocator>& Views,
	uint32 NumPrimaryViews,	// Number of non-mip views
	const FSharedContext& SharedContext,
	FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	const FRasterState& RasterState = FRasterState(),
	const TArray<FInstanceDraw, SceneRenderingAllocator>* OptionalInstanceDraws = nullptr,
	// VirtualShadowMapArray is the supplier of virtual to physical translation, probably could abstract this a bit better,
	FVirtualShadowMapArray* VirtualShadowMapArray = nullptr,
	bool bExtractStats = false
);

} // namespace Nanite
