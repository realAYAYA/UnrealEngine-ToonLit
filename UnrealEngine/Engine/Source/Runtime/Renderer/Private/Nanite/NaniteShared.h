// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Rendering/NaniteResources.h"
#include "NaniteFeedback.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "Misc/ScopeRWLock.h"
#include "Experimental/Containers/RobinHoodHashTable.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNanite, Warning, All);

DECLARE_GPU_STAT_NAMED_EXTERN(NaniteDebug, TEXT("Nanite Debug"));

struct FSceneTextures;
struct FDBufferTextures;

namespace Nanite
{

struct FPackedView
{
	FMatrix44f	SVPositionToTranslatedWorld;
	FMatrix44f	ViewToTranslatedWorld;

	FMatrix44f	TranslatedWorldToView;
	FMatrix44f	TranslatedWorldToClip;
	FMatrix44f	TranslatedWorldToSubpixelClip;
	FMatrix44f	ViewToClip;
	FMatrix44f	ClipToRelativeWorld;

	FMatrix44f	PrevTranslatedWorldToView;
	FMatrix44f	PrevTranslatedWorldToClip;
	FMatrix44f	PrevViewToClip;
	FMatrix44f	PrevClipToRelativeWorld;

	FIntVector4	ViewRect;
	FVector4f	ViewSizeAndInvSize;
	FVector4f	ClipSpaceScaleOffset;
	FVector4f	PreViewTranslation;
	FVector4f	PrevPreViewTranslation;
	FVector4f	WorldCameraOrigin;
	FVector4f	ViewForwardAndNearPlane;

	FVector3f	ViewTilePosition;
	float		RangeBasedCullingDistance;

	FVector3f	MatrixTilePosition;
	uint32		Padding1;

	FVector2f	LODScales;
	float		MinBoundsRadiusSq;
	uint32		StreamingPriorityCategory_AndFlags;

	FIntVector4 TargetLayerIdX_AndMipLevelY_AndNumMipLevelsZ;

	FIntVector4	HZBTestViewRect;	// In full resolution

	/**
	 * Calculates the LOD scales assuming view size and projection is already set up.
	 * TODO: perhaps more elegant/robust if this happened at construction time, and input was a non-packed NaniteView.
	 * Note: depends on the global 'GNaniteMaxPixelsPerEdge'.
	 */
	void UpdateLODScales();


	/**
	 * Helper to compute the derived subpixel transform.
	 */
	static FMatrix44f CalcTranslatedWorldToSubpixelClip(const FMatrix44f& TranslatedWorldToClip, const FIntRect& ViewRect);
};

struct FPackedViewParams
{
	FViewMatrices ViewMatrices;
	FViewMatrices PrevViewMatrices;
	FIntRect ViewRect;
	FIntPoint RasterContextSize;
	uint32 StreamingPriorityCategory = 0;
	float MinBoundsRadius = 0.0f;
	float LODScaleFactor = 1.0f;
	uint32 Flags = NANITE_VIEW_FLAG_NEAR_CLIP;

	int32 TargetLayerIndex = 0;
	int32 PrevTargetLayerIndex = INDEX_NONE;
	int32 TargetMipLevel = 0;
	int32 TargetMipCount = 1;

	float RangeBasedCullingDistance = 0.0f; // not used unless the flag NANITE_VIEW_FLAG_DISTANCE_CULL is set

	FIntRect HZBTestViewRect = {0, 0, 0, 0};
};

FPackedView CreatePackedView(const FPackedViewParams& Params);

// Convenience function to pull relevant packed view parameters out of a FViewInfo
FPackedView CreatePackedViewFromViewInfo(
	const FViewInfo& View,
	FIntPoint RasterContextSize,
	uint32 Flags,
	uint32 StreamingPriorityCategory = 0,
	float MinBoundsRadius = 0.0f,
	float LODScaleFactor = 1.0f,
	/** Note: this rect should be in HZB space. */
	const FIntRect* InHZBTestViewRect = nullptr
);

struct FVisualizeResult
{
	FRDGTextureRef ModeOutput;
	FName ModeName;
	int32 ModeID;
	uint8 bCompositeScene : 1;
	uint8 bSkippedTile    : 1;
};

struct FRasterState
{
	bool bReverseCulling = false;
};

struct FBinningData
{
	uint32 BinCount = 0;

