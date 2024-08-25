// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassDistanceVisualizationTrait.h"
#include "MassStationaryDistanceVisualizationTrait.generated.h"


UCLASS()
class MASSREPRESENTATION_API UMassStationaryDistanceVisualizationTrait : public UMassDistanceVisualizationTrait
{
	GENERATED_BODY()
public:
	UMassStationaryDistanceVisualizationTrait(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
