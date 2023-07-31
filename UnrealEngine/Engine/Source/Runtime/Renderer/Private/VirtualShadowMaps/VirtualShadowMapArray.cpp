// Copyright Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	VirtualShadowMap.h:
=============================================================================*/
#include "VirtualShadowMapArray.h"
#include "BasePassRendering.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/LightComponent.h"
#include "GPUMessaging.h"
#include "HairStrands/HairStrandsData.h"
#include "InstanceCulling/InstanceCullingMergedContext.h"
#include "RendererModule.h"
#include "Rendering/NaniteResources.h"
#include "SceneTextureReductions.h"
#include "ScreenPass.h"
#include "ShaderPrint.h"
#include "ShaderPrintParameters.h"
#include "VirtualShadowMapCacheManager.h"
#include "VirtualShadowMapClipmap.h"
#include "VirtualShadowMapVisualizationData.h"

#define DEBUG_ALLOW_STATIC_SEPARATE_WITHOUT_CACHING 0

IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(VirtualShadowMapUbSlot);

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FVirtualShadowMapUniformParameters, "VirtualShadowMap", VirtualShadowMapUbSlot);

struct FShadowMapCacheData
{
	int32 PrevVirtualShadowMapId = INDEX_NONE;

	FIntPoint ClipmapCornerOffsetDelta = FIntPoint(0, 0);
	int32 Padding[1];
};


struct FPhysicalPageMetaData
{	
	uint32 Flags;
	uint32 Age;
	uint32 VirtualPageOffset;
	uint32 VirtualShadowMapId;
};

struct FPhysicalPageRequest
{
	uint32 VirtualShadowMapId;
	uint32 GlobalPageOffset;
};

extern int32 GForceInvalidateDirectionalVSM;

int32 GVSMShowLightDrawEvents = 0;
FAutoConsoleVariableRef CVarVSMShowLightDrawEvents(
	TEXT("r.Shadow.Virtual.ShowLightDrawEvents"),
	GVSMShowLightDrawEvents,
	TEXT("Enable Virtual Shadow Maps per-light draw events - may affect performance especially when there are many small lights in the scene."),
	ECVF_RenderThreadSafe
);

int32 GEnableVirtualShadowMaps = 0;
FAutoConsoleVariableRef CVarEnableVirtualShadowMaps(
	TEXT("r.Shadow.Virtual.Enable"),
	GEnableVirtualShadowMaps,
	TEXT("Enable Virtual Shadow Maps."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// Needed because the depth state changes with method (so cached draw commands must be re-created) see SetStateForShadowDepth
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMaxPhysicalPages(
	TEXT("r.Shadow.Virtual.MaxPhysicalPages"),
	2048,
	TEXT("Maximum number of physical pages in the pool."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMaxPhysicalPagesSceneCapture(
	TEXT("r.Shadow.Virtual.MaxPhysicalPagesSceneCapture"),
	512,
	TEXT("Maximum number of physical pages in the pool for each scene capture component."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarCacheStaticSeparate(
	TEXT("r.Shadow.Virtual.Cache.StaticSeparate"),
	1,
	TEXT("When enabled, caches static objects in separate pages from dynamic objects.\n")
	TEXT("This can improve performance in largely static scenes, but doubles the memory cost of the physical page pool."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarShowStats(
	TEXT("r.Shadow.Virtual.ShowStats"),
	0,
	TEXT("ShowStats, also toggle shaderprint one!"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarResolutionLodBiasLocal(
	TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"),
	0.0f,
	TEXT("Bias applied to LOD calculations for local lights. -1.0 doubles resolution, 1.0 halves it and so on."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarPageDilationBorderSizeDirectional(
	TEXT("r.Shadow.Virtual.PageDilationBorderSizeDirectional"),
	0.05f,
	TEXT("If a screen pixel falls within this fraction of a page border for directional lights, the adacent page will also be mapped.")
	TEXT("Higher values can reduce page misses at screen edges or disocclusions, but increase total page counts."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarPageDilationBorderSizeLocal(
	TEXT("r.Shadow.Virtual.PageDilationBorderSizeLocal"),
	0.05f,
	TEXT("If a screen pixel falls within this fraction of a page border for local lights, the adacent page will also be mapped.")
	TEXT("Higher values can reduce page misses at screen edges or disocclusions, but increase total page counts."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMarkPixelPages(
	TEXT("r.Shadow.Virtual.MarkPixelPages"),
	1,
	TEXT("Marks pages in virtual shadow maps based on depth buffer pixels. Ability to disable is primarily for profiling and debugging."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMarkCoarsePagesDirectional(
	TEXT("r.Shadow.Virtual.MarkCoarsePagesDirectional"),
	1,
	TEXT("Marks coarse pages in directional light virtual shadow maps so that low resolution data is available everywhere.")
	TEXT("Ability to disable is primarily for profiling and debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMarkCoarsePagesLocal(
	TEXT("r.Shadow.Virtual.MarkCoarsePagesLocal"),
	1,
	TEXT("Marks coarse pages in local light virtual shadow maps so that low resolution data is available everywhere.")
	TEXT("Ability to disable is primarily for profiling and debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarCoarsePagesIncludeNonNanite(
	TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages"),
	1,
	TEXT("Include non-Nanite geometry in coarse pages.")
	TEXT("Rendering non-Nanite geometry into large coarse pages can be expensive; disabling this can be a significant performance win."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarShowClipmapStats(
	TEXT("r.Shadow.Virtual.ShowClipmapStats"),
	-1,
	TEXT("Set to the number of clipmap you want to show stats for (-1 == off)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarCullBackfacingPixels(
	TEXT("r.Shadow.Virtual.CullBackfacingPixels"),
	1,
	TEXT("When enabled does not generate shadow data for pixels that are backfacing to the light."),
	ECVF_RenderThreadSafe
);

int32 GEnableNonNaniteVSM = 1;
FAutoConsoleVariableRef CVarEnableNonNaniteVSM(
	TEXT("r.Shadow.Virtual.NonNaniteVSM"),
	GEnableNonNaniteVSM,
	TEXT("Enable support for non-nanite Virtual Shadow Maps.")
	TEXT("Read-only and to be set in a config file (requires restart)."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarNonNaniteVsmUseHzb(
	TEXT("r.Shadow.Virtual.NonNanite.UseHZB"),
	2,
	TEXT("Cull Non-Nanite instances using HZB. If set to 2, attempt to use Nanite-HZB from the current frame."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarInitializePhysicalUsingIndirect(
	TEXT("r.Shadow.Virtual.InitPhysicalUsingIndirect"),
	1,
	TEXT("."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMergePhysicalUsingIndirect(
	TEXT("r.Shadow.Virtual.MergePhysicalUsingIndirect"),
	1,
	TEXT("."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarVirtualShadowOnePassProjectionMaxLights(
	TEXT("r.Shadow.Virtual.OnePassProjection.MaxLightsPerPixel"),
	16,
	TEXT("Maximum lights per pixel that get full filtering when using one pass projection and clustered shading.")
	TEXT("Generally set to 8 (32bpp), 16 (64bpp) or 32 (128bpp). Lower values require less transient VRAM during the lighting pass."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarDoNonNaniteBatching(
	TEXT("r.Shadow.Virtual.NonNanite.Batch"),
	1,
	TEXT("."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarCoarsePagePixelThresholdDynamic(
	TEXT("r.Shadow.Virtual.CoarsePagePixelThresholdDynamic"),
	16.0f,
	TEXT("If a dynamic (non-nanite) instance has a smaller footprint, it should not be drawn into a coarse page."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarCoarsePagePixelThresholdStatic(
	TEXT("r.Shadow.Virtual.CoarsePagePixelThresholdStatic"),
	1.0f,
	TEXT("If a static (non-nanite) instance has a smaller footprint, it should not be drawn into a coarse page."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarCoarsePagePixelThresholdDynamicNanite(
	TEXT("r.Shadow.Virtual.CoarsePagePixelThresholdDynamicNanite"),
	4.0f,
	TEXT("If a dynamic Nanite instance has a smaller footprint, it should not be drawn into a coarse page."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);


#if !UE_BUILD_SHIPPING
bool GDumpVSMLightNames = false;
void DumpVSMLightNames()
{
	ENQUEUE_RENDER_COMMAND(DumpVSMLightNames)(
		[](FRHICommandList& RHICmdList)
		{
			GDumpVSMLightNames = true;
		});
}

FAutoConsoleCommand CmdDumpVSMLightNames(
	TEXT("r.Shadow.Virtual.Visualize.DumpLightNames"),
	TEXT("Dump light names with virtual shadow maps (for developer use in non-shiping builds)"),
	FConsoleCommandDelegate::CreateStatic(DumpVSMLightNames)
);

FString GVirtualShadowMapVisualizeLightName;
FAutoConsoleVariableRef CVarVisualizeLightName(
	TEXT("r.Shadow.Virtual.Visualize.LightName"),
	GVirtualShadowMapVisualizeLightName,
	TEXT("Sets the name of a specific light to visualize (for developer use in non-shiping builds)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVisualizeLayout(
	TEXT("r.Shadow.Virtual.Visualize.Layout"),
	0,
	TEXT("Overlay layout when virtual shadow map visualization is enabled:\n")
	TEXT("  0: Full screen\n")
	TEXT("  1: Thumbnail\n")
	TEXT("  2: Split screen"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarDebugSkipMergePhysical(
	TEXT("r.Shadow.Virtual.DebugSkipMergePhysical"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarDebugSkipDynamicPageInvalidation(
	TEXT("r.Shadow.Virtual.Cache.DebugSkipDynamicPageInvalidation"),
	0,
	TEXT("Skip invalidation of cached pages when geometry moves for debugging purposes. This will create obvious visual artifacts when disabled.\n"),
	ECVF_RenderThreadSafe
);
#endif // !UE_BUILD_SHIPPING

static TAutoConsoleVariable<float> CVarMaxMaterialPositionInvalidationRange(
	TEXT("r.Shadow.Virtual.Cache.MaxMaterialPositionInvalidationRange"),
	-1.0f,
	TEXT("Beyond this distance in world units, material position effects (e.g., WPO or PDO) cease to cause VSM invalidations.\n")
	TEXT(" This can be used to tune performance by reducing re-draw overhead, but causes some artifacts.\n")
	TEXT(" < 0 <=> infinite (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarShadowsVirtualUseHZB(
	TEXT("r.Shadow.Virtual.UseHZB"),
	2,
	TEXT("Enables HZB for (Nanite) Virtual Shadow Maps - Non-Nanite unfortunately has a separate flag with different semantics: r.Shadow.Virtual.NonNanite.UseHZB.\n")
	TEXT(" 0 - No HZB occlusion culling\n")
	TEXT(" 1 - Approximate Single-pass HZB occlusion culling (using previous frame HZB)\n")
	TEXT(" 2 - Two-pass occlusion culling (default)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowsVirtualForceFullHZBUpdate(
	TEXT("r.Shadow.Virtual.ForceFullHZBUpdate"),
	0,
	TEXT("Forces full HZB update every frame rather than just dirty pages.\n"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarVirtualShadowSinglePassBatched(
	TEXT("r.Shadow.Virtual.NonNanite.SinglePassBatched"),
	1,
	TEXT("."),
	ECVF_RenderThreadSafe);

FMatrix CalcTranslatedWorldToShadowUVMatrix(
	const FMatrix& TranslatedWorldToShadowView,
	const FMatrix& ViewToClip)
{
	FMatrix TranslatedWorldToShadowClip = TranslatedWorldToShadowView * ViewToClip;
	FMatrix ScaleAndBiasToSmUV = FScaleMatrix(FVector(0.5f, -0.5f, 1.0f)) * FTranslationMatrix(FVector(0.5f, 0.5f, 0.0f));
	FMatrix TranslatedWorldToShadowUv = TranslatedWorldToShadowClip * ScaleAndBiasToSmUV;
	return TranslatedWorldToShadowUv;
}

FMatrix CalcTranslatedWorldToShadowUVNormalMatrix(
	const FMatrix& TranslatedWorldToShadowView,
	const FMatrix& ViewToClip)
{
	return CalcTranslatedWorldToShadowUVMatrix(TranslatedWorldToShadowView, ViewToClip).GetTransposed().Inverse();
}


template <typename ShaderType>
static bool SetStatsArgsAndPermutation(FRDGBuilder& GraphBuilder, FRDGBufferRef StatsBufferRDG, typename ShaderType::FParameters *OutPassParameters, typename ShaderType::FPermutationDomain& OutPermutationVector)
{
	bool bGenerateStats = StatsBufferRDG != nullptr;
	if (bGenerateStats)
	{
		OutPassParameters->OutStatsBuffer = GraphBuilder.CreateUAV(StatsBufferRDG);
	}
	;
	OutPermutationVector.template Set<typename ShaderType::FGenerateStatsDim>(bGenerateStats);

	return bGenerateStats;
}

FVirtualShadowMapArray::FVirtualShadowMapArray(FScene& InScene) 
	: Scene(InScene)
{
}

BEGIN_SHADER_PARAMETER_STRUCT(FCacheDataParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FShadowMapCacheData >,	ShadowMapCacheData )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,					PrevPageFlags )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,					PrevPageTable )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPhysicalPageMetaData >,	PrevPhysicalPageMetaData)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,					PrevPhysicalPageMetaDataOut)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,					PrevProjectionData)
END_SHADER_PARAMETER_STRUCT()

static void SetCacheDataShaderParameters(FRDGBuilder& GraphBuilder, const TArray<TUniquePtr<FVirtualShadowMap>, SceneRenderingAllocator>& ShadowMaps, FVirtualShadowMapArrayCacheManager* CacheManager, FCacheDataParameters &CacheDataParameters)
{
	TArray<FShadowMapCacheData, SceneRenderingAllocator> ShadowMapCacheData;
	ShadowMapCacheData.AddDefaulted(ShadowMaps.Num());
	for (int32 SmIndex = 0; SmIndex < ShadowMaps.Num(); ++SmIndex)
	{
		TSharedPtr<FVirtualShadowMapCacheEntry> VirtualShadowMapCacheEntry = ShadowMaps[SmIndex].IsValid() ? ShadowMaps[SmIndex]->VirtualShadowMapCacheEntry : nullptr;
		if (VirtualShadowMapCacheEntry != nullptr && VirtualShadowMapCacheEntry->IsValid())
		{
			ShadowMapCacheData[SmIndex].PrevVirtualShadowMapId = VirtualShadowMapCacheEntry->PrevVirtualShadowMapId;
			
			const int64 TileSize = FLargeWorldRenderScalar::GetTileSize();
			const FInt64Point& CurOffset = VirtualShadowMapCacheEntry->Clipmap.CurrentClipmapCornerOffset;
			const FInt64Point& PrevOffset = VirtualShadowMapCacheEntry->Clipmap.PrevClipmapCornerOffset;
			ShadowMapCacheData[SmIndex].ClipmapCornerOffsetDelta = FInt32Point(PrevOffset - CurOffset);
		}
		else
		{
			ShadowMapCacheData[SmIndex].PrevVirtualShadowMapId = INDEX_NONE;
			ShadowMapCacheData[SmIndex].ClipmapCornerOffsetDelta = FInt32Point(0, 0);
		}
	}
	CacheDataParameters.ShadowMapCacheData = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.ShadowMapCacheData"), ShadowMapCacheData));
	CacheDataParameters.PrevPageFlags = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PageFlags, TEXT("Shadow.Virtual.PrevPageFlags")));
	CacheDataParameters.PrevPageTable = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PageTable, TEXT("Shadow.Virtual.PrevPageTable")));
	CacheDataParameters.PrevPhysicalPageMetaData = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PhysicalPageMetaData, TEXT("Shadow.Virtual.PrevPhysicalPageMetaData")));
	CacheDataParameters.PrevProjectionData = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.ProjectionData, TEXT("Shadow.Virtual.PrevProjectionData")));
}


void FVirtualShadowMapArray::Initialize(FRDGBuilder& GraphBuilder, FVirtualShadowMapArrayCacheManager* InCacheManager, bool bInEnabled, bool bIsSceneCapture)
{
	bInitialized = true;
	bEnabled = bInEnabled;
	CacheManager = InCacheManager;

#if WITH_MGPU
	CacheManager->UpdateGPUMask(GraphBuilder.RHICmdList.GetGPUMask());
#endif

	bCullBackfacingPixels = CVarCullBackfacingPixels.GetValueOnRenderThread() != 0;
	bUseHzbOcclusion = CVarShadowsVirtualUseHZB.GetValueOnRenderThread() != 0;
	bUseTwoPassHzbOcclusion = CVarShadowsVirtualUseHZB.GetValueOnRenderThread() == 2;

	UniformParameters.NumFullShadowMaps = 0;
	UniformParameters.NumSinglePageShadowMaps = 0;
	UniformParameters.NumShadowMapSlots = 0;
	UniformParameters.MaxPhysicalPages = 0;
	UniformParameters.StaticCachedArrayIndex = 0;
	// NOTE: Most uniform values don't matter when VSM is disabled

	UniformParameters.bExcludeNonNaniteFromCoarsePages = !CVarCoarsePagesIncludeNonNanite.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdDynamic = CVarCoarsePagePixelThresholdDynamic.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdStatic = CVarCoarsePagePixelThresholdStatic.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdDynamicNanite = CVarCoarsePagePixelThresholdDynamicNanite.GetValueOnRenderThread();

	// Reference dummy data in the UB initially
	const uint32 DummyPageTableElement = 0xFFFFFFFF;
	UniformParameters.PageTable = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(DummyPageTableElement), DummyPageTableElement));
	UniformParameters.ProjectionData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVirtualShadowMapProjectionShaderData)));
	UniformParameters.PageFlags = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(uint32)));
	UniformParameters.PageRectBounds = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FIntVector4)));

	if (bEnabled)
	{
		// Always reserve IDs for the single-page SMs.
		ShadowMaps.SetNum(VSM_MAX_SINGLE_PAGE_SHADOW_MAPS);

		// Fixed physical page pool width, we adjust the height to accomodate the requested maximum
		// NOTE: Row size in pages has to be POT since we use mask & shift in place of integer ops
		// NOTE: This assumes GetMax2DTextureDimension() is a power of two on supported platforms
		const uint32 PhysicalPagesX = FMath::DivideAndRoundDown(GetMax2DTextureDimension(), FVirtualShadowMap::PageSize);
		check(FMath::IsPowerOfTwo(PhysicalPagesX));
		const int32 MaxPhysicalPages = (bIsSceneCapture ? CVarMaxPhysicalPagesSceneCapture : CVarMaxPhysicalPages).GetValueOnRenderThread();
		uint32 PhysicalPagesY = FMath::DivideAndRoundUp((uint32)FMath::Max(1, MaxPhysicalPages), PhysicalPagesX);	

		UniformParameters.MaxPhysicalPages = PhysicalPagesX * PhysicalPagesY;
				
		if (CVarCacheStaticSeparate.GetValueOnRenderThread() != 0)
		{
			#if !DEBUG_ALLOW_STATIC_SEPARATE_WITHOUT_CACHING
			if (CacheManager->IsValid())
			#endif
			{
				// Enable separate static caching in the second texture array element
				UniformParameters.StaticCachedArrayIndex = 1;
			}
		}

		uint32 PhysicalX = PhysicalPagesX * FVirtualShadowMap::PageSize;
		uint32 PhysicalY = PhysicalPagesY * FVirtualShadowMap::PageSize;

		// TODO: Some sort of better fallback with warning?
		// All supported platforms support at least 16384 texture dimensions which translates to 16384 max pages with default 128x128 page size
		check(PhysicalX <= GetMax2DTextureDimension());
		check(PhysicalY <= GetMax2DTextureDimension());

		UniformParameters.PhysicalPageRowMask = (PhysicalPagesX - 1);
		UniformParameters.PhysicalPageRowShift = FMath::FloorLog2( PhysicalPagesX );
		UniformParameters.RecPhysicalPoolSize = FVector4f( 1.0f / PhysicalX, 1.0f / PhysicalY, 1.0f, 1.0f );
		UniformParameters.PhysicalPoolSize = FIntPoint( PhysicalX, PhysicalY );
		UniformParameters.PhysicalPoolSizePages = FIntPoint( PhysicalPagesX, PhysicalPagesY );

		// TODO: Parameterize this in a useful way; potentially modify it automatically
		// when there are fewer lights in the scene and/or clustered shading settings differ.
		UniformParameters.PackedShadowMaskMaxLightCount = FMath::Min(CVarVirtualShadowOnePassProjectionMaxLights.GetValueOnRenderThread(), 32);

		// If enabled, ensure we have a properly-sized physical page pool
		// We can do this here since the pool is independent of the number of shadow maps
		const int PoolArraySize = ShouldCacheStaticSeparately() ? 2 : 1;
		TRefCountPtr<IPooledRenderTarget> PhysicalPagePool = CacheManager->SetPhysicalPoolSize(GraphBuilder, GetPhysicalPoolSize(), PoolArraySize);
		PhysicalPagePoolRDG = GraphBuilder.RegisterExternalTexture(PhysicalPagePool);
		UniformParameters.PhysicalPagePool = PhysicalPagePoolRDG;
	}
	else
	{
		CacheManager->FreePhysicalPool();
		UniformParameters.PhysicalPagePool = GSystemTextures.GetZeroUIntArrayDummy(GraphBuilder);
	}

	if (bEnabled && bUseHzbOcclusion)
	{
		TRefCountPtr<IPooledRenderTarget> HzbPhysicalPagePool = CacheManager->SetHZBPhysicalPoolSize(GraphBuilder, GetHZBPhysicalPoolSize(), PF_R32_FLOAT);
		HZBPhysical = GraphBuilder.RegisterExternalTexture(HzbPhysicalPagePool);
	}
	else
	{
		CacheManager->FreeHZBPhysicalPool();
		HZBPhysical = GSystemTextures.GetZeroUIntDummy(GraphBuilder);
	}

	UpdateCachedUniformBuffer(GraphBuilder);
}

FVirtualShadowMap* FVirtualShadowMapArray::Allocate(bool bSinglePageShadowMap)
{
	check(IsEnabled());
	FVirtualShadowMap* SM = nullptr;
	if (bSinglePageShadowMap)
	{
		if (ensure(NumSinglePageSms < VSM_MAX_SINGLE_PAGE_SHADOW_MAPS))
		{
			SM = new FVirtualShadowMap(NumSinglePageSms, true);
			ShadowMaps[NumSinglePageSms++] = TUniquePtr<FVirtualShadowMap>(SM);
		}
	}
	else
	{
		SM = new FVirtualShadowMap(ShadowMaps.Num(), false);
		ShadowMaps.Emplace(SM);
	}

	return SM;
}

float FVirtualShadowMapArray::GetResolutionLODBiasLocal() const
{
	return CVarResolutionLodBiasLocal.GetValueOnRenderThread();
}

FVirtualShadowMapArray::~FVirtualShadowMapArray()
{
}

EPixelFormat FVirtualShadowMapArray::GetPackedShadowMaskFormat() const
{
	// TODO: Check if we're after any point that determines the format later too (light setup)
	check(bInitialized);
	// NOTE: Currently 4bpp/light
	if (UniformParameters.PackedShadowMaskMaxLightCount <= 8)
	{
		return PF_R32_UINT;
	}
	else if (UniformParameters.PackedShadowMaskMaxLightCount <= 16)
	{
		return PF_R32G32_UINT;
	}
	else
	{
		check(UniformParameters.PackedShadowMaskMaxLightCount <= 32);
		return PF_R32G32B32A32_UINT;
	}
}

FIntPoint FVirtualShadowMapArray::GetPhysicalPoolSize() const
{
	check(bInitialized);
	return FIntPoint(UniformParameters.PhysicalPoolSize.X, UniformParameters.PhysicalPoolSize.Y);
}

FIntPoint FVirtualShadowMapArray::GetHZBPhysicalPoolSize() const
{
	check(bInitialized);
	FIntPoint PhysicalPoolSize = GetPhysicalPoolSize();
	FIntPoint HZBSize(FMath::Max(FPlatformMath::RoundUpToPowerOfTwo(PhysicalPoolSize.X) >> 1, 1u),
	                  FMath::Max(FPlatformMath::RoundUpToPowerOfTwo(PhysicalPoolSize.Y) >> 1, 1u));
	return HZBSize;
}

uint32 FVirtualShadowMapArray::GetTotalAllocatedPhysicalPages() const
{
	check(bInitialized);
	return ShouldCacheStaticSeparately() ? (2U * UniformParameters.MaxPhysicalPages) : UniformParameters.MaxPhysicalPages;
}

TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> FVirtualShadowMapArray::GetUncachedUniformBuffer(FRDGBuilder& GraphBuilder) const
{
	// NOTE: Need to allocate new parameter space since the UB changes over the frame as dummy references are replaced
	FVirtualShadowMapUniformParameters* VersionedParameters = GraphBuilder.AllocParameters<FVirtualShadowMapUniformParameters>();
	*VersionedParameters = UniformParameters;
	return GraphBuilder.CreateUniformBuffer(VersionedParameters);
}

void FVirtualShadowMapArray::UpdateCachedUniformBuffer(FRDGBuilder& GraphBuilder)
{
	CachedUniformBuffer = GetUncachedUniformBuffer(GraphBuilder);
}

void FVirtualShadowMapArray::SetShaderDefines(FShaderCompilerEnvironment& OutEnvironment)
{
	static_assert(FVirtualShadowMap::Log2Level0DimPagesXY * 2U + NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS_BITS <= 32U, "Page indirection plus view index must fit into 32-bits for page-routing storage!");
	OutEnvironment.SetDefine(TEXT("ENABLE_NON_NANITE_VSM"), GEnableNonNaniteVSM);
	OutEnvironment.SetDefine(TEXT("VSM_PAGE_SIZE"), FVirtualShadowMap::PageSize);
	OutEnvironment.SetDefine(TEXT("VSM_PAGE_SIZE_MASK"), FVirtualShadowMap::PageSizeMask);
	OutEnvironment.SetDefine(TEXT("VSM_LOG2_PAGE_SIZE"), FVirtualShadowMap::Log2PageSize);
	OutEnvironment.SetDefine(TEXT("VSM_LEVEL0_DIM_PAGES_XY"), FVirtualShadowMap::Level0DimPagesXY);
	OutEnvironment.SetDefine(TEXT("VSM_LOG2_LEVEL0_DIM_PAGES_XY"), FVirtualShadowMap::Log2Level0DimPagesXY);
	OutEnvironment.SetDefine(TEXT("VSM_MAX_MIP_LEVELS"), FVirtualShadowMap::MaxMipLevels);
	OutEnvironment.SetDefine(TEXT("VSM_VIRTUAL_MAX_RESOLUTION_XY"), FVirtualShadowMap::VirtualMaxResolutionXY);
	OutEnvironment.SetDefine(TEXT("VSM_RASTER_WINDOW_PAGES"), FVirtualShadowMap::RasterWindowPages);
	OutEnvironment.SetDefine(TEXT("VSM_PAGE_TABLE_SIZE"), FVirtualShadowMap::PageTableSize);
	OutEnvironment.SetDefine(TEXT("VSM_NUM_STATS"), NumStats);
	OutEnvironment.SetDefine(TEXT("INDEX_NONE"), INDEX_NONE);
}

FVirtualShadowMapSamplingParameters FVirtualShadowMapArray::GetSamplingParameters(FRDGBuilder& GraphBuilder) const
{
	// Sanity check: either VSMs are disabled and it's expected to be relying on dummy data, or we should have valid data
	// If this fires, it is likely because the caller is trying to sample VSMs before they have been rendered by the ShadowDepths pass
	// This should not crash, but it is not an intended production path as it will not return valid shadow data.
	// TODO: Disabled warning until SkyAtmosphereLUT is moved after ShadowDepths
	//ensureMsgf(!IsEnabled() || IsAllocated(),
	//	TEXT("Attempt to use Virtual Shadow Maps before they have been rendered by ShadowDepths."));

	FVirtualShadowMapSamplingParameters Parameters;
	Parameters.VirtualShadowMap = GetUniformBuffer();
	return Parameters;
}

class FVirtualPageManagementShader : public FGlobalShader
{
public:
	// Kernel launch group sizes
	static constexpr uint32 DefaultCSGroupXY = 8;
	static constexpr uint32 DefaultCSGroupX = 256;
	static constexpr uint32 GeneratePageFlagsGroupXYZ = 4;
	static constexpr uint32 BuildExplicitBoundsGroupXY = 16;

	FVirtualPageManagementShader()
	{
	}

	FVirtualPageManagementShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("VSM_DEFAULT_CS_GROUP_X"), DefaultCSGroupX);
		OutEnvironment.SetDefine(TEXT("VSM_DEFAULT_CS_GROUP_XY"), DefaultCSGroupXY);
		OutEnvironment.SetDefine(TEXT("VSM_GENERATE_PAGE_FLAGS_CS_GROUP_XYZ"), GeneratePageFlagsGroupXYZ);
		OutEnvironment.SetDefine(TEXT("VSM_BUILD_EXPLICIT_BOUNDS_CS_XY"), BuildExplicitBoundsGroupXY);

		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}
};

class FPruneLightGridCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FPruneLightGridCS);
	SHADER_USE_PARAMETER_STRUCT(FPruneLightGridCS, FVirtualPageManagementShader)
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPrunedLightGridData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPrunedNumCulledLightsGrid)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPruneLightGridCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "PruneLightGridCS", SF_Compute);

class FGeneratePageFlagsFromPixelsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FGeneratePageFlagsFromPixelsCS);
	SHADER_USE_PARAMETER_STRUCT(FGeneratePageFlagsFromPixelsCS, FVirtualPageManagementShader)

	class FInputType : SHADER_PERMUTATION_INT("PERMUTATION_INPUT_TYPE", 3); 
	using FPermutationDomain = TShaderPermutationDomain<FInputType>;
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, VisBuffer64)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPageRequestFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, DirectionalLightIds)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PrunedLightGridData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PrunedNumCulledLightsGrid)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SingeLayerWaterDepthTexture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FStrataGlobalUniformParameters, Strata)
		RDG_BUFFER_ACCESS(IndirectBufferArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(uint32, InputType)
		SHADER_PARAMETER(uint32, NumDirectionalLightSmInds)
		SHADER_PARAMETER(uint32, bPostBasePass)
		SHADER_PARAMETER(float, ResolutionLodBiasLocal)
		SHADER_PARAMETER(float, PageDilationBorderSizeDirectional)
		SHADER_PARAMETER(float, PageDilationBorderSizeLocal)
		SHADER_PARAMETER(uint32, bCullBackfacingPixels)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FGeneratePageFlagsFromPixelsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "GeneratePageFlagsFromPixels", SF_Compute);

class FMarkCoarsePagesCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FMarkCoarsePagesCS);
	SHADER_USE_PARAMETER_STRUCT(FMarkCoarsePagesCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPageRequestFlags)
		SHADER_PARAMETER(uint32, bMarkCoarsePagesLocal)
		SHADER_PARAMETER(uint32, bIncludeNonNaniteGeometry)
		SHADER_PARAMETER(uint32, ClipmapIndexMask)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FMarkCoarsePagesCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "MarkCoarsePages", SF_Compute);


class FGenerateHierarchicalPageFlagsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FGenerateHierarchicalPageFlagsCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateHierarchicalPageFlagsCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPhysicalPageMetaData >, PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPageFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntVector4>, OutPageRectBounds)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FGenerateHierarchicalPageFlagsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "GenerateHierarchicalPageFlags", SF_Compute);


class FInitPhysicalPageMetaData : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FInitPhysicalPageMetaData);
	SHADER_USE_PARAMETER_STRUCT(FInitPhysicalPageMetaData, FVirtualPageManagementShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FPhysicalPageMetaData >,	OutPhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutFreePhysicalPages )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FPhysicalPageRequest >,	OutPhysicalPageAllocationRequests )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitPhysicalPageMetaData, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "InitPhysicalPageMetaData", SF_Compute );

class FCreateCachedPageMappingsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FCreateCachedPageMappingsCS);
	SHADER_USE_PARAMETER_STRUCT(FCreateCachedPageMappingsCS, FVirtualPageManagementShader)

	class FHasCacheDataDim : SHADER_PERMUTATION_BOOL("HAS_CACHE_DATA");
	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS"); 
	using FPermutationDomain = TShaderPermutationDomain<FHasCacheDataDim, FGenerateStatsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FVirtualShadowMapUniformParameters,		VirtualShadowMap )
		SHADER_PARAMETER_STRUCT_INCLUDE( FCacheDataParameters,							CacheDataParameters )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,						PageRequestFlags )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutPageFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutPageTable )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FPhysicalPageMetaData >,	OutPhysicalPageMetaData )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FPhysicalPageRequest >,	OutPhysicalPageAllocationRequests )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutStatsBuffer )
		SHADER_PARAMETER( int32,														bDynamicPageInvalidation )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCreateCachedPageMappingsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "CreateCachedPageMappingsCS", SF_Compute);

class FPackFreePagesCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FPackFreePagesCS);
	SHADER_USE_PARAMETER_STRUCT(FPackFreePagesCS, FVirtualPageManagementShader )

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters,			VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPhysicalPageMetaData >,		PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutFreePhysicalPages )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPackFreePagesCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "PackFreePages", SF_Compute );

class FAllocateNewPageMappingsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FAllocateNewPageMappingsCS);
	SHADER_USE_PARAMETER_STRUCT(FAllocateNewPageMappingsCS, FVirtualPageManagementShader)

	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS"); 
	using FPermutationDomain = TShaderPermutationDomain<FGenerateStatsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FVirtualShadowMapUniformParameters,		VirtualShadowMap )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,						PageRequestFlags )
		SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPhysicalPageRequest >,		PhysicalPageAllocationRequests )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutFreePhysicalPages )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutPageFlags)
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutPageTable )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FPhysicalPageMetaData >,	OutPhysicalPageMetaData )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,					OutStatsBuffer )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FAllocateNewPageMappingsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "AllocateNewPageMappings", SF_Compute);

class FPropagateMappedMipsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FPropagateMappedMipsCS);
	SHADER_USE_PARAMETER_STRUCT(FPropagateMappedMipsCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FVirtualShadowMapUniformParameters,	VirtualShadowMap )
		SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,				OutPageTable )
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPropagateMappedMipsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "PropagateMappedMips", SF_Compute);

class FInitializePhysicalPagesCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FInitializePhysicalPagesCS);
	SHADER_USE_PARAMETER_STRUCT(FInitializePhysicalPagesCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>,	PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutPhysicalPagePool)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitializePhysicalPagesCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "InitializePhysicalPages", SF_Compute);

class FSelectPagesToInitializeCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FSelectPagesToInitializeCS);
	SHADER_USE_PARAMETER_STRUCT(FSelectPagesToInitializeCS, FVirtualPageManagementShader)

	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS");
	using FPermutationDomain = TShaderPermutationDomain<FGenerateStatsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPhysicalPageMetaData>, PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutInitializePagesIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutPhysicalPagesToInitialize)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutStatsBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FSelectPagesToInitializeCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "SelectPagesToInitializeCS", SF_Compute);

class FInitializePhysicalPagesIndirectCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FInitializePhysicalPagesIndirectCS);
	SHADER_USE_PARAMETER_STRUCT(FInitializePhysicalPagesIndirectCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPhysicalPageMetaData>, PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PhysicalPagesToInitialize)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutPhysicalPagePool)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitializePhysicalPagesIndirectCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "InitializePhysicalPagesIndirectCS", SF_Compute);

class FClearIndirectDispatchArgs1DCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FClearIndirectDispatchArgs1DCS);
	SHADER_USE_PARAMETER_STRUCT(FClearIndirectDispatchArgs1DCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumIndirectArgs)
		SHADER_PARAMETER(uint32, IndirectArgStride)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutIndirectArgsBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FClearIndirectDispatchArgs1DCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "ClearIndirectDispatchArgs1DCS", SF_Compute);

static void AddClearIndirectDispatchArgs1DPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FRDGBufferRef IndirectArgsRDG, uint32 NumIndirectArgs = 1U, uint32 IndirectArgStride = 4U)
{
	FClearIndirectDispatchArgs1DCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearIndirectDispatchArgs1DCS::FParameters>();
	PassParameters->NumIndirectArgs = NumIndirectArgs;
	PassParameters->IndirectArgStride = IndirectArgStride;
	PassParameters->OutIndirectArgsBuffer = GraphBuilder.CreateUAV(IndirectArgsRDG);

	auto ComputeShader = GetGlobalShaderMap(FeatureLevel)->GetShader<FClearIndirectDispatchArgs1DCS>();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClearIndirectDispatchArgs"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(NumIndirectArgs, 64)
	);
}

class FMergeStaticPhysicalPagesCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FMergeStaticPhysicalPagesCS);
	SHADER_USE_PARAMETER_STRUCT(FMergeStaticPhysicalPagesCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>,	PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutPhysicalPagePool)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FMergeStaticPhysicalPagesCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "MergeStaticPhysicalPages", SF_Compute);

class FSelectPagesToMergeCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FSelectPagesToMergeCS);
	SHADER_USE_PARAMETER_STRUCT(FSelectPagesToMergeCS, FVirtualPageManagementShader)

	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS");
	using FPermutationDomain = TShaderPermutationDomain<FGenerateStatsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPhysicalPageMetaData>, PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutMergePagesIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutPhysicalPagesToMerge)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutStatsBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FSelectPagesToMergeCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "SelectPagesToMergeCS", SF_Compute);

class FMergeStaticPhysicalPagesIndirectCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FMergeStaticPhysicalPagesIndirectCS);
	SHADER_USE_PARAMETER_STRUCT(FMergeStaticPhysicalPagesIndirectCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PhysicalPagesToMerge)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutPhysicalPagePool)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FMergeStaticPhysicalPagesIndirectCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "MergeStaticPhysicalPagesIndirectCS", SF_Compute);



void FVirtualShadowMapArray::MergeStaticPhysicalPages(FRDGBuilder& GraphBuilder)
{
	check(IsEnabled());
	if (GetNumShadowMaps() == 0 || !ShouldCacheStaticSeparately())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (CVarDebugSkipMergePhysical.GetValueOnRenderThread() != 0)
	{
		return;
	}
#endif

	RDG_EVENT_SCOPE(GraphBuilder, "FVirtualShadowMapArray::MergeStaticPhysicalPages");
	if (CVarMergePhysicalUsingIndirect.GetValueOnRenderThread() != 0)
	{
		FRDGBufferRef MergePagesIndirectArgsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Shadow.Virtual.MergePagesIndirectArgs"));
		// Note: We use GetTotalAllocatedPhysicalPages() to size the buffer as the selection shader emits both static/dynamic pages separately when enabled.
		FRDGBufferRef PhysicalPagesToMergeRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), GetTotalAllocatedPhysicalPages() + 1), TEXT("Shadow.Virtual.PhysicalPagesToMerge"));

		// 1. Initialize the indirect args buffer
		AddClearIndirectDispatchArgs1DPass(GraphBuilder, Scene.GetFeatureLevel(), MergePagesIndirectArgsRDG);
		// 2. Filter the relevant physical pages and set up the indirect args
		{
			FSelectPagesToMergeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectPagesToMergeCS::FParameters>();
			PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
			PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
			PassParameters->OutMergePagesIndirectArgsBuffer = GraphBuilder.CreateUAV(MergePagesIndirectArgsRDG);
			PassParameters->OutPhysicalPagesToMerge = GraphBuilder.CreateUAV(PhysicalPagesToMergeRDG);

			FSelectPagesToMergeCS::FPermutationDomain PermutationVector;
			SetStatsArgsAndPermutation<FSelectPagesToMergeCS>(GraphBuilder, StatsBufferRDG, PassParameters, PermutationVector);

			auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FSelectPagesToMergeCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SelectPagesToMerge"),
				ComputeShader,
				PassParameters,
				FIntVector(FMath::DivideAndRoundUp(GetMaxPhysicalPages(), FSelectPagesToMergeCS::DefaultCSGroupX), 1, 1)
			);

		}
		// 3. Indirect dispatch to clear the selected pages
		{
			FMergeStaticPhysicalPagesIndirectCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMergeStaticPhysicalPagesIndirectCS::FParameters>();
			PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
			PassParameters->OutPhysicalPagePool = GraphBuilder.CreateUAV(PhysicalPagePoolRDG);
			PassParameters->IndirectArgs = MergePagesIndirectArgsRDG;
			PassParameters->PhysicalPagesToMerge = GraphBuilder.CreateSRV(PhysicalPagesToMergeRDG);
			auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FMergeStaticPhysicalPagesIndirectCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("MergeStaticPhysicalPagesIndirect"),
				ComputeShader,
				PassParameters,
				PassParameters->IndirectArgs,
				0
			);
		}
	}
	else
	{
		FMergeStaticPhysicalPagesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMergeStaticPhysicalPagesCS::FParameters>();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
		PassParameters->OutPhysicalPagePool = GraphBuilder.CreateUAV(PhysicalPagePoolRDG);		

		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FMergeStaticPhysicalPagesCS>();

		// Shader contains logic to deal with static cached pages if enabled
		// We only need to launch one per page, even if there are multiple cached pages per page
		FIntPoint PoolSize = GetPhysicalPoolSize();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("MergeStaticPhysicalPages"),
			ComputeShader,
			PassParameters,
			FIntVector(
				FMath::DivideAndRoundUp(PoolSize.X, 16),
				FMath::DivideAndRoundUp(PoolSize.Y, 16),
				1)
		);
	}
}

/**
 * Helper to get hold of / check for associated virtual shadow map
 */
FORCEINLINE FProjectedShadowInfo* GetVirtualShadowMapInfo(const FVisibleLightInfo &LightInfo)
{
	for (FProjectedShadowInfo *ProjectedShadowInfo : LightInfo.AllProjectedShadows)
	{
		if (ProjectedShadowInfo->HasVirtualShadowMap())
		{
			return ProjectedShadowInfo;
		}
	}

	return nullptr;
}


class FInitPageRectBoundsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FInitPageRectBoundsCS);
	SHADER_USE_PARAMETER_STRUCT(FInitPageRectBoundsCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntVector4>, OutPageRectBounds)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitPageRectBoundsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "InitPageRectBounds", SF_Compute);



class FVirtualSmFeedbackStatusCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmFeedbackStatusCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmFeedbackStatusCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, FreePhysicalPages)
		SHADER_PARAMETER_STRUCT_INCLUDE(GPUMessage::FParameters, GPUMessageParams)
		SHADER_PARAMETER(uint32, StatusMessageId)
	END_SHADER_PARAMETER_STRUCT()


	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVirtualPageManagementShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmFeedbackStatusCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "FeedbackStatusCS", SF_Compute);

void FVirtualShadowMapVisualizeLightSearch::CheckLight(const FLightSceneProxy* CheckProxy, int CheckVirtualShadowMapId)
{
#if !UE_BUILD_SHIPPING
	FString CheckLightName = CheckProxy->GetOwnerNameOrLabel();
	if (GDumpVSMLightNames)
	{
		UE_LOG(LogRenderer, Display, TEXT("%s"), *CheckLightName);
	}

	const ULightComponent* Component = CheckProxy->GetLightComponent();
	check(Component);

	// Fill out new sort key and compare to our best found so far
	SortKey CheckKey;
	CheckKey.Packed = 0;
	CheckKey.Fields.bExactNameMatch = (CheckLightName == GVirtualShadowMapVisualizeLightName);
	CheckKey.Fields.bPartialNameMatch = CheckKey.Fields.bExactNameMatch || CheckLightName.Contains(GVirtualShadowMapVisualizeLightName);
	CheckKey.Fields.bSelected = Component->IsSelected();
	CheckKey.Fields.bOwnerSelected = Component->IsOwnerSelected();
	CheckKey.Fields.bDirectionalLight = CheckProxy->GetLightType() == LightType_Directional;
	CheckKey.Fields.bExists = 1;

	if (CheckKey.Packed > FoundKey.Packed)		//-V547
	{
		FoundKey = CheckKey;
		FoundProxy = CheckProxy;
		FoundVirtualShadowMapId = CheckVirtualShadowMapId;
	}
#endif
}

static FRDGTextureRef CreateDebugVisualizationTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent)
{
	const FLinearColor ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
		Extent,
		PF_R8G8B8A8,
		FClearValueBinding(ClearColor),
		TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef Texture = GraphBuilder.CreateTexture(Desc, TEXT("Shadow.Virtual.DebugProjection"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Texture), ClearColor);
	return Texture;
}

static uint32 GetShadowMapsToAllocate(uint32 NumShadowMaps)
{
	// Round up to powers of two to be friendlier to the buffer pool
	return FMath::RoundUpToPowerOfTwo(FMath::Max(64U, NumShadowMaps));
}

void FVirtualShadowMapArray::BuildPageAllocations(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const TArray<FViewInfo>& Views,
	const FEngineShowFlags& EngineShowFlags,
	const FSortedLightSetSceneInfo& SortedLightsInfo,
	const TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const TArray<Nanite::FRasterResults, TInlineAllocator<2>>& NaniteRasterResults,
	FRDGTextureRef SingleLayerWaterDepthTexture)
{
	check(IsEnabled());

	if (GetNumShadowMaps() == 0 || Views.Num() == 0)
	{
		// Nothing to do
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "FVirtualShadowMapArray::BuildPageAllocation");

	{
		// Buffer for marking static invalidating instances for readback
		const uint32 StaticInvalidatingPrimitivesSize = FMath::Max(1, FMath::DivideAndRoundUp(Scene.GetMaxPersistentPrimitiveIndex(), 32));
		FRDGBufferDesc StaticInvalidatingPrimitivesDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), StaticInvalidatingPrimitivesSize);
		StaticInvalidatingPrimitivesDesc.Usage |= EBufferUsageFlags::SourceCopy;	// For copy to readback
		StaticInvalidatingPrimitivesRDG = GraphBuilder.CreateBuffer(StaticInvalidatingPrimitivesDesc, TEXT("Shadow.Virtual.StaticInvalidatingPrimitives"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(StaticInvalidatingPrimitivesRDG), 0);
	}

	bool bDebugOutputEnabled = false;
	VisualizeLight.Reset();
	VisualizeLight.AddDefaulted(Views.Num());

#if !UE_BUILD_SHIPPING
	if (GDumpVSMLightNames)
	{
		bDebugOutputEnabled = true;
		UE_LOG(LogRenderer, Display, TEXT("Lights with Virtual Shadow Maps:"));
	}

	// Setup debug visualization/output if enabled
	{
		FVirtualShadowMapVisualizationData& VisualizationData = GetVirtualShadowMapVisualizationData();
	
		for (const FViewInfo& View : Views)
		{
			const FName& VisualizationMode = View.CurrentVirtualShadowMapVisualizationMode;
			// for stereo views that aren't multi-view, don't account for the left
			FIntPoint Extent = View.ViewRect.Max - View.ViewRect.Min;
			if (VisualizationData.Update(VisualizationMode))
			{
				// TODO - automatically enable the show flag when set from command line?
				//EngineShowFlags.SetVisualizeVirtualShadowMap(true);
			}

			if (VisualizationData.IsActive() && EngineShowFlags.VisualizeVirtualShadowMap)
			{
				bDebugOutputEnabled = true;
				DebugVisualizationOutput.Add(CreateDebugVisualizationTexture(GraphBuilder, Extent));
			}
		}
	}