	FRDGBufferRef DataBuffer = nullptr;
	FRDGBufferRef HeaderBuffer = nullptr;
	FRDGBufferRef IndirectArgs = nullptr;
};

/*
 * GPU side buffers containing Nanite resource data.
 */
class FGlobalResources : public FRenderResource
{
public:
	struct PassBuffers
	{
		// Used for statistics
		TRefCountPtr<FRDGPooledBuffer> StatsRasterizeArgsSWHWBuffer;
	};

	// Used for statistics
	uint32 StatsRenderFlags = 0;
	uint32 StatsDebugFlags = 0;

	const int32 MaxPickingBuffers = 4;
	int32 PickingBufferWriteIndex = 0;
	int32 PickingBufferNumPending = 0;
	TArray<FRHIGPUBufferReadback*> PickingBuffers;

public:
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	void	Update(FRDGBuilder& GraphBuilder); // Called once per frame before any Nanite rendering has occurred.

	static uint32 GetMaxCandidateClusters();
	static uint32 GetMaxClusterBatches();
	static uint32 GetMaxVisibleClusters();
	static uint32 GetMaxNodes();

	inline PassBuffers& GetMainPassBuffers() { return MainPassBuffers; }
	inline PassBuffers& GetPostPassBuffers() { return PostPassBuffers; }

	TRefCountPtr<FRDGPooledBuffer>& GetMainAndPostNodesAndClusterBatchesBuffer() { return MainAndPostNodesAndClusterBatchesBuffer; };

	TRefCountPtr<FRDGPooledBuffer>& GetStatsBufferRef() { return StatsBuffer; }

#if !UE_BUILD_SHIPPING
	FFeedbackManager* GetFeedbackManager() { return FeedbackManager; }
#endif
private:
	PassBuffers MainPassBuffers;
	PassBuffers PostPassBuffers;

	TRefCountPtr<FRDGPooledBuffer> MainAndPostNodesAndClusterBatchesBuffer;

	// Used for statistics
	TRefCountPtr<FRDGPooledBuffer> StatsBuffer;

#if !UE_BUILD_SHIPPING
	FFeedbackManager* FeedbackManager = nullptr;
#endif
};

extern TGlobalResource< FGlobalResources > GGlobalResources;

} // namespace Nanite

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNaniteUniformParameters, )
	SHADER_PARAMETER(FIntVector4,					PageConstants)
	SHADER_PARAMETER(FIntVector4,					MaterialConfig) // .x mode, .yz grid size, .w unused
	SHADER_PARAMETER(uint32,						MaxNodes)
	SHADER_PARAMETER(uint32,						MaxVisibleClusters)
	SHADER_PARAMETER(uint32,						RenderFlags)
	SHADER_PARAMETER(float,							RayTracingCutError)
	SHADER_PARAMETER(FVector4f,						RectScaleOffset) // xy: scale, zw: offset

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer,		ClusterPageData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer,		VisibleClustersSWHW)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer,		HierarchyBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaterialTileRemap)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>,		VisBuffer64)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>,		DbgBuffer64)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>,			DbgBuffer32)

	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, RayTracingDataBuffer)

	// Multi view
	SHADER_PARAMETER(uint32,												MultiViewEnabled)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>,					MultiViewIndices)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>,				MultiViewRectScaleOffsets)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedNaniteView>,	InViews)
END_SHADER_PARAMETER_STRUCT()

extern TRDGUniformBufferRef<FNaniteUniformParameters> CreateDebugNaniteUniformBuffer(FRDGBuilder& GraphBuilder, uint32 InstanceSceneDataSOAStride);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNaniteRayTracingUniformParameters, )
	SHADER_PARAMETER(FIntVector4,	PageConstants)
	SHADER_PARAMETER(uint32,		MaxNodes)
	SHADER_PARAMETER(uint32,		MaxVisibleClusters)
	SHADER_PARAMETER(uint32,		RenderFlags)
	SHADER_PARAMETER(float,			RayTracingCutError)

	SHADER_PARAMETER_SRV(ByteAddressBuffer, ClusterPageData)
	SHADER_PARAMETER_SRV(ByteAddressBuffer, HierarchyBuffer)

	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, RayTracingDataBuffer)
END_SHADER_PARAMETER_STRUCT()

class FNaniteGlobalShader : public FGlobalShader
{
public:
	FNaniteGlobalShader() = default;
	FNaniteGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
	{
	}
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);

		// Force shader model 6.0+
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	}
};

class FNaniteMaterialShader : public FMaterialShader
{
public:
	FNaniteMaterialShader() = default;
	FNaniteMaterialShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
	{
	}

