// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteCullRaster.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "NaniteVisualizationData.h"
#include "NaniteSceneProxy.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "SceneTextureParameters.h"
#include "GPUScene.h"
#include "RendererModule.h"
#include "Rendering/NaniteStreamingManager.h"
#include "SystemTextures.h"
#include "ComponentRecreateRenderStateContext.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "SceneTextureReductions.h"
#include "Engine/Engine.h"
#include "RenderGraphUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "DynamicResolutionState.h"
#include "Lumen/Lumen.h"
#include "TessellationTable.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("CullingContexts"), STAT_NaniteCullingContexts, STATGROUP_Nanite);

#define CULLING_PASS_NO_OCCLUSION		0
#define CULLING_PASS_OCCLUSION_MAIN		1
#define CULLING_PASS_OCCLUSION_POST		2
#define CULLING_PASS_EXPLICIT_LIST		3

static_assert(NANITE_NUM_CULLING_FLAG_BITS + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS + NANITE_MAX_INSTANCES_BITS + NANITE_MAX_GPU_PAGES_BITS + NANITE_MAX_CLUSTERS_PER_PAGE_BITS <= 64, "FVisibleCluster fields don't fit in 64bits");
static_assert(1 + NANITE_NUM_CULLING_FLAG_BITS + NANITE_MAX_INSTANCES_BITS <= 32, "FCandidateNode.x fields don't fit in 32bits");
static_assert(1 + NANITE_MAX_NODES_PER_PRIMITIVE_BITS + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS <= 32, "FCandidateNode.y fields don't fit in 32bits");
static_assert(1 + NANITE_MAX_BVH_NODES_PER_GROUP <= 32, "FCandidateNode.z fields don't fit in 32bits");

int32 GNaniteShowDrawEvents = 0;
static FAutoConsoleVariableRef CVarNaniteShowDrawEvents(
	TEXT("r.Nanite.ShowMeshDrawEvents"),
	GNaniteShowDrawEvents,
	TEXT("")
);

int32 GNaniteAsyncRasterization = 1;
static FAutoConsoleVariableRef CVarNaniteEnableAsyncRasterization(
	TEXT("r.Nanite.AsyncRasterization"),
	GNaniteAsyncRasterization,
	TEXT("")
);

int32 GNaniteParallelRasterTranslateExperimental = 0;
static FAutoConsoleVariableRef CVarNaniteParallelRasterTranslateExperimental(
	TEXT("r.Nanite.ParallelRasterTranslateExperimental"),
	GNaniteParallelRasterTranslateExperimental,
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarNaniteAsyncRasterizeShadowDepths(
	TEXT("r.Nanite.AsyncRasterization.ShadowDepths"),
	1,
	TEXT("Whether to run Nanite SW rasterization on a compute pipe if possible.")
);

int32 GNaniteComputeRasterization = 1;
static FAutoConsoleVariableRef CVarNaniteComputeRasterization(
	TEXT("r.Nanite.ComputeRasterization"),
	GNaniteComputeRasterization,
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarNaniteFilterPrimitives(
	TEXT("r.Nanite.FilterPrimitives"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GNaniteMeshShaderRasterization = 1;
FAutoConsoleVariableRef CVarNaniteMeshShaderRasterization(
	TEXT("r.Nanite.MeshShaderRasterization"),
	GNaniteMeshShaderRasterization,
	TEXT("")
);

// Support disabling mesh shader raster for VSMs
int32 GNaniteVSMMeshShaderRasterization = 0;
FAutoConsoleVariableRef CVarNaniteVSMMeshShaderRasterization(
	TEXT("r.Nanite.VSMMeshShaderRasterization"),
	GNaniteVSMMeshShaderRasterization,
	TEXT("")
);

int32 GNanitePrimShaderRasterization = 1;
FAutoConsoleVariableRef CVarNanitePrimShaderRasterization(
	TEXT("r.Nanite.PrimShaderRasterization"),
	GNanitePrimShaderRasterization,
	TEXT("")
);

int32 GNaniteAutoShaderCulling = 0;
FAutoConsoleVariableRef CVarNaniteAutoShaderCulling(
	TEXT("r.Nanite.AutoShaderCulling"),
	GNaniteAutoShaderCulling,
	TEXT("")
);

float GNaniteMaxPixelsPerEdge = 1.0f;
FAutoConsoleVariableRef CVarNaniteMaxPixelsPerEdge(
	TEXT("r.Nanite.MaxPixelsPerEdge"),
	GNaniteMaxPixelsPerEdge,
	TEXT("")
	);

int32 GNaniteImposterMaxPixels = 5;
FAutoConsoleVariableRef CVarNaniteImposterMaxPixels(
	TEXT("r.Nanite.ImposterMaxPixels"),
	GNaniteImposterMaxPixels,
	TEXT("")
);

float GNaniteMinPixelsPerEdgeHW = 32.0f;
FAutoConsoleVariableRef CVarNaniteMinPixelsPerEdgeHW(
	TEXT("r.Nanite.MinPixelsPerEdgeHW"),
	GNaniteMinPixelsPerEdgeHW,
	TEXT("")
);

int32 GNaniteAllowProgrammableRaster = 1;
static FAutoConsoleVariableRef CVarNaniteAllowProgrammableRaster(
	TEXT("r.Nanite.AllowProgrammableRaster"),
	GNaniteAllowProgrammableRaster,
	TEXT(""),
	ECVF_ReadOnly
);

// 0 : Disabled
// 1 : Pixel Clear
// 2 : Tile Clear
int32 GNaniteFastVisBufferClear = 1;
static FAutoConsoleVariableRef CVarNaniteFastVisBufferClear(
	TEXT("r.Nanite.FastVisBufferClear"),
	GNaniteFastVisBufferClear,
	TEXT("")
);

// Requires r.Nanite.AllowProgrammableRaster=1 for compiled shaders
// 0: Disabled
// 1: Enabled
int32 GNaniteProgrammableRaster = 1;
static FAutoConsoleVariableRef CVarNaniteProgrammableRaster(
	TEXT("r.Nanite.ProgrammableRaster"),
	GNaniteProgrammableRaster,
	TEXT("")
);

// Support a max of 3 unique materials per visible cluster (i.e. if all clusters are fast path and use full range, never run out of space).
float GNaniteRasterIndirectionMultiplier = 3.0f;
static FAutoConsoleVariableRef CVarNaniteRasterIndirectionMultiplier(
	TEXT("r.Nanite.RasterIndirectionMultiplier"),
	GNaniteRasterIndirectionMultiplier,
	TEXT("")
);

int32 GNaniteCullingHZB = 1;
static FAutoConsoleVariableRef CVarNaniteCullingHZB(
	TEXT("r.Nanite.Culling.HZB"),
	GNaniteCullingHZB,
	TEXT("Set to 0 to test disabling Nanite culling due to occlusion by the hierarchical depth buffer.")
);

int32 GNaniteCullingFrustum = 1;
static FAutoConsoleVariableRef CVarNaniteCullingFrustum(
	TEXT("r.Nanite.Culling.Frustum"),
	GNaniteCullingFrustum,
	TEXT("Set to 0 to test disabling Nanite culling due to being outside of the view frustum.")
);

int32 GNaniteCullingGlobalClipPlane = 1;
static FAutoConsoleVariableRef CVarNaniteCullingGlobalClipPlane(
	TEXT("r.Nanite.Culling.GlobalClipPlane"),
	GNaniteCullingGlobalClipPlane,
	TEXT("Set to 0 to test disabling Nanite culling due to being beyond the global clip plane.\n")
	TEXT("NOTE: Has no effect if r.AllowGlobalClipPlane=0.")
);

int32 GNaniteCullingDrawDistance = 1;
static FAutoConsoleVariableRef CVarNaniteCullingDrawDistance(
	TEXT("r.Nanite.Culling.DrawDistance"),
	GNaniteCullingDrawDistance,
	TEXT("Set to 0 to test disabling Nanite culling due to instance draw distance.")
);

int32 GNaniteCullingWPODisableDistance = 1;
static FAutoConsoleVariableRef CVarNaniteCullingWPODisableDistance(
	TEXT("r.Nanite.Culling.WPODisableDistance"),
	GNaniteCullingWPODisableDistance,
	TEXT("Set to 0 to test disabling 'World Position Offset Disable Distance' for Nanite instances.")
);

static TAutoConsoleVariable<int32> CVarLargePageRectThreshold(
	TEXT("r.Nanite.LargePageRectThreshold"),
	128,
	TEXT("Threshold for the size in number of virtual pages overlapped of a candidate cluster to be recorded as large in the stats."),
	ECVF_RenderThreadSafe
);

int32 GNaniteDisocclusionHack = 0;
static FAutoConsoleVariableRef CVarNaniteDisocclusionHack(
	TEXT("r.Nanite.DisocclusionHack"),
	GNaniteDisocclusionHack,
	TEXT("HACK that lowers LOD level of disoccluded instances to mitigate performance spikes"),
	ECVF_RenderThreadSafe
);

int32 GNanitePersistentThreadsCulling = 1;
static FAutoConsoleVariableRef CVarNanitePersistentThreadsCulling(
	TEXT("r.Nanite.PersistentThreadsCulling"),
	GNanitePersistentThreadsCulling,
	TEXT("Perform node and cluster culling in one combined kernel using persistent threads."),
	ECVF_RenderThreadSafe
);

// i.e. if r.Nanite.MaxPixelsPerEdge is 1.0 and r.Nanite.PrimaryRaster.PixelsPerEdgeScaling is 20%, when heavily over budget r.Nanite.MaxPixelsPerEdge will be scaled to to 5.0
static TAutoConsoleVariable<float> CVarNanitePrimaryPixelsPerEdgeScalingPercentage(
	TEXT("r.Nanite.PrimaryRaster.PixelsPerEdgeScaling"),
	30.0f, // 100% - no scaling - set to < 100% to scale pixel error when over budget
	TEXT("Lower limit percentage to scale the Nanite primary raster MaxPixelsPerEdge value when over budget."),
	ECVF_RenderThreadSafe | ECVF_Default);

// i.e. if r.Nanite.MaxPixelsPerEdge is 1.0 and r.Nanite.ShadowRaster.PixelsPerEdgeScaling is 20%, when heavily over budget r.Nanite.MaxPixelsPerEdge will be scaled to to 5.0
static TAutoConsoleVariable<float> CVarNaniteShadowPixelsPerEdgeScalingPercentage(
	TEXT("r.Nanite.ShadowRaster.PixelsPerEdgeScaling"),
	100.0f, // 100% - no scaling - set to < 100% to scale pixel error when over budget
	TEXT("Lower limit percentage to scale the Nanite shadow raster MaxPixelsPerEdge value when over budget."),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<float> CVarNanitePrimaryTimeBudgetMs(
	TEXT("r.Nanite.PrimaryRaster.TimeBudgetMs"),
	DynamicRenderScaling::FHeuristicSettings::kBudgetMsDisabled,
	TEXT("Frame's time budget for Nanite primary raster in milliseconds."),
	ECVF_RenderThreadSafe | ECVF_Default);

static TAutoConsoleVariable<float> CVarNaniteShadowTimeBudgetMs(
	TEXT("r.Nanite.ShadowRaster.TimeBudgetMs"),
	DynamicRenderScaling::FHeuristicSettings::kBudgetMsDisabled,
	TEXT("Frame's time budget for Nanite shadow raster in milliseconds."),
	ECVF_RenderThreadSafe | ECVF_Default);

static DynamicRenderScaling::FHeuristicSettings GetDynamicNaniteScalingPrimarySettings()
{
	const float PixelsPerEdgeScalingPercentage = FMath::Clamp(CVarNanitePrimaryPixelsPerEdgeScalingPercentage.GetValueOnAnyThread(), 1.0f, 100.0f);

	DynamicRenderScaling::FHeuristicSettings BucketSetting;
	BucketSetting.Model = DynamicRenderScaling::EHeuristicModel::Linear;
	BucketSetting.bModelScalesWithPrimaryScreenPercentage = false; // r.Nanite.MaxPixelsPerEdge is not scaled by dynamic resolution of the primary view
	BucketSetting.MinResolutionFraction = DynamicRenderScaling::PercentageToFraction(PixelsPerEdgeScalingPercentage);
	BucketSetting.MaxResolutionFraction = DynamicRenderScaling::PercentageToFraction(100.0f);
	BucketSetting.BudgetMs = CVarNanitePrimaryTimeBudgetMs.GetValueOnAnyThread();
	BucketSetting.ChangeThreshold = DynamicRenderScaling::PercentageToFraction(1.0f);
	BucketSetting.TargetedHeadRoom = DynamicRenderScaling::PercentageToFraction(5.0f); // 5% headroom
	BucketSetting.UpperBoundQuantization = DynamicRenderScaling::FHeuristicSettings::kDefaultUpperBoundQuantization;
	return BucketSetting;
}

static DynamicRenderScaling::FHeuristicSettings GetDynamicNaniteScalingShadowSettings()
{
	const float PixelsPerEdgeScalingPercentage = FMath::Clamp(CVarNaniteShadowPixelsPerEdgeScalingPercentage.GetValueOnAnyThread(), 1.0f, 100.0f);

	DynamicRenderScaling::FHeuristicSettings BucketSetting;
	BucketSetting.Model = DynamicRenderScaling::EHeuristicModel::Linear;
	BucketSetting.bModelScalesWithPrimaryScreenPercentage = false; // r.Nanite.MaxPixelsPerEdge is not scaled by dynamic resolution of the primary view
	BucketSetting.MinResolutionFraction = DynamicRenderScaling::PercentageToFraction(PixelsPerEdgeScalingPercentage);
	BucketSetting.MaxResolutionFraction = DynamicRenderScaling::PercentageToFraction(100.0f);
	BucketSetting.BudgetMs = CVarNaniteShadowTimeBudgetMs.GetValueOnAnyThread();
	BucketSetting.ChangeThreshold = DynamicRenderScaling::PercentageToFraction(1.0f);
	BucketSetting.TargetedHeadRoom = DynamicRenderScaling::PercentageToFraction(5.0f); // 5% headroom
	BucketSetting.UpperBoundQuantization = DynamicRenderScaling::FHeuristicSettings::kDefaultUpperBoundQuantization;
	return BucketSetting;
}

DynamicRenderScaling::FBudget GDynamicNaniteScalingPrimary(TEXT("DynamicNaniteScalingPrimary"), &GetDynamicNaniteScalingPrimarySettings);
DynamicRenderScaling::FBudget GDynamicNaniteScalingShadow( TEXT("DynamicNaniteScalingShadow"),  &GetDynamicNaniteScalingShadowSettings);

extern int32 GNaniteShowStats;
extern int32 GSkipDrawOnPSOPrecaching;

// Set to 1 to pretend all programmable raster draws are not precached yet
int32 GNaniteTestPrecacheDrawSkipping = 0;
static FAutoConsoleVariableRef CVarNaniteTestPrecacheDrawSkipping(
	TEXT("r.Nanite.TestPrecacheDrawSkipping"),
	GNaniteTestPrecacheDrawSkipping,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static bool UseMeshShader(EShaderPlatform ShaderPlatform, Nanite::EPipeline Pipeline)
{
	// Disable mesh shaders if global clip planes are enabled and the platform cannot support MS with clip distance output
	static const auto AllowGlobalClipPlaneVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowGlobalClipPlane"));
	static const bool bAllowGlobalClipPlane = (AllowGlobalClipPlaneVar && AllowGlobalClipPlaneVar->GetValueOnAnyThread() != 0);
	const bool bMSSupportsClipDistance = FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersWithClipDistance(ShaderPlatform);

	// We require tier1 support to utilize primitive attributes
	const bool bSupported = GNaniteMeshShaderRasterization != 0 && GRHISupportsMeshShadersTier1 && (!bAllowGlobalClipPlane || bMSSupportsClipDistance);
	return bSupported && (GNaniteVSMMeshShaderRasterization != 0 || Pipeline != Nanite::EPipeline::Shadows);
}

static bool UsePrimitiveShader()
{
	return GNanitePrimShaderRasterization != 0 && GRHISupportsPrimitiveShaders;
}

static bool AllowProgrammableRaster(EShaderPlatform ShaderPlatform)
{
	return GNaniteAllowProgrammableRaster != 0;
}

static bool UseAutoCullingShader(bool bUsePrimitiveShader)
{
	return GRHISupportsPrimitiveShaders &&
		   !bUsePrimitiveShader &&
		   GNaniteAutoShaderCulling != 0;
}

static bool UseAsyncComputeForShadowMaps(const FViewFamilyInfo& ViewFamily)
{
	// Automatically disabled when Lumen async is enabled, as it then delays graphics pipe too much and regresses overall frame performance
	return CVarNaniteAsyncRasterizeShadowDepths.GetValueOnRenderThread() != 0 && !Lumen::UseAsyncCompute(ViewFamily);
}

#if WANTS_DRAW_MESH_EVENTS
static FORCEINLINE const TCHAR* GetRasterMaterialName(const FMaterialRenderProxy* InRasterMaterial, const FMaterialRenderProxy* InFixedFunction)
{
	if ((InRasterMaterial == nullptr) || (InRasterMaterial == InFixedFunction))
	{
		return TEXT("Fixed Function");
	}

	return *InRasterMaterial->GetMaterialName();
}
#endif

struct FCompactedViewInfo
{
	uint32 StartOffset;
	uint32 NumValidViews;
};

BEGIN_SHADER_PARAMETER_STRUCT( FCullingParameters, )
	SHADER_PARAMETER( FIntVector4,	PageConstants )
	SHADER_PARAMETER( uint32,		MaxCandidateClusters )
	SHADER_PARAMETER( uint32,		MaxVisibleClusters )
	SHADER_PARAMETER( uint32,		RenderFlags )
	SHADER_PARAMETER( uint32,		DebugFlags )
	SHADER_PARAMETER( uint32,		NumViews )
	SHADER_PARAMETER( uint32,		NumPrimaryViews )
	SHADER_PARAMETER( float,		DisocclusionLodScaleFactor )

	SHADER_PARAMETER( FVector2f,	HZBSize )

	SHADER_PARAMETER_RDG_TEXTURE( Texture2D,	HZBTexture )
	SHADER_PARAMETER_SAMPLER( SamplerState,		HZBSampler )
	
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPackedView >, InViews)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FCompactedViewInfo >, CompactedViewInfo)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint32 >, CompactedViewsAllocation)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT( FGPUSceneParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>,	GPUSceneInstanceSceneData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>,	GPUSceneInstancePayloadData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>,	GPUScenePrimitiveSceneData)
	SHADER_PARAMETER( uint32,						GPUSceneFrameNumber)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT( FVirtualTargetParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FVirtualShadowMapUniformParameters, VirtualShadowMap )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,	HZBPageTable )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint4 >,	HZBPageRectBounds )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,	HZBPageFlags )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutDirtyPageFlags)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutStaticInvalidatingPrimitives)
END_SHADER_PARAMETER_STRUCT()

class FRasterClearCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterClearCS);
	SHADER_USE_PARAMETER_STRUCT(FRasterClearCS, FNaniteGlobalShader);

	class FClearDepthDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_DEPTH");
	class FClearDebugDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_DEBUG");
	class FClearTiledDim : SHADER_PERMUTATION_BOOL("RASTER_CLEAR_TILED");
	using FPermutationDomain = TShaderPermutationDomain<FClearDepthDim, FClearDebugDim, FClearTiledDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FRasterParameters, RasterParameters)
		SHADER_PARAMETER(FUint32Vector4, ClearRect)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FRasterClearCS, "/Engine/Private/Nanite/NaniteRasterClear.usf", "RasterClear", SF_Compute);

