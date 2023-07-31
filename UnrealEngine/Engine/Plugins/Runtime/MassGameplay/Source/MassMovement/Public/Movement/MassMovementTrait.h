// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassMovementFragments.h"
#include "MassMovementTrait.generated.h"

UCLASS(meta = (DisplayName = "Movement"))
class MASSMOVEMENT_API UMassMovementTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category="Movement", EditAnywhere)
	FMassMovementParameters Movement;
};
