// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Parameters.h"


namespace UE::CADKernel
{
class CADKERNEL_API FKernelParameters : public FParameters
{
public:
	FParameter GeometricalTolerance;

	FKernelParameters()
		: FParameters(3)
		, GeometricalTolerance(TEXT("GeometricalTolerance"), 0.02, *this)
	{}
};
} // namespace UE::CADKernel

