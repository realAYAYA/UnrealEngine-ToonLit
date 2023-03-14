// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationBlueprintLibrary.h"
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