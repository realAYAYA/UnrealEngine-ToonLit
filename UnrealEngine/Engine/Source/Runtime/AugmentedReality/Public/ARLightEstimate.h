// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ARLightEstimate.generated.h"

struct FFrame;


UCLASS(BlueprintType, Experimental, Category="AR AugmentedReality|Light Estimation")
class UARLightEstimate : public UObject
{
	GENERATED_BODY()
};

UCLASS(BlueprintType, Category = "AR AugmentedReality|Light Estimation")
class AUGMENTEDREALITY_API UARBasicLightEstimate : public UARLightEstimate
{
	GENERATED_BODY()
	
public:
	void SetLightEstimate(float InAmbientIntensityLumens, float InColorTemperatureKelvin);
	
	void SetLightEstimate(FVector InRGBScaleFactor, float InPixelIntensity);

	void SetLightEstimate(float InColorTemperatureKelvin, FLinearColor InAmbientColor);

	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Light Estimation")
	float GetAmbientIntensityLumens() const;
	
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Light Estimation")
	float GetAmbientColorTemperatureKelvin() const;
	
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Light Estimation")
	FLinearColor GetAmbientColor() const;
	
private:
	UPROPERTY()
	float AmbientIntensityLumens;
	
	UPROPERTY()
	float AmbientColorTemperatureKelvin;
	
	UPROPERTY()
	FLinearColor AmbientColor;
};
