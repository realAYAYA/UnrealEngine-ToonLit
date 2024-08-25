// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGActorAndComponentMapping.h"

#include "PCGComponent.h"
#include "PCGDataAsset.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGGraphExecutionLogging.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "Algo/AnyOf.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceEditorPivotInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Materials/MaterialInterface.h"

namespace PCGActorAndComponentMapping
{
	FBox GetActorBounds(const AActor* InActor)
	{
		if (!InActor)
		{
			return FBox(EForceInit::ForceInit);
		}

		FBox ActorBounds = InActor->GetComponentsBoundingBox();
		if (!ActorBounds.IsValid && InActor->GetRootComponent() != nullptr)
		{
			// Try on the RootComponent
			ActorBounds = InActor->GetRootComponent()->Bounds.GetBox();
		}

		return ActorBounds;
	}

#if WITH_EDITOR
	static TAutoConsoleVariable<bool> CVarDisableObjectDependenciesTracking(
		TEXT("pcg.DisableObjectDependenciesTracking"),
		false,
		TEXT("If depencencies are being unstable, disable the tracking, allowing people to continue working while we investigate."));

	static TAutoConsoleVariable<bool> CVarDisableDelayedActorRegistering(
		TEXT("pcg.DisableDelayedActorRegistering"),
		false,
		TEXT("If delayed actor registering when their components aren't registered yet is introducing bad behavior, disables it, allowing people to continue working while we investigate."));

	static TAutoConsoleVariable<bool> CVarLandscapeDisableRefreshTracking(
		TEXT("pcg.LandscapeDisableRefreshTracking"),
		false,
		TEXT("Completely disable landscape refresh when it changes."));

	static TAutoConsoleVariable<bool> CVarLandscapeDisableRefreshTrackingInLandscapeEditingMode(
		TEXT("pcg.LandscapeDisableRefreshTrackingInLandscapeEditingMode"),
		false,
		TEXT("Disable landscape refresh when it changes in landscape editing mode."));

	static TAutoConsoleVariable<int> CVarLandscapeRefreshTimeDelay(
		TEXT("pcg.LandscapeRefreshTimeDelayMS"),
		1000,
		TEXT("Time in MS between a landscape change and PCG refresh. Set it to 0 or negative value to disable the delay."));

	static TAutoConsoleVariable<int> CVarActorModifiedPreviousDataCleanupDelayMS(
		TEXT("pcg.ActorModifiedPreviousDataCleanupDelayMS"),
		10000,
		TEXT("Time in MS between a cached previous data from modified actor and the time we remove it from our cache."));

	static TAutoConsoleVariable<int> CVarActorModifiedPreviousDataCheckDelayMS(
		TEXT("pcg.ActorModifiedPreviousDataCheckDelayMS"),
		1000,
		TEXT("Delay in MS between cleanup checks on previous data from modified actors."));

	static TAutoConsoleVariable<bool> CVarDisablePCGDataInterdependencyOptimization(
		TEXT("pcg.DisablePCGDataInterdependencyOptimization"),
		false,
		TEXT("Disable the optimization that keep track of components depending on others, as a safety measure."));
#endif // WITH_EDITOR

	static TAutoConsoleVariable<bool> CVarDisableDelayedUnregister(
		TEXT("pcg.DisableDelayedUnregister"),
		true,
		TEXT("If delayed unregister for all is introducing bad behavior, disables it, allowing people to continue working while we investigate."));

#if WITH_EDITOR
	bool ShouldIgnoreActor(AActor* InActor)
	{
		if(!InActor)
		{
			return true;
		}
				
		return InActor->bIsEditorPreviewActor || InActor->IsA<ALevelInstanceEditorInstanceActor>() || InActor->IsA<APCGWorldActor>() || InActor->Implements<ULevelInstanceEditorPivotInterface>();
	}

	void PropagateToLevelInstanceActors(ILevelInstanceInterface* InLevelInstance, TFunctionRef<bool(AActor* LevelActor)> InFunc)
	{
		if (InLevelInstance)
		{
			if (const ULevelInstanceSubsystem* LevelInstanceSubsystem = InLevelInstance->GetLevelInstanceSubsystem())
			{
				LevelInstanceSubsystem->ForEachActorInLevelInstance(InLevelInstance, [InFunc](AActor* LevelActor)
				{
					if (!ShouldIgnoreActor(LevelActor))
					{
						return InFunc(LevelActor);
					}
					return true;
				});
			}
		}
	}
	
	void PropagateToLevelInstanceActors(AActor* InActor, TFunctionRef<bool(AActor* LevelActor)> InFunc)
	{
		PropagateToLevelInstanceActors(Cast<ILevelInstanceInterface>(InActor), InFunc);
	}
		
	bool ShouldDiscardLandscapeRefresh(const ALandscapeProxy* InLandscape, bool& bIsInEditingMode, bool bIsExitingEditingMode)
	{
		bIsInEditingMode = false;
		// If it is not a landscape, we should refresh.
		if (!InLandscape)
		{
			return false;
		}

		// If refresh is globably disabled, never refresh
		if (PCGActorAndComponentMapping::CVarLandscapeDisableRefreshTracking.GetValueOnAnyThread())
		{
			return true;
		}

		// If refresh is not disabled in editing, always refresh
		if (!PCGActorAndComponentMapping::CVarLandscapeDisableRefreshTrackingInLandscapeEditingMode.GetValueOnAnyThread())
		{
			return false;
		}

		// Refresh only if we are not editing.
		const ALandscape* Landscape = InLandscape->GetLandscapeActor();
		bIsInEditingMode = Landscape && Landscape->HasLandscapeEdMode() && !bIsExitingEditingMode;
		return bIsInEditingMode;
	}
#endif
}

FPCGActorAndComponentMapping::FPCGActorAndComponentMapping(UPCGSubsystem* InPCGSubsystem)
	: PCGSubsystem(InPCGSubsystem)
{
	check(PCGSubsystem);

	// TODO: For now we set our octree to be 2km wide, but it would be perhaps better to
	// scale it to the size of our world.
	constexpr FVector::FReal OctreeExtent = 200000; // 2km
	PartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
	NonPartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
}

void FPCGActorAndComponentMapping::Initialize(UWorld* World)
{
#if WITH_EDITOR
	RegisterTrackingCallbacks();

	// Considering that some landscape might already be loaded, we should attach to their change callbacks now
	if (World)
	{
		UPCGActorHelpers::ForEachActorInWorld<ALandscapeProxy>(World, [this](AActor* InActor)
		{
			if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
			{
				if (!LandscapeProxy->OnComponentDataChanged.IsBoundToObject(this))
				{
					LandscapeProxy->OnComponentDataChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnLandscapeChanged);
				}
			}

			return true;
		});
	}
#endif // WITH_EDITOR
}

void FPCGActorAndComponentMapping::Deinitialize()
{
#if WITH_EDITOR
	TeardownTrackingCallbacks();
#endif
}

void FPCGActorAndComponentMapping::Tick()
{
	TSet<UPCGComponent*> ComponentToUnregister;
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		ComponentToUnregister = MoveTemp(DelayedComponentToUnregister);
	}

	for (UPCGComponent* Component : ComponentToUnregister)
	{
		UnregisterPCGComponent(Component, /*bForce=*/true);
	}

#if WITH_EDITOR
	ProcessDelayedEvents();

	const double CurrentTime = FApp::GetCurrentTime();

	if (!DelayedModifiedLandscapes.IsEmpty() && LastLandscapeDirtyTime > 0.0 && ((CurrentTime - LastLandscapeDirtyTime) * 1000.0) > PCGActorAndComponentMapping::CVarLandscapeRefreshTimeDelay.GetValueOnAnyThread())
	{
		LastLandscapeDirtyTime = -1.0;
		for (TObjectKey<ALandscapeProxy> Landscape : DelayedModifiedLandscapes)
		{
			ApplyLandscapeChanges(Landscape.ResolveObjectPtr());
		}

		DelayedModifiedLandscapes.Empty();
	}

	// Cleaning up previous data gathered by the OnObjectModified function but not consumed.
	if (LastPreviousActorDataCleanup < 0.0 || ((CurrentTime - LastPreviousActorDataCleanup) * 1000) > PCGActorAndComponentMapping::CVarActorModifiedPreviousDataCheckDelayMS.GetValueOnAnyThread())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::Tick::PreviousActorDataCleanupCheck);

		LastPreviousActorDataCleanup = CurrentTime;
		TArray<TObjectKey<AActor>> Keys;
		ActorToPreviousDataMap.GetKeys(Keys);
		for (const TObjectKey<AActor>& Key : Keys)
		{
			if (((CurrentTime - ActorToPreviousDataMap[Key].Get<2>()) * 1000) > PCGActorAndComponentMapping::CVarActorModifiedPreviousDataCleanupDelayMS.GetValueOnAnyThread())
			{
				ActorToPreviousDataMap.Remove(Key);
			}
		}
	}

	if (PCGActorAndComponentMapping::CVarDisablePCGDataInterdependencyOptimization.GetValueOnAnyThread() && !ComponentsToDependencyMap.IsEmpty())
	{
		ComponentsToDependencyMap.Empty();
	}
#endif // WITH_EDITOR
}

