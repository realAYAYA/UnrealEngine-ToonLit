// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassLODCollector.h"

#include "MassLODCollectorProcessor.generated.h"

struct FMassGenericCollectorLogic : public FLODDefaultLogic
{
	enum
	{
		bDoVisibilityLogic = true,
	};
};

/*
 * LOD collector which combines collection of LOD information for both Viewer and Simulation LODing when possible.
 */
UCLASS(meta = (DisplayName = "LOD Collector"))
class MASSLOD_API UMassLODCollectorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassLODCollectorProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	template <bool bLocalViewersOnly>
	void CollectLODForChunk(FMassExecutionContext& Context);

	template <bool bLocalViewersOnly>
	void ExecuteInternal(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	TMassLODCollector<FMassGenericCollectorLogic> Collector;

	// Queries for visualization and simulation calculation
	/** All entities that are in visible range and are On LOD*/
	FMassEntityQuery EntityQuery_VisibleRangeAndOnLOD;
	/** All entities that are in visible range but are Off LOD */
	FMassEntityQuery EntityQuery_VisibleRangeOnly;
	/** All entities that are NOT in visible range but are On LOD */
	FMassEntityQuery EntityQuery_OnLODOnly;
	/** All entities that are Not in visible range and are at Off LOD */
	FMassEntityQuery EntityQuery_NotVisibleRangeAndOffLOD;
};
