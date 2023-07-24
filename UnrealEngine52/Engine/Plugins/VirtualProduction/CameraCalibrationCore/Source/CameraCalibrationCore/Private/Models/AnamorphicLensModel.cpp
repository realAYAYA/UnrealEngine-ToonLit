// Copyright Epic Games, Inc. All Rights Reserved.

#include "Models/AnamorphicLensModel.h"

UScriptStruct* UAnamorphicLensModel::GetParameterStruct() const
{
	return FAnamorphicDistortionParameters::StaticStruct();
}

FName UAnamorphicLensModel::GetModelName() const
{
	return FName("3DE4 Anamorphic Standard Degree 4");
}

FName UAnamorphicLensModel::GetShortModelName() const
{
	return TEXT("3DE4 Anamorphic Standard Degree 4");
}