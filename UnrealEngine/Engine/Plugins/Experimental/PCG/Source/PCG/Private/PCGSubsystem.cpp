// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubsystem.h"
#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "PCGWorldActor.h"
#include "Graph/PCGGraphExecutor.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Math/GenericOctree.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "ObjectTools.h"
#endif

namespace PCGSubsystemConsole
{
	static FAutoConsoleCommand CommandFlushCache(
		TEXT("pcg.FlushCache"),
		TEXT("Clears the PCG results cache."),
		FConsoleCommandDelegate::CreateLambda([]()
			{
				UWorld* World = nullptr;

#if WITH_EDITOR
				if (GEditor)
				{
					if (GEditor->PlayWorld)
					{
						World = GEditor->PlayWorld;
					}
					else
					{
						World = GEditor->GetEditorWorldContext().World();
					}
				}
				else
#endif
				if (GEngine)
				{
					World = GEngine->GetCurrentPlayWorld();
				}

				if (World && World->GetSubsystem<UPCGSubsystem>())
				{
					World->GetSubsystem<UPCGSubsystem>()->FlushCache();
				}
			}));
}

#if WITH_EDITOR
namespace PCGSubsystem
{
	FPCGTaskId ForEachIntersectingCell(FPCGGraphExecutor* GraphExecutor, UWorld* World, const FBox& InBounds, bool bCreateActor, bool bLoadCell, bool bSaveActors, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&, const TArray<FPCGTaskId>&)> InOperation);
}
#endif

UPCGSubsystem::UPCGSubsystem()
	: Super()
{
	// TODO: For now we set our octree to be 2km wide, but it would be perhaps better to
	// scale it to the size of our world.
	constexpr FVector::FReal OctreeExtent = 200000; // 2km
	PCGComponentOctree = FPCGComponentOctree(FVector::ZeroVector, OctreeExtent);
}

void UPCGSubsystem::Deinitialize()
{
	// Cancel all tasks
	// TODO
	delete GraphExecutor;
	GraphExecutor = nullptr;

	Super::Deinitialize();
}

void UPCGSubsystem::PostInitialize()
{
	Super::PostInitialize();

	// Initialize graph executor
	check(!GraphExecutor);
	GraphExecutor = new FPCGGraphExecutor(this);
	
	// Gather world pcg actor if it exists
	if (!PCGWorldActor)
	{
		if (UWorld* World = GetWorld())
		{
			UPCGActorHelpers::ForEachActorInWorld<APCGWorldActor>(World, [this](AActor* InActor)
			{
				PCGWorldActor = Cast<APCGWorldActor>(InActor);
				return PCGWorldActor == nullptr;
			});
		}
	}
}

void UPCGSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// If we have any tasks to execute, schedule some
	GraphExecutor->Execute();

	// Lose references to landscape cache as needed
	if(PCGWorldActor && GetLandscapeCache())
	{
		GetLandscapeCache()->Tick(DeltaSeconds);
	}

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

APCGWorldActor* UPCGSubsystem::GetPCGWorldActor()
{
#if WITH_EDITOR
	if (!PCGWorldActor && !PCGHelpers::IsRuntimeOrPIE())
	{
		PCGWorldActorLock.Lock();
		if (!PCGWorldActor)
		{
			PCGWorldActor = APCGWorldActor::CreatePCGWorldActor(GetWorld());
		}
		PCGWorldActorLock.Unlock();
	}
#endif

	return PCGWorldActor;
}

#if WITH_EDITOR
void UPCGSubsystem::DestroyPCGWorldActor()
{
	if (PCGWorldActor)
	{
		PCGWorldActorLock.Lock();
		PCGWorldActor->Destroy();
		PCGWorldActor = nullptr;
		PCGWorldActorLock.Unlock();
	}
}
#endif

void UPCGSubsystem::RegisterPCGWorldActor(APCGWorldActor* InActor)
{
	// TODO: we should support merging or multi world actor support when relevant
	if (!PCGWorldActor)
	{
		PCGWorldActor = InActor;
	}
}

void UPCGSubsystem::UnregisterPCGWorldActor(APCGWorldActor* InActor)
{
	if (PCGWorldActor == InActor)
	{
		PCGWorldActor = nullptr;
	}
}

UPCGLandscapeCache* UPCGSubsystem::GetLandscapeCache()
{
	APCGWorldActor* LandscapeCacheOwner = GetPCGWorldActor();
	return LandscapeCacheOwner ? LandscapeCacheOwner->LandscapeCacheObject.Get() : nullptr;
}

