// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helper class for commonly used functions for camera calibration.
 */
class CAMERACALIBRATIONCORE_API FCameraCalibrationUtils
{
public:
	/** Compares two transforms and returns true if they are nearly equal in distance and angle */
	static bool IsNearlyEqual(const FTransform& A, const FTransform& B, float MaxLocationDelta = 2.0f, float MaxAngleDegrees = 2.0f);
};
