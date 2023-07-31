// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraModularFeature.h"

FName ICameraModularFeature::GetModularFeatureName()
{
	static FName FeatureName = FName(TEXT("Camera"));
	return FeatureName;
}