FPCGTaskId UPCGSubsystem::ScheduleComponent(UPCGComponent* PCGComponent, bool bSave, const TArray<FPCGTaskId>& Dependencies)
{
	check(GraphExecutor);

	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

#if WITH_EDITOR
	if (PCGComponent->IsPartitioned() && !PCGHelpers::IsRuntimeOrPIE())
	{
		// In this case create the PA and update the mapping
		// Note: This is an immediate operation, as we need the PA for the generation.
		auto ScheduleTask = [](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) { return InvalidPCGTaskId; };

		PCGSubsystem::ForEachIntersectingCell(GraphExecutor, PCGComponent->GetWorld(), PCGComponent->GetGridBounds(), /*bCreateActor=*/true, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);

		UpdateMappingPCGComponentPartitionActor(PCGComponent);
	}
#endif // WITH_EDITOR

	TArray<FPCGTaskId> AllTasks;

	// If the component is partitioned, we will forward the calls to its registered PCG Partition actors
	if (PCGComponent->IsPartitioned())
	{
		auto LocalGenerateTask = [OriginalComponent = PCGComponent, &Dependencies](UPCGComponent* LocalComponent)
		{
			// If the local component is currently generating, it's probably because it was requested by a refresh.
			// Wait after this one instead
			if (LocalComponent->IsGenerating())
			{
				return LocalComponent->CurrentGenerationTask;
			}

			// Ensure that the PCG actor match our original
			LocalComponent->SetPropertiesFromOriginal(OriginalComponent);

			if (LocalComponent->bGenerated && !OriginalComponent->bGenerated)
			{
				// Detected a mismatch between the original component and the local component.
				// Request a cleanup first
				LocalComponent->CleanupLocalImmediate(true);
			}

			return LocalComponent->GenerateInternal(/*bForce=*/ false, EPCGComponentGenerationTrigger::GenerateOnDemand, Dependencies);
		};

		AllTasks = DispatchToRegisteredLocalComponents(PCGComponent, LocalGenerateTask);
	}
	else
	{
		FPCGTaskId TaskId = PCGComponent->CreateGenerateTask(/*bForce=*/bSave, Dependencies);
		if (TaskId != InvalidPCGTaskId)
		{
			AllTasks.Add(TaskId);
		}
	}

	if (!AllTasks.IsEmpty())
	{
		TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);

		return GraphExecutor->ScheduleGenericWithContext([ComponentPtr](FPCGContext* Context) {
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				// If the component is not valid anymore, just early out.
				if (!IsValid(Component))
				{
					return true;
				}

				const FBox NewBounds = Component->GetGridBounds();
				Component->PostProcessGraph(NewBounds, /*bGenerate=*/true, Context);
			}

			return true;
			}, PCGComponent, AllTasks);
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("[ScheduleComponent] Didn't schedule any task."));
		if (PCGComponent)
		{
			PCGComponent->OnProcessGraphAborted();
		}
		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::ScheduleCleanup(UPCGComponent* PCGComponent, bool bRemoveComponents, bool bSave, const TArray<FPCGTaskId>& Dependencies)
{
	if (!PCGComponent)
	{
		return InvalidPCGTaskId;
	}

	TArray<FPCGTaskId> AllTasks;

	// If the component is partitioned, we will forward the calls to its registered PCG Partition actors
	if (PCGComponent->IsPartitioned())
	{
		auto LocalCleanupTask = [bRemoveComponents, &Dependencies](UPCGComponent* LocalComponent)
		{
			// If the local component is currently cleaning up, it's probably because it was requested by a refresh.
			// Wait after this one instead
			if (LocalComponent->IsCleaningUp())
			{
				return LocalComponent->CurrentCleanupTask;
			}

			return LocalComponent->CleanupInternal(bRemoveComponents, /*bSave=*/ false, Dependencies);
		};

		AllTasks = DispatchToRegisteredLocalComponents(PCGComponent, LocalCleanupTask);
	}
	else
	{
		FPCGTaskId TaskId = PCGComponent->CreateCleanupTask(bRemoveComponents, Dependencies);
		if (TaskId != InvalidPCGTaskId)
		{
			AllTasks.Add(TaskId);
		}
	}

	TWeakObjectPtr<UPCGComponent> ComponentPtr(PCGComponent);
	auto PostCleanupTask = [this, ComponentPtr]()
	{
		if (UPCGComponent* Component = ComponentPtr.Get())
		{
			// If the component is not valid anymore, just early out
			if (!IsValid(Component))
			{
				return true;
			}

			Component->PostCleanupGraph();

#if WITH_EDITOR
			// If we are in Editor and partitioned, delete the mapping.
			if (Component->IsPartitioned() && !PCGHelpers::IsRuntimeOrPIE())
			{
				DeleteMappingPCGComponentPartitionActor(Component);
			}
#endif // WITH_EDITOR
		}

		return true;
	};

	FPCGTaskId PostCleanupTaskId = InvalidPCGTaskId;

	// If we have no tasks to do, just call PostCleanup immediatly
	// otherwise wait for all the tasks to be done to call PostCleanup.
	if (AllTasks.IsEmpty())
	{
		PostCleanupTask();
	}
	else
	{
		PostCleanupTaskId = GraphExecutor->ScheduleGeneric(PostCleanupTask, PCGComponent, AllTasks);
	}

	return PostCleanupTaskId;
}

FPCGTaskId UPCGSubsystem::ScheduleGraph(UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& Dependencies)
{
	if (SourceComponent)
	{
		return GraphExecutor->Schedule(SourceComponent, Dependencies);
	}
	else
	{
		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::ScheduleGraph(UPCGGraph* Graph, UPCGComponent* SourceComponent, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& Dependencies)
{
	if (SourceComponent)
	{
		return GraphExecutor->Schedule(Graph, SourceComponent, InputElement, Dependencies);
	}
	else
	{
		return InvalidPCGTaskId;
	}
}

ETickableTickType UPCGSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always;
}

TStatId UPCGSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPCGSubsystem, STATGROUP_Tickables);
}

FPCGTaskId UPCGSubsystem::ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* Component, const TArray<FPCGTaskId>& TaskDependencies)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGeneric(InOperation, Component, TaskDependencies);
}

