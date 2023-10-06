// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassStationaryVisualizationTrait.h"
#include "MassLWIVisualizationTrait.generated.h"


UCLASS()
class MASSLWI_API UMassLWIVisualizationTrait : public UMassStationaryVisualizationTrait
{
	GENERATED_BODY()
public:
	UMassLWIVisualizationTrait(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
