// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusInputFunctionLibrary.h"
#include "OculusHandTracking.h"
#include "Logging/MessageLog.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

//-------------------------------------------------------------------------------------------------
// UOculusHandTrackingFunctionLibrary
//-------------------------------------------------------------------------------------------------
UDEPRECATED_UOculusInputFunctionLibrary::UDEPRECATED_UOculusInputFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEPRECATED_UOculusInputFunctionLibrary::FHandMovementFilterDelegate UDEPRECATED_UOculusInputFunctionLibrary::HandMovementFilter;


EOculusFinger UDEPRECATED_UOculusInputFunctionLibrary::ConvertBoneToFinger(const EBone Bone)
{
	switch (Bone)
	{
	case EBone::Index_1:
	case EBone::Index_2:
	case EBone::Index_3:
	case EBone::Index_Tip:
		return EOculusFinger::Index;
	case EBone::Middle_1:
	case EBone::Middle_2:
	case EBone::Middle_3:
	case EBone::Middle_Tip:
		return EOculusFinger::Middle;
	case EBone::Pinky_0:
	case EBone::Pinky_1:
	case EBone::Pinky_2:
	case EBone::Pinky_3:
	case EBone::Pinky_Tip:
		return EOculusFinger::Pinky;
	case EBone::Ring_1:
	case EBone::Ring_2:
	case EBone::Ring_3:
	case EBone::Ring_Tip:
		return EOculusFinger::Ring;
	case EBone::Thumb_0:
	case EBone::Thumb_1:
	case EBone::Thumb_2:
	case EBone::Thumb_3:
	case EBone::Thumb_Tip:
		return EOculusFinger::Thumb;
	default:
		return EOculusFinger::Invalid;
	}
}

ETrackingConfidence UDEPRECATED_UOculusInputFunctionLibrary::GetFingerTrackingConfidence(const EOculusHandType DeviceHand, const EOculusFinger Finger, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::GetFingerTrackingConfidence(ControllerIndex, DeviceHand, (OculusInput::EOculusHandAxes)(uint8)Finger);
}

bool UDEPRECATED_UOculusInputFunctionLibrary::GetHandSkeletalMesh(USkeletalMesh* HandSkeletalMesh, EOculusHandType SkeletonType, EOculusHandType MeshType, float WorldToMeters)
{
	return OculusInput::FOculusHandTracking::GetHandSkeletalMesh(HandSkeletalMesh, SkeletonType, MeshType, WorldToMeters);
}

TArray<FOculusCapsuleCollider> UDEPRECATED_UOculusInputFunctionLibrary::InitializeHandPhysics(EOculusHandType SkeletonType, USkinnedMeshComponent* HandComponent, const float WorldToMeters)
{
	return OculusInput::FOculusHandTracking::InitializeHandPhysics(SkeletonType, HandComponent, WorldToMeters);
}

FQuat UDEPRECATED_UOculusInputFunctionLibrary::GetBoneRotation(const EOculusHandType DeviceHand, const EBone BoneId, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::GetBoneRotation(ControllerIndex, DeviceHand, BoneId);
}

ETrackingConfidence UDEPRECATED_UOculusInputFunctionLibrary::GetTrackingConfidence(const EOculusHandType DeviceHand, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::GetTrackingConfidence(ControllerIndex, DeviceHand);
}

FTransform UDEPRECATED_UOculusInputFunctionLibrary::GetPointerPose(const EOculusHandType DeviceHand, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::GetPointerPose(ControllerIndex, DeviceHand);
}

bool UDEPRECATED_UOculusInputFunctionLibrary::IsPointerPoseValid(const EOculusHandType DeviceHand, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::IsPointerPoseValid(ControllerIndex, DeviceHand);
}

float UDEPRECATED_UOculusInputFunctionLibrary::GetHandScale(const EOculusHandType DeviceHand, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::GetHandScale(ControllerIndex, DeviceHand);
}

EOculusHandType UDEPRECATED_UOculusInputFunctionLibrary::GetDominantHand(const int32 ControllerIndex)
{
	EOculusHandType DominantHand = EOculusHandType::None;
	if (OculusInput::FOculusHandTracking::IsHandDominant(ControllerIndex, EOculusHandType::HandLeft))
	{
		DominantHand = EOculusHandType::HandLeft;
	}
	else if (OculusInput::FOculusHandTracking::IsHandDominant(ControllerIndex, EOculusHandType::HandRight))
	{
		DominantHand = EOculusHandType::HandRight;
	}
	return DominantHand;
}

bool UDEPRECATED_UOculusInputFunctionLibrary::IsHandTrackingEnabled()
{
	return OculusInput::FOculusHandTracking::IsHandTrackingEnabled();
}

bool UDEPRECATED_UOculusInputFunctionLibrary::IsHandPositionValid(const EOculusHandType DeviceHand, const int32 ControllerIndex)
{
	return OculusInput::FOculusHandTracking::IsHandPositionValid(ControllerIndex, DeviceHand);
}

FString UDEPRECATED_UOculusInputFunctionLibrary::GetBoneName(EBone BoneId)
{
	uint32 ovrBoneId = OculusInput::FOculusHandTracking::ToOvrBone(BoneId);
	return OculusInput::FOculusHandTracking::GetBoneName(ovrBoneId);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

