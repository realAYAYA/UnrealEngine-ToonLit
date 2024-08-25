// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/PointLightImportTestFunctions.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"

UClass* UPointLightImportTestFunctions::GetAssociatedAssetType() const
{
	return APointLight::StaticClass();
}

FInterchangeTestFunctionResult UPointLightImportTestFunctions::CheckLightFalloffExponent(APointLight* Light, float ExpectedLightFalloff)
{
	FInterchangeTestFunctionResult Result;

	if(Light->PointLightComponent)
	{
		if(!FMath::IsNearlyEqual(Light->PointLightComponent->LightFalloffExponent, ExpectedLightFalloff))
		{
			Result.AddError(FString::Printf(TEXT("Expected %g light falloff exponent, imported %g."), Light->PointLightComponent->LightFalloffExponent, ExpectedLightFalloff));
		}
	}
	else
	{
		Result.AddError(FString::Printf(TEXT("LightComponent is null for %s."),	*Light->GetName()));
	}

	return Result;
}