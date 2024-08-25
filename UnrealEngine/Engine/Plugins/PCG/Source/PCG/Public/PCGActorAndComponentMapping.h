// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Elements/PCGActorSelector.h"
#include "Grid/PCGComponentOctree.h"
#include "RuntimeGen/PCGRuntimeGenScheduler.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/Function.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class ALandscapeProxy;
class APCGPartitionActor;
class FLandscapeProxyComponentDataChangedParams;
class ILevelInstanceInterface;
class UObject;
class UPCGComponent;
class UPCGGraph;
class UPCGSubsystem;
class UWorld;

/**
* This class handle any necessary mapping between actors and pcg components.
* Its meant to be part of the PCG Subsystem and owned by it. We offload some logic to this class to avoid to clutter the subsystem.
* For now it is used for:
* - Mapping between PCG Components and Partition actors: Dispatch tasks from original components to local components
* - Tracking non partitioned PCG Components that has tracking actors needs.
* - Tracking actors: Be able to react to actors changes, and potentially dirty/refresh affected components
*/
class FPCGActorAndComponentMapping
{
public:
	friend UPCGSubsystem;
	friend FPCGRuntimeGenScheduler;

	~FPCGActorAndComponentMapping() = default;

	/** Initializes callbacks, etc, tied to the PCG subsystem */
	void Initialize(UWorld* World);

	/** Deinitializes callbacks, etc, tied to the PCG subsystem */
	void Deinitialize();
	
	/** Should be called by the subsystem to handle delayed operations. */
	void Tick();

#if WITH_EDITOR
	/** Notify that we exited the Landscape edit mode. */
	void NotifyLandscapeEditModeExited();
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
	TSet<UPCGComponent*> GetAllRegisteredPartitionedComponents() const;

	/** Return a copy of all the registered non-partitioned components. Thread safe */
	TSet<UPCGComponent*> GetAllRegisteredNonPartitionedComponents() const;

	/** Return a copy of all the registered components. Thread safe */
	TSet<UPCGComponent*> GetAllRegisteredComponents() const;

	/** Retrieves a local component using grid size and grid coordinates, returns nullptr if no such component is found. */
	UPCGComponent* GetLocalComponent(uint32 GridSize, const FIntVector& CellCoords, const UPCGComponent* InOriginalComponent, bool bRuntimeGenerated = false) const;

	/** Retrieves a partition actor using grid size and grid coordinates, returns nullptr if no such partition actor is found. */
	APCGPartitionActor* GetPartitionActor(uint32 GridSize, const FIntVector& CellCoords, bool bRuntimeGenerated = false) const;

private:

#if WITH_EDITOR
	void RegisterDelegates();
	void UnregisterDelegates();

	/** If the partition grid size change, call this to empty the Partition actors map */
	void ResetPartitionActorsMap();

	void RegisterTrackingCallbacks();
	void TeardownTrackingCallbacks();

	void RegisterTracking(UPCGComponent* InComponent);
	void UpdateTracking(UPCGComponent* InComponent, bool bInShouldDirtyActors, const TArray<FPCGSelectionKey>* OptionalChangedKeys = nullptr);

	void ProcessDelayedEvents();
	bool ShouldDelayActor(AActor* InActor) const;
#endif

	// This class is only meant to be used as part of the PCG Subsytem and owned by it.
	// So we put constructors private.
	// We also need this class to be default constructible, since the PCGSubsytem needs to be default constructible.
	FPCGActorAndComponentMapping() = default;
	explicit FPCGActorAndComponentMapping(UPCGSubsystem* PCGSubsystem);

	bool RegisterOrUpdatePartitionedPCGComponent(UPCGComponent* InComponent, bool bDoActorMapping = true);
	bool RegisterOrUpdateNonPartitionedPCGComponent(UPCGComponent* InComponent);

	void UnregisterPartitionedPCGComponent(UPCGComponent* InComponent);
	void UnregisterNonPartitionedPCGComponent(UPCGComponent* InComponent);

