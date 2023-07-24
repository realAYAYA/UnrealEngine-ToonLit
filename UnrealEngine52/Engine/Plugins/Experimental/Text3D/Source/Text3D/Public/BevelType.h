// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

UENUM()
enum class EText3DBevelType : uint8
{
    Linear,
	HalfCircle,
    Convex,
	Concave,
	OneStep,
	TwoSteps,
	Engraved
};