class FPrimitiveFilter_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPrimitiveFilter_CS);
	SHADER_USE_PARAMETER_STRUCT(FPrimitiveFilter_CS, FNaniteGlobalShader);

	class FHiddenPrimitivesListDim : SHADER_PERMUTATION_BOOL("HAS_HIDDEN_PRIMITIVES_LIST");
	class FShowOnlyPrimitivesListDim : SHADER_PERMUTATION_BOOL("HAS_SHOW_ONLY_PRIMITIVES_LIST");

	using FPermutationDomain = TShaderPermutationDomain<FHiddenPrimitivesListDim, FShowOnlyPrimitivesListDim>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumPrimitives)
		SHADER_PARAMETER(uint32, HiddenFilterFlags)
		SHADER_PARAMETER(uint32, NumHiddenPrimitives)
		SHADER_PARAMETER(uint32, NumShowOnlyPrimitives)

		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneParameters, GPUSceneParameters)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PrimitiveFilterBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, HiddenPrimitivesList)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ShowOnlyPrimitivesList)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPrimitiveFilter_CS, "/Engine/Private/Nanite/NanitePrimitiveFilter.usf", "PrimitiveFilter", SF_Compute);

class FInstanceCull_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInstanceCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceCull_CS, FNaniteGlobalShader);

	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST, CULLING_PASS_EXPLICIT_LIST);
	class FMultiViewDim : SHADER_PERMUTATION_BOOL("NANITE_MULTI_VIEW");
	class FPrimitiveFilterDim : SHADER_PERMUTATION_BOOL("PRIMITIVE_FILTER");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL("DEBUG_FLAGS");
	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	using FPermutationDomain = TShaderPermutationDomain<FCullingPassDim, FMultiViewDim, FPrimitiveFilterDim, FDebugFlagsDim, FDepthOnlyDim, FVirtualTextureTargetDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}
		
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		// Skip permutations targeting other culling passes, as they are covered in the specialized VSM instance cull
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && PermutationVector.Get<FCullingPassDim>() != CULLING_PASS_OCCLUSION_POST)
		{
			return false;
		}
		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );	// Still needed for shader to compile

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER( uint32, NumInstances )
		SHADER_PARAMETER( uint32, MaxNodes )
		SHADER_PARAMETER( int32,  ImposterMaxPixels )
		
		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FGPUSceneParameters, GPUSceneParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FRasterParameters, RasterParameters )

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ImposterAtlas )
		
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FInstanceDraw >, InInstanceDraws )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer, OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FInstanceDraw >, OutOccludedInstances )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >, OutQueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedInstancesArgs )

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>, OutStatsBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >, InPrimitiveFilterBuffer )

		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInstanceCull_CS, "/Engine/Private/Nanite/NaniteInstanceCulling.usf", "InstanceCull", SF_Compute);


class FCompactViewsVSM_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactViewsVSM_CS);
	SHADER_USE_PARAMETER_STRUCT(FCompactViewsVSM_CS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
		OutEnvironment.SetDefine(TEXT("CULLING_PASS"), CULLING_PASS_NO_OCCLUSION);
		OutEnvironment.SetDefine(TEXT("DEPTH_ONLY"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FCullingParameters, CullingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneParameters, GPUSceneParameters)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FPackedNaniteView >, CompactedViewsOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FCompactedViewInfo >, CompactedViewInfoOut)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, CompactedViewsAllocationOut)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)
		END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCompactViewsVSM_CS, "/Engine/Private/Nanite/NaniteInstanceCulling.usf", "CompactViewsVSM_CS", SF_Compute);


class FInstanceCullVSM_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInstanceCullVSM_CS );
	SHADER_USE_PARAMETER_STRUCT( FInstanceCullVSM_CS, FNaniteGlobalShader);

	class FPrimitiveFilterDim : SHADER_PERMUTATION_BOOL("PRIMITIVE_FILTER");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL( "DEBUG_FLAGS" );
	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN);

	using FPermutationDomain = TShaderPermutationDomain<FPrimitiveFilterDim, FDebugFlagsDim, FCullingPassDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );

		FVirtualShadowMapArray::SetShaderDefines( OutEnvironment );

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine( TEXT( "USE_GLOBAL_GPU_SCENE_DATA" ), 1 );
		OutEnvironment.SetDefine( TEXT( "NANITE_MULTI_VIEW" ), 1 );
		OutEnvironment.SetDefine( TEXT("DEPTH_ONLY" ), 1 );
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER( uint32, NumInstances )
		SHADER_PARAMETER( uint32, MaxNodes )
		
		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FGPUSceneParameters, GPUSceneParameters )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer, OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FInstanceDraw >, OutOccludedInstances)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueueState >, OutQueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >, OutOccludedInstancesArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FNaniteStats>, OutStatsBuffer )

		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FInstanceDraw >, InOccludedInstances )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >, InPrimitiveFilterBuffer )

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

		SHADER_PARAMETER_STRUCT_INCLUDE( FVirtualTargetParameters, VirtualShadowMap )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER( FInstanceCullVSM_CS, "/Engine/Private/Nanite/NaniteInstanceCulling.usf", "InstanceCullVSM", SF_Compute );


class FNodeAndClusterCull_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FNodeAndClusterCull_CS );
	SHADER_USE_PARAMETER_STRUCT( FNodeAndClusterCull_CS, FNaniteGlobalShader );

	class FCullingPassDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_PASS", CULLING_PASS_NO_OCCLUSION, CULLING_PASS_OCCLUSION_MAIN, CULLING_PASS_OCCLUSION_POST);
	class FCullingTypeDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_TYPE", NANITE_CULLING_TYPE_NODES, NANITE_CULLING_TYPE_CLUSTERS, NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS);
	class FMultiViewDim : SHADER_PERMUTATION_BOOL("NANITE_MULTI_VIEW");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FDebugFlagsDim : SHADER_PERMUTATION_BOOL("DEBUG_FLAGS");
	
	using FPermutationDomain = TShaderPermutationDomain<FCullingPassDim, FCullingTypeDim, FMultiViewDim, FVirtualTextureTargetDim, FDebugFlagsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( FCullingParameters, CullingParameters )
		SHADER_PARAMETER_STRUCT_INCLUDE( FGPUSceneParameters, GPUSceneParameters)

		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,				ClusterPageData )
		SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,				HierarchyBuffer )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FUintVector2 >,		InTotalPrevDrawClusters )
		SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,						OffsetClustersArgsSWHW )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >,		QueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,					MainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,					MainAndPostCandididateClusters )

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,					OutVisibleClustersSWHW )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer<FStreamingRequest>,	OutStreamingRequests )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,						VisibleClustersArgsSWHW )

		SHADER_PARAMETER_STRUCT_INCLUDE( FVirtualTargetParameters,				VirtualShadowMap )

		SHADER_PARAMETER(uint32,												MaxNodes)
		SHADER_PARAMETER(uint32,												LargePageRectThreshold)
		SHADER_PARAMETER(uint32,												StreamingRequestsBufferVersion)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteStats>,		OutStatsBuffer)
		RDG_BUFFER_ACCESS(IndirectArgs,											ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if( PermutationVector.Get<FVirtualTextureTargetDim>() &&
			!PermutationVector.Get<FMultiViewDim>() )
		{
			return false;
		}

		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NANITE_HIERARCHY_TRAVERSAL"), 1);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		// The routing requires access to page table data structures, only for 'VIRTUAL_TEXTURE_TARGET' really...
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FNodeAndClusterCull_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "NodeAndClusterCull", SF_Compute);

class FInitClusterBatches_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitClusterBatches_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitClusterBatches_CS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,	OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER( uint32,								MaxCandidateClusters )
		SHADER_PARAMETER( uint32,								MaxNodes )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitClusterBatches_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitClusterBatches", SF_Compute);

class FInitCandidateNodes_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitCandidateNodes_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitCandidateNodes_CS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,	OutMainAndPostNodesAndClusterBatches )
		SHADER_PARAMETER( uint32,								MaxCandidateClusters )
		SHADER_PARAMETER( uint32,								MaxNodes )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitCandidateNodes_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitCandidateNodes", SF_Compute);

class FInitArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER( FInitArgs_CS );
	SHADER_USE_PARAMETER_STRUCT( FInitArgs_CS, FNaniteGlobalShader);

	class FOcclusionCullingDim : SHADER_PERMUTATION_BOOL( "OCCLUSION_CULLING" );
	class FDrawPassIndexDim : SHADER_PERMUTATION_INT( "DRAW_PASS_INDEX", 3 );	// 0: no, 1: set, 2: add
	using FPermutationDomain = TShaderPermutationDomain<FOcclusionCullingDim, FDrawPassIndexDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(uint32, RenderFlags)

		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FQueueState >,		OutQueueState )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FUintVector2 >,	InOutTotalPrevDrawClusters )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,						InOutMainPassRasterizeArgsSWHW )
		
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, OutOccludedInstancesArgs )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >, InOutPostPassRasterizeArgsSWHW )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitArgs", SF_Compute);

class FInitCullArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCullArgs_CS);
	SHADER_USE_PARAMETER_STRUCT(FInitCullArgs_CS, FNaniteGlobalShader);

	class FCullingTypeDim : SHADER_PERMUTATION_SPARSE_INT("CULLING_TYPE", NANITE_CULLING_TYPE_NODES, NANITE_CULLING_TYPE_CLUSTERS);
	using FPermutationDomain = TShaderPermutationDomain<FCullingTypeDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueueState >,	OutQueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutCullArgs)
		SHADER_PARAMETER(uint32,											MaxCandidateClusters)
		SHADER_PARAMETER(uint32,											MaxNodes)
		SHADER_PARAMETER(uint32,											InitIsPostPass)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitCullArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "InitCullArgs", SF_Compute);

class FCalculateSafeRasterizerArgs_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateSafeRasterizerArgs_CS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateSafeRasterizerArgs_CS, FNaniteGlobalShader);

	class FIsPostPass : SHADER_PERMUTATION_BOOL("IS_POST_PASS");
	class FProgrammableRaster : SHADER_PERMUTATION_BOOL("PROGRAMMABLE_RASTER");
	using FPermutationDomain = TShaderPermutationDomain<FIsPostPass, FProgrammableRaster>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FUintVector2 >,	InTotalPrevDrawClusters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >,						OffsetClustersArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer< uint >,						InRasterizerArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutSafeRasterizerArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FUintVector2 >,	OutClusterCountSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >,					OutClusterClassifyArgs)

		SHADER_PARAMETER(uint32,											MaxVisibleClusters)
		SHADER_PARAMETER(uint32,											RenderFlags)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCalculateSafeRasterizerArgs_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "CalculateSafeRasterizerArgs", SF_Compute);

class FRasterBinBuild_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinBuild_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinBuild_CS, FNaniteGlobalShader);

	class FIsPostPass : SHADER_PERMUTATION_BOOL("IS_POST_PASS");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FBuildPassDim : SHADER_PERMUTATION_SPARSE_INT("RASTER_BIN_PASS", NANITE_RASTER_BIN_CLASSIFY, NANITE_RASTER_BIN_SCATTER);

	using FPermutationDomain = TShaderPermutationDomain<FIsPostPass, FVirtualTextureTargetDim, FBuildPassDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneParameters, GPUSceneParameters)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector4>, OutRasterizerBinHeaders)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutRasterizerBinArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector2>, OutRasterizerBinData)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector2>, InTotalPrevDrawClusters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector2>, InClusterCountSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InClusterOffsetSWHW)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialSlotTable)

		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, MaxVisibleClusters)
		SHADER_PARAMETER(uint32, RegularMaterialRasterBinCount)
		SHADER_PARAMETER(uint32, bUsePrimOrMeshShader)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinBuild_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinBuild", SF_Compute);

class FRasterBinReserve_CS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterBinReserve_CS);
	SHADER_USE_PARAMETER_STRUCT(FRasterBinReserve_CS, FNaniteGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutRangeAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutRasterizerBinArgsSWHW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector4>, OutRasterizerBinHeaders)

		SHADER_PARAMETER(uint32, RasterBinCount)
		SHADER_PARAMETER(uint32, RenderFlags)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return FNaniteGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RASTER_BIN_PASS"), NANITE_RASTER_BIN_RESERVE);
	}
};
IMPLEMENT_GLOBAL_SHADER(FRasterBinReserve_CS, "/Engine/Private/Nanite/NaniteRasterBinning.usf", "RasterBinReserve", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT( FRasterizePassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE( FGPUSceneParameters, GPUSceneParameters )
	SHADER_PARAMETER_STRUCT_INCLUDE( FRasterParameters, RasterParameters )

	SHADER_PARAMETER( FIntVector4,	PageConstants )
	SHADER_PARAMETER( uint32,		MaxVisibleClusters )
	SHADER_PARAMETER( uint32,		RenderFlags )
	SHADER_PARAMETER( uint32,		VisualizeModeBitMask )
	SHADER_PARAMETER( uint32,		ActiveRasterizerBin )
	SHADER_PARAMETER( FVector2f,	HardwareViewportSize )

	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer, ClusterPageData )
	SHADER_PARAMETER_SRV( ByteAddressBuffer, MaterialSlotTable )

	SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_Offsets )
	SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_Verts )
	SHADER_PARAMETER_SRV( ByteAddressBuffer,	TessellationTable_Indexes )

	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPackedView >,	InViews )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,					VisibleClustersSWHW )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FUintVector2 >,	InTotalPrevDrawClusters )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<uint>,			RasterizerBinData )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer<FUintVector4>,	RasterizerBinHeaders )

	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >, InClusterOffsetSWHW )

	RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)

	SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualTargetParameters, VirtualShadowMap)
END_SHADER_PARAMETER_STRUCT()

static uint32 GetMaterialBitFlags(const FMaterial& RasterMaterial, bool bMaterialUsesWorldPositionOffset, bool bMaterialUsesPixelDepthOffset, bool bForceDisableWPO)
{
	uint32 MaterialBitFlags = 0x00000000u;

	if (!bForceDisableWPO && bMaterialUsesWorldPositionOffset)
	{
		MaterialBitFlags |= NANITE_MATERIAL_FLAG_WORLD_POSITION_OFFSET;
	}

	if (bMaterialUsesPixelDepthOffset)
	{
		MaterialBitFlags |= NANITE_MATERIAL_FLAG_PIXEL_DEPTH_OFFSET;
	}

	if (RasterMaterial.IsMasked())
	{
		MaterialBitFlags |= NANITE_MATERIAL_FLAG_PIXEL_DISCARD;
	}

	if (false)
	{
		MaterialBitFlags |= NANITE_MATERIAL_FLAG_DYNAMIC_TESSELLATION;
	}

	return MaterialBitFlags;
}

class FMicropolyRasterizeCS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FMicropolyRasterizeCS, Material);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FTwoSidedDim : SHADER_PERMUTATION_BOOL("NANITE_TWO_SIDED");
	class FVisualizeDim : SHADER_PERMUTATION_BOOL("VISUALIZE");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	using FPermutationDomain = TShaderPermutationDomain<FDepthOnlyDim, FTwoSidedDim, FVisualizeDim, FVirtualTextureTargetDim, FVertexProgrammableDim, FPixelProgrammableDim>;

	using FParameters = FRasterizePassParameters;

	FMicropolyRasterizeCS() = default;
	FMicropolyRasterizeCS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
		: FNaniteMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}
		
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FDepthOnlyDim>() && PermutationVector.Get<FVisualizeDim>())
		{
			// Visualization not supported with depth only
			return false;
		}

		if (!Parameters.MaterialParameters.bIsDefaultMaterial && PermutationVector.Get<FTwoSidedDim>() != Parameters.MaterialParameters.bIsTwoSided)
		{
			return false;
		}

		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(Parameters.MaterialParameters, PermutationVector.Get<FVertexProgrammableDim>(), PermutationVector.Get<FPixelProgrammableDim>()))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompileComputePermutation(Parameters, AllowProgrammableRaster(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 1);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		if (PermutationVector.Get<FPixelProgrammableDim>())
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
			OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		}

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ShaderRHI, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FMicropolyRasterizeCS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("MicropolyRasterize"), SF_Compute);

class FHWRasterizeVS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FHWRasterizeVS, Material);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FPrimShaderDim : SHADER_PERMUTATION_BOOL("NANITE_PRIM_SHADER");
	class FAutoShaderCullDim : SHADER_PERMUTATION_BOOL("NANITE_AUTO_SHADER_CULL");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	using FPermutationDomain = TShaderPermutationDomain<FDepthOnlyDim, FPrimShaderDim, FAutoShaderCullDim, FVirtualTextureTargetDim, FVertexProgrammableDim, FPixelProgrammableDim>;

	using FParameters = FRasterizePassParameters;

	FHWRasterizeVS() = default;
	FHWRasterizeVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if ((PermutationVector.Get<FPrimShaderDim>() || PermutationVector.Get<FAutoShaderCullDim>()) &&
			!FDataDrivenShaderPlatformInfo::GetSupportsPrimitiveShaders(Parameters.Platform))
		{
			// Only some platforms support primitive shaders.
			return false;
		}

		if (PermutationVector.Get<FPrimShaderDim>() && PermutationVector.Get<FAutoShaderCullDim>())
		{
			// Mutually exclusive.
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(Parameters.MaterialParameters, PermutationVector.Get<FVertexProgrammableDim>(), PermutationVector.Get<FPixelProgrammableDim>()))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompileVertexPermutation(Parameters, AllowProgrammableRaster(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		const bool bIsPrimitiveShader = PermutationVector.Get<FPrimShaderDim>();
		
		if (bIsPrimitiveShader)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_VertexToPrimitiveShader);

			if (PermutationVector.Get<FVertexProgrammableDim>())
			{
				OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
				OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
			}
		}
		else if (PermutationVector.Get<FAutoShaderCullDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_VertexUseAutoCulling);
		}

		OutEnvironment.SetDefine(TEXT("NANITE_HW_COUNTER_INDEX"), bIsPrimitiveShader ? 4 : 5); // Mesh and primitive shaders use an index of 4 instead of 5

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FRHIVertexShader* ShaderRHI = RHICmdList.GetBoundVertexShader();

		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizeVS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizeVS"), SF_Vertex);

// TODO: Consider making a common base shader class for VS and MS (where possible)
class FHWRasterizeMS : public FNaniteMaterialShader
{
	DECLARE_SHADER_TYPE(FHWRasterizeMS, Material);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");
	using FPermutationDomain = TShaderPermutationDomain<FDepthOnlyDim, FVirtualTextureTargetDim, FVertexProgrammableDim, FPixelProgrammableDim>;

	using FParameters = FRasterizePassParameters;

