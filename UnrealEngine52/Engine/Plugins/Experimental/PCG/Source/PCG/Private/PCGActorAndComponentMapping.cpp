// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGActorAndComponentMapping.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Engine/Engine.h"

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
	TSet<TObjectPtr<UPCGComponent>> ComponentToUnregister;
	{
		FScopeLock Lock(&DelayedComponentToUnregisterLock);
		ComponentToUnregister = MoveTemp(DelayedComponentToUnregister);
	}

	for (UPCGComponent* Component : ComponentToUnregister)
	{
		UnregisterPCGComponent(Component, /*bForce=*/true);
	}
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
	if (!InComponent->GetOwner() || InComponent->GetOwner()->IsA<APCGPartitionActor>() || !IsValid(InComponent))
	{
		return false;
	}

	// Check also that the bounds are valid. If not early out.
	if (!InComponent->GetGridBounds().IsValid)
	{
		UE_LOG(LogPCG, Error, TEXT("[RegisterOrUpdatePCGComponent] Component has invalid bounds, not registered nor updated."));
		return false;
	}

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

	// And finally handle the tracking
	RegisterOrUpdateTracking(InComponent);

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
		PCGSubsystem->CreatePartitionActorsWithinBounds(Bounds);
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
	const bool bIsTracking = InComponent->ShouldTrackActors();

	if (!bIsTracking && NonPartitionedOctree.Contains(InComponent))
	{
		// Was tracking, but no more, remove it
		UnregisterNonPartitionedPCGComponent(InComponent);
		return true;
	}

	if (!bIsTracking)
	{
		// Nothing to do
		return false;
	}

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

	RemapTracking(OldComponent, NewComponent);

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
		// We also need to check that our current PCG Component is not deleted while being reconstructed by a construction script.
		// If so, it will be "re-created" at some point with the same properties.
		// In this particular case, we don't remove the PCG component from the octree and we won't delete the mapping, but mark it to be removed
		// at next Subsystem tick. If we call "RemapPCGComponent" before, we will re-connect everything correctly.
		// Ignore this if we force (aka when we actually unregister the delayed one)
		if (InComponent->IsCreatedByConstructionScript() && !bForce)
		{
			FScopeLock Lock(&DelayedComponentToUnregisterLock);
			DelayedComponentToUnregister.Add(InComponent);
			return;
		}
	}

	UnregisterPartitionedPCGComponent(InComponent);
	UnregisterNonPartitionedPCGComponent(InComponent);

	UnregisterTracking(InComponent);
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

		if (PartitionActorsMap.Contains(GridCoord))
		{
			return;
		}

		PartitionActorsMap.Add(GridCoord, Actor);
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

	{
		FWriteScopeLock WriteLock(PartitionActorsMapLock);
		PartitionActorsMap.Remove(GridCoord);
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

	const uint32 GridSize = PCGWorldActor->PartitionGridSize;
	const bool bUse2DGrid = PCGWorldActor->bUse2DGrid;
	FIntVector MinCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Min, GridSize, bUse2DGrid);
	FIntVector MaxCellCoords = UPCGActorHelpers::GetCellCoord(InBounds.Max, GridSize, bUse2DGrid);

	{
		FReadScopeLock ReadLock(PartitionActorsMapLock);

		if (PartitionActorsMap.IsEmpty())
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
					if (const TObjectPtr<APCGPartitionActor>* ActorPtr = PartitionActorsMap.Find(CellCoords))
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

		TSet<TObjectPtr<APCGPartitionActor>> NewMapping;
		ForAllIntersectingPartitionActors(Bounds, [&NewMapping, InComponent](APCGPartitionActor* Actor)
		{
			Actor->AddGraphInstance(InComponent);
			NewMapping.Add(Actor);
		});

		// Find the ones that were removed
		RemovedActors = PartitionActorsPtr->Difference(NewMapping);

		*PartitionActorsPtr = MoveTemp(NewMapping);
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

TSet<TObjectPtr<UPCGComponent>> UPCGActorAndComponentMapping::GetAllRegisteredPartitionedComponents() const
{
	return PartitionedOctree.GetAllComponents();
}

bool UPCGActorAndComponentMapping::IsComponentTracked(const UPCGComponent* InComponent) const
{
	FReadScopeLock ReadLock(TrackedComponentsLock);
	return TrackedComponentsToActorsMap.Contains(InComponent);
}

// TODO: In a following pass
void UPCGActorAndComponentMapping::RegisterOrUpdateTracking(UPCGComponent* InComponent)
{

}

void UPCGActorAndComponentMapping::RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent)
{

}

void UPCGActorAndComponentMapping::UnregisterTracking(UPCGComponent* InComponent)
{

}

#if WITH_EDITOR
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
}

void UPCGActorAndComponentMapping::TeardownTrackingCallbacks()
{
	GEngine->OnActorMoved().RemoveAll(this);
	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

// TODO: In a following pass
void UPCGActorAndComponentMapping::OnActorAdded(AActor* InActor)
{

}

void UPCGActorAndComponentMapping::OnActorDeleted(AActor* InActor)
{

}

void UPCGActorAndComponentMapping::OnActorMoved(AActor* InActor)
{

}

void UPCGActorAndComponentMapping::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{

}

#endif // WITH_EDITOR