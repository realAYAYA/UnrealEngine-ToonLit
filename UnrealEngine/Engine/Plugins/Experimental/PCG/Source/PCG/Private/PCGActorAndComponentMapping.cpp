// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGActorAndComponentMapping.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "LandscapeProxy.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

namespace PCGActorAndComponentMapping
{
	FBox GetActorBounds(const AActor* InActor)
	{
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
#endif // WITH_EDITOR

	static TAutoConsoleVariable<bool> CVarDisableDelayedUnregister(
		TEXT("pcg.DisableDelayedUnregister"),
		false,
		TEXT("If delayed unregister for all is introducing bad behavior, disables it, allowing people to continue working while we investigate."));
}

UPCGActorAndComponentMapping::UPCGActorAndComponentMapping(UPCGSubsystem* InPCGSubsystem)
	: PCGSubsystem(InPCGSubsystem)
{
	check(PCGSubsystem);

	// TODO: For now we set our octree to be 2km wide, but it would be perhaps better to
	// scale it to the size of our world.
	constexpr FVector::FReal OctreeExtent = 200000; // 2km
	PartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
	NonPartitionedOctree.Reset(FVector::ZeroVector, OctreeExtent);
}

void UPCGActorAndComponentMapping::Tick()
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
	AddDelayedActors();
#endif // WITH_EDITOR
}

TArray<FPCGTaskId> UPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::DispatchToRegisteredLocalComponents);

	// TODO: Might be more interesting to copy the set and release the lock.
	FReadScopeLock ReadLock(ComponentToPartitionActorsMapLock);
	const TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(OriginalComponent);

	if (!PartitionActorsPtr)
	{
		return TArray<FPCGTaskId>();
	}

	return DispatchToLocalComponents(OriginalComponent, *PartitionActorsPtr, InFunc);
}

TArray<FPCGTaskId> UPCGActorAndComponentMapping::DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
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

bool UPCGActorAndComponentMapping::RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
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

	const bool bWasAlreadyRegistered = NonPartitionedOctree.Contains(InComponent) || PartitionedOctree.Contains(InComponent);

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
		RegisterOrUpdateTracking(InComponent, /*bInShouldDirtyActors=*/ false);
	}
#endif // WITH_EDITOR

	return bHasChanged;
}

bool UPCGActorAndComponentMapping::RegisterOrUpdatePartitionedPCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	PartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

#if WITH_EDITOR
	// In Editor only, we will create new partition actors depending on the new bounds
	// TODO: For now it will always create the PA. But if we want to create them only when we generate, we need to make
	// sure to update the runtime flow, for them to also create PA if they need to.
	if (bComponentHasChanged || bComponentWasAdded)
	{
		bool bHasUnbounded = false;
		PCGHiGenGrid::FSizeArray GridSizes;
		ensure(PCGHelpers::GetGenerationGridSizes(InComponent ? InComponent->GetGraph() : nullptr, PCGSubsystem->GetPCGWorldActor(), GridSizes, bHasUnbounded));
		PCGSubsystem->CreatePartitionActorsWithinBounds(Bounds, GridSizes);
	}
#endif // WITH_EDITOR

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

bool UPCGActorAndComponentMapping::RegisterOrUpdateNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	// Tracking is only done in Editor for now
#if WITH_EDITOR
	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	NonPartitionedOctree.AddOrUpdateComponent(InComponent, Bounds, bComponentHasChanged, bComponentWasAdded);

	return bComponentHasChanged;
#else
	return false;
#endif // WITH_EDITOR
}

bool UPCGActorAndComponentMapping::RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent, bool bDoActorMapping)
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
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(OldComponent);

		if (PartitionActorsPtr)
		{
			TSet<TObjectPtr<APCGPartitionActor>> PartitionActorsToRemap = MoveTemp(*PartitionActorsPtr);
			ComponentToPartitionActorsMap.Remove(OldComponent);

			for (APCGPartitionActor* Actor : PartitionActorsToRemap)
			{
				Actor->RemapGraphInstance(OldComponent, NewComponent);
			}

			ComponentToPartitionActorsMap.Add(NewComponent, MoveTemp(PartitionActorsToRemap));
		}
	}

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

void UPCGActorAndComponentMapping::UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce)
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

