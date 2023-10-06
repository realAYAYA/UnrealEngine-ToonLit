// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassObserverProcessor.h"

#include "MassReplicationGridProcessor.generated.h"

class UMassReplicationSubsystem;

/** Processor to update entity in the replication grid used to fetch entities close to clients */
UCLASS()
class MASSREPLICATION_API UMassReplicationGridProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassReplicationGridProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery AddToGridEntityQuery;
	FMassEntityQuery UpdateGridEntityQuery;
	FMassEntityQuery RemoveFromGridEntityQuery;
};

/** Deinitializer processor to remove entity from the replication grid */
UCLASS()
class MASSREPLICATION_API UMassReplicationGridRemoverProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UMassReplicationGridRemoverProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};