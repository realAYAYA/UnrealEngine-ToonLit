// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "Stats/StatsTrace.h"

DECLARE_STATS_GROUP(TEXT("Nanite Coarse Mesh Streaming"), STATGROUP_NaniteCoarseMeshStreaming, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Mesh Count"), STAT_NaniteCoarseMeshTotalCount, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NonStreaming Mesh Count"), STAT_NaniteCoarseMeshNonStreamingCount, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_MEMORY_STAT(TEXT("NonStreaming"), STAT_NaniteCoarseMeshMemoryNonStreaming, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_MEMORY_STAT(TEXT("Total Memory"), STAT_NaniteCoarseMeshTotalMemory, STATGROUP_NaniteCoarseMeshStreaming);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Streaming Meshes Loaded"), STAT_NaniteCoarseMeshStreamingMeshesLoaded, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Streaming Meshes Requested"), STAT_NaniteCoarseMeshStreamingMeshesRequested, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_MEMORY_STAT(TEXT("Streaming Memory Used"), STAT_NaniteCoarseMeshStreamingMeshMemoryUsed, STATGROUP_NaniteCoarseMeshStreaming);
DECLARE_MEMORY_STAT(TEXT("Streaming Memory Requested"), STAT_NaniteCoarseMeshStreamingMeshMemoryRequested, STATGROUP_NaniteCoarseMeshStreaming);

