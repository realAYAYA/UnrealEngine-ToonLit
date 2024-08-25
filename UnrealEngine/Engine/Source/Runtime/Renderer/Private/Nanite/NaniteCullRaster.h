// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteSceneProxy.h"
#include "NaniteVisibility.h"
#include "PSOPrecacheMaterial.h"

class FVirtualShadowMapArray;
class FViewFamilyInfo;
class FSceneInstanceCullingQuery;

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

	bool				VisualizeActive;
	bool				VisualizeModeOverdraw;

	bool				bCustomPass;
};

struct FRasterResults
{
	FIntVector4		PageConstants;
	uint32			MaxVisibleClusters;
	uint32			MaxNodes;
	uint32			RenderFlags;

	FRDGBufferRef	ViewsBuffer			= nullptr;
	FRDGBufferRef	VisibleClustersSWHW	= nullptr;
	FRDGBufferRef	RasterBinMeta		= nullptr;

	FRDGTextureRef	VisBuffer64			= nullptr;
	FRDGTextureRef	DbgBuffer64			= nullptr;
	FRDGTextureRef	DbgBuffer32			= nullptr;

	FRDGTextureRef	MaterialDepth		= nullptr;
	FRDGTextureRef	ShadingMask			= nullptr;

	FRDGBufferRef	ClearTileArgs		= nullptr;
	FRDGBufferRef	ClearTileBuffer		= nullptr;

	FNaniteVisibilityQuery* VisibilityQuery = nullptr;

	TArray<FVisualizeResult, TInlineAllocator<32>> Visualizations;
};

void CollectRasterPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FPSOPrecacheParams& PreCacheParams,
	EShaderPlatform ShaderPlatform,
	int32 PSOCollectorIndex,
	TArray<FPSOPrecacheData>& PSOInitializers);

FRasterContext InitRasterContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FViewFamilyInfo& ViewFamily,
	FIntPoint TextureSize,
	FIntRect TextureRect,
	EOutputBufferMode RasterMode = EOutputBufferMode::VisBuffer,
	bool bClearTarget = true,
	FRDGBufferSRVRef RectMinMaxBufferSRV = nullptr,
	uint32 NumRects = 0,
	FRDGTextureRef ExternalDepthBuffer = nullptr,
	bool bCustomPass = false,
	bool bVisualize = false,
	bool bVisualizeOverdraw = false
);

struct FConfiguration
{
	uint32 bTwoPassOcclusion : 1;
	uint32 bUpdateStreaming : 1;
	uint32 bSupportsMultiplePasses : 1;
	uint32 bForceHWRaster : 1;
	uint32 bPrimaryContext : 1;
	uint32 bDrawOnlyRootGeometry : 1;
	uint32 bIsSceneCapture : 1;
	uint32 bIsReflectionCapture : 1;
	uint32 bIsLumenCapture : 1;
	uint32 bIsGameView : 1;
	uint32 bEditorShowFlag : 1;
	uint32 bGameShowFlag : 1;
	uint32 bDisableProgrammable : 1;
	uint32 bExtractStats : 1;
	EFilterFlags HiddenFilterFlags;

	void SetViewFlags(const FViewInfo& View);
};

class IRenderer
{
public:
	static TUniquePtr< IRenderer > Create(
		FRDGBuilder&			GraphBuilder,
		const FScene&			Scene,
		const FViewInfo&		SceneView,
		FSceneUniformBuffer&	SceneUniformBuffer,
		const FSharedContext&	SharedContext,
		const FRasterContext&	RasterContext,
		const FConfiguration&	Configuration,
		const FIntRect&			ViewRect,
		const TRefCountPtr<IPooledRenderTarget>& PrevHZB,
		FVirtualShadowMapArray*	VirtualShadowMapArray = nullptr );

	IRenderer() = default;
	virtual ~IRenderer() = default;

	virtual void DrawGeometry(
		FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		const FPackedViewArray& ViewArray,
		FSceneInstanceCullingQuery* OptionalSceneInstanceCullingQuery,
		const TConstArrayView<FInstanceDraw>* OptionalInstanceDraws) = 0;


	/**
	 * Draw scene geometry by brute-force culling against all instances in the scene.
	 */
	inline void DrawGeometry(FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		const FPackedViewArray& ViewArray)
	{
		DrawGeometry(RasterPipelines, VisibilityQuery, ViewArray, nullptr, nullptr);
	}

	/**
	 * Draw scene geometry driven by an explicit list FInstanceDraw (instance-id / view-id pairs).
	 */
	inline void DrawGeometry(FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		const FPackedViewArray& ViewArray,
		const TConstArrayView<FInstanceDraw> &InstanceDraws)
	{
		DrawGeometry(RasterPipelines, VisibilityQuery, ViewArray, nullptr, &InstanceDraws);
	}

	/**
	 * Draw scene geometry with and optional scene instance culling query. If non-null, the culling result is used to drive rendering, 
	 * otherwise falls back to brute-force culling (as above). 
	 */
	inline void DrawGeometry(FNaniteRasterPipelines& RasterPipelines,
		const FNaniteVisibilityQuery* VisibilityQuery,
		const FPackedViewArray& ViewArray,
		FSceneInstanceCullingQuery* OptionalSceneInstanceCullingQuery)
	{
		DrawGeometry(RasterPipelines, VisibilityQuery, ViewArray, OptionalSceneInstanceCullingQuery, nullptr);
	}

	virtual void ExtractResults( FRasterResults& RasterResults ) = 0;
};

} // namespace Nanite
