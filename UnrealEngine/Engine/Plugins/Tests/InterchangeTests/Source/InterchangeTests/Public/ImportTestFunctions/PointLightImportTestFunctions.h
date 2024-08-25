// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LightImportTestFunctions.h"
#include "InterchangeTestFunction.h"
#include "PointLightImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UPointLightImportTestFunctions : public ULightImportTestFunctions
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the light falloff exponent is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightFalloffExponent(APointLight* Light, float ExpectedLightFalloff);
};
