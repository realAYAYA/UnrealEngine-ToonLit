// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGPoint.h"

FPCGPoint::FPCGPoint(const FTransform& InTransform, float InDensity, int32 InSeed)
	: Transform(InTransform)
	, Density(InDensity)
	, Seed(InSeed)
{
}

FBox FPCGPoint::GetLocalBounds() const
{
	return FBox(BoundsMin, BoundsMax);
}

void FPCGPoint::SetLocalBounds(const FBox& InBounds)
{
	BoundsMin = InBounds.Min;
	BoundsMax = InBounds.Max;
}

FBoxSphereBounds FPCGPoint::GetDensityBounds() const
{
	return FBoxSphereBounds(FBox((2 - Steepness) * BoundsMin, (2 - Steepness) * BoundsMax).TransformBy(Transform));
}