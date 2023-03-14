// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGVolume.h"
#include "Grid/PCGComponentOctree.h"

#include "PCGSubsystem.generated.h"

class FPCGGraphExecutor;
class APCGPartitionActor;
class APCGWorldActor;
class UPCGGraph;
struct FPCGDataCollection;
class UPCGLandscapeCache;

class IPCGElement;
typedef TSharedPtr<IPCGElement, ESPMode::ThreadSafe> FPCGElementPtr;


/**
* UPCGSubsystem
*/
UCLASS()
class PCG_API UPCGSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPCGSubsystem();

	//~ Begin USubsystem Interface.
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	//~ Begin UWorldSubsystem Interface.
	virtual void PostInitialize() override;
	// need UpdateStreamingState? 
	//~ End UWorldSubsystem Interface.

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	APCGWorldActor* GetPCGWorldActor();
#if WITH_EDITOR
	void DestroyPCGWorldActor();
#endif
	void RegisterPCGWorldActor(APCGWorldActor* InActor);
	void UnregisterPCGWorldActor(APCGWorldActor* InActor);

	UPCGLandscapeCache* GetLandscapeCache();

	// Schedule graph (owner -> graph)
	FPCGTaskId ScheduleComponent(UPCGComponent* PCGComponent, bool bSave, const TArray<FPCGTaskId>& Dependencies);

	/** Schedule cleanup(owner->graph). Note that in non-partitioned mode, cleanup is immediate. */
	FPCGTaskId ScheduleCleanup(UPCGComponent* PCGComponent, bool bRemoveComponents, bool bSave, const TArray<FPCGTaskId>& Dependencies);

	// Schedule graph (used internally for dynamic subgraph execution)
	FPCGTaskId ScheduleGraph(UPCGGraph* Graph, UPCGComponent* SourceComponent, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& Dependencies);

	// Schedule graph (used internally for dynamic subgraph execution)
	FPCGTaskId ScheduleGraph(UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& Dependencies);

	/** General job scheduling
	*  @param InOperation:       Callback that returns true if the task is done, false otherwise.
	*  @param InSourceComponent: PCG component associated with this task. Can be null.
	*  @param TaskDependencies:  List of all the dependencies for this task.
	*/
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskDependencies);
	
	/** General job scheduling with context
	*  @param InOperation:       Callback that takes a Context as argument and returns true if the task is done, false otherwise.
	*  @param InSourceComponent: PCG component associated with this task. Can be null.
	*  @param TaskDependencies:  List of all the dependencies for this task.
	*  @param bConsumeInputData: If your task need a context, but don't need the input data, set this flag to false. Default is true.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskDependencies, bool bConsumeInputData = true);

	/** Gets the output data for a given task */
	bool GetOutputData(FPCGTaskId InTaskId, FPCGDataCollection& OutData);

	/** Register a new PCG Component or update it, will be added to the octree if it doesn't exists yet. Returns true if it was added/updated. Thread safe */
	bool RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping = true);

	/** In case of BP Actors, we need to remap the old component destroyed by the construction script to the new one. Returns true if re-mapping succeeded. */
	bool RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent);

	/** Unregister a PCG Component, will be removed from the octree. Can force it, if we have a delayed unregister. Thread safe */
	void UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce = false);

	/** Register a new Partition actor, will be added to a map and will query all intersecting volume to bind to them if asked. Thread safe */
	void RegisterPartitionActor(APCGPartitionActor* InActor, bool bDoComponentMapping = true);

	/** Unregister a Partition actor, will be removed from the map and remove itself to all intersecting volumes. Thread safe */
	void UnregisterPartitionActor(APCGPartitionActor* InActor);

	TSet<TObjectPtr<UPCGComponent>> GetAllRegisteredComponents() const;

	/** Flushes the graph cache completely, use only for debugging */
	void FlushCache();

#if WITH_EDITOR
public:
	/** Schedules an operation to cleanup the graph in the given bounds */
	FPCGTaskId CleanupGraph(UPCGComponent* Component, const FBox& InBounds, bool bRemoveComponents, bool bSave);

	/** Immediately dirties the partition actors in the given bounds */
	void DirtyGraph(UPCGComponent* Component, const FBox& InBounds, EPCGComponentDirtyFlag DirtyFlag);

	/** Partition actors methods */
	void CleanupPartitionActors(const FBox& InBounds);
	void DeletePartitionActors(bool bOnlyDeleteUnused);

	/** Propagate to the graph compiler graph changes */
	void NotifyGraphChanged(UPCGGraph* InGraph);

	/** Cleans up the graph cache on an element basis */
	void CleanFromCache(const IPCGElement* InElement);

	/** Move all resources from sub actors to a new actor */
	void ClearPCGLink(UPCGComponent* InComponent, const FBox& InBounds, AActor* InNewActor);

	/** If the partition grid size change, call this to empty the Partition actors map */
	void ResetPartitionActorsMap();

	/** Builds the landscape data cache */
	void BuildLandscapeCache();

	/** Clears the landscape data cache */
	void ClearLandscapeCache();

private:
	enum class EOperation : uint32
	{
		Partition,
		Unpartition,
		Generate
	};

	FPCGTaskId ProcessGraph(UPCGComponent* Component, const FBox& InPreviousBounds, const FBox& InNewBounds, EOperation InOperation, bool bSave);
#endif // WITH_EDITOR
	
private:
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
	
private:
	APCGWorldActor* PCGWorldActor = nullptr;
	FPCGGraphExecutor* GraphExecutor = nullptr;

#if WITH_EDITOR
	FCriticalSection PCGWorldActorLock;
#endif

	FPCGComponentOctree PCGComponentOctree;
	TMap<TObjectPtr<UPCGComponent>, FPCGComponentOctreeIDSharedRef> ComponentToIdMap;
	mutable FRWLock PCGVolumeOctreeLock;

	TMap<FIntVector, TObjectPtr<APCGPartitionActor>> PartitionActorsMap;
	mutable FRWLock PartitionActorsMapLock;

	TMap<TObjectPtr<const UPCGComponent>, TSet<TObjectPtr<APCGPartitionActor>>> ComponentToPartitionActorsMap;
	mutable FRWLock ComponentToPartitionActorsMapLock;

	TSet<TObjectPtr<UPCGComponent>> DelayedComponentToUnregister;
	mutable FCriticalSection DelayedComponentToUnregisterLock;
};