FPCGTaskId UPCGSubsystem::ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* Component, const TArray<FPCGTaskId>& TaskDependencies, bool bConsumeInputData)
{
	check(GraphExecutor);
	return GraphExecutor->ScheduleGenericWithContext(InOperation, Component, TaskDependencies, bConsumeInputData);
}

TArray<FPCGTaskId> UPCGSubsystem::DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DispatchToRegisteredLocalComponents);

	// TODO: Might be more interesting to copy the set and release the lock.
	FReadScopeLock ReadLock(ComponentToPartitionActorsMapLock);
	const TSet<TObjectPtr<APCGPartitionActor>>* PartitionActorsPtr = ComponentToPartitionActorsMap.Find(OriginalComponent);

	if (!PartitionActorsPtr)
	{
		return TArray<FPCGTaskId>();
	}

	return DispatchToLocalComponents(OriginalComponent, *PartitionActorsPtr, InFunc);
}

TArray<FPCGTaskId> UPCGSubsystem::DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const
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

bool UPCGSubsystem::GetOutputData(FPCGTaskId TaskId, FPCGDataCollection& OutData)
{
	check(GraphExecutor);
	return GraphExecutor->GetOutputData(TaskId, OutData);
}

bool UPCGSubsystem::RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping)
{
	// Just make sure that we don't register components that are from the PCG Partition Actor
	check(InComponent && InComponent->GetOwner() && !InComponent->GetOwner()->IsA<APCGPartitionActor>());

	// If the component is not partitioned nor valid, don't register/update it.
	if (!InComponent->IsPartitioned() || !IsValid(InComponent))
	{
		return false;
	}

	// Check also that the bounds are valid. If not early out.
	if (!InComponent->GetGridBounds().IsValid)
	{
		UE_LOG(LogPCG, Error, TEXT("[RegisterOrUpdatePCGComponent] Component has invalid bounds, not registered nor updated."));
		return false;
	}

	FBox Bounds(EForceInit::ForceInit);
	bool bComponentHasChanged = false;
	bool bComponentWasAdded = false;

	{
		FWriteScopeLock WriteLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

		if (!ElementIdPtr)
		{
			// Does not exist yet, add it.
			FPCGComponentOctreeIDSharedRef IdShared = MakeShared<FPCGComponentOctreeID>();
			FPCGComponentRef ComponentRef(InComponent, IdShared);
			Bounds = ComponentRef.Bounds.GetBox();
			check(Bounds.IsValid);

			// If the Component is already generated, it probably mean we are in loading. The component bounds and last
			// generated bounds should be the same.
			// If the bounds depends on other components on the owner however, it might not be the same, because of the registration order.
			// In this case, override the bounds by the last generated ones.
			if (InComponent->bGenerated && !InComponent->LastGeneratedBounds.Equals(Bounds))
			{
				Bounds = InComponent->LastGeneratedBounds;
				ComponentRef.Bounds = Bounds;
			}

			PCGComponentOctree.AddElement(ComponentRef);

			bComponentHasChanged = true;
			bComponentWasAdded = true;

			// Store the shared ptr, because if we add/remove components in the octree, the id might change.
			// We need to make sure that we always have the latest id for the given component.
			ComponentToIdMap.Add(InComponent, MoveTemp(IdShared));
		}
		else
		{
			// It already exists, update it if the bounds changed.

			// Do a copy here.
			FPCGComponentRef ComponentRef = PCGComponentOctree.GetElementById((*ElementIdPtr)->Id);
			FBox PreviousBounds = ComponentRef.Bounds.GetBox();
			ComponentRef.UpdateBounds();
			Bounds = ComponentRef.Bounds.GetBox();
			check(Bounds.IsValid);

			// If bounds changed, remove and re-add to the octree
			if (!PreviousBounds.Equals(Bounds))
			{
				PCGComponentOctree.RemoveElement((*ElementIdPtr)->Id);
				PCGComponentOctree.AddElement(ComponentRef);
				bComponentHasChanged = true;
			}
		}
	}

#if WITH_EDITOR
	// In Editor only, we will create new partition actors depending on the new bounds
	// TODO: For now it will always create the PA. But if we want to create them only when we generate, we need to make
	// sure to update the runtime flow, for them to also create PA if they need to.
	if (!PCGHelpers::IsRuntimeOrPIE())
	{
		auto ScheduleTask = [](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) { return InvalidPCGTaskId; };

		// We can't spawn actors if we are running constructions scripts, asserting when we try to get the actor with the WP API.
		// We should never enter this if we are in a construction script. If the ensure is hit, we need to fix it.
		UWorld* World = InComponent->GetWorld();
		if (ensure(World && !World->bIsRunningConstructionScript))
		{
			PCGSubsystem::ForEachIntersectingCell(GraphExecutor, World, Bounds, /*bCreateActor=*/true, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);
		}
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

bool UPCGSubsystem::RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent)
{
	check(OldComponent && NewComponent);

	// First verification we have the old component registered
	{
		FReadScopeLock ReadLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(OldComponent);

		if (!ElementIdPtr)
		{
			return false;
		}
	}

	// If so, lock again in write and recheck if it has not been remapped already.
	bool bBoundsChanged = false;
	{
		FWriteScopeLock WriteLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(OldComponent);

		if (!ElementIdPtr)
		{
			// It was remapped, so return true there.
			return true;
		}

		FPCGComponentOctreeIDSharedRef ElementId = *ElementIdPtr;

		FPCGComponentRef ComponentRef = PCGComponentOctree.GetElementById(ElementId->Id);
		FBox PreviousBounds = ComponentRef.Bounds.GetBox();
		ComponentRef.Component = NewComponent;
		ComponentRef.UpdateBounds();
		FBox Bounds = ComponentRef.Bounds.GetBox();
		check(Bounds.IsValid);

		// If bounds changed, we need to update the mapping
		bBoundsChanged = !Bounds.Equals(PreviousBounds);

		PCGComponentOctree.RemoveElement((*ElementIdPtr)->Id);
		PCGComponentOctree.AddElement(ComponentRef);
		ComponentToIdMap.Remove(OldComponent);
		ComponentToIdMap.Add(NewComponent, MoveTemp(ElementId));
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

	// And update the mapping if bounds changed
	if (bBoundsChanged)
	{
		UpdateMappingPCGComponentPartitionActor(NewComponent);
	}

	return true;
}

void UPCGSubsystem::UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce)
{
	check(InComponent);

	// First verification we have this component registered
	{
		FReadScopeLock ReadLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

		if (!ElementIdPtr)
		{
			return;
		}
	}

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

	// If so, lock in write and retry to find it (to avoid removing it twice)
	{
		FWriteScopeLock WriteLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

		if (!ElementIdPtr)
		{
			return;
		}

		PCGComponentOctree.RemoveElement((*ElementIdPtr)->Id);
		ComponentToIdMap.Remove(InComponent);
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

void UPCGSubsystem::ForAllIntersectingComponents(const FBoxCenterAndExtent& InBounds, TFunction<void(UPCGComponent*)> InFunc) const
{
	FReadScopeLock ReadLock(PCGVolumeOctreeLock);

	PCGComponentOctree.FindElementsWithBoundsTest(InBounds, [&InFunc](const FPCGComponentRef& ComponentRef)
		{
			InFunc(ComponentRef.Component);
		});
}

void UPCGSubsystem::RegisterPartitionActor(APCGPartitionActor* Actor, bool bDoComponentMapping)
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

void UPCGSubsystem::UnregisterPartitionActor(APCGPartitionActor* Actor)
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
					Actor->RemoveGraphInstance(Component);
					PartitionActorsPtr->Remove(Actor);
				}
			});
	}
}

