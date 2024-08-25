// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorImportTestFunctions.h"
#include "InterchangeTestFunction.h"
#include "LightImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API ULightImportTestFunctions : public UActorImportTestFunctions
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the light position is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightPosition(ALight* Light, const FVector& ExpectedLightPosition);

	/** Check whether the light direction is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightDirection(ALight* Light, const FVector& ExpectedLightDirection);

	/** Check whether the light intensity is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightIntensity(ALight* Light, float ExpectedLightIntensity);

	/** Check whether the light color is correct*/
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLightColor(ALight* Light, const FLinearColor& ExpectedLightColor);
};
