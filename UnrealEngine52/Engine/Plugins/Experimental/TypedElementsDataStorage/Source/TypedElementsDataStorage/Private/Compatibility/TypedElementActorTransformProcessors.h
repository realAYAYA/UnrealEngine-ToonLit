// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorTransformProcessors.generated.h"

/**
 * Checks actors that don't have a tranform column and adds one if an actor has been
 * assigned a transform.
 */
UCLASS()
class UTypedElementActorAddTransformColumnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementActorAddTransformColumnProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};


/**
 * Takes the transform set on an actor and copies it to the Data Storage or removes the 
 * transform column if there's not transform available anymore.
 */
UCLASS()
class UTypedElementActorLocalTransformToColumnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementActorLocalTransformToColumnProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};



/**
 * Takes the transform stored in the Data Storage and copies it to the actor's tranform if 
 * the FTypedElementSyncBackToWorldTag has been set.
 */
UCLASS()
class UTypedElementTransformColumnToActorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementTransformColumnToActorProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};