void UPCGSubsystem::ForAllIntersectingPartitionActors(const FBox& InBounds, TFunction<void(APCGPartitionActor*)> InFunc) const
{
	// No PCGWorldActor just early out. Same for invalid bounds.
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

void UPCGSubsystem::UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
{
	check(InComponent);

	// Get the bounds
	FBox Bounds(EForceInit::ForceInit);
	{
		FReadScopeLock OctreeReadLock(PCGVolumeOctreeLock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

		if (ensure(ElementIdPtr))
		{
			const FPCGComponentRef& ComponentRef = PCGComponentOctree.GetElementById((*ElementIdPtr)->Id);
			Bounds = ComponentRef.Bounds.GetBox();
		}
		else
		{
			return;
		}
	}

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

void UPCGSubsystem::DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent)
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

TSet<TObjectPtr<UPCGComponent>> UPCGSubsystem::GetAllRegisteredComponents() const
{
	TSet<TObjectPtr<UPCGComponent>> Components;
	ComponentToIdMap.GetKeys(Components);

	return Components;
}

#if WITH_EDITOR

namespace PCGSubsystem
{
	FPCGTaskId ForEachIntersectingCell(FPCGGraphExecutor* GraphExecutor, UWorld* World, const FBox& InBounds, bool bCreateActor, bool bLoadCell, bool bSaveActors, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&, const TArray<FPCGTaskId>&)> InOperation)
	{
		if (!GraphExecutor || !World)
		{
			UE_LOG(LogPCG, Error, TEXT("[ForEachIntersectingCell] GraphExecutor or World is null"));
			return InvalidPCGTaskId;
		}

		APCGWorldActor* PCGWorldActor = PCGHelpers::GetPCGWorldActor(World);

		check(PCGWorldActor);

		// In case of 2D grid, we are clamping our bounds in Z to be within 0 and PartitionGridSize
		// By doing so, WP will tie all the partition actors to [0, PartitionGridSize[ interval, generating a "2D grid" instead of 3D.
		FBox ModifiedInBounds = InBounds;
		if (PCGWorldActor->bUse2DGrid)
		{
			FVector MinBounds = InBounds.Min;
			FVector MaxBounds = InBounds.Max;

			MinBounds.Z = 0;
			MaxBounds.Z = FMath::Max<FVector::FReal>(0.0, PCGWorldActor->PartitionGridSize - 1.0); // -1 to be just below the PartitionGridSize
			ModifiedInBounds = FBox(MinBounds, MaxBounds);
		}

		TArray<FPCGTaskId> CellTasks;

		auto CellLambda = [&CellTasks, GraphExecutor, World, bCreateActor, bLoadCell, bSaveActors, ModifiedInBounds, &InOperation](const UActorPartitionSubsystem::FCellCoord& CellCoord, const FBox& CellBounds)
		{
			UActorPartitionSubsystem* PartitionSubsystem = UWorld::GetSubsystem<UActorPartitionSubsystem>(World);
			FBox IntersectedBounds = ModifiedInBounds.Overlap(CellBounds);

			if (IntersectedBounds.IsValid)
			{
				TSharedPtr<TSet<FWorldPartitionReference>> ActorReferences = MakeShared<TSet<FWorldPartitionReference>>();

				auto PostCreation = [](APartitionActor* Actor) { CastChecked<APCGPartitionActor>(Actor)->PostCreation(); };

				const bool bInBoundsSearch = true;
				const FGuid DefaultGridGuid;
				const uint32 DefaultGridSize = 0;

				APCGPartitionActor* PCGActor = Cast<APCGPartitionActor>(PartitionSubsystem->GetActor(APCGPartitionActor::StaticClass(), CellCoord, bCreateActor, DefaultGridGuid, DefaultGridSize, bInBoundsSearch, PostCreation));

				// At this point, if bCreateActor was true, then it exists, but it is not currently loaded; make sure it is loaded
				// Otherwise, we still need to load it if it exists
				// TODO: Revisit after API review on the WP side, we shouldn't have to load here or get the actor desc directly
				if (!PCGActor && bSaveActors)
				{
					const FWorldPartitionActorDesc* PCGActorDesc = nullptr;
					auto FindFirst = [&CellCoord, &PCGActorDesc](const FWorldPartitionActorDesc* ActorDesc) {
						FPartitionActorDesc* PartitionActorDesc = (FPartitionActorDesc*)ActorDesc;

						if (PartitionActorDesc &&
							PartitionActorDesc->GridIndexX == CellCoord.X &&
							PartitionActorDesc->GridIndexY == CellCoord.Y &&
							PartitionActorDesc->GridIndexZ == CellCoord.Z)
						{
							PCGActorDesc = ActorDesc;
							return false;
						}
						else
						{
							return true;
						}
					};

					FWorldPartitionHelpers::ForEachIntersectingActorDesc<APCGPartitionActor>(World->GetWorldPartition(), CellBounds, FindFirst);

					check(!bCreateActor || PCGActorDesc);
					if (PCGActorDesc)
					{
						ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), PCGActorDesc->GetGuid()));
						PCGActor = Cast<APCGPartitionActor>(PCGActorDesc->GetActor());
					}
				}
				else if(PCGActor)
				{
					// We still need to keep a reference on the PCG actor - note that newly created PCG actors will not have a reference here, but won't be unloaded
					ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), PCGActor->GetActorGuid()));
				}

				if (!PCGActor)
				{
					return true;
				}

				TArray<FPCGTaskId> PreviousTasks;

				auto SetPreviousTaskIfValid = [&PreviousTasks](FPCGTaskId TaskId){
					if (TaskId != InvalidPCGTaskId)
					{
						PreviousTasks.Reset();
						PreviousTasks.Add(TaskId);
					}
				};

				// We'll need to make sure actors in the bounds are loaded only if we need them.
				if (bLoadCell)
				{
					auto WorldPartitionLoadActorsInBounds = [World, ActorReferences](const FWorldPartitionActorDesc* ActorDesc) {
						check(ActorDesc);
						ActorReferences->Add(FWorldPartitionReference(World->GetWorldPartition(), ActorDesc->GetGuid()));
						// Load actor if not already loaded
						ActorDesc->GetActor();
						return true;
					};

					auto LoadActorsTask = [World, IntersectedBounds, WorldPartitionLoadActorsInBounds]() {
						FWorldPartitionHelpers::ForEachIntersectingActorDesc(World->GetWorldPartition(), IntersectedBounds, WorldPartitionLoadActorsInBounds);
						return true;
					};

					FPCGTaskId LoadTaskId = GraphExecutor->ScheduleGeneric(LoadActorsTask, nullptr, {});
					SetPreviousTaskIfValid(LoadTaskId);
				}

				// Execute
				FPCGTaskId ExecuteTaskId = InOperation(PCGActor, IntersectedBounds, PreviousTasks);
				SetPreviousTaskIfValid(ExecuteTaskId);

				// Save changes; note that there's no need to save if the operation was cancelled
				if (bSaveActors && ExecuteTaskId != InvalidPCGTaskId)
				{
					auto SaveActorTask = [PCGActor, GraphExecutor]() {
						GraphExecutor->AddToDirtyActors(PCGActor);
						return true;
					};

					FPCGTaskId SaveTaskId = GraphExecutor->ScheduleGeneric(SaveActorTask, nullptr, PreviousTasks);
					SetPreviousTaskIfValid(SaveTaskId);
				}

				// Unload actors from cell (or the pcg actor refered here)
				auto UnloadActorsTask = [GraphExecutor, ActorReferences]() {
					GraphExecutor->AddToUnusedActors(*ActorReferences);
					return true;
				};

				// Schedule after the save (if valid), then the execute so we can queue this after the load.
				FPCGTaskId UnloadTaskId = GraphExecutor->ScheduleGeneric(UnloadActorsTask, nullptr, PreviousTasks);
				SetPreviousTaskIfValid(UnloadTaskId);

				// Finally, mark "last" valid task in the cell tasks.
				CellTasks.Append(PreviousTasks);
			}

			return true;
		};

		// TODO: accumulate last phases of every cell + run a GC at the end?
		//  or add some mechanism on the graph executor side to run GC every now and then
		FActorPartitionGridHelper::ForEachIntersectingCell(APCGPartitionActor::StaticClass(), ModifiedInBounds, World->PersistentLevel, CellLambda);

		// Finally, create a dummy generic task to wait on all cells
		if (!CellTasks.IsEmpty())
		{
			return GraphExecutor->ScheduleGeneric([]() { return true; }, nullptr, CellTasks);
		}
		else
		{
			return InvalidPCGTaskId;
		}
	}

} // end namepsace PCGSubsystem