	FHWRasterizeMS() = default;
	FHWRasterizeMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		if (!FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(Parameters.Platform))
		{
			// Only some platforms support mesh shaders with tier1 support
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(Parameters.MaterialParameters, PermutationVector.Get<FVertexProgrammableDim>(), PermutationVector.Get<FPixelProgrammableDim>()))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompileVertexPermutation(Parameters, AllowProgrammableRaster(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_HW_COUNTER_INDEX"), 4); // Mesh and primitive shaders use an index of 4 instead of 5
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		const uint32 MSThreadGroupSize = FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(Parameters.Platform);
		check(MSThreadGroupSize == 128 || MSThreadGroupSize == 256);

		if (PermutationVector.Get<FVertexProgrammableDim>())
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
			OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER_TG_SIZE"), 32);
			OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("NANITE_MESH_SHADER_TG_SIZE"), MSThreadGroupSize);
		}

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FRHIMeshShader* ShaderRHI = RHICmdList.GetBoundMeshShader();

		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizeMS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizeMS"), SF_Mesh);

class FHWRasterizePS : public FNaniteMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FHWRasterizePS, Material);

	class FDepthOnlyDim : SHADER_PERMUTATION_BOOL("DEPTH_ONLY");
	class FMeshShaderDim : SHADER_PERMUTATION_BOOL("NANITE_MESH_SHADER");
	class FPrimShaderDim : SHADER_PERMUTATION_BOOL("NANITE_PRIM_SHADER");
	class FVisualizeDim : SHADER_PERMUTATION_BOOL("VISUALIZE");
	class FVirtualTextureTargetDim : SHADER_PERMUTATION_BOOL("VIRTUAL_TEXTURE_TARGET");
	class FVertexProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_VERTEX_PROGRAMMABLE");
	class FPixelProgrammableDim : SHADER_PERMUTATION_BOOL("NANITE_PIXEL_PROGRAMMABLE");

	using FPermutationDomain = TShaderPermutationDomain
	<
		FDepthOnlyDim,
		FMeshShaderDim,
		FPrimShaderDim,
		FVisualizeDim,
		FVirtualTextureTargetDim,
		FVertexProgrammableDim,
		FPixelProgrammableDim
	>;

	using FParameters = FRasterizePassParameters;

	FHWRasterizePS() = default;
	FHWRasterizePS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
	: FNaniteMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false
		);
	}

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only some platforms support native 64-bit atomics.
		if (!FDataDrivenShaderPlatformInfo::GetSupportsUInt64ImageAtomics(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FDepthOnlyDim>() && PermutationVector.Get<FVisualizeDim>())
		{
			// Visualization not supported with depth only
			return false;
		}

		if (PermutationVector.Get<FMeshShaderDim>() &&
			!FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(Parameters.Platform))
		{
			// Only some platforms support mesh shaders with tier1 support.
			return false;
		}

		if (PermutationVector.Get<FPrimShaderDim>() &&
			!FDataDrivenShaderPlatformInfo::GetSupportsPrimitiveShaders(Parameters.Platform))
		{
			// Only some platforms support primitive shaders.
			return false;
		}

		if (PermutationVector.Get<FMeshShaderDim>() && PermutationVector.Get<FPrimShaderDim>())
		{
			// Mutually exclusive.
			return false;
		}

		// VSM rendering is depth-only and multiview
		if (PermutationVector.Get<FVirtualTextureTargetDim>() && !PermutationVector.Get<FDepthOnlyDim>())
		{
			return false;
		}

		if (!ShouldCompileProgrammablePermutation(Parameters.MaterialParameters, PermutationVector.Get<FVertexProgrammableDim>(), PermutationVector.Get<FPixelProgrammableDim>()))
		{
			return false;
		}

		return FNaniteMaterialShader::ShouldCompilePixelPermutation(Parameters, AllowProgrammableRaster(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FNaniteMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetRenderTargetOutputFormat(0, EPixelFormat::PF_R32_UINT);
		OutEnvironment.SetDefine(TEXT("SOFTWARE_RASTER"), 0);
		OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);

		if (PermutationVector.Get<FVertexProgrammableDim>() && (PermutationVector.Get<FMeshShaderDim>() || PermutationVector.Get<FPrimShaderDim>()))
		{
			OutEnvironment.SetDefine(TEXT("NANITE_VERT_REUSE_BATCH"), 1);
		}

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();

		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}
};
IMPLEMENT_MATERIAL_SHADER_TYPE(, FHWRasterizePS, TEXT("/Engine/Private/Nanite/NaniteRasterizer.usf"), TEXT("HWRasterizePS"), SF_Pixel);

namespace Nanite
{

void SetupProgrammableRasterizePermutationVectors(
	EOutputBufferMode RasterMode,
	bool bUseMeshShader,
	bool bUsePrimitiveShader,
	bool bUseAutoCullingShader,
	bool bVisualizeActive,
	bool bHasVirtualShadowMapArray,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS)
{
	PermutationVectorVS.Set<FHWRasterizeVS::FDepthOnlyDim>(RasterMode == EOutputBufferMode::DepthOnly);
	PermutationVectorVS.Set<FHWRasterizeVS::FPrimShaderDim>(bUsePrimitiveShader);
	PermutationVectorVS.Set<FHWRasterizeVS::FAutoShaderCullDim>(bUseAutoCullingShader);
	PermutationVectorVS.Set<FHWRasterizeVS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);

	PermutationVectorMS.Set<FHWRasterizeMS::FDepthOnlyDim>(RasterMode == EOutputBufferMode::DepthOnly);
	PermutationVectorMS.Set<FHWRasterizeMS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);

	PermutationVectorPS.Set<FHWRasterizePS::FDepthOnlyDim>(RasterMode == EOutputBufferMode::DepthOnly);
	PermutationVectorPS.Set<FHWRasterizePS::FMeshShaderDim>(bUseMeshShader);
	PermutationVectorPS.Set<FHWRasterizePS::FPrimShaderDim>(bUsePrimitiveShader);
	PermutationVectorPS.Set<FHWRasterizePS::FVisualizeDim>(bVisualizeActive && RasterMode != EOutputBufferMode::DepthOnly);
	PermutationVectorPS.Set<FHWRasterizePS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);

	// SW Rasterize
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FDepthOnlyDim>(RasterMode == EOutputBufferMode::DepthOnly);
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FVisualizeDim>(bVisualizeActive && RasterMode != EOutputBufferMode::DepthOnly);
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FVirtualTextureTargetDim>(bHasVirtualShadowMapArray);
}

static void GetMaterialShaderTypes(
	bool bVertexProgrammable,
	bool bPixelProgrammable,
	bool bUseMeshShader,
	bool bIsTwoSided,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS,
	FMaterialShaderTypes& ProgrammableShaderTypes,
	FMaterialShaderTypes& NonProgrammableShaderTypes)
{
	ProgrammableShaderTypes.PipelineType = nullptr;

	// Vertex/Mesh shader
	if (bUseMeshShader)
	{
		PermutationVectorMS.Set<FHWRasterizeMS::FVertexProgrammableDim>(bVertexProgrammable);
		PermutationVectorMS.Set<FHWRasterizeMS::FPixelProgrammableDim>(bPixelProgrammable);
		if (bVertexProgrammable)
		{
			ProgrammableShaderTypes.AddShaderType<FHWRasterizeMS>(PermutationVectorMS.ToDimensionValueId());
		}
		else
		{
			NonProgrammableShaderTypes.AddShaderType<FHWRasterizeMS>(PermutationVectorMS.ToDimensionValueId());
		}
	}
	else
	{
		PermutationVectorVS.Set<FHWRasterizeVS::FVertexProgrammableDim>(bVertexProgrammable);
		PermutationVectorVS.Set<FHWRasterizeVS::FPixelProgrammableDim>(bPixelProgrammable);
		if (bVertexProgrammable)
		{
			ProgrammableShaderTypes.AddShaderType<FHWRasterizeVS>(PermutationVectorVS.ToDimensionValueId());
		}
		else
		{
			NonProgrammableShaderTypes.AddShaderType<FHWRasterizeVS>(PermutationVectorVS.ToDimensionValueId());
		}
	}

	// Pixel Shader
	PermutationVectorPS.Set<FHWRasterizePS::FVertexProgrammableDim>(bVertexProgrammable);
	PermutationVectorPS.Set<FHWRasterizePS::FPixelProgrammableDim>(bPixelProgrammable);
	if (bPixelProgrammable)
	{
		ProgrammableShaderTypes.AddShaderType<FHWRasterizePS>(PermutationVectorPS.ToDimensionValueId());
	}
	else
	{
		NonProgrammableShaderTypes.AddShaderType<FHWRasterizePS>(PermutationVectorPS.ToDimensionValueId());
	}

	// Programmable micropoly features
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FTwoSidedDim>(bIsTwoSided);
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(bVertexProgrammable);
	PermutationVectorCS.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(bPixelProgrammable);
	if (bVertexProgrammable || bPixelProgrammable)
	{
		ProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeCS>(PermutationVectorCS.ToDimensionValueId());
	}
	else
	{
		NonProgrammableShaderTypes.AddShaderType<FMicropolyRasterizeCS>(PermutationVectorCS.ToDimensionValueId());
	}
}

void CollectRasterPSOInitializersForPermutation(
	const FMaterial& Material,
	bool bVertexProgrammable,
	bool bPixelProgrammable,
	bool bUseMeshShader,
	bool bUsePrimitiveShader,
	bool bIsTwoSided,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	FMaterialShaderTypes ProgrammableShaderTypes;
	FMaterialShaderTypes NonProgrammableShaderTypes;
	GetMaterialShaderTypes(bVertexProgrammable, bPixelProgrammable, bUseMeshShader, bIsTwoSided,
		PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS, ProgrammableShaderTypes, NonProgrammableShaderTypes);
	
	// retrieve shaders from default material for not programmable vertex or pixel shaders
	const FMaterialResource* FixedMaterialResource = UMaterial::GetDefaultMaterial(MD_Surface)->GetMaterialResource(Material.GetFeatureLevel(), Material.GetQualityLevel());
	check(FixedMaterialResource);

	FMaterialShaders ProgrammableShaders;
	FMaterialShaders NonProgrammableShaders;
	if (Material.TryGetShaders(ProgrammableShaderTypes, nullptr, ProgrammableShaders) && FixedMaterialResource->TryGetShaders(NonProgrammableShaderTypes, nullptr, NonProgrammableShaders))
	{		
		// Graphics PSO setup
		{			
			FGraphicsMinimalPipelineStateInitializer MinimalPipelineStateInitializer;
			MinimalPipelineStateInitializer.BlendState = TStaticBlendState<>::GetRHI();
			MinimalPipelineStateInitializer.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI(); // TODO: PROG_RASTER - Support depth clip as a rasterizer bin and remove shader permutations
			MinimalPipelineStateInitializer.PrimitiveType = bUsePrimitiveShader ? PT_PointList : PT_TriangleList;
			MinimalPipelineStateInitializer.BoundShaderState.VertexDeclarationRHI = bUseMeshShader ? nullptr : GEmptyVertexDeclaration.VertexDeclarationRHI;
			MinimalPipelineStateInitializer.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, bIsTwoSided ? CM_None : CM_CW);

#if PLATFORM_SUPPORTS_MESH_SHADERS
			if (bUseMeshShader)
			{		
				FMaterialShaders* MeshMaterialShaders = ProgrammableShaders.Shaders[SF_Mesh] ? &ProgrammableShaders : &NonProgrammableShaders;
				MinimalPipelineStateInitializer.BoundShaderState.MeshShaderResource = MeshMaterialShaders->ShaderMap->GetResource();
				MinimalPipelineStateInitializer.BoundShaderState.MeshShaderIndex = MeshMaterialShaders->Shaders[SF_Mesh]->GetResourceIndex();
			}
			else
#else
			check(!bUseMeshShader);
#endif // PLATFORM_SUPPORTS_MESH_SHADERS
			{
				FMaterialShaders* VertexMaterialShaders = ProgrammableShaders.Shaders[SF_Vertex] ? &ProgrammableShaders : &NonProgrammableShaders;
				MinimalPipelineStateInitializer.BoundShaderState.VertexShaderResource = VertexMaterialShaders->ShaderMap->GetResource();
				MinimalPipelineStateInitializer.BoundShaderState.VertexShaderIndex = VertexMaterialShaders->Shaders[SF_Vertex]->GetResourceIndex();
			}

			FMaterialShaders* PixelMaterialShaders = ProgrammableShaders.Shaders[SF_Pixel] ? &ProgrammableShaders : &NonProgrammableShaders;
			MinimalPipelineStateInitializer.BoundShaderState.PixelShaderResource = PixelMaterialShaders->ShaderMap->GetResource();
			MinimalPipelineStateInitializer.BoundShaderState.PixelShaderIndex = PixelMaterialShaders->Shaders[SF_Pixel]->GetResourceIndex();

			MinimalPipelineStateInitializer.ComputePrecachePSOHash();
#if PSO_PRECACHING_VALIDATE
			PSOCollectorStats::AddMinimalPipelineStateToCache(MinimalPipelineStateInitializer, (uint32)EMeshPass::NaniteMeshPass, nullptr);
#endif // PSO_PRECACHING_VALIDATE

			// NOTE: AsGraphicsPipelineStateInitializer will create the RHIShaders internally if they are not cached yet
			FGraphicsPipelineStateInitializer GraphicsPSOInit = MinimalPipelineStateInitializer.AsGraphicsPipelineStateInitializer();
			
			FPSOPrecacheData PSOPrecacheData;
			PSOPrecacheData.Type = FPSOPrecacheData::EType::Graphics;
			PSOPrecacheData.GraphicsPSOInitializer = GraphicsPSOInit;
#if PSO_PRECACHING_VALIDATE
			PSOPrecacheData.MeshPassType = (uint32)EMeshPass::NaniteMeshPass;
			PSOPrecacheData.VertexFactoryType = nullptr;
#endif // PSO_PRECACHING_VALIDATE
			PSOInitializers.Add(PSOPrecacheData);
		}

		// Compute PSO setup
		TShaderRef<FMicropolyRasterizeCS> MicropolyRasterizeCS;
		if (ProgrammableShaders.TryGetComputeShader(&MicropolyRasterizeCS))
		{
			FPSOPrecacheData ComputePSOPrecacheData;
			ComputePSOPrecacheData.Type = FPSOPrecacheData::EType::Compute;
			ComputePSOPrecacheData.ComputeShader = MicropolyRasterizeCS.GetComputeShader();
#if PSO_PRECACHING_VALIDATE
			ComputePSOPrecacheData.MeshPassType = (uint32)EMeshPass::NaniteMeshPass;
#endif // PSO_PRECACHING_VALIDATE
			PSOInitializers.Add(ComputePSOPrecacheData);
		}
	}
}

void CollectRasterPSOInitializersForDefaultMaterial(
	const FMaterial& Material,
	bool bUseMeshShader,
	bool bUsePrimitiveShader,
	FHWRasterizeVS::FPermutationDomain& PermutationVectorVS,
	FHWRasterizeMS::FPermutationDomain& PermutationVectorMS,
	FHWRasterizePS::FPermutationDomain& PermutationVectorPS,
	FMicropolyRasterizeCS::FPermutationDomain& PermutationVectorCS,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Collect PSOs for all possible combinations of vertex/pixel programmable and if two sided or not
	for (uint32 VertexProgrammable = 0; VertexProgrammable < 2; ++VertexProgrammable)
	{
		bool bVertexProgrammable = VertexProgrammable > 0;
		for (uint32 PixelProgrammable = 0; PixelProgrammable < 2; ++PixelProgrammable)
		{
			bool bPixelProgrammable = PixelProgrammable > 0;
			for (uint32 IsTwoSided = 0; IsTwoSided < 2; ++IsTwoSided)
			{
				bool bIsTwoSided = IsTwoSided > 0;
				CollectRasterPSOInitializersForPermutation(Material, bVertexProgrammable, bPixelProgrammable, bUseMeshShader, bUsePrimitiveShader, bIsTwoSided,
					PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS, PSOInitializers);
			}
		}
	}
}

void CollectRasterPSOInitializersForPipeline(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& RasterMaterial,
	const FPSOPrecacheParams& PreCacheParams,
	EShaderPlatform ShaderPlatform,
	EPipeline Pipeline,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	const bool bUseMeshShader = UseMeshShader(ShaderPlatform, Pipeline);
	const bool bUsePrimitiveShader = UsePrimitiveShader() && !bUseMeshShader;
	const EOutputBufferMode RasterMode = Pipeline == EPipeline::Shadows ? EOutputBufferMode::DepthOnly : EOutputBufferMode::VisBuffer;
	const bool bHasVirtualShadowMapArray = Pipeline == EPipeline::Shadows; // true during shadow pass
	const bool bVisualizeActive = false; // no precache for visualization modes
	const bool bUseAutoCullingShader = UseAutoCullingShader(bUsePrimitiveShader);
	const bool bForceDisableWPO = false; // no precache for force disable WPO
		
	FHWRasterizeVS::FPermutationDomain PermutationVectorVS;
	FHWRasterizeMS::FPermutationDomain PermutationVectorMS;
	FHWRasterizePS::FPermutationDomain PermutationVectorPS;
	FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS;
	SetupProgrammableRasterizePermutationVectors(RasterMode, bUseMeshShader, bUsePrimitiveShader, bUseAutoCullingShader, bVisualizeActive, bHasVirtualShadowMapArray,
		PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS);

	if (PreCacheParams.bDefaultMaterial)
	{
		CollectRasterPSOInitializersForDefaultMaterial(RasterMaterial, bUseMeshShader, bUsePrimitiveShader, PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS, PSOInitializers);
	}
	else
	{
		const uint32 MaterialBitFlags = GetMaterialBitFlags(RasterMaterial, RasterMaterial.MaterialUsesWorldPositionOffset_GameThread(), RasterMaterial.MaterialUsesPixelDepthOffset_GameThread(), bForceDisableWPO);
		const bool bVertexProgrammable = FNaniteMaterialShader::IsVertexProgrammable(MaterialBitFlags);
		const bool bPixelProgrammable = FNaniteMaterialShader::IsPixelProgrammable(MaterialBitFlags);

		const FMeshPassProcessor::FMeshDrawingPolicyOverrideSettings OverrideSettings = FMeshPassProcessor::ComputeMeshOverrideSettings(PreCacheParams);
		ERasterizerCullMode MeshCullMode = FMeshPassProcessor::ComputeMeshCullMode(RasterMaterial, OverrideSettings);
		const bool bIsTwoSided = MeshCullMode == CM_None;

		CollectRasterPSOInitializersForPermutation(RasterMaterial, bVertexProgrammable, bPixelProgrammable, bUseMeshShader, bUsePrimitiveShader, bIsTwoSided,
			PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS, PSOInitializers);
	}
}

void CollectRasterPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& RasterMaterial,
	const FPSOPrecacheParams& PreCacheParams,
	EShaderPlatform ShaderPlatform,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	if (!GNaniteProgrammableRaster)
	{
		return;
	}

	// Collect for primary & shadows
	CollectRasterPSOInitializersForPipeline(SceneTexturesConfig, RasterMaterial, PreCacheParams, ShaderPlatform, EPipeline::Primary, PSOInitializers);
	CollectRasterPSOInitializersForPipeline(SceneTexturesConfig, RasterMaterial, PreCacheParams, ShaderPlatform, EPipeline::Shadows, PSOInitializers);
}


class FTessellationTableResources : public FRenderResource
{
public:
	FByteAddressBuffer	Offsets;
	FByteAddressBuffer	Verts;
	FByteAddressBuffer	Indexes;

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
};

template< typename T >
static void CreateAndUpload( FByteAddressBuffer& Buffer, const TArray<T>& Array, const TCHAR* InDebugName )
{
	Buffer.Initialize( InDebugName, Array.Num() * Array.GetTypeSize() );

	uint8* DataPtr = (uint8*)RHILockBuffer( Buffer.Buffer, 0, Buffer.NumBytes, RLM_WriteOnly );

	FMemory::Memcpy( DataPtr, Array.GetData(), Buffer.NumBytes );

	RHIUnlockBuffer( Buffer.Buffer );
}

void FTessellationTableResources::InitRHI()
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		FTessellationTable TessellationTable(8);

		CreateAndUpload( Offsets,	TessellationTable.OffsetTable,	TEXT("TessellationTable.Offsets") );
		CreateAndUpload( Verts,		TessellationTable.Verts,		TEXT("TessellationTable.Verts") );
		CreateAndUpload( Indexes,	TessellationTable.Indexes,		TEXT("TessellationTable.Indexes") );
	}
}

void FTessellationTableResources::ReleaseRHI()
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		Offsets.Release();
		Verts.Release();
		Indexes.Release();
	}
}

TGlobalResource< FTessellationTableResources > GTessellationTable;