	static bool RequiresProgrammableVertex(const FMaterialShaderPermutationParameters& Parameters)
	{
		return Parameters.MaterialParameters.bHasVertexPositionOffsetConnected;
	}

	static bool RequiresProgrammablePixel(const FMaterialShaderPermutationParameters& Parameters)
	{
		const bool bProgrammablePixel =
		(
			Parameters.MaterialParameters.bIsMasked || 
			Parameters.MaterialParameters.bHasPixelDepthOffsetConnected
		);

		return bProgrammablePixel;
	}

	static bool ShouldCompilePixelPermutation(const FMaterialShaderPermutationParameters& Parameters, bool bProgrammableRaster)
	{
		// Always compile default material as the fast opaque "fixed function" raster path
		bool bValidMaterial = Parameters.MaterialParameters.bIsDefaultMaterial;

		// Compile this pixel shader if it requires programmable raster and it's enabled
		if (bProgrammableRaster && Parameters.MaterialParameters.bIsUsedWithNanite && RequiresProgrammablePixel(Parameters))
		{
			bValidMaterial = true;
		}

		return
			DoesPlatformSupportNanite(Parameters.Platform) &&
			Parameters.MaterialParameters.MaterialDomain == MD_Surface &&
			bValidMaterial;
	}
	
	static bool ShouldCompileVertexPermutation(const FMaterialShaderPermutationParameters& Parameters, bool bProgrammableRaster)
	{
		// Always compile default material as the fast opaque "fixed function" raster path
		bool bValidMaterial = Parameters.MaterialParameters.bIsDefaultMaterial;

		// Compile this vertex shader if it requires programmable raster and it's enabled
		if (bProgrammableRaster && Parameters.MaterialParameters.bIsUsedWithNanite && RequiresProgrammableVertex(Parameters))
		{
			bValidMaterial = true;
		}

		return
			DoesPlatformSupportNanite(Parameters.Platform) &&
			Parameters.MaterialParameters.MaterialDomain == MD_Surface &&
			bValidMaterial;
	}
	
	static bool ShouldCompileComputePermutation(const FMaterialShaderPermutationParameters& Parameters, bool bProgrammableRaster)
	{
		// Always compile default material as the fast opaque "fixed function" raster path
		bool bValidMaterial = Parameters.MaterialParameters.bIsDefaultMaterial;

		// Compile this compute shader if it requires programmable raster and it's enabled
		if (bProgrammableRaster && Parameters.MaterialParameters.bIsUsedWithNanite && (RequiresProgrammableVertex(Parameters) || RequiresProgrammablePixel(Parameters)))
		{
			bValidMaterial = true;
		}

		return
			DoesPlatformSupportNanite(Parameters.Platform) &&
			Parameters.MaterialParameters.MaterialDomain == MD_Surface &&
			bValidMaterial;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Force shader model 6.0+
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NANITE_MATERIAL_SHADER"), 1);

		// Get data from GPUSceneParameters rather than View.
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		OutEnvironment.SetDefine(TEXT("IS_NANITE_RASTER_PASS"), 1);
		OutEnvironment.SetDefine(TEXT("IS_NANITE_PASS"), 1);

		OutEnvironment.SetDefine(TEXT("NANITE_USE_UNIFORM_BUFFER"), 0);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 0);

		// Force definitions of GetObjectWorldPosition(), etc..
		OutEnvironment.SetDefine(TEXT("HAS_PRIMITIVE_UNIFORM_BUFFER"), 1);
	}
};

class FMaterialRenderProxy;

struct FNaniteRasterPipeline
{
	const FMaterialRenderProxy* RasterMaterial = nullptr;
	bool bIsTwoSided = false;
	bool bPerPixelEval = false;
	bool bForceDisableWPO = false;
	bool bWPODisableDistance = false;

	static FNaniteRasterPipeline GetFixedFunctionPipeline(bool bIsTwoSided);

	inline uint32 GetPipelineHash() const
	{
		struct FHashKey
		{
			uint32 MaterialFlags;
			uint32 MaterialHash;

			static inline uint32 PointerHash(const void* Key)
			{
			#if PLATFORM_64BITS
				// Ignoring the lower 4 bits since they are likely zero anyway.
				// Higher bits are more significant in 64 bit builds.
				return reinterpret_cast<UPTRINT>(Key) >> 4;
			#else
				return reinterpret_cast<UPTRINT>(Key);
			#endif
			};

		} HashKey;

		HashKey.MaterialFlags  = 0;
		HashKey.MaterialFlags |= bIsTwoSided ? 0x1u : 0x0u;
		HashKey.MaterialFlags |= bForceDisableWPO ? 0x2u : 0x0u;
		HashKey.MaterialHash   = FHashKey::PointerHash(RasterMaterial);
		return uint32(CityHash64((char*)&HashKey, sizeof(FHashKey)));
	}