#endif //!UE_BUILD_SHIPPING
		
	// Store shadow map projection data for each virtual shadow map
	FRDGScatterUploadBuffer ProjectionDataUploader;
	ProjectionDataUploader.Init(GraphBuilder, GetNumShadowMaps(), sizeof(FVirtualShadowMapProjectionShaderData), false, TEXT("Shadow.Virtual.ProjectionData.UploadBuffer"));

	for (const FVisibleLightInfo& VisibleLightInfo : VisibleLightInfos)
	{
		for (const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap : VisibleLightInfo.VirtualShadowMapClipmaps)
		{
			// NOTE: Shader assumes all levels from a given clipmap are contiguous
			int32 ClipmapID = Clipmap->GetVirtualShadowMap()->ID;
			for (int32 ClipmapLevel = 0; ClipmapLevel < Clipmap->GetLevelCount(); ++ClipmapLevel)
			{
				*reinterpret_cast<FVirtualShadowMapProjectionShaderData*>(ProjectionDataUploader.Add_GetRef(ClipmapID + ClipmapLevel)) = Clipmap->GetProjectionShaderData(ClipmapLevel);
			}

			if (bDebugOutputEnabled)
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (Clipmap->GetDependentView() == Views[ViewIndex].GetPrimaryView())
					{
						VisualizeLight[ViewIndex].CheckLight(Clipmap->GetLightSceneInfo().Proxy, ClipmapID);
					}
				}
			}
		}

		const float ResolutionLodBiasLocal = GetResolutionLODBiasLocal();

		for (FProjectedShadowInfo* ProjectedShadowInfo : VisibleLightInfo.AllProjectedShadows)
		{
			if (ProjectedShadowInfo->HasVirtualShadowMap())
			{
				check(ProjectedShadowInfo->CascadeSettings.ShadowSplitIndex == INDEX_NONE);		// We use clipmaps for virtual shadow maps, not cascades

				// NOTE: Virtual shadow maps are never atlased, but verify our assumptions
				{
					const FVector4f ClipToShadowUV = ProjectedShadowInfo->GetClipToShadowBufferUvScaleBias();
					check(ProjectedShadowInfo->BorderSize == 0);
					check(ProjectedShadowInfo->X == 0);
					check(ProjectedShadowInfo->Y == 0);
					const FIntRect ShadowViewRect = ProjectedShadowInfo->GetInnerViewRect();
					check(ShadowViewRect.Min.X == 0);
					check(ShadowViewRect.Min.Y == 0);
					check(ShadowViewRect.Max.X == FVirtualShadowMap::VirtualMaxResolutionXY);
					check(ShadowViewRect.Max.Y == FVirtualShadowMap::VirtualMaxResolutionXY);
				}
				TSharedPtr<FVirtualShadowMapPerLightCacheEntry> CacheEntry = CacheManager->FindCreateLightCacheEntry(ProjectedShadowInfo->GetLightSceneInfo().Id);

				int32 NumMaps = ProjectedShadowInfo->bOnePassPointLightShadow ? 6 : 1;
				for(int32 Index = 0; Index < NumMaps; ++Index)
				{
					check(!CacheEntry.IsValid() || CacheEntry->bCurrentIsDistantLight == ProjectedShadowInfo->VirtualShadowMaps[Index]->bIsSinglePageSM);
					
					uint32 Flags = ProjectedShadowInfo->VirtualShadowMaps[Index]->bIsSinglePageSM ? VSM_PROJ_FLAG_CURRENT_DISTANT_LIGHT : 0U;
					int32 ID = ProjectedShadowInfo->VirtualShadowMaps[Index]->ID;

					const FViewMatrices ViewMatrices = ProjectedShadowInfo->GetShadowDepthRenderingViewMatrices(Index, true );
					const FLargeWorldRenderPosition PreViewTranslation(ProjectedShadowInfo->PreShadowTranslation);

					FVirtualShadowMapProjectionShaderData Data; 
					Data.TranslatedWorldToShadowViewMatrix		= FMatrix44f(ViewMatrices.GetTranslatedViewMatrix());	// LWC_TODO: Precision loss?
					Data.ShadowViewToClipMatrix					= FMatrix44f(ViewMatrices.GetProjectionMatrix());
					Data.TranslatedWorldToShadowUVMatrix		= FMatrix44f(CalcTranslatedWorldToShadowUVMatrix( ViewMatrices.GetTranslatedViewMatrix(), ViewMatrices.GetProjectionMatrix() ));
					Data.TranslatedWorldToShadowUVNormalMatrix	= FMatrix44f(CalcTranslatedWorldToShadowUVNormalMatrix( ViewMatrices.GetTranslatedViewMatrix(), ViewMatrices.GetProjectionMatrix() ));
					Data.PreViewTranslationLWCTile				= PreViewTranslation.GetTile();
					Data.PreViewTranslationLWCOffset			= PreViewTranslation.GetOffset();
					Data.LightType								= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetLightType();
					Data.LightSourceRadius						= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetSourceRadius();
					Data.ResolutionLodBias						= ResolutionLodBiasLocal;
					Data.LightRadius							= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetRadius();
					Data.Flags									= Flags;
					ProjectionDataUploader.Add(ID, &Data);
				}

				if (bDebugOutputEnabled)
				{
					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						VisualizeLight[ViewIndex].CheckLight(ProjectedShadowInfo->GetLightSceneInfo().Proxy, ProjectedShadowInfo->VirtualShadowMaps[0]->ID);
					}
				}
			}
		}
	}

	{
		// Create large enough to hold all the unused elements too (wastes GPU memory but allows direct indexing via the ID)
		uint32 DataSize = sizeof(FVirtualShadowMapProjectionShaderData) * uint32(ShadowMaps.Num());

		FRDGBufferDesc Desc;
		Desc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::ByteAddressBuffer | EBufferUsageFlags::StructuredBuffer;
		Desc.BytesPerElement = 4;
		Desc.NumElements = DataSize / 4U;

		ProjectionDataRDG = GraphBuilder.CreateBuffer(Desc, TEXT("Shadow.Virtual.ProjectionData"));
	}
	ProjectionDataUploader.ResourceUploadTo(GraphBuilder, ProjectionDataRDG);

	UniformParameters.NumFullShadowMaps = GetNumFullShadowMaps();
	UniformParameters.NumSinglePageShadowMaps = GetNumSinglePageShadowMaps();
	UniformParameters.NumShadowMapSlots = ShadowMaps.Num();
	UniformParameters.ProjectionData = GraphBuilder.CreateSRV(ProjectionDataRDG);

	UniformParameters.bExcludeNonNaniteFromCoarsePages = !CVarCoarsePagesIncludeNonNanite.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdDynamic = CVarCoarsePagePixelThresholdDynamic.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdStatic = CVarCoarsePagePixelThresholdStatic.GetValueOnRenderThread();
	UniformParameters.CoarsePagePixelThresholdDynamicNanite = CVarCoarsePagePixelThresholdDynamicNanite.GetValueOnRenderThread();

	if (CVarShowStats.GetValueOnRenderThread() || CacheManager->IsAccumulatingStats())
	{
		StatsBufferRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumStats), TEXT("Shadow.Virtual.StatsBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(StatsBufferRDG), 0);
	}
	
	// We potentially over-allocate these to avoid too many different allocation sizes each frame
	const uint32 NumShadowMapsToAllocate = GetShadowMapsToAllocate(GetNumShadowMaps());
	const int32 NumPageFlagsToAllocate = FMath::RoundUpToPowerOfTwo(FMath::Max(128 * 1024, GetNumFullShadowMaps() * int32(FVirtualShadowMap::PageTableSize) + int32(VSM_MAX_SINGLE_PAGE_SHADOW_MAPS)));

	// Create and clear the requested page flags	
	FRDGBufferRef PageRequestFlagsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumPageFlagsToAllocate), TEXT("Shadow.Virtual.PageRequestFlags"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PageRequestFlagsRDG), 0);

	DirtyPageFlagsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GetMaxPhysicalPages() * 3), TEXT("Shadow.Virtual.DirtyPageFlags"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DirtyPageFlagsRDG), 0);

	const uint32 NumPageRects = ShadowMaps.Num() * FVirtualShadowMap::MaxMipLevels;
	const uint32 NumPageRectsToAllocate = FMath::RoundUpToPowerOfTwo(NumPageRects);
	PageRectBoundsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FIntVector4), NumPageRectsToAllocate), TEXT("Shadow.Virtual.PageRectBounds"));
	{
		FInitPageRectBoundsCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FInitPageRectBoundsCS::FParameters >();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->OutPageRectBounds = GraphBuilder.CreateUAV(PageRectBoundsRDG);

		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FInitPageRectBoundsCS>();
		ClearUnusedGraphResources(ComputeShader, PassParameters);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitPageRectBounds"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(NumPageRects, FInitPageRectBoundsCS::DefaultCSGroupX), 1, 1)
		);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		const FViewInfo &View = Views[ViewIndex];

		// Gather directional light virtual shadow maps
		TArray<int32, SceneRenderingAllocator> DirectionalLightIds;
		for (const FVisibleLightInfo& VisibleLightInfo : VisibleLightInfos)
		{
			for (const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap : VisibleLightInfo.VirtualShadowMapClipmaps)
			{
				// mark pages from both views in stereo for the left (primary) view clipmap - assumes GetPrimaryView returns self for non-stereo views.
				if (Clipmap->GetDependentView() == Views[ViewIndex].GetPrimaryView())
				{
					// NOTE: Shader assumes all levels from a given clipmap are contiguous
					int32 ClipmapID = Clipmap->GetVirtualShadowMap()->ID;
					DirectionalLightIds.Add(ClipmapID);
				}
			}
		}	
		
		// This view contained no local lights (that were stored in the light grid), and no directional lights, so nothing to do.
		if (View.ForwardLightingResources.LocalLightVisibleLightInfosIndex.Num() + DirectionalLightIds.Num() == 0)
		{
			continue;
		}

		FRDGBufferRef DirectionalLightIdsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.DirectionalLightIds"), DirectionalLightIds);

		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

		FRDGBufferRef ScreenSpaceGridBoundsRDG = nullptr;
			
		{
			// Mark coarse pages
			// NOTE: Must do this *first*. In the case where bIncludeNonNaniteGeometry is false we need to ensure that the request
			// can be over-written by any pixel pages that *do* want Non-Nanite geometry. We avoid writing with atomics since that
			// is much slower.
			// Because of this we also cannot overlap this pass with the following ones.
			bool bMarkCoarsePagesDirectional = CVarMarkCoarsePagesDirectional.GetValueOnRenderThread() != 0;
			bool bMarkCoarsePagesLocal = CVarMarkCoarsePagesLocal.GetValueOnRenderThread() != 0;
			// Note: always run this pass such that the distant lights may be marked if need be
			{
				FMarkCoarsePagesCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FMarkCoarsePagesCS::FParameters >();
				PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
				PassParameters->OutPageRequestFlags = GraphBuilder.CreateUAV(PageRequestFlagsRDG);
				PassParameters->bMarkCoarsePagesLocal = bMarkCoarsePagesLocal ? 1 : 0;
				PassParameters->ClipmapIndexMask = bMarkCoarsePagesDirectional ? FVirtualShadowMapClipmap::GetCoarsePageClipmapIndexMask() : 0;
				PassParameters->bIncludeNonNaniteGeometry = CVarCoarsePagesIncludeNonNanite.GetValueOnRenderThread();

				auto ComputeShader = View.ShaderMap->GetShader<FMarkCoarsePagesCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("MarkCoarsePages"),
					ComputeShader,
					PassParameters,
					FIntVector(FMath::DivideAndRoundUp(uint32(GetNumShadowMaps()), FMarkCoarsePagesCS::DefaultCSGroupX), 1, 1)
				);
			}

			// Mark pages based on projected depth buffer pixels
			if (CVarMarkPixelPages.GetValueOnRenderThread() != 0)
			{
				// It's currently safe to overlap these passes that all write to same page request flags
				FRDGBufferUAVRef PageRequestFlagsUAV = GraphBuilder.CreateUAV(PageRequestFlagsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);

				uint32 LightGridIndexBufferSize = View.ForwardLightingResources.ForwardLightData->CulledLightDataGrid->Desc.Buffer->Desc.GetSize();
				FRDGBufferRef PrunedLightGridDataRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), LightGridIndexBufferSize), TEXT("Shadow.Virtual.PrunedLightGridData"));
				
				const uint32 NumLightGridCells = View.ForwardLightingResources.ForwardLightData->NumGridCells;
				FRDGBufferRef PrunedNumCulledLightsGridRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumLightGridCells), TEXT("Shadow.Virtual.PrunedNumCulledLightsGrid"));

				{
					FPruneLightGridCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FPruneLightGridCS::FParameters >();
					PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
					PassParameters->OutPrunedLightGridData = GraphBuilder.CreateUAV(PrunedLightGridDataRDG);
					PassParameters->OutPrunedNumCulledLightsGrid = GraphBuilder.CreateUAV(PrunedNumCulledLightsGridRDG);
					auto ComputeShader = View.ShaderMap->GetShader<FPruneLightGridCS>();

					FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("PruneLightGrid"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCount(NumLightGridCells, FPruneLightGridCS::DefaultCSGroupX));
				};


				auto GeneratePageFlags = [&](const EVirtualShadowMapProjectionInputType InputType)
				{
					FGeneratePageFlagsFromPixelsCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FGeneratePageFlagsFromPixelsCS::FInputType>(static_cast<uint32>(InputType));
					FGeneratePageFlagsFromPixelsCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FGeneratePageFlagsFromPixelsCS::FParameters >();
					PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);

					PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;

					PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
					PassParameters->View = View.ViewUniformBuffer;
					PassParameters->OutPageRequestFlags = PageRequestFlagsUAV;
					PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
					PassParameters->DirectionalLightIds = GraphBuilder.CreateSRV(DirectionalLightIdsRDG);
					PassParameters->PrunedLightGridData = GraphBuilder.CreateSRV(PrunedLightGridDataRDG);
					PassParameters->PrunedNumCulledLightsGrid = GraphBuilder.CreateSRV(PrunedNumCulledLightsGridRDG);
					PassParameters->SingeLayerWaterDepthTexture = SingleLayerWaterDepthTexture;
					PassParameters->NumDirectionalLightSmInds = uint32(DirectionalLightIds.Num());
					PassParameters->PageDilationBorderSizeLocal = CVarPageDilationBorderSizeLocal.GetValueOnRenderThread();
					PassParameters->PageDilationBorderSizeDirectional = CVarPageDilationBorderSizeDirectional.GetValueOnRenderThread();
					PassParameters->bCullBackfacingPixels = ShouldCullBackfacingPixels() ? 1 : 0;
					PassParameters->Strata = Strata::BindStrataGlobalUniformParameters(View);

					auto ComputeShader = View.ShaderMap->GetShader<FGeneratePageFlagsFromPixelsCS>(PermutationVector);

					static_assert((FVirtualPageManagementShader::DefaultCSGroupXY % 2) == 0, "GeneratePageFlagsFromPixels requires even-sized CS groups for quad swizzling.");
					const FIntPoint GridSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), FVirtualPageManagementShader::DefaultCSGroupXY);

					if (InputType == EVirtualShadowMapProjectionInputType::HairStrands)
					{
						check(View.HairStrandsViewData.VisibilityData.TileData.IsValid());
						PassParameters->IndirectBufferArgs = View.HairStrandsViewData.VisibilityData.TileData.TileIndirectDispatchBuffer;
						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("GeneratePageFlagsFromPixels(HairStrands,Tile)"),
							ComputeShader,
							PassParameters,
							View.HairStrandsViewData.VisibilityData.TileData.TileIndirectDispatchBuffer,
							View.HairStrandsViewData.VisibilityData.TileData.GetIndirectDispatchArgOffset(FHairStrandsTiles::ETileType::HairAll));
					}
					else
					{
						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("GeneratePageFlagsFromPixels(%s,NumShadowMaps=%d)", ToString(InputType), GetNumFullShadowMaps()),
							ComputeShader,
							PassParameters,
							FIntVector(GridSize.X, GridSize.Y, 1));
					}
				};

				const bool bHasValidSingleLayerWaterDepth = SingleLayerWaterDepthTexture != nullptr;
				GeneratePageFlags(bHasValidSingleLayerWaterDepth ? EVirtualShadowMapProjectionInputType::GBufferAndSingleLayerWaterDepth : EVirtualShadowMapProjectionInputType::GBuffer);
				if (HairStrands::HasViewHairStrandsData(View))
				{
					GeneratePageFlags(EVirtualShadowMapProjectionInputType::HairStrands);
				}
			}
		}
	}

	PageTableRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumPageFlagsToAllocate), TEXT("Shadow.Virtual.PageTable"));		
	// Note: these are passed to the rendering and are not identical to the PageRequest flags coming in from GeneratePageFlagsFromPixels 
	PageFlagsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumPageFlagsToAllocate), TEXT("Shadow.Virtual.PageFlags"));

	// One additional element as the last element is used as an atomic counter
	FRDGBufferRef FreePhysicalPagesRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), GetMaxPhysicalPages() + 1), TEXT("Shadow.Virtual.FreePhysicalPages"));
	FRDGBufferRef PhysicalPageAllocationRequestsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FPhysicalPageRequest), GetMaxPhysicalPages() + 1), TEXT("Shadow.Virtual.PhysicalPageAllocationRequests"));

	// Enough space for all physical pages that might be allocated
	PhysicalPageMetaDataRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FPhysicalPageMetaData), GetMaxPhysicalPages()), TEXT("Shadow.Virtual.PhysicalPageMetaData"));

	AllocatedPageRectBoundsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FIntVector4), NumPageRects), TEXT("Shadow.Virtual.AllocatedPageRectBounds"));

	{
		FInitPhysicalPageMetaData::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitPhysicalPageMetaData::FParameters>();
		PassParameters->VirtualShadowMap		= GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->OutPhysicalPageMetaData = GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG);
		PassParameters->OutFreePhysicalPages	= GraphBuilder.CreateUAV(FreePhysicalPagesRDG);
		PassParameters->OutPhysicalPageAllocationRequests = GraphBuilder.CreateUAV(PhysicalPageAllocationRequestsRDG);

		auto ComputeShader = Views[0].ShaderMap->GetShader<FInitPhysicalPageMetaData>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitPhysicalPageMetaData"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(GetMaxPhysicalPages(), FInitPhysicalPageMetaData::DefaultCSGroupX), 1, 1)
		);
	}
		
	// Start by marking any physical pages that we are going to keep due to caching
	// NOTE: We run this pass even with no caching since we still need to initialize the metadata
	{
		FCreateCachedPageMappingsCS::FParameters* PassParameters = GraphBuilder.AllocParameters< FCreateCachedPageMappingsCS::FParameters >();
		PassParameters->VirtualShadowMap		 = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->PageRequestFlags		 = GraphBuilder.CreateSRV(PageRequestFlagsRDG);
		PassParameters->OutPageTable			 = GraphBuilder.CreateUAV(PageTableRDG);
		PassParameters->OutPhysicalPageMetaData  = GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG);
		PassParameters->OutPhysicalPageAllocationRequests = GraphBuilder.CreateUAV(PhysicalPageAllocationRequestsRDG);
		PassParameters->OutPageFlags			 = GraphBuilder.CreateUAV(PageFlagsRDG);
		PassParameters->bDynamicPageInvalidation = 1;
#if !UE_BUILD_SHIPPING
		PassParameters->bDynamicPageInvalidation = CVarDebugSkipDynamicPageInvalidation.GetValueOnRenderThread() == 0 ? 1 : 0;
#endif

		bool bCacheEnabled = CacheManager->IsValid();
		if (bCacheEnabled)
		{
			SetCacheDataShaderParameters(GraphBuilder, ShadowMaps, CacheManager, PassParameters->CacheDataParameters);
		}

		FCreateCachedPageMappingsCS::FPermutationDomain PermutationVector;
		SetStatsArgsAndPermutation<FCreateCachedPageMappingsCS>(GraphBuilder, StatsBufferRDG, PassParameters, PermutationVector);
		PermutationVector.Set<FCreateCachedPageMappingsCS::FHasCacheDataDim>(bCacheEnabled);
		auto ComputeShader = Views[0].ShaderMap->GetShader<FCreateCachedPageMappingsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CreateCachedPageMappings"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(FVirtualShadowMap::PageTableSize, FCreateCachedPageMappingsCS::DefaultCSGroupX), GetNumShadowMaps(), 1)
		);
	}

	// After we've marked any cached pages, collect all the remaining free pages into a list
	// NOTE: We could optimize this more in the case where there's no caching of course; TBD priority
	{
		FPackFreePagesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPackFreePagesCS::FParameters>();
		PassParameters->VirtualShadowMap		= GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->PhysicalPageMetaData	= GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
		PassParameters->OutFreePhysicalPages	= GraphBuilder.CreateUAV(FreePhysicalPagesRDG);
			
		auto ComputeShader = Views[0].ShaderMap->GetShader<FPackFreePagesCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PackFreePages"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(GetMaxPhysicalPages(), FPackFreePagesCS::DefaultCSGroupX), 1, 1)
		);
	}

	// Allocate any new physical pages that were not cached from the free list
	{
		FAllocateNewPageMappingsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAllocateNewPageMappingsCS::FParameters>();
		PassParameters->VirtualShadowMap		= GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->PageRequestFlags		= GraphBuilder.CreateSRV(PageRequestFlagsRDG);
		PassParameters->PhysicalPageAllocationRequests = GraphBuilder.CreateSRV(PhysicalPageAllocationRequestsRDG);
		PassParameters->OutPageTable			= GraphBuilder.CreateUAV(PageTableRDG);
		PassParameters->OutPageFlags			= GraphBuilder.CreateUAV(PageFlagsRDG);
		PassParameters->OutFreePhysicalPages	= GraphBuilder.CreateUAV(FreePhysicalPagesRDG);
		PassParameters->OutPhysicalPageMetaData = GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG);
			
		FAllocateNewPageMappingsCS::FPermutationDomain PermutationVector;
		SetStatsArgsAndPermutation<FAllocateNewPageMappingsCS>(GraphBuilder, StatsBufferRDG, PassParameters, PermutationVector);

		auto ComputeShader = Views[0].ShaderMap->GetShader<FAllocateNewPageMappingsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("AllocateNewPageMappings"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(GetMaxPhysicalPages(), FAllocateNewPageMappingsCS::DefaultCSGroupX)
		);
	}

	{
		// Run pass building hierarchical page flags to make culling acceptable performance.
		FGenerateHierarchicalPageFlagsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateHierarchicalPageFlagsCS::FParameters>();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
		PassParameters->OutPageFlags = GraphBuilder.CreateUAV(PageFlagsRDG);
		PassParameters->OutPageRectBounds = GraphBuilder.CreateUAV(PageRectBoundsRDG);

		auto ComputeShader = Views[0].ShaderMap->GetShader<FGenerateHierarchicalPageFlagsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateHierarchicalPageFlags"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(GetMaxPhysicalPages(), FGenerateHierarchicalPageFlagsCS::DefaultCSGroupX)
		);
	}

	// NOTE: We could skip this (in shader) for shadow maps that only have 1 mip (ex. clipmaps)
	if (GetNumFullShadowMaps() > 0)
	{
		// Propagate mapped mips down the hierarchy to allow O(1) lookup of coarser mapped pages
		FPropagateMappedMipsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPropagateMappedMipsCS::FParameters>();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->OutPageTable	= GraphBuilder.CreateUAV(PageTableRDG);

		auto ComputeShader = Views[0].ShaderMap->GetShader<FPropagateMappedMipsCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PropagateMappedMips"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(FMath::Square(FVirtualShadowMap::Level0DimPagesXY), FPropagateMappedMipsCS::DefaultCSGroupX), GetNumFullShadowMaps(), 1)
		);
	}

	// Initialize the physical page pool
	check(PhysicalPagePoolRDG != nullptr);
	{
		RDG_EVENT_SCOPE( GraphBuilder, "InitializePhysicalPages" );
		if (CVarInitializePhysicalUsingIndirect.GetValueOnRenderThread() != 0)
		{
			FRDGBufferRef InitializePagesIndirectArgsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(3), TEXT("Shadow.Virtual.InitializePagesIndirectArgs"));
			// Note: We use GetTotalAllocatedPhysicalPages() to size the buffer as the selection shader emits both static/dynamic pages separately when enabled.
			FRDGBufferRef PhysicalPagesToInitializeRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), GetTotalAllocatedPhysicalPages() + 1), TEXT("Shadow.Virtual.PhysicalPagesToInitialize"));

			// 1. Initialize the indirect args buffer
			AddClearIndirectDispatchArgs1DPass(GraphBuilder, Scene.GetFeatureLevel(), InitializePagesIndirectArgsRDG);
			// 2. Filter the relevant physical pages and set up the indirect args
			{
				FSelectPagesToInitializeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectPagesToInitializeCS::FParameters>();
				PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
				PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
				PassParameters->OutInitializePagesIndirectArgsBuffer = GraphBuilder.CreateUAV(InitializePagesIndirectArgsRDG);
				PassParameters->OutPhysicalPagesToInitialize = GraphBuilder.CreateUAV(PhysicalPagesToInitializeRDG);
				bool bGenerateStats = StatsBufferRDG != nullptr;
				if (bGenerateStats)
				{
					PassParameters->OutStatsBuffer = GraphBuilder.CreateUAV(StatsBufferRDG);
				}
				FSelectPagesToInitializeCS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FSelectPagesToInitializeCS::FGenerateStatsDim>(bGenerateStats);

				auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FSelectPagesToInitializeCS>(PermutationVector);

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SelectPagesToInitialize"),
					ComputeShader,
					PassParameters,
					FIntVector(FMath::DivideAndRoundUp(GetMaxPhysicalPages(), FSelectPagesToInitializeCS::DefaultCSGroupX), 1, 1)
				);

			}
			// 3. Indirect dispatch to clear the selected pages
			{
				FInitializePhysicalPagesIndirectCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitializePhysicalPagesIndirectCS::FParameters>();
				PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
				PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
				PassParameters->OutPhysicalPagePool = GraphBuilder.CreateUAV(PhysicalPagePoolRDG);
				PassParameters->IndirectArgs = InitializePagesIndirectArgsRDG;
				PassParameters->PhysicalPagesToInitialize = GraphBuilder.CreateSRV(PhysicalPagesToInitializeRDG);
				auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FInitializePhysicalPagesIndirectCS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("InitializePhysicalMemoryIndirect"),
					ComputeShader,
					PassParameters,
					PassParameters->IndirectArgs,
					0
				);
			}
		}
		else
		{
			FInitializePhysicalPagesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitializePhysicalPagesCS::FParameters>();
			PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
			PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);
			PassParameters->OutPhysicalPagePool = GraphBuilder.CreateUAV(PhysicalPagePoolRDG);

			auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FInitializePhysicalPagesCS>();

			// Shader contains logic to deal with static cached pages if enabled
			// We only need to launch one per page, even if there are multiple cached pages per page
			FIntPoint PoolSize = GetPhysicalPoolSize();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("InitializePhysicalPages"),
				ComputeShader,
				PassParameters,
				FIntVector(
					FMath::DivideAndRoundUp(PoolSize.X, 16),
					FMath::DivideAndRoundUp(PoolSize.Y, 16),
					1)
			);
		}
	}

	UniformParameters.PageTable = GraphBuilder.CreateSRV(PageTableRDG);
	UniformParameters.PageFlags = GraphBuilder.CreateSRV(PageFlagsRDG);
	UniformParameters.PageRectBounds = GraphBuilder.CreateSRV(PageRectBoundsRDG);

	// Add pass to pipe back important stats
	{

		FVirtualSmFeedbackStatusCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmFeedbackStatusCS::FParameters>();
		PassParameters->FreePhysicalPages = GraphBuilder.CreateSRV(FreePhysicalPagesRDG);
		PassParameters->GPUMessageParams = GPUMessage::GetShaderParameters(GraphBuilder);
		PassParameters->StatusMessageId = CacheManager->StatusFeedbackSocket.GetMessageId().GetIndex();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);

		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FVirtualSmFeedbackStatusCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Feedback Status"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1)
		);
	}

	UpdateCachedUniformBuffer(GraphBuilder);