static void AddPassInitNodesAndClusterBatchesUAV( FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGBufferUAVRef UAVRef )
{
	LLM_SCOPE_BYTAG(Nanite);

	{
		FInitCandidateNodes_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitCandidateNodes_CS::FParameters >();
		PassParameters->OutMainAndPostNodesAndClusterBatches= UAVRef;
		PassParameters->MaxCandidateClusters				= Nanite::FGlobalResources::GetMaxCandidateClusters();
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();

		auto ComputeShader = ShaderMap->GetShader< FInitCandidateNodes_CS >();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "Nanite::InitNodes" ),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(Nanite::FGlobalResources::GetMaxNodes(), 64)
		);
	}

	{
		FInitClusterBatches_CS::FParameters* PassParameters	= GraphBuilder.AllocParameters< FInitClusterBatches_CS::FParameters >();
		PassParameters->OutMainAndPostNodesAndClusterBatches= UAVRef;
		PassParameters->MaxCandidateClusters				= Nanite::FGlobalResources::GetMaxCandidateClusters();
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();

		auto ComputeShader = ShaderMap->GetShader< FInitClusterBatches_CS >();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "Nanite::InitCullingBatches" ),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(Nanite::FGlobalResources::GetMaxClusterBatches(), 64)
		);
	}
}

FCullingContext InitCullingContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FScene& Scene,
	const TRefCountPtr<IPooledRenderTarget> &PrevHZB,
	const FIntRect &HZBBuildViewRect,
	const FCullingContext::FConfiguration& Configuration
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::InitContext");

	INC_DWORD_STAT(STAT_NaniteCullingContexts);

	const EShaderPlatform ShaderPlatform = Scene.GetShaderPlatform();

	FCullingContext CullingContext = {};
	CullingContext.PrevHZB					= PrevHZB;
	CullingContext.HZBBuildViewRect			= HZBBuildViewRect;
	CullingContext.Configuration			= Configuration;
	CullingContext.DrawPassIndex			= 0;
	CullingContext.RenderFlags				= 0;
	CullingContext.DebugFlags				= 0;

	// Disable two pass occlusion if previous HZB is invalid
	if (CullingContext.PrevHZB == nullptr)
	{
		CullingContext.Configuration.bTwoPassOcclusion = false;
	}

	if (!AllowProgrammableRaster(ShaderPlatform) || GNaniteProgrammableRaster == 0)
	{
		// Never use programmable raster if the material shaders are unavailable (or if globally disabled).
		CullingContext.Configuration.bProgrammableRaster = false;
	}

	CullingContext.RenderFlags |= CullingContext.Configuration.bProgrammableRaster		? NANITE_RENDER_FLAG_PROGRAMMABLE_RASTER : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bForceHWRaster			? NANITE_RENDER_FLAG_FORCE_HW_RASTER : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bUpdateStreaming			? NANITE_RENDER_FLAG_OUTPUT_STREAMING_REQUESTS : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bIsSceneCapture			? NANITE_RENDER_FLAG_IS_SCENE_CAPTURE : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bIsReflectionCapture		? NANITE_RENDER_FLAG_IS_REFLECTION_CAPTURE : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bIsLumenCapture			? NANITE_RENDER_FLAG_IS_LUMEN_CAPTURE : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bIsGameView				? NANITE_RENDER_FLAG_IS_GAME_VIEW : 0u;
	CullingContext.RenderFlags |= CullingContext.Configuration.bGameShowFlag			? NANITE_RENDER_FLAG_GAME_SHOW_FLAG_ENABLED : 0u;
#if WITH_EDITOR
	CullingContext.RenderFlags |= CullingContext.Configuration.bEditorShowFlag			? NANITE_RENDER_FLAG_EDITOR_SHOW_FLAG_ENABLED : 0u;
#endif

	if (UseMeshShader(ShaderPlatform, SharedContext.Pipeline))
	{
		CullingContext.RenderFlags |= NANITE_RENDER_FLAG_MESH_SHADER;
	}
	else if (UsePrimitiveShader())
	{
		CullingContext.RenderFlags |= NANITE_RENDER_FLAG_PRIMITIVE_SHADER;
	}

	// TODO: Exclude from shipping builds
	{
		if (GNaniteCullingFrustum == 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_FRUSTUM;
		}

		if (GNaniteCullingHZB == 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_HZB;
		}

		if (GNaniteCullingGlobalClipPlane == 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_GLOBAL_CLIP_PLANE;
		}

		if (GNaniteCullingDrawDistance == 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_CULL_DRAW_DISTANCE;
		}

		if (GNaniteCullingWPODisableDistance == 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DISABLE_WPO_DISABLE_DISTANCE;
		}

		if (GNaniteShowStats != 0)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_WRITE_STATS;
		}

		if (Configuration.bDrawOnlyVSMInvalidatingGeometry && Configuration.bPrimaryContext)
		{
			CullingContext.DebugFlags |= NANITE_DEBUG_FLAG_DRAW_ONLY_VSM_INVALIDATING;
		}
	}

	// TODO: Might this not break if the view has overridden the InstanceSceneData?
	const uint32 NumSceneInstancesPo2				= FMath::RoundUpToPowerOfTwo(Scene.GPUScene.InstanceSceneDataAllocator.GetMaxSize());
	CullingContext.PageConstants.X					= Scene.GPUScene.InstanceSceneDataSOAStride;
	CullingContext.PageConstants.Y					= Nanite::GStreamingManager.GetMaxStreamingPages();
	
	check(NumSceneInstancesPo2 <= NANITE_MAX_INSTANCES); // There are too many instances in the scene.

	CullingContext.QueueState						= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( (6*2 + 1) * sizeof(uint32), 1), TEXT("Nanite.QueueState"));

	FRDGBufferDesc VisibleClustersDesc				= FRDGBufferDesc::CreateStructuredDesc(4, 3 * Nanite::FGlobalResources::GetMaxVisibleClusters());	// Max visible clusters * sizeof(uint3)
	VisibleClustersDesc.Usage						= EBufferUsageFlags(VisibleClustersDesc.Usage | BUF_ByteAddressBuffer);

	CullingContext.VisibleClustersSWHW				= GraphBuilder.CreateBuffer(VisibleClustersDesc, TEXT("Nanite.VisibleClustersSWHW"));

	CullingContext.MainRasterizeArgsSWHW			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.MainRasterizeArgsSWHW"));
	CullingContext.SafeMainRasterizeArgsSWHW		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.SafeMainRasterizeArgsSWHW"));
	
	if (CullingContext.Configuration.bTwoPassOcclusion)
	{
		CullingContext.OccludedInstances			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FInstanceDraw), NumSceneInstancesPo2), TEXT("Nanite.OccludedInstances"));
		CullingContext.OccludedInstancesArgs		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("Nanite.OccludedInstancesArgs"));
		CullingContext.PostRasterizeArgsSWHW		= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.PostRasterizeArgsSWHW"));
		CullingContext.SafePostRasterizeArgsSWHW	= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.SafePostRasterizeArgsSWHW"));
	}

	if (CullingContext.Configuration.bProgrammableRaster)
	{
		CullingContext.ClusterCountSWHW				= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector2), 1), TEXT("Nanite.SWHWClusterCount"));
		CullingContext.ClusterClassifyArgs			= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Nanite.ClusterClassifyArgs"));
	}
	else
	{
		CullingContext.ClusterCountSWHW				= nullptr;
		CullingContext.ClusterClassifyArgs			= nullptr;
	}

	CullingContext.StreamingRequests = Nanite::GStreamingManager.GetStreamingRequestsBuffer(GraphBuilder);
	
	if (CullingContext.Configuration.bSupportsMultiplePasses)
	{
		CullingContext.TotalPrevDrawClustersBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(8, 1), TEXT("Nanite.TotalPrevDrawClustersBuffer"));
	}

	return CullingContext;
}

void AddPass_PrimitiveFilter(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& SceneView,
	const FGPUSceneParameters& GPUSceneParameters,
	const FSharedContext& SharedContext,
	FCullingContext& CullingContext
)
{
	LLM_SCOPE_BYTAG(Nanite);

	const uint32 PrimitiveCount = uint32(Scene.Primitives.Num());
	const uint32 HiddenPrimitiveCount = SceneView.HiddenPrimitives.Num();
	const uint32 ShowOnlyPrimitiveCount = SceneView.ShowOnlyPrimitives.IsSet() ? SceneView.ShowOnlyPrimitives->Num() : 0u;
	
	EFilterFlags HiddenFilterFlags = EFilterFlags::None;
	
	if (!SceneView.Family->EngineShowFlags.StaticMeshes)
	{
		HiddenFilterFlags |= EFilterFlags::StaticMesh;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedStaticMeshes)
	{
		HiddenFilterFlags |= EFilterFlags::InstancedStaticMesh;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedFoliage)
	{
		HiddenFilterFlags |= EFilterFlags::Foliage;
	}

	if (!SceneView.Family->EngineShowFlags.InstancedGrass)
	{
		HiddenFilterFlags |= EFilterFlags::Grass;
	}

	if (!SceneView.Family->EngineShowFlags.Landscape)
	{
		HiddenFilterFlags |= EFilterFlags::Landscape;
	}

	CullingContext.PrimitiveFilterBuffer = nullptr;
	CullingContext.HiddenPrimitivesBuffer = nullptr;
	CullingContext.ShowOnlyPrimitivesBuffer = nullptr;

	const bool bAnyPrimitiveFilter = (HiddenPrimitiveCount + ShowOnlyPrimitiveCount) > 0;
	const bool bAnyFilterFlags = PrimitiveCount > 0 && HiddenFilterFlags != EFilterFlags::None;
	
	if (CVarNaniteFilterPrimitives.GetValueOnRenderThread() != 0 && (bAnyPrimitiveFilter || bAnyFilterFlags))
	{
		check(PrimitiveCount > 0);
		const uint32 DWordCount = FMath::DivideAndRoundUp(PrimitiveCount, 32u); // 32 primitive bits per uint32
		const uint32 PrimitiveFilterBufferElements = FMath::RoundUpToPowerOfTwo(DWordCount);

		CullingContext.PrimitiveFilterBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PrimitiveFilterBufferElements), TEXT("Nanite.PrimitiveFilter"));
		FRDGBufferUAVRef PrimitiveFilterBufferUAV = GraphBuilder.CreateUAV(CullingContext.PrimitiveFilterBuffer);

		// Zeroed initially to indicate "all primitives unfiltered / visible"
		AddClearUAVPass(GraphBuilder, PrimitiveFilterBufferUAV, 0);

		// Create buffer from "show only primitives" set
		if (ShowOnlyPrimitiveCount > 0)
		{
			TArray<uint32, SceneRenderingAllocator> ShowOnlyPrimitiveIds;
			ShowOnlyPrimitiveIds.Reserve(FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount));

			const TSet<FPrimitiveComponentId>& ShowOnlyPrimitivesSet = SceneView.ShowOnlyPrimitives.GetValue();
			for (TSet<FPrimitiveComponentId>::TConstIterator It(ShowOnlyPrimitivesSet); It; ++It)
			{
				ShowOnlyPrimitiveIds.Add(It->PrimIDValue);
			}

			// Add extra entries to ensure the buffer is valid pow2 in size
			ShowOnlyPrimitiveIds.SetNumZeroed(FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount));

			// Sort the buffer by ascending value so the GPU binary search works properly
			Algo::Sort(ShowOnlyPrimitiveIds);

			CullingContext.ShowOnlyPrimitivesBuffer = CreateUploadBuffer(
				GraphBuilder,
				TEXT("Nanite.ShowOnlyPrimitivesBuffer"),
				sizeof(uint32),
				ShowOnlyPrimitiveIds.Num(),
				ShowOnlyPrimitiveIds.GetData(),
				sizeof(uint32) * ShowOnlyPrimitiveIds.Num()
			);
		}

		// Create buffer from "hidden primitives" set
		if (HiddenPrimitiveCount > 0)
		{
			TArray<uint32, SceneRenderingAllocator> HiddenPrimitiveIds;
			HiddenPrimitiveIds.Reserve(FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount));

			for (TSet<FPrimitiveComponentId>::TConstIterator It(SceneView.HiddenPrimitives); It; ++It)
			{
				HiddenPrimitiveIds.Add(It->PrimIDValue);
			}

			// Add extra entries to ensure the buffer is valid pow2 in size
			HiddenPrimitiveIds.SetNumZeroed(FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount));

			// Sort the buffer by ascending value so the GPU binary search works properly
			Algo::Sort(HiddenPrimitiveIds);

			CullingContext.HiddenPrimitivesBuffer = CreateUploadBuffer(
				GraphBuilder,
				TEXT("Nanite.HiddenPrimitivesBuffer"),
				sizeof(uint32),
				HiddenPrimitiveIds.Num(),
				HiddenPrimitiveIds.GetData(),
				sizeof(uint32) * HiddenPrimitiveIds.Num()
			);
		}

		FPrimitiveFilter_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPrimitiveFilter_CS::FParameters>();

		PassParameters->NumPrimitives = PrimitiveCount;
		PassParameters->HiddenFilterFlags = uint32(HiddenFilterFlags);
		PassParameters->NumHiddenPrimitives = FMath::RoundUpToPowerOfTwo(HiddenPrimitiveCount);
		PassParameters->NumShowOnlyPrimitives = FMath::RoundUpToPowerOfTwo(ShowOnlyPrimitiveCount);
		PassParameters->GPUSceneParameters = GPUSceneParameters;
		PassParameters->PrimitiveFilterBuffer = PrimitiveFilterBufferUAV;

		if (CullingContext.HiddenPrimitivesBuffer != nullptr)
		{
			PassParameters->HiddenPrimitivesList = GraphBuilder.CreateSRV(CullingContext.HiddenPrimitivesBuffer, PF_R32_UINT);
		}

		if (CullingContext.ShowOnlyPrimitivesBuffer != nullptr)
		{
			PassParameters->ShowOnlyPrimitivesList = GraphBuilder.CreateSRV(CullingContext.ShowOnlyPrimitivesBuffer, PF_R32_UINT);
		}

		FPrimitiveFilter_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FPrimitiveFilter_CS::FHiddenPrimitivesListDim>(CullingContext.HiddenPrimitivesBuffer != nullptr);
		PermutationVector.Set<FPrimitiveFilter_CS::FShowOnlyPrimitivesListDim>(CullingContext.ShowOnlyPrimitivesBuffer != nullptr);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FPrimitiveFilter_CS>(PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PrimitiveFilter"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(PrimitiveCount, 64)
		);
	}
}

static void AddPass_InitCullArgs(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const FSharedContext& SharedContext,
	const FCullingContext& CullingContext,
	FRDGBufferRef CullArgs,
	uint32 CullingPass,
	uint32 CullingType
)
{
	check(CullingType == NANITE_CULLING_TYPE_NODES || CullingType == NANITE_CULLING_TYPE_CLUSTERS);
	FInitCullArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitCullArgs_CS::FParameters >();

	PassParameters->OutQueueState			= GraphBuilder.CreateUAV(CullingContext.QueueState);
	PassParameters->OutCullArgs				= GraphBuilder.CreateUAV(CullArgs);
	PassParameters->MaxCandidateClusters	= Nanite::FGlobalResources::GetMaxCandidateClusters();
	PassParameters->MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
	PassParameters->InitIsPostPass			= (CullingPass == CULLING_PASS_OCCLUSION_POST) ? 1 : 0;

	FInitCullArgs_CS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FInitCullArgs_CS::FCullingTypeDim>(CullingType);
	auto ComputeShader = SharedContext.ShaderMap->GetShader<FInitCullArgs_CS>(PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		Forward<FRDGEventName>(PassName),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1)
	);
}

static void AddPass_NodeAndClusterCull(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& PassName,
	const FCullingParameters& CullingParameters,
	const FSharedContext& SharedContext,
	const FCullingContext& CullingContext,
	const FGPUSceneParameters& GPUSceneParameters,
	FRDGBufferRef MainAndPostNodesAndClusterBatchesBuffer,
	FRDGBufferRef MainAndPostCandididateClustersBuffer,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	FVirtualTargetParameters& VirtualTargetParameters,
	FRDGBufferRef IndirectArgs,
	uint32 CullingPass,
	uint32 CullingType,
	bool bMultiView
	)
{
	FNodeAndClusterCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FNodeAndClusterCull_CS::FParameters >();

	PassParameters->GPUSceneParameters		= GPUSceneParameters;
	PassParameters->CullingParameters		= CullingParameters;
	PassParameters->MaxNodes				= Nanite::FGlobalResources::GetMaxNodes();
		
	PassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	PassParameters->HierarchyBuffer			= Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
		
	check(CullingContext.DrawPassIndex == 0 || CullingContext.RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA); // sanity check
	if (CullingContext.RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA)
	{
		PassParameters->InTotalPrevDrawClusters = GraphBuilder.CreateSRV(CullingContext.TotalPrevDrawClustersBuffer);
	}
	else
	{
		FRDGBufferRef Dummy = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);
		PassParameters->InTotalPrevDrawClusters = GraphBuilder.CreateSRV(Dummy);
	}

	PassParameters->QueueState							= GraphBuilder.CreateUAV(CullingContext.QueueState);
	PassParameters->MainAndPostNodesAndClusterBatches	= GraphBuilder.CreateUAV(MainAndPostNodesAndClusterBatchesBuffer);
	PassParameters->MainAndPostCandididateClusters		= GraphBuilder.CreateUAV(MainAndPostCandididateClustersBuffer);

	if( CullingPass == CULLING_PASS_NO_OCCLUSION || CullingPass == CULLING_PASS_OCCLUSION_MAIN )
	{
		PassParameters->VisibleClustersArgsSWHW	= GraphBuilder.CreateUAV( CullingContext.MainRasterizeArgsSWHW );
	}
	else
	{
		PassParameters->OffsetClustersArgsSWHW	= GraphBuilder.CreateSRV( CullingContext.MainRasterizeArgsSWHW );
		PassParameters->VisibleClustersArgsSWHW	= GraphBuilder.CreateUAV( CullingContext.PostRasterizeArgsSWHW );
	}

	PassParameters->OutVisibleClustersSWHW			= GraphBuilder.CreateUAV( CullingContext.VisibleClustersSWHW );
	PassParameters->OutStreamingRequests			= GraphBuilder.CreateUAV( CullingContext.StreamingRequests );

	if (VirtualShadowMapArray)
	{
		PassParameters->VirtualShadowMap = VirtualTargetParameters;
		if (CullingPass == CULLING_PASS_OCCLUSION_POST)
		{
			PassParameters->VirtualShadowMap = VirtualTargetParameters;
			// Set the HZB page table and flags to match the now rebuilt HZB for the current frame
			PassParameters->VirtualShadowMap.HZBPageTable = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageTableRDG);
			PassParameters->VirtualShadowMap.HZBPageRectBounds = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageRectBoundsRDG);
			PassParameters->VirtualShadowMap.HZBPageFlags = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageFlagsRDG);
		}
	}

	if (CullingContext.StatsBuffer)
	{
		PassParameters->OutStatsBuffer = GraphBuilder.CreateUAV(CullingContext.StatsBuffer);
	}

	PassParameters->LargePageRectThreshold = CVarLargePageRectThreshold.GetValueOnRenderThread();
	PassParameters->StreamingRequestsBufferVersion = GStreamingManager.GetStreamingRequestsBufferVersion();

	check(CullingContext.ViewsBuffer);

	FNodeAndClusterCull_CS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FNodeAndClusterCull_CS::FCullingPassDim>(CullingPass);
	PermutationVector.Set<FNodeAndClusterCull_CS::FMultiViewDim>(bMultiView);
	PermutationVector.Set<FNodeAndClusterCull_CS::FVirtualTextureTargetDim>(VirtualShadowMapArray != nullptr);
	PermutationVector.Set<FNodeAndClusterCull_CS::FDebugFlagsDim>(CullingContext.DebugFlags != 0);
	PermutationVector.Set<FNodeAndClusterCull_CS::FCullingTypeDim>(CullingType);
	auto ComputeShader = SharedContext.ShaderMap->GetShader<FNodeAndClusterCull_CS>(PermutationVector);

	if (CullingType == NANITE_CULLING_TYPE_NODES || CullingType == NANITE_CULLING_TYPE_CLUSTERS)
	{
		PassParameters->IndirectArgs = IndirectArgs;
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			Forward<FRDGEventName>(PassName),
			ComputeShader,
			PassParameters,
			IndirectArgs,
			0
		);
	}
	else if(CullingType == NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS)
	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			Forward<FRDGEventName>(PassName),
			ComputeShader,
			PassParameters,
			FIntVector(GRHIPersistentThreadGroupCount, 1, 1)
		);
	}
	else
	{
		checkf(false, TEXT("Unknown culling type: %d"), CullingType);
	}
}

