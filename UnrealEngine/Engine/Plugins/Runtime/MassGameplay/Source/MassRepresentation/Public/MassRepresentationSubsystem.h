// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MassCommonTypes.h"
#include "Misc/MTAccessDetector.h"
#include "MassRepresentationTypes.h"
#include "MassActorSpawnerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassRepresentationSubsystem.generated.h"

class UMassVisualizationComponent;
class AMassVisualizer;
class UHierarchicalInstancedStaticMeshComponent;
struct FStaticMeshInstanceVisualizationDesc;
struct FMassInstancedStaticMeshInfo;
struct FMassActorSpawnRequestHandle;
class UMassActorSpawnerSubsystem;
class UMassAgentComponent;
struct FMassEntityManager;
enum class EMassProcessingPhase : uint8;
class UWorldPartitionSubsystem;

/**
 * Subsystem responsible for all visual of mass agents, will handle actors spawning and static mesh instances
 */
UCLASS()
class MASSREPRESENTATION_API UMassRepresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 
	 * Get the index of the static mesh visual type, will add a new one if does not exist  
	 * @param Desc is the information for the static mesh that will be instantiated later via AddStaticMeshInstance()
	 * @return The index of the static mesh type 
	 */
	int16 FindOrAddStaticMeshDesc(const FStaticMeshInstanceVisualizationDesc& Desc);

	/** @todo: need to add a release API at some point for static mesh types */

	/** 
	 * @return the array of all the static mesh instance component information
	 */
	FMassInstancedStaticMeshInfoArrayView GetMutableInstancedStaticMeshInfos();

	/** Mark render state of the static mesh instances dirty */
	void DirtyStaticMeshInstances();

	/** 
	 * Store the template actor uniquely and return an index to it 
	 * @param ActorClass is a template actor class we will need to spawn for an agent 
	 * @return The index of the template actor type
	 */
	int16 FindOrAddTemplateActor(const TSubclassOf<AActor>& ActorClass);

	/** 
	 * Get or spawn an actor from the TemplateActorIndex
	 * @param MassAgent is the handle to the associated mass agent
	 * @param Transform where to create this actor
	 * @param TemplateActorIndex is the index of the type fetched with FindOrAddTemplateActor()
	 * @param SpawnRequestHandle [IN/OUT] IN: previously requested spawn OUT: newly requested spawn
	 * @param Priority of this spawn request in comparison with the others, lower value means higher priority (optional)
	 * @param ActorPreSpawnDelegate is an optional delegate called before the spawning of an actor
	 * @param ActorPostSpawnDelegate is an optional delegate called once the actor is spawned
	 * @return The spawned actor from the template actor type if ready
	 */
	AActor* GetOrSpawnActorFromTemplate(const FMassEntityHandle MassAgent, const FTransform& Transform, const int16 TemplateActorIndex, FMassActorSpawnRequestHandle& SpawnRequestHandle, float Priority = MAX_FLT,
		FMassActorPreSpawnDelegate ActorPreSpawnDelegate = FMassActorPreSpawnDelegate(), FMassActorPostSpawnDelegate ActorPostSpawnDelegate = FMassActorPostSpawnDelegate());

	/**
	 * Cancel spawning request that is matching the TemplateActorIndex
	 * @param MassAgent is the handle to the associated mass agent
	 * @param TemplateActorIndex is the template type of the actor to release in case it was successfully spawned
	 * @param SpawnRequestHandle [IN/OUT] previously requested spawn, gets invalidated as a result of this call.
	 * @return True if spawning request was canceled
	 */
	bool CancelSpawning(const FMassEntityHandle MassAgent, const int16 TemplateActorIndex, FMassActorSpawnRequestHandle & SpawnRequestHandle);

	/**
	 * Release an actor that is matching the TemplateActorIndex
	 * @param MassAgent is the handle to the associated mass agent
	 * @param TemplateActorIndex is the template type of the actor to release in case it was successfully spawned
	 * @param ActorToRelease is the actual actor to release if any
	 * @param bImmediate means it needs to be done immediately and not queue for later
	 * @return True if actor was released
	 */
	bool ReleaseTemplateActor(const FMassEntityHandle MassAgent, const int16 TemplateActorIndex, AActor* ActorToRelease, bool bImmediate);

	/**
	 * Release an actor or cancel its spawning if it is matching the TemplateActorIndex
	 * @param MassAgent is the handle to the associated mass agent
	 * @param TemplateActorIndex is the template type of the actor to release in case it was successfully spawned
	 * @param ActorToRelease is the actual actor to release if any
	 * @param SpawnRequestHandle [IN/OUT] previously requested spawn, gets invalidated as a result of this call.
	 * @return True if actor was released or spawning request was canceled
	 */
	bool ReleaseTemplateActorOrCancelSpawning(const FMassEntityHandle MassAgent, const int16 TemplateActorIndex, AActor* ActorToRelease, FMassActorSpawnRequestHandle& SpawnRequestHandle);


	/**
	 * Compare if an actor matches the registered template actor
	 * @param Actor to compare its class against the template
	 * @param TemplateActorIndex is the template type of the actor to compare against
	 * @return True if actor matches the template
	 */
	bool DoesActorMatchTemplate(const AActor& Actor, const int16 TemplateActorIndex) const;

	TSubclassOf<AActor> GetTemplateActorClass(const int16 TemplateActorIndex);

	bool IsCollisionLoaded(const FName TargetGrid, const FTransform& Transform) const;

	/**
	 * Release all references to static meshes and template actors
	 * Use with caution, all entities using this representation subsystem must be destroy otherwise they will point to invalid resources */
	 void ReleaseAllResources();

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem END

	/** Needed for batching the update of static mesh transform */
	void OnProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase) const;

	void OnMassAgentComponentEntityAssociated(const UMassAgentComponent& AgentComponent);
	void OnMassAgentComponentEntityDetaching(const UMassAgentComponent& AgentComponent);

	bool ReleaseTemplateActorInternal(const int16 TemplateActorIndex, AActor* ActorToRelease, bool bImmediate);
	bool CancelSpawningInternal(const int16 TemplateActorIndex, FMassActorSpawnRequestHandle& SpawnRequestHandle);

protected:

	/** The array of all the template actors */
	UPROPERTY(Transient)
	TArray<TSubclassOf<AActor>> TemplateActors;
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(TemplateActorsMTAccessDetector);

	/** The component that handles all the static mesh instances */
	UPROPERTY(Transient)
	TObjectPtr<UMassVisualizationComponent> VisualizationComponent;

	/** The actor owning the above visualization component */
	UPROPERTY(Transient)
	TObjectPtr<AMassVisualizer> Visualizer;

	UPROPERTY(Transient)
	TObjectPtr<UMassActorSpawnerSubsystem> ActorSpawnerSubsystem;

	TSharedPtr<FMassEntityManager> EntityManager;

	UPROPERTY(Transient)
	TObjectPtr<UWorldPartitionSubsystem> WorldPartitionSubsystem;

	/** The time to wait before retrying a to spawn actor that failed */
	float RetryMovedDistanceSq = 1000000.0f;

	/** The distance a failed spawned actor needs to move before we retry */
	float RetryTimeInterval = 10.0f;

	/** Keeping track of all the mass agent this subsystem is responsible for spawning actors */
	TMap<FMassEntityHandle, int32> HandledMassAgents;
};