void UPCGActorAndComponentMapping::UnregisterPartitionedPCGComponent(UPCGComponent* InComponent)
{
	if (!PartitionedOctree.RemoveComponent(InComponent))
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

void UPCGActorAndComponentMapping::UnregisterNonPartitionedPCGComponent(UPCGComponent* InComponent)
{
	NonPartitionedOctree.RemoveComponent(InComponent);
}

void UPCGActorAndComponentMapping::ForAllIntersectingComponents(const FBoxCenterAndExtent& InBounds, TFunction<void(UPCGComponent*)> InFunc) const
{
	PartitionedOctree.FindElementsWithBoundsTest(InBounds, [&InFunc](const FPCGComponentRef& ComponentRef)
	{
		InFunc(ComponentRef.Component);
	});
}

void UPCGActorAndComponentMapping::RegisterPartitionActor(APCGPartitionActor* Actor, bool bDoComponentMapping)
{
	check(Actor);

	FIntVector GridCoord = Actor->GetGridCoord();
	{
		FWriteScopeLock WriteLock(PartitionActorsMapLock);

		TMap<FIntVector, TObjectPtr<APCGPartitionActor>>& PartitionActorsMapGrid = PartitionActorsMap.FindOrAdd(Actor->GetPCGGridSize());
		if (PartitionActorsMapGrid.Contains(GridCoord))
		{
			return;
		}

		PartitionActorsMapGrid.Add(GridCoord, Actor);
	}

	// For deprecration: bUse2DGrid is now true by default. But if we already have Partition Actors that were created when the flag was false by default,
	// we keep this flag
	if (APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor())
	{
		if (WorldActor->bUse2DGrid != Actor->IsUsing2DGrid())
		{
			WorldActor->bUse2DGrid = Actor->IsUsing2DGrid();
		}
	}

	// And then register itself to all the components that intersect with it
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingComponents(FBoxCenterAndExtent(Actor->GetFixedBounds()), [this, Actor, bDoComponentMapping](UPCGComponent* Component)
		{
			// For each component, do the mapping if we ask it explicitly, or if the component is generated
			if (bDoComponentMapping || Component->bGenerated)
			{
				TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component);
				// In editor we might load/create partition actors while the component is registering. Because of that,
				// the mapping might not already exists, even if the component is marked generated.
				if (PartitionActorsPtr)
				{
					Actor->AddGraphInstance(Component);
					PartitionActorsPtr->Add(Actor);
				}
			}
		});
	}
}

void UPCGActorAndComponentMapping::UnregisterPartitionActor(APCGPartitionActor* Actor)
{
	check(Actor);

	FIntVector GridCoord = Actor->GetGridCoord();

	if (TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = PartitionActorsMap.Find(Actor->GetPCGGridSize()))
	{
		FWriteScopeLock WriteLock(PartitionActorsMapLock);
		PartitionActorsMapGrid->Remove(GridCoord);
	}

	// And then unregister itself to all the components that intersect with it
	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);
		ForAllIntersectingComponents(FBoxCenterAndExtent(Actor->GetFixedBounds()), [this, Actor](UPCGComponent* Component)
		{
			TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(Component);
		if (PartitionActorsPtr)
		{
			PartitionActorsPtr->Remove(Actor);
		}
		});
	}
}

void UPCGActorAndComponentMapping::ForAllIntersectingPartitionActors(const FBox& InBounds, TFunction<void(APCGPartitionActor*)> InFunc) const
{
	// No PCGWorldActor just early out. Same for invalid bounds.
	APCGWorldActor* PCGWorldActor = PCGSubsystem->GetPCGWorldActor();

	if (!PCGWorldActor || !InBounds.IsValid)
	{
		return;
	}

	PCGHiGenGrid::FSizeToGuidMap GridSizeToGuid;
	PCGWorldActor->GetGridGuids(GridSizeToGuid);
	for (const TPair<uint32, FGuid>& SizeAndGuid : GridSizeToGuid)
	{
		const uint32 GridSize = SizeAndGuid.Key;

		const bool bUse2DGrid = PCGWorldActor->bUse2DGrid;
		FIntVector MinCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Min, GridSize, bUse2DGrid);
		FIntVector MaxCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Max, GridSize, bUse2DGrid);

		FReadScopeLock ReadLock(PartitionActorsMapLock);

		const TMap<FIntVector, TObjectPtr<APCGPartitionActor>>* PartitionActorsMapGrid = PartitionActorsMap.Find(GridSize);
		if (!PartitionActorsMapGrid || PartitionActorsMapGrid->IsEmpty())
		{
			continue;
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
	}
}

