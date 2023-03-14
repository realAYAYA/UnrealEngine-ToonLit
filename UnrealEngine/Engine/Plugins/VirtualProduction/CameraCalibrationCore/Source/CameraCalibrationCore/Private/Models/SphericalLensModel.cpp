// Copyright Epic Games, Inc. All Rights Reserved.

#include "Models/SphericalLensModel.h"

UScriptStruct* USphericalLensModel::GetParameterStruct() const
{
	return FSphericalDistortionParameters::StaticStruct();
}

FName USphericalLensModel::GetModelName() const 
{ 
	return FName("Spherical Lens Model"); 
}

FName USphericalLensModel::GetShortModelName() const
{
	return TEXT("Spherical");
}