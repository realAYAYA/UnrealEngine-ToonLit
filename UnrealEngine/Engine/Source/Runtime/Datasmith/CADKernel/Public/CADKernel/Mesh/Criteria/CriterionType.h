// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

namespace UE::CADKernel
{
enum class ECriterion : uint8
{
	Size = 0,
	MaxSize = 1,
	MinSize = 2,
	Angle = 3,
	Sag = 4,
	CADCurvature = 5,
	None = 255
};
} // namespace UE::CADKernel