void UPCGActorAndComponentMapping::UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
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

	TSet<TObjectPtr<APCGPartitionActor>> RemovedActors;

	{
		FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);

		TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(InComponent);

		if (!PartitionActorsPtr)
		{
			// Does not yet exists, add it
			PartitionActorsPtr = &ComponentToPartitionActorsMap.Emplace(InComponent);
			check(PartitionActorsPtr);
		}

		if (const APCGWorldActor* WorldActor = PCGSubsystem->GetPCGWorldActor())
		{
			const bool bIsHiGenEnabled = InComponent->GetGraph() && InComponent->GetGraph()->IsHierarchicalGenerationEnabled();

			TSet<TObjectPtr<APCGPartitionActor>> NewMapping;
			ForAllIntersectingPartitionActors(Bounds, [&NewMapping, InComponent, WorldActor, bIsHiGenEnabled](APCGPartitionActor* Actor)
			{
				// If this graph does not have HiGen enabled, we should only add a graph instance for
				// the partition actors whose grid size matches the WorldActor's partition grid size
				if (bIsHiGenEnabled || (Actor && Actor->GetPCGGridSize() == WorldActor->PartitionGridSize))
				{
					Actor->AddGraphInstance(InComponent);
					NewMapping.Add(Actor);
				}
			});

			// Find the ones that were removed
			RemovedActors = PartitionActorsPtr->Difference(NewMapping);

			*PartitionActorsPtr = MoveTemp(NewMapping);
		}
	}

	// No need to be locked to do this.
	for (APCGPartitionActor* RemovedActor : RemovedActors)
	{
		RemovedActor->RemoveGraphInstance(InComponent);
	}
}

void UPCGActorAndComponentMapping::DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	check(InComponent);

	if (!InComponent->IsPartitioned())
	{
		return;
	}

	FWriteScopeLock WriteLock(ComponentToPartitionActorsMapLock);

	TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(InComponent);
	if (PartitionActorsPtr)
	{
		for (APCGPartitionActor* Actor : *PartitionActorsPtr)
		{
			Actor->RemoveGraphInstance(InComponent);
		}

		PartitionActorsPtr->Empty();
	}
}

TSet<UPCGComponent*> UPCGActorAndComponentMapping::GetAllRegisteredPartitionedComponents() const
{
	return PartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> UPCGActorAndComponentMapping::GetAllRegisteredNonPartitionedComponents() const
{
	return NonPartitionedOctree.GetAllComponents();
}

TSet<UPCGComponent*> UPCGActorAndComponentMapping::GetAllRegisteredComponents() const
{
	TSet<UPCGComponent*> Res = GetAllRegisteredPartitionedComponents();
	Res.Append(GetAllRegisteredNonPartitionedComponents());
	return Res;
}

#if WITH_EDITOR
void UPCGActorAndComponentMapping::RegisterOrUpdateTracking(UPCGComponent* InComponent, bool bInShouldDirtyActors)
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
	RegisterActor(ComponentOwner);
	TSet<UPCGComponent*>& AllComponents = AlwaysTrackedActorsToComponentsMap.FindOrAdd(ComponentOwner);
	AllComponents.Add(InComponent);

	FPCGActorSelectionKey OwnerKey = FPCGActorSelectionKey(EPCGActorFilter::Self);
	KeysToComponentsMap.FindOrAdd(OwnerKey).Add(InComponent);

	const bool bDisableDelayedActorRegistering = PCGActorAndComponentMapping::CVarDisableDelayedActorRegistering.GetValueOnAnyThread();

	// And we also need to find all actors that should be tracked
	if (UPCGGraph* PCGGraph = InComponent->GetGraph())
	{
		auto FindActorsAndTrack = [this, InComponent, bInShouldDirtyActors, bDisableDelayedActorRegistering](const FPCGActorSelectionKey& InKey, const TArray<FPCGSettingsAndCulling>& InSettingsAndCulling)
		{
			// InKey provide the info for selecting a given actor.
			// We reconstruct the selector settings from this key, and we also force it to SelectMultiple, since
			// we want to gather all the actors that matches this given key.
			FPCGActorSelectorSettings SelectorSettings = FPCGActorSelectorSettings::ReconstructFromKey(InKey);
			SelectorSettings.bSelectMultiple = true;

			bool bShouldCull = true;
			for (const FPCGSettingsAndCulling& SettingsAndCulling : InSettingsAndCulling)
			{
				if (!SettingsAndCulling.Value)
				{
					bShouldCull = false;
					break;
				}
			}

			TArray<AActor*> AllActors = PCGActorSelector::FindActors(SelectorSettings, InComponent, [](const AActor*) { return true; }, [](const AActor*) { return true; });

			for (AActor* Actor : AllActors)
			{
				if (!Actor)
				{
					continue;
				}

				if (!Actor->HasActorRegisteredAllComponents() && !bDisableDelayedActorRegistering)
				{
					DelayedAddedActors.Emplace({ Actor, bInShouldDirtyActors });
					continue;
				}

				if (bShouldCull)
				{
					CulledTrackedActorsToComponentsMap.FindOrAdd(Actor).Add(InComponent);
				}
				else
				{
					AlwaysTrackedActorsToComponentsMap.FindOrAdd(Actor).Add(InComponent);
				}

				RegisterActor(Actor);

				if (bInShouldDirtyActors)
				{
					// If we need to force dirty, disregard culling (always intersect).
					InComponent->DirtyTrackedActor(Actor, /*bIntersect=*/ true, /*InRemovedTags=*/ {}, /*InOriginatingChangeObject=*/ nullptr);
				}
			}
		};

		for (TPair<FPCGActorSelectionKey, TArray<FPCGSettingsAndCulling>>& It : PCGGraph->GetTrackedActorKeysToSettings())
		{
			FindActorsAndTrack(It.Key, It.Value);
			KeysToComponentsMap.FindOrAdd(It.Key).Add(InComponent);
		}

		// Also while we support landscape pins on input node, we need to track landscape if we uses it, or the input is landscape.
		if (InComponent->ShouldTrackLandscape())
		{
			// Landscape doesn't have an associated setting and is always culled.
			FPCGActorSelectionKey LandscapeKey = FPCGActorSelectionKey(ALandscapeProxy::StaticClass());
			FindActorsAndTrack(LandscapeKey, { {nullptr, true} });
			KeysToComponentsMap.FindOrAdd(LandscapeKey).Add(InComponent);
		}
	}

	// Add tracking for when the graph was generated/cleaned, only once
	if (!InComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &UPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &UPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
	}

}

