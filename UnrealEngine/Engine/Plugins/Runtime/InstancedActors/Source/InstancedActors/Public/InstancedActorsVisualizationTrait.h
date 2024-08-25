// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStationaryDistanceVisualizationTrait.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "InstancedActorsVisualizationTrait.generated.h"


class UInstancedActorsData;

/** 
 * Subclass of UMassStationaryVisualizationTrait which forces required settings for instanced actor entities and overrides
 * FMassRepresentationFragment.StaticMeshDescHandle to use a custom registered Visulization which reuses InstanceData's 
 * ISMComponents via UMassRepresentationSubsystem::AddVisualDescWithISMComponent.
 */
UCLASS(MinimalAPI)
class UInstancedActorsVisualizationTrait : public UMassStationaryDistanceVisualizationTrait
{
	GENERATED_BODY()

public:

	INSTANCEDACTORS_API UInstancedActorsVisualizationTrait(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	INSTANCEDACTORS_API virtual void InitializeFromInstanceData(UInstancedActorsData& InInstanceData);

	//~ Begin UInstancedActorsVisualizationTrait Overrides
	INSTANCEDACTORS_API virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	//~ End UInstancedActorsVisualizationTrait Overrides

protected:
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UInstancedActorsData> InstanceData;
};
