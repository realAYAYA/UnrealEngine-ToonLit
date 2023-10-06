// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CalibratedMapFormat.generated.h"

/** Specifies the location of the pixel origin in a calibrated map */
UENUM()
enum class ECalibratedMapPixelOrigin : uint8
{
	TopLeft,
	BottomLeft,

	MAX // Required option for shader permutation
};

/** Specifies which two channels contain the calibrated map data (or None if there is no data) */
UENUM()
enum class ECalibratedMapChannels : uint8
{
	RG,
	BA,
	None,

	MAX // Required option for shader permutation
};

/** Formatting options for processing a calibrated map */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FCalibratedMapFormat
{
	GENERATED_BODY()

public:
	/** Specifies where in the image the (0, 0) pixel is */
	UPROPERTY(EditAnywhere, Category="Format")
	ECalibratedMapPixelOrigin PixelOrigin = ECalibratedMapPixelOrigin::TopLeft;

	/** Specifies which two channels contain the undistortion map (or None if there is no undistortion data) */
	UPROPERTY(EditAnywhere, Category = "Format")
	ECalibratedMapChannels UndistortionChannels = ECalibratedMapChannels::RG;

	/** Specifies which two channels contain the distortion map (or None if there is no distortion data) */
	UPROPERTY(EditAnywhere, Category = "Format")
	ECalibratedMapChannels DistortionChannels = ECalibratedMapChannels::BA;
};