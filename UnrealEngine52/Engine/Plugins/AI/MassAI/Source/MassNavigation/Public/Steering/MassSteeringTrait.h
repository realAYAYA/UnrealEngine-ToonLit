// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "Steering/MassSteeringFragments.h"
#include "MassSteeringTrait.generated.h"


UCLASS(meta = (DisplayName = "Steering"))
class MASSNAVIGATION_API UMassSteeringTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category="Steering", EditAnywhere, meta=(EditInline))
	FMassMovingSteeringParameters MovingSteering;

	UPROPERTY(Category="Steering", EditAnywhere, meta=(EditInline))
	FMassStandingSteeringParameters StandingSteering;
};
