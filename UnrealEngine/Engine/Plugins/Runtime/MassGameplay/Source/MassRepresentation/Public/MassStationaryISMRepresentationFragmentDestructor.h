// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassObserverProcessor.h"
#include "MassStationaryISMRepresentationFragmentDestructor.generated.h"


/** 
 * This class is responsible for cleaning up ISM instances visualizing stationary entities
 */
UCLASS()
class MASSREPRESENTATION_API UMassStationaryISMRepresentationFragmentDestructor : public UMassObserverProcessor
{
	GENERATED_BODY()
public:
	UMassStationaryISMRepresentationFragmentDestructor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