TArray<FPCGTaskId> FPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunctionRef<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents);
	if (!ensure(OriginalComponent))
	{
		return {};
	}

	const bool bIsRuntimeGenerated = OriginalComponent->IsManagedByRuntimeGenSystem();

	const TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map = bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMap : ComponentToPartitionActorsMap;

	// TODO: Might be more interesting to copy the set and release the lock.
	FReadScopeLock ReadLock(bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMapLock : ComponentToPartitionActorsMapLock);
	const TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(OriginalComponent);

	if (!PartitionActorsPtr)
	{
		return TArray<FPCGTaskId>();
	}

	return DispatchToLocalComponents(OriginalComponent, *PartitionActorsPtr, InFunc);
}

TArray<FPCGTaskId> FPCGActorAndComponentMapping::DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunctionRef<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TArray<FPCGTaskId> TaskIds;
	for (APCGPartitionActor* PartitionActor : PartitionActors)
	{
		if (PartitionActor)
		{
			if (UPCGComponent* LocalComponent = PartitionActor->GetLocalComponent(OriginalComponent))
			{
				// Add check to avoid infinite loop
				if (ensure(!LocalComponent->IsPartitioned()))
				{
					FPCGTaskId LocalTask = InFunc(LocalComponent);

					if (LocalTask != InvalidPCGTaskId)
					{
						TaskIds.Add(LocalTask);
					}
				}
			}
		}
	}

	return TaskIds;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	check(InComponent);

	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return false;
	}

	// Check also that the bounds are valid. If not early out.
	if (!InComponent->GetGridBounds().IsValid)
	{
		UE_LOG(LogPCG, Error, TEXT("[RegisterOrUpdatePCGComponent] Component has invalid bounds, not registered nor updated."));
		return false;
	}

	const bool bWasAlreadyRegistered = IsComponentRegistered(InComponent);

	// First check if the component has changed its partitioned flag.
	const bool bIsPartitioned = InComponent->IsPartitioned();
	if (bIsPartitioned && NonPartitionedOctree.Contains(InComponent))
	{
		UnregisterNonPartitionedPCGComponent(InComponent);
	}
	else if (!bIsPartitioned && PartitionedOctree.Contains(InComponent))
	{
		UnregisterPartitionedPCGComponent(InComponent);
	}

	PCGSubsystem->OnOriginalComponentRegistered(InComponent);

	// Then register/update accordingly
	bool bHasChanged = false;
	if (bIsPartitioned)
	{
		bHasChanged = RegisterOrUpdatePartitionedPCGComponent(InComponent, bDoActorMapping);
	}
	else
	{
		bHasChanged = RegisterOrUpdateNonPartitionedPCGComponent(InComponent);
	}

	// If the component was previously marked as to be unregistered, remove it here.
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		DelayedComponentToUnregister.Remove(InComponent);
	}

	// And finally handle the tracking. Only do it when the component is registered for the first time.
#if WITH_EDITOR
	if (!bWasAlreadyRegistered && bHasChanged)
	{
		RegisterTracking(InComponent);
	}
#endif // WITH_EDITOR

	return bHasChanged;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdatePartitionedPCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	PartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

	// After adding/updating, try to do the mapping (if we asked for it and the component changed)
	if (bDoActorMapping)
	{
		if (bComponentHasChanged)
		{
			UpdateMappingPCGComponentPartitionActor(InComponent);
		}
	}
	else
	{
		if (!bComponentWasAdded)
		{
			// If we do not want a mapping, delete the existing one
			DeleteMappingPCGComponentPartitionActor(InComponent);
		}
	}

	return bComponentHasChanged;
}

bool FPCGActorAndComponentMapping::RegisterOrUpdateNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	NonPartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

	return bComponentHasChanged;
}

bool FPCGActorAndComponentMapping::RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent, bool bDoActorMapping)
{
	check(OldComponent && NewComponent);

	bool bBoundsChanged = false;

	if (OldComponent->IsPartitioned())
	{
		if (!PartitionedOctree.RemapComponent(OldComponent, NewComponent, bBoundsChanged))
		{
			return false;
		}
	}
	else
	{
		if (!NonPartitionedOctree.RemapComponent(OldComponent, NewComponent, bBoundsChanged))
		{
			return false;
		}
	}

	// Remove it from the delayed
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		DelayedComponentToUnregister.Remove(OldComponent);
	}

	// Remap all previous instances
	auto RemapPreviousInstances = [OldComponent, NewComponent](TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock)
	{
		FWriteScopeLock WriteLock(Lock);

		if (TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(OldComponent))
		{
			TSet<TObjectPtr<APCGPartitionActor>> PartitionActorsToRemap = MoveTemp(*PartitionActorsPtr);
			Map.Remove(OldComponent);

			for (APCGPartitionActor* Actor : PartitionActorsToRemap)
			{
				Actor->RemapGraphInstance(OldComponent, NewComponent);
			}

			Map.Add(NewComponent, MoveTemp(PartitionActorsToRemap));
		}
	};

	RemapPreviousInstances(ComponentToPartitionActorsMap, ComponentToPartitionActorsMapLock);
	RemapPreviousInstances(ComponentToRuntimeGenPartitionActorsMap, ComponentToRuntimeGenPartitionActorsMapLock);

	// And update the mapping if bounds changed and we want to do actor mapping
	if (bBoundsChanged && NewComponent->IsPartitioned() && bDoActorMapping)
	{
		UpdateMappingPCGComponentPartitionActor(NewComponent);
	}

#if WITH_EDITOR
	RemapTracking(OldComponent, NewComponent);
#endif // WITH_EDITOR

	return true;
}

void FPCGActorAndComponentMapping::UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce)
{
	if (!InComponent)
	{
		return;
	}

	if ((PartitionedOctree.Contains(InComponent) || NonPartitionedOctree.Contains(InComponent)))
	{
		bool bShouldBeDelayed = true;
		if (PCGActorAndComponentMapping::CVarDisableDelayedUnregister.GetValueOnAnyThread())
		{
			// We also need to check that our current PCG Component is not deleted while being reconstructed by a construction script.
			// If so, it will be "re-created" at some point with the same properties.
			// In this particular case, we don't remove the PCG component from the octree and we won't delete the mapping, but mark it to be removed
			// at next Subsystem tick. If we call "RemapPCGComponent" before, we will re-connect everything correctly.
			// Ignore this if we force (aka when we actually unregister the delayed one)
			bShouldBeDelayed = InComponent->IsCreatedByConstructionScript();
		}

		if (!bForce && bShouldBeDelayed)
		{
			FScopeLock Lock(&DelayedComponentToUnregisterLock);
			DelayedComponentToUnregister.Add(InComponent);
			return;
		}

#if WITH_EDITOR
		UnregisterTracking(InComponent);
#endif // WITH_EDITOR
	}

	UnregisterPartitionedPCGComponent(InComponent);
	UnregisterNonPartitionedPCGComponent(InComponent);

	FScopeLock Lock(&DelayedComponentToUnregisterLock);
	if (DelayedComponentToUnregister.Contains(InComponent))
	{
		DelayedComponentToUnregister.Remove(InComponent);
	}
}

void FPCGActorAndComponentMapping::UnregisterPartitionedPCGComponent(UPCGComponent* InComponent)
{
	PCGSubsystem->OnOriginalComponentUnregistered(InComponent);

	if (!PartitionedOctree.RemoveComponent(InComponent) || InComponent->IsManagedByRuntimeGenSystem())
	{
		return;
	}

	// Because of recursive component deletes actors that has components, we cannot do RemoveGraphInstance
	// inside a lock. So copy the actors to clean up and release the lock before doing RemoveGraphInstance.
	TSet<TObjectPtr<APCGPartitionActor>> PartitionActorsToCleanUp;
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(InComponent);

		if (PartitionActorsPtr)
		{
			PartitionActorsToCleanUp = MoveTemp(*PartitionActorsPtr);
			ComponentToPartitionActorsMap.Remove(InComponent);
		}
	}

	for (APCGPartitionActor* Actor : PartitionActorsToCleanUp)
	{
		Actor->RemoveGraphInstance(InComponent);
	}
}

void FPCGActorAndComponentMapping::UnregisterNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	PCGSubsystem->OnOriginalComponentUnregistered(InComponent);

	NonPartitionedOctree.RemoveComponent(InComponent);
}

void FPCGActorAndComponentMapping::ForAllIntersectingPartitionedComponents(const FBoxCenterAndExtent& InBounds, TFunctionRef<void(UPCGComponent*)> InFunc) const
{
	PartitionedOctree.FindElementsWithBoundsTest(InBounds, [&InFunc](const FPCGComponentRef& ComponentRef)
	{
		InFunc(ComponentRef.Component);
	});
}

void FPCGActorAndComponentMapping::ForAllOriginalComponents(TFunctionRef<void(UPCGComponent*)> InFunc)
{
	for (UPCGComponent* Component : PartitionedOctree.GetAllComponents())
	{
		InFunc(Component);
	}

	for (UPCGComponent* Component : NonPartitionedOctree.GetAllComponents())
	{
		InFunc(Component);
	}
}

TArray<UPCGComponent*> FPCGActorAndComponentMapping::GetAllIntersectingComponents(const FBoxCenterAndExtent& InBounds) const
{
	TArray<UPCGComponent*> Result;
	auto AddToResult = [&Result](const FPCGComponentRef& ComponentRef)
	{
		if (IsValid(ComponentRef.Component))
		{
			Result.Add(ComponentRef.Component);
		}
	};

	PartitionedOctree.FindElementsWithBoundsTest(InBounds, AddToResult);
	NonPartitionedOctree.FindElementsWithBoundsTest(InBounds, AddToResult);

	return Result;
}

