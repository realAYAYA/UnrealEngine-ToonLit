// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "MassZoneGraphNavigationProcessors.generated.h"

class UMassSignalSubsystem;


/**
 * Processor for initializing nearest location on ZoneGraph.
 */
UCLASS()
class MASSZONEGRAPHNAVIGATION_API UMassZoneGraphLocationInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()
	
public:
	UMassZoneGraphLocationInitializer();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/** 
 * Processor for updating move target on ZoneGraph path.
 */
UCLASS()
class MASSZONEGRAPHNAVIGATION_API UMassZoneGraphPathFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UMassZoneGraphPathFollowProcessor();
	
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Conditional;

	UPROPERTY(Transient)
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem = nullptr;
};

/** ZoneGraph lane cache boundary processor */
// @todo MassMovement: Make this signal based.
UCLASS()
class MASSZONEGRAPHNAVIGATION_API UMassZoneGraphLaneCacheBoundaryProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassZoneGraphLaneCacheBoundaryProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	TWeakObjectPtr<UWorld> WeakWorld;
	FMassEntityQuery EntityQuery;
};
