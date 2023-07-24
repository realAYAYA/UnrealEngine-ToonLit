// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessorManager.cpp: 
=============================================================================*/

#include "PSOPrecache.h"
#include "Misc/App.h"
#include "SceneInterface.h"
#include "PipelineStateCache.h"
#include "VertexFactory.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"

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

int32 GPSOProxyCreationWhenPSOReady = 0;
static FAutoConsoleVariableRef CVarPSOProxyCreationWhenPSOReady(
	TEXT("r.PSOPrecache.ProxyCreationWhenPSOReady"),
	GPSOProxyCreationWhenPSOReady,
	TEXT("Delay the component proxy creation when the requested PSOs for precaching are still compiling."),
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

bool ProxyCreationWhenPSOReady()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOProxyCreationWhenPSOReady && !GIsEditor;
}

FPSOPrecacheRequestResultArray PrecachePSOs(const TArray<FPSOPrecacheData>& PSOInitializers)
{
	FPSOPrecacheRequestResultArray Results;
	for (const FPSOPrecacheData& PrecacheData : PSOInitializers)
	{
		switch (PrecacheData.Type)
		{
		case FPSOPrecacheData::EType::Graphics:
		{
#if PSO_PRECACHING_VALIDATE
			PSOCollectorStats::AddPipelineStateToCache(PrecacheData.GraphicsPSOInitializer, PrecacheData.MeshPassType, PrecacheData.VertexFactoryType);
#endif // PSO_PRECACHING_VALIDATE

			FPSOPrecacheRequestResult PSOPrecacheResult = PipelineStateCache::PrecacheGraphicsPipelineState(PrecacheData.GraphicsPSOInitializer);
			if (PSOPrecacheResult.IsValid() && PrecacheData.bRequired)
			{
				Results.AddUnique(PSOPrecacheResult);
			}
			break;
		}
		case FPSOPrecacheData::EType::Compute:
		{
#if PSO_PRECACHING_VALIDATE
			PSOCollectorStats::AddComputeShaderToCache(PrecacheData.ComputeShader, PrecacheData.MeshPassType);
#endif // PSO_PRECACHING_VALIDATE

			FPSOPrecacheRequestResult PSOPrecacheResult = PipelineStateCache::PrecacheComputePipelineState(PrecacheData.ComputeShader);
			if (PSOPrecacheResult.IsValid() && PrecacheData.bRequired)
			{
				Results.AddUnique(PSOPrecacheResult);
			}
			break;
		}
		}
	}

	return Results;
}

FPSOPrecacheVertexFactoryData::FPSOPrecacheVertexFactoryData(
	const FVertexFactoryType* InVertexFactoryType, const FVertexDeclarationElementList& ElementList) 
	: VertexFactoryType(InVertexFactoryType)
{
	CustomDefaultVertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(ElementList);
}

/**
 * Helper task used to release the strong object reference to the material interface on the game thread
 * The release has to happen on the gamethread and the material interface can't be GCd while the PSO
 * collection is happening because it touches the material resources
 */
class FMaterialInterfaceReleaseTask
{
public:
	explicit FMaterialInterfaceReleaseTask(TStrongObjectPtr<UMaterialInterface>* InMaterialInterface)
		: MaterialInterface(InMaterialInterface)
	{
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(IsInGameThread());
		delete MaterialInterface;
	}

public:

	TStrongObjectPtr<UMaterialInterface>* MaterialInterface;

	static ESubsequentsMode::Type	GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
	ENamedThreads::Type				GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId				GetStatId() const { return TStatId(); }
};


/**
 * Helper task used to offload the PSO collection from the GameThread. The shader decompression
 * takes too long to run this on the GameThread and it isn't blocking anything crucial.
 * The graph event used to create this task is extended with the PSO compilation tasks itself so the user can optionally
 * wait or known when all PSOs are ready for rendering
 */
class FMaterialPSOPrecacheCollectionTask
{
public:
	explicit FMaterialPSOPrecacheCollectionTask(
		TStrongObjectPtr<UMaterialInterface>* InMaterialInterface,
		const FMaterialPSOPrecacheParams& InPrecacheParams,
		FGraphEventRef& InCollectionGraphEvent)
		: MaterialInterface(InMaterialInterface)
		, PrecacheParams(InPrecacheParams)
		, CollectionGraphEvent(InCollectionGraphEvent)
	{
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

public:

	TStrongObjectPtr<UMaterialInterface>* MaterialInterface;
	FMaterialPSOPrecacheParams PrecacheParams;
	FGraphEventRef CollectionGraphEvent;

	static ESubsequentsMode::Type	GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type				GetDesiredThread() { return ENamedThreads::AnyBackgroundThreadNormalTask; }
	FORCEINLINE TStatId				GetStatId() const { return TStatId(); }
};

class FMaterialPSORequestManager
{
public:

	FMaterialPSOPrecacheRequestID PrecachePSOs(const FMaterialPSOPrecacheParams& Params, EPSOPrecachePriority Priority, FGraphEventArray& OutGraphEvents)
	{
		FMaterialPSOPrecacheRequestID RequestID = INDEX_NONE;		

		// Fast check first with read lock if it's not requested or completely finished already
		{
			FRWScopeLock ReadLock(RWLock, SLT_ReadOnly);
			FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
			if (FindResult != nullptr && FindResult->State == EState::Completed)
			{
				return RequestID;
			}
		}

		// Offload to background job task graph if threading is enabled
		// Don't use background thread in editor because shader maps and material resources could be destroyed while the task is running
		// If it's a perf problem at some point then FMaterialPSOPrecacheRequestID has to be used at material level in the correct places to wait for
		bool bUseBackgroundTask = FApp::ShouldUseThreadingForPerformance() && !GIsEditor;

		FGraphEventRef CollectionGraphEvent;

		// Now try and add with write lock
		{
			FRWScopeLock WriteLock(RWLock, SLT_Write);

			FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
			if (FindResult != nullptr)
			{
				// Update the list of compiling PSOs and update the internal state
				bool bBoostPriority = (Priority == EPSOPrecachePriority::High && FindResult->Priority != Priority);
				CheckCompilingPSOs(*FindResult, bBoostPriority);
				if (FindResult->State != EState::Completed)
				{
					// If there is a collection graph event than task is used for collection and PSO compiles
					// The collection graph event is extended until all PSOs are compiled and caller only has to wait
					// for this event to finish
					if (FindResult->CollectionGraphEvent)
					{
						OutGraphEvents.Add(FindResult->CollectionGraphEvent);
					}
					else
					{
						for (FPSOPrecacheRequestResult& Result : FindResult->ActivePSOPrecacheRequests)
						{
							OutGraphEvents.Add(Result.AsyncCompileEvent);
						}
					}
					RequestID = FindResult->RequestID;
				}

				return RequestID;
			}
			else
			{
				// Add to array to get the new RequestID
				RequestID = MaterialPSORequests.Add(Params);

				// Add data to map
				FPrecacheData PrecacheData;
				PrecacheData.State = EState::Collecting;
				PrecacheData.RequestID = RequestID;
				PrecacheData.Priority = Priority;
				if (bUseBackgroundTask)
				{
					CollectionGraphEvent = FGraphEvent::CreateGraphEvent();
					PrecacheData.CollectionGraphEvent = CollectionGraphEvent;
				}
				MaterialPSORequestData.Add(Params, PrecacheData);
			}
		}

		if (bUseBackgroundTask)
		{
			// Make sure the material instance isn't garbage collected or destroyed yet (create TStrongObjectPtr which will be destroyed on the GT when the collection is done)
			TStrongObjectPtr<UMaterialInterface>* MaterialInterface = new TStrongObjectPtr<UMaterialInterface>(Params.Material->GetMaterialInterface());

			// Create and kick the collection task
			TGraphTask<FMaterialPSOPrecacheCollectionTask>::CreateTask().ConstructAndDispatchWhenReady(MaterialInterface, Params, CollectionGraphEvent);
			
			// Need to wait for collection task which will be extented during run with the actual async compile events
			OutGraphEvents.Add(CollectionGraphEvent);
		}
		else
		{
			// Collect pso and start the async compiles
			FPSOPrecacheRequestResultArray PrecacheResults = Params.Material->GetGameThreadShaderMap()->CollectPSOs(Params);

			// Mark collection complete
			MarkCollectionComplete(Params, PrecacheResults);
			
			// Add the graph events to wait for
			for (FPSOPrecacheRequestResult& Result : PrecacheResults)
			{
				check(Result.IsValid());
				OutGraphEvents.Add(Result.AsyncCompileEvent);
			}
		}
		
		return RequestID;
	}

	void MarkCollectionComplete(const FMaterialPSOPrecacheParams& Params, const FPSOPrecacheRequestResultArray& PrecacheRequestResults)
	{
		FRWScopeLock WriteLock(RWLock, SLT_Write);
		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		check(FindResult && FindResult->State == EState::Collecting);
		check(FindResult->ActivePSOPrecacheRequests.IsEmpty());
		FindResult->ActivePSOPrecacheRequests = PrecacheRequestResults;

		// update the state
		FindResult->State = FindResult->ActivePSOPrecacheRequests.IsEmpty() ? EState::Completed : EState::Compiling;

		// Boost priority if requested already
		if (FindResult->Priority == EPSOPrecachePriority::High)
		{
			CheckCompilingPSOs(*FindResult, true /*bBoostPriority*/);
		}
	}