void FPCGActorAndComponentMapping::RegisterPartitionActor(APCGPartitionActor* InActor, bool bDoComponentMapping)
{
	check(InActor && PCGSubsystem);

	const uint32 GridSize = InActor->GetPCGGridSize();
	const FIntVector GridCoord = InActor->GetGridCoord();

	check(GridSize > 0);

	const bool bIsRuntimeGenerated = InActor->IsRuntimeGenerated();
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bIsRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;

	{
		FWriteScopeLock WriteLock(bIsRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

		TMap<FIntVector, TObjectPtr<APCGPartitionActor>>& PartitionActorsMapGrid = ActorsMap.FindOrAdd(GridSize);
		if (PartitionActorsMapGrid.Contains(GridCoord))
		{
			return;
		}

		PartitionActorsMapGrid.Add(GridCoord, InActor);
	}

	APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor();
	check(WorldActor);

	// For deprecration: bUse2DGrid is now true by default. But if we already have Partition Actors that were created when the flag was false by default,
	// we keep this flag
	if (WorldActor->bUse2DGrid != InActor->IsUsing2DGrid())
	{
		WorldActor->bUse2DGrid = InActor->IsUsing2DGrid();
	}

	// Register to all the components that intersect with the PA. Ignore for runtime generated, it is handled manually
	if (!bIsRuntimeGenerated)
	{
		WorldActor->AddSerializedPartitionActorRecord({ InActor->PCGGuid, GridSize, GridCoord });

		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingPartitionedComponents(FBoxCenterAndExtent(InActor->GetFixedBounds()), [this, InActor, bDoComponentMapping, WorldActor](UPCGComponent* Component)
		{
			// For each component, do the mapping if we ask it explicitly, or if the component is generated
			if (Component && (bDoComponentMapping || Component->bGenerated))
			{
				if (TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component))
				{
					// TODO: This will need to be revisited when execution domains become a broader concept.
					const bool bSameDomain = !Component->IsManagedByRuntimeGenSystem();

					// Only create the component mapping if actor and component are from the same execution domain.
					if (bSameDomain)
					{
						bool bHasUnbounded = false;
						PCGHiGenGrid::FSizeArray GridSizes;
						ensure(PCGHelpers::GetGenerationGridSizes(Component->GetGraph(), WorldActor, GridSizes, bHasUnbounded));

						// Only create the component mapping if the original component demands the same grid the actor lives on.
						if (GridSizes.Contains(InActor->GetPCGGridSize()))
						{
							// In editor we might load/create partition actors while the component is registering. Because of that,
							// the mapping might not already exists, even if the component is marked generated.
							InActor->AddGraphInstance(Component);
							PartitionActorsPtr->Add(InActor);
						}
					}
				}
			}
		});
	}
}

void FPCGActorAndComponentMapping::UnregisterPartitionActor(APCGPartitionActor* Actor)
{
	check(Actor);

	const FIntVector GridCoord = Actor->GetGridCoord();
	const uint32 GridSize = Actor->GetPCGGridSize();
	if (!ensure(GridSize > 0))
	{
		return;
	}

	const bool bIsRuntimeGenerated = Actor->IsRuntimeGenerated();
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bIsRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;

	if (TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = ActorsMap.Find(GridSize))
	{
		FWriteScopeLock WriteLock(bIsRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);
		PartitionActorsMapGrid->Remove(GridCoord);
	}

	// Unregister from all intersecting components. Ignore for runtime generated, it is handled manually
	if (!bIsRuntimeGenerated)
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingPartitionedComponents(FBoxCenterAndExtent(Actor->GetFixedBounds()), [this, Actor](UPCGComponent* Component)
		{
			TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component);
			if (PartitionActorsPtr)
			{
				PartitionActorsPtr->Remove(Actor);
			}
		});
	}
}

void FPCGActorAndComponentMapping::ForAllIntersectingPartitionActors(const FBox& InBounds, TFunctionRef<void(APCGPartitionActor*)> InFunc) const
{
	// No PCGWorldActor just early out. Same for invalid bounds.
	APCGWorldActor* PCGWorldActor = PCGSubsystem->GetPCGWorldActor();

	if (!PCGWorldActor || !InBounds.IsValid)
	{
		return;
	}

	auto ForAllIntersectingPartitionActorsOfGridSize = [InFunc, PCGWorldActor, &InBounds](const TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock, uint32 GridSize)
	{
		const bool bUse2DGrid = PCGWorldActor->bUse2DGrid;
		FIntVector MinCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Min, GridSize, bUse2DGrid);
		FIntVector MaxCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Max, GridSize, bUse2DGrid);

		FReadScopeLock ReadLock(Lock);

		const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = Map.Find(GridSize);
		if (!PartitionActorsMapGrid || PartitionActorsMapGrid->IsEmpty())
		{
			return;
		}

		for (int32 z = MinCellCoords.Z; z <= MaxCellCoords.Z; z++)
		{
			for (int32 y = MinCellCoords.Y; y <= MaxCellCoords.Y; y++)
			{
				for (int32 x = MinCellCoords.X; x <= MaxCellCoords.X; x++)
				{
					FIntVector CellCoords(x, y, z);
					if (const TObjectPtr<APCGPartitionActor>* ActorPtr = PartitionActorsMapGrid->Find(CellCoords))
					{
						if (APCGPartitionActor* Actor = ActorPtr->Get())
						{
							InFunc(Actor);
						}
					}
				}
			}
		}
	};

	PCGHiGenGrid::FSizeToGuidMap GridSizeToGuid;
	PCGWorldActor->GetSerializedGridGuids(GridSizeToGuid);
	for (const TPair<uint32, FGuid>& SizeAndGuid : GridSizeToGuid)
	{
		const uint32 GridSize = SizeAndGuid.Key;
		ForAllIntersectingPartitionActorsOfGridSize(PartitionActorsMap, PartitionActorsMapLock, GridSize);
	}

	GridSizeToGuid.Empty();
	PCGWorldActor->GetTransientGridGuids(GridSizeToGuid);
	for (const TPair<uint32, FGuid>& SizeAndGuid : GridSizeToGuid)
	{
		const uint32 GridSize = SizeAndGuid.Key;
		ForAllIntersectingPartitionActorsOfGridSize(RuntimeGenPartitionActorsMap, RuntimeGenPartitionActorsMapLock, GridSize);
	}
}

void FPCGActorAndComponentMapping::UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	if (!PCGSubsystem->IsInitialized())
	{
		return;
	}

	check(InComponent);

	// Get the bounds
	FBox Bounds = PartitionedOctree.GetBounds(InComponent);

	if (!Bounds.IsValid)
	{
		return;
	}

	if (const APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor())
	{
		bool bHasUnbounded = false;
		PCGHiGenGrid::FSizeArray GridSizes;
		ensure(PCGHelpers::GetGenerationGridSizes(InComponent->GetGraph(), WorldActor, GridSizes, bHasUnbounded));

		auto UpdateMapping = [this, InComponent, &Bounds, WorldActor, &GridSizes](bool bIsRuntimeGenerated)
		{
			TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map = bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMap : ComponentToPartitionActorsMap;
			TSet<TObjectPtr<APCGPartitionActor>> RemovedActors;

			{
				FWriteScopeLock WriteLock(bIsRuntimeGenerated ? ComponentToRuntimeGenPartitionActorsMapLock : ComponentToPartitionActorsMapLock);
				TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(InComponent);

				if (!PartitionActorsPtr)
				{
					// Does not yet exists, add it
					PartitionActorsPtr = &Map.Emplace(InComponent);
					check(PartitionActorsPtr);
				}

				TSet<TObjectPtr<APCGPartitionActor>> NewMapping;
				ForAllIntersectingPartitionActors(Bounds, [&NewMapping, InComponent, WorldActor, &GridSizes, bIsRuntimeGenerated](APCGPartitionActor* Actor)
				{
					if (!Actor)
					{
						return;
					}

					// TODO: This will need to be revisited when execution domains become a broader concept.
					const bool bSameDomain = Actor->IsRuntimeGenerated() == bIsRuntimeGenerated;

					// Only add a graph instance to partition actors that are:
					// * In the same execution domain as the original component.
					// * On a valid grid for the original component.
					if (bSameDomain && GridSizes.Contains(Actor->GetPCGGridSize()))
					{
						Actor->AddGraphInstance(InComponent);
						NewMapping.Add(Actor);
					}
				});

				// Find the ones that were removed
				RemovedActors = PartitionActorsPtr->Difference(NewMapping);

				*PartitionActorsPtr = MoveTemp(NewMapping);
			}

			// No need to be locked to do this.
			for (APCGPartitionActor* RemovedActor : RemovedActors)
			{
				if (RemovedActor)
				{
					RemovedActor->RemoveGraphInstance(InComponent);
				}
			}
		};

		UpdateMapping(InComponent->IsManagedByRuntimeGenSystem());
	}
}

void FPCGActorAndComponentMapping::DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	check(InComponent);

	if (!InComponent->IsPartitioned())
	{
		return;
	}

	auto DeleteMapping = [this, InComponent](TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>>& Map, FRWLock& Lock)
	{
		FWriteScopeLock WriteLock(Lock);

		if (TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = Map.Find(InComponent))
		{
			for (APCGPartitionActor* Actor : *PartitionActorsPtr)
			{
				Actor->RemoveGraphInstance(InComponent);
			}

			PartitionActorsPtr->Empty();
		}
	};

	DeleteMapping(ComponentToPartitionActorsMap, ComponentToPartitionActorsMapLock);
	DeleteMapping(ComponentToRuntimeGenPartitionActorsMap, ComponentToRuntimeGenPartitionActorsMapLock);
}

