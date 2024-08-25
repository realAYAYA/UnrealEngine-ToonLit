// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOPrecacheMaterial.cpp: 
=============================================================================*/

#include "PSOPrecacheMaterial.h"
#include "PSOPrecacheValidation.h"

#include "Misc/App.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "VertexFactory.h"
#include "SceneInterface.h"

int32 GPSOUseBackgroundThreadForCollection = 1;
static FAutoConsoleVariableRef CVarPSOUseBackgroundThreadForCollection(
	TEXT("r.PSOPrecache.UseBackgroundThreadForCollection"),
	GPSOUseBackgroundThreadForCollection,
	TEXT("Use background threads for PSO precache data collection on the mesh pass processors.\n"),
	ECVF_ReadOnly
);

FPSOCollectorCreateManager::FPSOCollectorData FPSOCollectorCreateManager::PSOCollectors[(int32)EShadingPath::Num][FPSOCollectorCreateManager::MaxPSOCollectorCount] = {};

int32 FPSOCollectorCreateManager::GetIndex(EShadingPath ShadingPath, const TCHAR* Name)
{
#if PSO_PRECACHING_VALIDATE
	if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		for (int32 Index = 0; Index < PSOCollectorCount[ShadingPathIdx]; ++Index)
		{
			if (FCString::Strcmp(PSOCollectors[ShadingPathIdx][Index].Name, Name) == 0)
			{
				return Index;
			}
		}
	}
#endif

	return INDEX_NONE;
}

/**
 * Start the actual PSO precache tasks for all the initializers provided and return the request result array containing the graph event on which to wait for the PSOs to finish compiling
 */
