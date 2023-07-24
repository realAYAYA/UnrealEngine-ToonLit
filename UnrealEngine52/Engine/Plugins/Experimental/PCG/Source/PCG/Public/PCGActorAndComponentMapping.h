// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Grid/PCGComponentOctree.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/Function.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class APCGPartitionActor;
class UObject;
class UPCGComponent;
class UPCGSubsystem;

/**
* This class handle any necessary mapping between actors and pcg components.
* Its meant to be part of the PCG Subsystem and owned by it. We offload some logic to this class to avoid to clutter the subsystem.
* For now it is used for:
* - Mapping between PCG Components and Partition actors: Dispatch tasks from original components to local components
* - Tracking non partitioned PCG Components that has tracking actors needs.
* - TODO: Tracking actors: Be able to react to actors changes, and potentially dirty/refresh affected components
*/
class UPCGActorAndComponentMapping
{
public:
	friend UPCGSubsystem;

	~UPCGActorAndComponentMapping() = default;
	
	/** Should be called by the subsystem to handle delayed operations. */
	void Tick();

#if WITH_EDITOR
	/** If the partition grid size change, call this to empty the Partition actors map */
	void ResetPartitionActorsMap();

	void RegisterTrackingCallbacks();
	void TeardownTrackingCallbacks();

	void OnActorAdded(AActor* InActor);
	void OnActorDeleted(AActor* InActor);
	void OnActorMoved(AActor* InActor);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
#endif // WITH_EDITOR

	/** Register a new PCG Component or update it. Returns true if it was added/updated. Thread safe */
	bool RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping = true);

	/** In case of BP Actors, we need to remap the old component destroyed by the construction script to the new one. Returns true if a re-mapping was done. Thread safe */
	bool RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent, bool bDoActorMapping);

	/** Unregister a PCG Component. Can force it, if we have a delayed unregister. Thread safe */
	void UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce = false);

	/** Register a new Partition actor, will be added to a map and will query all intersecting volume to bind to them if asked. Thread safe */
	void RegisterPartitionActor(APCGPartitionActor* InActor, bool bDoComponentMapping = true);

	/** Unregister a Partition actor, will be removed from the map and remove itself to all intersecting volumes. Thread safe */
	void UnregisterPartitionActor(APCGPartitionActor* InActor);

	/** Return a copy of all the registered partitioned components. Thread safe */
	TSet<TObjectPtr<UPCGComponent>> GetAllRegisteredPartitionedComponents() const;

	/** Check if a component is currently tracked. Thread safe */
	bool IsComponentTracked(const UPCGComponent* InComponent) const;

private:
	// This class is only meant to be used as part of the PCG Subsytem and owned by it.
	// So we put constructors private.
	// We also need this class to be default constructible, since the PCGSubsytem needs to be default constructible.
	UPCGActorAndComponentMapping() = default;
	explicit UPCGActorAndComponentMapping(UPCGSubsystem* PCGSubsystem);

	bool RegisterOrUpdatePartitionedPCGComponent(UPCGComponent* InComponent, bool bDoActorMapping = true);
	bool RegisterOrUpdateNonPartitionedPCGComponent(UPCGComponent* InComponent);

	void UnregisterPartitionedPCGComponent(UPCGComponent* InComponent);
	void UnregisterNonPartitionedPCGComponent(UPCGComponent* InComponent);

	/* Call the InFunc function to all local component registered to the original component. Return the list of all the tasks scheduled. Thread safe*/
	TArray<FPCGTaskId> DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const;

	/* Call the InFunc function to all local component from the set of partition actors. Return the list of all the tasks scheduled. */
	TArray<FPCGTaskId> DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunction<FPCGTaskId(UPCGComponent*)>& InFunc) const;

	/** Iterate other all the components which bounds intersect the box in param and call a callback. Thread safe */
	void ForAllIntersectingComponents(const FBoxCenterAndExtent& InBounds, TFunction<void(UPCGComponent*)> InFunc) const;

	/** Iterate other all the int coordinates given a box and call a callback. Thread safe */
	void ForAllIntersectingPartitionActors(const FBox& InBounds, TFunction<void(APCGPartitionActor*)> InFunc) const;

	/** Update the current mapping between a PCG component and its PCG Partition actors */
	void UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent);

	/** Delete the current mapping between a PCG component and its PCG Partition actors */
	void DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent);

	/** Will register/update tracking if a component was registered/updated. */
	void RegisterOrUpdateTracking(UPCGComponent* InComponent);

	/** Remap the tracking in case of BP components. */
	void RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent);

	/** Unregister tracking when a component is removed. */
	void UnregisterTracking(UPCGComponent* InComponent);

private:
	// Cached subsystem
	UPCGSubsystem* PCGSubsystem = nullptr;

	// Mapping Component <-> PartitionActor
	/** Octree tracking all partitioned pcg components */
	FPCGComponentOctreeAndMap PartitionedOctree;

	/** Grid mapping for partition actors. We can only have 1 partition actor per grid cell. */
	TMap<FIntVector, TObjectPtr<APCGPartitionActor>> PartitionActorsMap;
	mutable FRWLock PartitionActorsMapLock;

	/** Mapping between original components and its overlapping partition actors. */
	TMap<TObjectPtr<const UPCGComponent>, TSet<TObjectPtr<APCGPartitionActor>>> ComponentToPartitionActorsMap;
	mutable FRWLock ComponentToPartitionActorsMapLock;

	/** Components to be unregister at the next frame. cf. UnregisterComponent for a better understanding on why it is needed. */
	TSet<TObjectPtr<UPCGComponent>> DelayedComponentToUnregister;
	mutable FCriticalSection DelayedComponentToUnregisterLock;

	// Tracking actors
	/** Will hold all the components that are not partitioned (and not local) and are tracking something. Will be use to dispatch actor tracking updates. */
	FPCGComponentOctreeAndMap NonPartitionedOctree;

	/** Keep a mapping between tracked actors and the components that track them. */
	TMap<TWeakObjectPtr<AActor>, TSet<TObjectPtr<UPCGComponent>>> TrackedActorsToComponentsMap;

	/** Also keep the reverse mapping. */
	TMap<TObjectPtr<UPCGComponent>, TSet<TWeakObjectPtr<AActor>>> TrackedComponentsToActorsMap;
	mutable FRWLock TrackedComponentsLock;

	/** Keep a mapping between tracked actors and their dependencies. */
	TMap<TWeakObjectPtr<AActor>, TSet<TObjectPtr<UObject>>> TrackedActorsToDependenciesMap;
};