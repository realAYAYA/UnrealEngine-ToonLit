// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "TickableEditorObject.h"

#include "EditorGeometryGenerationSubsystem.generated.h"

class AGeneratedDynamicMeshActor;


/**
 * UEditorGeometryGenerationSubsystem manages recomputation of "generated" mesh actors, eg
 * to provide procedural mesh generation in-Editor. Generally such procedural mesh generation
 * is expensive, and if many objects need to be generated, the regeneration needs to be 
 * managed at a higher level to ensure that the Editor remains responsive/interactive.
 * 
 * AGeneratedDynamicMeshActors register themselves with this Subsystem, and
 * allow the Subsystem to tell them when they should regenerate themselves (if necessary).
 * The current behavior is to run all pending generations on a Tick, however in future
 * this regeneration will be more carefully managed via throttling / timeslicing / etc.
 * 
 */
UCLASS()
class GEOMETRYSCRIPTINGEDITOR_API UEditorGeometryGenerationSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	UPROPERTY()
	TObjectPtr<UEditorGeometryGenerationManager> GenerationManager;


	//
	// Static functions that simplify registration of an Actor w/ the Subsystem
	//
public:
	static bool RegisterGeneratedMeshActor(AGeneratedDynamicMeshActor* Actor);
	static void UnregisterGeneratedMeshActor(AGeneratedDynamicMeshActor* Actor);


protected:
	// callback connected to engine/editor shutdown events to set bIsShuttingDown, which disables the subsystem static functions above
	virtual void OnShutdown();

private:
	static bool bIsShuttingDown;
};




/**
 * UEditorGeometryGenerationManager is a class used by UEditorGeometryGenerationSubsystem to
 * store registrations and provide a Tick()
 */
UCLASS()
class GEOMETRYSCRIPTINGEDITOR_API UEditorGeometryGenerationManager : public UObject, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	virtual void Shutdown();

	virtual void RegisterGeneratedMeshActor(AGeneratedDynamicMeshActor* Actor);
	virtual void UnregisterGeneratedMeshActor(AGeneratedDynamicMeshActor* Actor);

public:

	//~ Begin FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject interface

protected:
	TSet<AGeneratedDynamicMeshActor*> ActiveGeneratedActors;
};