#if !UE_BUILD_SHIPPING
	// Only dump one frame of light data
	GDumpVSMLightNames = false;
#endif
}

class FDebugVisualizeVirtualSmCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FDebugVisualizeVirtualSmCS);
	SHADER_USE_PARAMETER_STRUCT(FDebugVisualizeVirtualSmCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, ProjectionParameters)
		SHADER_PARAMETER(uint32, DebugTargetWidth)
		SHADER_PARAMETER(uint32, DebugTargetHeight)
		SHADER_PARAMETER(uint32, BorderWidth)
		SHADER_PARAMETER(uint32, VisualizeModeId)
		SHADER_PARAMETER(int32, VirtualShadowMapId)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutVisualize)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FDebugVisualizeVirtualSmCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapDebug.usf", "DebugVisualizeVirtualSmCS", SF_Compute);


void FVirtualShadowMapArray::RenderDebugInfo(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views)
{
	check(IsEnabled());
			
	if (DebugVisualizationOutput.IsEmpty() || VisualizeLight.IsEmpty())
	{
		return;
	}

	const FVirtualShadowMapVisualizationData& VisualizationData = GetVirtualShadowMapVisualizationData();
	if (VisualizationData.GetActiveModeID() != VIRTUAL_SHADOW_MAP_VISUALIZE_CLIPMAP_VIRTUAL_SPACE)
	{
		return;
	}

	int32 BorderWidth = 2;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!VisualizeLight[ViewIndex].IsValid())
		{
			continue;
		}

		FViewInfo& View = Views[ViewIndex];

		FIntPoint DebugTargetExtent = DebugVisualizationOutput[ViewIndex]->Desc.Extent;

		FDebugVisualizeVirtualSmCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDebugVisualizeVirtualSmCS::FParameters>();
		PassParameters->ProjectionParameters = GetSamplingParameters(GraphBuilder);

		PassParameters->DebugTargetWidth = DebugTargetExtent.X;
		PassParameters->DebugTargetHeight = DebugTargetExtent.Y;
		PassParameters->BorderWidth = BorderWidth;
		PassParameters->VisualizeModeId = VisualizationData.GetActiveModeID();
		PassParameters->VirtualShadowMapId = VisualizeLight[ViewIndex].GetVirtualShadowMapId();

		PassParameters->OutVisualize = GraphBuilder.CreateUAV(DebugVisualizationOutput[ViewIndex]);

		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FDebugVisualizeVirtualSmCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DebugVisualizeVirtualShadowMap"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DebugTargetExtent, FVirtualPageManagementShader::DefaultCSGroupXY)
		);
	}
}


class FVirtualSmPrintStatsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmPrintStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmPrintStatsCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_STRUCT_INCLUDE( ShaderPrint::FShaderParameters, ShaderPrintStruct )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, InStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FIntVector4>, AllocatedPageRectBounds)
		SHADER_PARAMETER(int, ShowStatsValue)
	END_SHADER_PARAMETER_STRUCT()


	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVirtualPageManagementShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// Disable optimizations as shader print causes long compile times
		OutEnvironment.CompilerFlags.Add(CFLAG_SkipOptimizations);
	}

		
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmPrintStatsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPrintStats.usf", "PrintStats", SF_Compute);

void FVirtualShadowMapArray::PrintStats(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	check(IsEnabled());
	LLM_SCOPE_BYTAG(Nanite);

	// Print stats
	int ShowStatsValue = CVarShowStats.GetValueOnRenderThread();
	if (ShowStatsValue != 0 && StatsBufferRDG)
	{
		{
			FVirtualSmPrintStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmPrintStatsCS::FParameters>();

			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintStruct);
			PassParameters->InStatsBuffer = GraphBuilder.CreateSRV(StatsBufferRDG);
			PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
			PassParameters->ShowStatsValue = ShowStatsValue;

			auto ComputeShader = View.ShaderMap->GetShader<FVirtualSmPrintStatsCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Print Stats"),
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1)
			);
		}
	}
}

void FVirtualShadowMapArray::CreateMipViews( TArray<Nanite::FPackedView, SceneRenderingAllocator>& Views ) const
{
	// strategy: 
	// 1. Use the cull pass to generate copies of every node for every view needed.
	// [2. Fabricate a HZB array?]
	ensure(Views.Num() <= ShadowMaps.Num());
	
	const int32 NumPrimaryViews = Views.Num();

	// 1. create derivative views for each of the Mip levels, 
	Views.AddDefaulted( NumPrimaryViews * ( FVirtualShadowMap::MaxMipLevels - 1) );

	int32 MaxMips = 0;
	for (int32 ViewIndex = 0; ViewIndex < NumPrimaryViews; ++ViewIndex)
	{
		const Nanite::FPackedView& PrimaryView = Views[ViewIndex];
		
		ensure( PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.X >= 0 && PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.X < ShadowMaps.Num() );
		ensure( PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Y == 0 );
		ensure( PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z > 0 && PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z <= FVirtualShadowMap::MaxMipLevels );
		
		const int32 NumMips = PrimaryView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z;
		MaxMips = FMath::Max(MaxMips, NumMips);
		for (int32 MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			Nanite::FPackedView& MipView = Views[ MipLevel * NumPrimaryViews + ViewIndex ];	// Primary (Non-Mip views) first followed by derived mip views.

			if( MipLevel > 0 )
			{
				MipView = PrimaryView;

				// Slightly messy, but extract any scale factor that was applied to the LOD scale for re-application below
				MipView.UpdateLODScales();
				float LODScaleFactor = PrimaryView.LODScales.X / MipView.LODScales.X;

				MipView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Y = MipLevel;
				MipView.TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ.Z = NumMips;	//FVirtualShadowMap::MaxMipLevels;

				// Size of view, for the virtual SMs these are assumed to not be offset.
				FIntPoint ViewSize = FIntPoint::DivideAndRoundUp( FIntPoint( PrimaryView.ViewSizeAndInvSize.X + 0.5f, PrimaryView.ViewSizeAndInvSize.Y + 0.5f ), 1U <<  MipLevel );
				FIntPoint ViewMin = FIntPoint(MipView.ViewRect.X, MipView.ViewRect.Y) / (1U <<  MipLevel);

				MipView.ViewSizeAndInvSize = FVector4f(ViewSize.X, ViewSize.Y, 1.0f / float(ViewSize.X), 1.0f / float(ViewSize.Y));
				MipView.ViewRect = FIntVector4(ViewMin.X, ViewMin.Y, ViewMin.X + ViewSize.X, ViewMin.Y + ViewSize.Y);

				MipView.UpdateLODScales();
				MipView.LODScales.X *= LODScaleFactor;

				MipView.TranslatedWorldToSubpixelClip = Nanite::FPackedView::CalcTranslatedWorldToSubpixelClip(MipView.TranslatedWorldToClip, FIntRect(ViewMin.X, ViewMin.Y, ViewMin.X + ViewSize.X, ViewMin.Y + ViewSize.Y));
			}

			MipView.HZBTestViewRect = MipView.ViewRect;	// Assumed to always be the same for VSM

			float RcpExtXY = 1.0f / ( FVirtualShadowMap::PageSize * FVirtualShadowMap::RasterWindowPages );

			// Transform clip from virtual address space to viewport.
			MipView.ClipSpaceScaleOffset = FVector4f(
				  MipView.ViewSizeAndInvSize.X * RcpExtXY,
				  MipView.ViewSizeAndInvSize.Y * RcpExtXY,
				 (MipView.ViewSizeAndInvSize.X + 2.0f * MipView.ViewRect.X) * RcpExtXY - 1.0f,
				-(MipView.ViewSizeAndInvSize.Y + 2.0f * MipView.ViewRect.Y) * RcpExtXY + 1.0f);


			// Set streaming priority category to zero for some reason
			MipView.StreamingPriorityCategory_AndFlags &= ~uint32(NANITE_STREAMING_PRIORITY_CATEGORY_MASK);
		}
	}

	// Remove unused mip views
	check(Views.IsEmpty() || MaxMips > 0);
	Views.SetNum(MaxMips * NumPrimaryViews, false);
}


class FVirtualSmPrintClipmapStatsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmPrintClipmapStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmPrintClipmapStatsCS, FVirtualPageManagementShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintStruct)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FIntVector4 >, PageRectBounds)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FIntVector4 >, AllocatedPageRectBounds)
		SHADER_PARAMETER(uint32, ShadowMapIdRangeStart)
		SHADER_PARAMETER(uint32, ShadowMapIdRangeEnd)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmPrintClipmapStatsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPrintStats.usf", "PrintClipmapStats", SF_Compute);


BEGIN_SHADER_PARAMETER_STRUCT(FVirtualShadowDepthPassParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FShadowDepthPassUniformParameters, ShadowDepthPass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPackedView >, InViews)

	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


struct FVSMCullingBatchInfo
{
	FVector3f CullingViewOriginOffset;
	uint32 FirstPrimaryView;
	FVector3f CullingViewOriginTile;
	uint32 NumPrimaryViews;
	uint32 PrimitiveRevealedOffset;
	uint32 PrimitiveRevealedNum;
	uint32 padding[2]; // avoid error: cannot instantiate StructuredBuffer with given packed alignment; 'VK_EXT_scalar_block_layout' not supported
};


struct FVisibleInstanceCmd
{
	uint32 PackedPageInfo;
	uint32 InstanceId;
	uint32 DrawCommandId;
};

class FCullPerPageDrawCommandsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCullPerPageDrawCommandsCs);
	SHADER_USE_PARAMETER_STRUCT(FCullPerPageDrawCommandsCs, FGlobalShader)

	class FUseHzbDim : SHADER_PERMUTATION_BOOL("USE_HZB_OCCLUSION");
	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS");
	class FBatchedDim : SHADER_PERMUTATION_BOOL("ENABLE_BATCH_MODE");
	using FPermutationDomain = TShaderPermutationDomain< FUseHzbDim, FBatchedDim, FGenerateStatsDim >;

	static constexpr uint32 ThreadGroupSize = FInstanceProcessingGPULoadBalancer::ThreadGroupSize;

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	/**
	 */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FInstanceProcessingGPULoadBalancer::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
		OutEnvironment.SetDefine(TEXT("INDIRECT_ARGS_NUM_WORDS"), FInstanceCullingContext::IndirectArgsNumWords);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
	}


	BEGIN_SHADER_PARAMETER_STRUCT(FHZBShaderParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, HZBPageTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, HZBPageFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint4 >, HZBPageRectBounds)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(uint32, HZBMode)
	END_SHADER_PARAMETER_STRUCT()



	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER(uint32, GPUSceneFrameNumber)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PrimitiveRevealedMask)
		SHADER_PARAMETER(uint32, PrimitiveRevealedNum)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutDirtyPageFlags)

		SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceProcessingGPULoadBalancer::FShaderParameters, LoadBalancerParameters)

		SHADER_PARAMETER(int32, FirstPrimaryView)
		SHADER_PARAMETER(int32, NumPrimaryViews)
		SHADER_PARAMETER(uint32, TotalPrimaryViews)
		SHADER_PARAMETER(uint32, VisibleInstancesBufferNum)
		SHADER_PARAMETER(int32, DynamicInstanceIdOffset)
		SHADER_PARAMETER(int32, DynamicInstanceIdMax)
		SHADER_PARAMETER(float, MaxMaterialPositionInvalidationRange)
		SHADER_PARAMETER(FVector3f, CullingViewOriginOffset)
		SHADER_PARAMETER(FVector3f, CullingViewOriginTile)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FPackedView >, InViews)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint32 >, DrawCommandDescs)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FContextBatchInfo >, BatchInfos)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FVSMCullingBatchInfo >, VSMCullingBatchInfos)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint32 >, BatchInds)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVisibleInstanceCmd>, VisibleInstancesOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, DrawIndirectArgsBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, VisibleInstanceCountBufferOut)

		SHADER_PARAMETER_STRUCT_INCLUDE(FHZBShaderParameters, HZBShaderParameters)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutStaticInvalidatingPrimitives)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutInvalidatingInstances)
		SHADER_PARAMETER(uint32, NumInvalidatingInstanceSlots)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutStatsBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FCullPerPageDrawCommandsCs, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapBuildPerPageDrawCommands.usf", "CullPerPageDrawCommandsCs", SF_Compute);



class FAllocateCommandInstanceOutputSpaceCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAllocateCommandInstanceOutputSpaceCs);
	SHADER_USE_PARAMETER_STRUCT(FAllocateCommandInstanceOutputSpaceCs, FGlobalShader)
public:
	static constexpr int32 NumThreadsPerGroup = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	/**
	 */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FInstanceProcessingGPULoadBalancer::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
		OutEnvironment.SetDefine(TEXT("INDIRECT_ARGS_NUM_WORDS"), FInstanceCullingContext::IndirectArgsNumWords);
	}


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, DrawIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, InstanceIdOffsetBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputOffsetBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, TmpInstanceIdOffsetBufferOut)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FAllocateCommandInstanceOutputSpaceCs, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapBuildPerPageDrawCommands.usf", "AllocateCommandInstanceOutputSpaceCs", SF_Compute);


class FOutputCommandInstanceListsCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FOutputCommandInstanceListsCs);
	SHADER_USE_PARAMETER_STRUCT(FOutputCommandInstanceListsCs, FGlobalShader)
public:
	static constexpr int32 NumThreadsPerGroup = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	/**
	 */
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FInstanceProcessingGPULoadBalancer::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
		OutEnvironment.SetDefine(TEXT("NANITE_MULTI_VIEW"), 1);
		OutEnvironment.SetDefine(TEXT("INDIRECT_ARGS_NUM_WORDS"), FInstanceCullingContext::IndirectArgsNumWords);
	}


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< FVisibleInstanceCmd >, VisibleInstances)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, InstanceIdsBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, PageInfoBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, TmpInstanceIdOffsetBufferOut)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, VisibleInstanceCountBuffer)

		// Needed reference for make RDG happy somehow
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FOutputCommandInstanceListsCs, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapBuildPerPageDrawCommands.usf", "OutputCommandInstanceListsCs", SF_Compute);

class FUpdateAndClearDirtyFlagsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FUpdateAndClearDirtyFlagsCS);
	SHADER_USE_PARAMETER_STRUCT(FUpdateAndClearDirtyFlagsCS, FVirtualPageManagementShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, DirtyPageFlagsInOut)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FPhysicalPageMetaData >, OutPhysicalPageMetaData)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FUpdateAndClearDirtyFlagsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "UpdateAndClearDirtyFlagsCS", SF_Compute);


struct FCullingResult
{
	FRDGBufferRef DrawIndirectArgsRDG;
	FRDGBufferRef InstanceIdOffsetBufferRDG;
	FRDGBufferRef InstanceIdsBuffer;
	FRDGBufferRef PageInfoBuffer;
	uint32 MaxNumInstancesPerPass;
};

