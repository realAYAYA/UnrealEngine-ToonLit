// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PCGCommon.h"
#include "PCGActorAndComponentMapping.h"
#include "Grid/PCGComponentOctree.h"
#include "UObject/ObjectKey.h"
#include "Utils/PCGNodeVisualLogs.h"

#include "PCGSubsystem.generated.h"

class APCGPartitionActor;
class APCGWorldActor;
class UPCGGraph;
class UPCGLandscapeCache;
class FPCGRuntimeGenScheduler;

enum class EPCGComponentDirtyFlag : uint8;
enum class ETickableTickType : uint8;

class FPCGGraphCompiler;
class FPCGGraphExecutor;
struct FPCGContext;
struct FPCGDataCollection;
struct FPCGStack;
class UPCGSettings;

class IPCGElement;
typedef TSharedPtr<IPCGElement, ESPMode::ThreadSafe> FPCGElementPtr;

class UWorld;

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(FPCGOnComponentGenerationCompleteOrCancelled, UPCGSubsystem*);
#endif // WITH_EDITOR

/**
* UPCGSubsystem
*/
UCLASS()
class PCG_API UPCGSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	friend FPCGActorAndComponentMapping;

	UPCGSubsystem();

	/** To be used when a PCG component can not have a world anymore, to unregister itself. */
	static UPCGSubsystem* GetSubsystemForCurrentWorld();

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

	void SetConstructionScriptSourceComponent(UPCGComponent* InComponent);
	bool RemoveAndCopyConstructionScriptSourceComponent(AActor* InComponentOwner, FName InComponentName, UPCGComponent*& OutSourceComponent);
#endif

	/** Subsystem must not be used without this condition being true. */
	bool IsInitialized() const { return GraphExecutor != nullptr; }

	APCGWorldActor* GetPCGWorldActor();
	APCGWorldActor* FindPCGWorldActor();

	/** Returns current quality level between Low (0) and Cinematic (4). */
	static int32 GetPCGQualityLevel();
	void OnPCGQualityLevelChanged();

#if WITH_EDITOR
	void DestroyAllPCGWorldActors();
	void DestroyCurrentPCGWorldActor();
	void LogAbnormalComponentStates(bool bGroupByState) const;
