// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneManagement.h"
#include "SceneView.h"
#include "VirtualShadowMapDefinitions.h"

struct FMinimalSceneTextures;
struct FSortedLightSetSceneInfo;
struct FNaniteVisibilityQuery;
class FViewInfo;
class FProjectedShadowInfo;
class FVisibleLightInfo;
class FVirtualShadowMapCacheEntry;
class FVirtualShadowMapArrayCacheManager;
struct FSortedLightSetSceneInfo;
class FVirtualShadowMapClipmap;
struct FScreenPassTexture;
struct FSingleLayerWaterPrePassResult;
class FNaniteVisibilityResults;
class FSceneRenderer;
class FSceneUniformBuffer;
struct FShaderCompilerEnvironment;
struct FFrontLayerTranslucencyData;
class FSceneInstanceCullingQuery;

namespace Nanite
{
	struct FPackedView;
	class  FPackedViewArray;
	struct FRasterResults;
}

// TODO: does this exist?
constexpr uint32 ILog2Const(uint32 n)
{
	return (n > 1) ? 1 + ILog2Const(n / 2) : 0;
}

// See CalcLevelOffsets in PageAccessCommon.ush for some details on this logic
constexpr uint32 CalcVirtualShadowMapLevelOffsets(uint32 Level, uint32 Log2Level0DimPagesXY)
{
	uint32 NumBits = Level << 1;
	uint32 StartBit = (2U * Log2Level0DimPagesXY + 2U) - NumBits;
	uint32 Mask = ((1U << NumBits) - 1U) << StartBit;
	return 0x55555555U & Mask;
}

class FVirtualShadowMap
{
public:
	// PageSize * Level0DimPagesXY defines the virtual address space, e.g., 128x128 = 16k

	// 128x128 = 16k
	static constexpr uint32 PageSize = 128U;
	static constexpr uint32 Level0DimPagesXY = 128U;

	static constexpr uint32 PageSizeMask = PageSize - 1U;
	static constexpr uint32 Log2PageSize = ILog2Const(PageSize);
	static constexpr uint32 Log2Level0DimPagesXY = ILog2Const(Level0DimPagesXY);
	static constexpr uint32 MaxMipLevels = Log2Level0DimPagesXY + 1U;

	static constexpr uint32 PageTableSize = CalcVirtualShadowMapLevelOffsets(MaxMipLevels, Log2Level0DimPagesXY);

	static constexpr uint32 VirtualMaxResolutionXY = Level0DimPagesXY * PageSize;
	
	static constexpr uint32 PhysicalPageAddressBits = 16U;
	static constexpr uint32 MaxPhysicalTextureDimPages = 1U << PhysicalPageAddressBits;
	static constexpr uint32 MaxPhysicalTextureDimTexels = MaxPhysicalTextureDimPages * PageSize;

	static constexpr uint32 NumHZBLevels = Log2PageSize;

	static constexpr uint32 RasterWindowPages = 4u;
	
	static_assert(MaxMipLevels <= 8, ">8 mips requires more PageFlags bits. See VSM_PAGE_FLAGS_BITS_PER_HMIP in PageAccessCommon.ush");

	// TODO: Currently only used for these constants... probably rename the cache structure to virtual shadow map now instead
private:
	FVirtualShadowMap() {}
};

// Useful data for both the page mapping shader and the projection shader
// as well as cached shadow maps
struct FVirtualShadowMapProjectionShaderData
{
	FMatrix44f ShadowViewToClipMatrix;
	FMatrix44f TranslatedWorldToShadowUVMatrix;
	FMatrix44f TranslatedWorldToShadowUVNormalMatrix;

	FVector3f LightDirection;
	uint32 LightType = ELightComponentType::LightType_Directional;

	FVector3f PreViewTranslationHigh;
	float LightRadius;
	
	FVector3f PreViewTranslationLow;
	// Slightly different meaning for clipmaps (includes camera pixel size scaling stuff) and local lights (raw bias)
	float ResolutionLodBias = 0.0f;
	
