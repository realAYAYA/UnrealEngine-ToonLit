// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessorManager.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "Engine/EngineTypes.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "Experimental/Containers/RobinHoodHashTable.h"

struct FSceneTexturesConfig;
class FMaterial;
class FVertexFactoryType;
class FGraphicsPipelineStateInitializer;
enum class EVertexInputStreamType : uint8;

#define PSO_PRECACHING_VALIDATE (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

/**
 * Parameters which are needed to collect all possible PSOs used by the PSO collectors
 */
struct FPSOPrecacheParams
{
	FPSOPrecacheParams()
	{
		PrimitiveType = (uint8)PT_TriangleList;
		bDefaultMaterial = false;
		bRenderInMainPass = true;
		bRenderInDepthPass = true;
		bStaticLighting = true;
		bCastShadow = true;
		bRenderCustomDepth = false;
		bAffectDynamicIndirectLighting = true;
		bReverseCulling = false;
		bDisableBackFaceCulling = false;
		bCastShadowAsTwoSided = false;
		bForceLODModel = false;
		Mobility = (uint8)EComponentMobility::Static;
		bHasWorldPositionOffsetVelocity = false;
		StencilWriteMask = (uint8)EStencilMask::SM_Default;
		Unused = 0;
	}

	bool operator==(const FPSOPrecacheParams& Other) const
	{
		return Data == Other.Data;
	}

	bool operator!=(const FPSOPrecacheParams& rhs) const
	{
		return !(*this == rhs);
	}

	friend uint32 GetTypeHash(const FPSOPrecacheParams& Params)
	{
		return uint32(Params.Data);
	}

	void SetMobility(EComponentMobility::Type InMobility)
	{
		Mobility = (uint8)InMobility;
	}

	TEnumAsByte<EComponentMobility::Type> GetMobility() const
	{
		return TEnumAsByte<EComponentMobility::Type>((EComponentMobility::Type)Mobility);
	}

	bool IsMoveable() const
	{
		return Mobility == EComponentMobility::Movable || Mobility == EComponentMobility::Stationary;
	}

	void SetStencilWriteMask(EStencilMask InStencilMask)
	{
		StencilWriteMask = (uint8)InStencilMask;
	}

	TEnumAsByte<EStencilMask> GetStencilWriteMask() const
	{
		return TEnumAsByte<EStencilMask>((EStencilMask)StencilWriteMask);
	}

	union
	{
		struct
		{
			uint32 PrimitiveType : 6;

			uint32 bDefaultMaterial : 1;

			uint32 bRenderInMainPass : 1;
			uint32 bRenderInDepthPass : 1;
			uint32 bStaticLighting : 1;
			uint32 bCastShadow : 1;
			uint32 bRenderCustomDepth : 1;

			uint32 bAffectDynamicIndirectLighting : 1;
			uint32 bReverseCulling : 1;
			uint32 bDisableBackFaceCulling : 1;
			uint32 bCastShadowAsTwoSided : 1;
			uint32 bForceLODModel : 1;

			uint32 Mobility : 4;
			uint32 bHasWorldPositionOffsetVelocity : 1;
			uint32 StencilWriteMask : 4;

			uint32 Unused : 6;
		};
		uint32 Data;
	};
};

// Unique ID to find the FVertexDeclarationElementList - these can be shared
using FVertexDeclarationElementListID = uint16;

/**
 * PSO Precache request priority
 */
enum class EPSOPrecachePriority : uint8
{
	Medium,
	High
};

/**
 * Wraps vertex factory data used during PSO precaching - optional element list ID can be used if manual vertex fetch is not possible for the given vertex factory type
 */
struct FPSOPrecacheVertexFactoryData 
{
	FPSOPrecacheVertexFactoryData() = default;
	FPSOPrecacheVertexFactoryData(const FVertexFactoryType* InVertexFactoryType) : VertexFactoryType(InVertexFactoryType), CustomDefaultVertexDeclaration(nullptr) {}
	FPSOPrecacheVertexFactoryData(const FVertexFactoryType* InVertexFactoryType, const FVertexDeclarationElementList& ElementList);

	const FVertexFactoryType* VertexFactoryType = nullptr;

	// Custom vertex declaration used for EVertexInputStreamType::Default if provided - the others are directly retrieved from the type if needed
	FRHIVertexDeclaration* CustomDefaultVertexDeclaration = nullptr;

	bool operator==(const FPSOPrecacheVertexFactoryData& Other) const
	{
		return VertexFactoryType == Other.VertexFactoryType && CustomDefaultVertexDeclaration == Other.CustomDefaultVertexDeclaration;
	}

	bool operator!=(const FPSOPrecacheVertexFactoryData& rhs) const
	{
		return !(*this == rhs);
	}

	friend uint32 GetTypeHash(const FPSOPrecacheVertexFactoryData& Params)
	{
		return HashCombine(PointerHash(Params.VertexFactoryType), PointerHash(Params.CustomDefaultVertexDeclaration));
	}
};

typedef TArray<FPSOPrecacheVertexFactoryData, TInlineAllocator<2> > FPSOPrecacheVertexFactoryDataList;