static void AddPass_NodeAndClusterCull(
	FRDGBuilder& GraphBuilder,
	const FCullingParameters& CullingParameters,
	const FSharedContext& SharedContext,
	const FCullingContext& CullingContext,
	const FGPUSceneParameters& GPUSceneParameters,
	FRDGBufferRef MainAndPostNodesAndClusterBatchesBuffer,
	FRDGBufferRef MainAndPostCandididateClustersBuffer,
	uint32 CullingPass,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	FVirtualTargetParameters& VirtualTargetParameters,
	bool bMultiView
	)
{
	if (GNanitePersistentThreadsCulling)
	{
		AddPass_NodeAndClusterCull( GraphBuilder,
									RDG_EVENT_NAME("PersistentCull"),
									CullingParameters, SharedContext, CullingContext, GPUSceneParameters,
									MainAndPostNodesAndClusterBatchesBuffer, MainAndPostCandididateClustersBuffer,
									VirtualShadowMapArray, VirtualTargetParameters,
									nullptr,
									CullingPass,
									NANITE_CULLING_TYPE_PERSISTENT_NODES_AND_CLUSTERS,
									bMultiView);
	}
	else
	{
		RDG_EVENT_SCOPE(GraphBuilder, "NodeAndClusterCull");

		FRDGBufferRef NodeCullArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Nanite.NodeCullArgs"));

		const uint32 MaxNodeLevels = 12;	// TODO: Calculate max based on installed pages?
		for (uint32 NodeLevel = 0; NodeLevel < MaxNodeLevels; NodeLevel++)
		{
			AddPass_InitCullArgs(GraphBuilder, RDG_EVENT_NAME("InitNodeCullArgs"), SharedContext, CullingContext, NodeCullArgs, CullingPass, NANITE_CULLING_TYPE_NODES);
			
			AddPass_NodeAndClusterCull(
				GraphBuilder,
				RDG_EVENT_NAME("NodeCull_%d", NodeLevel),
				CullingParameters, SharedContext, CullingContext, GPUSceneParameters,
				MainAndPostNodesAndClusterBatchesBuffer, MainAndPostCandididateClustersBuffer,
				VirtualShadowMapArray, VirtualTargetParameters,
				NodeCullArgs,
				CullingPass,
				NANITE_CULLING_TYPE_NODES,
				bMultiView);
		}

		FRDGBufferRef ClusterCullArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Nanite.ClusterCullArgs"));

		AddPass_InitCullArgs(GraphBuilder, RDG_EVENT_NAME("InitClusterCullArgs"), SharedContext, CullingContext, ClusterCullArgs, CullingPass, NANITE_CULLING_TYPE_CLUSTERS);

		AddPass_NodeAndClusterCull(
			GraphBuilder,
			RDG_EVENT_NAME("ClusterCull"),
			CullingParameters, SharedContext, CullingContext, GPUSceneParameters,
			MainAndPostNodesAndClusterBatchesBuffer, MainAndPostCandididateClustersBuffer,
			VirtualShadowMapArray, VirtualTargetParameters,
			ClusterCullArgs,
			CullingPass,
			NANITE_CULLING_TYPE_CLUSTERS,
			bMultiView);
	}
}

static void AddPass_InstanceHierarchyAndClusterCull(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FCullingParameters& CullingParameters,
	const TArray<FPackedView, SceneRenderingAllocator>& Views,
	const uint32 NumPrimaryViews,
	const FSharedContext& SharedContext,
	const FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	const FGPUSceneParameters &GPUSceneParameters,
	FRDGBufferRef MainAndPostNodesAndClusterBatchesBuffer,
	FRDGBufferRef MainAndPostCandididateClustersBuffer,
	uint32 CullingPass,
	FVirtualShadowMapArray *VirtualShadowMapArray,
	FVirtualTargetParameters &VirtualTargetParameters
	)
{
	LLM_SCOPE_BYTAG(Nanite);

	checkf(GRHIPersistentThreadGroupCount > 0, TEXT("GRHIPersistentThreadGroupCount must be configured correctly in the RHI."));

	const bool bMultiView = Views.Num() > 1 || VirtualShadowMapArray != nullptr;

	FRDGBufferRef Dummy = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);

	if (VirtualShadowMapArray && (CullingPass != CULLING_PASS_OCCLUSION_POST))
	{
		FInstanceCullVSM_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceCullVSM_CS::FParameters >();

		PassParameters->NumInstances						= CullingContext.NumInstancesPreCull;
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();
		
		PassParameters->GPUSceneParameters = GPUSceneParameters;
		PassParameters->CullingParameters = CullingParameters;

		PassParameters->VirtualShadowMap = VirtualTargetParameters;		
		
		PassParameters->OutQueueState						= GraphBuilder.CreateUAV( CullingContext.QueueState );

		if (CullingContext.StatsBuffer)
		{
			PassParameters->OutStatsBuffer					= GraphBuilder.CreateUAV(CullingContext.StatsBuffer);
		}

		if (CullingContext.PrimitiveFilterBuffer)
		{
			PassParameters->InPrimitiveFilterBuffer			= GraphBuilder.CreateSRV(CullingContext.PrimitiveFilterBuffer);
		}

		check( CullingContext.InstanceDrawsBuffer == nullptr );
		PassParameters->OutMainAndPostNodesAndClusterBatches = GraphBuilder.CreateUAV( MainAndPostNodesAndClusterBatchesBuffer );
		
		if (CullingPass == CULLING_PASS_OCCLUSION_MAIN)
		{
			PassParameters->OutOccludedInstances = GraphBuilder.CreateUAV(CullingContext.OccludedInstances);
			PassParameters->OutOccludedInstancesArgs = GraphBuilder.CreateUAV(CullingContext.OccludedInstancesArgs);
		}

		check(CullingContext.ViewsBuffer);

		FInstanceCullVSM_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInstanceCullVSM_CS::FPrimitiveFilterDim>(CullingContext.PrimitiveFilterBuffer != nullptr);
		PermutationVector.Set<FInstanceCullVSM_CS::FDebugFlagsDim>(CullingContext.DebugFlags != 0);
		PermutationVector.Set<FInstanceCullVSM_CS::FCullingPassDim>(CullingPass);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FInstanceCullVSM_CS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "InstanceCullVSM" ),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(CullingContext.NumInstancesPreCull, 64)
		);
	}
	else if (CullingContext.NumInstancesPreCull > 0 || CullingPass == CULLING_PASS_OCCLUSION_POST)
	{
		FInstanceCull_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInstanceCull_CS::FParameters >();

		PassParameters->NumInstances						= CullingContext.NumInstancesPreCull;
		PassParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();
		PassParameters->ImposterMaxPixels					= GNaniteImposterMaxPixels;

		PassParameters->GPUSceneParameters = GPUSceneParameters;
		PassParameters->RasterParameters = RasterContext.Parameters;
		PassParameters->CullingParameters = CullingParameters;

		PassParameters->ImposterAtlas = Nanite::GStreamingManager.GetImposterDataSRV(GraphBuilder);

		PassParameters->OutQueueState = GraphBuilder.CreateUAV( CullingContext.QueueState );
		
		if (VirtualShadowMapArray)
		{
			check( CullingPass == CULLING_PASS_OCCLUSION_POST );
			PassParameters->VirtualShadowMap = VirtualTargetParameters;
			// Set the HZB page table and flags to match the now rebuilt HZB for the current frame
			PassParameters->VirtualShadowMap.HZBPageTable = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageTableRDG);
			PassParameters->VirtualShadowMap.HZBPageRectBounds = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageRectBoundsRDG);
			PassParameters->VirtualShadowMap.HZBPageFlags = GraphBuilder.CreateSRV(VirtualShadowMapArray->PageFlagsRDG);
		}

		if (CullingContext.StatsBuffer)
		{
			PassParameters->OutStatsBuffer					= GraphBuilder.CreateUAV(CullingContext.StatsBuffer);
		}

		PassParameters->OutMainAndPostNodesAndClusterBatches = GraphBuilder.CreateUAV( MainAndPostNodesAndClusterBatchesBuffer );
		if( CullingPass == CULLING_PASS_NO_OCCLUSION )
		{
			if( CullingContext.InstanceDrawsBuffer )
			{
				PassParameters->InInstanceDraws			= GraphBuilder.CreateSRV( CullingContext.InstanceDrawsBuffer );
			}
		}
		else if( CullingPass == CULLING_PASS_OCCLUSION_MAIN )
		{
			PassParameters->OutOccludedInstances		= GraphBuilder.CreateUAV( CullingContext.OccludedInstances );
			PassParameters->OutOccludedInstancesArgs	= GraphBuilder.CreateUAV( CullingContext.OccludedInstancesArgs );
		}
		else
		{
			PassParameters->InInstanceDraws				= GraphBuilder.CreateSRV( CullingContext.OccludedInstances );
			PassParameters->InOccludedInstancesArgs		= GraphBuilder.CreateSRV( CullingContext.OccludedInstancesArgs );
		}

		if (CullingContext.PrimitiveFilterBuffer)
		{
			PassParameters->InPrimitiveFilterBuffer		= GraphBuilder.CreateSRV(CullingContext.PrimitiveFilterBuffer);
		}
		
		check(CullingContext.ViewsBuffer);

		const uint32 InstanceCullingPass = CullingContext.InstanceDrawsBuffer != nullptr ? CULLING_PASS_EXPLICIT_LIST : CullingPass;
		FInstanceCull_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInstanceCull_CS::FCullingPassDim>(InstanceCullingPass);
		PermutationVector.Set<FInstanceCull_CS::FMultiViewDim>(bMultiView);
		PermutationVector.Set<FInstanceCull_CS::FPrimitiveFilterDim>(CullingContext.PrimitiveFilterBuffer != nullptr);
		PermutationVector.Set<FInstanceCull_CS::FDebugFlagsDim>(CullingContext.DebugFlags != 0);
		PermutationVector.Set<FInstanceCull_CS::FDepthOnlyDim>(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);
		PermutationVector.Set<FInstanceCull_CS::FVirtualTextureTargetDim>(VirtualShadowMapArray != nullptr);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FInstanceCull_CS>(PermutationVector);
		if( InstanceCullingPass == CULLING_PASS_OCCLUSION_POST )
		{
			PassParameters->IndirectArgs = CullingContext.OccludedInstancesArgs;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME( "InstanceCull" ),
				ComputeShader,
				PassParameters,
				PassParameters->IndirectArgs,
				0
			);
		}
		else
		{
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				InstanceCullingPass == CULLING_PASS_EXPLICIT_LIST ?	RDG_EVENT_NAME( "InstanceCull - Explicit List" ) : RDG_EVENT_NAME( "InstanceCull" ),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCountWrapped(CullingContext.NumInstancesPreCull, 64)
			);
		}
	}


	AddPass_NodeAndClusterCull(
		GraphBuilder,
		CullingParameters,
		SharedContext,
		CullingContext,
		GPUSceneParameters,
		MainAndPostNodesAndClusterBatchesBuffer,
		MainAndPostCandididateClustersBuffer,
		CullingPass,
		VirtualShadowMapArray,
		VirtualTargetParameters,
		bMultiView);
	

	{
		FCalculateSafeRasterizerArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FCalculateSafeRasterizerArgs_CS::FParameters >();

		const bool bProgrammableRaster	= (CullingContext.RenderFlags & NANITE_RENDER_FLAG_PROGRAMMABLE_RASTER) != 0;
		const bool bPrevDrawData		= (CullingContext.RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA) != 0;
		const bool bPostPass			= (CullingPass == CULLING_PASS_OCCLUSION_POST) != 0;

		if (bPrevDrawData)
		{
			PassParameters->InTotalPrevDrawClusters		= GraphBuilder.CreateSRV(CullingContext.TotalPrevDrawClustersBuffer);
		}
		else
		{
			PassParameters->InTotalPrevDrawClusters		= GraphBuilder.CreateSRV(Dummy);
		}

		if (bPostPass)
		{
			PassParameters->OffsetClustersArgsSWHW		= GraphBuilder.CreateSRV(CullingContext.MainRasterizeArgsSWHW);
			PassParameters->InRasterizerArgsSWHW		= GraphBuilder.CreateSRV(CullingContext.PostRasterizeArgsSWHW);
			PassParameters->OutSafeRasterizerArgsSWHW	= GraphBuilder.CreateUAV(CullingContext.SafePostRasterizeArgsSWHW);
		}
		else
		{
			PassParameters->InRasterizerArgsSWHW		= GraphBuilder.CreateSRV(CullingContext.MainRasterizeArgsSWHW);
			PassParameters->OutSafeRasterizerArgsSWHW	= GraphBuilder.CreateUAV(CullingContext.SafeMainRasterizeArgsSWHW);
		}

		if (bProgrammableRaster)
		{
			PassParameters->OutClusterCountSWHW			= GraphBuilder.CreateUAV(CullingContext.ClusterCountSWHW);
			PassParameters->OutClusterClassifyArgs		= GraphBuilder.CreateUAV(CullingContext.ClusterClassifyArgs);
		}
		
		PassParameters->MaxVisibleClusters				= Nanite::FGlobalResources::GetMaxVisibleClusters();
		PassParameters->RenderFlags						= CullingContext.RenderFlags;
		
		FCalculateSafeRasterizerArgs_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCalculateSafeRasterizerArgs_CS::FIsPostPass>(bPostPass);
		PermutationVector.Set<FCalculateSafeRasterizerArgs_CS::FProgrammableRaster>(bProgrammableRaster);

		auto ComputeShader = SharedContext.ShaderMap->GetShader< FCalculateSafeRasterizerArgs_CS >(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CalculateSafeRasterizerArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1)
		);
	}
}

static FBinningData AddPass_Binning(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FSharedContext& SharedContext,
	FIntVector4 PageConstants,
	uint32 RenderFlags,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef ClusterOffsetSWHW,
	FRDGBufferRef ClusterCountSWHW,
	FRDGBufferRef ClusterClassifyArgs,
	FRDGBufferRef TotalPrevDrawClustersBuffer,
	const FGPUSceneParameters& GPUSceneParameters,
	bool bMainPass,
	bool bVirtualTextureTarget,
	bool bUsePrimOrMeshShader,
	const TConstArrayView<FUintVector4> HeaderBufferData
)
{
	FBinningData BinningData = {};
	BinningData.BinCount = HeaderBufferData.Num();

	if (BinningData.BinCount > 0)
	{
		BinningData.HeaderBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.RasterizerBinHeaders"),
			sizeof(FUintVector4),
			FMath::RoundUpToPowerOfTwo(FMath::Max(BinningData.BinCount, 1u)),
			HeaderBufferData.GetData(),
			sizeof(FUintVector4) * HeaderBufferData.Num(),
			ERDGInitialDataFlags::None
		);

		BinningData.IndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(BinningData.BinCount * NANITE_RASTERIZER_ARG_COUNT), TEXT("Nanite.RasterizerBinIndirectArgs"));

		const uint32 MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
		const uint32 MaxClusterIndirections = uint32(float(MaxVisibleClusters) * FMath::Max<float>(1.0f, GNaniteRasterIndirectionMultiplier));
		check(MaxClusterIndirections > 0);
		BinningData.DataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * 2, MaxClusterIndirections), TEXT("Nanite.RasterizerBinData"));

		FRasterBinBuild_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRasterBinBuild_CS::FParameters>();

		PassParameters->GPUSceneParameters		= GPUSceneParameters;
		PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
		PassParameters->ClusterPageData			= GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->MaterialSlotTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
		PassParameters->InClusterCountSWHW		= GraphBuilder.CreateSRV(ClusterCountSWHW);
		PassParameters->InClusterOffsetSWHW		= GraphBuilder.CreateSRV(ClusterOffsetSWHW, PF_R32_UINT);
		PassParameters->IndirectArgs			= ClusterClassifyArgs;
		PassParameters->InTotalPrevDrawClusters = GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
		PassParameters->OutRasterizerBinHeaders = GraphBuilder.CreateUAV(BinningData.HeaderBuffer);

		PassParameters->PageConstants = PageConstants;
		PassParameters->RenderFlags = RenderFlags;
		PassParameters->MaxVisibleClusters = MaxVisibleClusters;
		PassParameters->RegularMaterialRasterBinCount = Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass].GetRegularBinCount();
		PassParameters->bUsePrimOrMeshShader = bUsePrimOrMeshShader;

	// Classify SW & HW Clusters
	{
		FRasterBinBuild_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRasterBinBuild_CS::FIsPostPass>(!bMainPass);
		PermutationVector.Set<FRasterBinBuild_CS::FVirtualTextureTargetDim>(bVirtualTextureTarget);
		PermutationVector.Set<FRasterBinBuild_CS::FBuildPassDim>(NANITE_RASTER_BIN_CLASSIFY);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinBuild_CS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterBinClassify"),
			ComputeShader,
			PassParameters,
			PassParameters->IndirectArgs,
			0
		);
	}

	// Reserve Bin Ranges
	{
		FRDGBufferRef RangeAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.RangeAllocatorBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RangeAllocatorBuffer), 0);

		FRasterBinReserve_CS::FParameters* ReservePassParameters = GraphBuilder.AllocParameters<FRasterBinReserve_CS::FParameters>();
		ReservePassParameters->OutRasterizerBinArgsSWHW = GraphBuilder.CreateUAV(BinningData.IndirectArgs);
		ReservePassParameters->OutRasterizerBinHeaders = GraphBuilder.CreateUAV(BinningData.HeaderBuffer);
		ReservePassParameters->OutRangeAllocator = GraphBuilder.CreateUAV(RangeAllocatorBuffer);
		ReservePassParameters->RasterBinCount = BinningData.BinCount;
		ReservePassParameters->RenderFlags = RenderFlags;

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinReserve_CS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterBinReserve"),
			ComputeShader,
			ReservePassParameters,
			FComputeShaderUtils::GetGroupCountWrapped(BinningData.BinCount, 64)
		);
	}

	PassParameters->OutRasterizerBinData = GraphBuilder.CreateUAV(BinningData.DataBuffer);
	PassParameters->OutRasterizerBinArgsSWHW = GraphBuilder.CreateUAV(BinningData.IndirectArgs);

	// Scatter SW & HW Clusters
	{
		PassParameters->OutRasterizerBinHeaders = GraphBuilder.CreateUAV(BinningData.HeaderBuffer);

		FRasterBinBuild_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRasterBinBuild_CS::FIsPostPass>(!bMainPass);
		PermutationVector.Set<FRasterBinBuild_CS::FVirtualTextureTargetDim>(bVirtualTextureTarget);
		PermutationVector.Set<FRasterBinBuild_CS::FBuildPassDim>(NANITE_RASTER_BIN_SCATTER);

		auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterBinBuild_CS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterBinScatter"),
			ComputeShader,
			PassParameters,
			PassParameters->IndirectArgs,
			0
		);
	}
}

	return BinningData;
}

