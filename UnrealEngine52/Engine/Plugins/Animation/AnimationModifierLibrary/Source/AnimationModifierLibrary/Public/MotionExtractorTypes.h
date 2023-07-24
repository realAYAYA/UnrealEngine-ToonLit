// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MotionExtractorTypes.generated.h"

/** Type of motion to extract */
UENUM(BlueprintType)
enum class EMotionExtractor_MotionType : uint8
{
	Translation,
	Rotation,
	Scale,
	TranslationSpeed,
	RotationSpeed
};

/** Axis to get the final value from */
UENUM(BlueprintType)
enum class EMotionExtractor_Axis : uint8
{
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	XYZ
};

/** Math operations that can be applied to the extracted value before add it to the curve */
UENUM(BlueprintType)
enum class EMotionExtractor_MathOperation : uint8
{
	None,
	Addition,
	Subtraction,
	Division,
	Multiplication
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AnimationBlueprintLibrary.h"
#include "CoreMinimal.h"
#endif