bool FPCGActorAndComponentMapping::IsComponentRegistered(const UPCGComponent* InComponent) const
{
	return PartitionedOctree.Contains(InComponent) || NonPartitionedOctree.Contains(InComponent);
}

bool FPCGActorAndComponentMapping::AnyRuntimeGenComponentsExist() const
{
	for (UPCGComponent* Component : PartitionedOctree.GetAllComponents())
	{
		if (Component && Component->IsManagedByRuntimeGenSystem())
		{
			return true;
		}
	}

	for (UPCGComponent* Component : NonPartitionedOctree.GetAllComponents())
	{
		if (Component && Component->IsManagedByRuntimeGenSystem())
		{
			return true;
		}
	}

	return false;
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredPartitionedComponents() const
{
	return PartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredNonPartitionedComponents() const
{
	return NonPartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> FPCGActorAndComponentMapping::GetAllRegisteredComponents() const
{
	TSet<UPCGComponent*> Res = GetAllRegisteredPartitionedComponents();
	Res.Append(GetAllRegisteredNonPartitionedComponents());
	return Res;
}

UPCGComponent* FPCGActorAndComponentMapping::GetLocalComponent(uint32 GridSize, const FIntVector& CellCoords, const UPCGComponent* InOriginalComponent, bool bRuntimeGenerated) const
{
	const TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& ActorsMap = bRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;
	FReadScopeLock ReadLock(bRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

	if (const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsOnGrid = ActorsMap.Find(GridSize))
	{
		const TObjectPtr<APCGPartitionActor>* PartitionActor = PartitionActorsOnGrid->Find(CellCoords);
		if (PartitionActor && *PartitionActor)
		{
			return (*PartitionActor)->GetLocalComponent(InOriginalComponent);
		}
	}

	return nullptr;
}

APCGPartitionActor* FPCGActorAndComponentMapping::GetPartitionActor(uint32 GridSize, const FIntVector& CellCoords, bool bRuntimeGenerated) const
{
	const TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>>& Map = bRuntimeGenerated ? RuntimeGenPartitionActorsMap : PartitionActorsMap;
	FReadScopeLock ReadLock(bRuntimeGenerated ? RuntimeGenPartitionActorsMapLock : PartitionActorsMapLock);

	if (const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsOnGrid = Map.Find(GridSize))
	{
		const TObjectPtr<APCGPartitionActor>* PartitionActor = PartitionActorsOnGrid->Find(CellCoords);
		return PartitionActor ? *PartitionActor : nullptr;
	}

	return nullptr;
}

#if WITH_EDITOR
void FPCGActorAndComponentMapping::RegisterTracking(UPCGComponent* InComponent)
{
	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return;
	}

	AActor* ComponentOwner = InComponent->GetOwner();

	// If we have no owner, we might be in a BP so don't track
	if (!ComponentOwner)
	{
		return;
	}

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;

	if (!World)
	{
		return;
	}

	// Components owner needs to be always tracked
	AlwaysTrackedKeysToComponentsMap.FindOrAdd(FPCGSelectionKey(EPCGActorFilter::Self)).Add(InComponent);

	UpdateTracking(InComponent, /*bInShouldDirtyActors=*/ false);

	// Add tracking for when the graph was generated/cleaned, only once
	if (!InComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InComponent->OnPCGGraphStartGeneratingDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphStartsGenerating);
		InComponent->OnPCGGraphCancelledDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphCancelled);
	}
}

void FPCGActorAndComponentMapping::UpdateTracking(UPCGComponent* InComponent, bool bInShouldDirtyActors, const TArray<FPCGSelectionKey>* ChangedKeys)
{
	// Discard BP templates, local components and invalid component
	if (!IsValid(InComponent) || !InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>())
	{
		return;
	}

	AActor* ComponentOwner = InComponent->GetOwner();

	// If we have no owner, we might be in a BP so don't track
	if (!ComponentOwner)
	{
		return;
	}

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;

	if (!World)
	{
		return;
	}

	// If no keys are provided, update all tracking keys.
	TArray<FPCGSelectionKey> AllKeys;
	if (ChangedKeys == nullptr)
	{
		AllKeys = InComponent->GatherTrackingKeys();
		ChangedKeys = &AllKeys;
	}

	check(ChangedKeys);

	auto RemoveFromMap = [InComponent](TMap<FPCGSelectionKey, TSet<UPCGComponent*>>& InMap, const FPCGSelectionKey& InKey)
	{
		if (TSet<UPCGComponent*>* Components = InMap.Find(InKey))
		{
			Components->Remove(InComponent);
			if (Components->IsEmpty())
			{
				InMap.Remove(InKey);
			}
		}
	};

	for (const FPCGSelectionKey& Key : *ChangedKeys)
	{
		// bShouldBeCulled is modified in IsKeyTrackedAndCulled
		bool bShouldBeCulled = false;
		if (!InComponent->IsKeyTrackedAndCulled(Key, bShouldBeCulled))
		{
			// Untrack
			RemoveFromMap(CulledTrackedKeysToComponentsMap, Key);
			RemoveFromMap(AlwaysTrackedKeysToComponentsMap, Key);
		}
		else if (bShouldBeCulled)
		{
			RemoveFromMap(AlwaysTrackedKeysToComponentsMap, Key);
			CulledTrackedKeysToComponentsMap.FindOrAdd(Key).Add(InComponent);
		}
		else
		{
			RemoveFromMap(CulledTrackedKeysToComponentsMap, Key);
			AlwaysTrackedKeysToComponentsMap.FindOrAdd(Key).Add(InComponent);
		}
	}
}

void FPCGActorAndComponentMapping::RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent)
{
	auto ReplaceInMap = [InOldComponent, InNewComponent](TMap<FPCGSelectionKey, TSet<UPCGComponent*>>& InMap)
	{
		for (TPair<FPCGSelectionKey, TSet<UPCGComponent*>>& It : InMap)
		{
			if (It.Value.Remove(InOldComponent) > 0)
			{
				It.Value.Add(InNewComponent);
			}
		}
	};

	ReplaceInMap(CulledTrackedKeysToComponentsMap);
	ReplaceInMap(AlwaysTrackedKeysToComponentsMap);

	// If this component has dependencies, we transfer them.
	if (ComponentsToDependencyMap.Contains(InOldComponent))
	{
		TArray<TObjectKey<UPCGComponent>> Temp;
		ComponentsToDependencyMap.RemoveAndCopyValue(InOldComponent, Temp);
		ComponentsToDependencyMap.Emplace(InNewComponent, std::move(Temp));
	}

	// If this component was a dependency to any other component, just remove it. New one will register itself when generating if needed.
	for (TPair<TObjectKey<UPCGComponent>, TArray<TObjectKey<UPCGComponent>>>& It : ComponentsToDependencyMap)
	{
		It.Value.RemoveSwap(InOldComponent);
	}

	// Old component will probably die, but we'll force removing the delegates even if it is const.
	if (UPCGComponent* MutableOldComponent = const_cast<UPCGComponent*>(InOldComponent))
	{
		MutableOldComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		MutableOldComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
		MutableOldComponent->OnPCGGraphStartGeneratingDelegate.RemoveAll(this);
		MutableOldComponent->OnPCGGraphCancelledDelegate.RemoveAll(this);
	}

	// And just making sure we are not registering multiple times
	if (!InNewComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InNewComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InNewComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InNewComponent->OnPCGGraphStartGeneratingDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphStartsGenerating);
		InNewComponent->OnPCGGraphCancelledDelegate.AddRaw(this, &FPCGActorAndComponentMapping::OnPCGGraphCancelled);
	}
}

void FPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent, const TSet<FPCGSelectionKey>* OptionalKeysToUntrack)
{
	if (!InComponent)
	{
		return;
	}

	TSet<FPCGSelectionKey> KeysToRemove;
	auto RemoveAllFromMap = [InComponent, &KeysToRemove](TMap<FPCGSelectionKey, TSet<UPCGComponent*>>& InMap)
	{
		for (auto& It : InMap)
		{
			It.Value.Remove(InComponent);
			if (It.Value.IsEmpty())
			{
				KeysToRemove.Add(It.Key);
			}
		}
	};

	auto RemoveKeysFromMap = [InComponent, &KeysToRemove](TMap<FPCGSelectionKey, TSet<UPCGComponent*>>& InMap, const TSet<FPCGSelectionKey>& SetToIterateOn)
	{
		for (const FPCGSelectionKey& KeyIt : SetToIterateOn)
		{
			if (TSet<UPCGComponent*>* ComponentSetPtr = InMap.Find(KeyIt))
			{
				ComponentSetPtr->Remove(InComponent);
				if (ComponentSetPtr->IsEmpty())
				{
					KeysToRemove.Add(KeyIt);
				}
			}
		}
	};

	if (OptionalKeysToUntrack)
	{
		RemoveKeysFromMap(CulledTrackedKeysToComponentsMap, *OptionalKeysToUntrack);
		RemoveKeysFromMap(AlwaysTrackedKeysToComponentsMap, *OptionalKeysToUntrack);
	}
	else
	{
		RemoveAllFromMap(CulledTrackedKeysToComponentsMap);
		RemoveAllFromMap(AlwaysTrackedKeysToComponentsMap);
	}

	for (const FPCGSelectionKey& Key : KeysToRemove)
	{
		CulledTrackedKeysToComponentsMap.Remove(Key);
		AlwaysTrackedKeysToComponentsMap.Remove(Key);
	}
}

void FPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	UnregisterTracking(InComponent, nullptr);

	InComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	InComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
	InComponent->OnPCGGraphStartGeneratingDelegate.RemoveAll(this);
	InComponent->OnPCGGraphCancelledDelegate.RemoveAll(this);
}

bool FPCGActorAndComponentMapping::IsKeyTracked(const FPCGSelectionKey& InKey) const
{
	return CulledTrackedKeysToComponentsMap.Contains(InKey) || AlwaysTrackedKeysToComponentsMap.Contains(InKey);
}

bool FPCGActorAndComponentMapping::IsActorTracked(const AActor* InActor) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::IsActorTracked);

	TSet<FName> EmptySet;

	auto Matching = [InActor, &EmptySet](const TPair<FPCGSelectionKey, TSet<UPCGComponent*>>& It) -> bool
	{
		return It.Key.IsMatching(InActor, EmptySet, It.Value, nullptr);
	};

	return Algo::AnyOf(CulledTrackedKeysToComponentsMap, Matching) || Algo::AnyOf(AlwaysTrackedKeysToComponentsMap, Matching);
}

void FPCGActorAndComponentMapping::ResetPartitionActorsMap()
{
	PartitionActorsMapLock.WriteLock();
	PartitionActorsMap.Empty();
	PartitionActorsMapLock.WriteUnlock();
}

void FPCGActorAndComponentMapping::RegisterTrackingCallbacks()
{
	GEngine->OnLevelActorAdded().AddRaw(this, &FPCGActorAndComponentMapping::OnActorAdded);
	GEngine->OnLevelActorDeleted().AddRaw(this, &FPCGActorAndComponentMapping::OnActorDeleted);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnObjectPropertyChanged);
	FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FPCGActorAndComponentMapping::OnObjectModified);
	FCoreUObjectDelegates::OnObjectPreSave.AddRaw(this, &FPCGActorAndComponentMapping::OnObjectSaved);

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;
	// Need the World condition for static analysis...
	if (World && IsValid(World) && World->PersistentLevel)
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddRaw(this, &FPCGActorAndComponentMapping::OnActorLoaded);
		World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.AddRaw(this, &FPCGActorAndComponentMapping::OnActorUnloaded);

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(World))
		{
			LevelInstanceSubsystem->OnLevelInstancesUpdated().AddRaw(this, &FPCGActorAndComponentMapping::OnLevelInstancesUpdated);
		}
	}
}

void FPCGActorAndComponentMapping::TeardownTrackingCallbacks()
{
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);

	UWorld* World = PCGSubsystem ? PCGSubsystem->GetWorld() : nullptr;
	// Need the World condition for static analysis...
	if (World && IsValid(World) && World->PersistentLevel)
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
		World->PersistentLevel->OnLoadedActorRemovedFromLevelEvent.RemoveAll(this);

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(World))
		{
			LevelInstanceSubsystem->OnLevelInstancesUpdated().RemoveAll(this);
		}
	}

	TSet<UPCGComponent*> StillRegisteredComponents = GetAllRegisteredComponents();
	// FIXME: When the changes to unregister will go in, we can enable this ensure. At the moment, we didn't unregister when the world is shutdown
	// because the subsystem is dead before we destroy the component. So it is expected to have still all of those components registered at that point.
	//if (!ensureMsgf(StillRegisteredComponents.IsEmpty(), TEXT("Found components not unregistered during PCG subsystem shutdown.")))
	{
		for (UPCGComponent* Component : StillRegisteredComponents)
		{
			if (IsValid(Component))
			{
				Component->OnPCGGraphGeneratedDelegate.RemoveAll(this);
				Component->OnPCGGraphCleanedDelegate.RemoveAll(this);
				Component->OnPCGGraphStartGeneratingDelegate.RemoveAll(this);
				Component->OnPCGGraphCancelledDelegate.RemoveAll(this);
			}
		}
	}
}

bool FPCGActorAndComponentMapping::ShouldDelayActor(AActor* InActor) const
{
	const bool bDisableDelayedActorRegistering = PCGActorAndComponentMapping::CVarDisableDelayedActorRegistering.GetValueOnAnyThread();
	if (!InActor->HasActorRegisteredAllComponents() && !bDisableDelayedActorRegistering)
	{
		return true;
	}

	bool bShouldDelay = false;

	// Check that whole Level Instance hierarchy is loaded
	if (ILevelInstanceInterface* LevelInstance = Cast<ILevelInstanceInterface>(InActor))
	{
		// If Level Instance isn't loaded yet it should be delayed
		if (!LevelInstance->IsLoaded() && !InActor->IsA<APackedLevelActor>())
		{
			return true;
		}
			
		PCGActorAndComponentMapping::PropagateToLevelInstanceActors(LevelInstance, [this, &bShouldDelay](AActor* InChildActor)
		{
			if(ShouldDelayActor(InChildActor))
			{
				bShouldDelay = true;
				return false;
			}
			return true;
		});
	}

	return bShouldDelay;
}

void FPCGActorAndComponentMapping::ProcessDelayedEvents()
{
	// Safeguard, we can't add delayed actors if the subsystem is not initialized
	if (!PCGSubsystem || !PCGSubsystem->IsInitialized() || DelayedChangedActors.IsEmpty())
	{
		return;
	}
			
	TMap<TObjectKey<AActor>, TObjectKey<UObject>> LocalDelayedChangedActors = MoveTemp(DelayedChangedActors);

	for (const TPair<TObjectKey<AActor>, TObjectKey<UObject>>& ActorToObject : LocalDelayedChangedActors)
	{
		AActor* Actor = ActorToObject.Key.ResolveObjectPtr();
		UObject* Object = ActorToObject.Value.ResolveObjectPtr();
		if (!Actor)
		{
			continue;
		}

		OnActorChanged_Internal(Actor, Object);
	}
}

void FPCGActorAndComponentMapping::OnLevelInstancesUpdated(const TArray<ILevelInstanceInterface*>& InLevelInstances)
{
	for(ILevelInstanceInterface* LevelInstance : InLevelInstances)
	{
		OnActorAdded(Cast<AActor>(LevelInstance));
	}
}

void FPCGActorAndComponentMapping::OnActorLoaded(AActor& InActor)
{
	// We have to make sure to not create a infinite loop
	if (PCGActorAndComponentMapping::ShouldIgnoreActor(&InActor) || !PCGSubsystem || InActor.GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	// Loaded actors should not dirty.
	OnActorAdded_Internal(&InActor, /*bShouldDirty=*/ false);
}

void FPCGActorAndComponentMapping::OnActorAdded(AActor* InActor)
{
	// We have to make sure to not create a infinite loop
	if (PCGActorAndComponentMapping::ShouldIgnoreActor(InActor) || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}
			
	// Implementation note: We delay adding because OnActorAdded fires before an actor's properties are set,
	// so the actor is not ready for processing until the next tick.
	OnActorAdded_Internal(InActor, /*bShouldDirty=*/ true);
}

void FPCGActorAndComponentMapping::OnActorChanged_Recursive(AActor* InActor, UObject* InOriginatingChangeObject)
{
	check(!DelayedChangedActors.Contains(InActor));

	// Process actors recursively
	auto OnActorChanged = [this, InOriginatingChangeObject](AActor* InActor, auto OnActorChangedRecursive) -> void
	{
		if (InActor)
		{
			FActorPreviousData* PreviousData = ActorToPreviousDataMap.Find(InActor);
			OnObjectChanged(InActor, PreviousData, InOriginatingChangeObject);
			if (PreviousData)
			{
				ActorToPreviousDataMap.Remove(InActor);
			}
		}
		
		PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, [this, OnActorChangedRecursive](AActor* LevelActor)
		{
			OnActorChangedRecursive(LevelActor, OnActorChangedRecursive);
			return true;
		});
	};

	OnActorChanged(InActor, OnActorChanged);
}

void FPCGActorAndComponentMapping::OnActorAdded_Internal(AActor* InActor, bool bShouldDirty)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorAdded);
	check(!PCGActorAndComponentMapping::ShouldIgnoreActor(InActor) && PCGSubsystem && InActor->GetWorld() == PCGSubsystem->GetWorld());

	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
	{
		if (!LandscapeProxy->OnComponentDataChanged.IsBoundToObject(this))
		{
			LandscapeProxy->OnComponentDataChanged.AddRaw(this, &FPCGActorAndComponentMapping::OnLandscapeChanged);
		}
	}

	if (!bShouldDirty || DelayedChangedActors.Contains(InActor))
	{
		// Nothing to do
		return;
	}

	// We delay adding because OnActorAdded fires before an actor's properties are set,
	// so the actor is not ready for processing until the next tick.
	DelayedChangedActors.Add(InActor, nullptr);
}

void FPCGActorAndComponentMapping::OnActorChanged_Internal(AActor* InActor, UObject* InOriginatingChangeObject)
{
	// Currently delayed, return
	if (DelayedChangedActors.Contains(InActor))
	{
		return;
	}

	// Should still be delayed, add to delayed actors, return
	if (ShouldDelayActor(InActor))
	{
		DelayedChangedActors.Add(InActor, InOriginatingChangeObject);
		return;
	}
	
	// Call Recursive method
	OnActorChanged_Recursive(InActor, InOriginatingChangeObject);
}

