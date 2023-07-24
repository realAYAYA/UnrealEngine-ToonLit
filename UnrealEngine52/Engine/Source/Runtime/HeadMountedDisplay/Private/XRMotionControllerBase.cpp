// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRMotionControllerBase.h"
#include "UObject/UObjectGlobals.h" // for FindObject<>
#include "UObject/Class.h" // for UEnum
#include "InputCoreTypes.h" // for EControllerHand

FName FXRMotionControllerBase::LeftHandSourceId(TEXT("Left"));
FName FXRMotionControllerBase::RightHandSourceId(TEXT("Right"));
FName FXRMotionControllerBase::HMDSourceId(TEXT("HMD"));

namespace XRMotionControllerBase_Impl
{
	static const FName LegacySources[] = {
		{ FXRMotionControllerBase::LeftHandSourceId, }, // EControllerHand::Left
		{ FXRMotionControllerBase::RightHandSourceId }, // EControllerHand::Right
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

bool FXRMotionControllerBase::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
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

bool FXRMotionControllerBase::GetControllerOrientationAndPositionForTime(const int32 ControllerIndex, const FName MotionSource, FTimespan Time, bool& OutTimeWasUsed, FRotator& OutOrientation, FVector& OutPosition, bool& OutbProvidedLinearVelocity, FVector& OutLinearVelocity, bool& OutbProvidedAngularVelocity, FVector& OutAngularVelocityRadPerSec, bool& OutbProvidedLinearAcceleration, FVector& OutLinearAcceleration, float WorldToMetersScale) const
{
	// Default implementation simply ignores the Timespan, no additional accuracy is provided by this call, velocities are not provided.
	OutTimeWasUsed = false;
	OutbProvidedLinearVelocity = false;
	OutbProvidedAngularVelocity = false;
	OutbProvidedLinearAcceleration = false;
	return GetControllerOrientationAndPosition(ControllerIndex, MotionSource, OutOrientation, OutPosition, WorldToMetersScale);
}

ETrackingStatus FXRMotionControllerBase::GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const
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

void FXRMotionControllerBase::EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const
{
	const int32 LegaceSourcesCount = UE_ARRAY_COUNT(XRMotionControllerBase_Impl::LegacySources);
	for (int32 Index = 0; Index < LegaceSourcesCount; ++Index)
	{
		SourcesOut.Add(XRMotionControllerBase_Impl::LegacySources[Index]);
	}
}

bool FXRMotionControllerBase::GetHandEnumForSourceName(const FName Source, EControllerHand& OutHand)
{
	static TMap<FName, EControllerHand> MotionSourceToEControllerHandMap;
	if (MotionSourceToEControllerHandMap.Num() == 0)
	{
		// Motion source names that map to legacy EControllerHand values
		MotionSourceToEControllerHandMap.Add(FXRMotionControllerBase::LeftHandSourceId, EControllerHand::Left);
		MotionSourceToEControllerHandMap.Add(FXRMotionControllerBase::RightHandSourceId, EControllerHand::Right);
		MotionSourceToEControllerHandMap.Add(TEXT("AnyHand"), EControllerHand::AnyHand);
		MotionSourceToEControllerHandMap.Add(TEXT("Pad"), EControllerHand::Pad);
		MotionSourceToEControllerHandMap.Add(TEXT("ExternalCamera"), EControllerHand::ExternalCamera);
		MotionSourceToEControllerHandMap.Add(TEXT("Gun"), EControllerHand::Gun);
		MotionSourceToEControllerHandMap.Add(TEXT("HMD"), EControllerHand::HMD);
		MotionSourceToEControllerHandMap.Add(TEXT("Chest"), EControllerHand::Chest);
		MotionSourceToEControllerHandMap.Add(TEXT("LeftShoulder"), EControllerHand::LeftShoulder);
		MotionSourceToEControllerHandMap.Add(TEXT("RightShoulder"), EControllerHand::RightShoulder);
		MotionSourceToEControllerHandMap.Add(TEXT("LeftElbow"), EControllerHand::LeftElbow);
		MotionSourceToEControllerHandMap.Add(TEXT("RightElbow"), EControllerHand::RightElbow);
		MotionSourceToEControllerHandMap.Add(TEXT("Waist"), EControllerHand::Waist);
		MotionSourceToEControllerHandMap.Add(TEXT("LeftKnee"), EControllerHand::LeftKnee);
		MotionSourceToEControllerHandMap.Add(TEXT("RightKnee"), EControllerHand::RightKnee);
		MotionSourceToEControllerHandMap.Add(TEXT("LeftFoot"), EControllerHand::LeftFoot);
		MotionSourceToEControllerHandMap.Add(TEXT("RightFoot"), EControllerHand::RightFoot);
		MotionSourceToEControllerHandMap.Add(TEXT("Special"), EControllerHand::Special);
		// EControllerHand enum names mapped to EControllerHand enum values
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Left"), EControllerHand::Left);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Right"), EControllerHand::Right);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::AnyHand"), EControllerHand::AnyHand);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Pad"), EControllerHand::Pad);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::ExternalCamera"), EControllerHand::ExternalCamera);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Gun"), EControllerHand::Gun);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::HMD"), EControllerHand::HMD);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Chest"), EControllerHand::Chest);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::LeftShoulder"), EControllerHand::LeftShoulder);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::RightShoulder"), EControllerHand::RightShoulder);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::LeftElbow"), EControllerHand::LeftElbow);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::RightElbow"), EControllerHand::RightElbow);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Waist"), EControllerHand::Waist);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::LeftKnee"), EControllerHand::LeftKnee);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::RightKnee"), EControllerHand::RightKnee);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::LeftFoot"), EControllerHand::LeftFoot);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::RightFoot"), EControllerHand::RightFoot);
		MotionSourceToEControllerHandMap.Add(TEXT("EControllerHand::Special"), EControllerHand::Special);
		// Newer source names that can usefully map to legacy EControllerHand values
		MotionSourceToEControllerHandMap.Add(TEXT("LeftGrip"), EControllerHand::Left);
		MotionSourceToEControllerHandMap.Add(TEXT("RightGrip"), EControllerHand::Right);
		MotionSourceToEControllerHandMap.Add(TEXT("LeftAim"), EControllerHand::Left);
		MotionSourceToEControllerHandMap.Add(TEXT("RightAim"), EControllerHand::Right);
	}

	EControllerHand* FoundEnum = MotionSourceToEControllerHandMap.Find(Source);
	if (FoundEnum != nullptr)
	{
		OutHand = *FoundEnum;
		return true;
	}
	else
	{
		return false;
	}
}