// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PointLightImportTestFunctions.h"
#include "InterchangeTestFunction.h"
#include "SpotLightImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API USpotLightImportTestFunctions : public UPointLightImportTestFunctions
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the light inner cone angle is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightInnerConeAngle(ASpotLight* Light, float ExpectedLightInnerConeAngle);

	/** Check whether the light outer cone angle is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightOuterConeAngle(ASpotLight* Light, float ExpectedLightOuterConeAngle);
};