void UPCGActorAndComponentMapping::RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent)
{
	auto ReplaceInMap = [InOldComponent, InNewComponent](auto& InMap)
	{
		for (auto& It : InMap)
		{
			if (It.Value.Remove(InOldComponent) > 0)
			{
				It.Value.Add(InNewComponent);
			}
		}
	};

	ReplaceInMap(CulledTrackedActorsToComponentsMap);
	ReplaceInMap(AlwaysTrackedActorsToComponentsMap);
	ReplaceInMap(KeysToComponentsMap);

	// Old component will probably die, but we'll force removing the delegates even if it is const.
	if (UPCGComponent* MutableOldComponent = const_cast<UPCGComponent*>(InOldComponent))
	{
		MutableOldComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		MutableOldComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
	}

	// And just making sure we are not registering multiple times
	if (!InNewComponent->OnPCGGraphGeneratedDelegate.IsBoundToObject(this))
	{
		InNewComponent->OnPCGGraphGeneratedDelegate.AddRaw(this, &UPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
		InNewComponent->OnPCGGraphCleanedDelegate.AddRaw(this, &UPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned);
	}
}

void UPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	TSet<TObjectKey<AActor>> CandidatesForUntrack;
	TSet<FPCGActorSelectionKey> KeysToRemove;

	auto RemoveFromMap = [InComponent](auto& InMap, auto& InCandidateToRemove)
	{
		for (auto& It : InMap)
		{
			It.Value.Remove(InComponent);
			if (It.Value.IsEmpty())
			{
				InCandidateToRemove.Add(It.Key);
			}
		}
	};

	RemoveFromMap(CulledTrackedActorsToComponentsMap, CandidatesForUntrack);
	RemoveFromMap(AlwaysTrackedActorsToComponentsMap, CandidatesForUntrack);
	RemoveFromMap(KeysToComponentsMap, KeysToRemove);

	for (const FPCGActorSelectionKey& Key : KeysToRemove)
	{
		KeysToComponentsMap.Remove(Key);
	}

	// We also need to untrack actors that doesn't have any component that tracks them.
	auto ShouldBeRemoved = [](const TObjectKey<AActor>& InActor, TMap<TObjectKey<AActor>, TSet<UPCGComponent*>>& InMap)
	{
		TSet<UPCGComponent*>* RegisteredComponents = InMap.Find(InActor);
		return !RegisteredComponents || RegisteredComponents->IsEmpty();
	};

	for (TObjectKey<AActor> Candidate : CandidatesForUntrack)
	{
		if (ShouldBeRemoved(Candidate, CulledTrackedActorsToComponentsMap) && ShouldBeRemoved(Candidate, AlwaysTrackedActorsToComponentsMap))
		{
			UnregisterActor(Candidate.ResolveObjectPtr());
		}
	}

	InComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
	InComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
}

