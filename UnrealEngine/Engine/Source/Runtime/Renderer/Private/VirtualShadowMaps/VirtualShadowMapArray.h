// Copyright Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	VirtualShadowMapArray.h:
=============================================================================*/
#pragma once

#include "MeshDrawCommands.h"
#include "Nanite/Nanite.h"
#include "SceneTypes.h"
#include "VirtualShadowMapDefinitions.h"

struct FMinimalSceneTextures;
struct FSortedLightSetSceneInfo;
class FViewInfo;
class FProjectedShadowInfo;
class FVisibleLightInfo;
class FVirtualShadowMapCacheEntry;
class FVirtualShadowMapArrayCacheManager;
struct FSortedLightSetSceneInfo;
class FVirtualShadowMapClipmap;
struct FScreenPassTexture;

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

	FVirtualShadowMap(uint32 InID, bool bInIsSinglePageSM) : ID(InID), bIsSinglePageSM(bInIsSinglePageSM)
	{
	}

	const int32 ID = INDEX_NONE;
	const bool bIsSinglePageSM;

	TSharedPtr<FVirtualShadowMapCacheEntry> VirtualShadowMapCacheEntry;
};

// Useful data for both the page mapping shader and the projection shader
// as well as cached shadow maps
struct FVirtualShadowMapProjectionShaderData
{
	/**
	 * Transform from shadow-pre-translated world space to shadow view space, example use: (WorldSpacePos + ShadowPreViewTranslation) * TranslatedWorldToShadowViewMatrix
	 * TODO: Why don't we call it a rotation and store in a 3x3? Does it ever have translation in?
	 */
	FMatrix44f TranslatedWorldToShadowViewMatrix;
	FMatrix44f ShadowViewToClipMatrix;
	FMatrix44f TranslatedWorldToShadowUVMatrix;
	FMatrix44f TranslatedWorldToShadowUVNormalMatrix;

	FVector3f PreViewTranslationLWCTile;
	uint32 LightType = ELightComponentType::LightType_Directional;
	FVector3f PreViewTranslationLWCOffset;
	float LightSourceRadius;						// This should live in shared light structure...
	
	// TODO: There are more local lights than directional
	// We should move the directional-specific stuff out to its own structure.
	FVector3f NegativeClipmapWorldOriginLWCOffset;	// Shares the LWCTile with PreViewTranslation
	// Slightly different meaning for clipmaps (includes camera pixel size scaling stuff) and local lights (raw bias)
	float ResolutionLodBias = 0.0f;

	FIntPoint ClipmapCornerRelativeOffset = FIntPoint(0, 0);
	int32 ClipmapIndex = 0;					// 0 .. ClipmapLevelCount-1
	int32 ClipmapLevel = 0;					// "Absolute" level, can be negative

	int32 ClipmapLevelCount = 0;
	uint32 Flags = 0U;
	float LightRadius;