	// TODO: There are more local lights than directional
	// We should move the directional-specific stuff out to its own structure.
	FVector3f NegativeClipmapWorldOriginLWCOffset;	// Shares the LWCTile with PreViewTranslation
	float LightSourceRadius;

	FIntPoint ClipmapCornerRelativeOffset = FIntPoint(0, 0);
	int32 ClipmapLevel = MAX_int32;			// "Absolute" level, can be negative. Max_int32 if not a clipmap.
	int32 ClipmapLevelCountRemaining = -1;	// Remaining levels, relative to this one. Negative if not a clipmap.

	uint32 Flags = 0U;
	// This clipmap level should allow WPO if this value is less than InstanceWPODisableDistanceSquared
	float ClipmapLevelWPODistanceDisableThresholdSquared = 0.0f;
	float TexelDitherScale;
	
	// Seems the FMatrix forces 16-byte alignment
	float Padding;
};
static_assert(sizeof(FVirtualShadowMapProjectionShaderData) == (16*18), "FVirtualShadowMapProjectionShaderData does not match size in shader. See VirtualShadowMapProjectionStructs.ush.");

struct FVirtualShadowMapHZBMetadata
{
	FViewMatrices ViewMatrices;
	FIntRect	  ViewRect;
	uint32		  TargetLayerIndex = INDEX_NONE;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FVirtualShadowMapUniformParameters, )
	SHADER_PARAMETER(uint32, NumFullShadowMaps)
	SHADER_PARAMETER(uint32, NumSinglePageShadowMaps)
	SHADER_PARAMETER(uint32, MaxPhysicalPages)
	SHADER_PARAMETER(uint32, NumShadowMapSlots)
	// Set to 0 if separate static caching is disabled
	SHADER_PARAMETER(uint32, StaticCachedArrayIndex)
	// use to map linear index to x,y page coord
	SHADER_PARAMETER(uint32, PhysicalPageRowMask)
	SHADER_PARAMETER(uint32, PhysicalPageRowShift)
	SHADER_PARAMETER(uint32, PackedShadowMaskMaxLightCount)
	SHADER_PARAMETER(FVector4f, RecPhysicalPoolSize)
	SHADER_PARAMETER(FIntPoint, PhysicalPoolSize)
	SHADER_PARAMETER(FIntPoint, PhysicalPoolSizePages)

	// Set to 1 if r.Shadow.Virtual.NonNanite.IncludeInCoarsePages is set to 0 in order to signal that we want to use the legacy path for just excluding non-nanite
	SHADER_PARAMETER(uint32, bExcludeNonNaniteFromCoarsePages)
	SHADER_PARAMETER(float, CoarsePagePixelThresholdDynamic)
	SHADER_PARAMETER(float, CoarsePagePixelThresholdStatic)
	SHADER_PARAMETER(float, CoarsePagePixelThresholdDynamicNanite)

	// For shadow page age calculations
	SHADER_PARAMETER(uint32, SceneFrameNumber)

	SHADER_PARAMETER(uint32, bClipmapGreedyLevelSelection)
	SHADER_PARAMETER(float, GlobalResolutionLodBias)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ProjectionData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PageTable)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PageFlags)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, PageRectBounds)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, PhysicalPagePool)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CachePrimitiveAsDynamic)

	// Light grid with only the lights that have VSMs present
	// Still references the original indices from the global light grid
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, LightGridData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, NumCulledLightsGrid)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FVirtualShadowMapSamplingParameters, )
	// NOTE: These parameters must only be uniform buffers/references! Loose parameters do not get bound
	// in some of the forward passes that use this structure.
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
END_SHADER_PARAMETER_STRUCT()

FMatrix CalcTranslatedWorldToShadowUVMatrix(const FMatrix& TranslatedWorldToShadowView, const FMatrix& ViewToClip);
FMatrix CalcTranslatedWorldToShadowUVNormalMatrix(const FMatrix& TranslatedWorldToShadowView, const FMatrix& ViewToClip);

struct FVirtualShadowMapVisualizeLightSearch
{
public:
	FVirtualShadowMapVisualizeLightSearch()
	{
		Reset();
	}
	
