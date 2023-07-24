// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassVisualizationLODProcessor.h"
#include "MassLODCollectorProcessor.h"
#include "MassCrowdVisualizationLODProcessor.generated.h"

/*
 * Created a crowd version for parallelization of the crowd with the traffic
 */
UCLASS(meta=(DisplayName="Crowd visualization LOD"))
class MASSCROWD_API UMassCrowdVisualizationLODProcessor : public UMassVisualizationLODProcessor
{
	GENERATED_BODY()
public:
	UMassCrowdVisualizationLODProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
};

/*
 * Created a crowd version for parallelization of the crowd with the traffic
 */
UCLASS(meta = (DisplayName = "Crowd LOD Collection "))
class MASSCROWD_API UMassCrowdLODCollectorProcessor : public UMassLODCollectorProcessor
{
	GENERATED_BODY()

	UMassCrowdLODCollectorProcessor();

protected:
	virtual void ConfigureQueries() override;
};

