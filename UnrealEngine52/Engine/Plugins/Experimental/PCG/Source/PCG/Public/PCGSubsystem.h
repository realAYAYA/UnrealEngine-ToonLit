// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PCGCommon.h"
#include "PCGActorAndComponentMapping.h"
#include "Grid/PCGComponentOctree.h"
#include "Utils/PCGNodeVisualLogs.h"

#include "PCGSubsystem.generated.h"


class APCGPartitionActor;
class APCGWorldActor;
class UPCGGraph;
class UPCGLandscapeCache;

enum class EPCGComponentDirtyFlag : uint8;
enum class ETickableTickType : uint8;

class FPCGGraphCompiler;
class FPCGGraphExecutor;
struct FPCGContext;
struct FPCGDataCollection;
class UPCGSettings;

class IPCGElement;
typedef TSharedPtr<IPCGElement, ESPMode::ThreadSafe> FPCGElementPtr;

class UWorld;

/**
* UPCGSubsystem
*/
UCLASS()
class PCG_API UPCGSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	friend UPCGActorAndComponentMapping;

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

	/** Will return the subsystem from the World if it exists and if it is initialized */
	static UPCGSubsystem* GetInstance(UWorld* World);

#if WITH_EDITOR
	/** Returns PIE world if it is active, otherwise returns editor world. */
	static UPCGSubsystem* GetActiveEditorInstance();
#endif

	/** Subsystem must not be used without this condition being true. */
	bool IsInitialized() const { return GraphExecutor != nullptr; }

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

	/** Cancels currently running generation */
	void CancelGeneration(UPCGComponent* Component);

	/** Cancels currently running generation on given graph */
	void CancelGeneration(UPCGGraph* Graph);

	/** Returns true if there are any tasks for this graph currently scheduled or executing. */
	bool IsGraphCurrentlyExecuting(UPCGGraph* Graph);

	/** Cancels everything running */
	void CancelAllGeneration();

	/** Gets the output data for a given task */
	bool GetOutputData(FPCGTaskId InTaskId, FPCGDataCollection& OutData);

	/** Register a new PCG Component or update it, will be added to the octree if it doesn't exists yet. Returns true if it was added/updated. Thread safe */
	bool RegisterOrUpdatePCGComponent(UPCGComponent* InComponent, bool bDoActorMapping = true) { return ActorAndComponentMapping.RegisterOrUpdatePCGComponent(InComponent, bDoActorMapping); }

	/** In case of BP Actors, we need to remap the old component destroyed by the construction script to the new one. Returns true if re-mapping succeeded. */
	bool RemapPCGComponent(const UPCGComponent* OldComponent, UPCGComponent* NewComponent, bool bDoActorMapping) { return ActorAndComponentMapping.RemapPCGComponent(OldComponent, NewComponent, bDoActorMapping); }

	/** Unregister a PCG Component, will be removed from the octree. Can force it, if we have a delayed unregister. Thread safe */
	void UnregisterPCGComponent(UPCGComponent* InComponent, bool bForce = false) { ActorAndComponentMapping.UnregisterPCGComponent(InComponent, bForce); }

	/** Register a new Partition actor, will be added to a map and will query all intersecting volume to bind to them if asked. Thread safe */
	void RegisterPartitionActor(APCGPartitionActor* InActor, bool bDoComponentMapping = true) { ActorAndComponentMapping.RegisterPartitionActor(InActor, bDoComponentMapping); }

	/** Unregister a Partition actor, will be removed from the map and remove itself to all intersecting volumes. Thread safe */
	void UnregisterPartitionActor(APCGPartitionActor* InActor) { ActorAndComponentMapping.UnregisterPartitionActor(InActor); }

	TSet<TObjectPtr<UPCGComponent>> GetAllRegisteredPartitionedComponents() const { return ActorAndComponentMapping.GetAllRegisteredPartitionedComponents(); }

	/** Flushes the graph cache completely, use only for debugging */
	void FlushCache();

	/* Call the InFunc function to all local component registered to the original component. Thread safe*/
	void ForAllRegisteredLocalComponents(UPCGComponent* OriginalComponent, const TFunction<void(UPCGComponent*)>& InFunc) const;

	/** True if graph cache debugging is enabled. */
	bool IsGraphCacheDebuggingEnabled() const;

#if WITH_EDITOR
public:
	/** Schedule refresh on the current or next frame */
	FPCGTaskId ScheduleRefresh(UPCGComponent* SourceComponent);

	/** Schedules an operation to cleanup the graph in the given bounds */
	FPCGTaskId CleanupGraph(UPCGComponent* Component, const FBox& InBounds, bool bRemoveComponents, bool bSave);

	/** Immediately dirties the partition actors in the given bounds */
	void DirtyGraph(UPCGComponent* Component, const FBox& InBounds, EPCGComponentDirtyFlag DirtyFlag);

	/** Partition actors methods */
	void CleanupPartitionActors(const FBox& InBounds);
	void DeletePartitionActors(bool bOnlyDeleteUnused);

	/** Propagate to the graph compiler graph changes */
	void NotifyGraphChanged(UPCGGraph* InGraph);

	/** Cleans up the graph cache on an element basis. InSettings is used for debugging and is optional. */
	void CleanFromCache(const IPCGElement* InElement, const UPCGSettings* InSettings = nullptr);

	/** Move all resources from sub actors to a new actor */
	void ClearPCGLink(UPCGComponent* InComponent, const FBox& InBounds, AActor* InNewActor);

	/** If the partition grid size change, call this to empty the Partition actors map */
	void ResetPartitionActorsMap() { ActorAndComponentMapping.ResetPartitionActorsMap(); }

	/** Builds the landscape data cache */
	void BuildLandscapeCache(bool bQuiet = false);

	/** Clears the landscape data cache */
	void ClearLandscapeCache();

	/** Returns the graph compiler so we can figure out task info in the profiler view **/
	const FPCGGraphCompiler* GetGraphCompiler() const;

	/** Returns how many times InElement is present in the cache. */
	uint32 GetGraphCacheEntryCount(IPCGElement* InElement) const;

	/** Get graph warnings and errors for all nodes. */
	const FPCGNodeVisualLogs& GetNodeVisualLogs() const { return NodeVisualLogs; }
	FPCGNodeVisualLogs& GetNodeVisualLogsMutable() { return NodeVisualLogs; }

private:
	enum class EOperation : uint32
	{
		Partition,
		Unpartition,
		Generate
	};

	FPCGTaskId ProcessGraph(UPCGComponent* Component, const FBox& InPreviousBounds, const FBox& InNewBounds, EOperation InOperation, bool bSave);
	void CreatePartitionActorsWithinBounds(const FBox& InBounds);

	FPCGNodeVisualLogs NodeVisualLogs;
#endif // WITH_EDITOR
	
private:
	APCGWorldActor* PCGWorldActor = nullptr;
	FPCGGraphExecutor* GraphExecutor = nullptr;
	bool bHasTickedOnce = false;
	UPCGActorAndComponentMapping ActorAndComponentMapping;

#if WITH_EDITOR
	FCriticalSection PCGWorldActorLock;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGComponent.h"
#include "PCGVolume.h"
#endif