void UPCGActorAndComponentMapping::ResetPartitionActorsMap()
{
	PartitionActorsMapLock.WriteLock();
	PartitionActorsMap.Empty();
	PartitionActorsMapLock.WriteUnlock();
}

void UPCGActorAndComponentMapping::RegisterTrackingCallbacks()
{
	GEngine->OnActorMoved().AddRaw(this, &UPCGActorAndComponentMapping::OnActorMoved);
	GEngine->OnLevelActorAdded().AddRaw(this, &UPCGActorAndComponentMapping::OnActorAdded);
	GEngine->OnLevelActorDeleted().AddRaw(this, &UPCGActorAndComponentMapping::OnActorDeleted);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &UPCGActorAndComponentMapping::OnObjectPropertyChanged);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddRaw(this, &UPCGActorAndComponentMapping::OnPreObjectPropertyChanged);
}

void UPCGActorAndComponentMapping::TeardownTrackingCallbacks()
{
	GEngine->OnActorMoved().RemoveAll(this);
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
}

void UPCGActorAndComponentMapping::AddDelayedActors()
{
	// Safeguard, we can't add delayed actors if the subsystem is not initialized
	if (!PCGSubsystem || !PCGSubsystem->IsInitialized() || DelayedAddedActors.IsEmpty())
	{
		return;
	}

	TSet<TTuple<TObjectKey<AActor>, bool>> StillDelayedActors;
	const bool bDisableDelayedActorRegistering = PCGActorAndComponentMapping::CVarDisableDelayedActorRegistering.GetValueOnAnyThread();

	for (TTuple<TObjectKey<AActor>, bool>& ActorPtrAndShouldDirty : DelayedAddedActors)
	{
		AActor* Actor = ActorPtrAndShouldDirty.Get<0>().ResolveObjectPtr();
		if (!Actor)
		{
			continue;
		}

		if (!Actor->HasActorRegisteredAllComponents() && !bDisableDelayedActorRegistering)
		{
			StillDelayedActors.Add(ActorPtrAndShouldDirty);
		}
		else
		{
			OnActorAdded_Internal(Actor, ActorPtrAndShouldDirty.Get<1>());
		}
	}

	DelayedAddedActors = MoveTemp(StillDelayedActors);
}

void UPCGActorAndComponentMapping::OnActorAdded(AActor* InActor)
{
	OnActorAdded_Internal(InActor);
}

void UPCGActorAndComponentMapping::OnActorAdded_Internal(AActor* InActor, bool bShouldDirty)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorAdded);

	// We have to make sure to not create a infinite loop
	if (!InActor || InActor->IsA<APCGWorldActor>() || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	// If the subsystem is not initialized, wait for it to be, and store all the actors to check
	if (!PCGSubsystem->IsInitialized())
	{
		DelayedAddedActors.Emplace({ InActor, bShouldDirty });
		return;
	}

	if (AddOrUpdateTrackedActor(InActor) && bShouldDirty)
	{
		// Finally notify them all
		OnActorChanged(InActor, /*bInHasMoved=*/ false);
	}
}

bool UPCGActorAndComponentMapping::AddOrUpdateTrackedActor(AActor* InActor)
{
	// We have to make sure to not create a infinite loop
	if (!InActor || InActor->IsA<APCGWorldActor>() || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return false;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return false;
	}
#endif

	// Gather all components, and check if they want to track this one
	TSet<UPCGComponent*> AllComponents = GetAllRegisteredComponents();

	TSet<UPCGComponent*>* CulledTrackedComponents = nullptr;
	TSet<UPCGComponent*>* AlwaysTrackedComponents = nullptr;
	
	for (UPCGComponent* PCGComponent : AllComponents)
	{
		// Making sure that they are not currently waiting to die
		{
			FScopeLock Lock(&DelayedComponentToUnregisterLock);
			if (DelayedComponentToUnregister.Contains(PCGComponent))
			{
				continue;
			}
		}

		bool bTrackingIsCulled = false;
		if (PCGComponent && PCGComponent->IsActorTracked(InActor, bTrackingIsCulled))
		{
			if (bTrackingIsCulled)
			{
				if (!CulledTrackedComponents)
				{
					CulledTrackedComponents = &CulledTrackedActorsToComponentsMap.FindOrAdd(InActor);
				}

				check(CulledTrackedComponents);
				CulledTrackedComponents->Add(PCGComponent);
			}
			else
			{
				if (!AlwaysTrackedComponents)
				{
					AlwaysTrackedComponents = &AlwaysTrackedActorsToComponentsMap.FindOrAdd(InActor);
				}

				check(AlwaysTrackedComponents);
				AlwaysTrackedComponents->Add(PCGComponent);
			}
		}
	}

	if (CulledTrackedComponents || AlwaysTrackedComponents)
	{
		RegisterActor(InActor);
		return true;
	}
	else if (IsActorTracked(InActor))
	{
		// Do some cleanup if the actor was tracked. 
		// We will force the refresh here, so return false to make sure we don't refresh it twice.
		OnActorDeleted(InActor);
		return false;
	}
	else
	{
		// If it is not tracked, and should not be tracked, just do nothing.
		return false;
	}
}