#endif
	void RegisterPCGWorldActor(APCGWorldActor* InActor);
	void UnregisterPCGWorldActor(APCGWorldActor* InActor);

	void OnOriginalComponentRegistered(UPCGComponent* InComponent);
	void OnOriginalComponentUnregistered(UPCGComponent* InComponent);

	/** Called by graph executor when a graph is scheduled. */
	void OnScheduleGraph(const FPCGStackContext& StackContext);

	UPCGLandscapeCache* GetLandscapeCache();

	/** Schedule graph(owner->graph) */
	FPCGTaskId ScheduleComponent(UPCGComponent* PCGComponent, EPCGHiGenGrid Grid, bool bForce, const TArray<FPCGTaskId>& InDependencies);

	/** Schedule cleanup(owner->graph). */
	FPCGTaskId ScheduleCleanup(UPCGComponent* PCGComponent, bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies);

	// Schedule graph (used internally for dynamic subgraph execution)
	FPCGTaskId ScheduleGraph(
		UPCGGraph* Graph,
		UPCGComponent* SourceComponent,
		FPCGElementPtr PreGraphElement,
		FPCGElementPtr InputElement,
		const TArray<FPCGTaskId>& Dependencies,
		const FPCGStack* InFromStack,
		bool bAllowHierarchicalGeneration);

	// Schedule graph (used internally for dynamic subgraph execution)
	FPCGTaskId ScheduleGraph(UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& Dependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that returns true if the task is done, false otherwise.
	*  @param SourceComponent:           PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*/
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);

	/** General job scheduling
	*  @param InOperation:               Callback that returns true if the task is done, false otherwise.
	*  @param InAbortOperation:          Callback that will be called if the generic task is cancelled for any reason.
	*  @param SourceComponent:           PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*/
	FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, TFunction<void()> InAbortOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);

	/** General job scheduling with context
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise.
	*  @param SourceComponent:           PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** General job scheduling with context
	*  @param InOperation:               Callback that takes a Context as argument and returns true if the task is done, false otherwise.
	*  @param InAbortOperation:          Callback that will be called if the generic task is cancelled for any reason.
	*  @param SourceComponent:           PCG component associated with this task. Can be null.
	*  @param TaskExecutionDependencies: Task will wait on these tasks to execute and won't take their output data as input.
	*  @param TaskDataDependencies:      Task will wait on these tasks to execute and will take their output data as input.
	*/
	FPCGTaskId ScheduleGenericWithContext(TFunction<bool(FPCGContext*)> InOperation, TFunction<void(FPCGContext*)> InAbortOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies, const TArray<FPCGTaskId>& TaskDataDependencies);

	/** Cancels currently running generation */
	void CancelGeneration(UPCGComponent* Component);

	/** Cancels currently running generation on given graph */
	void CancelGeneration(UPCGGraph* Graph);

	/** Asks the runtime generation scheduler to refresh a given GenerateAtRuntime component. bRemovePartitionActors will also perform a full cleanup of PAs and local components. */
	void RefreshRuntimeGenComponent(UPCGComponent* RuntimeComponent, EPCGChangeType ChangeType = EPCGChangeType::None);

	/** Returns true if there are any tasks for this graph currently scheduled or executing. */
	bool IsGraphCurrentlyExecuting(UPCGGraph* Graph);

	/** Returns true if any task is scheduled or executing for any graph */
	bool IsAnyGraphCurrentlyExecuting() const;

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

	TSet<UPCGComponent*> GetAllRegisteredPartitionedComponents() const { return ActorAndComponentMapping.GetAllRegisteredPartitionedComponents(); }
	TSet<UPCGComponent*> GetAllRegisteredComponents() const { return ActorAndComponentMapping.GetAllRegisteredComponents(); }

	/** Flushes the graph cache completely, use only for debugging */
	void FlushCache();

	/** Call the InFunc function to all local component registered to the original component. Thread safe*/
	void ForAllRegisteredLocalComponents(UPCGComponent* InOriginalComponent, const TFunctionRef<void(UPCGComponent*)>& InFunc) const;

	/** Call the InFunc function to all local component registered to the original component within some bounds. Thread safe*/
	void ForAllRegisteredIntersectingLocalComponents(UPCGComponent* InOriginalComponent, const FBoxCenterAndExtent& InBounds, const TFunctionRef<void(UPCGComponent*)>& InFunc) const;

	/** Get all components in specified bounds. */
	void ForAllIntersectingPartitionedComponents(const FBoxCenterAndExtent& InBounds, TFunctionRef<void(UPCGComponent*)> InFunc) const { return ActorAndComponentMapping.ForAllIntersectingPartitionedComponents(InBounds, std::move(InFunc)); }

	/** Gather all the PCG components within some bounds. */
	TArray<UPCGComponent*> GetAllIntersectingComponents(const FBoxCenterAndExtent& InBounds) const { return ActorAndComponentMapping.GetAllIntersectingComponents(InBounds); }

	/** Traverses the hierarchy associated with the given component and calls InFunc for each overlapping component. */
	void ForAllOverlappingComponentsInHierarchy(UPCGComponent* InComponent, const TFunctionRef<void(UPCGComponent*)>& InFunc) const;

	/**
	 * Call InFunc to all partition grid cells matching 'InGridSizes' and overlapping with 'InBounds'. 'InFunc' can schedule work or execute immediately.
	 * 'InGridSizes' should be sorted in descending order. If 'bCanCreateActor' is true, it will create the partition actor at that cell if necessary.
	 */
	FPCGTaskId ForAllOverlappingCells(const FBox& InBounds, const PCGHiGenGrid::FSizeArray& InGridSizes, bool bCanCreateActor, const TArray<FPCGTaskId>& Dependencies, TFunctionRef<FPCGTaskId(APCGPartitionActor*, const FBox&)> InFunc) const;

	/** Immediately cleanup the local components associated with an original component. */
	void CleanupLocalComponentsImmediate(UPCGComponent* InOriginalComponent, bool bRemoveComponents);

	/** Retrieves a local component using grid size and grid coordinates, returns nullptr if no such component found. */
	UPCGComponent* GetLocalComponent(uint32 GridSize, const FIntVector& CellCoords, const UPCGComponent* InOriginalComponent, bool bTransient = false) const;

	/** Retrieves a registered partition actor using grid size and grid coordinates, returns nullptr if no such partition actor is found. */
	APCGPartitionActor* GetRegisteredPCGPartitionActor(uint32 GridSize, const FIntVector& GridCoords, bool bRuntimeGenerated = false) const;

	/** Creates a new partition actor if one does not already exist with the same grid size, coords, and generation mode. */
	APCGPartitionActor* FindOrCreatePCGPartitionActor(const FGuid& Guid, uint32 GridSize, const FIntVector& GridCoords, bool bRuntimeGenerated, bool bCanCreateActor = true) const;

	/** True if graph cache debugging is enabled. */
	bool IsGraphCacheDebuggingEnabled() const;

	FPCGGenSourceManager* GetGenSourceManager() const;