	inline bool GetSecondaryPipeline(FNaniteRasterPipeline& OutSecondary) const
	{
		if (bWPODisableDistance)
		{
			if (bPerPixelEval)
			{
				// The secondary bin must still be a programmable bin, but with WPO force disabled.
				OutSecondary = *this;
				OutSecondary.bWPODisableDistance = false;
				OutSecondary.bForceDisableWPO = true;
			}
			else
			{
				// The secondary bin can be a non-programmable, fixed-function bin
				OutSecondary = GetFixedFunctionPipeline(bIsTwoSided);
			}
			return true;
		}

		return false;
	}

	FORCENOINLINE friend uint32 GetTypeHash(const FNaniteRasterPipeline& Other)
	{
		return Other.GetPipelineHash();
	}
};

struct FNaniteRasterBin
{
	int32  BinId = INDEX_NONE;
	uint16 BinIndex = 0xFFFFu;

	inline bool operator==(const FNaniteRasterBin& Other) const
	{
		return Other.BinId == BinId && Other.BinIndex == BinIndex;
	}
	
	inline bool operator!=(const FNaniteRasterBin& Other) const
	{
		return !(*this == Other);
	}

	inline bool IsValid() const
	{
		return *this != FNaniteRasterBin();
	}
};

struct FNaniteRasterEntry
{
	FNaniteRasterPipeline RasterPipeline{};
	uint32 ReferenceCount = 0;
	uint16 BinIndex = 0xFFFFu;
	bool bForceDisableWPO = false;
};

struct FNaniteRasterEntryKeyFuncs : TDefaultMapHashableKeyFuncs<FNaniteRasterPipeline, FNaniteRasterEntry, false>
{
	static inline bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.GetPipelineHash() == B.GetPipelineHash();
	}

	static inline uint32 GetKeyHash(KeyInitType Key)
	{
		return Key.GetPipelineHash();
	}
};

using FNaniteRasterPipelineMap = Experimental::TRobinHoodHashMap<FNaniteRasterPipeline, FNaniteRasterEntry, FNaniteRasterEntryKeyFuncs>;

class FNaniteRasterBinIndexTranslator
{
public:
	FNaniteRasterBinIndexTranslator() = default;

	uint16 Translate(uint16 BinIndex) const
	{
		return BinIndex < RegularBinCount ? BinIndex : RevertBinIndex(BinIndex) + RegularBinCount;
	}

private:
	friend class FNaniteRasterPipelines;

	uint32 RegularBinCount;

	FNaniteRasterBinIndexTranslator(uint32 InRegularBinCount)
		: RegularBinCount(InRegularBinCount)
	{}

	static uint16 RevertBinIndex(uint16 BinIndex)
	{
		return MAX_uint16 - BinIndex;
	}
};

class FNaniteRasterPipelines
{
public:
	typedef Experimental::FHashType FRasterHash;
	typedef Experimental::FHashElementId FRasterId;

public:
	FNaniteRasterPipelines();
	~FNaniteRasterPipelines();

	uint16 AllocateBin(bool bPerPixelEval);
	void ReleaseBin(uint16 BinIndex);

	bool IsBinAllocated(uint16 BinIndex) const;

	uint32 GetRegularBinCount() const;
	uint32 GetBinCount() const;

	FNaniteRasterBin Register(const FNaniteRasterPipeline& InRasterPipeline);
	void Unregister(const FNaniteRasterBin& InRasterBin);

	inline const FNaniteRasterPipelineMap& GetRasterPipelineMap() const
	{
		return PipelineMap;
	}

	inline FNaniteRasterBinIndexTranslator GetBinIndexTranslator() const
	{
		return FNaniteRasterBinIndexTranslator(GetRegularBinCount());
	}

private:
	TBitArray<> PipelineBins;
	TBitArray<> PerPixelEvalPipelineBins;
	FNaniteRasterPipelineMap PipelineMap;
};

/// TODO: Work in progress / experimental

struct FNaniteShadingBin
{
	int32  BinId = INDEX_NONE;
	uint16 BinIndex = 0xFFFFu;

