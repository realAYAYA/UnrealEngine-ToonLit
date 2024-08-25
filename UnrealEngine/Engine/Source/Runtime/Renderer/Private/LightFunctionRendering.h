// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"

class FViewInfo;
class FLightSceneInfo;

// Computes a matrix to transform float4(SvPosition.xyz,1) directly to coordinate system of the given light
void LightFunctionSvPositionToLightTransform(FMatrix44f& OutMatrix, const FViewInfo& View, const FLightSceneInfo& LightSceneInfo);

// Returns a fade fraction for a light function and a given view based on the appropriate fade settings.
float GetLightFunctionFadeFraction(const FViewInfo& View, FSphere LightBounds);
