// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassRepresentationTypes.h"
#include "InstancedActorsVisualizationSwitcherProcessor.generated.h"


struct FMassRepresentationFragment;

/**
 * Executes on entities with FInstancedActorsMeshSwitchFragment's, processing them as `pending requests` to switch to
 * the specified NewStaticMeshDescHandle, then removing the fragments once complete
 */
UCLASS(MinimalAPI)
class UInstancedActorsVisualizationSwitcherProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UInstancedActorsVisualizationSwitcherProcessor();

	// Switch EntityHandle to NewStaticMeshDescHandle by removing any current / previous ISMC instances for the current StaticMeshDescIHandle
	// and setting RepresentationFragment.PrevRepresentation = EMassRepresentationType::None to let the subsequent
	// UMassStationaryISMSwitcherProcessor see that a new isntance needs to be added for the now set NewStaticMeshDescHandle
	INSTANCEDACTORS_API static void SwitchEntityMeshDesc(FMassInstancedStaticMeshInfoArrayView& ISMInfosView, FMassRepresentationFragment& RepresentationFragment, FMassEntityHandle EntityHandle, FStaticMeshInstanceVisualizationDescHandle NewStaticMeshDescHandle);

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