struct FPSOPrecacheVertexFactoryDataPerMaterialIndex
{
	int16 MaterialIndex;
	FPSOPrecacheVertexFactoryDataList VertexFactoryDataList;
};

typedef TArray<FPSOPrecacheVertexFactoryDataPerMaterialIndex, TInlineAllocator<4> > FPSOPrecacheVertexFactoryDataPerMaterialIndexList;

/**
 * Wrapper class around the initializer to collect some extra validation data during PSO collection on the different collectors
 */
struct FPSOPrecacheData
{
	FPSOPrecacheData() : Type(EType::Graphics), bRequired(true)
#if PSO_PRECACHING_VALIDATE
		, MeshPassType(0)
		, VertexFactoryType(nullptr)
#endif // PSO_PRECACHING_VALIDATE
	{
	}

	enum class EType : uint8
	{
		Graphics,
		Compute,
	};
	EType Type;

	// Is the PSO required to be able render the object or can it provide a fallback path
	// (proxy creation won't wait for these PSOs if enabled)
	bool bRequired;

	union
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInitializer;
		FRHIComputeShader* ComputeShader;
	};
	
#if PSO_PRECACHING_VALIDATE
	uint32 MeshPassType;
	const FVertexFactoryType* VertexFactoryType;
#endif // PSO_PRECACHING_VALIDATE
};

typedef TArray<FPSOPrecacheRequestResult, TInlineAllocator<4> > FPSOPrecacheRequestResultArray;

/**
 * Interface class implemented by the mesh pass processor to collect all possible PSOs
 */
class IPSOCollector
{
public:
	virtual ~IPSOCollector() {}
	
	UE_DEPRECATED(5.2, "Call CollectPSOInitializers with FPSOPrecacheVertexFactoryData instead.")
	void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
	{
		FPSOPrecacheVertexFactoryData VertexFactoryData;
		VertexFactoryData.VertexFactoryType = VertexFactoryType;
		return CollectPSOInitializers(SceneTexturesConfig, Material, VertexFactoryData, PreCacheParams, PSOInitializers);
	}

	// Collect all PSO for given material, vertex factory & params
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) = 0;
};

/**
 * Precaching PSOs for components?
 */
extern ENGINE_API bool IsComponentPSOPrecachingEnabled();

/**
 * Precaching PSOs for resources?
 */
extern ENGINE_API bool IsResourcePSOPrecachingEnabled();

/**
 * Delay component proxy creation when it's requested PSOs are still precaching
 */
extern ENGINE_API bool ProxyCreationWhenPSOReady();

/**
 * Try and create PSOs for all the given initializers and return an optional array of graph events of async compiling PSOs
 */
extern FPSOPrecacheRequestResultArray PrecachePSOs(const TArray<FPSOPrecacheData>& PSOInitializers);

/**
 * Predeclared IPSOCollector create function
 */
typedef IPSOCollector* (*PSOCollectorCreateFunction)(ERHIFeatureLevel::Type InFeatureLevel);

/**
 * Manages all create functions of the globally defined IPSOCollectors
 */
class ENGINE_API FPSOCollectorCreateManager
{
public:

	constexpr static uint32 MaxPSOCollectorCount = 33;

	static PSOCollectorCreateFunction GetCreateFunction(EShadingPath ShadingPath, uint32 Index)
	{
		check(Index < MaxPSOCollectorCount);
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		return JumpTable[ShadingPathIdx][Index];
	}

private:

	// Have to used fixed size array instead of TArray because of order of initialization of static member variables
	static PSOCollectorCreateFunction JumpTable[(uint32)EShadingPath::Num][MaxPSOCollectorCount];
	friend class FRegisterPSOCollectorCreateFunction;
};

/**
 * Helper class used to register/unregister the IPSOCollector to the manager at static startup time
 */
class ENGINE_API FRegisterPSOCollectorCreateFunction
{
public:
	FRegisterPSOCollectorCreateFunction(PSOCollectorCreateFunction InCreateFunction, EShadingPath InShadingPath, uint32 InIndex)
		: ShadingPath(InShadingPath)
		, Index(InIndex)
	{
		check(InIndex < FPSOCollectorCreateManager::MaxPSOCollectorCount);
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		FPSOCollectorCreateManager::JumpTable[ShadingPathIdx][Index] = InCreateFunction;
	}

	~FRegisterPSOCollectorCreateFunction()
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		FPSOCollectorCreateManager::JumpTable[ShadingPathIdx][Index] = nullptr;
	}

private:
	EShadingPath ShadingPath;
	uint32 Index;
};

// Unique request ID of MaterialPSOPrecache which can be used to boost the priority of a PSO precache requests if it's needed for rendering
using FMaterialPSOPrecacheRequestID = uint32;

struct FMaterialPSOPrecacheParams
{
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	const FMaterial* Material = nullptr;
	FPSOPrecacheVertexFactoryData VertexFactoryData;
	FPSOPrecacheParams PrecachePSOParams;