void FPCGActorAndComponentMapping::OnActorUnloaded(AActor& InActor)
{
	// Don't dirty on unload (to mirror the behavior in load)
	OnActorDeleted_Internal(&InActor, /*bShouldDirty=*/false);
}

void FPCGActorAndComponentMapping::OnActorDeleted(AActor* InActor)
{
	if (PCGActorAndComponentMapping::ShouldIgnoreActor(InActor) || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	// Implementation note: since this is called only for actors directly in the current level, the depth here is 0.
	OnActorDeleted_Internal(InActor, /*bShouldDirty=*/true);
}

void FPCGActorAndComponentMapping::OnActorDeleted_Internal(AActor* InActor, bool bShouldDirty)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorDeleted);
	check(InActor && PCGSubsystem && InActor->GetWorld() == PCGSubsystem->GetWorld());

	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
	{
		LandscapeProxy->OnComponentDataChanged.RemoveAll(this);
	}

	if (!bShouldDirty)
	{
		// Nothing to do
		return;
	}

#if WITH_EDITOR
	PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, [this, bShouldDirty](AActor* LevelActor)
	{
		OnActorDeleted_Internal(LevelActor, bShouldDirty);
		return true;
	});
#endif // WITH_EDITOR

	// Notify all components that the actor has changed (was removed), but the Refresh will only happen AFTER the actor was actually removed from the world (because of delayed refresh).
	OnObjectChanged(InActor, /*InPreviousData=*/nullptr, /*InOriginatingObject=*/nullptr, /*bNoRefreshOnOwner=*/true);
}

void FPCGActorAndComponentMapping::OnObjectModified(UObject* InObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnObjectModified);

	// Nothing to do if we track nothing
	if (CulledTrackedKeysToComponentsMap.IsEmpty() && AlwaysTrackedKeysToComponentsMap.IsEmpty())
	{
		return;
	}

	AActor* Actor = Cast<AActor>(InObject);

	// Otherwise, if it's an actor component, track it as well
	if (!Actor)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(InObject))
		{
			Actor = ActorComponent->GetOwner();
		}
	}

	if (!Actor || (PCGSubsystem && Actor->GetWorld() != PCGSubsystem->GetWorld()))
	{
		return;
	}

#if WITH_EDITOR
	if (PCGActorAndComponentMapping::ShouldIgnoreActor(Actor))
	{
		return;
	}
#endif

	auto StorePreviousData = [this](AActor* InActor, auto RecursiveCall) -> void
	{
		if (!ActorToPreviousDataMap.Contains(InActor))
		{
			if (IsActorTracked(InActor))
			{
				FActorPreviousData& PreviousData = ActorToPreviousDataMap.Add(InActor);
				PreviousData.Get<0>() = PCGActorAndComponentMapping::GetActorBounds(InActor);
				PreviousData.Get<1>() = TSet<FName>(InActor->Tags);
				PreviousData.Get<2>() = FApp::GetCurrentTime();
			}

			// Also propagate the pre-change to all child actors if it is within a level instance.
			PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, [this, RecursiveCall](AActor* LevelActor)
			{
				RecursiveCall(LevelActor, RecursiveCall);
				return true;
			});
		}
	};

	StorePreviousData(Actor, StorePreviousData);
}

void FPCGActorAndComponentMapping::OnObjectSaved(UObject* InObject, FObjectPreSaveContext InObjectSaveContext)
{
	// Nothing to do if we track nothing
	if (CulledTrackedKeysToComponentsMap.IsEmpty() && AlwaysTrackedKeysToComponentsMap.IsEmpty())
	{
		return;
	}

	// Only trigger a refresh on save for limited data classes.
	// At this point in time, We only track data tables and PCG data assets because in most cases we probably will catch other changes with OnObjectPropertyChanged.
	// This is especially important to make sure we don't trigger refresh multiple times (less a problem for PCG assets, but a big problem for data tables).
	if (!InObjectSaveContext.IsProceduralSave() && (Cast<UDataTable>(InObject) || Cast<UPCGDataAsset>(InObject)))
	{
		FPropertyChangedEvent Event{ nullptr };
		OnObjectPropertyChanged(InObject, Event);
	}
}

void FPCGActorAndComponentMapping::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnObjectPropertyChanged);

	// Nothing to do if we track nothing
	if (CulledTrackedKeysToComponentsMap.IsEmpty() && AlwaysTrackedKeysToComponentsMap.IsEmpty())
	{
		return;
	}

	const bool bValueNotInteractive = (InEvent.ChangeType != EPropertyChangeType::Interactive);
	// Special exception for actor tags, as we can't track otherwise an actor "losing" a tag
	const bool bActorTagChange = (InEvent.Property && InEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags));
	// Another special exception for texture/mesh compilation. If the InEvent is empty and the object is a texture/mesh, we ignore it.
	const bool bEventIsEmpty = (InEvent.Property == nullptr) && (InEvent.ChangeType == EPropertyChangeType::Unspecified);
	const bool bIsTextureCompilationResult = bEventIsEmpty && InObject && InObject->IsA<UTexture>()
		&& FTextureCompilingManager::Get().IsCompilingTexture(Cast<UTexture>(InObject));
	// There is no equivalent for StaticMesh to know if we are in PostCompilation, so we assume there are still some meshes to compile (including this one).
	// Might be an over-optimistic approach, might need a revisit.
	const bool bIsStaticMeshCompilationResult = bEventIsEmpty && InObject && InObject->IsA<UStaticMesh>()
		&& FStaticMeshCompilingManager::Get().GetNumRemainingMeshes() > 0;

	// First check if it is an actor
	AActor* Actor = Cast<AActor>(InObject);

	// Otherwise, if it's an actor component, track it as well
	if (!Actor)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(InObject))
		{
			Actor = ActorComponent->GetOwner();
		}
	}

	if (PCGSubsystem && Actor && Actor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

	if (Actor && PCGActorAndComponentMapping::ShouldIgnoreActor(Actor))
	{
		return;
	}

	const bool bNoOperation = (!bValueNotInteractive && !bActorTagChange)
		|| bIsTextureCompilationResult
		|| bIsStaticMeshCompilationResult
		|| (Actor && (DelayedChangedActors.Contains(Actor)));

	if (bNoOperation)
	{
		if (Actor)
		{
			auto RemoveAll = [this](AActor* InActor, auto RecursiveCall) -> void
			{
				ActorToPreviousDataMap.Remove(InActor);
				PCGActorAndComponentMapping::PropagateToLevelInstanceActors(InActor, [this, RecursiveCall](AActor* LevelActor)
				{
					RecursiveCall(LevelActor, RecursiveCall);
					return true;
				});
			};

			RemoveAll(Actor, RemoveAll);
		}
		
		return;
	}

	if (Actor)
	{
		OnActorChanged_Internal(Actor, /*InOriginatingChangeObject=*/ InObject);
	}
	else
	{
		OnObjectChanged(InObject, /*InPreviousData=*/nullptr, /*InOriginatingChangeObject=*/ InObject);
	}
}