	void Reset()
	{
		FoundKey.Packed = 0;
		FoundProxy = nullptr;
		FoundVirtualShadowMapId = INDEX_NONE;
	}

	void CheckLight(const FLightSceneProxy* CheckProxy, int CheckVirtualShadowMapId);

	bool IsValid() const { return FoundProxy != nullptr; }

	int GetVirtualShadowMapId() const { return FoundVirtualShadowMapId; }
	const FLightSceneProxy* GetProxy() const { return FoundProxy; }
	const FString GetLightName() const;

private:
	union SortKey
	{
		struct
		{
			// NOTE: Lowest to highest priority
			uint32 bExists : 1;				// Catch-all
			uint32 bDirectionalLight : 1;
			uint32 bOwnerSelected : 1;		// In editor
			uint32 bSelected : 1;			// In editor
			uint32 bPartialNameMatch : 1;
			uint32 bExactNameMatch : 1;
		} Fields;
		uint32 Packed;
	};

	SortKey FoundKey;
	const FLightSceneProxy* FoundProxy = nullptr;
	int FoundVirtualShadowMapId = INDEX_NONE;
};

class FVirtualShadowMapArray
{
public:	
	FVirtualShadowMapArray(FScene& InScene);
	~FVirtualShadowMapArray();

	void Initialize(
		FRDGBuilder& GraphBuilder,
		FVirtualShadowMapArrayCacheManager* InCacheManager,
		bool bInEnabled,
		const FEngineShowFlags& EngineShowFlags);

	// Returns true if virtual shadow maps are enabled
	bool IsEnabled() const
	{
		return bEnabled;
	}

	// Returns the first in a continuously allocated range of new VirtualShadowMapIds
	int32 Allocate(bool bSinglePageShadowMap, int32 Count);

	// TODO: Can probably make this 1:1 with allocate directly
	void UpdateNextData(int32 PrevVirtualShadowMapId, int32 CurrentVirtualShadowMapId, FInt32Point PageOffset = FInt32Point(0, 0));

	static bool IsSinglePage(int VirtualShadowMapId)
	{
		return (VirtualShadowMapId < VSM_MAX_SINGLE_PAGE_SHADOW_MAPS);
	}

	int32 GetNumShadowMapSlots() const
	{
		return NumShadowMapSlots;
	}

	int32 GetNumFullShadowMaps() const
	{
		return FMath::Max(GetNumShadowMapSlots() - int32(VSM_MAX_SINGLE_PAGE_SHADOW_MAPS), 0);
	}

	int32 GetNumSinglePageShadowMaps() const
	{
		return NumSinglePageShadowMaps;
	}

	/**
	 * Return the total of allocated SMs, both full and single-page SMs
	 */
	int32 GetNumShadowMaps() const
	{
		// If not initialized ShadowMaps is empty, but we want it to return at most 0 anyway
		return GetNumFullShadowMaps() + GetNumSinglePageShadowMaps();
	}

	// Raw size of the physical pool, including both static and dynamic pages (if enabled)
	FIntPoint GetPhysicalPoolSize() const;
	// Size of HZB (level 0)
	FIntPoint GetHZBPhysicalPoolSize() const;

	// Maximum number of physical pages to allocate. This value is NOT doubled when static caching is
	// enabled as we always allocate both as pairs (offset in the page pool).
	uint32 GetMaxPhysicalPages() const { return UniformParameters.MaxPhysicalPages; }
	// Total physical page count that includes separate static pages
	uint32 GetTotalAllocatedPhysicalPages() const;

	EPixelFormat GetPackedShadowMaskFormat() const;

	static void SetShaderDefines(FShaderCompilerEnvironment& OutEnvironment);

	void MergeStaticPhysicalPages(FRDGBuilder& GraphBuilder);

	void UpdatePhysicalPageAddresses(FRDGBuilder& GraphBuilder);

