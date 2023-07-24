// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassSmoothOrientationFragments.h"
#include "MassSmoothOrientationTrait.generated.h"

UCLASS(meta = (DisplayName = "Smooth Orientation"))
class MASSNAVIGATION_API UMassSmoothOrientationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category="")
	FMassSmoothOrientationParameters Orientation;
};