static int32 GNaniteCoarseMeshStreamingMemoryPoolSizeInMB = 220;
static FAutoConsoleVariableRef CVarNaniteCoarseMeshStreamingMemoryPoolSizeInMB(
	TEXT("r.Nanite.CoarseStreamingMeshMemoryPoolSizeInMB"),
	GNaniteCoarseMeshStreamingMemoryPoolSizeInMB,
	TEXT("Pool size for streaming in the render mesh & blas data for the coarse nanite meshes (default 200MB)\n")
	TEXT("This budget will be part of the mesh streaming pool size.\n")
	TEXT("On consoles the actual BLAS memory will be part of this, on PC only the vertex data because the BLAS is dependent on the GPU & driver.\n"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteCoarseMeshStreamingMode = 0;
static FAutoConsoleVariableRef CVarNaniteCoarseMeshStreamingMode(
	TEXT("r.Nanite.CoarseMeshStreamingMode"),
	GNaniteCoarseMeshStreamingMode,
	TEXT("Streaming mode:\n")
	TEXT("0: Use TLAS proxies to drive what to stream within the budget (default)\n")
	TEXT("1: Stream in all registered meshes\n")
	TEXT("2: Don't stream in any coarse LODs\n"),
	ECVF_RenderThreadSafe
);

namespace Nanite
{

#if WITH_EDITOR
	int32 FCoarseMeshStreamingManager::CachedNaniteCoarseMeshStreamingMode = GNaniteCoarseMeshStreamingMode;
#endif // WITH_EDITOR

	FCoarseMeshStreamingManager::FCoarseMeshStreamingManager()
	{
	}

#if WITH_EDITOR
	bool FCoarseMeshStreamingManager::CheckStreamingMode()
	{
		if (CachedNaniteCoarseMeshStreamingMode != GNaniteCoarseMeshStreamingMode)
		{
			CachedNaniteCoarseMeshStreamingMode = GNaniteCoarseMeshStreamingMode;
			return true;
		}
		return false;
	}
#endif // WITH_EDITOR

	int32 FCoarseMeshStreamingManager::BlockTillAllRequestsFinished(float TimeLimit, bool bLogResults)
	{
		// Anything special?
		int32 Result = 0;
		return Result;
	}

	void FCoarseMeshStreamingManager::CancelForcedResources()
	{
		// Anything special?
	}

	void FCoarseMeshStreamingManager::NotifyActorDestroyed(AActor* Actor)
	{
		ProcessNotifiedActor(Actor, ENotifyMode::Unregister);
	}

	void FCoarseMeshStreamingManager::NotifyPrimitiveDetached(const UPrimitiveComponent* Primitive)
	{
		UnregisterComponent(Primitive);
	}

	void FCoarseMeshStreamingManager::NotifyPrimitiveUpdated(const UPrimitiveComponent* Primitive)
	{
		bool bCheckStaticMeshChanged = true;
		RegisterComponent(Primitive, bCheckStaticMeshChanged);
	}

	void FCoarseMeshStreamingManager::NotifyPrimitiveUpdated_Concurrent(const UPrimitiveComponent* Primitive)
	{
		bool bCheckStaticMeshChanged = false;
		RegisterComponent(Primitive, bCheckStaticMeshChanged);
	}

	void FCoarseMeshStreamingManager::AddLevel(ULevel* Level)
	{
		// already added
		if (RegisteredLevels.Contains(Level))
		{
			return;
		}

		for (const AActor* Actor : Level->Actors)
		{
			if (Actor != nullptr)
			{
				ProcessNotifiedActor(Actor, ENotifyMode::Register);
			}
		}

		RegisteredLevels.Add(Level);
	}

	void FCoarseMeshStreamingManager::RemoveLevel(ULevel* Level)
	{
		/*
		// Enabling this cause issues because of missing components - it looks like level can be added with the flags
		// and readded immediately again with different actors
		if (!Level->IsPendingKill() && !Level->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
		{
			return;
		}
		*/

		if (RegisteredLevels.Remove(Level) > 0)
		{
			for (const AActor* Actor : Level->Actors)
			{
				if (Actor != nullptr)
				{
					ProcessNotifiedActor(Actor, ENotifyMode::Unregister);
				}
			}
		}
	}

	void FCoarseMeshStreamingManager::RegisterRenderAsset(UStreamableRenderAsset* RenderAsset)
	{
		INC_DWORD_STAT(STAT_NaniteCoarseMeshTotalCount);

		const FStreamableRenderResourceState& ResourceState = RenderAsset->GetStreamableResourceState();
		uint64 NonStreamingSize = RenderAsset->CalcCumulativeLODSize(ResourceState.NumNonStreamingLODs);
		uint64 StreamingSize = RenderAsset->CalcCumulativeLODSize(ResourceState.NumNonOptionalLODs) - NonStreamingSize;

		INC_MEMORY_STAT_BY(STAT_NaniteCoarseMeshMemoryNonStreaming, NonStreamingSize);
		INC_MEMORY_STAT_BY(STAT_NaniteCoarseMeshTotalMemory, StreamingSize + NonStreamingSize);

		if (StreamingSize == 0)
		{
			check(!ResourceState.bSupportsStreaming);
			INC_DWORD_STAT(STAT_NaniteCoarseMeshNonStreamingCount);
			return;
		}

		check(ResourceState.bSupportsStreaming);

		FRegisteredRenderAsset RenderAssetInfo;
		RenderAssetInfo.RenderAsset = RenderAsset;
		RenderAssetInfo.StreamingSize = StreamingSize;

		FScopeLock ScopeLock(&UpdateCS);
		CoarseMeshStreamingHandle Handle = RegisteredRenderAssets.Add(RenderAssetInfo);
		RegisteredRenderAssets[Handle].Handle = Handle;

		check(RenderAsset->StreamingIndex == INDEX_NONE);
		RenderAsset->StreamingIndex = Handle;
	}

	void FCoarseMeshStreamingManager::UnregisterRenderAsset(UStreamableRenderAsset* RenderAsset)
	{
		DEC_DWORD_STAT(STAT_NaniteCoarseMeshTotalCount);

		const FStreamableRenderResourceState& ResourceState = RenderAsset->GetStreamableResourceState();
		uint64 NonStreamingSize = RenderAsset->CalcCumulativeLODSize(ResourceState.NumNonStreamingLODs);
		uint64 StreamingSize = RenderAsset->CalcCumulativeLODSize(ResourceState.NumNonOptionalLODs) - NonStreamingSize;

		DEC_MEMORY_STAT_BY(STAT_NaniteCoarseMeshMemoryNonStreaming, NonStreamingSize);
		DEC_MEMORY_STAT_BY(STAT_NaniteCoarseMeshTotalMemory, StreamingSize + NonStreamingSize);

		if (StreamingSize == 0)
		{
			check(RenderAsset->StreamingIndex == INDEX_NONE);
			DEC_DWORD_STAT(STAT_NaniteCoarseMeshNonStreamingCount);
			return;
		}

		check(RenderAsset->StreamingIndex != INDEX_NONE);
		
		FScopeLock ScopeLock(&UpdateCS);

		FRegisteredRenderAsset& RenderAssetInfo = RegisteredRenderAssets[RenderAsset->StreamingIndex];

		TSet<const UPrimitiveComponent*>* RegisteredComponents = RegisteredComponentsMap.Find(RenderAsset);

		// If there are still components registered then mark them as unattached and remove from map
		// (this can happen because of different order of destruction during garbage collection)
		if (RegisteredComponents)
		{
			// If already marked for release then don't touch the component anymore (could have already been destroyed and bool was already set to false)
			TSet<const UPrimitiveComponent*>* ComponentsToRelease = RequestReleaseComponentsMap.Find(RenderAsset);
			for (const UPrimitiveComponent* Component : *RegisteredComponents)
			{
				bool bRequestReleased = ComponentsToRelease && ComponentsToRelease->Contains(Component);
				if (!bRequestReleased)
				{
					Component->bAttachedToCoarseMeshStreamingManager = false;
				}
			}
		}
		RegisteredComponentsMap.Remove(RenderAsset);
		RequestReleaseComponentsMap.Remove(RenderAsset);

		if (RenderAssetInfo.ResourceState != EResourceState::Unloaded)
		{
			TotalRequestedRenderAssetSize -= RenderAssetInfo.StreamingSize;

			// Still marked as loaded?
			if (RenderAssetInfo.ResourceState == EResourceState::Loaded ||
				RenderAssetInfo.ResourceState == EResourceState::StreamingOut ||
				RenderAssetInfo.ResourceState == EResourceState::RequestStreamOut)
			{
				TotalLoadedRenderAssetSize -= RenderAssetInfo.StreamingSize;
			}

			verify(ActiveHandles.Remove(RenderAsset->StreamingIndex) == 1);
		}

		RegisteredRenderAssets.RemoveAt(RenderAsset->StreamingIndex);
		RenderAsset->StreamingIndex = INDEX_NONE;
	}

	void FCoarseMeshStreamingManager::ProcessNotifiedActor(const AActor* Actor, ENotifyMode NotifyMode)
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents(Primitives);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			check(Primitive);
			if (NotifyMode == ENotifyMode::Register)
			{
				bool bCheckStaticMeshChanged = true;
				RegisterComponent(Primitive, bCheckStaticMeshChanged);
			}
			else if (NotifyMode == ENotifyMode::Unregister)
			{
				UnregisterComponent(Primitive);
			}
		}
	}

	void FCoarseMeshStreamingManager::RegisterComponent(const UPrimitiveComponent* Primitive, bool bCheckStaticMeshChanged)
	{		
		// Not a static mesh or no nanite data
		if (!Primitive->IsA<UStaticMeshComponent>())
		{
			return;
		}

		UStaticMesh* StaticMesh = ((UStaticMeshComponent*)Primitive)->GetStaticMesh();

		FScopeLock ScopeLock(&UpdateCS);
						
		if (bCheckStaticMeshChanged && Primitive->bAttachedToCoarseMeshStreamingManager)
		{
			UStreamableRenderAsset** RegisteredAsset = ComponentToRenderAssetLookUpMap.Find(Primitive);
			check(RegisteredAsset);

			// Static mesh has changed, first unregister
			if (*RegisteredAsset != StaticMesh)
			{
				UnregisterComponentInternal(Primitive, *RegisteredAsset);
			}
		}

		// Still/already registered, then done
		if (Primitive->bAttachedToCoarseMeshStreamingManager)
		{
			return;
		}

		// Don't need to register if the (new) static mesh isn't set, doesn't have any nanite data or doesn't support streaming
		if (StaticMesh == nullptr || !StaticMesh->HasValidNaniteData() || !StaticMesh->GetStreamableResourceState().bSupportsStreaming)
		{
			return;
		}

		// Make sure it's not requested for release anymore
		bool bNeedToAdd = true;
		TSet<const UPrimitiveComponent*>* ComponentsToRelease = RequestReleaseComponentsMap.Find(StaticMesh);
		if (ComponentsToRelease && ComponentsToRelease->Contains(Primitive))
		{
			ComponentsToRelease->Remove(Primitive);
			bNeedToAdd = false;
		}
		TSet<const UPrimitiveComponent*>& RegisteredComponents = RegisteredComponentsMap.FindOrAdd(StaticMesh);
		if (bNeedToAdd)
		{
			check(!RegisteredComponents.Contains(Primitive));
			RegisteredComponents.Add(Primitive);
		}
		else
		{
			check(RegisteredComponents.Contains(Primitive));
		}

		// Store link to current static mesh so it can be checked if it has changed
		ComponentToRenderAssetLookUpMap.Add(Primitive, StaticMesh);

		// Mark tracked
		Primitive->bAttachedToCoarseMeshStreamingManager = true;
	}

	void FCoarseMeshStreamingManager::UnregisterComponent(const UPrimitiveComponent* Primitive)
	{
		// Not a static mesh or no nanite data
		if (!Primitive->IsA<UStaticMeshComponent>())
		{
			return;
		}
		
		UStaticMesh* StaticMesh = ((UStaticMeshComponent*)Primitive)->GetStaticMesh();
		if (StaticMesh == nullptr || !StaticMesh->HasValidNaniteData())
		{
			return;
		}

		const FStreamableRenderResourceState& ResourceState = StaticMesh->GetStreamableResourceState();
		if (!ResourceState.bSupportsStreaming)
		{
			return;
		}

		FScopeLock ScopeLock(&UpdateCS);

		// Not tracked then early out
		if (!Primitive->bAttachedToCoarseMeshStreamingManager)
		{
			return;
		}

		UnregisterComponentInternal(Primitive, StaticMesh);
	}

	void FCoarseMeshStreamingManager::UnregisterComponentInternal(const UPrimitiveComponent* Primitive, UStreamableRenderAsset* RenderAsset)
	{
		TSet<const UPrimitiveComponent*>& Components = RequestReleaseComponentsMap.FindOrAdd(RenderAsset);
		check(!Components.Contains(Primitive));
		Components.Add(Primitive);

		verify(ComponentToRenderAssetLookUpMap.Remove(Primitive) == 1);

		// Mark untracked
		Primitive->bAttachedToCoarseMeshStreamingManager = false;
	}

	void FCoarseMeshStreamingManager::RequestUpdateCachedRenderState(const UStreamableRenderAsset* RenderAsset)
	{
		check(IsInRenderingThread());

#if RHI_RAYTRACING

		// For safety - perhaps only required because another component could be added/removed here?
		FScopeLock ScopeLock(&UpdateCS);

		TSet<const UPrimitiveComponent*>* Components = RegisteredComponentsMap.Find(RenderAsset);
		if (Components)
		{
			for (const UPrimitiveComponent* Component : *Components)
			{
				if (Component->SceneProxy)
				{
					Component->SceneProxy->GetScene().UpdateCachedRayTracingState(Component->SceneProxy);
					//Component->SceneProxy->GetPrimitiveSceneInfo()->bCachedRaytracingDataDirty = true;
				}
			}
		}
#endif // RHI_RAYTRACING
	}

	void FCoarseMeshStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCoarseMeshStreamingManager::UpdateResourceStreaming);

		FScopeLock ScopeLock(&UpdateCS);

		// Process the request release of the components on RenderThread now
		for (auto Iter = RequestReleaseComponentsMap.CreateIterator(); Iter ; ++Iter)
		{
			UStreamableRenderAsset* RenderAsset = Iter.Key();			
			TSet<const UPrimitiveComponent*>* RegisteredComponents = RegisteredComponentsMap.Find(RenderAsset);
			if (RegisteredComponents) 
			{
				TSet<const UPrimitiveComponent*>& ComponentsToRelease = Iter.Value();
				for (const UPrimitiveComponent* Component : ComponentsToRelease)
				{
					verify(RegisteredComponents->Remove(Component) == 1);
				}

				// remove if empty
				if (RegisteredComponents->Num() == 0)
				{
					RegisteredComponentsMap.Remove(RenderAsset);
				}
			}
		}
		RequestReleaseComponentsMap.Empty();

		// Iterate over all active handles and check if they have stream in/out requests
		// can be optimized to only iterate over handles which are actively streaming in & out if this would be a problem
		for (int32 ActiveHandleIndex = 0; ActiveHandleIndex < ActiveHandles.Num(); ++ActiveHandleIndex)
		{
			CoarseMeshStreamingHandle& Handle = ActiveHandles[ActiveHandleIndex];
			FRegisteredRenderAsset& RenderAsset = RegisteredRenderAssets[Handle];
			const FStreamableRenderResourceState& StreamingResourceState = RenderAsset.RenderAsset->GetStreamableResourceState();
			switch (RenderAsset.ResourceState)
			{
			case EResourceState::Unloaded:
			{
				checkf(false, TEXT("Active handles list should not contain unloaded resources (Resource: %s)"), *RenderAsset.RenderAsset->GetName());
				break;
			}
			case EResourceState::RequestStreamIn:
			{
				// Request stream in operation (check result because it could still be initializing)
				if (RenderAsset.RenderAsset->StreamIn(StreamingResourceState.NumNonOptionalLODs, false))
				{
					RenderAsset.RenderAsset->TickStreaming();
					RenderAsset.ResourceState = EResourceState::StreamingIn;
				}
				break;
			}
			case EResourceState::StreamingIn:
			{
				// Streaming active, update the state
				RenderAsset.RenderAsset->TickStreaming();

				// finished?
				if (StreamingResourceState.NumResidentLODs == StreamingResourceState.NumRequestedLODs)
				{
					RenderAsset.ResourceState = EResourceState::Loaded;
					TotalLoadedRenderAssetSize += RenderAsset.StreamingSize;
				}
				break;
			}
			case EResourceState::Loaded:
			{
				// don't have to do anything special now
				break;
			}
			case EResourceState::RequestStreamOut:
			{
				// Request stream in operation
				verify(RenderAsset.RenderAsset->StreamOut(StreamingResourceState.NumNonStreamingLODs));
				RenderAsset.RenderAsset->TickStreaming();

				RenderAsset.ResourceState = EResourceState::StreamingOut;

				break;
			}
			case EResourceState::StreamingOut:
			{
				// Streaming active, update the state
				RenderAsset.RenderAsset->TickStreaming();

				// finished?
				if (StreamingResourceState.NumResidentLODs == StreamingResourceState.NumRequestedLODs)
				{
					RenderAsset.ResourceState = EResourceState::Unloaded;
					ActiveHandles.RemoveAt(ActiveHandleIndex);
					ActiveHandleIndex--;

					TotalLoadedRenderAssetSize -= RenderAsset.StreamingSize;
				}
				break;
			}
			}
		}

