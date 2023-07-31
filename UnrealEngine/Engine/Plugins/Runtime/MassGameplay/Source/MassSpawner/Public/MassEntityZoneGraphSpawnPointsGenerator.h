// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "ZoneGraphTypes.h"

#include "MassEntityZoneGraphSpawnPointsGenerator.generated.h"

class AZoneGraphData;

/**
 * Describes the SpawnPoints Generator when we want to spawn directly on Zone Graph
 */
UCLASS(BlueprintType, meta=(DisplayName="ZoneGraph SpawnPoints Generator"))
class MASSSPAWNER_API UMassEntityZoneGraphSpawnPointsGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()

public:
	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

protected:
	void GeneratePointsForZoneGraphData(const ::AZoneGraphData& ZoneGraphData, TArray<FVector>& Locations, const FRandomStream& RandomStream) const;

	/** Tags to filter which lane to use to generate points on */
	UPROPERTY(EditAnywhere, Category = "ZoneGraph Generator Config")
	FZoneGraphTagFilter TagFilter;

	/** Minimum gap for spawning entities on a given lanes */
	UPROPERTY(EditAnywhere, Category = "ZoneGraph Generator Config")
	float MinGap = 100;

	/** Minimum gap for spawning entities on a given lanes */
	UPROPERTY(EditAnywhere, Category = "ZoneGraph Generator Config")
	float MaxGap = 300;
};