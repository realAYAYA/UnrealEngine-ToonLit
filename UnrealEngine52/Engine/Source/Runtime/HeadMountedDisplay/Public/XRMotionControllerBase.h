// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IMotionController.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/Timespan.h"
#include "UObject/NameTypes.h"

enum class EControllerHand : uint8;

/**
* Base utility class for implementations of the IMotionController interface
*/
class HEADMOUNTEDDISPLAY_API FXRMotionControllerBase : public IMotionController
{
public:
	virtual ~FXRMotionControllerBase() {};

	// Begin IMotionController interface
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const;
	virtual bool GetControllerOrientationAndPositionForTime(const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& OutTimeWasUsed, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityRadPerSec, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const;
	virtual void EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const;
	virtual float GetCustomParameterValue(const FName MotionSource, FName ParameterName, bool& bValueFound) const { bValueFound = false;  return 0.f; }
	virtual bool GetHandJointPosition(const FName MotionSource, int jointIndex, FVector& OutPosition) const override { return false; }
	// End IMotionController interface

	// explicit source names
	static FName LeftHandSourceId;
	static FName RightHandSourceId;
	static FName HMDSourceId;

	static bool GetHandEnumForSourceName(const FName Source, EControllerHand& OutHand);

	// Original GetControllerOrientationAndPosition signature for backwards compatibility 
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const = 0;
	// Original GetControllerTrackingStatus signature for backwards compatibility
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const = 0;
};