void UPCGActorAndComponentMapping::RegisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
	{
		// Only add it once.
		if (!TrackedActorToPositionMap.Contains(InActor))
		{
			LandscapeProxy->OnComponentDataChanged.AddRaw(this, &UPCGActorAndComponentMapping::OnLandscapeChanged);
		}
	}

	TrackedActorToPositionMap.FindOrAdd(InActor) = PCGActorAndComponentMapping::GetActorBounds(InActor);

	// Also gather dependencies
	UpdateActorDependencies(InActor);
}

bool UPCGActorAndComponentMapping::UnregisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}

	if (IsActorTracked(InActor))
	{
		TrackedActorToPositionMap.Remove(InActor);
		CulledTrackedActorsToComponentsMap.Remove(InActor);
		AlwaysTrackedActorsToComponentsMap.Remove(InActor);
		TrackedActorsToDependenciesMap.Remove(InActor);

		if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActor))
		{
			LandscapeProxy->OnComponentDataChanged.RemoveAll(this);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void UPCGActorAndComponentMapping::OnActorDeleted(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorDeleted);

	if (!InActor || !PCGSubsystem || InActor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	if (!IsActorTracked(InActor))
	{
		return;
	}

	// Notify all components that the actor has changed (was removed), but the Refresh will only happen AFTER the actor was actually removed from the world (because of delayed refresh).
	OnActorChanged(InActor, /*bInHasMoved=*/ false);

	// And then delete everything
	UnregisterActor(InActor);
}

void UPCGActorAndComponentMapping::OnActorMoved(AActor* InActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorMoved);

	if (!InActor || (PCGSubsystem && InActor->GetWorld() != PCGSubsystem->GetWorld()) || !IsActorTracked(InActor))
	{
		return;
	}

#if WITH_EDITOR
	if (InActor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	// Notify all components
	OnActorChanged(InActor, /*bInHasMoved=*/ true);

	// Update Actor position
	if (FBox* ActorBounds = TrackedActorToPositionMap.Find(InActor))
	{
		*ActorBounds = PCGActorAndComponentMapping::GetActorBounds(InActor);
	}
}

void UPCGActorAndComponentMapping::OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& InEditPropertyChain)
{
	// We want to track tags, to see if a tag was removed
	TempTrackedActorTags.Empty();
	FProperty* MemberProperty = InEditPropertyChain.GetActiveMemberNode() ? InEditPropertyChain.GetActiveMemberNode()->GetValue() : nullptr;
	AActor* Actor = Cast<AActor>(InObject);

	if (!Actor || (PCGSubsystem && Actor->GetWorld() != PCGSubsystem->GetWorld()) || !MemberProperty || MemberProperty->GetFName() != GET_MEMBER_NAME_CHECKED(AActor, Tags))
	{
		return;
	}

#if WITH_EDITOR
	if (Actor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	TempTrackedActorTags = TSet<FName>(Actor->Tags);
}

void UPCGActorAndComponentMapping::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnObjectPropertyChanged);

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

	if ((!bValueNotInteractive && !bActorTagChange) || bIsTextureCompilationResult || bIsStaticMeshCompilationResult)
	{
		return;
	}

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

	// If we don't find any actor, try to see if it is a dependency
	if (!Actor)
	{
		for (const TPair<TObjectKey<AActor>, TSet<TObjectPtr<UObject>>>& TrackedActor : TrackedActorsToDependenciesMap)
		{
			if (TrackedActor.Value.Contains(InObject))
			{
				if (AActor* ActorToChange = TrackedActor.Key.ResolveObjectPtr())
				{
					OnActorChanged(ActorToChange, /*bInHasMoved=*/ false, /*InOriginatingChangeObject=*/ InObject);
					UpdateActorDependencies(ActorToChange);
				}
			}
		}

		return;
	}

	if (PCGSubsystem && Actor->GetWorld() != PCGSubsystem->GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	if (Actor->bIsEditorPreviewActor)
	{
		return;
	}
#endif

	// Check if we are not tracking it or is a tag change.
	bool bShouldChange = true;
	if (!IsActorTracked(Actor) || bActorTagChange)
	{
		bShouldChange = AddOrUpdateTrackedActor(Actor);
	}

	if (bShouldChange)
	{
		OnActorChanged(Actor, /*bInHasMoved=*/ false, /*InOriginatingChangeObject=*/ InObject);
	}
	else
	{
		// Otherwise we are already tracking the actor, so update its dependencies
		UpdateActorDependencies(Actor);
	}
}