	inline bool operator==(const FNaniteShadingBin& Other) const
	{
		return Other.BinId == BinId && Other.BinIndex == BinIndex;
	}

	inline bool operator!=(const FNaniteShadingBin& Other) const
	{
		return !(*this == Other);
	}

	inline bool IsValid() const
	{
		return *this != FNaniteShadingBin();
	}
};

struct FNaniteShadingPipeline
{
	const FMaterialRenderProxy* ShadingMaterial = nullptr;
	bool bIsTwoSided = false;
	bool bIsMasked = false;

	inline uint32 GetPipelineHash() const
	{
		struct FHashKey
		{
			uint32 MaterialFlags;
			uint32 MaterialHash;

			static inline uint32 PointerHash(const void* Key)
			{
			#if PLATFORM_64BITS
				// Ignoring the lower 4 bits since they are likely zero anyway.
				// Higher bits are more significant in 64 bit builds.
				return reinterpret_cast<UPTRINT>(Key) >> 4;
			#else
				return reinterpret_cast<UPTRINT>(Key);
			#endif
			};

		} HashKey;

		HashKey.MaterialFlags  = 0;
		HashKey.MaterialFlags |= bIsTwoSided ? 0x1u : 0x0u;
		HashKey.MaterialHash   = FHashKey::PointerHash(ShadingMaterial);
		return uint32(CityHash64((char*)&HashKey, sizeof(FHashKey)));
	}

	FORCENOINLINE friend uint32 GetTypeHash(const FNaniteShadingPipeline& Other)
	{
		return Other.GetPipelineHash();
	}
};

struct FNaniteShadingEntry
{
	FNaniteShadingPipeline ShadingPipeline{};
	uint32 ReferenceCount = 0;
	uint16 BinIndex = 0xFFFFu;
};

struct FNaniteShadingEntryKeyFuncs : TDefaultMapHashableKeyFuncs<FNaniteShadingPipeline, FNaniteShadingEntry, false>
{
	static inline bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.GetPipelineHash() == B.GetPipelineHash();
	}

	static inline uint32 GetKeyHash(KeyInitType Key)
	{
		return Key.GetPipelineHash();
	}
};

using FNaniteShadingPipelineMap = Experimental::TRobinHoodHashMap<FNaniteShadingPipeline, FNaniteShadingEntry, FNaniteShadingEntryKeyFuncs>;

class FNaniteShadingPipelines
{
public:
	typedef Experimental::FHashType FShadingHash;
	typedef Experimental::FHashElementId FShadingId;

public:
	FNaniteShadingPipelines();
	~FNaniteShadingPipelines();

	uint16 AllocateBin();
	void ReleaseBin(uint16 BinIndex);

	bool IsBinAllocated(uint16 BinIndex) const;

	uint32 GetBinCount() const;

	FNaniteShadingBin Register(const FNaniteShadingPipeline& InShadingPipeline);
	void Unregister(const FNaniteShadingBin& InShadingBin);

	inline const FNaniteShadingPipelineMap& GetShadingPipelineMap() const
	{
		return PipelineMap;
	}

private:
	TBitArray<> PipelineBins;
	FNaniteShadingPipelineMap PipelineMap;
};

/// END-TODO: Work in progress / experimental

struct FNaniteVisibilityQuery;

class FNaniteVisibilityResults
{
	friend class FNaniteVisibility;

public:
	FNaniteVisibilityResults() = default;

	FNaniteVisibilityResults(const FNaniteVisibilityResults& Other)
	: RasterBinVisibility(Other.RasterBinVisibility)
	, ShadingDrawVisibility(Other.ShadingDrawVisibility)
	, BinIndexTranslator(Other.BinIndexTranslator)
	, TotalRasterBins(Other.TotalRasterBins)
	, TotalShadingDraws(Other.TotalShadingDraws)
	, VisibleRasterBins(Other.VisibleRasterBins)
	, VisibleShadingDraws(Other.VisibleShadingDraws)
	, bRasterTestValid(Other.bRasterTestValid)
	, bShadingTestValid(Other.bShadingTestValid)
	{
	}

	bool IsRasterBinVisible(uint16 BinIndex) const;
	bool IsShadingDrawVisible(uint32 DrawId) const;

	void Invalidate();

	FORCEINLINE bool IsRasterTestValid() const
	{
		return bRasterTestValid;
	}

	FORCEINLINE bool IsShadingTestValid() const
	{
		return bShadingTestValid;
	}