	// Seems the FMatrix forces 16-byte alignment
	float Padding[1];
};
static_assert((sizeof(FVirtualShadowMapProjectionShaderData) % 16) == 0, "FVirtualShadowMapProjectionShaderData size should be a multiple of 16-bytes for alignment.");

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

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ProjectionData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PageTable)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PageFlags)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, PageRectBounds)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, PhysicalPagePool)
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
	const FString GetLightName() const { return FoundProxy->GetOwnerNameOrLabel(); }

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

	void Initialize(FRDGBuilder& GraphBuilder, FVirtualShadowMapArrayCacheManager* InCacheManager, bool bInEnabled, bool bIsSceneCapture);

	// Returns true if virtual shadow maps are enabled
	bool IsEnabled() const
	{
		return bEnabled;
	}

	FVirtualShadowMap* Allocate(bool bSinglePageShadowMap);

	int32 GetNumFullShadowMaps() const
	{
		return FMath::Max(ShadowMaps.Num() - int32(VSM_MAX_SINGLE_PAGE_SHADOW_MAPS), 0);
	}

	int32 GetNumSinglePageShadowMaps() const
	{
		return NumSinglePageSms;
	}

	/**
	 * Return the total of allocated SMs, both full and single-page SMs
	 */
	int32 GetNumShadowMaps() const
	{
		// If not initialized ShadowMaps is empty, but we want it to return at most 0 anyway
		return GetNumFullShadowMaps() + GetNumSinglePageShadowMaps();
	}

	/**
	 * Get configured LOD bias for the local lights (maps to cvar, 
	 */
	float GetResolutionLODBiasLocal() const;

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

	void BuildPageAllocations(
		FRDGBuilder& GraphBuilder,
		const FMinimalSceneTextures& SceneTextures,
		const TArray<FViewInfo> &Views, 
		const FEngineShowFlags& EngineShowFlags,
		const FSortedLightSetSceneInfo& SortedLights, 
		const TArray<FVisibleLightInfo, SceneRenderingAllocator> &VisibleLightInfos, 
		const TArray<Nanite::FRasterResults, TInlineAllocator<2>> &NaniteRasterResults,
		FRDGTextureRef SingleLayerWaterDepthTexture);

	bool IsAllocated() const
	{
		return PhysicalPagePoolRDG != nullptr && PageTableRDG != nullptr;
	}

	bool ShouldCacheStaticSeparately() const
	{
		return UniformParameters.StaticCachedArrayIndex > 0;
	}

	void CreateMipViews( TArray<Nanite::FPackedView, SceneRenderingAllocator>& Views ) const;

	/**
	 * Draw Non-Nanite geometry into the VSMs.
	 */
	void RenderVirtualShadowMapsNonNanite(FRDGBuilder& GraphBuilder, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& VirtualSmMeshCommandPasses, TArrayView<FViewInfo> Views);

	void RenderDebugInfo(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views);
	
	void PrintStats(FRDGBuilder& GraphBuilder, const FViewInfo& View);

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
	uint32 AddRenderViews(const TSharedPtr<FVirtualShadowMapClipmap>& Clipmap, float LODScaleFactor, bool bSetHzbParams, bool bUpdateHZBMetaData, bool bClampToNearPlane, TArray<Nanite::FPackedView, SceneRenderingAllocator>& OutVirtualShadowViews);
	uint32 AddRenderViews(const FProjectedShadowInfo* ProjectedShadowInfo, float LODScaleFactor, bool bSetHzbParams, bool bUpdateHZBMetaData, bool bClampToNearPlane, TArray<Nanite::FPackedView, SceneRenderingAllocator>& OutVirtualShadowViews);

	// Add visualization composite pass, if enabled
	void AddVisualizePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, int32 ViewIndex, FScreenPassTexture Output);

	//
	bool UseHzbOcclusion() const { return bUseHzbOcclusion; }
	bool UseTwoPassHzbOcclusion() const { return bUseTwoPassHzbOcclusion; }

	// We keep a reference to the cache manager that was used to initialize this frame as it owns some of the buffers
	FVirtualShadowMapArrayCacheManager* CacheManager = nullptr;

	FVirtualShadowMapUniformParameters UniformParameters;

	// Physical page pool shadow data
	// NOTE: The underlying texture is owned by FVirtualShadowMapCacheManager.
	// We just import and maintain a copy of the RDG reference for this frame here.
	FRDGTextureRef PhysicalPagePoolRDG = nullptr;

	// Buffer that serves as the page table for all virtual shadow maps
	FRDGBufferRef PageTableRDG = nullptr;
		
	// Buffer that stores flags (uints) marking each page that needs to be rendered and cache status, for all virtual shadow maps.
	// Flag values defined in PageAccessCommon.ush
	FRDGBufferRef PageFlagsRDG = nullptr;

	// Allocation info for each page.
	FRDGBufferRef CachedPageInfosRDG = nullptr;
	FRDGBufferRef PhysicalPageMetaDataRDG = nullptr;

	// uint4 buffer with one rect for each mip level in all SMs, calculated to bound committed pages
	// Used to clip the rect size of clusters during culling.
	FRDGBufferRef PageRectBoundsRDG = nullptr;
	FRDGBufferRef AllocatedPageRectBoundsRDG = nullptr;
	FRDGBufferRef ProjectionDataRDG = nullptr;

	FRDGBufferRef DirtyPageFlagsRDG = nullptr; // Dirty flags that are cleared after render passes
	bool bHZBBuiltThisFrame = false;

	FRDGBufferRef StaticInvalidatingPrimitivesRDG = nullptr;

	FRDGTextureRef HZBPhysical = nullptr;
	TMap<int32, FVirtualShadowMapHZBMetadata> HZBMetadata;

	// See Engine\Shaders\Private\VirtualShadowMaps\VirtualShadowMapStats.ush for definitions of the different stat indexes
	static constexpr uint32 NumStats = 16;

	FRDGBufferRef StatsBufferRDG = nullptr;

	// Debug visualization
	TArray<FRDGTextureRef> DebugVisualizationOutput;
	TArray<FVirtualShadowMapVisualizeLightSearch> VisualizeLight;

private:
	TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> GetUncachedUniformBuffer(FRDGBuilder& GraphBuilder) const;
	void UpdateCachedUniformBuffer(FRDGBuilder& GraphBuilder);

	TArray<TUniquePtr<FVirtualShadowMap>, SceneRenderingAllocator> ShadowMaps;
	int32 NumSinglePageSms = 0;

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
