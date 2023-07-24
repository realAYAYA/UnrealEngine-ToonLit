// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassAvoidanceProcessors.generated.h"

MASSNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidance, Warning, All);
MASSNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceVelocities, Warning, All);
MASSNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceAgents, Warning, All);
MASSNAVIGATION_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceObstacles, Warning, All);

class UMassNavigationSubsystem;

/** Experimental: move using cumulative forces to avoid close agents */
UCLASS()
class MASSNAVIGATION_API UMassMovingAvoidanceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassMovingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UWorld> World;
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};

/** Avoidance while standing. */
UCLASS()
class MASSNAVIGATION_API UMassStandingAvoidanceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassStandingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UWorld> World;
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};