FPCGTaskId UPCGSubsystem::ProcessGraph(UPCGComponent* Component, const FBox& InPreviousBounds, const FBox& InNewBounds, EOperation InOperation, bool bSave)
{
	check(Component);
	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	// TODO: optimal implementation would find the difference between the previous bounds and the new bounds
	// and process these only. This is esp. important because of the CreateActor parameter.
	auto ScheduleTask = [this, ComponentPtr, InOperation, bSave](APCGPartitionActor* PCGActor, const FBox& InBounds, const TArray<FPCGTaskId>& TaskDependencies){
		TWeakObjectPtr<APCGPartitionActor> PCGActorPtr(PCGActor);

		auto UnpartitionTask = [ComponentPtr, PCGActorPtr]() {
			// TODO: PCG actors that become empty could be deleted, but we also need to keep track
			// of packages that would need to be deleted from SCC.
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					PCGActor->RemoveGraphInstance(Component);
				}
				else
				{
#if WITH_EDITOR
					PCGActor->CleanupDeadGraphInstances();
#endif
				}
			}
			return true;
		};

		auto PartitionTask = [ComponentPtr, PCGActorPtr]() {
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					PCGActor->AddGraphInstance(Component);
				}
			}

			return true;
		};

		auto ScheduleGraph = [this, PCGActorPtr, ComponentPtr, bSave, TaskDependencies]() {
			if (APCGPartitionActor* PCGActor = PCGActorPtr.Get())
			{
				if (UPCGComponent* Component = ComponentPtr.Get())
				{
					// Ensure that the PCG actor has a matching local component.
					// This is done immediately, but technically we could add it as a task
					PCGActor->AddGraphInstance(Component);

					UPCGComponent* LocalComponent = PCGActor->GetLocalComponent(Component);

					if (!LocalComponent)
					{
						return InvalidPCGTaskId;
					}

					return LocalComponent->GenerateInternal(bSave, EPCGComponentGenerationTrigger::GenerateOnDemand, TaskDependencies);
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("[ProcessGraph] PCG Component on PCG Actor is null"));
					return InvalidPCGTaskId;
				}
			}
			
			return InvalidPCGTaskId;
		};

		switch (InOperation)
		{
		case EOperation::Unpartition:
			return GraphExecutor->ScheduleGeneric(UnpartitionTask, ComponentPtr.Get(), TaskDependencies);
		case EOperation::Partition:
			return GraphExecutor->ScheduleGeneric(PartitionTask, ComponentPtr.Get(), TaskDependencies);
		case EOperation::Generate:
		default:
			return ScheduleGraph();
		}
	};

	FBox UnionBounds = InPreviousBounds + InNewBounds;
	const bool bGenerate = InOperation == EOperation::Generate;
	const bool bCreateActors = InOperation != EOperation::Unpartition;
	const bool bLoadCell = (bGenerate && bSave); // TODO: review this
	const bool bSaveActors = (bSave);

	FPCGTaskId ProcessAllCellsTaskId = InvalidPCGTaskId;

	if (UnionBounds.IsValid)
	{
		ProcessAllCellsTaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, Component->GetWorld(), UnionBounds, bCreateActors, bLoadCell, bSaveActors, ScheduleTask);
	}

	// Finally, call PostProcessGraph if something happened
	if (ProcessAllCellsTaskId != InvalidPCGTaskId)
	{
		auto PostProcessGraphTask = [ComponentPtr, InNewBounds, bGenerate](FPCGContext* Context) {
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				Component->PostProcessGraph(InNewBounds, bGenerate, Context);
			}
			return true;
		};

		return GraphExecutor->ScheduleGenericWithContext(PostProcessGraphTask, Component, { ProcessAllCellsTaskId });
	}
	else
	{
		if(Component)
		{
			Component->OnProcessGraphAborted();
		}

		return InvalidPCGTaskId;
	}
}