void FPCGActorAndComponentMapping::OnObjectChanged(UObject* InObject, const FActorPreviousData* InPreviousData, const UObject* InOriginatingChangeObject, bool bNoRefreshOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnObjectChanged);

	// Nothing to do if we track nothing or there is no object.
	if (!InObject || (CulledTrackedKeysToComponentsMap.IsEmpty() && AlwaysTrackedKeysToComponentsMap.IsEmpty()))
	{
		return;
	}

	// Don't react to what the PCG Component is already reacting to.
	static const TArray<const UClass*> ExcludedClasses =
	{
		UPCGComponent::StaticClass()
	};

	if (Algo::AnyOf(ExcludedClasses, [InObject](const UClass* Class) -> bool { return InObject->IsA(Class); }))
	{
		return;
	}

	const AActor* Actor = Cast<AActor>(InObject);

	ensure(!PCGSubsystem || !Actor || Actor->GetWorld() == PCGSubsystem->GetWorld());

	/** First gather all the components that are tracking this actor */
	TSet<UPCGComponent*> CulledTrackedComponents;
	TSet<UPCGComponent*> AlwaysTrackedComponents;
	TArray<FPCGSelectionKey> MatchedKeys;

	TSet<FName> RemovedTags;
	if (Actor && InPreviousData && !InPreviousData->Get<1>().IsEmpty())
	{
		RemovedTags = InPreviousData->Get<1>().Difference(TSet<FName>(Actor->Tags));
	}

	auto Gather = [InObject, &RemovedTags, &MatchedKeys](TMap<FPCGSelectionKey, TSet<UPCGComponent*>> InMap, TSet<UPCGComponent*>& OutSet)
	{
		for (auto& It : InMap)
		{
			if (It.Key.IsMatching(InObject, RemovedTags, It.Value, &OutSet))
			{
				MatchedKeys.Add(It.Key);
			}
		}
	};

	Gather(CulledTrackedKeysToComponentsMap, CulledTrackedComponents);
	Gather(AlwaysTrackedKeysToComponentsMap, AlwaysTrackedComponents);

	// If this actor is not tracked, just early out
	if (CulledTrackedComponents.IsEmpty() && AlwaysTrackedComponents.IsEmpty())
	{
		return;
	}

	TSet<UPCGComponent*> DirtyComponents;

	EPCGComponentDirtyFlag DirtyFlag = EPCGComponentDirtyFlag::Actor;
	if (InObject->IsA<ALandscapeProxy>())
	{
		DirtyFlag = DirtyFlag | EPCGComponentDirtyFlag::Landscape;
	}

	// We need the actor bounds to know if we are intersecting, for the culled settings to be dirtied.
	const FBox ActorBounds = PCGActorAndComponentMapping::GetActorBounds(Actor);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::AlwaysTrackedUpdate);

		for (UPCGComponent* PCGComponent : AlwaysTrackedComponents)
		{
			if (!PCGComponent || PCGComponent == InOriginatingChangeObject || (bNoRefreshOwner && PCGComponent->GetOwner() == InObject))
			{
				continue;
			}

			UPCGComponent* OriginalComponent = PCGComponent->GetOriginalComponent();

			if (ensure(OriginalComponent) && OriginalComponent->IsIgnoringChangeOrigin(InObject))
			{
				PCGGraphExecutionLogging::LogChangeOriginIgnoredForComponent(InObject, PCGComponent);
				continue;
			}

			// Don't mark "Owner changed" if the change originate from a PCG Component. It will be delegated to the ClearCacheForActor.
			// It is necessary to avoid infine loops when there are multiple PCG components on one actor, and one component was generated.
			const bool bOwnerChanged = (PCGComponent->GetOwner() == InObject) && (!InOriginatingChangeObject || !InOriginatingChangeObject->IsA<UPCGComponent>());
			bool bShouldDirty = bOwnerChanged || (PCGComponent->ShouldTrackLandscape() && InObject->IsA<ALandscapeProxy>());

			// Since when we clear the cache for a settings, we clear it all, it's only necessary to do it once on the original component. And we will dirty all the local components (with dirty dispatch)
			if (!DirtyComponents.Contains(PCGComponent))
			{
				const FBox ComponentBounds = PCGComponent->GetGridBounds();
				const bool bIntersect = bOwnerChanged || ActorBounds.Intersect(ComponentBounds);// || (OldActorBoundsPtr && OldActorBoundsPtr->Intersect(ComponentBounds));

				bShouldDirty |= ClearCacheForKeys(MatchedKeys, PCGComponent, /*bIntersect=*/true, InOriginatingChangeObject);
				if (bShouldDirty)
				{
					PCGComponent->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/true);
					DirtyComponents.Add(PCGComponent);
				}
			}
		}
	}

	if (!CulledTrackedComponents.IsEmpty())
	{
		// Keep a pointer to the current bounds we are looking at for the partition case. Set it to Actorbounds first, then will be set to old bounds if necessary.
		const FBox* CurrentActorBoundsPtr = &ActorBounds;

		// Then do an octree find to get all components that intersect with this actor.
		// If the actor has moved, we also need to find components that intersected with it before
		// We first do it for non-partitioned, then we do it for partitioned
		auto UpdateNonPartitioned = [this, &DirtyComponents, InObject, CulledTrackedComponents, &RemovedTags, DirtyFlag, InOriginatingChangeObject, bNoRefreshOwner, &MatchedKeys](const FPCGComponentRef& ComponentRef) -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::UpdateNonPartitioned);

			// Don't dirty if the component was already dirtied, not tracked, the origin of the change or its owner is the changed actor and we should not refresh.
			if (DirtyComponents.Contains(ComponentRef.Component) ||
				!CulledTrackedComponents.Contains(ComponentRef.Component) ||
				(InOriginatingChangeObject == ComponentRef.Component) ||
				(bNoRefreshOwner && ComponentRef.Component->GetOwner() == InObject))
			{
				return;
			}

			UPCGComponent* OriginalComponent = ComponentRef.Component->GetOriginalComponent();

			if (ensure(OriginalComponent) && OriginalComponent->IsIgnoringChangeOrigin(InObject))
			{
				PCGGraphExecutionLogging::LogChangeOriginIgnoredForComponent(InObject, ComponentRef.Component);
				return;
			}

			bool bShouldDirty = ComponentRef.Component->ShouldTrackLandscape() && InObject->IsA<ALandscapeProxy>();
			bShouldDirty |= ClearCacheForKeys(MatchedKeys, ComponentRef.Component, /*bIntersect=*/true, InOriginatingChangeObject);
			if (bShouldDirty)
			{
				ComponentRef.Component->DirtyGenerated(DirtyFlag);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		NonPartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdateNonPartitioned);

		// For partitioned, we first need check if the original component intersect with the bounds, then forward the dirty call only to locals that intersect with the bounds.
		// Note: CurrentActorBoundsPtr is passed by reference because it will be modified between lambda calls (cf comment above).
		auto UpdatePartitioned = [this, &DirtyComponents, InObject, CulledTrackedComponents, &CurrentActorBoundsPtr, &RemovedTags, DirtyFlag, InOriginatingChangeObject, bNoRefreshOwner, &MatchedKeys](const FPCGComponentRef& ComponentRef)  -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::UpdatePartitioned);

			// Don't dirty if the component is not tracked, the origin of the change or its owner is the changed actor and we should not refresh.
			// We can "re-dirty" it because changes can impact different local components, from the same
			// original component.
			if (!CulledTrackedComponents.Contains(ComponentRef.Component) ||
				(InOriginatingChangeObject == ComponentRef.Component) ||
				(bNoRefreshOwner && ComponentRef.Component->GetOwner() == InObject))
			{
				return;
			}

			UPCGComponent* OriginalComponent = ComponentRef.Component->GetOriginalComponent();

			if (ensure(OriginalComponent) && OriginalComponent->IsIgnoringChangeOrigin(InObject))
			{
				PCGGraphExecutionLogging::LogChangeOriginIgnoredForComponent(InObject, ComponentRef.Component);
				return;
			}

			check(CurrentActorBoundsPtr);
			const FBox ComponentBounds = ComponentRef.Bounds.GetBox();
			const bool bIntersect = CurrentActorBoundsPtr->Intersect(ComponentBounds);
			if (!bIntersect)
			{
				return;
			}

			const FBox Overlap = CurrentActorBoundsPtr->Overlap(ComponentBounds);
			bool bShouldDirty = ComponentRef.Component->ShouldTrackLandscape() && InObject->IsA<ALandscapeProxy>();
			bShouldDirty |= ClearCacheForKeys(MatchedKeys, ComponentRef.Component, /*bIntersect=*/true, InOriginatingChangeObject);
			bool bWasDirtied = false;

			// Since when we clear the cache for a settings, we clear it all, it's only necessary to do it once on the original component, then only dirty the local that intersects.
			if (bShouldDirty)
			{
				ForAllIntersectingPartitionActors(Overlap, [InObject, Component = ComponentRef.Component, &bWasDirtied, DirtyFlag, InOriginatingChangeObject](APCGPartitionActor* InPartitionActor) -> void
				{
					if (UPCGComponent* LocalComponent = InPartitionActor->GetLocalComponent(Component))
					{
						bWasDirtied = true;
						LocalComponent->DirtyGenerated(DirtyFlag);
					}
				});
			}

			if (bWasDirtied)
			{
				// Don't dispatch
				ComponentRef.Component->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/false);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		PartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdatePartitioned);

		// If it has moved, redo it with the old bounds.
		if (InPreviousData)
		{
			const FBox& PreviousBounds = InPreviousData->Get<0>();
			if (PreviousBounds.IsValid && !PreviousBounds.Equals(ActorBounds))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnActorChanged::SecondUpdateHasMoved);

				// Set the actor bounds with the old one, to have the right Overlap in the Partition case.
				CurrentActorBoundsPtr = &PreviousBounds;
				NonPartitionedOctree.FindElementsWithBoundsTest(PreviousBounds, UpdateNonPartitioned);
				PartitionedOctree.FindElementsWithBoundsTest(PreviousBounds, UpdatePartitioned);
			}
		}
	}

	// Finally, dirty all components that always track this actor that are not yet notified.

	// If it is a landscape and we should discard the refresh, early out.
	bool bIsInEditingMode = false;
	ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(InObject);
	if (PCGActorAndComponentMapping::ShouldDiscardLandscapeRefresh(Landscape, bIsInEditingMode, bIsCurrentlyExitingLandscapeEditMode))
	{
		// If we are in editing, keep track of all the dirtied landscape to refresh them when we exit.
		if (bIsInEditingMode)
		{
			DirtiedLandscapes.AddUnique(Landscape);
		}

		return;
	}

	ULevelInstanceSubsystem* LevelInstanceSubsystem = (PCGSubsystem && PCGSubsystem->GetWorld()) ? PCGSubsystem->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	const UPCGComponent* OriginatingComponent = Cast<const UPCGComponent>(InOriginatingChangeObject);

	// And refresh all dirtied components
	for (UPCGComponent* Component : DirtyComponents)
	{
		if (!ensure(Component))
		{
			continue;
		}

		// This part checks if the change originates from a PCG Component. If so, we check the dirty component has no more other PCG dependencies.
		// In that case we can proceed for the refresh, otherwise early out, this will be woken up by another dependency change.
		if (OriginatingComponent)
		{
			if (TArray<TObjectKey<UPCGComponent>>* Dependencies = ComponentsToDependencyMap.Find(Component))
			{
				Dependencies->RemoveSwap(OriginatingComponent);
				if (Dependencies->IsEmpty())
				{
					ComponentsToDependencyMap.Remove(Component);
				}
				else
				{
					continue;
				}
			}
		}

		const bool bOwnerHasChanged = Component->GetOwner() == InObject;

		if ((!bNoRefreshOwner || !bOwnerHasChanged) && (!Component->bOnlyTrackItself || bOwnerHasChanged))
		{
			// When an object changes, we need to make sure that we don't trigger a refresh on PCG components that are "higher" in the
			// level hierarchy, otherwise we will end up generating in the Level Instance level, which is wrong.
			// Note that in some instances (e.g. when something happens at the Level Instance level) we need to make sure that the
			// PCG components higher-up are properly updated, hence the level instance depth
			if (LevelInstanceSubsystem && Actor)
			{
				// Immediate Level Instance Parent (Actor)
				const ILevelInstanceInterface* ActorLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Actor);

				// Immediate Level Instance Parent (PCG Component)
				const ILevelInstanceInterface* ComponentLevelInstance = LevelInstanceSubsystem->GetParentLevelInstance(Component->GetOwner());
								
				// Is the Actor under a Level Instance that is the same or under the Components owning Level Instance
				bool bActorLevelInstanceInComponentLevelInstanceHierarchy = false;

				// Go up Level Instance hierarchy for Actor and see if we are part of the Component's level instance hierarchy, stop if we find an Editing Level instance as we don't want to impact top level 
				// Components from an editing level instance
				if (ComponentLevelInstance)
				{
					LevelInstanceSubsystem->ForEachLevelInstanceAncestors(Actor, [ComponentLevelInstance, &bActorLevelInstanceInComponentLevelInstanceHierarchy](const ILevelInstanceInterface* Ancestor)
					{
						if (Ancestor == ComponentLevelInstance)
						{
							bActorLevelInstanceInComponentLevelInstanceHierarchy = true;
							return false; // stop iterating
						}

						return !Ancestor->IsEditing();
					});
				}
				
				// Actor in same level instance as Component or under the Components Level instance
				if (bActorLevelInstanceInComponentLevelInstanceHierarchy)
				{
					// Allow update if Component is inside Editing level instance or if it is only in preview mode (generates transient data because it is in a non editing Level instance)
					if (!ComponentLevelInstance->IsEditing() && Component->GetEditingMode() != EPCGEditorDirtyMode::Preview)
					{
						continue;
					}
				}
				else if (Actor->IsInEditLevelInstanceHierarchy())
				{
					continue;
				}
				// If ComponentLevelInstance is non-null and Actor Level instance isn't in its hierarchy then skip it
				else if (ActorLevelInstance && ComponentLevelInstance != nullptr)
				{
					continue;
				}
			}

			Component->Refresh();
		}
	}
}

