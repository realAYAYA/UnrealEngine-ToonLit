// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "InstancedActorsTypes.h"
#include "SharedStruct.h"
#include "InstancedActorsStationaryLODBatchProcessor.generated.h"


UCLASS()
class UInstancedActorsStationaryLODBatchProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	UInstancedActorsStationaryLODBatchProcessor();

protected:
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const override { return false; }
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery LODChangingEntityQuery;
	FMassEntityQuery DirtyVisualizationEntityQuery;

	UPROPERTY(EditDefaultsOnly, Category="Mass", config)
	double DelayPerBulkLOD[(int)EInstancedActorsBulkLOD::MAX];

	struct FNextTickSharedFragment
	{
		FSharedStruct SharedStruct;
		double NextTickTime = 0;

		bool operator<(const FNextTickSharedFragment& Other) const
		{
			return NextTickTime < Other.NextTickTime;
		}
	};

	/** The container storing a sorted queue of FSharedStruct instances, ordered by the NextTickTime */
	TArray<FNextTickSharedFragment> SortedSharedFragments;
};