#ifdef STATS
		// Update the stats
		SET_DWORD_STAT(STAT_NaniteCoarseMeshStreamingMeshesLoaded, ActiveHandles.Num());
		SET_MEMORY_STAT(STAT_NaniteCoarseMeshStreamingMeshMemoryUsed, TotalLoadedRenderAssetSize);
#endif // STATS
	}

	void FCoarseMeshStreamingManager::AddUsedStreamingHandles(TArray<CoarseMeshStreamingHandle>& UsedAssets)
	{
		check(IsInRenderingThread());
		
		TRACE_CPUPROFILER_EVENT_SCOPE(FCoarseMeshStreamingManager::AddUsedStreamingHandles);
		UsedStreamableHandles.Append(UsedAssets);
	}

	void FCoarseMeshStreamingManager::UpdateResourceStates()
	{		
		check(IsInRenderingThread());

		TRACE_CPUPROFILER_EVENT_SCOPE(FCoarseMeshStreamingManager::UpdateRenderThread);

		FScopeLock ScopeLock(&UpdateCS);

		// Update the tick count
		TickCount++;

		// TODO: use stack allocator ?
		TArray<CoarseMeshStreamingHandle> RequestStreamInRenderAssets;
		RequestStreamInRenderAssets.Reserve(UsedStreamableHandles.Num());
		uint64 RequestStreamInRenderAssetsSize = 0;
	
		// Check all the used streaming assets 
		uint64 RequestedMemorySize = 0;
		uint32 RequestedMeshCount = UsedStreamableHandles.Num();
		for (TSet<CoarseMeshStreamingHandle>::TConstIterator SetIt(UsedStreamableHandles); SetIt; ++SetIt)
		{
			CoarseMeshStreamingHandle Handle = *SetIt;
			if (!RegisteredRenderAssets.IsValidIndex(Handle))
			{
				continue;
			}

			FRegisteredRenderAsset& RenderAsset = RegisteredRenderAssets[Handle];
			check(RenderAsset.Handle == Handle);

			// update the frame ID
			RenderAsset.LastUsedTickIndex = TickCount;

			RequestedMemorySize += RenderAsset.StreamingSize;

			// Keep track if not loaded yet
			if (RenderAsset.ResourceState == EResourceState::Unloaded)
			{
				RequestStreamInRenderAssetsSize += RenderAsset.StreamingSize;
				RequestStreamInRenderAssets.Add(Handle);
			}
		}

		// Clear the working set
		UsedStreamableHandles.Empty(UsedStreamableHandles.Num());

		// Special mode to either stream in everything or nothing
		if (GNaniteCoarseMeshStreamingMode == 1)
		{
			ForceStreamIn();
		}
		else if (GNaniteCoarseMeshStreamingMode == 2)
		{
			ForceStreamOut();
		}
		else
		{
			// need to evict stuff?
			uint64 CoarseMeshPoolSize = GNaniteCoarseMeshStreamingMemoryPoolSizeInMB * 1024 * 1024;
			if (TotalRequestedRenderAssetSize + RequestStreamInRenderAssetsSize > CoarseMeshPoolSize)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CheckStreamOut);

				// Sort the loaded handles by frame ID so we evict last used ones first
				Algo::Sort(ActiveHandles, [this](CoarseMeshStreamingHandle& LHS, CoarseMeshStreamingHandle& RHS)
					{
						// first by tick index
						if (RegisteredRenderAssets[LHS].LastUsedTickIndex != RegisteredRenderAssets[RHS].LastUsedTickIndex)
						{
							return RegisteredRenderAssets[LHS].LastUsedTickIndex < RegisteredRenderAssets[RHS].LastUsedTickIndex;
						}

						// then by size
						return RegisteredRenderAssets[LHS].StreamingSize > RegisteredRenderAssets[RHS].StreamingSize;
					});

				// Start streaming out until we either have enough memory available, or have reached the current requested frame
				for (CoarseMeshStreamingHandle& Handle : ActiveHandles)
				{
					FRegisteredRenderAsset& RenderAsset = RegisteredRenderAssets[Handle];

					// reach the current tick count, means it's still needed
					if (RenderAsset.LastUsedTickIndex == TickCount)
					{
						break;
					}

					// request stream out if marked as streamed in (otherwise still busy and don't touch the state)
					if (RenderAsset.ResourceState == EResourceState::Loaded)
					{
						RenderAsset.ResourceState = EResourceState::RequestStreamOut;
						TotalRequestedRenderAssetSize -= RenderAsset.StreamingSize;

						// done when we have enough streamed out
						if (TotalRequestedRenderAssetSize + RequestStreamInRenderAssetsSize <= CoarseMeshPoolSize)
						{
							break;
						}
					}
				}
			}

			// sort requests by size (if we don't have enough memory then skip the biggest ones)
			if (TotalRequestedRenderAssetSize + RequestStreamInRenderAssetsSize > CoarseMeshPoolSize)
			{
				Algo::Sort(RequestStreamInRenderAssets, [this](CoarseMeshStreamingHandle& LHS, CoarseMeshStreamingHandle& RHS)
					{
						return RegisteredRenderAssets[LHS].StreamingSize < RegisteredRenderAssets[RHS].StreamingSize;
					});
			}

			// request stream-in ops for new data
			for (CoarseMeshStreamingHandle& Handle : RequestStreamInRenderAssets)
			{
				FRegisteredRenderAsset& StreamInRenderAsset = RegisteredRenderAssets[Handle];

				// can't be loaded anymore?
				if (StreamInRenderAsset.StreamingSize + TotalRequestedRenderAssetSize > CoarseMeshPoolSize)
				{
					break;
				}

				check(StreamInRenderAsset.ResourceState == EResourceState::Unloaded);
				StreamInRenderAsset.ResourceState = EResourceState::RequestStreamIn;

				ActiveHandles.Add(Handle);
				TotalRequestedRenderAssetSize += StreamInRenderAsset.StreamingSize;
			}
		}

