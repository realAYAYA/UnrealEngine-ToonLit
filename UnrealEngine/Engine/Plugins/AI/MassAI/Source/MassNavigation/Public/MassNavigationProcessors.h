// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "MassNavigationProcessors.generated.h"

class UMassNavigationSubsystem;

/**
 * Updates Off-LOD entities position to move targets position.
 */
UCLASS()
class MASSNAVIGATION_API UMassOffLODNavigationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassOffLODNavigationProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery_Conditional;
};

/**
 * Updates entities height to move targets position smoothly.
 * Does not update Off-LOD entities.
 */
UCLASS()
class MASSNAVIGATION_API UMassNavigationSmoothHeightProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassNavigationSmoothHeightProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

/**
 * Initializes the move target's location to the agents initial position.
 */
UCLASS()
class MASSNAVIGATION_API UMassMoveTargetFragmentInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UMassMoveTargetFragmentInitializer();
	
protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery InitializerQuery;
};

/** Processor to update obstacle grid */
UCLASS()
class MASSNAVIGATION_API UMassNavigationObstacleGridProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassNavigationObstacleGridProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery AddToGridEntityQuery;
	FMassEntityQuery UpdateGridEntityQuery;
	FMassEntityQuery RemoveFromGridEntityQuery;
};

/** Deinitializer processor to remove avoidance obstacles from the avoidance obstacle grid */
UCLASS()
class MASSNAVIGATION_API UMassNavigationObstacleRemoverProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UMassNavigationObstacleRemoverProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