void FPCGActorAndComponentMapping::OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams)
{
	if (!InLandscape)
	{
		return;
	}

	if (PCGActorAndComponentMapping::CVarLandscapeRefreshTimeDelay.GetValueOnAnyThread() > 0)
	{
		LastLandscapeDirtyTime = FApp::GetCurrentTime();
		DelayedModifiedLandscapes.AddUnique(InLandscape);
	}
	else
	{
		ApplyLandscapeChanges(InLandscape);
	}
}

void FPCGActorAndComponentMapping::ApplyLandscapeChanges(ALandscapeProxy* InLandscape)
{
	if (!InLandscape)
	{
		return;
	}
	
	OnObjectChanged(InLandscape, /*InPreviousData=*/nullptr, InLandscape);
}

void FPCGActorAndComponentMapping::OnPCGGraphStartsGenerating(UPCGComponent* InComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGActorAndComponentMapping::OnPCGGraphStartsGenerating);

	if (!InComponent || !InComponent->GetOwner() || PCGActorAndComponentMapping::CVarDisablePCGDataInterdependencyOptimization.GetValueOnAnyThread())
	{
		return;
	}

	// When a graph starts generating, look for component that depends on it and keep a count.
	// When this graph will be done generating, we'll trigger a dependency generation. But if multiple graphs are generated at the same time and 
	// they all contribute to the same dependency, we'll trigger the dependency only when all the graphs are done generating
	// We only need to gather components that depends on PCG Data, because other dependency changes will be caught by other engine callbacks (such as OnObjectPropertyChanged)
	TSet<UPCGComponent*> TrackedComponents;
	TSet<FName> RemovedTags;

	const FBox ComponentBounds = InComponent->GetGridBounds();

	auto IsKeyTrackingPCGData = [](const FPCGSelectionKey& InKey) { return InKey.OptionalExtraDependency && InKey.OptionalExtraDependency->IsChildOf(UPCGComponent::StaticClass()); };

	for (TPair<FPCGSelectionKey, TSet<UPCGComponent*>>& It : CulledTrackedKeysToComponentsMap)
	{
		if (!IsKeyTrackingPCGData(It.Key))
		{
			continue;
		}

		TSet<UPCGComponent*> TempTrackedComponents;
		It.Key.IsMatching(InComponent->GetOwner(), RemovedTags, It.Value, &TempTrackedComponents);

		// Removing all components that aren't intersecting with it, since it won't contribute to refresh
		for (UPCGComponent* TrackedComponent : TempTrackedComponents)
		{
			if (ensure(TrackedComponent) && TrackedComponent->GetOwner() && PCGActorAndComponentMapping::GetActorBounds(TrackedComponent->GetOwner()).Intersect(ComponentBounds))
			{
				TrackedComponents.Add(TrackedComponent);
			}
		}
	}

	for (TPair<FPCGSelectionKey, TSet<UPCGComponent*>>& It : AlwaysTrackedKeysToComponentsMap)
	{
		if (!IsKeyTrackingPCGData(It.Key))
		{
			continue;
		}

		It.Key.IsMatching(InComponent->GetOwner(), RemovedTags, It.Value, &TrackedComponents);
	}

	for (UPCGComponent* Component : TrackedComponents)
	{
		// Don't have a dependency on itself.
		if (!Component || Component == InComponent)
		{
			continue;
		}

		// If the tracked component is currently ignoring the refresh from InComponent, don't add to the dependencies
		if (Component->IsIgnoringChangeOrigin(InComponent->GetOwner()))
		{
			continue;
		}

		TArray<TObjectKey<UPCGComponent>>& CurrentDependencies = ComponentsToDependencyMap.FindOrAdd(Component);
		CurrentDependencies.AddUnique(InComponent);
	}
}

void FPCGActorAndComponentMapping::OnPCGGraphCancelled(UPCGComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	TArray<TObjectKey<UPCGComponent>> Keys;
	ComponentsToDependencyMap.GetKeys(Keys);
	for (const TObjectKey<UPCGComponent>& Key : Keys)
	{
		TArray<TObjectKey<UPCGComponent>>& Value = ComponentsToDependencyMap[Key];
		Value.Remove(InComponent);
		if (Value.IsEmpty())
		{
			ComponentsToDependencyMap.Remove(Key);
		}
	}
}

void FPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned(UPCGComponent* InComponent)
{
	if (!InComponent || !InComponent->GetOwner())
	{
		return;
	}

	OnObjectChanged(InComponent->GetOwner(), /*InPreviousData=*/nullptr, InComponent);
}

bool FPCGActorAndComponentMapping::ClearCacheForKeys(const TArray<FPCGSelectionKey>& InKeys, const UPCGComponent* InComponent, const bool bIntersect, const UObject* InOriginatingChange) const
{
	check(InComponent);

	if (!PCGSubsystem)
	{
		return false;
	}

	bool bShouldDirty = InComponent->ShouldTrackLandscape() && InOriginatingChange && InOriginatingChange->IsA<ALandscapeProxy>();
	auto ClearCache = [this, InOriginatingChange, bIntersect, &bShouldDirty](const FPCGSelectionKey& Key, const FPCGSettingsAndCulling& SettingsAndCulling)
	{
		if (!SettingsAndCulling.Key.IsValid() || (SettingsAndCulling.Value && !bIntersect))
		{
			return;
		}

		// Extra care if the change originates from a PCGComponent. Only dirty if we are tracking a PCG component.
		if (InOriginatingChange && InOriginatingChange->IsA<UPCGComponent>()
			&& (!Key.OptionalExtraDependency || !Key.OptionalExtraDependency->IsChildOf(UPCGComponent::StaticClass())))
		{
			return;
		}

		bShouldDirty = true;
		const UPCGSettings* Settings = SettingsAndCulling.Key.Get();
		PCGSubsystem->CleanFromCache(Settings->GetElement().Get(), Settings);
	};

	for (const FPCGSelectionKey& Key : InKeys)
	{
		InComponent->ApplyToEachSettings(Key, ClearCache);
	}

	return bShouldDirty;
}

void FPCGActorAndComponentMapping::NotifyLandscapeEditModeExited()
{
	bIsCurrentlyExitingLandscapeEditMode = true;
	// When the landscape edit mode is exited, force the refresh on all modified/dirtied landscapes.
	for (TObjectKey<ALandscapeProxy> Landscape : DelayedModifiedLandscapes)
	{
		DirtiedLandscapes.AddUnique(Landscape);
	}

	DelayedModifiedLandscapes.Empty();

	for (TObjectKey<ALandscapeProxy> Landscape : DirtiedLandscapes)
	{
		ApplyLandscapeChanges(Landscape.ResolveObjectPtr());
	}

	DirtiedLandscapes.Empty();

	bIsCurrentlyExitingLandscapeEditMode = false;
}
#endif // WITH_EDITOR