FPSOPrecacheRequestResultArray RequestPrecachePSOs(const FPSOPrecacheDataArray& PSOInitializers)
{
	FPSOPrecacheRequestResultArray Results;
	for (const FPSOPrecacheData& PrecacheData : PSOInitializers)
	{
		switch (PrecacheData.Type)
		{
		case FPSOPrecacheData::EType::Graphics:
		{
#if PSO_PRECACHING_VALIDATE
			PSOCollectorStats::GetFullPSOPrecacheStatsCollector().AddStateToCache(PrecacheData.GraphicsPSOInitializer, PSOCollectorStats::GetPSOPrecacheHash, nullptr, PrecacheData.PSOCollectorIndex, PrecacheData.VertexFactoryType);
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
			PSOCollectorStats::GetFullPSOPrecacheStatsCollector().AddStateToCache(*PrecacheData.ComputeShader, PSOCollectorStats::GetPSOPrecacheHash, nullptr, PrecacheData.PSOCollectorIndex, nullptr);
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
		FGraphEventRef& InCollectionGraphEvent,
		uint32 InRequestLifecycleID)
		: MaterialInterface(InMaterialInterface)
		, PrecacheParams(InPrecacheParams)
		, CollectionGraphEvent(InCollectionGraphEvent)
		, RequestLifecycleID(InRequestLifecycleID)
	{
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

public:

	TStrongObjectPtr<UMaterialInterface>* MaterialInterface;
	FMaterialPSOPrecacheParams PrecacheParams;
	FGraphEventRef CollectionGraphEvent;
	uint32 RequestLifecycleID;	

	static ESubsequentsMode::Type	GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type				GetDesiredThread() { return ENamedThreads::AnyBackgroundThreadNormalTask; }
	FORCEINLINE TStatId				GetStatId() const { return TStatId(); }
};


/**
 * Manages all the material PSO requests and cached which PSOs are still compiling for a certain material, vertex factory and precache param combination
 * Also caches all the request information used for detailed logging on PSO precache misses
 */
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
		bool bUseBackgroundTask = GPSOUseBackgroundThreadForCollection && FApp::ShouldUseThreadingForPerformance() && !GIsEditor;

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
					uint32 RequestLifecycleID = LifecycleID;
					FFunctionGraphTask::CreateAndDispatchWhenReady(
						[this, Params, RequestLifecycleID]
						{
							MarkCompilationComplete(Params, RequestLifecycleID);
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
			TGraphTask<FMaterialPSOPrecacheCollectionTask>::CreateTask().ConstructAndDispatchWhenReady(MaterialInterface, Params, CollectionGraphEvent, LifecycleID);
			
			// Need to wait for collection task which will be extented during run with the actual async compile events
			OutGraphEvents.Add(CollectionGraphEvent);
		}
		else
		{
			// Collect pso data
			FPSOPrecacheDataArray PSOPrecacheData = Params.Material->GetGameThreadShaderMap()->CollectPSOPrecacheData(Params);

			// Start the async compiles
			FPSOPrecacheRequestResultArray PrecacheResults = RequestPrecachePSOs(PSOPrecacheData);
						
			// Mark collection complete
			MarkCollectionComplete(Params, PSOPrecacheData, PrecacheResults, LifecycleID);
			
			// Add the graph events to wait for
			for (FPSOPrecacheRequestResult& Result : PrecacheResults)
			{
				check(Result.IsValid());
				OutGraphEvents.Add(Result.AsyncCompileEvent);
			}
		}
		
		return RequestID;
	}

	void MarkCollectionComplete(const FMaterialPSOPrecacheParams& Params, const FPSOPrecacheDataArray& PrecacheData, const FPSOPrecacheRequestResultArray& PrecacheRequestResults, uint32 RequestLifecycleID)
	{
		FRWScopeLock WriteLock(RWLock, SLT_Write);
		
		// Ignore requests not coming from current life cycle ID
		if (RequestLifecycleID != LifecycleID)
		{
			return;
		}

		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		check(FindResult->State == EState::Collecting);
		check(FindResult->ActivePSOPrecacheRequests.IsEmpty());
		FindResult->ActivePSOPrecacheRequests = PrecacheRequestResults;
#if PSO_PRECACHING_TRACKING
		FindResult->PSOPrecachaData = PrecacheData;
#endif

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

			if (MaterialPSORequestID >= (uint32)MaterialPSORequests.Num())
			{
				return;
			}

			const FMaterialPSOPrecacheParams& Params = MaterialPSORequests[MaterialPSORequestID];
			FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);

			// Only process if not boosted yet and not completed yet
			if (FindResult == nullptr || FindResult->Priority == EPSOPrecachePriority::High || FindResult->State == EState::Completed)
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

	void ClearMaterialPSORequests()
	{
		check(IsInGameThread());

		FRWScopeLock WriteLock(RWLock, SLT_Write);

		// Increment the life cycle ID - all current active collection tasks are 'not important' anymore and can either be skipped or ignored
		LifecycleID++;

		TSet<FMaterial*> Materials;
		for (FMaterialPSOPrecacheParams& Params : MaterialPSORequests)
		{
			if (Params.Material)
			{
				Materials.Add(Params.Material);
			}
		}

		for (FMaterial* Material : Materials)
		{
			Material->ClearPrecachedPSORequestIDs();
		}

		// Clear the current cached pso requests so we gather the PSOs to compile again (usually called on cvar changes which could influence MDC and thus PSOs)			
		MaterialPSORequests.Empty(MaterialPSORequests.Num());
		MaterialPSORequestData.Empty(MaterialPSORequestData.Num());
	}

	uint32 GetLifecycleID() const { return LifecycleID; }

#if PSO_PRECACHING_TRACKING

	FMaterialPSOPrecacheParams GetMaterialPSOPrecacheParams(FMaterialPSOPrecacheRequestID MaterialPSORequestID)
	{
		check(MaterialPSORequestID != INDEX_NONE);

		FRWScopeLock ReadLock(RWLock, SLT_ReadOnly);
		return MaterialPSORequests[MaterialPSORequestID];
	}

	FPSOPrecacheDataArray GetMaterialPSOPrecacheData(FMaterialPSOPrecacheRequestID MaterialPSORequestID)
	{
		check(MaterialPSORequestID != INDEX_NONE);

		FRWScopeLock ReadLock(RWLock, SLT_ReadOnly);
		const FMaterialPSOPrecacheParams&  Params = MaterialPSORequests[MaterialPSORequestID];
		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		check(FindResult);

		// Should wait when still collecting?
		if (FindResult->State == EState::Collecting)
		{
			int i = 0;
		}

		return FindResult->PSOPrecachaData;
	}

#endif // PSO_PRECACHING_TRACKING

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
#if PSO_PRECACHING_TRACKING
		FPSOPrecacheDataArray PSOPrecachaData;
#endif // PSO_PRECACHING_TRACKING
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

	void MarkCompilationComplete(const FMaterialPSOPrecacheParams& Params, uint32 RequestLifecycleID)
	{
		FRWScopeLock WriteLock(RWLock, SLT_Write);
		FPrecacheData* FindResult = MaterialPSORequestData.Find(Params);
		if (FindResult && RequestLifecycleID == LifecycleID)
		{
			verify(!CheckCompilingPSOs(*FindResult, false /*bBoostPriority*/));
		}
	}

	FRWLock RWLock;
	TArray<FMaterialPSOPrecacheParams> MaterialPSORequests;	
	TMap<FMaterialPSOPrecacheParams, FPrecacheData> MaterialPSORequestData;
	uint32 LifecycleID = 0; //< ID to check current outstanding requests are still valid - incremented on re-precache of all current requests
};

// The global request manager - only used locally in a few global function to precache, release or boost PSO precache requests
FMaterialPSORequestManager GMaterialPSORequestManager;

void FMaterialPSOPrecacheCollectionTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialPSOPrecacheCollectionTask);

	// Make sure task is still relevant
	if (RequestLifecycleID != GMaterialPSORequestManager.GetLifecycleID())
	{
		CollectionGraphEvent->DispatchSubsequents();
		return;
	}

	FTaskTagScope ParallelGTScope(ETaskTag::EParallelGameThread);

	// Collect pso data
	FPSOPrecacheDataArray PSOPrecacheData;
	if (PrecacheParams.Material->GetGameThreadShaderMap())
	{
		PSOPrecacheData = PrecacheParams.Material->GetGameThreadShaderMap()->CollectPSOPrecacheData(PrecacheParams);
	}

	// Start the async compiles
	FPSOPrecacheRequestResultArray PrecacheResults = RequestPrecachePSOs(PSOPrecacheData);

	// Mark collection complete
	GMaterialPSORequestManager.MarkCollectionComplete(PrecacheParams, PSOPrecacheData, PrecacheResults, RequestLifecycleID);

	// Won't touch the material interface anymore - PSO compile jobs take refs to all RHI resources while creating the task
	TGraphTask<FMaterialInterfaceReleaseTask>::CreateTask().ConstructAndDispatchWhenReady(MaterialInterface);

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

void PrecacheMaterialPSOs(const FMaterialInterfacePSOPrecacheParamsList& PSOPrecacheParamsList, TArray<FMaterialPSOPrecacheRequestID>& OutMaterialPSOPrecacheRequestIDs, FGraphEventArray& OutGraphEvents)
{
	for (const FMaterialInterfacePSOPrecacheParams& MaterialPSOPrecacheParams : PSOPrecacheParamsList)
	{
		if (MaterialPSOPrecacheParams.MaterialInterface)
		{
			OutGraphEvents.Append(MaterialPSOPrecacheParams.MaterialInterface->PrecachePSOs(MaterialPSOPrecacheParams.VertexFactoryDataList, MaterialPSOPrecacheParams.PSOPrecacheParams, MaterialPSOPrecacheParams.Priority, OutMaterialPSOPrecacheRequestIDs));
		}
	}
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

void ClearMaterialPSORequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ClearMaterialPSORequests);

	return GMaterialPSORequestManager.ClearMaterialPSORequests();
}

#if PSO_PRECACHING_TRACKING

FMaterialPSOPrecacheParams GetMaterialPSOPrecacheParams(FMaterialPSOPrecacheRequestID RequestID)
{
	return GMaterialPSORequestManager.GetMaterialPSOPrecacheParams(RequestID);
}

FPSOPrecacheDataArray GetMaterialPSOPrecacheData(FMaterialPSOPrecacheRequestID RequestID)
{
	return GMaterialPSORequestManager.GetMaterialPSOPrecacheData(RequestID);
}

#else

FMaterialPSOPrecacheParams GetMaterialPSOPrecacheParams(FMaterialPSOPrecacheRequestID RequestID)
{
	return FMaterialPSOPrecacheParams();
}

FPSOPrecacheDataArray GetMaterialPSOPrecacheData(FMaterialPSOPrecacheRequestID RequestID)
{
	return FPSOPrecacheDataArray();
}


#endif // PSO_PRECACHING_TRACKING
