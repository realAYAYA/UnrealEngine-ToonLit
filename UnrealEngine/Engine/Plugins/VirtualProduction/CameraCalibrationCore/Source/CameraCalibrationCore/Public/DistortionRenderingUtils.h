// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector2D.h"

class UTextureRenderTarget2D;

namespace DistortionRenderingUtils
{
	/** Undistort the input array of 2D image points using the input displacement map */
	CAMERACALIBRATIONCORE_API void UndistortImagePoints(UTextureRenderTarget2D* DistortionMap, TArray<FVector2D> ImagePoints, TArray<FVector2D>& OutUndistortedPoints);
};