#ifdef STATS
		// Update the stats
		SET_DWORD_STAT(STAT_NaniteCoarseMeshStreamingMeshesRequested, RequestedMeshCount);
		SET_MEMORY_STAT(STAT_NaniteCoarseMeshStreamingMeshMemoryRequested, RequestedMemorySize);;
#endif // STATS
	}

	void FCoarseMeshStreamingManager::ForceStreamIn()
	{
		for (FRegisteredRenderAsset& RenderAsset : RegisteredRenderAssets)
		{
			if (RenderAsset.ResourceState == EResourceState::Unloaded)
			{
				RenderAsset.ResourceState = EResourceState::RequestStreamIn;
				ActiveHandles.Add(RenderAsset.Handle);
				TotalRequestedRenderAssetSize += RenderAsset.StreamingSize;
			}
		}
	}

	void FCoarseMeshStreamingManager::ForceStreamOut()
	{
		for (FRegisteredRenderAsset& RenderAsset : RegisteredRenderAssets)
		{
			if (RenderAsset.ResourceState == EResourceState::Loaded)
			{
				RenderAsset.ResourceState = EResourceState::RequestStreamOut;
				TotalRequestedRenderAssetSize -= RenderAsset.StreamingSize;
			}
		}
	}

} // namespace Nanite

