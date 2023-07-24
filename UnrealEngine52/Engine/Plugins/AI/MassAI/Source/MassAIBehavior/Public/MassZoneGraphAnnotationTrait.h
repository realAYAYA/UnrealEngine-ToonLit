// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassZoneGraphAnnotationTrait.generated.h"

UCLASS(meta = (DisplayName = "ZoneGraph Annotation"))
class MASSAIBEHAVIOR_API UMassZoneGraphAnnotationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