void UPCGActorAndComponentMapping::OnActorChanged(AActor* InActor, bool bInHasMoved, const UObject* InOriginatingChangeObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorChanged);

	check(InActor);
	ensure(!PCGSubsystem || InActor->GetWorld() == PCGSubsystem->GetWorld());
	TSet<UPCGComponent*> DirtyComponents;

	EPCGComponentDirtyFlag DirtyFlag = EPCGComponentDirtyFlag::Actor;
	if (InActor->IsA<ALandscapeProxy>())
	{
		DirtyFlag = DirtyFlag | EPCGComponentDirtyFlag::Landscape;
	}

	// Check if we have a change of tag too
	TSet<FName> RemovedTags = TempTrackedActorTags.Difference(TSet<FName>(InActor->Tags));

	if (TSet<UPCGComponent*>* CulledTrackedComponents = CulledTrackedActorsToComponentsMap.Find(InActor))
	{
		// Not const, since it will be updated with old actor bounds
		FBox ActorBounds = PCGActorAndComponentMapping::GetActorBounds(InActor);

		// Then do an octree find to get all components that intersect with this actor.
		// If the actor has moved, we also need to find components that intersected with it before
		// We first do it for non-partitioned, then we do it for partitioned
		auto UpdateNonPartitioned = [&DirtyComponents, InActor, CulledTrackedComponents, &RemovedTags, DirtyFlag, InOriginatingChangeObject](const FPCGComponentRef& ComponentRef) -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorChanged::UpdateNonPartitioned);

			// Don't dirty if the component was already dirtied, not tracked, or the origin of the change.
			if (DirtyComponents.Contains(ComponentRef.Component) || 
				!CulledTrackedComponents->Contains(ComponentRef.Component) ||
				InOriginatingChangeObject == ComponentRef.Component)
			{
				return;
			}

			if (ComponentRef.Component->DirtyTrackedActor(InActor, /*bIntersect=*/true, RemovedTags, InOriginatingChangeObject))
			{
				ComponentRef.Component->DirtyGenerated(DirtyFlag);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		NonPartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdateNonPartitioned);

		// For partitioned, we first need to find all components that intersect with our actor and then forward the dirty call to all local components that intersect.
		auto UpdatePartitioned = [this, &DirtyComponents, InActor, CulledTrackedComponents, &ActorBounds, &RemovedTags, DirtyFlag, InOriginatingChangeObject](const FPCGComponentRef& ComponentRef)  -> void
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorChanged::UpdatePartitioned);

			// Don't dirty if the component is not tracked, or the origin of the change.
			// We can "re-dirty" it because changes can impact different local components, from the same
			// original component.
			if (!CulledTrackedComponents->Contains(ComponentRef.Component) ||
				InOriginatingChangeObject == ComponentRef.Component)
			{
				return;
			}

			const FBox Overlap = ActorBounds.Overlap(ComponentRef.Bounds.GetBox());
			bool bWasDirtied = false;

			ForAllIntersectingPartitionActors(Overlap, [InActor, Component = ComponentRef.Component, &RemovedTags, &bWasDirtied, DirtyFlag, InOriginatingChangeObject](APCGPartitionActor* InPartitionActor) -> void
			{
				if (UPCGComponent* LocalComponent = InPartitionActor->GetLocalComponent(Component))
				{
					if (LocalComponent->DirtyTrackedActor(InActor, /*bIntersect=*/true, RemovedTags, InOriginatingChangeObject))
					{
						bWasDirtied = true;
						LocalComponent->DirtyGenerated(DirtyFlag);
					}
				}
			});

			if (bWasDirtied)
			{
				// Don't dispatch
				ComponentRef.Component->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/false);
				DirtyComponents.Add(ComponentRef.Component);
			}
		};

		PartitionedOctree.FindElementsWithBoundsTest(ActorBounds, UpdatePartitioned);

		// If it has moved, redo it with the old bounds.
		if (bInHasMoved)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorChanged::SecondUpdateHasMoved);

			if (FBox* OldActorBounds = TrackedActorToPositionMap.Find(InActor))
			{
				if (!OldActorBounds->Equals(ActorBounds))
				{
					// Set the actor bounds with the old one, to have the right Overlap in the Partition case.
					ActorBounds = *OldActorBounds;
					NonPartitionedOctree.FindElementsWithBoundsTest(*OldActorBounds, UpdateNonPartitioned);
					PartitionedOctree.FindElementsWithBoundsTest(*OldActorBounds, UpdatePartitioned);
				}
			}
		}
	}

	// Finally, dirty all components that always track this actor that are not yet notified.
	if (TSet<UPCGComponent*>* AlwaysTrackedComponents = AlwaysTrackedActorsToComponentsMap.Find(InActor))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorAndComponentMapping::OnActorChanged::AlwaysTrackedUpdate);

		for (UPCGComponent* PCGComponent : *AlwaysTrackedComponents)
		{
			if (!PCGComponent || PCGComponent == InOriginatingChangeObject)
			{
				continue;
			}

			// Don't mark "Owner changed" if the change originate from a PCG Component. It will be delegated to the DirtyTrackedActor
			// It is necessary to avoid infine loops when there are multiple PCG components on one actor, and one component was generated.
			const bool bOwnerChanged = (PCGComponent->GetOwner() == InActor) && (!InOriginatingChangeObject || !InOriginatingChangeObject->IsA<UPCGComponent>());
			bool bWasDirtied = false;

			if (!DirtyComponents.Contains(PCGComponent) && !bOwnerChanged)
			{
				if (PCGComponent->IsPartitioned())
				{
					DispatchToRegisteredLocalComponents(PCGComponent, [InActor, &RemovedTags, &bWasDirtied, DirtyFlag, InOriginatingChangeObject](UPCGComponent* InLocalComponent) -> FPCGTaskId
					{
						if (InLocalComponent->DirtyTrackedActor(InActor, /*bIntersect=*/false, RemovedTags, InOriginatingChangeObject))
						{
							bWasDirtied = true;
							InLocalComponent->DirtyGenerated(DirtyFlag);
						}
						return InvalidPCGTaskId;
					});
				}
				else
				{
					bWasDirtied = PCGComponent->DirtyTrackedActor(InActor, /*bIntersect=*/false, RemovedTags, InOriginatingChangeObject);
				}
			}

			if (bWasDirtied || bOwnerChanged)
			{
				PCGComponent->DirtyGenerated(DirtyFlag, /*bDispatchToLocalComponents=*/bOwnerChanged);
				DirtyComponents.Add(PCGComponent);
			}
		}
	}

	// And refresh all dirtied components
	for (UPCGComponent* Component : DirtyComponents)
	{
		if (Component)
		{
			Component->Refresh();
		}
	}
}

