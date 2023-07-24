// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassZoneGraphNavigationTrait.generated.h"


UCLASS(meta = (DisplayName = "ZoneGraph Navigation"))
class MASSZONEGRAPHNAVIGATION_API UMassZoneGraphNavigationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category="Movement", EditAnywhere)
	FMassZoneGraphNavigationParameters NavigationParameters;
};