	bool operator==(const FMaterialPSOPrecacheParams& Other) const
	{
		return FeatureLevel == Other.FeatureLevel && 
			Material == Other.Material &&
			VertexFactoryData == Other.VertexFactoryData &&
			PrecachePSOParams == Other.PrecachePSOParams;
	}

	bool operator!=(const FMaterialPSOPrecacheParams& rhs) const
	{
		return !(*this == rhs);
	}

	friend uint32 GetTypeHash(const FMaterialPSOPrecacheParams& Params)
	{
		return HashCombine(GetTypeHash(Params.FeatureLevel), HashCombine(PointerHash(Params.Material),
			HashCombine(GetTypeHash(Params.VertexFactoryData), GetTypeHash(Params.PrecachePSOParams))));
	}
};

/**
 * Precache all PSOs for given material and parameters
 */
extern ENGINE_API FMaterialPSOPrecacheRequestID PrecacheMaterialPSOs(const FMaterialPSOPrecacheParams& MaterialPSOPrecacheParams, EPSOPrecachePriority Priority, FGraphEventArray& GraphEvents);

/**
 * Release PSO material request data
 */
extern ENGINE_API void ReleasePSOPrecacheData(const TArray<FMaterialPSOPrecacheRequestID>& MaterialPSORequestIDs);

/**
 * Boost priority for all the PSOs still compiling for the request material request IDs
 */
extern ENGINE_API void BoostPSOPriority(const TArray<FMaterialPSOPrecacheRequestID>& MaterialPSORequestIDs);


#if PSO_PRECACHING_VALIDATE

/**
 * Wraps all PSO precache state data
 */
namespace PSOCollectorStats
{
	extern ENGINE_API int32 IsPrecachingValidationEnabled();

	using VertexFactoryCountTableType = Experimental::TRobinHoodHashMap<const FVertexFactoryType*, uint32>;
	struct FShaderStateUsage
	{
		bool bPrecached = false;
		bool bUsed = false;
	};

	/**
	 * Track precache stats in total, per mesh pass type and per vertex factory type
	 */
	struct FPrecacheUsageData
	{
		FPrecacheUsageData()
		{
			Empty();
		}

		void Empty()
		{
			Count = 0;
			FMemory::Memzero(PerMeshPassCount, FPSOCollectorCreateManager::MaxPSOCollectorCount * sizeof(uint32));
			PerVertexFactoryCount.Empty();
		}

		void UpdateStats(uint32 MeshPassType, const FVertexFactoryType* VFType)
		{
			Count++;

			if (MeshPassType < FPSOCollectorCreateManager::MaxPSOCollectorCount)
			{
				PerMeshPassCount[MeshPassType]++;
			}

			if (VFType != nullptr)
			{
				Experimental::FHashElementId TableId = PerVertexFactoryCount.FindId(VFType);
				if (!TableId.IsValid())
				{
					TableId = PerVertexFactoryCount.FindOrAddId(VFType, uint32());
				}
				uint32& Value = PerVertexFactoryCount.GetByElementId(TableId).Value;
				Value++;
			}
		}

		uint32 Count;
		uint32 PerMeshPassCount[FPSOCollectorCreateManager::MaxPSOCollectorCount];
		VertexFactoryCountTableType PerVertexFactoryCount;
	};

	/**
	 * Collect stats for different cache hit states
	 */
	struct FPrecacheStats
	{
		void Empty()
		{
			PrecacheData.Empty();
			UsageData.Empty();
			HitData.Empty();
			MissData.Empty();
			TooLateData.Empty();
			UntrackedData.Empty();
		}

		FPrecacheUsageData PrecacheData;		//< PSOs which have been precached
		FPrecacheUsageData UsageData;			//< PSOs which are used during rendering 
		FPrecacheUsageData HitData;				//< PSOs which are used during rendering and have been successfully precached
		FPrecacheUsageData MissData;			//< PSOs which are used during rendering and have not been precached (but should have been)
		FPrecacheUsageData TooLateData;			//< PSOs which are used during rendering and are still precaching (will cause hitch - component could wait for it to be done)
		FPrecacheUsageData UntrackedData;		//< PSOs which are used during rendering but are currently not precached because for example the MeshPassProcessor or VertexFactory type don't support PSO precaching yet
	};

	/**
	 * Add PSO initializer to cache for validation & state tracking
	 */
	extern ENGINE_API void AddPipelineStateToCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType);

	/**
	 * Is the requested graphics PSO initializer precached (only PSO relevant data is checked)
	 */
	extern ENGINE_API EPSOPrecacheResult CheckPipelineStateInCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType);

	/**
	 * Add compute shader to cache for validation & state tracking
	 */
	extern ENGINE_API void AddComputeShaderToCache(FRHIComputeShader* ComputeShader, uint32 MeshPassType);

	/**
	 * Is the requested compute shader precached
	 */
	extern ENGINE_API EPSOPrecacheResult CheckComputeShaderInCache(FRHIComputeShader* ComputeShader, uint32 MeshPassType);
}

#endif // PSO_PRECACHING_VALIDATE
