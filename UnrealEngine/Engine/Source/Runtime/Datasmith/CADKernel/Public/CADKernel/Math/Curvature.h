// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/GeoEnum.h"

namespace UE::CADKernel
{
struct FIsoCurvature
{
	double Min = HUGE_VALUE;
	double Max = 0;
};

struct FSurfaceCurvature
{
	FIsoCurvature Curvatures[2];

	constexpr const FIsoCurvature& operator[](const EIso& Iso) const
	{
		return Curvatures[Iso];
	}

	constexpr FIsoCurvature& operator[](const EIso& Iso)
	{
		return Curvatures[Iso];
	}
};

}