FBinningData AddPass_Rasterize(
	FRDGBuilder& GraphBuilder,
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityResults& VisibilityResults,
	const TArray<FPackedView, SceneRenderingAllocator>& Views,
	const FScene& Scene,
	const FViewInfo& SceneView,
	const FSharedContext& SharedContext,
	const FRasterContext& RasterContext,
	FIntVector4 PageConstants,
	uint32 RenderFlags,
	FRDGBufferRef ViewsBuffer,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef ClusterOffsetSWHW,
	FRDGBufferRef ClusterCountSWHW,
	FRDGBufferRef ClusterClassifyArgs,
	FRDGBufferRef IndirectArgs,
	FRDGBufferRef TotalPrevDrawClustersBuffer,
	const FGPUSceneParameters& GPUSceneParameters,
	bool bMainPass,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	FVirtualTargetParameters& VirtualTargetParameters
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);

	const EShaderPlatform ShaderPlatform = Scene.GetShaderPlatform();

	FRDGBufferRef DummyBuffer8 = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 8);
	FRDGBufferRef DummyBuffer16 = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 16);

	if (bMainPass)
	{
		check(ClusterOffsetSWHW == nullptr);
		ClusterOffsetSWHW = GSystemTextures.GetDefaultBuffer(GraphBuilder, sizeof(uint32));
	}
	else
	{
		RenderFlags |= NANITE_RENDER_FLAG_ADD_CLUSTER_OFFSET;
	}

	const bool bHasPrevDrawData = (RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA);
	if (!bHasPrevDrawData)
	{
		TotalPrevDrawClustersBuffer = DummyBuffer8;
	}

	const bool bUseMeshShader = UseMeshShader(ShaderPlatform, SharedContext.Pipeline);
	const bool bUsePrimitiveShader = UsePrimitiveShader() && !bUseMeshShader;
	const bool bUseProgrammableRaster = (RenderFlags & NANITE_RENDER_FLAG_PROGRAMMABLE_RASTER) != 0;

	const uint32 RasterBinCount = bUseProgrammableRaster ? Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass].GetBinCount() : 0u;
	if (RasterBinCount > 0)
	{
		RenderFlags |= NANITE_RENDER_FLAG_HAS_RASTER_BIN;
	}

	const ERasterScheduling Scheduling = RasterContext.RasterScheduling;

	const auto CreateSkipBarrierUAV = [&](auto& InOutUAV)
	{
		if (InOutUAV)
		{
			InOutUAV = GraphBuilder.CreateUAV(InOutUAV->Desc, ERDGUnorderedAccessViewFlags::SkipBarrier);
		}
	};

	// Create a new set of UAVs with the SkipBarrier flag enabled to avoid barriers between dispatches.
	FRasterParameters RasterParameters = RasterContext.Parameters;
	CreateSkipBarrierUAV(RasterParameters.OutDepthBuffer);
	CreateSkipBarrierUAV(RasterParameters.OutDepthBufferArray);
	CreateSkipBarrierUAV(RasterParameters.OutVisBuffer64);
	CreateSkipBarrierUAV(RasterParameters.OutDbgBuffer64);
	CreateSkipBarrierUAV(RasterParameters.OutDbgBuffer32);

	const ERDGPassFlags ComputePassFlags = (Scheduling == ERasterScheduling::HardwareAndSoftwareOverlap) ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

	FIntRect ViewRect = {};
	ViewRect.Min = FIntPoint::ZeroValue;
	ViewRect.Max = RasterContext.TextureSize;

	if (VirtualShadowMapArray)
	{
		ViewRect.Min = FIntPoint::ZeroValue;
		ViewRect.Max = FIntPoint( FVirtualShadowMap::PageSize, FVirtualShadowMap::PageSize ) * FVirtualShadowMap::RasterWindowPages;
	}

	FRHIRenderPassInfo RPInfo;
	RPInfo.ResolveRect = FResolveRect(ViewRect);

	const bool bUseAutoCullingShader = UseAutoCullingShader(bUsePrimitiveShader);

	FHWRasterizeVS::FPermutationDomain PermutationVectorVS;
	FHWRasterizeMS::FPermutationDomain PermutationVectorMS;
	FHWRasterizePS::FPermutationDomain PermutationVectorPS;
	FMicropolyRasterizeCS::FPermutationDomain PermutationVectorCS;
	SetupProgrammableRasterizePermutationVectors(RasterContext.RasterMode, bUseMeshShader, bUsePrimitiveShader, bUseAutoCullingShader, RasterContext.VisualizeActive, VirtualShadowMapArray != nullptr,
		PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS);
	
	const FMaterialRenderProxy* FixedMaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	check(FixedMaterialProxy);
	const FMaterialRenderProxy* HiddenMaterialProxy = GEngine->NaniteHiddenSectionMaterial->GetRenderProxy();

	struct FRasterizerPass
	{
		TShaderRef<FHWRasterizePS> RasterPixelShader;
		TShaderRef<FHWRasterizeVS> RasterVertexShader;
		TShaderRef<FHWRasterizeMS> RasterMeshShader;

		TShaderRef<FMicropolyRasterizeCS> RasterComputeShader;

		FNaniteRasterPipeline RasterPipeline{};

		const FMaterialRenderProxy* VertexMaterialProxy		= nullptr;
		const FMaterialRenderProxy* PixelMaterialProxy		= nullptr;
		const FMaterialRenderProxy* ComputeMaterialProxy	= nullptr;

		const FMaterial* VertexMaterial		= nullptr;
		const FMaterial* PixelMaterial		= nullptr;
		const FMaterial* ComputeMaterial	= nullptr;

		bool bVertexProgrammable = false;
		bool bPixelProgrammable = false;
		bool bHidden = false;

		uint32 IndirectOffset = 0u;
		uint32 RasterizerBin = ~uint32(0u);
	};

	int32 FixedFunctionPassIndex = INDEX_NONE;

	auto& RasterizerPasses = GraphBuilder.AllocArray<FRasterizerPass>();

	TArray<FUintVector4, SceneRenderingAllocator> HeaderBufferData;
	HeaderBufferData.SetNumZeroed(RasterBinCount);

	if ((RenderFlags & NANITE_RENDER_FLAG_HAS_RASTER_BIN) != 0u)
	{
		const FNaniteRasterPipelineMap& Pipelines = RasterPipelines.GetRasterPipelineMap();
		const FNaniteRasterBinIndexTranslator BinIndexTranslator = RasterPipelines.GetBinIndexTranslator();

		RasterizerPasses.Reserve(RasterPipelines.GetBinCount());
		for (const auto& RasterBin : Pipelines)
		{
			const FNaniteRasterEntry& RasterEntry = RasterBin.Value;

			if (RasterContext.bCustomPass && !RasterPipelines.ShouldBinRenderInCustomPass(RasterEntry.BinIndex))
			{
				// Predicting that this bin will be empty if we rasterize it in the Custom Pass (i.e. Custom)
				continue;
			}

			// Test for visibility
			if (!VisibilityResults.IsRasterBinVisible(RasterEntry.BinIndex))
			{
				continue;
			}

			FRasterizerPass& RasterizerPass = RasterizerPasses.AddDefaulted_GetRef();
			RasterizerPass.RasterizerBin = uint32(BinIndexTranslator.Translate(RasterEntry.BinIndex));
			RasterizerPass.RasterPipeline = RasterEntry.RasterPipeline;

			RasterizerPass.VertexMaterialProxy	= FixedMaterialProxy;
			RasterizerPass.PixelMaterialProxy	= FixedMaterialProxy;
			RasterizerPass.ComputeMaterialProxy	= FixedMaterialProxy;

			FUintVector4& HeaderEntry = HeaderBufferData[RasterizerPass.RasterizerBin];
			uint32& MaterialBitFlags = HeaderEntry.W;

			const FMaterial& RasterMaterial 	= RasterizerPass.RasterPipeline.RasterMaterial->GetIncompleteMaterialWithFallback(Scene.GetFeatureLevel());
			MaterialBitFlags = GetMaterialBitFlags(RasterMaterial, RasterMaterial.MaterialUsesWorldPositionOffset_RenderThread(), RasterMaterial.MaterialUsesPixelDepthOffset_RenderThread(), RasterEntry.bForceDisableWPO);

			RasterizerPass.bVertexProgrammable = FNaniteMaterialShader::IsVertexProgrammable(MaterialBitFlags);
			RasterizerPass.bPixelProgrammable  = FNaniteMaterialShader::IsPixelProgrammable(MaterialBitFlags);

			if (RasterizerPass.bVertexProgrammable || RasterizerPass.bPixelProgrammable)
			{
				FMaterialShaderTypes ProgrammableShaderTypes;
				FMaterialShaderTypes NonProgrammableShaderTypes;
				GetMaterialShaderTypes(RasterizerPass.bVertexProgrammable, RasterizerPass.bPixelProgrammable, bUseMeshShader, RasterizerPass.RasterPipeline.bIsTwoSided,
					PermutationVectorVS, PermutationVectorMS, PermutationVectorPS, PermutationVectorCS, ProgrammableShaderTypes, NonProgrammableShaderTypes);

				const FMaterialRenderProxy* ProgrammableRasterProxy = RasterEntry.RasterPipeline.RasterMaterial;
				while (ProgrammableRasterProxy)
				{
					const FMaterial* Material = ProgrammableRasterProxy->GetMaterialNoFallback(Scene.GetFeatureLevel());
					if (Material)
					{
						FMaterialShaders ProgrammableShaders;
						if (Material->TryGetShaders(ProgrammableShaderTypes, nullptr, ProgrammableShaders))
						{
							if (RasterizerPass.bVertexProgrammable)
							{
								if (bUseMeshShader)
								{
									if (ProgrammableShaders.TryGetMeshShader(&RasterizerPass.RasterMeshShader))
									{
										RasterizerPass.VertexMaterialProxy = ProgrammableRasterProxy;
									}
								}
								else
								{
									if (ProgrammableShaders.TryGetVertexShader(&RasterizerPass.RasterVertexShader))
									{
										RasterizerPass.VertexMaterialProxy = ProgrammableRasterProxy;
									}
								}
							}

							if (RasterizerPass.bPixelProgrammable && ProgrammableShaders.TryGetPixelShader(&RasterizerPass.RasterPixelShader))
							{
								RasterizerPass.PixelMaterialProxy = ProgrammableRasterProxy;
							}

							if (ProgrammableShaders.TryGetComputeShader(&RasterizerPass.RasterComputeShader))
							{
								RasterizerPass.ComputeMaterialProxy = ProgrammableRasterProxy;
							}

							break;
						}
					}

					ProgrammableRasterProxy = ProgrammableRasterProxy->GetFallback(Scene.GetFeatureLevel());
				}
#if !UE_BUILD_SHIPPING
				if (ProgrammableRasterProxy != nullptr)
				{
					const FMaterial* Material = ProgrammableRasterProxy->GetMaterialNoFallback(Scene.GetFeatureLevel());
					if (Material != nullptr && (Material->MaterialUsesPixelDepthOffset_RenderThread() || Material->IsMasked()))
					{
						GGlobalResources.GetFeedbackManager()->ReportMaterialPerformanceWarning(ProgrammableRasterProxy->GetMaterialName());
					}
				}
#endif
			}

			// Note: The indirect args offset is in bytes
			RasterizerPass.IndirectOffset = (RasterizerPass.RasterizerBin * NANITE_RASTERIZER_ARG_COUNT) * 4u;

			if (FixedFunctionPassIndex == INDEX_NONE &&
				RasterizerPass.VertexMaterialProxy  == FixedMaterialProxy &&
				RasterizerPass.PixelMaterialProxy   == FixedMaterialProxy &&
				RasterizerPass.ComputeMaterialProxy == FixedMaterialProxy)
			{
				FixedFunctionPassIndex = RasterizerPasses.Num() - 1;
			}

			if (RasterizerPass.VertexMaterialProxy	== HiddenMaterialProxy &&
				RasterizerPass.PixelMaterialProxy	== HiddenMaterialProxy &&
				RasterizerPass.ComputeMaterialProxy	== HiddenMaterialProxy)
			{
				RasterizerPass.bHidden = true;
			}
		}
	}
	else
	{
		FRasterizerPass& RasterizerPass		= RasterizerPasses.AddDefaulted_GetRef();
		RasterizerPass.VertexMaterialProxy	= FixedMaterialProxy;
		RasterizerPass.PixelMaterialProxy	= FixedMaterialProxy;
		RasterizerPass.ComputeMaterialProxy	= FixedMaterialProxy;
		RasterizerPass.IndirectOffset		= 0u;
		RasterizerPass.RasterizerBin		= 0u;

		FixedFunctionPassIndex = 0;
	}

	// Rasterizer Binning
	FBinningData BinningData = AddPass_Binning(
		GraphBuilder,
		Scene,
		SharedContext,
		PageConstants,
		RenderFlags,
		VisibleClustersSWHW,
		ClusterOffsetSWHW,
		ClusterCountSWHW,
		ClusterClassifyArgs,
		TotalPrevDrawClustersBuffer,
		GPUSceneParameters,
		bMainPass,
		VirtualShadowMapArray != nullptr,
		bUsePrimitiveShader || bUseMeshShader,
		HeaderBufferData
	);

	if (BinningData.DataBuffer == nullptr)
	{
		BinningData.DataBuffer = DummyBuffer8;
	}

	if (BinningData.HeaderBuffer == nullptr)
	{
		BinningData.HeaderBuffer = DummyBuffer16;
	}

	FRDGBufferRef BinIndirectArgs = (RenderFlags & NANITE_RENDER_FLAG_HAS_RASTER_BIN) != 0u ? BinningData.IndirectArgs : IndirectArgs;

	for (FRasterizerPass& RasterizerPass : RasterizerPasses)
	{
		if (bUseMeshShader)
		{
			if (RasterizerPass.RasterMeshShader.IsNull())
			{
				const FMaterialShaderMap* VertexShaderMap = RasterizerPass.VertexMaterialProxy->GetMaterialWithFallback(Scene.GetFeatureLevel(), RasterizerPass.VertexMaterialProxy).GetRenderingThreadShaderMap();
				check(VertexShaderMap);

				PermutationVectorMS.Set<FHWRasterizeMS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
				PermutationVectorMS.Set<FHWRasterizeMS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
				RasterizerPass.RasterMeshShader = VertexShaderMap->GetShader<FHWRasterizeMS>(PermutationVectorMS);
				check(!RasterizerPass.RasterMeshShader.IsNull());
			}
		}
		else
		{
			if (RasterizerPass.RasterVertexShader.IsNull())
			{
				const FMaterialShaderMap* VertexShaderMap = RasterizerPass.VertexMaterialProxy->GetMaterialWithFallback(Scene.GetFeatureLevel(), RasterizerPass.VertexMaterialProxy).GetRenderingThreadShaderMap();
				check(VertexShaderMap);

				PermutationVectorVS.Set<FHWRasterizeVS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
				PermutationVectorVS.Set<FHWRasterizeVS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
				RasterizerPass.RasterVertexShader = VertexShaderMap->GetShader<FHWRasterizeVS>(PermutationVectorVS);
				check(!RasterizerPass.RasterVertexShader.IsNull());
			}
		}

		if (RasterizerPass.RasterPixelShader.IsNull())
		{
			const FMaterialShaderMap* PixelShaderMap = RasterizerPass.PixelMaterialProxy->GetMaterialWithFallback(Scene.GetFeatureLevel(), RasterizerPass.PixelMaterialProxy).GetRenderingThreadShaderMap();
			check(PixelShaderMap);

			PermutationVectorPS.Set<FHWRasterizePS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
			PermutationVectorPS.Set<FHWRasterizePS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
			RasterizerPass.RasterPixelShader = PixelShaderMap->GetShader<FHWRasterizePS>(PermutationVectorPS);
			check(!RasterizerPass.RasterPixelShader.IsNull());
		}

		if (RasterizerPass.RasterComputeShader.IsNull())
		{
			const FMaterialShaderMap* ComputeShaderMap = RasterizerPass.ComputeMaterialProxy->GetMaterialWithFallback(Scene.GetFeatureLevel(), RasterizerPass.ComputeMaterialProxy).GetRenderingThreadShaderMap();
			check(ComputeShaderMap);
			
			PermutationVectorCS.Set<FMicropolyRasterizeCS::FTwoSidedDim>(RasterizerPass.RasterPipeline.bIsTwoSided);
			PermutationVectorCS.Set<FMicropolyRasterizeCS::FVertexProgrammableDim>(RasterizerPass.bVertexProgrammable);
			PermutationVectorCS.Set<FMicropolyRasterizeCS::FPixelProgrammableDim>(RasterizerPass.bPixelProgrammable);
			RasterizerPass.RasterComputeShader = ComputeShaderMap->GetShader<FMicropolyRasterizeCS>(PermutationVectorCS);
			check(!RasterizerPass.RasterComputeShader.IsNull());
		}

		RasterizerPass.VertexMaterial = RasterizerPass.VertexMaterialProxy->GetMaterialNoFallback(Scene.GetFeatureLevel());
		check(RasterizerPass.VertexMaterial);

		RasterizerPass.PixelMaterial = RasterizerPass.PixelMaterialProxy->GetMaterialNoFallback(Scene.GetFeatureLevel());
		check(RasterizerPass.PixelMaterial);

		RasterizerPass.ComputeMaterial = RasterizerPass.ComputeMaterialProxy->GetMaterialNoFallback(Scene.GetFeatureLevel());
		check(RasterizerPass.ComputeMaterial);
	}

	auto* RasterPassParameters = GraphBuilder.AllocParameters<FRasterizePassParameters>();
	RasterPassParameters->RenderFlags = RenderFlags;

	RasterPassParameters->View = SceneView.ViewUniformBuffer;
	RasterPassParameters->ClusterPageData = GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	RasterPassParameters->GPUSceneParameters = GPUSceneParameters;
	RasterPassParameters->RasterParameters = RasterParameters;
	RasterPassParameters->VisualizeModeBitMask = RasterContext.VisualizeModeBitMask;
	RasterPassParameters->PageConstants = PageConstants;
	RasterPassParameters->HardwareViewportSize = FVector2f(ViewRect.Width(), ViewRect.Height());
	RasterPassParameters->MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
	RasterPassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);
	RasterPassParameters->IndirectArgs = BinIndirectArgs;
	RasterPassParameters->InViews = ViewsBuffer != nullptr ? GraphBuilder.CreateSRV(ViewsBuffer) : nullptr;
	RasterPassParameters->InClusterOffsetSWHW = GraphBuilder.CreateSRV(ClusterOffsetSWHW, PF_R32_UINT);
	RasterPassParameters->InTotalPrevDrawClusters = GraphBuilder.CreateSRV(TotalPrevDrawClustersBuffer);
	RasterPassParameters->MaterialSlotTable = Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
	RasterPassParameters->RasterizerBinData = GraphBuilder.CreateSRV(BinningData.DataBuffer);
	RasterPassParameters->RasterizerBinHeaders = GraphBuilder.CreateSRV(BinningData.HeaderBuffer);

	RasterPassParameters->TessellationTable_Offsets	= GTessellationTable.Offsets.SRV;
	RasterPassParameters->TessellationTable_Verts	= GTessellationTable.Verts.SRV;
	RasterPassParameters->TessellationTable_Indexes	= GTessellationTable.Indexes.SRV;

	if (VirtualShadowMapArray != nullptr)
	{
		RasterPassParameters->VirtualShadowMap = VirtualTargetParameters;
	}

	uint32 PassWorkload = RasterizerPasses.Num();
	ERDGPassFlags ParallelTranslateFlag = ERDGPassFlags::None;

	if (GNaniteParallelRasterTranslateExperimental)
	{
		// Force the pass onto its own async command list.
		PassWorkload = 1000;
		ParallelTranslateFlag = ERDGPassFlags::ParallelTranslate;
	}

	// Disabled for now because this will call PipelineStateCache::IsPrecaching which requires the PSO to have
	// the minimal state hash computed. Computing this for each PSO each frame is not cheap and ideally the minimal
	// PSO state can be cached like regular MDCs before activating this (UE-171561)
	const bool bAllowPrecacheSkip = false;// GSkipDrawOnPSOPrecaching != 0;

	FRDGPass* HWPass = GraphBuilder.AddPass(
		RDG_EVENT_NAME("HW Rasterize"),
		RasterPassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ParallelTranslateFlag,
		[RasterPassParameters, &RasterizerPasses, ViewRect, &SceneView, FixedMaterialProxy, bAllowPrecacheSkip, FixedFunctionPassIndex, RPInfo, bMainPass, bUsePrimitiveShader, bUseMeshShader](FRHICommandList& RHICmdList)
	{
		RHICmdList.BeginRenderPass(RPInfo, TEXT("HW Rasterize"));
		RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, FMath::Min(ViewRect.Max.X, 32767), FMath::Min(ViewRect.Max.Y, 32767), 1.0f);
		RHICmdList.SetStreamSource(0, nullptr, 0);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI(); // TODO: PROG_RASTER - Support depth clip as a rasterizer bin and remove shader permutations
		GraphicsPSOInit.PrimitiveType = bUsePrimitiveShader ? PT_PointList : PT_TriangleList;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = bUseMeshShader ? nullptr : GEmptyVertexDeclaration.VertexDeclarationRHI;

		FHWRasterizePS::FParameters Parameters = *RasterPassParameters;

		Parameters.IndirectArgs->MarkResourceAsUsed();

		for (const FRasterizerPass& RasterizerPass : RasterizerPasses)
		{
			if (RasterizerPass.bHidden)
			{
				continue;
			}

		#if WANTS_DRAW_MESH_EVENTS
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, HWRaster, GNaniteShowDrawEvents != 0, TEXT("%s"), GetRasterMaterialName(RasterizerPass.RasterPipeline.RasterMaterial, FixedMaterialProxy));
		#endif

			Parameters.ActiveRasterizerBin = RasterizerPass.RasterizerBin;

			// NOTE: We do *not* use any CullMode overrides here because HWRasterize[VS/MS] already
			// changes the index order in cases where the culling should be flipped.
			// The exception is if CM_None is specified for two sided materials, or if the entire raster pass has CM_None specified.
			const bool bCullModeNone = RasterizerPass.RasterPipeline.bIsTwoSided;
			GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, bCullModeNone ? CM_None : CM_CW);

			auto BindShadersToPSOInit = [bUseMeshShader, &GraphicsPSOInit](const FRasterizerPass& PassToBind)
			{
				if (bUseMeshShader)
				{
					GraphicsPSOInit.BoundShaderState.SetMeshShader(PassToBind.RasterMeshShader.GetMeshShader());
				}
				else
				{
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = PassToBind.RasterVertexShader.GetVertexShader();
				}

				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PassToBind.RasterPixelShader.GetPixelShader();
			};

			auto BindShaderParameters = [bUseMeshShader, &RHICmdList, &SceneView, &Parameters](const FRasterizerPass& PassToBind)
			{
				if (bUseMeshShader)
				{
					PassToBind.RasterMeshShader->SetParameters(RHICmdList, SceneView, PassToBind.VertexMaterialProxy, *PassToBind.VertexMaterial);
				}
				else
				{
					PassToBind.RasterVertexShader->SetParameters(RHICmdList, SceneView, PassToBind.VertexMaterialProxy, *PassToBind.VertexMaterial);
				}

				PassToBind.RasterPixelShader->SetParameters(RHICmdList, SceneView, PassToBind.PixelMaterialProxy, *PassToBind.PixelMaterial);

				if (bUseMeshShader)
				{
					SetShaderParameters(RHICmdList, PassToBind.RasterMeshShader, PassToBind.RasterMeshShader.GetMeshShader(), Parameters);
				}
				else
				{
					SetShaderParameters(RHICmdList, PassToBind.RasterVertexShader, PassToBind.RasterVertexShader.GetVertexShader(), Parameters);
				}

				SetShaderParameters(RHICmdList, PassToBind.RasterPixelShader, PassToBind.RasterPixelShader.GetPixelShader(), Parameters);
			};

			BindShadersToPSOInit(RasterizerPass);

			if (bAllowPrecacheSkip && FixedFunctionPassIndex != INDEX_NONE && (GNaniteTestPrecacheDrawSkipping != 0 || PipelineStateCache::IsPrecaching(GraphicsPSOInit)))
			{
				// Programmable raster PSO has not been precached yet, fallback to fixed function in the meantime to avoid hitching.
				const FRasterizerPass& FixedFunctionPass = RasterizerPasses[FixedFunctionPassIndex];

				BindShadersToPSOInit(FixedFunctionPass);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				BindShaderParameters(FixedFunctionPass);
			}
			else
			{
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				BindShaderParameters(RasterizerPass);
			}

			if (bUseMeshShader)
			{
				RHICmdList.DispatchIndirectMeshShader(Parameters.IndirectArgs->GetIndirectRHICallBuffer(), RasterizerPass.IndirectOffset + 16);
			}
			else
			{
				RHICmdList.DrawPrimitiveIndirect(Parameters.IndirectArgs->GetIndirectRHICallBuffer(), RasterizerPass.IndirectOffset + 16);
			}
		}

		RHICmdList.EndRenderPass();
	});

	GraphBuilder.SetPassWorkload(HWPass, PassWorkload);

	if (Scheduling != ERasterScheduling::HardwareOnly)
	{
		FRDGPass* SWPass = GraphBuilder.AddPass(
			RDG_EVENT_NAME("SW Rasterize"),
			RasterPassParameters,
			ComputePassFlags | ParallelTranslateFlag,
			[RasterPassParameters, &RasterizerPasses, &SceneView, FixedMaterialProxy, bAllowPrecacheSkip, FixedFunctionPassIndex](FRHIComputeCommandList& RHICmdList)
		{
			FRasterizePassParameters Parameters = *RasterPassParameters;
			Parameters.IndirectArgs->MarkResourceAsUsed();

			for (const FRasterizerPass& RasterizerPass : RasterizerPasses)
			{
				if (RasterizerPass.bHidden)
				{
					continue;
				}

			#if WANTS_DRAW_MESH_EVENTS
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SWRaster, GNaniteShowDrawEvents != 0, TEXT("%s"), GetRasterMaterialName(RasterizerPass.RasterPipeline.RasterMaterial, FixedMaterialProxy));
			#endif

				Parameters.ActiveRasterizerBin = RasterizerPass.RasterizerBin;

				FRHIBuffer* IndirectArgsBuffer = Parameters.IndirectArgs->GetIndirectRHICallBuffer();
				FRHIComputeShader* ShaderRHI = RasterizerPass.RasterComputeShader.GetComputeShader();

				// TODO: Implement support for testing precache and skipping if needed

				FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer->GetSize(), RasterizerPass.IndirectOffset);
				SetComputePipelineState(RHICmdList, ShaderRHI);
				SetShaderParameters(RHICmdList, RasterizerPass.RasterComputeShader, ShaderRHI, Parameters);
				
				RasterizerPass.RasterComputeShader->SetParameters(
					RHICmdList,
					RasterizerPass.RasterComputeShader.GetComputeShader(),
					SceneView,
					RasterizerPass.ComputeMaterialProxy,
					*RasterizerPass.ComputeMaterial
				);
				
				RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer, RasterizerPass.IndirectOffset);
				UnsetShaderUAVs(RHICmdList, RasterizerPass.RasterComputeShader, ShaderRHI);
			}
		});

		GraphBuilder.SetPassWorkload(SWPass, PassWorkload);
	}

	return BinningData;
}

