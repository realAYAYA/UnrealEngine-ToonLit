// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFCurveUtility.h"
#include "Curves/CurveLinearColor.h"

bool FGLTFCurveUtility::HasAnyAdjustment(const UCurveLinearColor& ColorCurve, float Tolerance)
{
	return !FMath::IsNearlyEqual(ColorCurve.AdjustBrightness,      1, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustHue,             0, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustSaturation,      1, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustVibrance,        0, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustBrightnessCurve, 1, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustMaxAlpha,        1, Tolerance)
		|| !FMath::IsNearlyEqual(ColorCurve.AdjustMinAlpha,        0, Tolerance);
}
