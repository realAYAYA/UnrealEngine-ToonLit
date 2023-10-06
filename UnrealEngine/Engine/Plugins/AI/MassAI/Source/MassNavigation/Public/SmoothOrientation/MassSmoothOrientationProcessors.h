// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassSmoothOrientationProcessors.generated.h"

/**
 * Updates agent's orientation based on current movement.
 */
UCLASS()
class MASSNAVIGATION_API UMassSmoothOrientationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassSmoothOrientationProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery HighResEntityQuery;
	FMassEntityQuery LowResEntityQuery_Conditional;
};