void AddClearVisBufferPass(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const EPixelFormat PixelFormat64,
	const FRasterContext& RasterContext,
	const FIntRect& TextureRect,
	bool bClearTarget,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FRDGTextureRef ExternalDepthBuffer)
{
	if (!bClearTarget)
	{
		return;
	}

	const bool bUseFastClear = GNaniteFastVisBufferClear != 0 && (RectMinMaxBufferSRV == nullptr && NumRects == 0 && ExternalDepthBuffer == nullptr);
	if (bUseFastClear)
	{
		// TODO: Don't currently support offset views.
		checkf(TextureRect.Min.X == 0 && TextureRect.Min.Y == 0, TEXT("Viewport offset support is not implemented."));

		const bool bTiled = (GNaniteFastVisBufferClear == 2);

		FRasterClearCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRasterClearCS::FParameters>();
		PassParameters->ClearRect = FUint32Vector4((uint32)TextureRect.Min.X, (uint32)TextureRect.Min.Y, (uint32)TextureRect.Max.X, (uint32)TextureRect.Max.Y);
		PassParameters->RasterParameters = RasterContext.Parameters;

		FRasterClearCS::FPermutationDomain PermutationVectorCS;
		PermutationVectorCS.Set<FRasterClearCS::FClearDepthDim>(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);
		PermutationVectorCS.Set<FRasterClearCS::FClearDebugDim>(RasterContext.VisualizeActive);
		PermutationVectorCS.Set<FRasterClearCS::FClearTiledDim>(bTiled);
		auto ComputeShader = SharedContext.ShaderMap->GetShader<FRasterClearCS>(PermutationVectorCS);

		const FIntPoint ClearSize(TextureRect.Width(), TextureRect.Height());
		const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(ClearSize, bTiled ? 32 : 8);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RasterClear"),
			ComputeShader,
			PassParameters,
			DispatchDim
		);
	}
	else
	{
		const uint32 ClearValue[4] = { 0, 0, 0, 0 };

		TArray<FRDGTextureUAVRef, TInlineAllocator<3>> BufferClearList;
		if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
		{
			BufferClearList.Add(RasterContext.Parameters.OutDepthBuffer);
		}
		else
		{
			BufferClearList.Add(RasterContext.Parameters.OutVisBuffer64);

			if (RasterContext.VisualizeActive)
			{
				BufferClearList.Add(RasterContext.Parameters.OutDbgBuffer64);
				BufferClearList.Add(RasterContext.Parameters.OutDbgBuffer32);
			}
		}

		for (FRDGTextureUAVRef UAVRef : BufferClearList)
		{
			AddClearUAVPass(GraphBuilder, SharedContext.FeatureLevel, UAVRef, ClearValue, RectMinMaxBufferSRV, NumRects);
		}
	}
}

FRasterContext InitRasterContext(
	FRDGBuilder& GraphBuilder,
	const FSharedContext& SharedContext,
	const FViewFamilyInfo& ViewFamily,
	FIntPoint TextureSize,
	FIntRect TextureRect,
	bool bVisualize,
	EOutputBufferMode RasterMode,
	bool bClearTarget,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FRDGTextureRef ExternalDepthBuffer,
	bool bCustomPass
)
{
	// If an external depth buffer is provided, it must match the context size
	check( ExternalDepthBuffer == nullptr || ExternalDepthBuffer->Desc.Extent == TextureSize );
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::InitContext");

	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();

	FRasterContext RasterContext{};

	RasterContext.bCustomPass = bCustomPass;
	RasterContext.VisualizeActive = VisualizationData.IsActive() && bVisualize;
	if (RasterContext.VisualizeActive)
	{
		if (VisualizationData.GetActiveModeID() == 0) // Overview
		{
			RasterContext.VisualizeModeBitMask = VisualizationData.GetOverviewModeBitMask();
		}
		else
		{
			RasterContext.VisualizeModeBitMask |= VisualizationData.GetActiveModeID();
		}
	}

	RasterContext.TextureSize = TextureSize;

	// Set rasterizer scheduling based on config and platform capabilities.
	if (GNaniteComputeRasterization != 0)
	{
		const bool bUseAsyncCompute = GSupportsEfficientAsyncCompute && (GNaniteAsyncRasterization != 0) && EnumHasAnyFlags(GRHIMultiPipelineMergeableAccessMask, ERHIAccess::UAVMask);
		RasterContext.RasterScheduling = bUseAsyncCompute ? ERasterScheduling::HardwareAndSoftwareOverlap : ERasterScheduling::HardwareThenSoftware;
	}
	else
	{
		// Force hardware-only rasterization.
		RasterContext.RasterScheduling = ERasterScheduling::HardwareOnly;
	}

	RasterContext.RasterMode = RasterMode;

	const EPixelFormat PixelFormat64 = GPixelFormats[PF_R64_UINT].Supported ? PF_R64_UINT : PF_R32G32_UINT;

	RasterContext.DepthBuffer	= ExternalDepthBuffer ? ExternalDepthBuffer :
								  GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Nanite.DepthBuffer32") );
	RasterContext.VisBuffer64	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PixelFormat64, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | ETextureCreateFlags::Atomic64Compatible), TEXT("Nanite.VisBuffer64") );
	RasterContext.DbgBuffer64	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PixelFormat64, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | ETextureCreateFlags::Atomic64Compatible), TEXT("Nanite.DbgBuffer64") );
	RasterContext.DbgBuffer32	= GraphBuilder.CreateTexture( FRDGTextureDesc::Create2D(RasterContext.TextureSize, PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Nanite.DbgBuffer32") );

	if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
	{
		if (!UseAsyncComputeForShadowMaps(ViewFamily) && RasterContext.RasterScheduling == ERasterScheduling::HardwareAndSoftwareOverlap)
		{
			RasterContext.RasterScheduling = ERasterScheduling::HardwareThenSoftware;
		}

		if (RasterContext.DepthBuffer->Desc.Dimension == ETextureDimension::Texture2DArray)
		{
			RasterContext.Parameters.OutDepthBufferArray = GraphBuilder.CreateUAV(RasterContext.DepthBuffer);
			check(!bClearTarget); // Clearing is not required; this path is only used with VSMs.
		}
		else
		{
			RasterContext.Parameters.OutDepthBuffer = GraphBuilder.CreateUAV(RasterContext.DepthBuffer);
		}
	}
	else
	{
		RasterContext.Parameters.OutVisBuffer64 = GraphBuilder.CreateUAV(RasterContext.VisBuffer64);
		
		if (RasterContext.VisualizeActive)
		{
			RasterContext.Parameters.OutDbgBuffer64 = GraphBuilder.CreateUAV(RasterContext.DbgBuffer64);
			RasterContext.Parameters.OutDbgBuffer32 = GraphBuilder.CreateUAV(RasterContext.DbgBuffer32);
		}
	}

	AddClearVisBufferPass(
		GraphBuilder,
		SharedContext,
		PixelFormat64,
		RasterContext,
		TextureRect,
		bClearTarget,
		RectMinMaxBufferSRV,
		NumRects,
		ExternalDepthBuffer
	);

	return RasterContext;
}

static void AllocateNodesAndBatchesBuffers(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGBufferRef* MainAndPostNodesAndClusterBatchesBufferRef)
{
	const uint32 MaxNodes				=	Nanite::FGlobalResources::GetMaxNodes();
	const uint32 MaxClusterBatches		=	Nanite::FGlobalResources::GetMaxClusterBatches();
	check(MainAndPostNodesAndClusterBatchesBufferRef);

	// Initialize node and cluster batch arrays.
	// They only have to be initialized once as the culling code reverts nodes/batches to their cleared state after they have been consumed.
	{
		FNodesAndClusterBatchesBuffer& MainAndPostNodesAndClusterBatchesBuffer = Nanite::GGlobalResources.GetMainAndPostNodesAndClusterBatchesBuffer();
		if (MainAndPostNodesAndClusterBatchesBuffer.Buffer.IsValid() && MaxNodes == MainAndPostNodesAndClusterBatchesBuffer.NumNodes && MaxClusterBatches == MainAndPostNodesAndClusterBatchesBuffer.NumClusterBatches)
		{
			*MainAndPostNodesAndClusterBatchesBufferRef = GraphBuilder.RegisterExternalBuffer(MainAndPostNodesAndClusterBatchesBuffer.Buffer, TEXT("Nanite.MainAndPostNodesAndClusterBatchesBuffer"));
		}
		else
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(4, MaxClusterBatches * 2 + MaxNodes * (2 + 3));
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			*MainAndPostNodesAndClusterBatchesBufferRef = GraphBuilder.CreateBuffer(Desc, TEXT("Nanite.MainAndPostNodesAndClusterBatchesBuffer"));
			AddPassInitNodesAndClusterBatchesUAV(GraphBuilder, ShaderMap, GraphBuilder.CreateUAV(*MainAndPostNodesAndClusterBatchesBufferRef));
			MainAndPostNodesAndClusterBatchesBuffer.Buffer = GraphBuilder.ConvertToExternalBuffer(*MainAndPostNodesAndClusterBatchesBufferRef);
			MainAndPostNodesAndClusterBatchesBuffer.NumNodes = MaxNodes;
			MainAndPostNodesAndClusterBatchesBuffer.NumClusterBatches = MaxClusterBatches;
		}
	}
}

