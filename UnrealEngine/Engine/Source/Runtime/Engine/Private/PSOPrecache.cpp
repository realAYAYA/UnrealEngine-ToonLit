// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessorManager.cpp: 
=============================================================================*/

#include "PSOPrecache.h"
#include "SceneInterface.h"
#include "VertexFactory.h"

static TAutoConsoleVariable<int32> CVarPrecacheGlobalComputeShaders(
	TEXT("r.PSOPrecache.GlobalComputeShaders"),
	0,
	TEXT("Precache all global compute shaders during startup (default 0)."),
	ECVF_ReadOnly
);

int32 GPSOPrecacheComponents = 1;
static FAutoConsoleVariableRef CVarPSOPrecacheComponents(
	TEXT("r.PSOPrecache.Components"),
	GPSOPrecacheComponents,
	TEXT("Precache all possible used PSOs by components during Postload (default 1 if PSOPrecaching is enabled)."),
	ECVF_ReadOnly
);

int32 GPSOPrecacheResources = 0;
static FAutoConsoleVariableRef CVarPSOPrecacheResources(
	TEXT("r.PSOPrecache.Resources"),
	GPSOPrecacheResources,
	TEXT("Precache all possible used PSOs by resources during Postload (default 0 if PSOPrecaching is enabled)."),
	ECVF_ReadOnly
);

PSOCollectorCreateFunction FPSOCollectorCreateManager::JumpTable[(int32)EShadingPath::Num][FPSOCollectorCreateManager::MaxPSOCollectorCount] = {};

bool IsComponentPSOPrecachingEnabled()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOPrecacheComponents && !GIsEditor;
}

bool IsResourcePSOPrecachingEnabled()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOPrecacheResources && !GIsEditor;
}

FGraphEventArray PrecachePSOs(const TArray<FPSOPrecacheData>& PSOInitializers)
{
	FGraphEventArray GraphEvents;
	for (const FPSOPrecacheData& PrecacheData : PSOInitializers)
	{
#if PSO_PRECACHING_VALIDATE
		PSOCollectorStats::AddPipelineStateToCache(PrecacheData.PSOInitializer, PrecacheData.MeshPassType, PrecacheData.VertexFactoryType);
#endif // PSO_PRECACHING_VALIDATE

		FGraphEventRef GraphEvent = PipelineStateCache::PrecacheGraphicsPipelineState(PrecacheData.PSOInitializer);
		if (GraphEvent)
		{
			GraphEvents.Add(GraphEvent);
		}
	}

	return GraphEvents;
}

#if PSO_PRECACHING_VALIDATE

int32 GValidatePrecaching = 0;
static FAutoConsoleVariableRef CVarValidatePSOPrecaching(
	TEXT("r.PSOPrecache.Validation"),
	GValidatePrecaching,
	TEXT("Check if runtime used PSOs are correctly precached and track information per pass type, vertex factory and cache hit state (default 0)."),
	ECVF_ReadOnly
);

// Stats for the complete PSO
static FCriticalSection FullPSOPrecacheLock;
static PSOCollectorStats::FPrecacheStats FullPSOPrecacheStats;

// Cache stats on hash - TODO: should use FGraphicsPipelineStateInitializer as key and use FGraphicsPSOInitializerKeyFuncs for search
static Experimental::TRobinHoodHashMap<uint64, PSOCollectorStats::FShaderStateUsage> FullPSOPrecacheMap;

int32 PSOCollectorStats::IsPrecachingValidationEnabled()
{
	return PipelineStateCache::IsPSOPrecachingEnabled() && GValidatePrecaching;
}

void PSOCollectorStats::AddPipelineStateToCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
{
	if (!GValidatePrecaching)
	{
		return;
	}

	uint64 PrecachePSOHash = RHIComputePrecachePSOHash(PSOInitializer);

	FScopeLock Lock(&FullPSOPrecacheLock);

	static bool bFirstTime = true;
	if (bFirstTime)
	{
		FullPSOPrecacheStats.Empty();
		bFirstTime = false;
	}

	Experimental::FHashElementId TableId = FullPSOPrecacheMap.FindId(PrecachePSOHash);
	if (!TableId.IsValid())
	{
		TableId = FullPSOPrecacheMap.FindOrAddId(PrecachePSOHash, FShaderStateUsage());
	}

	FShaderStateUsage& Value = FullPSOPrecacheMap.GetByElementId(TableId).Value;
	if (!Value.bPrecached)
	{
		check(!Value.bUsed);
		FullPSOPrecacheStats.PrecacheData.UpdateStats(MeshPassType, VertexFactoryType);
		Value.bPrecached = true;
	}
}

EPSOPrecacheResult PSOCollectorStats::CheckPipelineStateInCache(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
{
	if (!GValidatePrecaching)
	{
		return EPSOPrecacheResult::Unknown;
	}

	bool bUpdateStats = false;
	bool bTracked = true;
	if (MeshPassType < FPSOCollectorCreateManager::MaxPSOCollectorCount && VertexFactoryType)
	{
		const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(GMaxRHIFeatureLevel);
		bool bCollectPSOs = FPSOCollectorCreateManager::GetCreateFunction(ShadingPath, MeshPassType) != nullptr;
		bTracked = bCollectPSOs && VertexFactoryType->SupportsPSOPrecaching();

		bUpdateStats = true;
	}
	EPSOPrecacheResult PrecacheResult = EPSOPrecacheResult::NotSupported;

	// only search the cache if it's tracked
	if (bTracked)
	{
		PrecacheResult = PipelineStateCache::CheckPipelineStateInCache(PSOInitializer);
	}

	if (bUpdateStats)
	{
		uint64 PrecachePSOHash = RHIComputePrecachePSOHash(PSOInitializer);

		FScopeLock Lock(&FullPSOPrecacheLock);

		Experimental::FHashElementId TableId = FullPSOPrecacheMap.FindId(PrecachePSOHash);
		if (!TableId.IsValid())
		{
			TableId = FullPSOPrecacheMap.FindOrAddId(PrecachePSOHash, FShaderStateUsage());
		}

		FShaderStateUsage& Value = FullPSOPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bUsed)
		{
			FullPSOPrecacheStats.UsageData.UpdateStats(MeshPassType, VertexFactoryType);
			if (!bTracked)
			{
				FullPSOPrecacheStats.UntrackedData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else if (!Value.bPrecached)
			{
				check(PrecacheResult == EPSOPrecacheResult::Missed);
				FullPSOPrecacheStats.MissData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else
			{
				check(PrecacheResult == EPSOPrecacheResult::Active || PrecacheResult == EPSOPrecacheResult::Complete);
				FullPSOPrecacheStats.HitData.UpdateStats(MeshPassType, VertexFactoryType);
			}

			Value.bUsed = true;
		}
	}

	return PrecacheResult;
}

#endif // PSO_PRECACHING_VALIDATE