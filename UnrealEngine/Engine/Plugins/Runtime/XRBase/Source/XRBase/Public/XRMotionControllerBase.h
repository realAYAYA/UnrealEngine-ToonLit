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
class XRBASE_API FXRMotionControllerBase : public IMotionController
{
public:
	virtual ~FXRMotionControllerBase() {};

	// Begin IMotionController interface
	//Note: In this class we are providing a default implementation of GetControllerOrientationAndPosition with velocity, etc parameters and GetControllerOrientationAndPositionForTime that never provide that 
	// data and require override of the four param version because this is the most common implementation now. 
	//Any child class that does support some of the additional params should override all three functions.
	// See the cpp file for some sample implementations of those overrides.
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const = 0;
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const;
	virtual bool GetControllerOrientationAndPositionForTime(const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& OutTimeWasUsed, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const = 0;
	virtual void EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const;
	virtual float GetCustomParameterValue(const FName MotionSource, FName ParameterName, bool& bValueFound) const { bValueFound = false;  return 0.f; }
	virtual bool GetHandJointPosition(const FName MotionSource, int jointIndex, FVector& OutPosition) const override { return false; }
	// End IMotionController interface
};

// This class adapts deprecated or maintenence only xr plugins which use the old EControllerHand motion sources to the newer FName MotionSource API.
// If a plugin is still in active development it should refactor and derive from IMotionController or FXRMotionControllerBase rather than derive from this class.
class XRBASE_API FXRMotionControllerBaseLegacy : public FXRMotionControllerBase
{
private:
	// Begin IMotionController interface
	using FXRMotionControllerBase::GetControllerOrientationAndPosition; // This avoids a -Woverloaded-virtual warning from clang about the legacy GetControllerOrientationAndPosition overload below hiding the parent versions of GetControllerOrientationAndPosition (because that is tricky, and possibly a bug especially when overloads can be matched by type conversion). We do want them hidden from legacy implementations, however, so it is private.
	// These are overridden to call into the backward compatibility functions below.  They are final and private because a legacy vr plugin would not use or implement these functions.
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override final;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const override final;
	// End IMotionController interface

public:
	// Original GetControllerOrientationAndPosition signature for backwards compatibility.  They are pure virtual because a legacy plugin must implement them and public because a legacy vr plugin could use them internally.
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const = 0;
	// Original GetControllerTrackingStatus signature for backwards compatibility
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const = 0;
};