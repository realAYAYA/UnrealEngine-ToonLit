// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorLabelProcessors.generated.h"

/**
 * Takes the label set on an actor and copies it to the Data Storage if they differ.
 */
UCLASS()
class UTypedElementActorLabelToColumnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementActorLabelToColumnProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};



/**
 * Takes the label stored in the Data Storage and copies it to the actor's label if the FTypedElementSyncBackToWorldTag
 * has been set and the labels differ.
 */
UCLASS()
class UTypedElementLabelColumnToActorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementLabelColumnToActorProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};