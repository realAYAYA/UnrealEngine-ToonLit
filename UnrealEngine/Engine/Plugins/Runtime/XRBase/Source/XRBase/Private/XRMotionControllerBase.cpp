// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRMotionControllerBase.h"
#include "UObject/UObjectGlobals.h" // for FindObject<>
#include "UObject/Class.h" // for UEnum
#include "InputCoreTypes.h" // for EControllerHand

namespace XRMotionControllerBase_Impl
{
	static const FName LegacySources[] = {
		{ IMotionController::LeftHandSourceId, }, // EControllerHand::Left
		{ IMotionController::RightHandSourceId }, // EControllerHand::Right
		{ TEXT("AnyHand")                            }, // EControllerHand::AnyHand
		{ TEXT("Pad")                                }, // EControllerHand::Pad
		{ TEXT("ExternalCamera")                     }, // EControllerHand::ExternalCamera
		{ TEXT("Gun")                                }, // EControllerHand::Gun
		{ TEXT("HMD")                                }, // EControllerHand::HMD
		{ TEXT("Chest")                              }, // EControllerHand::Chest
		{ TEXT("LeftShoulder")                       }, // EControllerHand::LeftShoulder
		{ TEXT("RightShoulder")                      }, // EControllerHand::RightShoulder
		{ TEXT("LeftElbow")                          }, // EControllerHand::LeftElbow
		{ TEXT("RightElbow")                         }, // EControllerHand::RightElbow
		{ TEXT("Waist")                              }, // EControllerHand::Waist
		{ TEXT("LeftKnee")                           }, // EControllerHand::LeftKnee
		{ TEXT("RightKnee")                          }, // EControllerHand::RightKnee
		{ TEXT("LeftFoot")                           }, // EControllerHand::LeftFoot
		{ TEXT("RightFoot")                          }, // EControllerHand::RightFoot
		{ TEXT("Special")                            }  // EControllerHand::Special
	};
}

/* // Sample overrides of these functions for controllers that do support velocity and/or acceleration and where getting velocity.
// Be aware that these functions can be called from the game thread or the render thread!
// One might squeeze some efficiency out by providing an implementation of GetControllerOrientationAndPosition without velocity data that does less work rather than
// using the implementations below..
bool YOURCLASS::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	bool Unused_bProvidedLinearVelocity;
	bool Unused_OutbProvidedAngularVelocity;
	bool Unused_OutbProvidedLinearAcceleration;
	FVector Unused_LinearVelocity;
	FVector Unused_AngularVelocity;
	FVector Unused_LinearAcceleration;
	return GetControllerOrientationAndPosition(ControllerIndex, MotionSource, OutOrientation, OutPosition, Unused_bProvidedLinearVelocity, Unused_LinearVelocity, Unused_OutbProvidedAngularVelocity, Unused_AngularVelocity, Unused_OutbProvidedLinearAcceleration, Unused_LinearAcceleration, WorldToMetersScale);
}
bool YOURCLASS::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const
{
	// FTimespan initializes to 0 and GetControllerOrientationAndPositionForTime with time 0 will return the latest data.
	FTimespan Time;
	bool OutTimeWasUsed = false;
	return GetControllerOrientationAndPositionForTime(ControllerIndex, MotionSource, Time, OutTimeWasUsed, OutOrientation, OutPosition, OutbProvidedLinearVelocity, OutLinearVelocity, OutbProvidedAngularVelocity, OutAngularVelocityAsAxisAndLength, OutbProvidedLinearAcceleration, OutLinearAcceleration, WorldToMetersScale);
}
bool YOURCLASS::GetControllerOrientationAndPositionForTime(const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& OutTimeWasUsed, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const
{
	// Do the actual implementation, and ensure that Time 0 returns the latest data.
}
*/

bool FXRMotionControllerBase::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const
{
	// Default implementation so that the vr implementations that do not support velocity, etc, do not have to implement this function.
	OutbProvidedLinearVelocity = false;
	OutbProvidedAngularVelocity = false;
	OutbProvidedLinearAcceleration = false;
	return GetControllerOrientationAndPosition(ControllerIndex, MotionSource, OutOrientation, OutPosition, WorldToMetersScale);
}

bool FXRMotionControllerBase::GetControllerOrientationAndPositionForTime(const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& OutTimeWasUsed, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityAsAxisAndLength, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const
{
	// Default implementation simply ignores the Timespan, no additional accuracy is provided by this call, velocities are not provided.
	OutTimeWasUsed = false;
	OutbProvidedLinearVelocity = false;
	OutbProvidedAngularVelocity = false;
	OutbProvidedLinearAcceleration = false;
	return GetControllerOrientationAndPosition(ControllerIndex, MotionSource, OutOrientation, OutPosition, WorldToMetersScale);
}

void FXRMotionControllerBase::EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const
{
	const int32 LegaceSourcesCount = UE_ARRAY_COUNT(XRMotionControllerBase_Impl::LegacySources);
	for (int32 Index = 0; Index < LegaceSourcesCount; ++Index)
	{
		SourcesOut.Add(XRMotionControllerBase_Impl::LegacySources[Index]);
	}
}

bool FXRMotionControllerBaseLegacy::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	bool bSucess = false;
	if (ControllerIndex != INDEX_NONE)
	{
		EControllerHand DeviceHand;
		if (GetHandEnumForSourceName(MotionSource, DeviceHand))
		{
			if (DeviceHand == EControllerHand::AnyHand)
			{
				bSucess = GetControllerOrientationAndPosition(ControllerIndex, EControllerHand::Left, OutOrientation, OutPosition, WorldToMetersScale);
				if (!bSucess)
				{
					bSucess = GetControllerOrientationAndPosition(ControllerIndex, EControllerHand::Right, OutOrientation, OutPosition, WorldToMetersScale);
				}
			}
			else
			{
				bSucess = GetControllerOrientationAndPosition(ControllerIndex, DeviceHand, OutOrientation, OutPosition, WorldToMetersScale);
			}
		}
	}
	return bSucess;
}

ETrackingStatus FXRMotionControllerBaseLegacy::GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const
{
	if (ControllerIndex != INDEX_NONE)
	{
		EControllerHand DeviceHand;
		if (GetHandEnumForSourceName(MotionSource, DeviceHand))
		{
			if (DeviceHand == EControllerHand::AnyHand)
			{
				FRotator ThrowawayOrientation;
				FVector  ThrowawayPosition;
				// we've moved explicit handling of the 'AnyHand' source into this class from UMotionControllerComponent,
				// to maintain behavior we use the return value of GetControllerOrientationAndPosition() to determine which hand's
				// status we should check
				if (GetControllerOrientationAndPosition(ControllerIndex, EControllerHand::Left, ThrowawayOrientation, ThrowawayPosition, 100.f))
				{
					return GetControllerTrackingStatus(ControllerIndex, EControllerHand::Left);
				}
				return GetControllerTrackingStatus(ControllerIndex, EControllerHand::Right);
			}
			return GetControllerTrackingStatus(ControllerIndex, DeviceHand);
		}
	}
	return ETrackingStatus::NotTracked;
}