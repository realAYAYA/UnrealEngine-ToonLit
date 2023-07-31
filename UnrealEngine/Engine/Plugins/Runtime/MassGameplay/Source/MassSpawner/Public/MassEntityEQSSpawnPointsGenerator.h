// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "MassEntityEQSSpawnPointsGenerator.generated.h"

/**
 * Describes the SpawnPoints Generator when we want to leverage the points given by an EQS Query
 */
UCLASS(BlueprintType, meta=(DisplayName="EQS SpawnPoints Generator"))
class MASSSPAWNER_API UMassEntityEQSSpawnPointsGenerator : public UMassEntitySpawnDataGeneratorBase
{	
	GENERATED_BODY()

public:
	UMassEntityEQSSpawnPointsGenerator();
	
	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

protected:
	void OnEQSQueryFinished(TSharedPtr<FEnvQueryResult> EQSResult, TArray<FMassEntitySpawnDataGeneratorResult> Results, FFinishedGeneratingSpawnDataSignature FinishedGeneratingSpawnPointsDelegate) const;

	UPROPERTY(Category = "Query", EditAnywhere)
	FEQSParametrizedQueryExecutionRequest EQSRequest;
};