void UPCGActorAndComponentMapping::OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams)
{
	if (!InLandscape)
	{
		return;
	}

	// We don't know if the landscape moved, only that it has changed. Since `bHasMoved` is doing a bit more, always assume that the landscape has moved.
	OnActorChanged(InLandscape, /*bHasMoved=*/true);

	// Also update its dependencies
	UpdateActorDependencies(InLandscape);
}

void UPCGActorAndComponentMapping::UpdateActorDependencies(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	// Don't track what the PCG Component is already tracking (itself and graphs)
	static const TArray<UClass*> ExcludedClasses =
	{
		UPCGComponent::StaticClass(),
		UPCGGraphInterface::StaticClass()
	};

	TSet<TObjectPtr<UObject>>& DependenciesSet = TrackedActorsToDependenciesMap.FindOrAdd(InActor);
	DependenciesSet.Empty();

	if (!PCGActorAndComponentMapping::CVarDisableObjectDependenciesTracking.GetValueOnAnyThread())
	{
		PCGHelpers::GatherDependencies(InActor, DependenciesSet, 1, ExcludedClasses);
	}
}

void UPCGActorAndComponentMapping::OnPCGGraphGeneratedOrCleaned(UPCGComponent* InComponent)
{
	if (!InComponent || !InComponent->GetOwner())
	{
		return;
	}

	OnActorChanged(InComponent->GetOwner(), /*bInHasMoved=*/false, InComponent);
}

bool UPCGActorAndComponentMapping::IsActorTracked(const AActor* InActor) const
{
	return InActor && (CulledTrackedActorsToComponentsMap.Contains(InActor) || AlwaysTrackedActorsToComponentsMap.Contains(InActor));
}

#endif // WITH_EDITOR