FPCGTaskId UPCGSubsystem::CleanupGraph(UPCGComponent* Component, const FBox& InBounds, bool bRemoveComponents, bool bSave)
{
	TWeakObjectPtr<UPCGComponent> ComponentPtr(Component);

	auto ScheduleTask = [this, ComponentPtr, bRemoveComponents, bSave](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		UPCGComponent* Component = ComponentPtr.Get();
		check(Component != nullptr && PCGActor != nullptr);

		if (!PCGActor || !Component || !IsValid(Component))
		{
			return InvalidPCGTaskId;
		}

		if (UPCGComponent* LocalComponent = PCGActor->GetLocalComponent(Component))
		{
			// Ensure to avoid infinite loop
			if (ensure(!LocalComponent->IsPartitioned()))
			{
				return LocalComponent->CleanupInternal(bRemoveComponents, bSave, TaskDependencies);
			}
		}

		return InvalidPCGTaskId;
	};

	FPCGTaskId ProcessAllCellsTaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, Component->GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, bSave, ScheduleTask);

	// Finally, call PostCleanupGraph if something happened
	if (ProcessAllCellsTaskId != InvalidPCGTaskId)
	{
		auto PostCleanupGraph = [ComponentPtr]() {
			if (UPCGComponent* Component = ComponentPtr.Get())
			{
				Component->PostCleanupGraph();
			}
			return true;
		};

		return GraphExecutor->ScheduleGeneric(PostCleanupGraph, Component, { ProcessAllCellsTaskId });
	}
	else
	{
		if (Component)
		{
			Component->PostCleanupGraph();
		}

		return InvalidPCGTaskId;
	}
}

