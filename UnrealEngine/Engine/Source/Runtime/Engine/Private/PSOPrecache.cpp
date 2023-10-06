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

/**
* Different IHVs and drivers can have different opinions on what subset of a PSO
* matters for caching. We track multiple PSO subsets, ranging from shaders-only
* to the full PSO created by the RHI. Generally, a precaching miss on a minimal
* state has worse consequences than a miss on the full state.
*/
DECLARE_STATS_GROUP(TEXT("PSOPrecache"), STATGROUP_PSOPrecache, STATCAT_Advanced);

// Stats tracking shaders-only PSOs (everything except shaders is ignored).
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shaders-only PSO Precache Miss Count"), STAT_ShadersOnlyPSOPrecacheMissCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shaders-only PSO Precache Miss (Untracked) Count"), STAT_ShadersOnlyPSOPrecacheUntrackedCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shaders-only PSO Precache Hit Count"), STAT_ShadersOnlyPSOPrecacheHitCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Shaders-only PSO Precache Used Count"), STAT_ShadersOnlyPSOPrecacheUsedCount, STATGROUP_PSOPrecache);

// Stats tracking minimal PSOs (render target state is ignored).
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Minimal PSO Precache Miss Count"), STAT_MinimalPSOPrecacheMissCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Minimal PSO Precache Miss (Untracked) Count"), STAT_MinimalPSOPrecacheUntrackedCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Minimal PSO Precache Hit Count"), STAT_MinimalPSOPrecacheHitCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Minimal PSO Precache Used Count"), STAT_MinimalPSOPrecacheUsedCount, STATGROUP_PSOPrecache);

// Stats tracking full PSOs (the ones used by the RHI).
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full PSO Precache Miss Count"), STAT_FullPSOPrecacheMissCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full PSO Precache Miss (Untracked) Count"), STAT_FullPSOPrecacheUntrackedCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full PSO Precache Hit Count"), STAT_FullPSOPrecacheHitCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full PSO Precache Used Count"), STAT_FullPSOPrecacheUsedCount, STATGROUP_PSOPrecache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Full PSO Precache Too Late Count"), STAT_FullPSOPrecacheTooLateCount, STATGROUP_PSOPrecache);


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

int32 GPSOProxyCreationWhenPSOReady = 1;
static FAutoConsoleVariableRef CVarPSOProxyCreationWhenPSOReady(
	TEXT("r.PSOPrecache.ProxyCreationWhenPSOReady"),
	GPSOProxyCreationWhenPSOReady,
	TEXT("Delay the component proxy creation when the requested PSOs for precaching are still compiling.\n")
	TEXT(" 0: always create regardless of PSOs status (default)\n")
	TEXT(" 1: delay the creation of the render proxy depending on the specific strategy controlled by r.PSOPrecache.ProxyCreationDelayStrategy\n"),
	ECVF_ReadOnly
);

