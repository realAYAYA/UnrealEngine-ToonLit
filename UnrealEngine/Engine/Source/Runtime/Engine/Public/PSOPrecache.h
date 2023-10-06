// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOPrecache.h
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "Engine/EngineTypes.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "Experimental/Containers/RobinHoodHashTable.h"

struct FSceneTexturesConfig;
class FRHIComputeShader;
class FMaterial;
class FVertexFactoryType;
class FGraphicsPipelineStateInitializer;
enum class EVertexInputStreamType : uint8;

#define PSO_PRECACHING_VALIDATE !WITH_EDITOR

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
	ENGINE_API FPSOPrecacheVertexFactoryData(const FVertexFactoryType* InVertexFactoryType, const FVertexDeclarationElementList& ElementList);

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

enum class EPSOPrecacheProxyCreationStrategy : uint8
{
	// Always create the render proxy regardless of whether the PSO has finished precaching or not. 
	// This will introduce a blocking wait when the proxy is rendered if the PSO is not ready.
	AlwaysCreate = 0, 

	// Delay the creation of the render proxy until the PSO has finished precaching. 
	// This effectively skips drawing components until the PSO is ready, when the proxy will be created.
	DelayUntilPSOPrecached = 1, 
	
	// Create a render proxy that uses the default material if the PSO has not finished precaching by creation time.
	// The proxy will be re-created with the actual materials once the PSO is ready.
	// Currently implemented only for static and skinned mesh components, while Niagara components will skip render proxy creation altogether.
	UseDefaultMaterialUntilPSOPrecached = 2
};

extern ENGINE_API EPSOPrecacheProxyCreationStrategy GetPSOPrecacheProxyCreationStrategy();

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
class FPSOCollectorCreateManager
{
public:

	constexpr static uint32 MaxPSOCollectorCount = 34;

	static PSOCollectorCreateFunction GetCreateFunction(EShadingPath ShadingPath, uint32 Index)
	{
		check(Index < MaxPSOCollectorCount);
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		return JumpTable[ShadingPathIdx][Index];
	}

private:

	// Have to used fixed size array instead of TArray because of order of initialization of static member variables
	static ENGINE_API PSOCollectorCreateFunction JumpTable[(uint32)EShadingPath::Num][MaxPSOCollectorCount];
	friend class FRegisterPSOCollectorCreateFunction;
};

/**
 * Helper class used to register/unregister the IPSOCollector to the manager at static startup time
 */
