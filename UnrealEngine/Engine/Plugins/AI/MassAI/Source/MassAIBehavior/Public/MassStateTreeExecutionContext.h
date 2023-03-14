// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "StateTreeExecutionContext.h"
//#include "MassStateTreeExecutionContext.generated.h"

struct FMassExecutionContext;
struct FMassEntityManager;
class UMassSignalSubsystem;

/**
 * Extends FStateTreeExecutionContext to provide additional data to Evaluators and Tasks related to MassSimulation
 */
struct MASSAIBEHAVIOR_API FMassStateTreeExecutionContext : public FStateTreeExecutionContext 
{
public:
	// @todo: refactor subsystems out of the exec context.
	FMassStateTreeExecutionContext(UObject& InOwner, const UStateTree& InStateTree, FStateTreeInstanceData& InInstanceData, FMassEntityManager& InEntityManager, UMassSignalSubsystem& InSignalSubsystem, FMassExecutionContext& InContext);

	FMassEntityManager& GetEntityManager() const { check(EntityManager); return *EntityManager; }
	FMassExecutionContext& GetEntitySubsystemExecutionContext() const { return *EntitySubsystemExecutionContext; }

	FMassEntityHandle GetEntity() const { return Entity; }
	void SetEntity(const FMassEntityHandle InEntity) { Entity = InEntity; }

protected:

	/** Prefix that will be used by STATETREE_LOG and STATETREE_CLOG, using Entity description. */
	virtual FString GetInstanceDescription() const override { return FString::Printf(TEXT("Entity [%s]: "), *Entity.DebugGetDescription()); }

	virtual void BeginGatedTransition(const FStateTreeExecutionState& Exec) override;

	FMassEntityManager* EntityManager;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	FMassExecutionContext* EntitySubsystemExecutionContext = nullptr;
	FMassEntityHandle Entity;
};
