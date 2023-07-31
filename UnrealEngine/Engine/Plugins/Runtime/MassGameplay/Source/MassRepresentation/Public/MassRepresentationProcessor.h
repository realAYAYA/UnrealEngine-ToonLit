// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassObserverProcessor.h"

#include "MassRepresentationProcessor.generated.h"

class UMassRepresentationSubsystem;
class UMassActorSubsystem;
struct FMassActorFragment;

UCLASS()
class MASSREPRESENTATION_API UMassRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassRepresentationProcessor();

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	/** 
	 * Execution method for this processor 
	 * @param EntitySubsystem is the system to execute the lambdas on each entity chunk
	 * @param Context is the execution context to be passed when executing the lambdas
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 * Release the actor to the subsystem, will only release it the actor or spawn request matches the template actor
	 * @param RepresentationSubsystem to use to release the actor or cancel the spawning
	 * @param MassAgent is the handle to the associated mass agent
	 * @param ActorInfo is the fragment where we are going to store the actor pointer
	 * @param TemplateActorIndex is the index of the type to release
	 * @param SpawnRequestHandle (in/out) In: previously requested spawn to cancel if any
	 * @param CommandBuffer to queue up anything that is thread sensitive
	 * @param bCancelSpawningOnly tell to only cancel the existing spawning request and to not release the associated actor it any.
	 * @return if the actor was release or the spawning was canceled.
	 */
	bool ReleaseActorOrCancelSpawning(UMassRepresentationSubsystem& RepresentationSubsystem, UMassActorSubsystem* MassActorSubsystem, const FMassEntityHandle MassAgent, FMassActorFragment& ActorInfo, const int16 TemplateActorIndex, FMassActorSpawnRequestHandle& SpawnRequestHandle, FMassCommandBuffer& CommandBuffer, const bool bCancelSpawningOnly = false);

	/*
	 * Update representation type for each entity, must be called within a ForEachEntityChunk
	 * @param Context of the execution from the entity sub system
	 */
	void UpdateRepresentation(FMassExecutionContext& Context);

	FMassEntityQuery EntityQuery;
};

UCLASS()
class MASSREPRESENTATION_API UMassVisualizationProcessor : public UMassRepresentationProcessor
{
	GENERATED_BODY()

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	/**
	 * Execution method for this processor
	 * @param EntitySubsystem is the system to execute the lambdas on each entity chunk
	 * @param Context is the execution context to be passed when executing the lambdas
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 * Updates chunk visibility info for later chunk logic optimization
	 * @param Context of the execution from the entity sub system
	 * @return The visualization chunk fragment
	 */
	FMassVisualizationChunkFragment& UpdateChunkVisibility(FMassExecutionContext& Context) const;

	/**
	 * Updates entity visibility tag for later chunk logic optimization
	 * @param Entity of the entity to update visibility on
	 * @param Representation fragment containing the current and previous visual state
	 * @param RepresentationLOD fragment containing the visibility information
	 * @param ChunkData is the visualization chunk fragment
	 * @param CommandBuffer to queue up anything that is thread sensitive
	 */
	static void UpdateEntityVisibility(const FMassEntityHandle Entity, const FMassRepresentationFragment& Representation, const FMassRepresentationLODFragment& RepresentationLOD, FMassVisualizationChunkFragment& ChunkData, FMassCommandBuffer& CommandBuffer);

	/**
	 * Update representation and visibility for each entity, must be called within a ForEachEntityChunk
	 * @param Context of the execution from the entity sub system
	 */
	void UpdateVisualization(FMassExecutionContext& Context);
};


UCLASS()
class MASSREPRESENTATION_API UMassRepresentationFragmentDestructor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UMassRepresentationFragmentDestructor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};