template <typename InstanceCullingLoadBalancerType>
static FCullingResult AddCullingPasses(FRDGBuilder& GraphBuilder,
	const TConstArrayView<FRHIDrawIndexedIndirectParameters> &IndirectArgs,
	const TConstArrayView<uint32>& DrawCommandDescs,
	const TConstArrayView<uint32>& InstanceIdOffsets,
	InstanceCullingLoadBalancerType *LoadBalancer,
	const TConstArrayView<FInstanceCullingMergedContext::FContextBatchInfo> BatchInfos,
	const TConstArrayView<FVSMCullingBatchInfo> VSMCullingBatchInfos,
	const TConstArrayView<uint32> BatchInds,
	uint32 TotalInstances,
	uint32 TotalPrimaryViews,
	FRDGBufferRef VirtualShadowViewsRDG,
	const FCullPerPageDrawCommandsCs::FHZBShaderParameters &HZBShaderParameters,
	FVirtualShadowMapArray &VirtualShadowMapArray,
	FGPUScene& GPUScene,
	const TConstArrayView<uint32> PrimitiveRevealedMask)
{
	const bool bUseBatchMode = !BatchInds.IsEmpty();

	int32 NumIndirectArgs = IndirectArgs.Num();

	FRDGBufferRef TmpInstanceIdOffsetBufferRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.TmpInstanceIdOffsetBuffer"), sizeof(uint32), NumIndirectArgs, nullptr, 0);

	// TODO: This is both not right, and also over conservative when running with the atomic path
	FCullingResult CullingResult;
	CullingResult.MaxNumInstancesPerPass = TotalInstances * 64u;
	FRDGBufferRef VisibleInstancesRdg = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.VisibleInstances"), sizeof(FVisibleInstanceCmd), CullingResult.MaxNumInstancesPerPass, nullptr, 0);

	FRDGBufferRef VisibleInstanceWriteOffsetRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.VisibleInstanceWriteOffset"), sizeof(uint32), 1, nullptr, 0);
	FRDGBufferRef OutputOffsetBufferRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.OutputOffsetBuffer"), sizeof(uint32), 1, nullptr, 0);

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VisibleInstanceWriteOffsetRDG), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(OutputOffsetBufferRDG), 0);

	// Create buffer for indirect args and upload draw arg data, also clears the instance to zero
	FRDGBufferDesc IndirectArgsDesc = FRDGBufferDesc::CreateIndirectDesc(FInstanceCullingContext::IndirectArgsNumWords * IndirectArgs.Num());
	IndirectArgsDesc.Usage |= BUF_MultiGPUGraphIgnore;

	CullingResult.DrawIndirectArgsRDG = GraphBuilder.CreateBuffer(IndirectArgsDesc, TEXT("Shadow.Virtual.DrawIndirectArgsBuffer"));
	GraphBuilder.QueueBufferUpload(CullingResult.DrawIndirectArgsRDG, IndirectArgs.GetData(), IndirectArgs.GetTypeSize() * IndirectArgs.Num());

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GPUScene.GetFeatureLevel());

	// Note: we redundantly clear the instance counts here as there is some issue with replays on certain consoles.
	FInstanceCullingContext::AddClearIndirectArgInstanceCountPass(GraphBuilder, ShaderMap, CullingResult.DrawIndirectArgsRDG);

	// not using structured buffer as we have to get at it as a vertex buffer 
	CullingResult.InstanceIdOffsetBufferRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), InstanceIdOffsets.Num()), TEXT("Shadow.Virtual.InstanceIdOffsetBuffer"));


	FRDGBufferRef PrimitiveRevealedMaskRdg = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 4);

	if (!PrimitiveRevealedMask.IsEmpty())
	{
		PrimitiveRevealedMaskRdg = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.RevealedPrimitivesMask"), PrimitiveRevealedMask);
	}


	{
		FCullPerPageDrawCommandsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FCullPerPageDrawCommandsCs::FParameters>();

		PassParameters->VirtualShadowMap = VirtualShadowMapArray.GetUniformBuffer();

		const FGPUSceneResourceParameters GPUSceneParameters = GPUScene.GetShaderParameters();

		PassParameters->GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
		PassParameters->GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
		PassParameters->GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
		PassParameters->GPUSceneFrameNumber = GPUSceneParameters.GPUSceneFrameNumber;
		PassParameters->InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;

		// Make sure there is enough space in the buffer for all the primitive IDs that might be used to index, at least in the first batch...
		check(PrimitiveRevealedMaskRdg->Desc.NumElements * 32u >= uint32(VSMCullingBatchInfos[0].PrimitiveRevealedNum));
		PassParameters->PrimitiveRevealedMask = GraphBuilder.CreateSRV(PrimitiveRevealedMaskRdg);
		PassParameters->PrimitiveRevealedNum = VSMCullingBatchInfos[0].PrimitiveRevealedNum;
		PassParameters->OutDirtyPageFlags = GraphBuilder.CreateUAV(VirtualShadowMapArray.DirtyPageFlagsRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		PassParameters->DynamicInstanceIdOffset = BatchInfos[0].DynamicInstanceIdOffset;
		PassParameters->DynamicInstanceIdMax = BatchInfos[0].DynamicInstanceIdMax;
		
		PassParameters->MaxMaterialPositionInvalidationRange = CVarMaxMaterialPositionInvalidationRange.GetValueOnRenderThread();

		auto GPUData = LoadBalancer->Upload(GraphBuilder);
		GPUData.GetShaderParameters(GraphBuilder, PassParameters->LoadBalancerParameters);

		PassParameters->FirstPrimaryView = VSMCullingBatchInfos[0].FirstPrimaryView;
		PassParameters->NumPrimaryViews = VSMCullingBatchInfos[0].NumPrimaryViews;
		PassParameters->CullingViewOriginOffset = VSMCullingBatchInfos[0].CullingViewOriginOffset;
		PassParameters->CullingViewOriginTile = VSMCullingBatchInfos[0].CullingViewOriginTile;

		PassParameters->TotalPrimaryViews = TotalPrimaryViews;
		PassParameters->VisibleInstancesBufferNum = CullingResult.MaxNumInstancesPerPass;
		PassParameters->InViews = GraphBuilder.CreateSRV(VirtualShadowViewsRDG);
		PassParameters->DrawCommandDescs = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.DrawCommandDescs"), DrawCommandDescs));

		if (bUseBatchMode)
		{
			PassParameters->BatchInfos = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.BatchInfos"), BatchInfos));
			PassParameters->VSMCullingBatchInfos = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.VSMCullingBatchInfos"), VSMCullingBatchInfos));
			PassParameters->BatchInds = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.BatchInds"), BatchInds));
		}

		PassParameters->DrawIndirectArgsBufferOut = GraphBuilder.CreateUAV(CullingResult.DrawIndirectArgsRDG, PF_R32_UINT, ERDGUnorderedAccessViewFlags::SkipBarrier);

		PassParameters->VisibleInstancesOut = GraphBuilder.CreateUAV(VisibleInstancesRdg, ERDGUnorderedAccessViewFlags::SkipBarrier);
		PassParameters->VisibleInstanceCountBufferOut = GraphBuilder.CreateUAV(VisibleInstanceWriteOffsetRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		PassParameters->OutStaticInvalidatingPrimitives = GraphBuilder.CreateUAV(VirtualShadowMapArray.StaticInvalidatingPrimitivesRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		PassParameters->HZBShaderParameters = HZBShaderParameters;

		bool bGenerateStats = VirtualShadowMapArray.StatsBufferRDG != nullptr;
		if (bGenerateStats)
		{
			PassParameters->OutStatsBuffer = GraphBuilder.CreateUAV(VirtualShadowMapArray.StatsBufferRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		}

		FCullPerPageDrawCommandsCs::FPermutationDomain PermutationVector;
		PermutationVector.Set< FCullPerPageDrawCommandsCs::FBatchedDim >(bUseBatchMode);
		PermutationVector.Set< FCullPerPageDrawCommandsCs::FUseHzbDim >(HZBShaderParameters.HZBTexture != nullptr);
		PermutationVector.Set< FCullPerPageDrawCommandsCs::FGenerateStatsDim >(bGenerateStats);

		auto ComputeShader = ShaderMap->GetShader<FCullPerPageDrawCommandsCs>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CullPerPageDrawCommands"),
			ComputeShader,
			PassParameters,
			LoadBalancer->GetWrappedCsGroupCount()
		);
	}
	// 2.2.Allocate space for the final instance ID output and so on.
	{
		FAllocateCommandInstanceOutputSpaceCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FAllocateCommandInstanceOutputSpaceCs::FParameters>();

		FRDGBufferRef InstanceIdOutOffsetBufferRDG = CreateStructuredBuffer(GraphBuilder, TEXT("InstanceCulling.OutputOffsetBufferOut"), sizeof(uint32), 1, nullptr, 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(InstanceIdOutOffsetBufferRDG), 0);

		PassParameters->NumIndirectArgs = NumIndirectArgs;
		PassParameters->InstanceIdOffsetBufferOut = GraphBuilder.CreateUAV(CullingResult.InstanceIdOffsetBufferRDG, PF_R32_UINT);;
		PassParameters->OutputOffsetBufferOut = GraphBuilder.CreateUAV(InstanceIdOutOffsetBufferRDG);
		PassParameters->TmpInstanceIdOffsetBufferOut = GraphBuilder.CreateUAV(TmpInstanceIdOffsetBufferRDG);
		PassParameters->DrawIndirectArgsBuffer = GraphBuilder.CreateSRV(CullingResult.DrawIndirectArgsRDG, PF_R32_UINT);

		auto ComputeShader = ShaderMap->GetShader<FAllocateCommandInstanceOutputSpaceCs>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("AllocateCommandInstanceOutputSpaceCs"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(NumIndirectArgs, FAllocateCommandInstanceOutputSpaceCs::NumThreadsPerGroup)
		);
	}
	// 2.3. Perform final pass to re-shuffle the instance ID's to their final resting places
	CullingResult.InstanceIdsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), CullingResult.MaxNumInstancesPerPass), TEXT("Shadow.Virtual.InstanceIdsBuffer"));
	CullingResult.PageInfoBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), CullingResult.MaxNumInstancesPerPass), TEXT("Shadow.Virtual.PageInfoBuffer"));

	FRDGBufferRef OutputPassIndirectArgs = FComputeShaderUtils::AddIndirectArgsSetupCsPass1D(GraphBuilder, GPUScene.GetFeatureLevel(), VisibleInstanceWriteOffsetRDG, TEXT("Shadow.Virtual.IndirectArgs"), FOutputCommandInstanceListsCs::NumThreadsPerGroup);
	{

		FOutputCommandInstanceListsCs::FParameters* PassParameters = GraphBuilder.AllocParameters<FOutputCommandInstanceListsCs::FParameters>();

		PassParameters->VisibleInstances = GraphBuilder.CreateSRV(VisibleInstancesRdg);
		PassParameters->PageInfoBufferOut = GraphBuilder.CreateUAV(CullingResult.PageInfoBuffer);
		PassParameters->InstanceIdsBufferOut = GraphBuilder.CreateUAV(CullingResult.InstanceIdsBuffer);
		PassParameters->TmpInstanceIdOffsetBufferOut = GraphBuilder.CreateUAV(TmpInstanceIdOffsetBufferRDG);
		PassParameters->VisibleInstanceCountBuffer = GraphBuilder.CreateSRV(VisibleInstanceWriteOffsetRDG);
		PassParameters->IndirectArgs = OutputPassIndirectArgs;

		auto ComputeShader = ShaderMap->GetShader<FOutputCommandInstanceListsCs>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("OutputCommandInstanceListsCs"),
			ComputeShader,
			PassParameters,
			OutputPassIndirectArgs,
			0
		);
	}

	return CullingResult;
}


static void AddRasterPass(
	FRDGBuilder& GraphBuilder, 
	FRDGEventName&& PassName,
	const FViewInfo * ShadowDepthView, 
	const TRDGUniformBufferRef<FShadowDepthPassUniformParameters> &ShadowDepthPassUniformBuffer,
	FVirtualShadowMapArray& VirtualShadowMapArray,
	FRDGBufferRef VirtualShadowViewsRDG,
	const FCullingResult &CullingResult, 
	FParallelMeshDrawCommandPass& MeshCommandPass,
	FVirtualShadowDepthPassParameters* PassParameters,
	TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> InstanceCullingUniformBuffer)
{
	PassParameters->View = ShadowDepthView->ViewUniformBuffer;
	PassParameters->ShadowDepthPass = ShadowDepthPassUniformBuffer;

	PassParameters->VirtualShadowMap = VirtualShadowMapArray.GetUniformBuffer();
	PassParameters->InViews = GraphBuilder.CreateSRV(VirtualShadowViewsRDG);
	PassParameters->InstanceCullingDrawParams.DrawIndirectArgsBuffer = CullingResult.DrawIndirectArgsRDG;
	PassParameters->InstanceCullingDrawParams.InstanceIdOffsetBuffer = CullingResult.InstanceIdOffsetBufferRDG;
	PassParameters->InstanceCullingDrawParams.InstanceCulling = InstanceCullingUniformBuffer;

	FIntRect ViewRect;
	ViewRect.Max = FVirtualShadowMap::VirtualMaxResolutionXY;

	GraphBuilder.AddPass(
		MoveTemp(PassName),
		PassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[&MeshCommandPass, PassParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			FRHIRenderPassInfo RPInfo;
			RPInfo.ResolveRect = FResolveRect(ViewRect);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("RasterizeVirtualShadowMaps(Non-Nanite)"));

			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, FMath::Min(ViewRect.Max.X, 32767), FMath::Min(ViewRect.Max.Y, 32767), 1.0f);

			MeshCommandPass.DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
			RHICmdList.EndRenderPass();
		});
}

