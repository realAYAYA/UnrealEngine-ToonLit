// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SynthesisBlueprintUtilities.generated.h"

/** Synthesis Utilities Blueprint Function Library
*  A library of synthesis related functions for use in Blueprints
*/
UCLASS()
class SYNTHESIS_API USynthesisUtilitiesBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	// Returns the log frequency of the input value. Maps linear domain and range values to log output (good for linear slider controlling frequency)
	UFUNCTION(BlueprintCallable, Category = "Synthesis Utilities Library")
	static float GetLogFrequency(float InLinearValue, float InDomainMin, float InDomainMax, float InRangeMin, float InRangeMax);

	// Returns the log frequency of the input value. Maps linear domain and range values to log output (good for linear slider controlling frequency)
	UFUNCTION(BlueprintCallable, Category = "Synthesis Utilities Library")
	static float GetLinearFrequency(float InLogFrequencyValue, float InDomainMin, float InDomainMax, float InRangeMin, float InRangeMax);

};