// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Distributions/DistributionFloatParameterBase.h"
#include "DistributionFloatParticleParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, editinlinenew, MinimalAPI)
class UDistributionFloatParticleParameter : public UDistributionFloatParameterBase
{
	GENERATED_UCLASS_BODY()


	//~ Begin UDistributionFloatParameterBase Interface
	ENGINE_API virtual bool GetParamValue(UObject* Data, FName ParamName, float& OutFloat) const override;
	//~ End UDistributionFloatParameterBase Interface
};

