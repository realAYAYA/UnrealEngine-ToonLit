// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

// Returns whether any calibration material pass is enabled.
bool IsPostProcessVisualizeCalibrationMaterialEnabled(const FSceneView& View);

// Returns whether any calibration material pass is enabled.
const UMaterialInterface* GetPostProcessVisualizeCalibrationMaterialInterface(const FSceneView& View);
