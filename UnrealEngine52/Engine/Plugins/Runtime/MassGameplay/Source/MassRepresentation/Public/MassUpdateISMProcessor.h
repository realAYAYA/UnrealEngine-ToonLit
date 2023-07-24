// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"

#include "MassUpdateISMProcessor.generated.h"

class UMassRepresentationSubsystem;

struct FMassInstancedStaticMeshInfo;

UCLASS()
class MASSREPRESENTATION_API UMassUpdateISMProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassUpdateISMProcessor();

	static void UpdateISMTransform(int32 EntityId, FMassInstancedStaticMeshInfo& ISMInfo, const FTransform& Transform, const FTransform& PrevTransform, const float LODSignificance, const float PrevLODSignificance = -1.0f);

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	/**
	 * Execution method for this processor
	 * @param EntitySubsystem is the system to execute the lambdas on each entity chunk
	 * @param Context is the execution context to be passed when executing the lambdas */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};