void FVirtualShadowMapArray::RenderVirtualShadowMapsNonNanite(FRDGBuilder& GraphBuilder, const TArray<FProjectedShadowInfo *, SceneRenderingAllocator>& VirtualSmMeshCommandPasses, TArrayView<FViewInfo> Views)
{
	if (VirtualSmMeshCommandPasses.Num() == 0)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "RenderVirtualShadowMaps(Non-Nanite)");

	FGPUScene& GPUScene = Scene.GPUScene;

	FRDGBufferSRVRef PrevPageTableRDGSRV = CacheManager->IsValid() ? GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PageTable, TEXT("Shadow.Virtual.PrevPageTable"))) : nullptr;
	FRDGBufferSRVRef PrevPageFlagsRDGSRV = CacheManager->IsValid() ? GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PageFlags, TEXT("Shadow.Virtual.PrevPageFlags"))) : nullptr;
	FRDGBufferSRVRef PrevPageRectBoundsRDGSRV = CacheManager->IsValid() ? GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(CacheManager->PrevBuffers.PageRectBounds, TEXT("Shadow.Virtual.PrevPageRectBounds"))) : nullptr;

	int32 HZBMode = CVarNonNaniteVsmUseHzb.GetValueOnRenderThread();
	// When disabling Nanite, there may be stale data in the Nanite-HZB causing incorrect culling.
	if (!bHZBBuiltThisFrame)
	{
		HZBMode = 0; /* Disable HZB culling */
	}

	auto InitHZB = [&]()->FRDGTextureRef
	{
		if (HZBMode == 1 && CacheManager->IsValid())
		{
			return GraphBuilder.RegisterExternalTexture(CacheManager->PrevBuffers.HZBPhysical);
		}

		if (HZBMode == 2 && HZBPhysical != nullptr)
		{
			return HZBPhysical;
		}
		return nullptr;
	};
	const FRDGTextureRef HZBTexture = InitHZB();

	TArray<FVSMCullingBatchInfo, SceneRenderingAllocator> UnBatchedVSMCullingBatchInfo;
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> BatchedVirtualSmMeshCommandPasses;
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> UnBatchedVirtualSmMeshCommandPasses;
	UnBatchedVSMCullingBatchInfo.Reserve(VirtualSmMeshCommandPasses.Num());
	BatchedVirtualSmMeshCommandPasses.Reserve(VirtualSmMeshCommandPasses.Num());
	UnBatchedVirtualSmMeshCommandPasses.Reserve(VirtualSmMeshCommandPasses.Num());
	TArray<Nanite::FPackedView, SceneRenderingAllocator> VirtualShadowViews;

	TArray<uint32, SceneRenderingAllocator> PrimitiveRevealedMask;

	TArray<FVSMCullingBatchInfo, SceneRenderingAllocator> VSMCullingBatchInfos;
	VSMCullingBatchInfos.Reserve(VirtualSmMeshCommandPasses.Num());

	TArray<FVirtualShadowDepthPassParameters*, SceneRenderingAllocator> BatchedPassParameters;
	BatchedPassParameters.Reserve(VirtualSmMeshCommandPasses.Num());

	/**
	 * Use the 'dependent view' i.e., the view used to set up a view dependent CSM/VSM(clipmap) OR select the view closest to the local light.
	 * This last is important to get some kind of reasonable behavior for split screen.
	 */
	auto GetCullingViewOrigin = [&Views](const FProjectedShadowInfo* ProjectedShadowInfo) -> FLargeWorldRenderPosition
	{
		if (ProjectedShadowInfo->DependentView != nullptr)
		{
			return FLargeWorldRenderPosition(ProjectedShadowInfo->DependentView->ShadowViewMatrices.GetViewOrigin());
		}

		// VSM supports only whole scene shadows, so those without a "DependentView" are local lights
		// For local lights the origin is the (inverse of) pre-shadow translation. 
		check(ProjectedShadowInfo->bWholeSceneShadow);

		FVector MinOrigin = Views[0].ShadowViewMatrices.GetViewOrigin();
		double MinDistanceSq = (MinOrigin + ProjectedShadowInfo->PreShadowTranslation).SquaredLength();
		for (int Index = 1; Index < Views.Num(); ++Index)
		{
			FVector TestOrigin = Views[Index].ShadowViewMatrices.GetViewOrigin();
			double TestDistanceSq = (TestOrigin + ProjectedShadowInfo->PreShadowTranslation).SquaredLength();
			if (TestDistanceSq < MinDistanceSq)
			{
				MinOrigin = TestOrigin;
				MinDistanceSq = TestDistanceSq;
			}

		}
		return FLargeWorldRenderPosition(MinOrigin);
	};

	FInstanceCullingMergedContext InstanceCullingMergedContext(GPUScene.GetFeatureLevel());
	// We don't use the registered culling views (this redundancy should probably be addressed at some point), set the number to disable index range checking
	InstanceCullingMergedContext.NumCullingViews = -1;
	for (int32 Index = 0; Index < VirtualSmMeshCommandPasses.Num(); ++Index)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VirtualSmMeshCommandPasses[Index];

		if (!ProjectedShadowInfo->bShouldRenderVSM)
		{
			continue;
		}

		ProjectedShadowInfo->BeginRenderView(GraphBuilder, &Scene);

		FVSMCullingBatchInfo VSMCullingBatchInfo;
		VSMCullingBatchInfo.FirstPrimaryView = uint32(VirtualShadowViews.Num());
		VSMCullingBatchInfo.NumPrimaryViews = 0U;

		VSMCullingBatchInfo.PrimitiveRevealedOffset = uint32(PrimitiveRevealedMask.Num());
		VSMCullingBatchInfo.PrimitiveRevealedNum = 0U;

		const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap = ProjectedShadowInfo->VirtualShadowMapClipmap;
		if (Clipmap.IsValid() && !Clipmap->GetRevealedPrimitivesMask().IsEmpty())
		{
			PrimitiveRevealedMask.Append(Clipmap->GetRevealedPrimitivesMask().GetData(), Clipmap->GetRevealedPrimitivesMask().Num());
			VSMCullingBatchInfo.PrimitiveRevealedNum = Clipmap->GetNumRevealedPrimitives();
		}

		{
			const FLargeWorldRenderPosition CullingViewOrigin = GetCullingViewOrigin(ProjectedShadowInfo);
			VSMCullingBatchInfo.CullingViewOriginOffset = CullingViewOrigin.GetOffset();
			VSMCullingBatchInfo.CullingViewOriginTile = CullingViewOrigin.GetTile();
		}

		check(Clipmap.IsValid() || ProjectedShadowInfo->HasVirtualShadowMap());
		{
			FParallelMeshDrawCommandPass& MeshCommandPass = ProjectedShadowInfo->GetShadowDepthPass();
			MeshCommandPass.WaitForSetupTask();

			FInstanceCullingContext* InstanceCullingContext = MeshCommandPass.GetInstanceCullingContext();

			if (InstanceCullingContext->HasCullingCommands())
			{
				VSMCullingBatchInfo.NumPrimaryViews = AddRenderViews(ProjectedShadowInfo, 1.0f, HZBTexture != nullptr, false, ProjectedShadowInfo->ShouldClampToNearPlane(), VirtualShadowViews);

				if (CVarDoNonNaniteBatching.GetValueOnRenderThread() != 0)
				{
					FViewInfo* ShadowDepthView = ProjectedShadowInfo->ShadowDepthView;
					uint32 DynamicInstanceIdOffset = ShadowDepthView->DynamicPrimitiveCollector.GetInstanceSceneDataOffset();
					uint32 DynamicInstanceIdMax = DynamicInstanceIdOffset + ShadowDepthView->DynamicPrimitiveCollector.NumInstances();

					VSMCullingBatchInfos.Add(VSMCullingBatchInfo);

					// Note: we have to allocate these up front as the context merging machinery writes the offsets directly to the &PassParameters->InstanceCullingDrawParams, 
					// this is a side-effect from sharing the code with the deferred culling. Should probably be refactored.
					FVirtualShadowDepthPassParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualShadowDepthPassParameters>();
					InstanceCullingMergedContext.AddBatch(GraphBuilder, InstanceCullingContext, DynamicInstanceIdOffset, ShadowDepthView->DynamicPrimitiveCollector.NumInstances(), &PassParameters->InstanceCullingDrawParams);
					BatchedVirtualSmMeshCommandPasses.Add(ProjectedShadowInfo);
					BatchedPassParameters.Add(PassParameters);
				}
				else
				{
					UnBatchedVSMCullingBatchInfo.Add(VSMCullingBatchInfo);
					UnBatchedVirtualSmMeshCommandPasses.Add(ProjectedShadowInfo);
				}
			}
		}
	}
	uint32 TotalPrimaryViews = uint32(VirtualShadowViews.Num());
	CreateMipViews(VirtualShadowViews);
	FRDGBufferRef VirtualShadowViewsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.VirtualShadowViews"), VirtualShadowViews);

	// Helper function to create raster pass UB - only really need two of these ever
	const FSceneTextures* SceneTextures = &GetViewFamilyInfo(Views).GetSceneTextures();
	auto CreateShadowDepthPassUniformBuffer = [this, &VirtualShadowViewsRDG, &GraphBuilder, SceneTextures](bool bClampToNearPlane)
	{
		FShadowDepthPassUniformParameters* ShadowDepthPassParameters = GraphBuilder.AllocParameters<FShadowDepthPassUniformParameters>();
		check(PhysicalPagePoolRDG != nullptr);
		// TODO: These are not used for this case anyway
		ShadowDepthPassParameters->ProjectionMatrix = FMatrix44f::Identity;
		ShadowDepthPassParameters->ViewMatrix = FMatrix44f::Identity;
		ShadowDepthPassParameters->ShadowParams = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
		ShadowDepthPassParameters->bRenderToVirtualShadowMap = true;

		ShadowDepthPassParameters->VirtualSmPageTable = GraphBuilder.CreateSRV(PageTableRDG);
		ShadowDepthPassParameters->PackedNaniteViews = GraphBuilder.CreateSRV(VirtualShadowViewsRDG);
		ShadowDepthPassParameters->PageRectBounds = GraphBuilder.CreateSRV(PageRectBoundsRDG);
		ShadowDepthPassParameters->OutDepthBufferArray = GraphBuilder.CreateUAV(PhysicalPagePoolRDG, ERDGUnorderedAccessViewFlags::SkipBarrier);
		SetupSceneTextureUniformParameters(GraphBuilder, SceneTextures, Scene.GetFeatureLevel(), ESceneTextureSetupMode::None, ShadowDepthPassParameters->SceneTextures);
		ShadowDepthPassParameters->bClampToNearPlane = bClampToNearPlane;

		return GraphBuilder.CreateUniformBuffer(ShadowDepthPassParameters);
	};

	FCullPerPageDrawCommandsCs::FHZBShaderParameters HZBShaderParameters;
	if (HZBTexture)
	{
		// Mode 2 uses the current frame HZB  & page table.
		HZBShaderParameters.HZBPageTable = HZBMode == 2 ? GraphBuilder.CreateSRV(PageTableRDG) : PrevPageTableRDGSRV;
		HZBShaderParameters.HZBPageFlags = HZBMode == 2 ? GraphBuilder.CreateSRV(PageFlagsRDG) : PrevPageFlagsRDGSRV;
		HZBShaderParameters.HZBPageRectBounds = HZBMode == 2 ? GraphBuilder.CreateSRV(PageRectBoundsRDG) : PrevPageRectBoundsRDGSRV;
		HZBShaderParameters.HZBTexture = HZBTexture;
		HZBShaderParameters.HZBSize = HZBTexture->Desc.Extent;
		HZBShaderParameters.HZBSampler = TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI();
		HZBShaderParameters.HZBMode = HZBMode;
	}

	// Process batched passes
	if (!InstanceCullingMergedContext.Batches.IsEmpty())
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Batched");

		InstanceCullingMergedContext.MergeBatches();

		GraphBuilder.BeginEventScope(RDG_EVENT_NAME("CullingPasses"));
		FCullingResult CullingResult = AddCullingPasses(
			GraphBuilder,
			InstanceCullingMergedContext.IndirectArgs,
			InstanceCullingMergedContext.DrawCommandDescs,
			InstanceCullingMergedContext.InstanceIdOffsets,
			&InstanceCullingMergedContext.LoadBalancers[uint32(EBatchProcessingMode::Generic)],
			InstanceCullingMergedContext.BatchInfos,
			VSMCullingBatchInfos,
			InstanceCullingMergedContext.BatchInds[uint32(EBatchProcessingMode::Generic)],
			InstanceCullingMergedContext.TotalInstances,
			TotalPrimaryViews,
			VirtualShadowViewsRDG,
			HZBShaderParameters,
			*this,
			GPUScene,
			PrimitiveRevealedMask
		);
		GraphBuilder.EndEventScope();

		TRDGUniformBufferRef<FShadowDepthPassUniformParameters> ShadowDepthPassUniformBuffer = CreateShadowDepthPassUniformBuffer(false);

		FInstanceCullingGlobalUniforms* InstanceCullingGlobalUniforms = GraphBuilder.AllocParameters<FInstanceCullingGlobalUniforms>();
		InstanceCullingGlobalUniforms->InstanceIdsBuffer = GraphBuilder.CreateSRV(CullingResult.InstanceIdsBuffer);
		InstanceCullingGlobalUniforms->PageInfoBuffer = GraphBuilder.CreateSRV(CullingResult.PageInfoBuffer);
		InstanceCullingGlobalUniforms->BufferCapacity = CullingResult.MaxNumInstancesPerPass;
		TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> InstanceCullingUniformBuffer = GraphBuilder.CreateUniformBuffer(InstanceCullingGlobalUniforms);

		if(!BatchedVirtualSmMeshCommandPasses.IsEmpty())
		{
			if (CVarVirtualShadowSinglePassBatched.GetValueOnRenderThread() != 0)
			{
				FVirtualShadowDepthPassParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualShadowDepthPassParameters>();
				FProjectedShadowInfo* ProjectedShadowInfo0 = BatchedVirtualSmMeshCommandPasses[0];
				FViewInfo* ShadowDepthView = ProjectedShadowInfo0->ShadowDepthView;

				PassParameters->View = ShadowDepthView->ViewUniformBuffer;
				PassParameters->ShadowDepthPass = ShadowDepthPassUniformBuffer;

				PassParameters->VirtualShadowMap = GetUniformBuffer();
				PassParameters->InViews = GraphBuilder.CreateSRV(VirtualShadowViewsRDG);
				PassParameters->InstanceCullingDrawParams.DrawIndirectArgsBuffer = CullingResult.DrawIndirectArgsRDG;
				PassParameters->InstanceCullingDrawParams.InstanceIdOffsetBuffer = CullingResult.InstanceIdOffsetBufferRDG;
				PassParameters->InstanceCullingDrawParams.InstanceCulling = InstanceCullingUniformBuffer;
				PassParameters->InstanceCullingDrawParams.IndirectArgsByteOffset = 0U;
				PassParameters->InstanceCullingDrawParams.InstanceDataByteOffset = 0U;


				GraphBuilder.AddPass(
					RDG_EVENT_NAME("RasterPasses"),
					PassParameters,
					ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
					[PassParameters, 
					BatchedPassParameters=MoveTemp(BatchedPassParameters), 
					BatchedVirtualSmMeshCommandPasses=MoveTemp(BatchedVirtualSmMeshCommandPasses)](FRHICommandList& RHICmdList)
			{
						FIntRect ViewRect;
						ViewRect.Min = FIntPoint(0, 0);
						ViewRect.Max = FVirtualShadowMap::VirtualMaxResolutionXY;
						FRHIRenderPassInfo RPInfo;
						RPInfo.ResolveRect = FResolveRect(ViewRect);
						RHICmdList.BeginRenderPass(RPInfo, TEXT("RasterizeVirtualShadowMaps(Non-Nanite)"));

						RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, FMath::Min(ViewRect.Max.X, 32767), FMath::Min(ViewRect.Max.Y, 32767), 1.0f);

						for (int32 Index = 0; Index < BatchedVirtualSmMeshCommandPasses.Num(); ++Index)
						{
							FProjectedShadowInfo* ProjectedShadowInfo = BatchedVirtualSmMeshCommandPasses[Index];
							FParallelMeshDrawCommandPass& MeshCommandPass = ProjectedShadowInfo->GetShadowDepthPass();

							FInstanceCullingDrawParams InstanceCullingDrawParams = PassParameters->InstanceCullingDrawParams;
							InstanceCullingDrawParams.IndirectArgsByteOffset = BatchedPassParameters[Index]->InstanceCullingDrawParams.IndirectArgsByteOffset;
							InstanceCullingDrawParams.InstanceDataByteOffset = BatchedPassParameters[Index]->InstanceCullingDrawParams.InstanceDataByteOffset;
#if WITH_PROFILEGPU
							FString LightNameWithLevel;
							if (GVSMShowLightDrawEvents != 0)
							{
								FSceneRenderer::GetLightNameForDrawEvent(ProjectedShadowInfo->GetLightSceneInfo().Proxy, LightNameWithLevel);
							}
							SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, BatchedNonNanite, GVSMShowLightDrawEvents != 0, TEXT("%s"), *LightNameWithLevel);
#endif
							MeshCommandPass.DispatchDraw(nullptr, RHICmdList, &InstanceCullingDrawParams);
						}

						RHICmdList.EndRenderPass();
					});

			}
			else
			{
				RDG_EVENT_SCOPE(GraphBuilder, "RasterPasses");
				for (int32 Index = 0; Index < BatchedVirtualSmMeshCommandPasses.Num(); ++Index)
				{
					FProjectedShadowInfo* ProjectedShadowInfo = BatchedVirtualSmMeshCommandPasses[Index];
					FParallelMeshDrawCommandPass& MeshCommandPass = ProjectedShadowInfo->GetShadowDepthPass();
					FViewInfo* ShadowDepthView = ProjectedShadowInfo->ShadowDepthView;

					FString LightNameWithLevel;
					FSceneRenderer::GetLightNameForDrawEvent(ProjectedShadowInfo->GetLightSceneInfo().Proxy, LightNameWithLevel);
					AddRasterPass(GraphBuilder, RDG_EVENT_NAME("Rasterize[%s]", *LightNameWithLevel), ShadowDepthView, ShadowDepthPassUniformBuffer, *this, VirtualShadowViewsRDG, CullingResult, MeshCommandPass, BatchedPassParameters[Index], InstanceCullingUniformBuffer);
			}
		}
	}
	}

	// Loop over the un batched mesh command passes needed, these are all the clipmaps (but we may change the criteria)
	for (int32 Index = 0; Index < UnBatchedVirtualSmMeshCommandPasses.Num(); ++Index)
	{
		const auto VSMCullingBatchInfo = UnBatchedVSMCullingBatchInfo[Index];
		FProjectedShadowInfo* ProjectedShadowInfo = UnBatchedVirtualSmMeshCommandPasses[Index];
		FInstanceCullingMergedContext::FContextBatchInfo CullingBatchInfo = FInstanceCullingMergedContext::FContextBatchInfo{ 0 };

		FParallelMeshDrawCommandPass& MeshCommandPass = ProjectedShadowInfo->GetShadowDepthPass();
		const TSharedPtr<FVirtualShadowMapClipmap> Clipmap = ProjectedShadowInfo->VirtualShadowMapClipmap;
		FViewInfo* ShadowDepthView = ProjectedShadowInfo->ShadowDepthView;

		MeshCommandPass.WaitForSetupTask();

		FInstanceCullingContext* InstanceCullingContext = MeshCommandPass.GetInstanceCullingContext();

		if (InstanceCullingContext->HasCullingCommands())
		{
			FString LightNameWithLevel;
			FSceneRenderer::GetLightNameForDrawEvent(ProjectedShadowInfo->GetLightSceneInfo().Proxy, LightNameWithLevel);
			RDG_EVENT_SCOPE(GraphBuilder, "%s", *LightNameWithLevel);

			CullingBatchInfo.DynamicInstanceIdOffset = ShadowDepthView->DynamicPrimitiveCollector.GetInstanceSceneDataOffset();
			CullingBatchInfo.DynamicInstanceIdMax = CullingBatchInfo.DynamicInstanceIdOffset + ShadowDepthView->DynamicPrimitiveCollector.NumInstances();

			FCullingResult CullingResult = AddCullingPasses(
				GraphBuilder,
				InstanceCullingContext->IndirectArgs, 
				InstanceCullingContext->DrawCommandDescs,
				InstanceCullingContext->InstanceIdOffsets,
				InstanceCullingContext->LoadBalancers[uint32(EBatchProcessingMode::Generic)],
				MakeArrayView(&CullingBatchInfo, 1),
				MakeArrayView(&VSMCullingBatchInfo, 1),
				MakeArrayView<const uint32>(nullptr, 0),
				InstanceCullingContext->TotalInstances,
				TotalPrimaryViews,
				VirtualShadowViewsRDG,
				HZBShaderParameters,
				*this,
				GPUScene,
				Clipmap->GetRevealedPrimitivesMask()
			);

			TRDGUniformBufferRef<FShadowDepthPassUniformParameters> ShadowDepthPassUniformBuffer = CreateShadowDepthPassUniformBuffer(ProjectedShadowInfo->ShouldClampToNearPlane());

			FInstanceCullingGlobalUniforms* InstanceCullingGlobalUniforms = GraphBuilder.AllocParameters<FInstanceCullingGlobalUniforms>();
			InstanceCullingGlobalUniforms->InstanceIdsBuffer = GraphBuilder.CreateSRV(CullingResult.InstanceIdsBuffer);
			InstanceCullingGlobalUniforms->PageInfoBuffer = GraphBuilder.CreateSRV(CullingResult.PageInfoBuffer);
			InstanceCullingGlobalUniforms->BufferCapacity = CullingResult.MaxNumInstancesPerPass;
			TRDGUniformBufferRef<FInstanceCullingGlobalUniforms> InstanceCullingUniformBuffer = GraphBuilder.CreateUniformBuffer(InstanceCullingGlobalUniforms);

			FVirtualShadowDepthPassParameters* DepthPassParams = GraphBuilder.AllocParameters<FVirtualShadowDepthPassParameters>();
			DepthPassParams->InstanceCullingDrawParams.IndirectArgsByteOffset = 0;
			DepthPassParams->InstanceCullingDrawParams.InstanceDataByteOffset = 0;
			AddRasterPass(GraphBuilder, RDG_EVENT_NAME("Rasterize"), ShadowDepthView, ShadowDepthPassUniformBuffer, *this, VirtualShadowViewsRDG, CullingResult, MeshCommandPass, DepthPassParams, InstanceCullingUniformBuffer);
		}


		//
		if (Index == CVarShowClipmapStats.GetValueOnRenderThread())
		{
			// The 'main' view the shadow was created with respect to
			const FViewInfo* ViewUsedToCreateShadow = ProjectedShadowInfo->DependentView;
			const FViewInfo& View = *ViewUsedToCreateShadow;

			FVirtualSmPrintClipmapStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmPrintClipmapStatsCS::FParameters>();

			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintStruct);
			//PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
			PassParameters->ShadowMapIdRangeStart = Clipmap->GetVirtualShadowMap(0)->ID;
			// Note: assumes range!
			PassParameters->ShadowMapIdRangeEnd = Clipmap->GetVirtualShadowMap(0)->ID + Clipmap->GetLevelCount();
			PassParameters->PageRectBounds = GraphBuilder.CreateSRV(PageRectBoundsRDG);
			PassParameters->AllocatedPageRectBounds = GraphBuilder.CreateSRV(AllocatedPageRectBoundsRDG);

			auto ComputeShader = View.ShaderMap->GetShader<FVirtualSmPrintClipmapStatsCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("PrintClipmapStats"),
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1)
			);
		}
	}

	// Update the dirty page flags & the page table meta data for invalidations.
	{
		FUpdateAndClearDirtyFlagsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpdateAndClearDirtyFlagsCS::FParameters>();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->OutPhysicalPageMetaData = GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG);
		PassParameters->DirtyPageFlagsInOut = GraphBuilder.CreateUAV(DirtyPageFlagsRDG);
		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FUpdateAndClearDirtyFlagsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("UpdateAndClearDirtyFlags"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(UniformParameters.MaxPhysicalPages, FUpdateAndClearDirtyFlagsCS::DefaultCSGroupX), 1, 1)
		);
}
}

