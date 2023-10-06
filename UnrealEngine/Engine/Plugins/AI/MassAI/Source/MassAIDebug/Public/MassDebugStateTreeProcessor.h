// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassDebugStateTreeProcessor.generated.h"

struct FMassEntityManager;
struct FMassEntityQuery;
struct FMassExecutionContext;

UCLASS()
class MASSAIDEBUG_API UMassDebugStateTreeProcessor : public UMassProcessor
{
	GENERATED_BODY()

protected:
	UMassDebugStateTreeProcessor();

	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