int32 GPSOProxyCreationDelayStrategy = 0;
static FAutoConsoleVariableRef CVarPSOProxyCreationDelayStrategy(
	TEXT("r.PSOPrecache.ProxyCreationDelayStrategy"),
	GPSOProxyCreationDelayStrategy,
	TEXT("Control the component proxy creation strategy when the requested PSOs for precaching are still compiling. Ignored if r.PSOPrecache.ProxyCreationWhenPSOReady = 0.\n")
	TEXT(" 0: delay creation until PSOs are ready (default)\n")
	TEXT(" 1: create a proxy using the default material until PSOs are ready. Currently implemented for static and skinned meshes - Niagara components will delay creation instead"),
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

EPSOPrecacheProxyCreationStrategy GetPSOPrecacheProxyCreationStrategy()
{
	if (GPSOProxyCreationWhenPSOReady != 1)
	{
		return EPSOPrecacheProxyCreationStrategy::AlwaysCreate;
	}

	switch (GPSOProxyCreationDelayStrategy)
	{
	case 1:
		return EPSOPrecacheProxyCreationStrategy::UseDefaultMaterialUntilPSOPrecached;
	case 0:
		[[fallthrough]];
	default:
		return EPSOPrecacheProxyCreationStrategy::DelayUntilPSOPrecached;
	}
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
			PSOCollectorStats::GetFullPSOPrecacheStatsCollector().AddStateToCache(PrecacheData.GraphicsPSOInitializer, PSOCollectorStats::GetPSOPrecacheHash, PrecacheData.MeshPassType, PrecacheData.VertexFactoryType);
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
			PSOCollectorStats::GetFullPSOPrecacheStatsCollector().AddStateToCache(*PrecacheData.ComputeShader, PSOCollectorStats::GetPSOPrecacheHash, PrecacheData.MeshPassType, nullptr);
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

					// Create task the clear mark fully complete in the cache when done
					FFunctionGraphTask::CreateAndDispatchWhenReady(
						[this, Params]
						{
							MarkCompilationComplete(Params);
						},
						TStatId{}, CollectionGraphEvent
						);
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

		// Release the graph event when done
		if (FindResult->State == EState::Completed)
		{
			FindResult->CollectionGraphEvent = nullptr;
		}

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
				PrecacheData.CollectionGraphEvent = nullptr;
			}
		}

		// Not done yet?
		return (PrecacheData.State != EState::Completed);
	}	

	void MarkCompilationComplete(const FMaterialPSOPrecacheParams& Params)
	{
		FRWScopeLock WriteLock(RWLock, SLT_Write);
		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		if (FindResult)
		{
			verify(!CheckCompilingPSOs(*FindResult, false /*bBoostPriority*/));
		}
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

int32 GPrecachingValidationMode = 0;
static FAutoConsoleVariableRef CVarValidatePSOPrecaching(
	TEXT("r.PSOPrecache.Validation"),
	GPrecachingValidationMode,
	TEXT("Validate whether runtime PSOs are correctly precached and optionally track information per pass type, vertex factory and cache hit state.\n")
	TEXT(" 0: disabled (default)\n")
	TEXT(" 1: lightweight tracking (counter stats)\n")
	TEXT(" 2: full tracking (counter stats by vertex factory, mesh pass type)\n"),
	ECVF_ReadOnly
);

int32 GPrecachingValidationTrackMinimalPSOs = 0;
static FAutoConsoleVariableRef CVarPSOPrecachingTrackMinimalPSOs(
	TEXT("r.PSOPrecache.Validation.TrackMinimalPSOs"),
	GPrecachingValidationTrackMinimalPSOs,
	TEXT("1 enables tracking minimal PSOs precaching stats (shaders-only and minimal PSO states), 0 (default) disables it. Only valid when r.PSOPrecache.Validation != 0."),
	ECVF_ReadOnly
);

int32 PSOCollectorStats::IsPrecachingValidationEnabled()
{
	return PipelineStateCache::IsPSOPrecachingEnabled() && GetPrecachingValidationMode() != EPSOPrecacheValidationMode::Disabled;
}

ENGINE_API PSOCollectorStats::EPSOPrecacheValidationMode PSOCollectorStats::GetPrecachingValidationMode()
{
	switch (GPrecachingValidationMode)
	{
	case 1:
		return EPSOPrecacheValidationMode::Lightweight;
	case 2:
		return EPSOPrecacheValidationMode::Full;
	case 0:
		// Passthrough.
	default:
		return EPSOPrecacheValidationMode::Disabled;
	}
}

bool PSOCollectorStats::IsMinimalPSOValidationEnabled()
{
	return IsPrecachingValidationEnabled() && GPrecachingValidationTrackMinimalPSOs != 0;
}

void PSOCollectorStats::FPrecacheUsageData::Empty()
{
	if (!StatFName.IsNone())
	{
		SET_DWORD_STAT_FName(StatFName, 0)
	}

	FPlatformAtomics::InterlockedExchange(&Count, 0);

	if (GetPrecachingValidationMode() == EPSOPrecacheValidationMode::Full)
	{
		FScopeLock Lock(&StatsLock);

		FMemory::Memzero(PerMeshPassCount, FPSOCollectorCreateManager::MaxPSOCollectorCount * sizeof(uint32));
		PerVertexFactoryCount.Empty();
	}
}

void PSOCollectorStats::FPrecacheUsageData::UpdateStats(uint32 MeshPassType, const FVertexFactoryType* VFType)
{
	if (!StatFName.IsNone())
	{
		INC_DWORD_STAT_FName(StatFName);
	}

	FPlatformAtomics::InterlockedIncrement(&Count);

	if (ShouldRecordFullStats(MeshPassType, VFType))
	{
		FScopeLock Lock(&StatsLock);

		if (MeshPassType < FPSOCollectorCreateManager::MaxPSOCollectorCount)
		{
			PerMeshPassCount[MeshPassType]++;
		}

		if (VFType != nullptr)
		{
			uint32* Value = PerVertexFactoryCount.FindOrAdd(VFType, 0);
			(*Value)++;
		}
	}
}

bool PSOCollectorStats::FPrecacheStatsCollector::IsStateTracked(uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType) const {
	bool bTracked = true;
	if (MeshPassType < FPSOCollectorCreateManager::MaxPSOCollectorCount) {
		const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(GMaxRHIFeatureLevel);
		bool bCollectPSOs = FPSOCollectorCreateManager::GetCreateFunction(ShadingPath, MeshPassType) != nullptr;
		bTracked = bCollectPSOs && (VertexFactoryType == nullptr || VertexFactoryType->SupportsPSOPrecaching());
	}
	return bTracked;
}

void PSOCollectorStats::FPrecacheStatsCollector::UpdatePrecacheStats(uint64 PrecacheHash, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType, bool bTracked, EPSOPrecacheResult PrecacheResult) {
	// Only update stats once per state.
	bool bUpdateStats = false;
	bool bWasPrecached = false;
	{
		FScopeLock Lock(&StateMapLock);
		FShaderStateUsage* Value = HashedStateMap.FindOrAdd(PrecacheHash, FShaderStateUsage());
		if (!Value->bUsed)
		{
			Value->bUsed = true;

			bUpdateStats = true;
			bWasPrecached = Value->bPrecached;
		}
	}	

	if (bUpdateStats)
	{
		Stats.UsageData.UpdateStats(MeshPassType, VertexFactoryType);

		if (!bTracked)
		{
			Stats.UntrackedData.UpdateStats(MeshPassType, VertexFactoryType);
		}
		else if (!bWasPrecached)
		{
			Stats.MissData.UpdateStats(MeshPassType, VertexFactoryType);
		}
		else if (PrecacheResult == EPSOPrecacheResult::Active)
		{
			Stats.TooLateData.UpdateStats(MeshPassType, VertexFactoryType);
		}
		else
		{
			Stats.HitData.UpdateStats(MeshPassType, VertexFactoryType);
		}
	}
}

static PSOCollectorStats::FPrecacheStatsCollector FullPSOPrecacheStatsCollector(
	GET_STATFNAME(STAT_FullPSOPrecacheUntrackedCount), GET_STATFNAME(STAT_FullPSOPrecacheMissCount), GET_STATFNAME(STAT_FullPSOPrecacheHitCount), GET_STATFNAME(STAT_FullPSOPrecacheUsedCount), GET_STATFNAME(STAT_FullPSOPrecacheTooLateCount));

static PSOCollectorStats::FPrecacheStatsCollector ShadersOnlyPSOPrecacheStatsCollector(
	GET_STATFNAME(STAT_ShadersOnlyPSOPrecacheUntrackedCount), GET_STATFNAME(STAT_ShadersOnlyPSOPrecacheMissCount), GET_STATFNAME(STAT_ShadersOnlyPSOPrecacheHitCount), GET_STATFNAME(STAT_ShadersOnlyPSOPrecacheUsedCount));

static PSOCollectorStats::FPrecacheStatsCollector MinimalPSOPrecacheStatsCollector(
	GET_STATFNAME(STAT_MinimalPSOPrecacheUntrackedCount), GET_STATFNAME(STAT_MinimalPSOPrecacheMissCount), GET_STATFNAME(STAT_MinimalPSOPrecacheHitCount), GET_STATFNAME(STAT_MinimalPSOPrecacheUsedCount));

PSOCollectorStats::FPrecacheStatsCollector& PSOCollectorStats::GetShadersOnlyPSOPrecacheStatsCollector()
{
	return ShadersOnlyPSOPrecacheStatsCollector;
}

PSOCollectorStats::FPrecacheStatsCollector& PSOCollectorStats::GetMinimalPSOPrecacheStatsCollector()
{
	return MinimalPSOPrecacheStatsCollector;
}

PSOCollectorStats::FPrecacheStatsCollector& PSOCollectorStats::GetFullPSOPrecacheStatsCollector()
{
	return FullPSOPrecacheStatsCollector;
}

uint64 PSOCollectorStats::GetPSOPrecacheHash(const FGraphicsPipelineStateInitializer& GraphicsPSOInitializer)
{
	if (GraphicsPSOInitializer.StatePrecachePSOHash == 0)
	{
		ensureMsgf(false, TEXT("StatePrecachePSOHash should not be zero"));
		return 0;
	}
	return RHIComputePrecachePSOHash(GraphicsPSOInitializer);
}

uint64 PSOCollectorStats::GetPSOPrecacheHash(const FRHIComputeShader& ComputeShader)
{
    FSHAHash ShaderHash = ComputeShader.GetHash();
	uint64 PSOHash = *reinterpret_cast<const uint64*>(&ShaderHash.Hash);
	return PSOHash;
}

#endif // PSO_PRECACHING_VALIDATE
