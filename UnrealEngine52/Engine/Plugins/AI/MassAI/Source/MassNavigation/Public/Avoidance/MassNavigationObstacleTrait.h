// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassNavigationObstacleTrait.generated.h"

UCLASS(meta = (DisplayName = "Navigation Obstacle"))
class MASSNAVIGATION_API UMassNavigationObstacleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

};