	void BuildPageAllocations(
		FRDGBuilder& GraphBuilder,
		const FMinimalSceneTextures& SceneTextures,
		const TConstArrayView<FViewInfo> &Views,
		const FSortedLightSetSceneInfo& SortedLights, 
		const TConstArrayView<FVisibleLightInfo>& VisibleLightInfos,
		const FSingleLayerWaterPrePassResult* SingleLayerWaterPrePassResult,
		const FFrontLayerTranslucencyData& FrontLayerTranslucencyData);

	bool IsAllocated() const
	{
		return PhysicalPagePoolRDG != nullptr && PageTableRDG != nullptr;
	}

	bool ShouldCacheStaticSeparately() const
	{
		return UniformParameters.StaticCachedArrayIndex > 0;
	}

	void CreateMipViews( TArray<Nanite::FPackedView, SceneRenderingAllocator>& Views ) const;

	Nanite::FPackedViewArray* CreateVirtualShadowMapNaniteViews(FRDGBuilder& GraphBuilder, TConstArrayView<FViewInfo> Views, TConstArrayView<FProjectedShadowInfo*> Shadows, float ShadowsLODScaleFactor, FSceneInstanceCullingQuery* InstanceCullingQuery);

	/**
	 * Draw Nanite geometry into the VSMs.
	 */
	void RenderVirtualShadowMapsNanite(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, bool bUpdateNaniteStreaming, const FNaniteVisibilityQuery* VisibilityQuery, Nanite::FPackedViewArray* VirtualShadowMapViews, FSceneInstanceCullingQuery* SceneInstanceCullingQuery);

	/**
	 * Draw Non-Nanite geometry into the VSMs.
	 */
	void RenderVirtualShadowMapsNonNanite(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& VirtualSmMeshCommandPasses, TArrayView<FViewInfo> Views);

	void RenderDebugInfo(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views);
	
	void LogStats(FRDGBuilder& GraphBuilder, const FViewInfo& View);

	// Get shader parameters necessary to sample virtual shadow maps
	// It is safe to bind this buffer even if VSMs are disabled, but the sampling should be branched around in the shader.
	// This data becomes valid after the shadow depths pass if VSMs are enabled
	FVirtualShadowMapSamplingParameters GetSamplingParameters(FRDGBuilder& GraphBuilder) const;
	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> GetUniformBuffer() const
	{
		return CachedUniformBuffer;
	}

	bool HasAnyShadowData() const { return PhysicalPagePoolRDG != nullptr;  }

	bool ShouldCullBackfacingPixels() const { return bCullBackfacingPixels; }

	void UpdateHZB(FRDGBuilder& GraphBuilder);		

	// Add render views, and mark shadow maps as rendered for a given clipmap or set of VSMs, returns the number of primary views added.
	uint32 AddRenderViews(const FProjectedShadowInfo* ProjectedShadowInfo, TConstArrayView<FViewInfo> Views, float LODScaleFactor, bool bSetHzbParams, bool bUpdateHZBMetaData, bool bClampToNearPlane, TArray<Nanite::FPackedView, SceneRenderingAllocator>& OutVirtualShadowViews);

