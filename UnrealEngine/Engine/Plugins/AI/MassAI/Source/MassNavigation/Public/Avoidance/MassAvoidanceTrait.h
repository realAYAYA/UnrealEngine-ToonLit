// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassAvoidanceFragments.h"
#include "MassAvoidanceTrait.generated.h"

UCLASS(meta = (DisplayName = "Avoidance"))
class MASSNAVIGATION_API UMassObstacleAvoidanceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category="")
	FMassMovingAvoidanceParameters MovingParameters;
	
	UPROPERTY(EditAnywhere, Category="")
	FMassStandingAvoidanceParameters StandingParameters;
};
