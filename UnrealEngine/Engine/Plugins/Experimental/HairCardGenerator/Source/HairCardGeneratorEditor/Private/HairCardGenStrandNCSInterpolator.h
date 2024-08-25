// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"
#include "HairCardGenNaturalCubicSplines.h"

class FHairCardGenStrandNSCInterpolator
{
private:
    TArray<FHairCardGenNaturalCubicSplines> Splines;

public:
    struct StrandInterpolationResult
    {
        TArray<float> Positions;
        TArray<float> Widths;
    };

public:
	FHairCardGenStrandNSCInterpolator(const TArray<FVector>& Positions, const TArray<float>& Widths);
    StrandInterpolationResult GetInterpolatedStrand(const int NumInterpolatedPoints);
}; 