void UPCGSubsystem::DirtyGraph(UPCGComponent* Component, const FBox& InBounds, EPCGComponentDirtyFlag DirtyFlag)
{
	check(Component);

	// Immediate operation
	auto DirtyTask = [DirtyFlag](UPCGComponent* LocalComponent)
	{
		LocalComponent->DirtyGenerated(DirtyFlag);
		return InvalidPCGTaskId;
	};

	DispatchToRegisteredLocalComponents(Component, DirtyTask);
}

void UPCGSubsystem::CleanupPartitionActors(const FBox& InBounds)
{
	auto ScheduleTask = [this](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		auto CleanupTask = [PCGActor]() {
			check(PCGActor);
			PCGActor->CleanupDeadGraphInstances();

			return true;
		};

		return GraphExecutor->ScheduleGeneric(CleanupTask, nullptr, TaskDependencies);
	};

	PCGSubsystem::ForEachIntersectingCell(GraphExecutor, GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);
}

void UPCGSubsystem::ClearPCGLink(UPCGComponent* InComponent, const FBox& InBounds, AActor* InNewActor)
{
	TWeakObjectPtr<AActor> NewActorPtr(InNewActor);
	TWeakObjectPtr<UPCGComponent> ComponentPtr(InComponent);

	auto ScheduleTask = [this, NewActorPtr, ComponentPtr](APCGPartitionActor* PCGActor, const FBox& InIntersectedBounds, const TArray<FPCGTaskId>& TaskDependencies) {
		auto MoveTask = [this, NewActorPtr, ComponentPtr, PCGActor]() {
			check(NewActorPtr.IsValid() && ComponentPtr.IsValid() && PCGActor != nullptr);

			if (TObjectPtr<UPCGComponent> LocalComponent = PCGActor->GetLocalComponent(ComponentPtr.Get()))
			{
				LocalComponent->MoveResourcesToNewActor(NewActorPtr.Get(), /*bCreateChild=*/true);
			}

			return true;
		};

		return GraphExecutor->ScheduleGeneric(MoveTask, ComponentPtr.Get(), TaskDependencies);
	};

	FPCGTaskId TaskId = PCGSubsystem::ForEachIntersectingCell(GraphExecutor, GetWorld(), InBounds, /*bCreateActor=*/false, /*bLoadCell=*/false, /*bSave=*/false, ScheduleTask);

	// Verify if the NewActor has some components attached to its root or attached actors. If not, destroy it.
	// Return false if the new actor is not valid or destroyed.
	auto VerifyAndDestroyNewActor = [this, NewActorPtr]() {
		check(NewActorPtr.IsValid());

		USceneComponent* RootComponent = NewActorPtr->GetRootComponent();
		check(RootComponent);

		AActor* NewActor = NewActorPtr.Get();

		TArray<AActor*> AttachedActors;
		NewActor->GetAttachedActors(AttachedActors);

		if (RootComponent->GetNumChildrenComponents() == 0 && AttachedActors.IsEmpty())
		{
			GetWorld()->DestroyActor(NewActor);
			return false;
		}

		return true;
	};

	if (TaskId != InvalidPCGTaskId)
	{
		auto CleanupTask = [this, ComponentPtr, VerifyAndDestroyNewActor]() {

			// If the new actor is valid, clean up the original component.
			if (VerifyAndDestroyNewActor())
			{
				check(ComponentPtr.IsValid());
				ComponentPtr->Cleanup(/*bRemoveComponents=*/true);
			}

			return true;
		};

		GraphExecutor->ScheduleGeneric(CleanupTask, InComponent, { TaskId });
	}
	else
	{
		VerifyAndDestroyNewActor();
	}
}

