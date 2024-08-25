// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/LightImportTestFunctions.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"

UClass* ULightImportTestFunctions::GetAssociatedAssetType() const
{
	return ALight::StaticClass();
}

FInterchangeTestFunctionResult ULightImportTestFunctions::CheckLightPosition(ALight* Light, const FVector& ExpectedLightPosition)
{
	FInterchangeTestFunctionResult Result;
	
	if(ULightComponent* LightComponent = Light->GetLightComponent())
	{
		if(const FVector LightPosition = LightComponent->GetLightPosition(); !ExpectedLightPosition.Equals(LightPosition))
		{
			Result.AddError(FString::Printf(TEXT("Expected (%g, %g, %g) light position, imported (%g, %g, %g)."),
											ExpectedLightPosition.X, ExpectedLightPosition.Y, ExpectedLightPosition.Z,
											LightPosition.X, LightPosition.Y, LightPosition.Z));
		}
	}
	else
	{
		Result.AddError(FString::Printf(TEXT("LightComponent is null for %s."), *Light->GetName()));
	}

	return Result;
}

FInterchangeTestFunctionResult ULightImportTestFunctions::CheckLightDirection(ALight* Light, const FVector& ExpectedLightDirection)
{
	FInterchangeTestFunctionResult Result;

	if(ULightComponent* LightComponent = Light->GetLightComponent())
	{
		if(const FVector LightDirection = LightComponent->GetDirection(); !ExpectedLightDirection.Equals(LightDirection))
		{
			Result.AddError(FString::Printf(TEXT("Expected (%g, %g, %g) light position, imported (%g, %g, %g)."),
											ExpectedLightDirection.X, ExpectedLightDirection.Y, ExpectedLightDirection.Z,
											LightDirection.X, LightDirection.Y, LightDirection.Z));
		}
	}
	else
	{
		Result.AddError(FString::Printf(TEXT("LightComponent is null for %s."), *Light->GetName()));
	}

	return Result;
}

FInterchangeTestFunctionResult ULightImportTestFunctions::CheckLightIntensity(ALight* Light, float ExpectedLightIntensity)
{
	FInterchangeTestFunctionResult Result;

	if(ULightComponent* LightComponent = Light->GetLightComponent())
	{
		if(float LightIntensity = LightComponent->Intensity; !FMath::IsNearlyEqual(LightIntensity, ExpectedLightIntensity))
		{
			Result.AddError(FString::Printf(TEXT("Expected %g light intensity, imported %g."), LightIntensity, ExpectedLightIntensity));
		}
	}
	else
	{
		Result.AddError(FString::Printf(TEXT("LightComponent is null for %s."), *Light->GetName()));
	}

	return Result;
}

FInterchangeTestFunctionResult ULightImportTestFunctions::CheckLightColor(ALight* Light, const FLinearColor& ExpectedLightColor)
{
	FInterchangeTestFunctionResult Result;

	if(ULightComponent* LightComponent = Light->GetLightComponent())
	{
		if(LightComponent->LightColor != ExpectedLightColor.ToFColor(true))
		{
			const FLinearColor LightLinearColor = LightComponent->GetLightColor();
			Result.AddError(FString::Printf(TEXT("Expected (%g, %g, %g) light color, imported (%g, %g, %g)."),
											ExpectedLightColor.R, ExpectedLightColor.G, ExpectedLightColor.B,
											LightLinearColor.R, LightLinearColor.G, LightLinearColor.B));
		}
	}
	else
	{
		Result.AddError(FString::Printf(TEXT("LightComponent is null for %s."), *Light->GetName()));
	}

	return Result;
}