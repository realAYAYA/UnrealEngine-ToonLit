// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "InstancedActorsInitializerProcessor.generated.h"


class UInstancedActorsData;

USTRUCT()
struct FInstancedActorsMassSpawnData
{
	GENERATED_BODY()

	TWeakObjectPtr<UInstancedActorsData> InstanceData;
};

UCLASS()
class UInstancedActorsInitializerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UInstancedActorsInitializerProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