class FRegisterPSOCollectorCreateFunction
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
	enum class EPSOPrecacheValidationMode : uint8 {
		Disabled = 0,
		Lightweight = 1,
		Full = 2
	};

	extern ENGINE_API int32 IsPrecachingValidationEnabled();
	extern ENGINE_API EPSOPrecacheValidationMode GetPrecachingValidationMode();
	extern ENGINE_API bool IsMinimalPSOValidationEnabled();

	/**
	 * Compute the hash of a graphics PSO initializer to be used by PSO precaching validation.
	 */
	extern ENGINE_API uint64 GetPSOPrecacheHash(const FGraphicsPipelineStateInitializer& GraphicsPSOInitializer);

	/**
	 * Compute the hash of a compute shader to be used by PSO precaching validation.
	 */
	extern ENGINE_API uint64 GetPSOPrecacheHash(const FRHIComputeShader& ComputeShader);

	using VertexFactoryCountTableType = Experimental::TRobinHoodHashMap<const FVertexFactoryType*, uint32>;
	struct FShaderStateUsage
	{
		bool bPrecached = false;
		bool bUsed = false;
	};

	/**
	 * Track a PSO precache stat's total count, and optionally also track the counts at
	 * the mesh pass type and vertex factory type granularity.
	 */
	class FPrecacheUsageData
	{
	public:
		FPrecacheUsageData(FName StatFName = FName())
			: StatFName(StatFName)
		{
			Empty();
		}

		uint64 GetTotalCount() const
		{
			return FPlatformAtomics::AtomicRead(&Count);
		}

		ENGINE_API void Empty();

		ENGINE_API void UpdateStats(uint32 MeshPassType, const FVertexFactoryType* VFType);

	private:
		bool ShouldRecordFullStats(uint32 MeshPassType, const FVertexFactoryType* VFType) const 
		{
			return GetPrecachingValidationMode() == EPSOPrecacheValidationMode::Full &&
				(MeshPassType < FPSOCollectorCreateManager::MaxPSOCollectorCount || VFType != nullptr);
		}

		volatile int64 Count;

		FName StatFName;

		// Full stats by mesh pass type and by vertex factory type.
		// Only used when the validation mode is set to EPSOPrecacheValidationMode::Full,
		// and when the mesh pass and vertex factory type are known.
		FCriticalSection StatsLock;
		uint32 PerMeshPassCount[FPSOCollectorCreateManager::MaxPSOCollectorCount];
		VertexFactoryCountTableType PerVertexFactoryCount;
	};

	/**
	 * Collect stats for different cache hit states
	 */
	struct FPrecacheStats
	{
		FPrecacheStats(FName UntrackedStatFName = FName(), FName MissStatFName = FName(), FName HitStatFName = FName(), FName UsedStatFName = FName(), FName TooLateStatFName = FName())
			: UsageData(UsedStatFName)
			, HitData(HitStatFName)
			, MissData(MissStatFName)
			, TooLateData(TooLateStatFName)
			, UntrackedData(UntrackedStatFName)
		{
		}

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

	class FPrecacheStatsCollector
	{
	public:
		FPrecacheStatsCollector(FName UntrackedStatFName = FName(), FName MissStatFName = FName(), FName HitStatFName = FName(), FName UsedStatFName = FName(), FName TooLateStatFName = FName())
			: Stats(UntrackedStatFName, MissStatFName, HitStatFName, UsedStatFName, TooLateStatFName)
		{
		}

		void ResetStats()
		{
			Stats.Empty();
		}

		const FPrecacheStats& GetStats() const { return Stats; }

		template <typename TPrecacheState>
		void AddStateToCache(const TPrecacheState& PrecacheState, uint64 HashFn(const TPrecacheState&), uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
		{
			if (!IsPrecachingValidationEnabled())
			{
				return;
			}

			uint64 PrecacheStateHash = HashFn(PrecacheState);

			// Only update stats once per state.
			bool bUpdateStats = false;
			{
				FScopeLock Lock(&StateMapLock);

				FShaderStateUsage* Value = HashedStateMap.FindOrAdd(PrecacheStateHash, FShaderStateUsage());
				if (!Value->bPrecached)
				{
					Value->bPrecached = true;
					bUpdateStats = true;
				}
			}

			if (bUpdateStats)
			{
				Stats.PrecacheData.UpdateStats(MeshPassType, VertexFactoryType);
			}
		}

		template <typename TPrecacheState>
		void CheckStateInCache(const TPrecacheState& PrecacheState, uint64 HashFn(const TPrecacheState&), EPSOPrecacheResult PrecacheResult, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
		{
			if (!IsPrecachingValidationEnabled())
			{
				return;
			}

			uint64 PrecacheStateHash = HashFn(PrecacheState);
			
			bool bTracked = IsStateTracked(MeshPassType, VertexFactoryType);
			UpdatePrecacheStats(PrecacheStateHash, MeshPassType, VertexFactoryType, bTracked, PrecacheResult);
		}

		void CheckStateInCacheByHash(const uint64 PrecacheStateHash, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
		{
			if (!IsPrecachingValidationEnabled())
			{
				return;
			}

			bool bTracked = IsStateTracked(MeshPassType, VertexFactoryType);
			UpdatePrecacheStats(PrecacheStateHash, MeshPassType, VertexFactoryType, bTracked, EPSOPrecacheResult::Unknown);
		}

	private:
		ENGINE_API bool IsStateTracked(uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType) const;

		ENGINE_API void UpdatePrecacheStats(uint64 PrecacheHash, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType, bool bTracked, EPSOPrecacheResult PrecacheResult);

		FPrecacheStats Stats;

		FCriticalSection StateMapLock;
		Experimental::TRobinHoodHashMap<uint64, FShaderStateUsage> HashedStateMap;
	};

	extern ENGINE_API FPrecacheStatsCollector& GetShadersOnlyPSOPrecacheStatsCollector();
	extern ENGINE_API FPrecacheStatsCollector& GetMinimalPSOPrecacheStatsCollector();
	extern ENGINE_API FPrecacheStatsCollector& GetFullPSOPrecacheStatsCollector();
}

#endif // PSO_PRECACHING_VALIDATE