	void ReleasePrecacheData(FMaterialPSOPrecacheRequestID MaterialPSORequestID)
	{
		check(MaterialPSORequestID != INDEX_NONE);

		FRWScopeLock WriteLock(RWLock, SLT_Write);
		const FMaterialPSOPrecacheParams& Params = MaterialPSORequests[MaterialPSORequestID];

		// Mark invalid & remove from from map (could reused IDs with free list)
		verify(MaterialPSORequestData.Remove(Params) == 1);
		MaterialPSORequests[MaterialPSORequestID] = FMaterialPSOPrecacheParams();
	}

	void BoostPriority(FMaterialPSOPrecacheRequestID MaterialPSORequestID)
	{
		check(MaterialPSORequestID != INDEX_NONE);

		{
			FRWScopeLock ReadLock(RWLock, SLT_ReadOnly);
			const FMaterialPSOPrecacheParams& Params = MaterialPSORequests[MaterialPSORequestID];
			FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
			check(FindResult);

			// Only process if not boosted yet and not completed yet
			if (FindResult->Priority == EPSOPrecachePriority::High || FindResult->State == EState::Completed)
			{
				return;
			}
		}

		FRWScopeLock WriteLock(RWLock, SLT_Write);
		const FMaterialPSOPrecacheParams& Params = MaterialPSORequests[MaterialPSORequestID];
		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		check(FindResult);
		FindResult->Priority = EPSOPrecachePriority::High;

		// Boost PSOs which are still compiling
		CheckCompilingPSOs(*FindResult, true /*bBoostPriority*/);
	}

private:

	// Request state
	enum class EState : uint8
	{
		Unknown,
		Collecting,
		Compiling,
		Completed,
	};

	struct FPrecacheData
	{
		FMaterialPSOPrecacheRequestID RequestID = INDEX_NONE;
		EState State = EState::Unknown;
		FGraphEventRef CollectionGraphEvent;
		FPSOPrecacheRequestResultArray ActivePSOPrecacheRequests;
		EPSOPrecachePriority Priority;
	};

	bool CheckCompilingPSOs(FPrecacheData& PrecacheData, bool bBoostPriority)
	{
		check(PrecacheData.State != EState::Unknown);

		// Check if compilation is done
		if (PrecacheData.State == EState::Compiling)
		{
			for (int32 Index = 0; Index < PrecacheData.ActivePSOPrecacheRequests.Num(); ++Index)
			{
				FPSOPrecacheRequestResult& RequestResult = PrecacheData.ActivePSOPrecacheRequests[Index];
				if (!PipelineStateCache::IsPrecaching(RequestResult.RequestID))
				{
					PrecacheData.ActivePSOPrecacheRequests.RemoveAtSwap(Index);
					Index--;
				}
				else if (bBoostPriority)
				{
					PipelineStateCache::BoostPrecachePriority(RequestResult.RequestID);
				}
			}

			if (PrecacheData.ActivePSOPrecacheRequests.IsEmpty())
			{
				PrecacheData.State = EState::Completed;
			}
		}

		// Not done yet?
		return (PrecacheData.State != EState::Completed);
	}

	FRWLock RWLock;
	TArray<FMaterialPSOPrecacheParams> MaterialPSORequests;	
	TMap<FMaterialPSOPrecacheParams, FPrecacheData> MaterialPSORequestData;
};

FMaterialPSORequestManager GMaterialPSORequestManager;

void FMaterialPSOPrecacheCollectionTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialPSOPrecacheCollectionTask);

	FTaskTagScope ParallelGTScope(ETaskTag::EParallelGameThread);

	// Collect pso and start the async compiles
	FPSOPrecacheRequestResultArray PrecacheResults;
	if (PrecacheParams.Material->GetGameThreadShaderMap())
	{
		PrecacheResults = PrecacheParams.Material->GetGameThreadShaderMap()->CollectPSOs(PrecacheParams);
	}

	// Won't touch the material interface anymore - PSO compile jobs take refs to all RHI resources while creating the task
	TGraphTask<FMaterialInterfaceReleaseTask>::CreateTask().ConstructAndDispatchWhenReady(MaterialInterface);

	// Mark collection complete
	GMaterialPSORequestManager.MarkCollectionComplete(PrecacheParams, PrecacheResults);

	// Extend MyCompletionGraphEvent to wait for all the async compile events
	if (PrecacheResults.Num() > 0)
	{
		for (FPSOPrecacheRequestResult& Result : PrecacheResults)
		{
			check(Result.IsValid());
			CollectionGraphEvent->DontCompleteUntil(Result.AsyncCompileEvent);
		}
	}
	
	CollectionGraphEvent->DispatchSubsequents();
}

