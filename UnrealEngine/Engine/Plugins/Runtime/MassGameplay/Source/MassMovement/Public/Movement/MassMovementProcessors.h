// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassMovementProcessors.generated.h"

/**
 * Updates entities position based on force and velocity.
 * Not applied on Off-LOD entities.
 */
UCLASS()
class MASSMOVEMENT_API UMassApplyMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassApplyMovementProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