	FORCEINLINE void GetRasterBinStats(uint32& OutNumVisible, uint32& OutNumTotal) const
	{
		OutNumTotal = TotalRasterBins;
		OutNumVisible = IsRasterTestValid() ? VisibleRasterBins : OutNumTotal;
	}

	FORCEINLINE void GetShadingDrawStats(uint32& OutNumVisible, uint32& OutNumTotal) const
	{
		OutNumTotal = TotalShadingDraws;
		OutNumVisible = IsShadingTestValid() ? VisibleShadingDraws : OutNumTotal;
	}

	void SetRasterBinIndexTranslator(const FNaniteRasterBinIndexTranslator InTranslator)
	{
		BinIndexTranslator = InTranslator;
	}

private:
	TBitArray<> RasterBinVisibility;
	TArray<uint32> ShadingDrawVisibility;
	FNaniteRasterBinIndexTranslator BinIndexTranslator;
	uint32 TotalRasterBins		= 0;
	uint32 TotalShadingDraws	= 0;
	uint32 VisibleRasterBins	= 0;
	uint32 VisibleShadingDraws	= 0;
	bool bRasterTestValid		= false;
	bool bShadingTestValid		= false;
};

class FNaniteVisibility
{
	friend class FNaniteVisibilityTask;

public:
	struct FPrimitiveBins
	{
		uint16 Primary = 0xFFFFu;
		uint16 Secondary = 0xFFFFu;
	};

	typedef TArray<FPrimitiveBins, TInlineAllocator<1>> PrimitiveBinsType;
	typedef TArray<uint32, TInlineAllocator<1>> PrimitiveDrawType;

	struct FPrimitiveReferences
	{
		const FPrimitiveSceneInfo* SceneInfo = nullptr;
		PrimitiveBinsType RasterBins;
		PrimitiveDrawType ShadingDraws;
	};

	typedef TMap<const FPrimitiveSceneInfo*, FPrimitiveReferences> PrimitiveMapType;

public:
	FNaniteVisibility();

	void BeginVisibilityFrame(const FNaniteRasterBinIndexTranslator InTranslator);
	void FinishVisibilityFrame();

	FNaniteVisibilityQuery* BeginVisibilityQuery(
		const TConstArrayView<FConvexVolume>& ViewList,
		const class FNaniteRasterPipelines* RasterPipelines,
		const class FNaniteMaterialCommands* MaterialCommands = nullptr
	);

	void FinishVisibilityQuery(FNaniteVisibilityQuery* Query, FNaniteVisibilityResults& OutResults) const;

	PrimitiveBinsType& GetRasterBinReferences(const FPrimitiveSceneInfo* SceneInfo);
	PrimitiveDrawType& GetShadingDrawReferences(const FPrimitiveSceneInfo* SceneInfo);
	void RemoveReferences(const FPrimitiveSceneInfo* SceneInfo);

private:
	// Translator should remain valid between Begin/FinishVisibilityFrame. That is, no adding or removing raster bins
	FNaniteRasterBinIndexTranslator BinIndexTranslator;
	TArray<FNaniteVisibilityQuery*, TInlineAllocator<32>> VisibilityQueries;
	PrimitiveMapType PrimitiveReferences;
	TArray<FPrimitiveReferences, SceneRenderingAllocator> CapturedPrimitiveReferences;
	uint8 bCalledBegin : 1;
};

class FNaniteScopedVisibilityFrame
{
public:
	FNaniteScopedVisibilityFrame(const bool bInEnabled, FNaniteVisibility& InVisibility, const FNaniteRasterBinIndexTranslator InTranslator)
	: Visibility(InVisibility)
	, bEnabled(bInEnabled)
	{
		if (bEnabled)
		{
			Visibility.BeginVisibilityFrame(InTranslator);
		}
	}

	~FNaniteScopedVisibilityFrame()
	{
		if (bEnabled)
		{
			Visibility.FinishVisibilityFrame();
		}
	}

	FORCEINLINE FNaniteVisibility& Get()
	{
		return Visibility;
	}

private:
	FNaniteVisibility& Visibility;
	bool bEnabled;
};

extern bool ShouldRenderNanite(const FScene* Scene, const FViewInfo& View, bool bCheckForAtomicSupport = true);

/** Checks whether Nanite would be rendered in this view. Used to give a visual warning about the project settings that can disable Nanite. */
extern bool WouldRenderNanite(const FScene* Scene, const FViewInfo& View, bool bCheckForAtomicSupport = true, bool bCheckForProjectSetting = true);

extern bool UseComputeDepthExport();