void UPCGSubsystem::DeletePartitionActors(bool bOnlyDeleteUnused)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeletePartitionActors);

	// TODO: This should be handled differently when we are able to stop a generation
	// For now, we need to be careful and not delete partition actors that are linked to components that are currently
	// generating stuff.
	// Also, we keep a set on all those actors, in case the partition actor status changes during the loop.
	TSet<TObjectPtr<APCGPartitionActor>> ActorsNotSafeToBeDeleted;

	TSet<UPackage*> PackagesToCleanup;
	TSet<FString> PackagesToDeleteFromSCC;
	UWorld* World = GetWorld();

	if (!World || !World->GetWorldPartition())
	{
		return;
	}

	auto GatherAndDestroyLoadedActors = [&PackagesToCleanup, &PackagesToDeleteFromSCC, World, &ActorsNotSafeToBeDeleted](AActor* Actor) -> bool
	{
		// Make sure that this actor was not flagged to not be deleted, or not safe for deletion
		TObjectPtr<APCGPartitionActor> PartitionActor = CastChecked<APCGPartitionActor>(Actor);
		if (!PartitionActor->IsSafeForDeletion())
		{
			ActorsNotSafeToBeDeleted.Add(PartitionActor);
		}
		else if (!ActorsNotSafeToBeDeleted.Contains(PartitionActor))
		{
			// Also reset the last generated bounds to indicate to the component to re-create its PartitionActors when it generates.
			for (TObjectPtr<UPCGComponent> PCGComponent : PartitionActor->GetAllOriginalPCGComponents())
			{
				if (PCGComponent)
				{
					PCGComponent->ResetLastGeneratedBounds();
				}
			}

			if (UPackage* ExternalPackage = PartitionActor->GetExternalPackage())
			{
				PackagesToCleanup.Add(ExternalPackage);
			}

			World->DestroyActor(PartitionActor);
		}

		return true;
	};

	auto GatherAndDestroyActors = [&PackagesToCleanup, &PackagesToDeleteFromSCC, World, &ActorsNotSafeToBeDeleted, &GatherAndDestroyLoadedActors](const FWorldPartitionActorDesc* ActorDesc) {
		if (ActorDesc->GetActor())
		{
			GatherAndDestroyLoadedActors(ActorDesc->GetActor());
		}
		else
		{
			PackagesToDeleteFromSCC.Add(ActorDesc->GetActorPackage().ToString());
			World->GetWorldPartition()->RemoveActor(ActorDesc->GetGuid());
		}

		return true;
	};

	// First, clear selection otherwise it might crash
	if (GEditor)
	{
		GEditor->SelectNone(true, true, false);
	}

	if (bOnlyDeleteUnused)
	{		
		// Need to copy to avoid deadlock
		TMap<FIntVector, TObjectPtr<APCGPartitionActor>> PartitionActorsMapCopy;
		{
			FReadScopeLock ReadLock(PartitionActorsMapLock);
			PartitionActorsMapCopy = PartitionActorsMap;
		}

		for (const TPair<FIntVector, TObjectPtr<APCGPartitionActor>>& Item : PartitionActorsMapCopy)
		{
			if (APCGPartitionActor* PartitionActor = Item.Value)
			{
				bool bIntersectWithOneComponent = false;

				auto FoundComponents = [&bIntersectWithOneComponent](UPCGComponent*) { bIntersectWithOneComponent = true; };

				ForAllIntersectingComponents(PartitionActor->GetFixedBounds(), FoundComponents);

				if (!bIntersectWithOneComponent)
				{
					GatherAndDestroyLoadedActors(PartitionActor);
				}
			}
		}
	}
	else
	{
		FWorldPartitionHelpers::ForEachActorDesc<APCGPartitionActor>(World->GetWorldPartition(), GatherAndDestroyActors);

		// Also cleanup the remaining actors that don't have descriptors, if we have a loaded level
		if (ULevel* Level = World->GetCurrentLevel())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGSubsystem::DeletePartitionActors::ForEachActorInLevel);
			UPCGActorHelpers::ForEachActorInLevel<APCGPartitionActor>(Level, GatherAndDestroyLoadedActors);
		}
	}

	if (!ActorsNotSafeToBeDeleted.IsEmpty())
	{
		// FIXME: see at the top for this function.
		UE_LOG(LogPCG, Error, TEXT("Tried to delete PCGPartitionActors while their PCGComponent were refreshing. All PCGPartitionActors that are linked to those PCGComponents won't be deleted. You should retry deleting them when the refresh is done."));
	}

	if (PackagesToCleanup.Num() > 0)
	{
		ObjectTools::CleanupAfterSuccessfulDelete(PackagesToCleanup.Array(), /*bPerformanceReferenceCheck=*/true);
	}

	if (PackagesToDeleteFromSCC.Num() > 0)
	{
		FPackageSourceControlHelper PackageHelper;
		if (!PackageHelper.Delete(PackagesToDeleteFromSCC.Array()))
		{
			// Log error...
		}
	}
}

void UPCGSubsystem::NotifyGraphChanged(UPCGGraph* InGraph)
{
	if (GraphExecutor)
	{
		GraphExecutor->NotifyGraphChanged(InGraph);
	}
}

void UPCGSubsystem::CleanFromCache(const IPCGElement* InElement)
{
	if (GraphExecutor)
	{
		GraphExecutor->GetCache().CleanFromCache(InElement);
	}
}

void UPCGSubsystem::BuildLandscapeCache()
{
	if (UPCGLandscapeCache* LandscapeCache = GetLandscapeCache())
	{
		PCGWorldActor->Modify();
		LandscapeCache->PrimeCache();
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Unable to build landscape cache because either the world is null or there is no PCG world actor"));
	}
}

void UPCGSubsystem::ClearLandscapeCache()
{
	if (UPCGLandscapeCache* LandscapeCache = GetLandscapeCache())
	{
		LandscapeCache->ClearCache();
	}
}

void UPCGSubsystem::ResetPartitionActorsMap()
{
	PartitionActorsMapLock.WriteLock();
	PartitionActorsMap.Empty();
	PartitionActorsMapLock.WriteUnlock();
}

#endif // WITH_EDITOR

void UPCGSubsystem::FlushCache()
{
	if (GraphExecutor)
	{
		GraphExecutor->GetCache().ClearCache();
	}
}