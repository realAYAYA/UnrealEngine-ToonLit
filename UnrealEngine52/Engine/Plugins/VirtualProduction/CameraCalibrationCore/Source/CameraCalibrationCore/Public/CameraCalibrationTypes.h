// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "LensDistortionModelHandlerBase.h"
#include "Misc/Guid.h"

#include "CameraCalibrationTypes.generated.h"

/** Utility structure for selecting a distortion handler from the camera calibration subsystem */
struct UE_DEPRECATED(5.1, "This struct has been deprecated.") FDistortionHandlerPicker;

USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FDistortionHandlerPicker
{
	GENERATED_BODY()

public:
	/** Default destructor */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~FDistortionHandlerPicker() = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

public:
	/** CineCameraComponent with which the desired distortion handler is associated */
	UPROPERTY(BlueprintReadWrite, Category = "Distortion", Transient)
	TObjectPtr<UCineCameraComponent> TargetCameraComponent = nullptr;

	/** UObject that produces the distortion state for the desired distortion handler */
	UPROPERTY(BlueprintReadWrite, Category = "Distortion")
	FGuid DistortionProducerID;

	/** Display name of the desired distortion handler */
	UPROPERTY(BlueprintReadWrite, Category = "Distortion")
	FString HandlerDisplayName;
};