// Render a large number of views by splitting them into multiple passes. This is only supported for depth-only rendering.
// Visibility buffer rendering requires that view references are uniquely decodable.
static void CullRasterizeMultiPass(
	FRDGBuilder& GraphBuilder,
	FNaniteRasterPipelines& RasterPipelines,
	const FNaniteVisibilityResults& VisibilityResults,
	const FScene& Scene,
	const FViewInfo& SceneView,
	const TArray<FPackedView, SceneRenderingAllocator>& Views,
	uint32 NumPrimaryViews,
	const FSharedContext& SharedContext,
	FCullingContext& CullingContext,
	const FRasterContext& RasterContext,
	const TArray<FInstanceDraw, SceneRenderingAllocator>* OptionalInstanceDraws,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	bool bExtractStats
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::CullRasterizeMultiPass");

	check(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);

	uint32 NextPrimaryViewIndex = 0;
	while (NextPrimaryViewIndex < NumPrimaryViews)
	{
		// Fit as many views as possible into the next range
		int32 RangeStartPrimaryView = NextPrimaryViewIndex;
		int32 RangeNumViews = 0;
		int32 RangeMaxMip = 0;
		while (NextPrimaryViewIndex < NumPrimaryViews)
		{
			const Nanite::FPackedView& PrimaryView = Views[NextPrimaryViewIndex];
			const int32 NumMips = PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z;

			// Can we include the next primary view and its mips?
			int32 NextRangeNumViews = FMath::Max(RangeMaxMip, NumMips) * (NextPrimaryViewIndex - RangeStartPrimaryView + 1);
			if (NextRangeNumViews > NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS)
				break;

			RangeNumViews = NextRangeNumViews;
			NextPrimaryViewIndex++;
			RangeMaxMip = FMath::Max(RangeMaxMip, NumMips);
		}

		// Construct new view range
		int32 RangeNumPrimaryViews = NextPrimaryViewIndex - RangeStartPrimaryView;
		TArray<FPackedView, SceneRenderingAllocator> RangeViews;
		RangeViews.SetNum(RangeNumViews);

		for (int32 ViewIndex = 0; ViewIndex < RangeNumPrimaryViews; ++ViewIndex)
		{
			const Nanite::FPackedView& PrimaryView = Views[RangeStartPrimaryView + ViewIndex];
			const int32 NumMips = PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z;

			for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
			{
				RangeViews[MipIndex * RangeNumPrimaryViews + ViewIndex] = Views[MipIndex * NumPrimaryViews + (RangeStartPrimaryView + ViewIndex)];
			}
		}

		CullRasterize(
			GraphBuilder,
			RasterPipelines,
			VisibilityResults,
			Scene,
			SceneView,
			RangeViews,
			RangeNumPrimaryViews,
			SharedContext,
			CullingContext,
			RasterContext,
			OptionalInstanceDraws,
			VirtualShadowMapArray,
			bExtractStats
		);
	}
}

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
	const TArray<FInstanceDraw, SceneRenderingAllocator>* OptionalInstanceDraws,
	// VirtualShadowMapArray is the supplier of virtual to physical translation, probably could abstract this a bit better,
	FVirtualShadowMapArray* VirtualShadowMapArray,
	bool bExtractStats
)
{
	LLM_SCOPE_BYTAG(Nanite);
	
	// Split rasterization into multiple passes if there are too many views. Only possible for depth-only rendering.
	if (Views.Num() > NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS)
	{
		check(RasterContext.RasterMode == EOutputBufferMode::DepthOnly);
		CullRasterizeMultiPass(
			GraphBuilder,
			RasterPipelines,
			VisibilityResults,
			Scene,
			SceneView,
			Views,
			NumPrimaryViews,
			SharedContext,
			CullingContext,
			RasterContext,
			OptionalInstanceDraws,
			VirtualShadowMapArray,
			bExtractStats
		);
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::CullRasterize");

	check(!Nanite::GStreamingManager.IsAsyncUpdateInProgress());

	// Calling CullRasterize more than once on a CullingContext is illegal unless bSupportsMultiplePasses is enabled.
	check(CullingContext.DrawPassIndex == 0 || CullingContext.Configuration.bSupportsMultiplePasses);

	//check(Views.Num() == 1 || !CullingContext.PrevHZB);	// HZB not supported with multi-view, yet
	ensure(Views.Num() > 0 && Views.Num() <= NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS);

	{
		const uint32 ViewsBufferElements = FMath::RoundUpToPowerOfTwo(Views.Num());
		CullingContext.ViewsBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Nanite.Views"), Views.GetTypeSize(), ViewsBufferElements, Views.GetData(), Views.Num() * Views.GetTypeSize());
	}

	if (OptionalInstanceDraws)
	{
		const uint32 InstanceDrawsBufferElements = FMath::RoundUpToPowerOfTwo(OptionalInstanceDraws->Num());
		CullingContext.InstanceDrawsBuffer = CreateStructuredBuffer
		(
			GraphBuilder,
			TEXT("Nanite.InstanceDraws"),
			OptionalInstanceDraws->GetTypeSize(),
			InstanceDrawsBufferElements,
			OptionalInstanceDraws->GetData(),
			OptionalInstanceDraws->Num() * OptionalInstanceDraws->GetTypeSize()
		);
		CullingContext.NumInstancesPreCull = OptionalInstanceDraws->Num();
	}
	else
	{
		CullingContext.InstanceDrawsBuffer = nullptr;
		CullingContext.NumInstancesPreCull = Scene.GPUScene.InstanceSceneDataAllocator.GetMaxSize();
	}

	if (CullingContext.DebugFlags != 0)
	{
		FNaniteStats Stats;
		FMemory::Memzero(Stats);
		Stats.NumMainInstancesPreCull	= CullingContext.NumInstancesPreCull;

		CullingContext.StatsBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Nanite.StatsBuffer"), sizeof(FNaniteStats), 1, &Stats, sizeof(FNaniteStats));
	}
	else
	{
		CullingContext.StatsBuffer = nullptr;
	}

	FCullingParameters CullingParameters;
	{
		// Never use the disocclusion hack with virtual shadows as it interacts very poorly with caching that first frame
		const bool bDisocclusionHack = GNaniteDisocclusionHack && !VirtualShadowMapArray;

		CullingParameters.InViews						= GraphBuilder.CreateSRV(CullingContext.ViewsBuffer);
		CullingParameters.NumViews						= Views.Num();
		CullingParameters.NumPrimaryViews				= NumPrimaryViews;
		CullingParameters.DisocclusionLodScaleFactor	= bDisocclusionHack ? 0.01f : 1.0f;	// TODO: Get rid of this hack
		CullingParameters.HZBTexture					= RegisterExternalTextureWithFallback(GraphBuilder, CullingContext.PrevHZB, GSystemTextures.BlackDummy);
		CullingParameters.HZBSize						= CullingContext.PrevHZB ? CullingContext.PrevHZB->GetDesc().Extent : FVector2f(0.0f);
		CullingParameters.HZBSampler					= TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI();
		CullingParameters.PageConstants					= CullingContext.PageConstants;
		CullingParameters.MaxCandidateClusters			= Nanite::FGlobalResources::GetMaxCandidateClusters();
		CullingParameters.MaxVisibleClusters			= Nanite::FGlobalResources::GetMaxVisibleClusters();
		CullingParameters.RenderFlags					= CullingContext.RenderFlags;
		CullingParameters.DebugFlags					= CullingContext.DebugFlags;
		CullingParameters.CompactedViewInfo				= nullptr;
		CullingParameters.CompactedViewsAllocation		= nullptr;
	}

	FVirtualTargetParameters VirtualTargetParameters;
	if (VirtualShadowMapArray)
	{
		VirtualTargetParameters.VirtualShadowMap = VirtualShadowMapArray->GetUniformBuffer();
		
		// HZB (if provided) comes from the previous frame, so we need last frame's page table
		FRDGBufferRef HZBPageTableRDG = VirtualShadowMapArray->PageTableRDG;	// Dummy data, but matches the expected format
		FRDGBufferRef HZBPageRectBoundsRDG = VirtualShadowMapArray->PageRectBoundsRDG;	// Dummy data, but matches the expected format
		FRDGBufferRef HZBPageFlagsRDG = VirtualShadowMapArray->PageFlagsRDG;	// Dummy data, but matches the expected format

		if (CullingContext.PrevHZB)
		{
			check( VirtualShadowMapArray->CacheManager );
			HZBPageTableRDG = GraphBuilder.RegisterExternalBuffer( VirtualShadowMapArray->CacheManager->PrevBuffers.PageTable, TEXT( "Shadow.Virtual.HZBPageTable" ) );
			HZBPageRectBoundsRDG = GraphBuilder.RegisterExternalBuffer( VirtualShadowMapArray->CacheManager->PrevBuffers.PageRectBounds, TEXT("Shadow.Virtual.HZBPageRectBounds"));
			HZBPageFlagsRDG = GraphBuilder.RegisterExternalBuffer(VirtualShadowMapArray->CacheManager->PrevBuffers.PageFlags, TEXT( "Shadow.Virtual.HZBPageFlags" ) );
		}
		VirtualTargetParameters.HZBPageTable = GraphBuilder.CreateSRV( HZBPageTableRDG );
		VirtualTargetParameters.HZBPageRectBounds = GraphBuilder.CreateSRV( HZBPageRectBoundsRDG );
		VirtualTargetParameters.HZBPageFlags = GraphBuilder.CreateSRV( HZBPageFlagsRDG );
		VirtualTargetParameters.OutDirtyPageFlags = GraphBuilder.CreateUAV(VirtualShadowMapArray->DirtyPageFlagsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		VirtualTargetParameters.OutStaticInvalidatingPrimitives = GraphBuilder.CreateUAV(VirtualShadowMapArray->StaticInvalidatingPrimitivesRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	FGPUSceneParameters GPUSceneParameters;

	{
		const FGPUSceneResourceParameters ShaderParameters = Scene.GPUScene.GetShaderParameters();
		GPUSceneParameters.GPUSceneInstanceSceneData = ShaderParameters.GPUSceneInstanceSceneData;
		GPUSceneParameters.GPUSceneInstancePayloadData = ShaderParameters.GPUSceneInstancePayloadData;
		GPUSceneParameters.GPUScenePrimitiveSceneData = ShaderParameters.GPUScenePrimitiveSceneData;
		GPUSceneParameters.GPUSceneFrameNumber = ShaderParameters.GPUSceneFrameNumber;
	}
	
	if (VirtualShadowMapArray != nullptr)
	{
		// Compact the views to remove needless (empty) mip views - need to do on GPU as that is where we know what mips have pages.
		const uint32 ViewsBufferElements = FMath::RoundUpToPowerOfTwo(Views.Num());
		FRDGBufferRef CompactedViews = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FPackedView), ViewsBufferElements), TEXT("Shadow.Virtual.CompactedViews"));
		FRDGBufferRef CompactedViewInfo = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FCompactedViewInfo), Views.Num()), TEXT("Shadow.Virtual.CompactedViewInfo"));
		
		// Just a pair of atomic counters, zeroed by a clear UAV pass.
		FRDGBufferRef CompactedViewsAllocation = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("Shadow.Virtual.CompactedViewsAllocation"));
		FRDGBufferUAVRef CompactedViewsAllocationUAV = GraphBuilder.CreateUAV(CompactedViewsAllocation);
		AddClearUAVPass(GraphBuilder, CompactedViewsAllocationUAV, 0);

		{
			FCompactViewsVSM_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FCompactViewsVSM_CS::FParameters >();

			PassParameters->GPUSceneParameters = GPUSceneParameters;
			PassParameters->CullingParameters = CullingParameters;
			PassParameters->VirtualShadowMap = VirtualTargetParameters;


			PassParameters->CompactedViewsOut = GraphBuilder.CreateUAV(CompactedViews);
			PassParameters->CompactedViewInfoOut = GraphBuilder.CreateUAV(CompactedViewInfo);
			PassParameters->CompactedViewsAllocationOut = CompactedViewsAllocationUAV;

			check(CullingContext.ViewsBuffer);
			auto ComputeShader = SharedContext.ShaderMap->GetShader<FCompactViewsVSM_CS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CompactViewsVSM"),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(NumPrimaryViews, 64)
			);
		}

		// Override the view info with the compacted info.
		CullingParameters.InViews = GraphBuilder.CreateSRV(CompactedViews);
		CullingContext.ViewsBuffer = CompactedViews;
		CullingParameters.CompactedViewInfo = GraphBuilder.CreateSRV(CompactedViewInfo);
		CullingParameters.CompactedViewsAllocation = GraphBuilder.CreateSRV(CompactedViewsAllocation);
	}

	{
		FInitArgs_CS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitArgs_CS::FParameters >();

		PassParameters->RenderFlags = CullingParameters.RenderFlags;

		PassParameters->OutQueueState						= GraphBuilder.CreateUAV( CullingContext.QueueState );
		PassParameters->InOutMainPassRasterizeArgsSWHW		= GraphBuilder.CreateUAV( CullingContext.MainRasterizeArgsSWHW );

		uint32 ClampedDrawPassIndex = FMath::Min(CullingContext.DrawPassIndex, 2u);

		if (CullingContext.Configuration.bTwoPassOcclusion)
		{
			PassParameters->OutOccludedInstancesArgs = GraphBuilder.CreateUAV( CullingContext.OccludedInstancesArgs );
			PassParameters->InOutPostPassRasterizeArgsSWHW = GraphBuilder.CreateUAV( CullingContext.PostRasterizeArgsSWHW );
		}
		
		check(CullingContext.DrawPassIndex == 0 || CullingContext.RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA); // sanity check
		if (CullingContext.RenderFlags & NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA)
		{
			PassParameters->InOutTotalPrevDrawClusters = GraphBuilder.CreateUAV(CullingContext.TotalPrevDrawClustersBuffer);
		}
		else
		{
			// Use any UAV just to keep render graph happy that something is bound, but the shader doesn't actually touch this.
			PassParameters->InOutTotalPrevDrawClusters = PassParameters->OutQueueState;
		}

		FInitArgs_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FInitArgs_CS::FOcclusionCullingDim>(CullingContext.Configuration.bTwoPassOcclusion);
		PermutationVector.Set<FInitArgs_CS::FDrawPassIndexDim>( ClampedDrawPassIndex );
		
		auto ComputeShader = SharedContext.ShaderMap->GetShader< FInitArgs_CS >( PermutationVector );

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME( "InitArgs" ),
			ComputeShader,
			PassParameters,
			FIntVector( 1, 1, 1 )
		);
	}

	// Allocate buffer for nodes and cluster batches
	FRDGBufferRef MainAndPostNodesAndClusterBatchesBuffer = nullptr;
	AllocateNodesAndBatchesBuffers(GraphBuilder, SharedContext.ShaderMap, &MainAndPostNodesAndClusterBatchesBuffer);

	// Allocate candidate cluster buffer. Lifetime only duration of CullRasterize
	FRDGBufferRef MainAndPostCandididateClustersBuffer = nullptr;
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(4, Nanite::FGlobalResources::GetMaxCandidateClusters() * 2);
		Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
		MainAndPostCandididateClustersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("Nanite.MainAndPostCandididateClustersBuffer"));
	}

	// Per-view primitive filtering
	AddPass_PrimitiveFilter(
		GraphBuilder,
		Scene,
		SceneView,
		GPUSceneParameters,
		SharedContext,
		CullingContext
	);
	
	FBinningData MainPassBinning{};
	FBinningData PostPassBinning{};

	// No Occlusion Pass / Occlusion Main Pass
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, !CullingContext.Configuration.bTwoPassOcclusion, "NoOcclusionPass");
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, CullingContext.Configuration.bTwoPassOcclusion, "MainPass");

		AddPass_InstanceHierarchyAndClusterCull(
			GraphBuilder,
			Scene,
			CullingParameters,
			Views,
			NumPrimaryViews,
			SharedContext,
			CullingContext,
			RasterContext,
			GPUSceneParameters,
			MainAndPostNodesAndClusterBatchesBuffer,
			MainAndPostCandididateClustersBuffer,
			CullingContext.Configuration.bTwoPassOcclusion ? CULLING_PASS_OCCLUSION_MAIN : CULLING_PASS_NO_OCCLUSION,
			VirtualShadowMapArray,
			VirtualTargetParameters
		);

		MainPassBinning = AddPass_Rasterize(
			GraphBuilder,
			RasterPipelines,
			VisibilityResults,
			Views,
			Scene,
			SceneView,
			SharedContext,
			RasterContext,
			CullingContext.PageConstants,
			CullingContext.RenderFlags,
			CullingContext.ViewsBuffer,
			CullingContext.VisibleClustersSWHW,
			nullptr,
			CullingContext.ClusterCountSWHW,
			CullingContext.ClusterClassifyArgs,
			CullingContext.SafeMainRasterizeArgsSWHW,
			CullingContext.TotalPrevDrawClustersBuffer,
			GPUSceneParameters,
			true,
			VirtualShadowMapArray,
			VirtualTargetParameters
		);
	}

	
	// Occlusion post pass. Retest instances and clusters that were not visible last frame. If they are visible now, render them.
	if (CullingContext.Configuration.bTwoPassOcclusion)
	{
		// Build a closest HZB with previous frame occluders to test remainder occluders against.
		if (VirtualShadowMapArray)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "BuildPreviousOccluderHZB(VSM)");
			VirtualShadowMapArray->UpdateHZB(GraphBuilder);
			CullingParameters.HZBTexture = VirtualShadowMapArray->HZBPhysical;
			CullingParameters.HZBSize = CullingParameters.HZBTexture->Desc.Extent;
		}
		else
		{
			RDG_EVENT_SCOPE(GraphBuilder, "BuildPreviousOccluderHZB");
			
			FSceneTextureParameters SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneView);

			FRDGTextureRef SceneDepth = SceneTextures.SceneDepthTexture;
			FRDGTextureRef RasterizedDepth = RasterContext.VisBuffer64;

			if (RasterContext.RasterMode == EOutputBufferMode::DepthOnly)
			{
				SceneDepth = GraphBuilder.RegisterExternalTexture( GSystemTextures.BlackDummy );
				RasterizedDepth = RasterContext.DepthBuffer;
			}

			FRDGTextureRef OutFurthestHZBTexture;

			FIntRect ViewRect(0, 0, RasterContext.TextureSize.X, RasterContext.TextureSize.Y);
			if (Views.Num() == 1)
			{
				//TODO: This is a hack. Using full texture can lead to 'far' borders on left/bottom. How else can we ensure good culling perf for main view.
				ViewRect = FIntRect(Views[0].ViewRect.X, Views[0].ViewRect.Y, Views[0].ViewRect.Z, Views[0].ViewRect.W);
			}
			
			BuildHZBFurthest(
				GraphBuilder,
				SceneDepth,
				RasterizedDepth,
				CullingContext.HZBBuildViewRect,
				Scene.GetFeatureLevel(),
				Scene.GetShaderPlatform(),
				TEXT("Nanite.PreviousOccluderHZB"),
				/* OutFurthestHZBTexture = */ &OutFurthestHZBTexture);

			CullingParameters.HZBTexture = OutFurthestHZBTexture;
			CullingParameters.HZBSize = CullingParameters.HZBTexture->Desc.Extent;
		}

		RDG_EVENT_SCOPE(GraphBuilder, "PostPass");
		// Post Pass
		AddPass_InstanceHierarchyAndClusterCull(
			GraphBuilder,
			Scene,
			CullingParameters,
			Views,
			NumPrimaryViews,
			SharedContext,
			CullingContext,
			RasterContext,
			GPUSceneParameters,
			MainAndPostNodesAndClusterBatchesBuffer,
			MainAndPostCandididateClustersBuffer,
			CULLING_PASS_OCCLUSION_POST,
			VirtualShadowMapArray,
			VirtualTargetParameters
		);

		// Render post pass
		PostPassBinning = AddPass_Rasterize(
			GraphBuilder,
			RasterPipelines,
			VisibilityResults,
			Views,
			Scene,
			SceneView,
			SharedContext,
			RasterContext,
			CullingContext.PageConstants,
			CullingContext.RenderFlags,
			CullingContext.ViewsBuffer,
			CullingContext.VisibleClustersSWHW,
			CullingContext.MainRasterizeArgsSWHW,
			CullingContext.ClusterCountSWHW,
			CullingContext.ClusterClassifyArgs,
			CullingContext.SafePostRasterizeArgsSWHW,
			CullingContext.TotalPrevDrawClustersBuffer,
			GPUSceneParameters,
			false,
			VirtualShadowMapArray,
			VirtualTargetParameters
		);
	}

	if (RasterContext.RasterMode != EOutputBufferMode::DepthOnly)
	{
		// Pass index and number of clusters rendered in previous passes are irrelevant for depth-only rendering.
		CullingContext.DrawPassIndex++;
		CullingContext.RenderFlags |= NANITE_RENDER_FLAG_HAS_PREV_DRAW_DATA;
	}

	if (bExtractStats)
	{
		const bool bVirtualTextureTarget = VirtualShadowMapArray != nullptr;
		ExtractRasterStats(
			GraphBuilder,
			SharedContext,
			CullingContext,
			MainPassBinning,
			PostPassBinning,
			bVirtualTextureTarget
		);
	}

#if !UE_BUILD_SHIPPING
	GGlobalResources.GetFeedbackManager()->Update(GraphBuilder, SharedContext, CullingContext);
#endif
}

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
	const TArray<FInstanceDraw, SceneRenderingAllocator>* OptionalInstanceDraws,
	bool bExtractStats
)
{
	CullRasterize(
		GraphBuilder,
		RasterPipelines,
		VisibilityResults,
		Scene,
		SceneView,
		Views,
		Views.Num(),
		SharedContext,
		CullingContext,
		RasterContext,
		OptionalInstanceDraws,
		nullptr,
		bExtractStats
	);
}

void FCullingContext::FConfiguration::SetViewFlags(const FViewInfo& View)
{
	bIsGameView							= View.bIsGameView;
	bIsSceneCapture						= View.bIsSceneCapture;
	bIsReflectionCapture				= View.bIsReflectionCapture;
	bGameShowFlag						= !!View.Family->EngineShowFlags.Game;
	bEditorShowFlag						= !!View.Family->EngineShowFlags.Editor;
	bDrawOnlyVSMInvalidatingGeometry	= !!View.Family->EngineShowFlags.DrawOnlyVSMInvalidatingGeo;
}

} // namespace Nanite