class FSelectPagesForHZBAndUpdateDirtyFlagsCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FSelectPagesForHZBAndUpdateDirtyFlagsCS);
	SHADER_USE_PARAMETER_STRUCT(FSelectPagesForHZBAndUpdateDirtyFlagsCS, FVirtualPageManagementShader)

	class FGenerateStatsDim : SHADER_PERMUTATION_BOOL("VSM_GENERATE_STATS");
	using FPermutationDomain = TShaderPermutationDomain< FGenerateStatsDim >;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FPhysicalPageMetaData>, OutPhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutPagesForHZBIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutPhysicalPagesForHZB)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, DirtyPageFlagsInOut)
		SHADER_PARAMETER(uint32, bFirstBuildThisFrame)
		SHADER_PARAMETER(uint32, bForceFullHZBUpdate)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< uint >, OutStatsBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FSelectPagesForHZBAndUpdateDirtyFlagsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "SelectPagesForHZBAndUpdateDirtyFlagsCS", SF_Compute);

class FVirtualSmBuildHZBPerPageCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmBuildHZBPerPageCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmBuildHZBPerPageCS, FVirtualPageManagementShader)

	static constexpr uint32 TotalHZBLevels = FVirtualShadowMap::NumHZBLevels;
	static constexpr uint32 HZBLevelsBase = TotalHZBLevels - 2U;

	static_assert(HZBLevelsBase == 5U, "The shader is expecting 5 levels, if the page size is changed, this needs to be massaged");

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPhysicalPageMetaData>, PhysicalPageMetaData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PhysicalPagesForHZB)
		SHADER_PARAMETER_SAMPLER(SamplerState, PhysicalPagePoolSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, PhysicalPagePool)

		SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(RWTexture2D<float>, FurthestHZBOutput, [HZBLevelsBase])
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmBuildHZBPerPageCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "BuildHZBPerPageCS", SF_Compute);


class FVirtualSmBBuildHZBPerPageTopCS : public FVirtualPageManagementShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmBBuildHZBPerPageTopCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmBBuildHZBPerPageTopCS, FVirtualPageManagementShader)

	// We need one level less as HZB starts at half-size (not really sure if we really need 1x1 and 2x2 sized levels).
	static constexpr uint32 HZBLevelsTop = 2;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, PhysicalPagesForHZB)
		SHADER_PARAMETER_SAMPLER(SamplerState, ParentTextureMipSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ParentTextureMip)
		SHADER_PARAMETER(FVector2f, InvHzbInputSize)

		SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(RWTexture2D<float>, FurthestHZBOutput, [HZBLevelsTop])
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmBBuildHZBPerPageTopCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapPageManagement.usf", "BuildHZBPerPageTopCS", SF_Compute);

void FVirtualShadowMapArray::UpdateHZB(FRDGBuilder& GraphBuilder)
{
	const FIntRect ViewRect(0, 0, GetPhysicalPoolSize().X, GetPhysicalPoolSize().Y);

	// 1. Gather up all physical pages that are allocated
	FRDGBufferRef PagesForHZBIndirectArgsRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(2U * 4U), TEXT("Shadow.Virtual.PagesForHZBIndirectArgs"));
	// NOTE: Total allocated pages since the shader outputs separate entries for static/dynamic pages
	FRDGBufferRef PhysicalPagesForHZBRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), GetTotalAllocatedPhysicalPages() + 1), TEXT("Shadow.Virtual.PhysicalPagesForHZB"));

	// 1. Clear the indirect args buffer (note 2x args)
	AddClearIndirectDispatchArgs1DPass(GraphBuilder, Scene.GetFeatureLevel(), PagesForHZBIndirectArgsRDG, 2U);

	// 2. Filter the relevant physical pages and set up the indirect args
	{
		FSelectPagesForHZBAndUpdateDirtyFlagsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectPagesForHZBAndUpdateDirtyFlagsCS::FParameters>();
		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		PassParameters->OutPagesForHZBIndirectArgsBuffer = GraphBuilder.CreateUAV(PagesForHZBIndirectArgsRDG);
		PassParameters->OutPhysicalPagesForHZB = GraphBuilder.CreateUAV(PhysicalPagesForHZBRDG);
		PassParameters->DirtyPageFlagsInOut = GraphBuilder.CreateUAV(DirtyPageFlagsRDG);
		PassParameters->OutPhysicalPageMetaData = GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG);
		PassParameters->bFirstBuildThisFrame = !bHZBBuiltThisFrame;
		PassParameters->bForceFullHZBUpdate = CVarShadowsVirtualForceFullHZBUpdate.GetValueOnRenderThread();
		FSelectPagesForHZBAndUpdateDirtyFlagsCS::FPermutationDomain PermutationVector;
		SetStatsArgsAndPermutation<FSelectPagesForHZBAndUpdateDirtyFlagsCS>(GraphBuilder, StatsBufferRDG, PassParameters, PermutationVector);
		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FSelectPagesForHZBAndUpdateDirtyFlagsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SelectPagesForHZB"),
			ComputeShader,
			PassParameters,
			FIntVector(FMath::DivideAndRoundUp(UniformParameters.MaxPhysicalPages, FSelectPagesForHZBAndUpdateDirtyFlagsCS::DefaultCSGroupX), 1, 1)
		);
	}

	bHZBBuiltThisFrame = true;
		
	{
		FVirtualSmBuildHZBPerPageCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmBuildHZBPerPageCS::FParameters>();

		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);
		for (int32 DestMip = 0; DestMip < FVirtualSmBuildHZBPerPageCS::HZBLevelsBase; DestMip++)
		{
			PassParameters->FurthestHZBOutput[DestMip] = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(HZBPhysical, DestMip));
		}
		PassParameters->PhysicalPagePool = PhysicalPagePoolRDG;
		PassParameters->PhysicalPagePoolSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->PhysicalPageMetaData = GraphBuilder.CreateSRV(PhysicalPageMetaDataRDG);

		PassParameters->IndirectArgs = PagesForHZBIndirectArgsRDG;
		PassParameters->PhysicalPagesForHZB = GraphBuilder.CreateSRV(PhysicalPagesForHZBRDG);
		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FVirtualSmBuildHZBPerPageCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BuildHZBPerPage"),
			ComputeShader,
			PassParameters,
			PassParameters->IndirectArgs,
			0
		);
	}
	{
		FVirtualSmBBuildHZBPerPageTopCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmBBuildHZBPerPageTopCS::FParameters>();

		PassParameters->VirtualShadowMap = GetUncachedUniformBuffer(GraphBuilder);

		uint32 StartDestMip = FVirtualSmBuildHZBPerPageCS::HZBLevelsBase;
		for (int32 DestMip = 0; DestMip < FVirtualSmBBuildHZBPerPageTopCS::HZBLevelsTop; DestMip++)
		{
			PassParameters->FurthestHZBOutput[DestMip] = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(HZBPhysical, StartDestMip + DestMip));
		}
		FIntPoint SrcSize = FIntPoint::DivideAndRoundUp(FIntPoint(HZBPhysical->Desc.GetSize().X, HZBPhysical->Desc.GetSize().Y), 1 << int32(StartDestMip - 1));
		PassParameters->InvHzbInputSize = FVector2f(1.0f / SrcSize.X, 1.0f / SrcSize.Y);;
		PassParameters->ParentTextureMip = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(HZBPhysical, StartDestMip - 1));
		PassParameters->ParentTextureMipSampler = TStaticSamplerState<SF_Point>::GetRHI();

		PassParameters->IndirectArgs = PagesForHZBIndirectArgsRDG;
		PassParameters->PhysicalPagesForHZB = GraphBuilder.CreateSRV(PhysicalPagesForHZBRDG);
		auto ComputeShader = GetGlobalShaderMap(Scene.GetFeatureLevel())->GetShader<FVirtualSmBBuildHZBPerPageTopCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BuildHZBPerPageTop"),
			ComputeShader,
			PassParameters,
			PassParameters->IndirectArgs,
			// NOTE: offset 4 to get second set of args in the buffer.
			4U * sizeof(uint32)
		);
	}
}


uint32 FVirtualShadowMapArray::AddRenderViews(const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap, float LODScaleFactor, bool bSetHZBParams, bool bUpdateHZBMetaData, bool bClampToNearPlane, TArray<Nanite::FPackedView, SceneRenderingAllocator> &OutVirtualShadowViews)
{
	check(bClampToNearPlane);

	// TODO: Decide if this sort of logic belongs here or in Nanite (as with the mip level view expansion logic)
	// We're eventually going to want to snap/quantize these rectangles/positions somewhat so probably don't want it
	// entirely within Nanite, but likely makes sense to have some sort of "multi-viewport" notion in Nanite that can
	// handle both this and mips.
	// NOTE: There's still the additional VSM view logic that runs on top of this in Nanite too (see CullRasterize variant)
	Nanite::FPackedViewParams BaseParams;
	BaseParams.ViewRect = FIntRect(0, 0, FVirtualShadowMap::VirtualMaxResolutionXY, FVirtualShadowMap::VirtualMaxResolutionXY);
	BaseParams.HZBTestViewRect = BaseParams.ViewRect;
	BaseParams.RasterContextSize = GetPhysicalPoolSize();
	BaseParams.LODScaleFactor = LODScaleFactor;
	BaseParams.PrevTargetLayerIndex = INDEX_NONE;
	BaseParams.TargetMipLevel = 0;
	BaseParams.TargetMipCount = 1;	// No mips for clipmaps
	BaseParams.Flags = (bClampToNearPlane ? 0u : NANITE_VIEW_FLAG_NEAR_CLIP)
		| (GForceInvalidateDirectionalVSM != 0 ? NANITE_VIEW_FLAG_UNCACHED : 0u);

	if (Clipmap->GetCacheEntry())
	{
		Clipmap->GetCacheEntry()->MarkRendered(Scene.GetFrameNumber());
	}

	for (int32 ClipmapLevelIndex = 0; ClipmapLevelIndex < Clipmap->GetLevelCount(); ++ClipmapLevelIndex)
	{
		FVirtualShadowMap* VirtualShadowMap = Clipmap->GetVirtualShadowMap(ClipmapLevelIndex);

		Nanite::FPackedViewParams Params = BaseParams;
		Params.TargetLayerIndex = VirtualShadowMap->ID;
		Params.ViewMatrices = Clipmap->GetViewMatrices(ClipmapLevelIndex);
		Params.PrevTargetLayerIndex = INDEX_NONE;
		Params.PrevViewMatrices = Params.ViewMatrices;

		// TODO: Clean this up - could be stored in a single structure for the whole clipmap
		int32 HZBKey = Clipmap->GetHZBKey(ClipmapLevelIndex);

		if (bSetHZBParams)
		{
			CacheManager->SetHZBViewParams(HZBKey, Params);
		}

		// If we're going to generate a new HZB this frame, save the associated metadata
		if (bUpdateHZBMetaData)
		{
			FVirtualShadowMapHZBMetadata& HZBMeta = HZBMetadata.FindOrAdd(HZBKey);
			HZBMeta.TargetLayerIndex = Params.TargetLayerIndex;
			HZBMeta.ViewMatrices = Params.ViewMatrices;
			HZBMeta.ViewRect = Params.ViewRect;
		}

		Nanite::FPackedView View = Nanite::CreatePackedView(Params);
		OutVirtualShadowViews.Add(View);
	}

	return uint32(Clipmap->GetLevelCount());
}

uint32 FVirtualShadowMapArray::AddRenderViews(const FProjectedShadowInfo* ProjectedShadowInfo, float LODScaleFactor, bool bSetHZBParams, bool bUpdateHZBMetaData, bool bClampToNearPlane, TArray<Nanite::FPackedView, SceneRenderingAllocator>& OutVirtualShadowViews)
{
	if (ProjectedShadowInfo->VirtualShadowMapClipmap)
	{
		return AddRenderViews(ProjectedShadowInfo->VirtualShadowMapClipmap, LODScaleFactor, bSetHZBParams, bUpdateHZBMetaData, bClampToNearPlane, OutVirtualShadowViews);
	}
	Nanite::FPackedViewParams BaseParams;
	BaseParams.ViewRect = ProjectedShadowInfo->GetOuterViewRect();
	BaseParams.HZBTestViewRect = BaseParams.ViewRect;
	BaseParams.RasterContextSize = GetPhysicalPoolSize();
	BaseParams.LODScaleFactor = LODScaleFactor;
	BaseParams.PrevTargetLayerIndex = INDEX_NONE;
	BaseParams.TargetMipLevel = 0;
	BaseParams.TargetMipCount = FVirtualShadowMap::MaxMipLevels;
	// local lights enable distance cull by default
	BaseParams.Flags = NANITE_VIEW_FLAG_DISTANCE_CULL | (bClampToNearPlane ? 0u : NANITE_VIEW_FLAG_NEAR_CLIP);

	if (ProjectedShadowInfo->VirtualShadowMapPerLightCacheEntry)
	{
		ProjectedShadowInfo->VirtualShadowMapPerLightCacheEntry->MarkRendered(Scene.GetFrameNumber());
	}

	int32 NumMaps = ProjectedShadowInfo->bOnePassPointLightShadow ? 6 : 1;
	for (int32 Index = 0; Index < NumMaps; ++Index)
	{
		FVirtualShadowMap* VirtualShadowMap = ProjectedShadowInfo->VirtualShadowMaps[Index];

		Nanite::FPackedViewParams Params = BaseParams;
		Params.TargetLayerIndex = VirtualShadowMap->ID;
		Params.ViewMatrices = ProjectedShadowInfo->GetShadowDepthRenderingViewMatrices(Index, true);
		Params.RangeBasedCullingDistance = ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetRadius();

		int32 HZBKey = ProjectedShadowInfo->GetLightSceneInfo().Id + (Index << 24);

		if (bSetHZBParams)
		{
			CacheManager->SetHZBViewParams(HZBKey, Params);
		}

		// If we're going to generate a new HZB this frame, save the associated metadata
		if (bUpdateHZBMetaData)
		{
			FVirtualShadowMapHZBMetadata& HZBMeta = HZBMetadata.FindOrAdd(HZBKey);
			HZBMeta.TargetLayerIndex = Params.TargetLayerIndex;
			HZBMeta.ViewMatrices = Params.ViewMatrices;
			HZBMeta.ViewRect = Params.ViewRect;
		}

		OutVirtualShadowViews.Add(Nanite::CreatePackedView(Params));
	}

	return uint32(NumMaps);
}

void FVirtualShadowMapArray::AddVisualizePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, int32 ViewIndex, FScreenPassTexture Output)
{
#if !UE_BUILD_SHIPPING
	if (!IsAllocated() || DebugVisualizationOutput.IsEmpty())
	{
		return;
	}

	const FVirtualShadowMapVisualizationData& VisualizationData = GetVirtualShadowMapVisualizationData();
	if (VisualizationData.IsActive() && VisualizeLight[ViewIndex].IsValid())
	{	
		FCopyRectPS::FParameters* Parameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
		Parameters->InputTexture = DebugVisualizationOutput[ViewIndex];
		Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ENoAction);

		TShaderMapRef<FCopyRectPS> PixelShader(View.ShaderMap);

		FScreenPassTextureViewport InputViewport(DebugVisualizationOutput[ViewIndex]->Desc.Extent);
		FScreenPassTextureViewport OutputViewport(Output);

		// See CVarVisualizeLayout documentation
		const int32 VisualizeLayout = CVarVisualizeLayout.GetValueOnRenderThread();
		if (VisualizeLayout == 1)		// Thumbnail
		{
			const int32 TileWidth  = View.UnscaledViewRect.Width() / 3;
			const int32 TileHeight = View.UnscaledViewRect.Height() / 3;

			OutputViewport.Rect.Max = OutputViewport.Rect.Min + FIntPoint(TileWidth, TileHeight);
		}
		else if (VisualizeLayout == 2)	// Split screen
		{
			InputViewport.Rect.Max.X = InputViewport.Rect.Min.X + (InputViewport.Rect.Width() / 2);
			OutputViewport.Rect.Max.X = OutputViewport.Rect.Min.X + (OutputViewport.Rect.Width() / 2);
		}

		// Use separate input and output viewports w/ bilinear sampling to properly support dynamic resolution scaling
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DrawTexture"), View, OutputViewport, InputViewport, PixelShader, Parameters, EScreenPassDrawFlags::None);
		
		// Visualization light name
		{
			FScreenPassRenderTarget OutputTarget(Output.Texture, View.UnscaledViewRect, ERenderTargetLoadAction::ELoad);

			AddDrawCanvasPass(GraphBuilder, RDG_EVENT_NAME("Labels"), View, OutputTarget,
				[&VisualizeLight=VisualizeLight[ViewIndex], &OutputViewport=OutputViewport](FCanvas& Canvas)
			{
				const FLinearColor LabelColor(1, 1, 0);
				Canvas.DrawShadowedString(
					OutputViewport.Rect.Min.X + 8,
					OutputViewport.Rect.Max.Y - 19,
					*VisualizeLight.GetLightName(),
					GetStatsFont(),
					LabelColor);
			});
		}
	}
#endif
}
