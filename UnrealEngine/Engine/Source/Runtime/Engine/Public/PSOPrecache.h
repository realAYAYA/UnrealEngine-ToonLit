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

#define PSO_PRECACHING_VALIDATE (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

/**
 * Parameters which are needed to collect all possible PSOs used by the PSO collectors
 */
struct FPSOPrecacheParams
{
	FPSOPrecacheParams()
	{
		PrimitiveType = (uint8)PT_TriangleList;
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

			uint32 Unused : 7;
		};
		uint32 Data;
	};
};

/**
 * Wrapper class around the initializer to collect some extra validation data during PSO collection on the different collectors
 */
struct FPSOPrecacheData
{
	FGraphicsPipelineStateInitializer PSOInitializer;

#if PSO_PRECACHING_VALIDATE
	uint32 MeshPassType = 0;
	const FVertexFactoryType* VertexFactoryType = nullptr;
#endif // PSO_PRECACHING_VALIDATE
};

/** 
 * Interface class implemented by the mesh pass processor to collect all possible PSOs
 */
class IPSOCollector
{
public:
	virtual ~IPSOCollector() {}
	
	// Collect all PSO for given material, vertex factory & params
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) = 0;
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
 * Try and create PSOs for all the given initializers and return an optional array of graph events of async compiling PSOs
 */
extern FGraphEventArray PrecachePSOs(const TArray<FPSOPrecacheData>& PSOInitializers);

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

	constexpr static uint32 MaxPSOCollectorCount = 32;

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
		void Empty()
		{
			FMemory::Memzero(PerMeshPassCount, FPSOCollectorCreateManager::MaxPSOCollectorCount * sizeof(uint32));
		}

		void UpdateStats(uint32 MeshPassType, const FVertexFactoryType* VFType)
		{
			Count++;
			PerMeshPassCount[MeshPassType]++;

			Experimental::FHashElementId TableId = PerVertexFactoryCount.FindId(VFType);
			if (!TableId.IsValid())
			{
				TableId = PerVertexFactoryCount.FindOrAddId(VFType, uint32());
			}
			uint32& Value = PerVertexFactoryCount.GetByElementId(TableId).Value;
			Value++;
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
			UntrackedData.Empty();
		}

		FPrecacheUsageData PrecacheData;		//< PSOs which have been precached
		FPrecacheUsageData UsageData;			//< PSOs which are used during rendering 
		FPrecacheUsageData HitData;				//< PSOs which are used during rendering and have been successfully precached
		FPrecacheUsageData MissData;			//< PSOs which are used during rendering and have not been precached (but should have been)
		FPrecacheUsageData UntrackedData;		//< PSOs which are used during rendering but are currently not precached because for example the MeshPassProcessor or VertexFactory type don't support PSO precaching yet
	};

	/**
	 * Add PSO initializer to cache for validation & state tracking
	 */
	extern ENGINE_API void AddPipelineStateToCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType);

	/**
	 * Is the requested PSO initializer precached (only PSO relevant data is checked)
	 */
	extern ENGINE_API EPSOPrecacheResult CheckPipelineStateInCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType);
}

#endif // PSO_PRECACHING_VALIDATE
