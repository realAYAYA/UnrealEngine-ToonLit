// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

class FViewInfo;
class FScene;

struct FRayTracingReflectionOptions
{
	enum EAlgorithm
	{
		BruteForce,
		Sorted,
		SortedDeferred
	};

	bool bEnabled = true;
	EAlgorithm Algorithm = EAlgorithm::Sorted;
	int32 SamplesPerPixel = 1;
	float ResolutionFraction = 1.0f;
	float MaxRoughness = 1.0f;
	bool bReflectOnlyWater = false;
	bool bSkyLight = true;
	bool bDirectLighting = true;
	bool bEmissiveAndIndirectLighting = true;
	bool bReflectionCaptures = true;
};

FRayTracingReflectionOptions GetRayTracingReflectionOptions(const FViewInfo& View, const FScene& Scene);
