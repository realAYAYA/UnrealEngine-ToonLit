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

	static void UpdateISMTransform(FMassEntityHandle EntityHandle, FMassInstancedStaticMeshInfo& ISMInfo, const FTransform& Transform, const FTransform& PrevTransform, const float LODSignificance, const float PrevLODSignificance = -1.0f);

	UE_DEPRECATED(5.4, "Deprecated in favor of new version taking FMassEntityHandle parameter instead of int32 to identify the entity. This deprecated function is now defunct.")
	static void UpdateISMTransform(int32 EntityId, FMassInstancedStaticMeshInfo& ISMInfo, const FTransform& Transform, const FTransform& PrevTransform, const float LODSignificance, const float PrevLODSignificance = -1.0f) {}


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