	/* Call the InFunc function to all local component registered to the original component. Return the list of all the tasks scheduled. Thread safe*/
	TArray<FPCGTaskId> DispatchToRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunctionRef<FPCGTaskId(UPCGComponent*)>& InFunc) const;

	/* Call the InFunc function to all local component from the set of partition actors. Return the list of all the tasks scheduled. */
	TArray<FPCGTaskId> DispatchToLocalComponents(UPCGComponent* OriginalComponent, const TSet<TObjectPtr<APCGPartitionActor>>& PartitionActors, const TFunctionRef<FPCGTaskId(UPCGComponent*)>& InFunc) const;

	/** Call the InFunc function to all partitioned components which bounds intersect 'InBounds'. */
	void ForAllIntersectingPartitionedComponents(const FBoxCenterAndExtent& InBounds, TFunctionRef<void(UPCGComponent*)> InFunc) const;

	/** Call the InFunc function for all original components (regardless of partitioned or not). */
	void ForAllOriginalComponents(TFunctionRef<void(UPCGComponent*)> InFunc);

	/** Gather all the PCG components within some bounds. */
	TArray<UPCGComponent*> GetAllIntersectingComponents(const FBoxCenterAndExtent& InBounds) const;

	/** Iterate other all the int coordinates given a box and call a callback. Thread safe */
	void ForAllIntersectingPartitionActors(const FBox& InBounds, TFunctionRef<void(APCGPartitionActor*)> InFunc) const;

	/** Update the current mapping between a PCG component and its PCG Partition actors */
	void UpdateMappingPCGComponentPartitionActor(UPCGComponent* InComponent);

	/** Delete the current mapping between a PCG component and its PCG Partition actors */
	void DeleteMappingPCGComponentPartitionActor(UPCGComponent* InComponent);

	/** Returns true if a component is registered */
	bool IsComponentRegistered(const UPCGComponent* InComponent) const;

	/** Return true if there are any Original or Non-Partitioned components set to GenerateAtRuntime. */
	bool AnyRuntimeGenComponentsExist() const;

	// Typedef to store the previous position and previous tags of changed actors.
	using FActorPreviousData = TTuple<FBox, TSet<FName>, double>;

#if WITH_EDITOR
	/** Return true if the key is tracked.*/
	bool IsKeyTracked(const FPCGSelectionKey& InKey) const;

	/** Return true if the actor is tracked.*/
	bool IsActorTracked(const AActor* InActor) const;

	void OnActorAdded(AActor* InActor);
	void OnActorLoaded(AActor& InActor);
	void OnActorAdded_Internal(AActor* InActor, bool bShouldDirty);
	void OnActorChanged_Internal(AActor* InActor, UObject* InOriginatingChangeObject);
	void OnActorChanged_Recursive(AActor* InActor, UObject* InOriginatingChangeObject);
	void OnActorDeleted(AActor* InActor);
	void OnActorUnloaded(AActor& InActor);
	void OnActorDeleted_Internal(AActor* InActor, bool bShouldDirty);
	void OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams);
	void OnLevelInstancesUpdated(const TArray<ILevelInstanceInterface*>& InLevelInstances);
	void ApplyLandscapeChanges(ALandscapeProxy* InLandscape);
	void OnObjectModified(UObject* InObject);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
	void OnObjectSaved(UObject* InObject, FObjectPreSaveContext InObjectSaveContext);
	void OnPCGGraphStartsGenerating(UPCGComponent* InComponent);
	void OnPCGGraphGeneratedOrCleaned(UPCGComponent* InComponent);
	void OnPCGGraphCancelled(UPCGComponent* InComponent);

	/** Remap the tracking in case of BP components. */
	void RemapTracking(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent);

	/** Unregister tracking when a component is removed */
	void UnregisterTracking(UPCGComponent* InComponent);

	/** Unregister tracking when a component is removed or keys are unregistered */
	void UnregisterTracking(UPCGComponent* InComponent, const TSet<FPCGSelectionKey>* OptionalKeysToUntrack);

	/** Trigger an update when an object/actor changed.
	* Can specify previous actor data if it has moved or tags changed to also update components that were at its previous position or tracked previous tags.
	* Can also specify an optional object, originating the change, to avoid re-dirtying a component if it was the origin.
	* Another option when an actor is deleted/unload, don't refresh their components.
	*/
	void OnObjectChanged(UObject* InObject, const FActorPreviousData* InPreviousData = nullptr, const UObject* InOriginatingChangeObject = nullptr, bool bNoRefreshOnOwner = false);

	/** Gather all settings from a given component that track the key, and clear the cache for them. Returns true if we should dirty afterwards (aka at least one settings was cleared and/or landscape changed). */
	bool ClearCacheForKeys(const TArray<FPCGSelectionKey>& InKeys, const UPCGComponent* InComponent, const bool bIntersect, const UObject* InOriginatingChange) const;