#if WITH_EDITOR
public:
	/** Schedule refresh on the current or next frame */
	FPCGTaskId ScheduleRefresh(UPCGComponent* SourceComponent, bool bForceRefresh);

	/** Immediately dirties the partition actors in the given bounds */
	void DirtyGraph(UPCGComponent* Component, const FBox& InBounds, EPCGComponentDirtyFlag DirtyFlag);

	/** Delete serialized partition actors in the level. If 'bOnlyDeleteUnused' is true, only PAs with no graph instances will be deleted. */
	void DeleteSerializedPartitionActors(bool bOnlyDeleteUnused, bool bOnlyChildren = false);

	/** Propagate to the graph compiler graph changes */
	void NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType);

	/** Update the tracking on a given component. */
	void UpdateComponentTracking(UPCGComponent* InComponent, bool bShouldDirtyActors, const TArray<FPCGSelectionKey>* OptionalChangedKeys = nullptr) { ActorAndComponentMapping.UpdateTracking(InComponent, bShouldDirtyActors, OptionalChangedKeys); }

	/** Propagates transient state change from an original component to the relevant partition actors */
	void PropagateEditingModeToLocalComponents(UPCGComponent* InOriginalComponent, EPCGEditorDirtyMode EditingMode);

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
	FPCGGraphCompiler* GetGraphCompiler();

	/** Get the execution stack information for the given component. */
	bool GetStackContext(const UPCGComponent* InComponent, FPCGStackContext& OutStackContext);

	/** Returns how many times InElement is present in the cache. */
	uint32 GetGraphCacheEntryCount(IPCGElement* InElement) const;

	/** Get graph warnings and errors for all nodes. */
	const FPCGNodeVisualLogs& GetNodeVisualLogs() const { return NodeVisualLogs; }
	FPCGNodeVisualLogs& GetNodeVisualLogsMutable() { return NodeVisualLogs; }

	/** Notify that we exited the Landscape edit mode. */
	void NotifyLandscapeEditModeExited();

	/** Clear any data collected during execution, normally called prior to generating the component. */
	void ClearExecutionMetadata(const FPCGStack& BaseStack);
	void ClearExecutionMetadata(UPCGComponent* InComponent);

	/** Get a list of stacks that were executed during the last execution. */
	TArray<FPCGStack> GetExecutedStacks(const UPCGComponent* InComponent, const UPCGGraph* InSubgraph);
	void ClearExecutedStacks(FPCGStack BeginningWithStack);
	void ClearExecutedStacks(const UPCGGraph* InContainingGraph);

	FPCGOnComponentGenerationCompleteOrCancelled OnComponentGenerationCompleteOrCancelled;

private:
	enum class EOperation : uint32
	{
		Partition,
		Unpartition,
		Generate
	};

	void CreatePartitionActorsWithinBounds(const FBox& InBounds, const PCGHiGenGrid::FSizeArray& InGridSizes);

	FPCGNodeVisualLogs NodeVisualLogs;
#endif // WITH_EDITOR
	
private:
	APCGWorldActor* PCGWorldActor = nullptr;
	FPCGGraphExecutor* GraphExecutor = nullptr;
	FPCGRuntimeGenScheduler* RuntimeGenScheduler = nullptr;
	bool bHasTickedOnce = false;
	FPCGActorAndComponentMapping ActorAndComponentMapping;

	/** A record of stacks that were executed. Used to populate debugging tool UIs. */
	TArray<FPCGStack> ExecutedStacks;
	mutable FRWLock ExecutedStacksLock;

#if WITH_EDITOR
	FCriticalSection PCGWorldActorLock;
		
	using FConstructionScriptSourceComponents = TMap<FName, TObjectKey<UPCGComponent>>;
	TMap<TObjectKey<AActor>, FConstructionScriptSourceComponents> PerActorConstructionScriptSourceComponents;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGComponent.h"
#include "PCGVolume.h"
#endif
