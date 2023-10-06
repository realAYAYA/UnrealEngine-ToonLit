// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "BevelType.generated.h"

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