#endif // WITH_EDITOR

private:
	// Cached subsystem
	UPCGSubsystem* PCGSubsystem = nullptr;

	// Mapping Component <-> PartitionActor
	/** Octree tracking all partitioned pcg components */
	FPCGComponentOctreeAndMap PartitionedOctree;

	/** Mapping from grid size and grid coords to partition actor. We can only have 1 partition actor per grid cell. */
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>> PartitionActorsMap;
	mutable FRWLock PartitionActorsMapLock;

	/** Mapping from grid size and grid coords to RuntimeGen partition actor. We can only have 1 RuntimeGen partition actor per grid cell. */
	TMap<uint32, TMap<FIntVector, TObjectPtr<APCGPartitionActor>>> RuntimeGenPartitionActorsMap;
	mutable FRWLock RuntimeGenPartitionActorsMapLock;

	/** Mapping between original components and its overlapping partition actors. */
	TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>> ComponentToPartitionActorsMap;
	mutable FRWLock ComponentToPartitionActorsMapLock;

	/** Mapping between original components and its overlapping RuntimeGen partition actors. */
	TMap<const UPCGComponent*, TSet<TObjectPtr<APCGPartitionActor>>> ComponentToRuntimeGenPartitionActorsMap;
	mutable FRWLock ComponentToRuntimeGenPartitionActorsMapLock;

	/** Components to be unregister at the next frame. cf. UnregisterComponent for a better understanding on why it is needed. */
	TSet<UPCGComponent*> DelayedComponentToUnregister;
	mutable FCriticalSection DelayedComponentToUnregisterLock;

	/** Will hold all the components that are not partitioned (and not local) and are tracking something. Will be use to dispatch actor tracking updates. */
	FPCGComponentOctreeAndMap NonPartitionedOctree;

#if WITH_EDITOR
	// Tracking actors
	/** Keep a mapping between tracked keys and the components that track them, and the tracking needs to be culled.*/
	TMap<FPCGSelectionKey, TSet<UPCGComponent*>> CulledTrackedKeysToComponentsMap;

	/** Same mapping but for always tracked keys */
	TMap<FPCGSelectionKey, TSet<UPCGComponent*>> AlwaysTrackedKeysToComponentsMap;

	mutable FRWLock TrackedComponentsLock;

	// Keep track of actors that aren't yet ready (or if the subsystem is not yet ready), whether we should dirty them in next tick.
	TMap<TObjectKey<AActor>, TObjectKey<UObject>> DelayedChangedActors;
	
	/** Transient map of actors and their previous data, it's set in the pre object change to be able to track changes (such as tags or positions) */
	TMap<TObjectKey<AActor>, FActorPreviousData> ActorToPreviousDataMap;

	/** Transient map that keep track of all components that depends on another component currently generating, to trigger the refresh only once, when all are done. */
	TMap<TObjectKey<UPCGComponent>, TArray<TObjectKey<UPCGComponent>>> ComponentsToDependencyMap;

	// Part for the delayed landscape change update
	double LastLandscapeDirtyTime = -1.0;
	TArray<TObjectKey<ALandscapeProxy>> DelayedModifiedLandscapes;

	// Keep track of all dirtied landscapes and force a refresh on them when exiting edit mode.
	TArray<TObjectKey<ALandscapeProxy>> DirtiedLandscapes;

	// Temp boolean to indicate we are currently exiting the landscape edit mode
	bool bIsCurrentlyExitingLandscapeEditMode = false;

	// Time keeper for cleaning up cached previous actor data
	double LastPreviousActorDataCleanup = -1.0;
#endif // WITH_EDITOR
};