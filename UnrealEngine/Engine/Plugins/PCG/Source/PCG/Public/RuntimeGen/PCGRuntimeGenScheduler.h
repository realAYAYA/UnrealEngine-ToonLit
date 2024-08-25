// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "UObject/WeakObjectPtr.h"

class APCGPartitionActor;
class APCGWorldActor;
class FPCGActorAndComponentMapping;
class FPCGGenSourceManager;
class IPCGGenSourceBase;
class UPCGComponent;
class UPCGSubsystem;
class UWorld;

/**
 * The Runtime Generation Scheduler system handles the scheduling of PCG Components marked as GenerateAtRuntime.
 * It searches the level for Partitioned and Non-Partitioned components in range of the currently active
 * UPCGGenSources in the level, and schedules them efficiently based on their UPCGSchedulingPolicy, creating 
 * APCGPartitionActors as necessary to support hierarchical generation.
 *
 * APCGPartitionActors can be created/destroyed on demand or provided by a dynamically growing pool of actors. If
 * enabled, the pool will double in capacity anytime the number of available PAs reaches zero.
 * 
 * Components and PartitionActors created by the Runtime Generation Scheduler should be managed exclusively by the
 * runtime gen scheduling system.
 */
class FPCGRuntimeGenScheduler
{
	friend class UPCGSubsystem;

public:
	FPCGRuntimeGenScheduler(UWorld* InWorld, FPCGActorAndComponentMapping* InActorAndComponentMapping);
	~FPCGRuntimeGenScheduler();

	FPCGRuntimeGenScheduler(const FPCGRuntimeGenScheduler&) = delete;
	FPCGRuntimeGenScheduler(FPCGRuntimeGenScheduler&& other) = delete;
	FPCGRuntimeGenScheduler& operator=(const FPCGRuntimeGenScheduler& other) = delete;
	FPCGRuntimeGenScheduler& operator=(FPCGRuntimeGenScheduler&& other) = delete; 

	void Tick(APCGWorldActor* InPCGWorldActor);

	void OnOriginalComponentRegistered(UPCGComponent* InOriginalComponent);
	void OnOriginalComponentUnregistered(UPCGComponent* InOriginalComponent);

protected:
	struct FGridGenerationKey : TTuple<uint32, FIntVector, UPCGComponent*>
	{
		FGridGenerationKey(uint32 InGridSize, const FIntVector& InGridCoords, UPCGComponent* InOriginalComponent)
			: TTuple<uint32, FIntVector, UPCGComponent*>(InGridSize, InGridCoords, InOriginalComponent) {}

		uint32 GetGridSize() const { return Get<0>(); }
		FIntVector GetGridCoords() const { return Get<1>(); }
		UPCGComponent* GetOriginalComponent() const { return Get<2>(); }
	};

	/** Returns true if the scheduler should tick this frame. */
	bool ShouldTick();

	/** Queue nearby components for generation. */
	void TickQueueComponentsForGeneration(
		const TSet<IPCGGenSourceBase*>& GenSources,
		APCGWorldActor* InPCGWorldActor,
		TMap<FGridGenerationKey, double>& OutComponentsToGenerate);

	/** Perform immediate cleanup on components that become out of range. */
	void TickCleanup(const TSet<IPCGGenSourceBase*>& GenSources, const APCGWorldActor* InPCGWorldActor);

	/** Schedule generation on components in priority order. */
	void TickScheduleGeneration(TMap<FGridGenerationKey, double>& ComponentsToGenerate);

	/** Detects changes in RuntimeGen CVars to keep the PA pool in a valid state. */
	void TickCVars(const APCGWorldActor* InPCGWorldActor);

	/** Cleanup all local components in the GeneratedComponents set. */
	void CleanupLocalComponents(const APCGWorldActor* InPCGWorldActor);

	/** Cleanup a component and remove it from the GeneratedComponents set. */
	void CleanupComponent(const FGridGenerationKey& GenerationKey, UPCGComponent* GeneratedComponent);

	/** Remove components from the GeneratedComponents set that have been marked for delayed refresh. Fully cleanup any that would be leaked otherwise. */
	void CleanupDelayedRefreshComponents();

	/** Refresh a generated component. bRemovePartitionActors will also perform a full cleanup of PAs and local components. */
	void RefreshComponent(UPCGComponent* InComponent, bool bRemovePartitionActors = false);
	
	/** Grabs an empty RuntimeGen PA from the PartitionActorPool and initializes it at the given GridSize and GridCoords. If no PAs are available in the pool,
	* the pool capacity will double and new PAs will be created.
	*/
	APCGPartitionActor* GetPartitionActorFromPool(uint32 GridSize, const FIntVector& GridCoords);

	/** Adds Count new RuntimeGen PAs to the Runtime PA pool. */
	void AddPartitionActorPoolCount(int32 Count);

	/** Destroy all pooled partition actors and rebuild with the NewPoolSize. */
	void ResetPartitionActorPoolToSize(uint32 NewPoolSize);

	/** Create grid guids for the given component (if necessary). Only succeeds on partitioned original components. */
	void CreateGridGuidsForComponent(UPCGComponent* InComponent);

private:
	/** Tracks the generated components managed by the RuntimeGenScheduler. For local components, this generation key will hold the original component.
	* For non-partitioned components, the generation key should have unbounded grid size and (0, 0, 0) grid coordinates.
	*/
	TSet<FGridGenerationKey> GeneratedComponents;

	/** Tracks the components which should be removed from the GeneratedComponents set on the next tick. This helps us defer removal in case we get multiple
	* refreshes in a single tick. For example, a shallow refresh followed by a deep refresh would require the generated components to persist, otherwise we
	* will leak Partition Actors.
	*/
	TSet<FGridGenerationKey> GeneratedComponentsToRemove;

	/** Pool of RuntimeGen PartitionActors used for hierarchical generation. */
	TArray<APCGPartitionActor*> PartitionActorPool;

	/** PartitionActorPoolSize represents the current maximum capacity of the PartitionActorPool. */
	int32 PartitionActorPoolSize = 0;

	/** Used to track the number of frames until we can schedule another generation. */
	uint32 ScheduleFrameCounter = 0;

	FPCGActorAndComponentMapping* ActorAndComponentMapping = nullptr;
	FPCGGenSourceManager* GenSourceManager = nullptr;
	UPCGSubsystem* Subsystem = nullptr;
	UWorld* World = nullptr;

	bool bPoolingWasEnabledLastFrame = true;
	uint32 BasePoolSizeLastFrame = 0;

	/** Track the existence of runtime gen components to avoid unnecessary computation when there is no work to do. */
	bool bAnyRuntimeGenComponentsExist = false;
	bool bAnyRuntimeGenComponentsExistDirty = false;

	/** Setting up a PA calls APCGPartitionActor::AddGraphInstance which later calls RefreshComponent, which can create
	* an infinite refresh loop. To break this loop we write the OC pointer to this variable, and if Refresh gets called for
	* this OC we early out. Basically we don't respond to refresh calls for a component we are midway through setting up.
	*/
	const UPCGComponent* OriginalComponentBeingGenerated = nullptr;
};