	// Add visualization composite pass, if enabled
	void AddVisualizePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, int32 ViewIndex, FScreenPassTexture Output);

	//
	bool UseHzbOcclusion() const { return bUseHzbOcclusion; }
	bool UseTwoPassHzbOcclusion() const { return bUseTwoPassHzbOcclusion; }

	/**
	 * Helper function to add clamping when interpolating the LOD resolution biases to ensure the bias for moving lights can never be lower than the one for not.
	 * This could occur fairly easily since it is possible to both set the values through console as well as scalability.
	 */
	static float InterpolateResolutionBias(float BiasNonMoving, float BiasMoving, float LightMobilityFactor);

	/**
	* Helper function to create and clear an indirect args buffer
	*/
	static FRDGBufferRef CreateAndInitializeDispatchIndirectArgs1D(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, const TCHAR* Name);

	// We keep a reference to the cache manager that was used to initialize this frame as it owns some of the buffers
	FVirtualShadowMapArrayCacheManager* CacheManager = nullptr;

	FVirtualShadowMapUniformParameters UniformParameters;

	// Physical page pool shadow data and associated HZB and metadata
	// NOTE: The underlying textures are owned by FVirtualShadowMapCacheManager.
	// We just import and maintain a copy of the RDG reference for this frame here.
	FRDGTextureRef PhysicalPagePoolRDG = nullptr;
	TRefCountPtr<IPooledRenderTarget> HZBPhysical = nullptr;
	FRDGTextureRef HZBPhysicalRDG = nullptr;
	FRDGBufferRef PhysicalPageMetaDataRDG = nullptr;

	// Buffer that serves as the page table for all virtual shadow maps
	FRDGBufferRef PageTableRDG = nullptr;
		
	// Buffer that stores flags (uints) marking each page that needs to be rendered and cache status, for all virtual shadow maps.
	// Flag values defined in PageAccessCommon.ush
	FRDGBufferRef PageFlagsRDG = nullptr;

	// List(s) of physical pages used during allocation/updates
	// These can be saved frame to frame to allow keeping an LRU-sorted order for cached pages
	FRDGBufferRef PhysicalPageListsRDG = nullptr;

	// Allocation info for each page.
	FRDGBufferRef CachedPageInfosRDG = nullptr;

	// uint4 buffer with one rect for each mip level in all SMs, calculated to bound committed pages
	// Used to clip the rect size of clusters during culling.
	FRDGBufferRef PageRectBoundsRDG = nullptr;
	FRDGBufferRef AllocatedPageRectBoundsRDG = nullptr;
	FRDGBufferRef ProjectionDataRDG = nullptr;

	FRDGBufferRef DirtyPageFlagsRDG = nullptr; // Dirty flags that are cleared after render passes
	bool bHZBBuiltThisFrame = false;

	// See Engine\Shaders\Private\VirtualShadowMaps\VirtualShadowMapStats.ush for definitions of the different stat indexes
	static constexpr uint32 NumStats = 32;
	static constexpr uint32 MaxPageAreaDiagnosticSlots = 32;

	FRDGBufferRef StatsBufferRDG = nullptr;
	FRDGBufferUAVRef StatsBufferUAV = nullptr;
	FRDGBufferRef StatsNaniteBufferRDG = nullptr;

	// Debug visualization
	TArray<FRDGTextureRef> DebugVisualizationOutput;
	TArray<FVirtualShadowMapVisualizeLightSearch> VisualizeLight;
	bool bEnableVisualization = false;
	bool bEnableNaniteVisualization = false;

private:
	void UpdateVisualizeLight(
		const TConstArrayView<FViewInfo> &Views,
		const TConstArrayView<FVisibleLightInfo>& VisibleLightInfos);

	void UploadProjectionData(FRDGBuilder& GraphBuilder);

	void AppendPhysicalPageList(FRDGBuilder& GraphBuilder, bool bEmptyToAvailable);

	uint32 AddRenderViews(const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap, const FViewInfo* CullingView, float LODScaleFactor, bool bSetHzbParams, bool bUpdateHZBMetaData, TArray<Nanite::FPackedView, SceneRenderingAllocator>& OutVirtualShadowViews);

	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> GetUncachedUniformBuffer(FRDGBuilder& GraphBuilder) const;
	void UpdateCachedUniformBuffer(FRDGBuilder& GraphBuilder);
			
	// Track mapping of previous VSM data -> current frame VSM data
	// This is primarily an indirection that allows us to reallocate/repack VirtualShadowMapIds each frame
	TArray<FNextVirtualShadowMapData, SceneRenderingAllocator> NextData;

	int32 NumShadowMapSlots = 0;
	int32 NumSinglePageShadowMaps = 0;

	// Cached copy of the latest uniform parameters
	// Gets created in dummy form at initialization time, then updated after VSM data is computed
	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> CachedUniformBuffer;

	FScene &Scene;
	//
	bool bUseHzbOcclusion = true;
	bool bUseTwoPassHzbOcclusion = true;

	bool bInitialized = false;

	// Are virtual shadow maps enabled? We store this at the start of the frame to centralize the logic.
	bool bEnabled = false;

	// Is backface culling of pixels enabled? We store this here to keep it consistent between projection and generation
	bool bCullBackfacingPixels = false;
};