FMaterialPSOPrecacheRequestID PrecacheMaterialPSOs(const FMaterialPSOPrecacheParams& MaterialPSOPrecacheParams, EPSOPrecachePriority Priority, FGraphEventArray& GraphEvents)
{
	return GMaterialPSORequestManager.PrecachePSOs(MaterialPSOPrecacheParams, Priority, GraphEvents);
}

void ReleasePSOPrecacheData(const TArray<FMaterialPSOPrecacheRequestID>& MaterialPSORequestIDs)
{
	for (FMaterialPSOPrecacheRequestID RequestID : MaterialPSORequestIDs)
	{
		GMaterialPSORequestManager.ReleasePrecacheData(RequestID);
	}
}

void BoostPSOPriority(const TArray<FMaterialPSOPrecacheRequestID>& MaterialPSORequestIDs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BoostPSOPriority);

	for (FMaterialPSOPrecacheRequestID RequestID : MaterialPSORequestIDs)
	{
		GMaterialPSORequestManager.BoostPriority(RequestID);
	}
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

	Experimental::FHashElementId TableId = FullPSOPrecacheMap.FindId(PrecachePSOHash);
	if (!TableId.IsValid())
	{
		TableId = FullPSOPrecacheMap.FindOrAddId(PrecachePSOHash, FShaderStateUsage());
	}

	FShaderStateUsage& Value = FullPSOPrecacheMap.GetByElementId(TableId).Value;
	if (!Value.bPrecached)
	{
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
			else if (PrecacheResult == EPSOPrecacheResult::Active)
			{
				FullPSOPrecacheStats.TooLateData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else
			{
				check(PrecacheResult == EPSOPrecacheResult::Complete);
				FullPSOPrecacheStats.HitData.UpdateStats(MeshPassType, VertexFactoryType);
			}

			Value.bUsed = true;
		}
	}

	return PrecacheResult;
}

void PSOCollectorStats::AddComputeShaderToCache(FRHIComputeShader* ComputeShader, uint32 MeshPassType)
{
	if (!GValidatePrecaching)
	{
		return;
	}

	FSHAHash ShaderHash = ComputeShader->GetHash();
	uint64 PrecachePSOHash = *reinterpret_cast<const uint64*>(ShaderHash.Hash);

	FScopeLock Lock(&FullPSOPrecacheLock);

	Experimental::FHashElementId TableId = FullPSOPrecacheMap.FindId(PrecachePSOHash);
	if (!TableId.IsValid())
	{
		TableId = FullPSOPrecacheMap.FindOrAddId(PrecachePSOHash, FShaderStateUsage());
	}

	FShaderStateUsage& Value = FullPSOPrecacheMap.GetByElementId(TableId).Value;
	if (!Value.bPrecached)
	{
		FullPSOPrecacheStats.PrecacheData.UpdateStats(MeshPassType, nullptr);
		Value.bPrecached = true;
	}
}

EPSOPrecacheResult PSOCollectorStats::CheckComputeShaderInCache(FRHIComputeShader* ComputeShader, uint32 MeshPassType)
{
	if (!GValidatePrecaching)
	{
		return EPSOPrecacheResult::Unknown;
	}

	bool bTracked = true;

	EPSOPrecacheResult PrecacheResult = PipelineStateCache::CheckPipelineStateInCache(ComputeShader);
	{
		FSHAHash ShaderHash = ComputeShader->GetHash();
		uint64 PrecachePSOHash = *reinterpret_cast<const uint64*>(ShaderHash.Hash);

		FScopeLock Lock(&FullPSOPrecacheLock);

		Experimental::FHashElementId TableId = FullPSOPrecacheMap.FindId(PrecachePSOHash);
		if (!TableId.IsValid())
		{
			TableId = FullPSOPrecacheMap.FindOrAddId(PrecachePSOHash, FShaderStateUsage());
		}

		FShaderStateUsage& Value = FullPSOPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bUsed)
		{
			FullPSOPrecacheStats.UsageData.UpdateStats(MeshPassType, nullptr);
			if (!bTracked)
			{
				FullPSOPrecacheStats.UntrackedData.UpdateStats(MeshPassType, nullptr);
			}
			else if (!Value.bPrecached)
			{
				check(PrecacheResult == EPSOPrecacheResult::Missed);
				FullPSOPrecacheStats.MissData.UpdateStats(MeshPassType, nullptr);
			}
			else
			{
				check(PrecacheResult == EPSOPrecacheResult::Active || PrecacheResult == EPSOPrecacheResult::Complete);
				FullPSOPrecacheStats.HitData.UpdateStats(MeshPassType, nullptr);
			}

			Value.bUsed = true;
		}
	}

	return PrecacheResult;
}

#endif // PSO_PRECACHING_VALIDATE
