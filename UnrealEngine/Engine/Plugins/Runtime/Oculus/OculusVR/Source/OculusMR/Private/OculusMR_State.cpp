// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusMR_State.h"
#include "OculusMRFunctionLibrary.h"

UOculusMR_State::UOculusMR_State(const FObjectInitializer& ObjectInitializer)
	: TrackedCamera()
	, TrackingReferenceComponent(nullptr)
	, ScalingFactor(1.0f)
	, CurrentCapturingCamera(ovrpCameraDevice_None)
	, ChangeCameraStateRequested(false)
	, BindToTrackedCameraIndexRequested(false)
{
}