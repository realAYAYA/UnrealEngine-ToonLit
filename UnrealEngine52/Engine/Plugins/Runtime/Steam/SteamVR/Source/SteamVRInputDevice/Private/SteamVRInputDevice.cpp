/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "SteamVRInputDevice.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/WorldSettings.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/IInputInterface.h"
#include "HAL/FileManagerGeneric.h"
#include "IMotionController.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "MotionControllerComponent.h"
#include "SteamVRControllerKeys.h"
#include "SteamVRSkeletonDefinition.h"
#include "openvr.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#include "IVREditorModule.h"
#include "VREditorMode.h"
#include "VREditorInteractor.h"
#endif

#define LOCTEXT_NAMESPACE "SteamVRInputDevice"
DEFINE_LOG_CATEGORY_STATIC(LogSteamVRInputDevice, Log, All);

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// List of bones that are effectively in model space because
// they are children of the root
static const int32 kModelSpaceBones[] = {
	ESteamVRBone_Wrist,
	ESteamVRBone_Aux_Thumb,
	ESteamVRBone_Aux_IndexFinger,
	ESteamVRBone_Aux_MiddleFinger,
	ESteamVRBone_Aux_RingFinger,
	ESteamVRBone_Aux_PinkyFinger,
};

// List of the metacarpal bones of the SteamVR skeleton
static const int32 kMetacarpalBones[] = {
	ESteamVRBone_Thumb0,
	ESteamVRBone_IndexFinger0,
	ESteamVRBone_MiddleFinger0,
	ESteamVRBone_RingFinger0,
	ESteamVRBone_PinkyFinger0
};

// List of bones that only need to have their translation mirrored in the SteamVR skeleton
static const int32 kMirrorTranslationOnlyBones[] = {
	ESteamVRBone_Thumb1,
	ESteamVRBone_Thumb2,
	ESteamVRBone_Thumb3,
	ESteamVRBone_IndexFinger1,
	ESteamVRBone_IndexFinger2,
	ESteamVRBone_IndexFinger3,
	ESteamVRBone_IndexFinger4,
	ESteamVRBone_MiddleFinger1,
	ESteamVRBone_MiddleFinger2,
	ESteamVRBone_MiddleFinger3,
	ESteamVRBone_MiddleFinger4,
	ESteamVRBone_RingFinger1,
	ESteamVRBone_RingFinger2,
	ESteamVRBone_RingFinger3,
	ESteamVRBone_RingFinger4,
	ESteamVRBone_PinkyFinger1,
	ESteamVRBone_PinkyFinger2,
	ESteamVRBone_PinkyFinger3,
	ESteamVRBone_PinkyFinger4
};


FSteamVRInputDevice::FSteamVRInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	: MessageHandler(InMessageHandler)
{
	// Initializations
	InitSteamVRSystem();
	InitControllerMappings();
	InitControllerKeys();

#if WITH_EDITOR
	GenerateActionManifest();
#endif

	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

FSteamVRInputDevice::~FSteamVRInputDevice()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

void FSteamVRInputDevice::InitSteamVRSystem()
{
	//UE_LOG(LogTemp, Warning, TEXT("Attempting to load Steam VR System..."));
	SteamVRHMDModule = FModuleManager::LoadModulePtr<ISteamVRPlugin>(TEXT("SteamVR"));

	if (SteamVRHMDModule && SteamVRHMDModule->GetVRSystem() && VRSystem() && VRInput())
	{
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("SteamVR runtime %u.%u.%u loaded."), k_nSteamVRVersionMajor, k_nSteamVRVersionMinor, k_nSteamVRVersionBuild);

		// Set Skeletal Handles
		bIsSkeletalControllerLeftPresent = SetSkeletalHandle(TCHAR_TO_UTF8(*FString(TEXT(ACTION_PATH_SKELETON_LEFT))), VRSkeletalHandleLeft);
		bIsSkeletalControllerRightPresent = SetSkeletalHandle(TCHAR_TO_UTF8(*FString(TEXT(ACTION_PATH_SKELETON_RIGHT))), VRSkeletalHandleRight);

		// (Re)Load Action Manifest
		GenerateActionManifest(true, true, true, false);

		// Set haptic handles
		LastInputError = VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT(ACTION_PATH_VIBRATE_LEFT))), &VRVibrationLeft);
		if (LastInputError != VRInputError_None || VRVibrationLeft == k_ulInvalidActionHandle)
		{
			VRVibrationLeft = k_ulInvalidActionHandle;
		}

		LastInputError = VRInput()->GetActionHandle(TCHAR_TO_UTF8(*FString(TEXT(ACTION_PATH_VIBRATE_RIGHT))), &VRVibrationRight);
		if (LastInputError != VRInputError_None || VRVibrationRight == k_ulInvalidActionHandle)
		{
			VRVibrationRight = k_ulInvalidActionHandle;
		}

		bSteamVRWasShutdown = false;
	}
}

void FSteamVRInputDevice::Tick(float DeltaTime)
{
	// Place current delta time in buffer for use in determining haptic duration
	CurrentDeltaTime = DeltaTime;

	// Watch for SteamVR availability & restarts
	if ((SteamVRHMDModule && !SteamVRHMDModule->GetVRSystem()) || bSteamVRWasShutdown)
	{
		bSteamVRWasShutdown = true;
		InitSteamVRSystem();
		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("SteamVR System Inactive. Trying to initialize..."));
	}
	else if (GEngine->XRSystem.IsValid() && SteamVRHMDModule && SteamVRHMDModule->GetVRSystem())
	{
		// Cache the controller transform to ensure ResetOrientationAndPosition gets the correct values (Valid for UE4.18 upwards)
		// https://github.com/ValveSoftware/steamvr_unreal_plugin/issues/2
		CachedBaseOrientation = GEngine->XRSystem->GetBaseOrientation();
		CachedBasePosition = GEngine->XRSystem->GetBasePosition();
	}
	else
	{
		CachedBaseOrientation = FQuat::Identity;
		CachedBasePosition = FVector::ZeroVector;
	}
}

void FSteamVRInputDevice::FindAxisMappings(const UInputSettings* InputSettings, const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const
{
	if (InAxisName.IsValid())
	{
		for (int32 AxisIndex = InputSettings->GetAxisMappings().Num() - 1; AxisIndex >= 0; --AxisIndex)
		{
			if (InputSettings->GetAxisMappings()[AxisIndex].AxisName == InAxisName)
			{
				OutMappings.Add(InputSettings->GetAxisMappings()[AxisIndex]);
			}
		}
	}
}

void FSteamVRInputDevice::GetSteamVRMappings(TArray<FInputAxisKeyMapping>& InUEKeyMappings, TArray<FSteamVRAxisKeyMapping>& OutMappings)
{
	OutMappings.Empty();
	for (auto& UEKeyMapping : InUEKeyMappings)
	{
		OutMappings.Add(FSteamVRAxisKeyMapping(UEKeyMapping, false, false));
	}
}

void FSteamVRInputDevice::FindActionMappings(const UInputSettings* InputSettings, const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const
{
	if (InActionName.IsValid())
	{
		for (int32 ActionIndex = InputSettings->GetActionMappings().Num() - 1; ActionIndex >= 0; --ActionIndex)
		{
			if (InputSettings->GetActionMappings()[ActionIndex].ActionName == InActionName)
			{
				OutMappings.Add(InputSettings->GetActionMappings()[ActionIndex]);
			}
		}
	}
}

FString FSteamVRInputDevice::SanitizeString(FString& InString)
{
	FString SanitizedString = InString.Replace(TEXT(" "), TEXT("-"));
	SanitizedString = SanitizedString.Replace(TEXT("*"), TEXT("-"));
	SanitizedString = SanitizedString.Replace(TEXT("."), TEXT("-"));

	return SanitizedString;
}

FTransform CalcModelSpaceTransform(const FTransform* OutBoneTransform, int32 BoneIndex)
{
	FTransform BoneTransformMS = OutBoneTransform[BoneIndex];

	while (BoneIndex != -1)
	{
		int32 parentIndex = SteamVRSkeleton::GetParentIndex(BoneIndex);
		if (parentIndex != -1)
		{
			BoneTransformMS = BoneTransformMS * OutBoneTransform[parentIndex];
			BoneIndex = parentIndex;
		}
		else
		{
			break;
		}
	}

	return BoneTransformMS;
}

bool FSteamVRInputDevice::GetSkeletalData(bool bLeftHand, bool bMirror, EVRSkeletalMotionRange MotionRange, FTransform* OutBoneTransform, int32 OutBoneTransformCount)
{
	// Check that the size of the buffer we will be writing into is big enough to hold all the bone transforms
	if (OutBoneTransformCount < STEAMVR_SKELETON_BONE_COUNT)
	{
		return false;
	}

	if (VRSystem() && VRInput())
	{
		// Get the handle for the skeletal action.  If its invalid (the necessary skeletal action is not in the manifest) then return false
		vr::VRActionHandle_t ActionHandle = (bLeftHand) ? VRSkeletalHandleLeft : VRSkeletalHandleRight;
		if (ActionHandle == k_ulInvalidActionHandle)
		{
			return false;
		}

		// Get skeletal data
		VRBoneTransform_t SteamVRBoneTransforms[STEAMVR_SKELETON_BONE_COUNT];
		EVRInputError Err = VRInput()->GetSkeletalBoneData(ActionHandle, vr::EVRSkeletalTransformSpace::VRSkeletalTransformSpace_Parent, MotionRange, SteamVRBoneTransforms, STEAMVR_SKELETON_BONE_COUNT);

		if (Err != VRInputError_None)
		{
			return false;
		}

		// Optionally mirror the pose to the opposite hand
		if (bMirror)
		{
			MirrorSteamVRSkeleton(SteamVRBoneTransforms, STEAMVR_SKELETON_BONE_COUNT);
		}

		// GetSkeletalBoneData returns bone transforms are in SteamVR's coordinate system, so
		// we need to convert them to UE4's coordinate system.
		// SteamVR coords:	X=right,	Y=up,		Z=backwards,	right-handed,	scale is meters
		// UE4 coords:		X=forward,	Y=right,	Z=up,			left-handed,	scale is centimeters

		// The root is positioned at the controller's anchor position with zero rotation.
		// However because of the conversion from SteamVR coordinates to Unreal coordinates the root bone is scaled
		// to the new coordinate system
		FTransform& RootTransform = OutBoneTransform[ESteamVRBone_Root];
		RootTransform.SetComponents(FQuat::Identity, FVector::ZeroVector, FVector(100.f, 100.f, 100.f));

		// Transform all the non-root bones to the new coordinate system
		for (int32 BoneIndex = ESteamVRBone_Root + 1; BoneIndex < STEAMVR_SKELETON_BONE_COUNT; ++BoneIndex)
		{
			const VRBoneTransform_t& SrcTransform = SteamVRBoneTransforms[BoneIndex];

			FQuat NewRotation(
				SrcTransform.orientation.z,
				-SrcTransform.orientation.x,
				SrcTransform.orientation.y,
				-SrcTransform.orientation.w
			);

			FVector NewTranslation(
				SrcTransform.position.v[2],
				-SrcTransform.position.v[0],
				SrcTransform.position.v[1]
			);

			FTransform& DstTransform = OutBoneTransform[BoneIndex];
			DstTransform.SetRotation(NewRotation);
			DstTransform.SetTranslation(NewTranslation);
		}

		// Apply an extra transformation to the children of the root bone to compensate for the changes made to the root
		// to make it fit the new coordinate system even though it has zero rotation
		FQuat FixupRotation(FVector(0.f, 0.f, 1.f), PI);

		for (int32 ChildIndex = 0; ChildIndex < SteamVRSkeleton::GetChildCount(ESteamVRBone_Root); ++ChildIndex)
		{
			int32 BoneIndex = SteamVRSkeleton::GetChildIndex(ESteamVRBone_Root, ChildIndex);

			FTransform& DstTransform = OutBoneTransform[BoneIndex];

			FVector NewTranslation = DstTransform.GetTranslation() * FVector(-1.f, -1.f, 1.f);
			FQuat NewRotation = FixupRotation * DstTransform.GetRotation();

			DstTransform.SetRotation(NewRotation);
			DstTransform.SetTranslation(NewTranslation);
		}

		return true;
	}

	return false;
}

void FSteamVRInputDevice::SendAnalogMessage(const ETrackedControllerRole TrackedControllerRole, const FGamepadKeyNames::Type AxisButton, float AnalogValue)
{
	FInputDeviceId DeviceId = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
	FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	
	if (TrackedControllerRole == ETrackedControllerRole::TrackedControllerRole_LeftHand && bCurlsAndSplaysEnabled_L)
	{
		MessageHandler->OnControllerAnalog(AxisButton, UserId, DeviceId, AnalogValue);
		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Left Index value: %f for axis %s"), ControllerState.IndexGripAnalog, *AxisButton.ToString());
	}
	else if (TrackedControllerRole == ETrackedControllerRole::TrackedControllerRole_RightHand && bCurlsAndSplaysEnabled_R)
	{
		MessageHandler->OnControllerAnalog(AxisButton, UserId, DeviceId, AnalogValue);
		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Left Index value: %f for axis %s"), ControllerState.IndexGripAnalog, *AxisButton.ToString());
	}
}

void FSteamVRInputDevice::SendControllerEvents()
{
	if (SteamVRHMDModule && SteamVRHMDModule->GetVRSystem() && VRSystem() && VRInput() && SteamVRInputActionSets.Num() > 0)
	{
		EVRInputError ActionStateError = VRInput()->UpdateActionState(ActiveActionSets, sizeof(VRActiveActionSet_t), 1);

		if (ActionStateError != VRInputError_None)
		{
			//GetInputError(ActionStateError, TEXT("Error encountered when trying to update the action state"));
			return;
		}

		// Go through all Actions in all active ActionSets
		for (auto& SteamVRInputActionSet : SteamVRInputActionSets)
		{
			ProcessActionEvents(SteamVRInputActionSet);
		}
	}
}

void FSteamVRInputDevice::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FSteamVRInputDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

bool FSteamVRInputDevice::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	if (SteamVRHMDModule && SteamVRHMDModule->GetVRSystem() && VRInput() && VRCompositor())
	{
		InputPoseActionData_t PoseData = {};
		EVRInputError InputError = VRInputError_NoData;

		VRActionHandle_t LeftActionHandle = bUseSkeletonPose ? VRSkeletalHandleLeft : VRControllerHandleLeft;
		VRActionHandle_t RightActionHandle = bUseSkeletonPose ? VRSkeletalHandleRight : VRControllerHandleRight;

		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("MOTION SOURCE: %s"), *MotionSource.ToString());
		if (MotionSource.IsEqual(TEXT("Left")) || MotionSource.IsEqual(TEXT("EControllerHand::Left")))
		{
			if (LeftActionHandle != vr::k_ulInvalidActionHandle)
			{
				if (GlobalPredictedSecondsFromNow <= -9999.f)
				{
					InputError = VRInput()->GetPoseActionDataForNextFrame(LeftActionHandle, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
				}
				else
				{
					InputError = VRInput()->GetPoseActionDataRelativeToNow(LeftActionHandle, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
				}

				if (InputError != VRInputError_None)
				{
					return false;
				}
			}
		}
		else if (MotionSource.IsEqual(TEXT("Right")) || MotionSource.IsEqual(TEXT("EControllerHand::Right")))
		{
			if (RightActionHandle != vr::k_ulInvalidActionHandle)
			{
				if (GlobalPredictedSecondsFromNow <= -9999.f)
				{
					InputError = VRInput()->GetPoseActionDataForNextFrame(RightActionHandle, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
				}
				else
				{
					InputError = VRInput()->GetPoseActionDataRelativeToNow(RightActionHandle, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
				}

				if (InputError != VRInputError_None)
				{
					return false;
				}
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Camera")))
		{
			if (VRTRackerCamera == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTRackerCamera, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTRackerCamera, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Chest")))
		{
			if (VRTrackerChest == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerChest, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerChest, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Waist")))
		{
			if (VRTrackerWaist == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerWaist, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerWaist, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Foot_Left")))
		{
			if (VRTrackerFootL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerFootL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerFootL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Foot_Right")))
		{
			if (VRTrackerFootR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerFootR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerFootR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Shoulder_Left")))
		{
			if (VRTrackerShoulderL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerShoulderL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerShoulderL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Shoulder_Right")))
		{
			if (VRTrackerShoulderR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerShoulderR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerShoulderR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Elbow_Left")))
		{
			if (VRTrackerElbowL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerElbowL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerElbowL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Elbow_Right")))
		{
			if (VRTrackerElbowR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerElbowR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerElbowR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Knee_Left")))
		{
			if (VRTrackerKneeL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKneeL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKneeL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Knee_Right")))
		{
			if (VRTrackerKneeR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKneeR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKneeR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_RawPose_Left")))
		{
			if (VRTrackerHandedPoseL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedPoseL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedPoseL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}

		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_RawPose_Right")))
		{
			if (VRTrackerHandedPoseR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedPoseR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedPoseR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Back_Left")))
		{
			if (VRTrackerHandedBackL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedBackL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedBackL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Back_Right")))
		{
			if (VRTrackerHandedBackR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedBackR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedBackR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Front_Left")))
		{
			if (VRTrackerHandedFrontL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Front_Right")))
		{
			if (VRTrackerHandedFrontR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_FrontRolled_Left")))
		{
			if (VRTrackerHandedFrontRL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontRL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontRL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_FrontRolled_Right")))
		{
			if (VRTrackerHandedFrontRR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontRR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontRR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_PistolGrip_Left")))
		{
			if (VRTrackerHandedGripL == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedGripL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedGripL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_PistolGrip_Right")))
		{
			if (VRTrackerHandedGripR == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedGripR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedGripR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Keyboard")))
		{
			if (VRTrackerKeyboard == k_ulInvalidActionHandle)
			{
				return false;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKeyboard, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKeyboard, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		if (InputError == VRInputError_None)
		{
			// Get SteamVR Transform Matrix for this controller
			HmdMatrix34_t Matrix = PoseData.pose.mDeviceToAbsoluteTracking;

			// Transform SteamVR Pose to Unreal Pose
			FMatrix Pose = FMatrix(
				FPlane(Matrix.m[0][0], Matrix.m[1][0], Matrix.m[2][0], 0.0f),
				FPlane(Matrix.m[0][1], Matrix.m[1][1], Matrix.m[2][1], 0.0f),
				FPlane(Matrix.m[0][2], Matrix.m[1][2], Matrix.m[2][2], 0.0f),
				FPlane(Matrix.m[0][3], Matrix.m[1][3], Matrix.m[2][3], 1.0f)
			);


			// Transform SteamVR Rotation Quaternion to a UE FRotator
			FQuat OrientationQuat;
			FQuat Orientation(Pose);
			OrientationQuat.X = -Orientation.Z;
			OrientationQuat.Y = Orientation.X;
			OrientationQuat.Z = Orientation.Y;
			OrientationQuat.W = -Orientation.W;

			// Return controller transform
			FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1])) * WorldToMetersScale - CachedBasePosition);
			OutPosition = CachedBaseOrientation.Inverse().RotateVector(Position);

			OrientationQuat = CachedBaseOrientation.Inverse() * OrientationQuat;
			OrientationQuat.Normalize();
			OutOrientation = OrientationQuat.Rotator();

		}
	}

	return true;
}

bool FSteamVRInputDevice::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	return GetControllerOrientationAndPosition(ControllerIndex, GetMotionSourceName(DeviceHand), OutOrientation, OutPosition, WorldToMetersScale);
}

ETrackingStatus FSteamVRInputDevice::GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;
	//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("STATUS MOTION SOURCE: %s"), *MotionSource.ToString());

	if (VRInput() && VRCompositor())
	{
		//FName MotionSource = GetMotionSourceName(DeviceHand);
		InputPoseActionData_t PoseData = {};
		EVRInputError InputError = VRInputError_NoData;

		if (MotionSource.IsEqual(TEXT("Left")) || MotionSource.IsEqual(TEXT("EControllerHand::Left")))
		{
			if (VRControllerHandleLeft == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRControllerHandleLeft, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRControllerHandleLeft, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Right")) || MotionSource.IsEqual(TEXT("EControllerHand::Right")))
		{
			if (VRControllerHandleRight == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRControllerHandleRight, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRControllerHandleRight, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Camera")))
		{
			if (VRTRackerCamera == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTRackerCamera, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTRackerCamera, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Chest")))
		{
			if (VRTrackerChest == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerChest, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerChest, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Waist")))
		{
			if (VRTrackerWaist == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerWaist, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerWaist, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Foot_Left")))
		{
			if (VRTrackerFootL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerFootL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerFootL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Foot_Right")))
		{
			if (VRTrackerFootR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerFootR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerFootR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Shoulder_Left")))
		{
			if (VRTrackerShoulderL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerShoulderL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerShoulderL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Shoulder_Right")))
		{
			if (VRTrackerShoulderR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerShoulderR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerShoulderR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Elbow_Left")))
		{
			if (VRTrackerElbowL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerElbowL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerElbowL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Elbow_Right")))
		{
			if (VRTrackerElbowR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerElbowR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerElbowR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Knee_Left")))
		{
			if (VRTrackerKneeL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKneeL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKneeL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Knee_Right")))
		{
			if (VRTrackerKneeR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKneeR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKneeR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_RawPose_Left")))
		{
			if (VRTrackerHandedPoseL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedPoseL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedPoseL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_RawPose_Right")))
		{
			if (VRTrackerHandedPoseR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedPoseR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedPoseR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Back_Left")))
		{
			if (VRTrackerHandedBackL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedBackL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedBackL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Back_Right")))
		{
			if (VRTrackerHandedBackR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedBackR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedBackR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Front_Left")))
		{
			if (VRTrackerHandedFrontL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_Front_Right")))
		{
			if (VRTrackerHandedFrontR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_FrontRolled_Left")))
		{
			if (VRTrackerHandedFrontRL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontRL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontRL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_FrontRolled_Right")))
		{
			if (VRTrackerHandedFrontRR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedFrontRR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedFrontRR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_PistolGrip_Left")))
		{
			if (VRTrackerHandedGripL == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedGripL, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedGripL, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Handheld_PistolGrip_Right")))
		{
			if (VRTrackerHandedGripR == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerHandedGripR, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerHandedGripR, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}
		else if (MotionSource.IsEqual(TEXT("Tracker_Keyboard")))
		{
			if (VRTrackerKeyboard == k_ulInvalidActionHandle)
			{
				return ETrackingStatus::NotTracked;
			}

			if (GlobalPredictedSecondsFromNow <= -9999.f)
			{
				InputError = VRInput()->GetPoseActionDataForNextFrame(VRTrackerKeyboard, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}
			else
			{
				InputError = VRInput()->GetPoseActionDataRelativeToNow(VRTrackerKeyboard, VRCompositor()->GetTrackingSpace(), GlobalPredictedSecondsFromNow, &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
			}

			if (InputError != VRInputError_None)
			{
				return ETrackingStatus::NotTracked;
			}
		}

		if (InputError == VRInputError_None && PoseData.pose.bDeviceIsConnected)
		{
			TrackingStatus = ETrackingStatus::Tracked;
		}
	}

	return TrackingStatus;
}

ETrackingStatus FSteamVRInputDevice::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	return GetControllerTrackingStatus(ControllerIndex, GetMotionSourceName(DeviceHand));
}

FName FSteamVRInputDevice::GetMotionControllerDeviceTypeName() const
{
	return FName(TEXT("SteamVRInputDevice"));
}

bool FSteamVRInputDevice::GetHandJointPosition(const FName MotionSource, int jointIndex, FVector& OutPosition) const
{
	return false;
}


FName FSteamVRInputDevice::GetMotionSourceName(const EControllerHand MotionSource) const
{
	switch ((uint32)MotionSource)
	{
	case 0:
		return FName(TEXT("Left"));
		break;
	case 1:
		return FName(TEXT("Right"));
		break;
	case 2:
		return FName(TEXT("AnyHand"));
		break;
	case 3:
		return FName(TEXT("Pad"));
		break;
	case 4:
		return FName(TEXT("ExternalCamera"));
		break;
	case 5:
		return FName(TEXT("Gun"));
		break;
	case 6:
		return FName(TEXT("Special_1"));
		break;
	case 7:
		return FName(TEXT("Special_2"));
		break;
	case 8:
		return FName(TEXT("Special_3"));
		break;
	case 9:
		return FName(TEXT("Special_4"));
		break;
	case 10:
		return FName(TEXT("Special_5"));
		break;
	case 11:
		return FName(TEXT("Special_6"));
		break;
	case 12:
		return FName(TEXT("Special_7"));
		break;
	case 13:
		return FName(TEXT("Special_8"));
		break;
	case 14:
		return FName(TEXT("Special_9"));
		break;
	case 15:
		return FName(TEXT("Special_10"));
		break;
	case 16:
		return FName(TEXT("Special_11"));
		break;
	case 17:
		return FName(TEXT("Tracker_Camera"));
		break;
	case 18:
		return FName(TEXT("Tracker_Chest"));
		break;
	case 19:
		return FName(TEXT("Tracker_Handheld_Back_Left"));
		break;
	case 20:
		return FName(TEXT("Tracker_Handheld_Back_Right"));
		break;
	case 21:
		return FName(TEXT("Tracker_Handheld_Front_Left"));
		break;
	case 22:
		return FName(TEXT("Tracker_Handheld_Front_Right"));
		break;
	case 23:
		return FName(TEXT("Tracker_Handheld_FrontRolled_Left"));
		break;
	case 24:
		return FName(TEXT("Tracker_Handheld_FrontRolled_Right"));
		break;
	case 25:
		return FName(TEXT("Tracker_Handheld_PistolGrip_Left"));
		break;
	case 26:
		return FName(TEXT("Tracker_Handheld_PistolGrip_Right"));
		break;
	case 27:
		return FName(TEXT("Tracker_Handheld_RawPose_Left"));
		break;
	case 28:
		return FName(TEXT("Tracker_Handheld_RawPose_Right"));
		break;
	case 29:
		return FName(TEXT("Tracker_Foot_Left"));
		break;
	case 30:
		return FName(TEXT("Tracker_Foot_Right"));
		break;
	case 31:
		return FName(TEXT("Tracker_Shoulder_Left"));
		break;
	case 32:
		return FName(TEXT("Tracker_Shoulder_Right"));
		break;
	case 33:
		return FName(TEXT("Tracker_Keyboard"));
		break;
	case 34:
		return FName(TEXT("Tracker_Waist"));
		break;
	case 35:
		return FName(TEXT("Tracker_Elbow_Left"));
		break;
	case 36:
		return FName(TEXT("Tracker_Elbow_Right"));
		break;
	case 37:
		return FName(TEXT("Tracker_Knee_Left"));
		break;
	case 38:
		return FName(TEXT("Tracker_Knee_Right"));
		break;
	default:
		return NAME_None;
		break;
	}
}

void FSteamVRInputDevice::EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const
{
	SourcesOut.Add(FMotionControllerSource(TEXT("Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("AnyHand")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Pad")));
	SourcesOut.Add(FMotionControllerSource(TEXT("ExternalCamera")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Gun")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Camera")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Chest")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Elbow_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Elbow_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Foot_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Foot_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Knee_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Knee_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Shoulder_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Shoulder_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Keyboard")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Waist")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_RawPose_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_RawPose_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_Back_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_Back_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_Front_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_Front_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_FrontRolled_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_FrontRolled_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_PistolGrip_Left")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Tracker_Handheld_PistolGrip_Right")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_1")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_2")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_3")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_4")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_5")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_6")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_7")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_8")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_9")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_10")));
	SourcesOut.Add(FMotionControllerSource(TEXT("Special_11")));
}

void FSteamVRInputDevice::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	VRActionHandle_t VibrationAction = k_ulInvalidActionHandle;

	switch (Hand)
	{
	case (int32)EControllerHand::Left:
		VibrationAction = VRVibrationLeft;
		break;
	case (int32)EControllerHand::Right:
		VibrationAction = VRVibrationRight;
		break;
	case (int32)EControllerHand::AnyHand:
		VibrationAction = VRVibrationLeft;	// UE4.17+: Hardwire AnyHand to OpenVR's left path as it is the lowest device id to cover most use cases without triggering a duplicate vibration/rumble. TODO: May need refactor for cases where there's a left hand device set & right hand was set to AnyHand for some reason.
		break;
	}

	if (VRSystem() && VRInput() && VibrationAction != k_ulInvalidActionHandle)
	{
		VRInput()->TriggerHapticVibrationAction(VibrationAction, 0.f, CurrentDeltaTime, Values.Frequency, Values.Amplitude, k_ulInvalidInputValueHandle);
		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("[HAPTIC] Hand: %i, Duration: %f, Frequency: %f, Amplitude: %f"), Hand, CurrentDeltaTime, Values.Frequency, Values.Amplitude);
	}
}

void FSteamVRInputDevice::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = MaxFrequency = 0.f;
}

float FSteamVRInputDevice::GetHapticAmplitudeScale() const
{
	return 1.f;
}

void FSteamVRInputDevice::GetControllerFidelity()
{
	if (VRInput() && VRCompositor())
	{
		InputPoseActionData_t PoseData = {};
		EVRInputError InputError = VRInputError_NoData;

		if (VRControllerHandleLeft == k_ulInvalidActionHandle)
		{
			return;
		}

		InputError = VRInput()->GetPoseActionDataForNextFrame(VRControllerHandleLeft, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);

		if (InputError != VRInputError_None)
		{
			return;
		}

		if (PoseData.bActive && PoseData.pose.bDeviceIsConnected)
		{
			if (VRSkeletalHandleLeft == k_ulInvalidActionHandle)
			{
				return;
			}

			InputError = VRInput()->GetSkeletalTrackingLevel(VRSkeletalHandleLeft, &LeftControllerFidelity);

			if (InputError != VRInputError_None)
			{
				return;
			}

			bIsSkeletalControllerLeftPresent = (LeftControllerFidelity >= VRSkeletalTracking_Partial);
		}
		else
		{
			bIsSkeletalControllerLeftPresent = false;
			LeftControllerFidelity = EVRSkeletalTrackingLevel::VRSkeletalTracking_Estimated;
		}

		if (VRControllerHandleRight == k_ulInvalidActionHandle)
		{
			return;
		}

		InputError = VRInput()->GetPoseActionDataForNextFrame(VRControllerHandleRight, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);
		if (PoseData.bActive && PoseData.pose.bDeviceIsConnected)
		{
			if (VRSkeletalHandleRight == k_ulInvalidActionHandle)
			{
				return;
			}

			VRInput()->GetSkeletalTrackingLevel(VRSkeletalHandleRight, &RightControllerFidelity);

			if (InputError != VRInputError_None)
			{
				return;
			}

			bIsSkeletalControllerRightPresent = (RightControllerFidelity >= VRSkeletalTracking_Partial);
		}
		else
		{
			bIsSkeletalControllerRightPresent = false;
			RightControllerFidelity = EVRSkeletalTrackingLevel::VRSkeletalTracking_Estimated;
		}
	}
}

void FSteamVRInputDevice::GetLeftHandPoseData(FVector& Position, FRotator& Orientation, FVector& AngularVelocity, FVector& Velocity)
{
	InputPoseActionData_t PoseData = {};
	EVRInputError InputError = VRInputError_NoData;

	if (bIsSkeletalControllerRightPresent && VRInput())
	{
		if (VRSkeletalHandleLeft == k_ulInvalidActionHandle)
		{
			return;
		}

		InputError = VRInput()->GetPoseActionDataForNextFrame(VRSkeletalHandleLeft, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);

		if (InputError != VRInputError_None || VRSkeletalHandleLeft == k_ulInvalidActionHandle)
		{
			return;
		}

		if (PoseData.bActive && PoseData.pose.bDeviceIsConnected && InputError == VRInputError_None)
		{
			GetUETransform(PoseData, Position, Orientation);
			AngularVelocity = FVector(
				PoseData.pose.vAngularVelocity.v[2],
				-PoseData.pose.vAngularVelocity.v[0],
				PoseData.pose.vAngularVelocity.v[1]
			);
			Velocity = FVector(
				PoseData.pose.vVelocity.v[2],
				-PoseData.pose.vVelocity.v[0],
				PoseData.pose.vVelocity.v[1]
			);
		}
	}
}

void FSteamVRInputDevice::GetRightHandPoseData(FVector& Position, FRotator& Orientation, FVector& AngularVelocity, FVector& Velocity)
{
	InputPoseActionData_t PoseData = {};
	EVRInputError InputError = VRInputError_NoData;

	if (bIsSkeletalControllerRightPresent && VRInput())
	{
		if (VRSkeletalHandleRight == k_ulInvalidActionHandle)
		{
			return;
		}

		InputError = VRInput()->GetPoseActionDataForNextFrame(VRSkeletalHandleRight, VRCompositor()->GetTrackingSpace(), &PoseData, sizeof(PoseData), k_ulInvalidInputValueHandle);

		if (InputError != VRInputError_None)
		{
			return;
		}

		if (PoseData.bActive && PoseData.pose.bDeviceIsConnected && InputError == VRInputError_None)
		{
			GetUETransform(PoseData, Position, Orientation);
			AngularVelocity = FVector(
				PoseData.pose.vAngularVelocity.v[2],
				-PoseData.pose.vAngularVelocity.v[0],
				PoseData.pose.vAngularVelocity.v[1]
			);
			Velocity = FVector(
				PoseData.pose.vVelocity.v[2],
				-PoseData.pose.vVelocity.v[0],
				PoseData.pose.vVelocity.v[1]
			);
		}
	}
}

void FSteamVRInputDevice::GetUETransform(InputPoseActionData_t PoseData, FVector& OutPosition, FRotator& OutOrientation)
{
	// Get SteamVR Transform Matrix for this skeleton
	HmdMatrix34_t Matrix = PoseData.pose.mDeviceToAbsoluteTracking;

	// Transform SteamVR Pose to Unreal Pose
	FMatrix Pose = FMatrix(
		FPlane(Matrix.m[0][0], Matrix.m[1][0], Matrix.m[2][0], 0.0f),
		FPlane(Matrix.m[0][1], Matrix.m[1][1], Matrix.m[2][1], 0.0f),
		FPlane(Matrix.m[0][2], Matrix.m[1][2], Matrix.m[2][2], 0.0f),
		FPlane(Matrix.m[0][3], Matrix.m[1][3], Matrix.m[2][3], 1.0f)
	);


	// Transform SteamVR Rotation Quaternion to a UE FRotator
	FQuat OrientationQuat;
	FQuat Orientation(Pose);
	OrientationQuat.X = -Orientation.Z;
	OrientationQuat.Y = Orientation.X;
	OrientationQuat.Z = Orientation.Y;
	OrientationQuat.W = -Orientation.W;


	FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1])) * GWorld->GetWorldSettings()->WorldToMeters);
	OutPosition = Position;

	//OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
	OutOrientation = OrientationQuat.Rotator();
}

void FSteamVRInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// Empty on purpose
}

void FSteamVRInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values)
{
	// Empty on purpose
}

void FSteamVRInputDevice::InitControllerMappings()
{
	for (unsigned int i = 0; i < k_unMaxTrackedDeviceCount; ++i)
	{
		DeviceToControllerMap[i] = INDEX_NONE;
	}

	for (unsigned int id = 0; id < SteamVRInputDeviceConstants::MaxUnrealControllers; ++id)
	{
		for (unsigned int hand = 0; hand < k_unMaxTrackedDeviceCount; ++hand)
		{
			UnrealControllerIdAndHandToDeviceIdMap[id][hand] = INDEX_NONE;
		}
	}

	for (int32& HandCount : MaxUEHandCount)
	{
		HandCount = 0;
	}
}

void FSteamVRInputDevice::InitControllerKeys()
{
	EKeys::AddMenuCategoryDisplayInfo("SteamVRInput", LOCTEXT("SteamVRInputSubCategory", "SteamVR Input"), TEXT("GraphEditor.PadEvent_16x"));

#pragma region GENERIC KEYS
	EKeys::AddKey(FKeyDetails(GenericKeys::SteamVR_MotionController_None, LOCTEXT("SteamVR_MotionController_None", "SteamVR Generic Key"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "SteamVRInput"));
	EKeys::AddKey(FKeyDetails(GenericKeys::SteamVR_HMD_Proximity, LOCTEXT("SteamVR_HMD_Proximity", "SteamVR HMD Proximity"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "SteamVRInput"));
#pragma endregion

#pragma region INDEX CONTROLLER
	EKeys::AddKey(FKeyDetails(IndexControllerKeys::ValveIndex_Left_Pinch_Grab, LOCTEXT("ValveIndex_Left_Pinch_Grab", "Valve Index (L) Pinch Grab"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(IndexControllerKeys::ValveIndex_Right_Pinch_Grab, LOCTEXT("ValveIndex_Right_Pinch_Grab", "Valve Index (R) Pinch Grab"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));

	EKeys::AddKey(FKeyDetails(IndexControllerKeys::ValveIndex_Left_Grip_Grab, LOCTEXT("ValveIndex_Left_Grip_Grab", "Valve Index (L) Grip Grab"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(IndexControllerKeys::ValveIndex_Right_Grip_Grab, LOCTEXT("ValveIndex_Right_Grip_Grab", "Valve Index (R) Grip Grab"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
#pragma endregion

#pragma region COSMOS KEYS
	EKeys::AddMenuCategoryDisplayInfo("Cosmos", LOCTEXT("CosmosSubCategory", "HTC Cosmos"), TEXT("GraphEditor.PadEvent_16x"));

	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_X_Click, LOCTEXT("Cosmos_Left_X_Click", "Cosmos (L) X Press"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Y_Click, LOCTEXT("Cosmos_Left_Y_Click", "Cosmos (L) Y Press"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_X_Touch, LOCTEXT("Cosmos_Left_X_Touch", "Cosmos (L) X Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Y_Touch, LOCTEXT("Cosmos_Left_Y_Touch", "Cosmos (L) Y Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Menu_Click, LOCTEXT("Cosmos_Left_Menu_Click", "Cosmos (L) Menu"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Grip_Click, LOCTEXT("Cosmos_Left_Grip_Click", "Cosmos (L) Grip"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Grip_Axis, LOCTEXT("Cosmos_Left_Grip_Axis", "Cosmos (L) Grip Axis"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Trigger_Click, LOCTEXT("Cosmos_Left_Trigger_Click", "Cosmos (L) Trigger"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Trigger_Axis, LOCTEXT("Cosmos_Left_Trigger_Axis", "Cosmos (L) Trigger Axis"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Trigger_Touch, LOCTEXT("Cosmos_Left_Trigger_Touch", "Cosmos (L) Trigger Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Vector, LOCTEXT("Cosmos_Left_Thumbstick_Vector", "Cosmos (L) Thumbstick Vector"), FKeyDetails::GamepadKey | FKeyDetails::Axis2D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_X, LOCTEXT("Cosmos_Left_Thumbstick_X", "Cosmos (L) Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Y, LOCTEXT("Cosmos_Left_Thumbstick_Y", "Cosmos (L) Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Click, LOCTEXT("Cosmos_Left_Thumbstick_Click", "Cosmos (L) Thumbstick"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Bumper_Click, LOCTEXT("Cosmos_Left_Bumper_Click", "Cosmos (L) Bumper"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Touch, LOCTEXT("Cosmos_Left_Thumbstick_Touch", "Cosmos (L) Thumbstick Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_A_Click, LOCTEXT("Cosmos_Right_A_Click", "Cosmos (R) A Press"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_B_Click, LOCTEXT("Cosmos_Right_B_Click", "Cosmos (R) B Press"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_A_Touch, LOCTEXT("Cosmos_Right_A_Touch", "Cosmos (R) A Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_B_Touch, LOCTEXT("Cosmos_Right_B_Touch", "Cosmos (R) B Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_System_Click, LOCTEXT("Cosmos_Right_System_Click", "Cosmos (R) System"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Grip_Click, LOCTEXT("Cosmos_Right_Grip_Click", "Cosmos (R) Grip"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Grip_Axis, LOCTEXT("Cosmos_Right_Grip_Axis", "Cosmos (R) Grip Axis"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Trigger_Click, LOCTEXT("Cosmos_Right_Trigger_Click", "Cosmos (R) Trigger"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Trigger_Axis, LOCTEXT("Cosmos_Right_Trigger_Axis", "Cosmos (R) Trigger Axis"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Trigger_Touch, LOCTEXT("Cosmos_Right_Trigger_Touch", "Cosmos (R) Trigger Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Vector, LOCTEXT("Cosmos_Right_Thumbstick_Vector", "Cosmos (R) Thumbstick Vector"), FKeyDetails::GamepadKey | FKeyDetails::Axis2D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_X, LOCTEXT("Cosmos_Right_Thumbstick_X", "Cosmos (R) Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Y, LOCTEXT("Cosmos_Right_Thumbstick_Y", "Cosmos (R) Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::Axis1D | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Click, LOCTEXT("Cosmos_Right_Thumbstick_Click", "Cosmos (R) Thumbstick"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Touch, LOCTEXT("Cosmos_Right_Thumbstick_Touch", "Cosmos (R) Thumbstick Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Bumper_Click, LOCTEXT("Cosmos_Right_Bumper_Click", "Cosmos (R) Bumper"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));

	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Up, LOCTEXT("Cosmos_Left_Thumbstick_Up", "Cosmos (L) Thumbstick Up"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Down, LOCTEXT("Cosmos_Left_Thumbstick_Down", "Cosmos (L) Thumbstick Down"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Left, LOCTEXT("Cosmos_Left_Thumbstick_Left", "Cosmos (L) Thumbstick Left"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Left_Thumbstick_Right, LOCTEXT("Cosmos_Left_Thumbstick_Right", "Cosmos (L) Thumbstick Right"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));

	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Up, LOCTEXT("Cosmos_Right_Thumbstick_Up", "Cosmos (R) Thumbstick Up"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Down, LOCTEXT("Cosmos_Right_Thumbstick_Down", "Cosmos (R) Thumbstick Down"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Left, LOCTEXT("Cosmos_Right_Thumbstick_Left", "Cosmos (R) Thumbstick Left"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
	EKeys::AddKey(FKeyDetails(CosmosKeys::Cosmos_Right_Thumbstick_Right, LOCTEXT("Cosmos_Right_Thumbstick_Right", "Cosmos (R) Thumbstick Right"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Cosmos"));
#pragma endregion

#pragma region INPUT KEYS (Additional non-standard OpenXR keys)
	// Valve Index - Additional input keys not implemented in OpenXR yet
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Left_Trackpad_Up_Touch, LOCTEXT("ValveIndex_Left_Trackpad_Up_Touch", "Valve Index (L) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Left_Trackpad_Down_Touch, LOCTEXT("ValveIndex_Left_Trackpad_Down_Touch", "Valve Index (L) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Left_Trackpad_Left_Touch, LOCTEXT("ValveIndex_Left_Trackpad_Left_Touch", "Valve Index (L) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Left_Trackpad_Right_Touch, LOCTEXT("ValveIndex_Left_Trackpad_Right_Touch", "Valve Index (L) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));

	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Right_Trackpad_Up_Touch, LOCTEXT("ValveIndex_Right_Trackpad_Up_Touch", "Valve Index (R) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Right_Trackpad_Down_Touch, LOCTEXT("ValveIndex_Right_Trackpad_Down_Touch", "Valve Index (R) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Right_Trackpad_Left_Touch, LOCTEXT("ValveIndex_Right_Trackpad_Left_Touch", "Valve Index (R) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));
	EKeys::AddKey(FKeyDetails(InputKeys::ValveIndex_Right_Trackpad_Right_Touch, LOCTEXT("ValveIndex_Right_Trackpad_Right_Touch", "Valve Index (R) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "ValveIndex"));

	// HTC Vive - Additional input keys not implemented in OpenXR yet
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Left_Trackpad_Up_Touch, LOCTEXT("Vive_Left_Trackpad_Up", "Vive (L) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Left_Trackpad_Down_Touch, LOCTEXT("Vive_Left_Trackpad_Down", "Vive (L) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Left_Trackpad_Left_Touch, LOCTEXT("Vive_Left_Trackpad_Left", "Vive (L) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Left_Trackpad_Right_Touch, LOCTEXT("Vive_Left_Trackpad_Right", "Vive (L) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));

	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Right_Trackpad_Up_Touch, LOCTEXT("Vive_Right_Trackpad_Up", "Vive (R) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Right_Trackpad_Down_Touch, LOCTEXT("Vive_Right_Trackpad_Down", "Vive (R) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Right_Trackpad_Left_Touch, LOCTEXT("Vive_Right_Trackpad_Left", "Vive (R) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));
	EKeys::AddKey(FKeyDetails(InputKeys::Vive_Right_Trackpad_Right_Touch, LOCTEXT("Vive_Right_Trackpad_Right", "Vive (R) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "Vive"));

	// Windows Mixed Reality - Additional input keys not implemented in OpenXR yet
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Left_Trackpad_Up_Touch, LOCTEXT("MixedReality_Left_Trackpad_Up_Touch", "Mixed Reality (L) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Left_Trackpad_Down_Touch, LOCTEXT("MixedReality_Left_Trackpad_Down_Touch", "Mixed Reality (L) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Left_Trackpad_Left_Touch, LOCTEXT("MixedReality_Left_Trackpad_Left_Touch", "Mixed Reality (L) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Left_Trackpad_Right_Touch, LOCTEXT("MixedReality_Left_Trackpad_Right_Touch", "Mixed Reality (L) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));

	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Right_Trackpad_Up_Touch, LOCTEXT("MixedReality_Right_Trackpad_Up_Touch", "Mixed Reality (R) Trackpad Up Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Right_Trackpad_Down_Touch, LOCTEXT("MixedReality_Right_Trackpad_Down_Touch", "Mixed Reality (R) Trackpad Down Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Right_Trackpad_Left_Touch, LOCTEXT("MixedReality_Right_Trackpad_Left_Touch", "Mixed Reality (R) Trackpad Left Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
	EKeys::AddKey(FKeyDetails(InputKeys::MixedReality_Right_Trackpad_Right_Touch, LOCTEXT("MixedReality_Right_Trackpad_Right_Touch", "Mixed Reality (R) Trackpad Right Touch"), FKeyDetails::GamepadKey | FKeyDetails::NotBlueprintBindableKey, "MixedReality"));
#pragma endregion

}

#if WITH_EDITOR
void FSteamVRInputDevice::RegenerateActionManifest()
{
	this->GenerateActionManifest(true, false, true, true);
}

void FSteamVRInputDevice::RegenerateControllerBindings()
{
	this->GenerateActionManifest(false, true, true, true);
}

void FSteamVRInputDevice::OnActionMappingsChanged()
{
	this->GenerateActionManifest(true, true, true, true);
}

void FSteamVRInputDevice::OnVREditingModeEnter()
{
	// Set Input Settings
	auto InputSettings = GetMutableDefault<UInputSettings>();

	UVREditorMode* EditorMode = IVREditorModule::Get().GetVRMode();
	if (!EditorMode)
	{
		return;
	}

	for (UVREditorInteractor* Interactor : EditorMode->GetVRInteractors())
	{
		FName Hand = Interactor->GetControllerHandSide();
		for (const auto& KeyToAction : Interactor->GetKeyToActionMap())
		{
			const FKey Key = KeyToAction.Key;
			if (Key.GetFName().ToString().StartsWith("MotionController"))
			{
				continue;
			}

			const FName Action = KeyToAction.Value.ActionType;
			FName CombinedName = FName("VREditor_" + Hand.ToString() + "_" + Action.ToString());

			if (Key.IsAxis1D())
			{
				InputSettings->AddAxisMapping(FInputAxisKeyMapping(CombinedName, Key), false);
			}
			else
			{
				InputSettings->AddActionMapping(FInputActionKeyMapping(CombinedName, Key), false);
			}
		}
	}

	InputSettings->ForceRebuildKeymaps();

	this->GenerateActionManifest(true, true, true, true);
}

void FSteamVRInputDevice::OnVREditingModeExit()
{
	// Set Input Settings
	auto InputSettings = GetMutableDefault<UInputSettings>();

	UVREditorMode* EditorMode = IVREditorModule::Get().GetVRMode();
	if (!EditorMode)
	{
		return;
	}

	for (UVREditorInteractor* Interactor : EditorMode->GetVRInteractors())
	{
		FName Hand = Interactor->GetControllerHandSide();
		for (const auto& KeyToAction : Interactor->GetKeyToActionMap())
		{
			const FKey Key = KeyToAction.Key;
			if (Key.GetFName().ToString().StartsWith("MotionController"))
			{
				continue;
			}

			const FName Action = KeyToAction.Value.ActionType;
			FName CombinedName = FName("VREditor_" + Hand.ToString() + "_" + Action.ToString());

			if (Key.IsAxis1D())
			{
				InputSettings->RemoveAxisMapping(FInputAxisKeyMapping(CombinedName, Key), false);
			}
			else
			{
				InputSettings->RemoveActionMapping(FInputActionKeyMapping(CombinedName, Key), false);
			}
		}
	}

	InputSettings->ForceRebuildKeymaps();

	this->GenerateActionManifest(true, true, true, true);
}

bool FSteamVRInputDevice::GenerateAppManifest(FString ManifestPath, FString ProjectName, FString& OutAppKey, FString& OutAppManifestPath)
{
	// Set SteamVR AppKey
	OutAppKey = (TEXT(APP_MANIFEST_PREFIX) + SanitizeString(GameProjectName) + TEXT(".") + ProjectName).ToLower();
	EditorAppKey = FString(OutAppKey);

	// Set Application Manifest Path - same directory where the action manifest will be
	OutAppManifestPath = FPaths::ProjectConfigDir() / APP_MANIFEST_FILE;
	IFileManager& FileManager = FFileManagerGeneric::Get();

	// Create Application Manifest json objects
	TSharedRef<FJsonObject> AppManifestObject = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> ManifestApps;

	// Add current engine version being used as source
	AppManifestObject->SetStringField("source", FString::Printf(TEXT("UE")));

	// Define the application setting that will be registered with SteamVR
	TArray<TSharedPtr<FJsonValue>> ManifestApp;

	// Create Application Object
	TSharedRef<FJsonObject> ApplicationObject = MakeShareable(new FJsonObject());
	TArray<FString> AppStringFields = { "app_key",  OutAppKey,
										"launch_type", "url",
										"url", "steam://launch/",
										"action_manifest_path", *IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath)
	};
	BuildJsonObject(AppStringFields, ApplicationObject);

	// Create localization object
	TSharedPtr<FJsonObject> LocStringsObject = MakeShareable(new FJsonObject());
	TSharedRef<FJsonObject> AppNameObject = MakeShareable(new FJsonObject());
	AppNameObject->SetStringField("name", GameProjectName + " [UE Editor]");
	LocStringsObject->SetObjectField("en_us", AppNameObject);
	ApplicationObject->SetObjectField("strings", LocStringsObject);

	// Assemble the json app manifest
	ManifestApps.Add(MakeShareable(new FJsonValueObject(ApplicationObject)));
	AppManifestObject->SetArrayField(TEXT("applications"), ManifestApps);

	// Serialize json app manifest
	FString AppManifestString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&AppManifestString);
	FJsonSerializer::Serialize(AppManifestObject, JsonWriter);

	// Save json as a UTF8 file
	if (!FFileHelper::SaveStringToFile(AppManifestString, *OutAppManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("Error trying to generate application manifest in: %s"), *OutAppManifestPath);
		return false;
	}

	return true;
}

void FSteamVRInputDevice::ReloadActionManifest()
{
	if (VRSystem() && VRInput() && VRApplications())
	{
		// Set Action Manifest Path
		const FString ManifestPath = FPaths::ProjectConfigDir() / CONTROLLER_BINDING_PATH / ACTION_MANIFEST;
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("Reloading Action Manifest in: %s"), *ManifestPath);

		// Load application manifest
		FString AppManifestPath = FPaths::ProjectConfigDir() / APP_MANIFEST_FILE;
		EVRApplicationError AppError = VRApplications()->AddApplicationManifest(TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*AppManifestPath)), true);
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Registering Application Manifest %s : %s"), *AppManifestPath, *FString(UTF8_TO_TCHAR(VRApplications()->GetApplicationsErrorNameFromEnum(AppError))));

		// Get the App Process Id
		uint32 AppProcessId = FPlatformProcess::GetCurrentProcessId();

		// Set SteamVR AppKey
		FString AppFileName = FPaths::GetCleanFilename(FPlatformProcess::GetApplicationName(AppProcessId));
		FString SteamVRAppKey = (TEXT(APP_MANIFEST_PREFIX) + SanitizeString(GameProjectName) + TEXT(".") + AppFileName).ToLower();

		// Set AppKey for this Editor Session
		AppError = VRApplications()->IdentifyApplication(AppProcessId, TCHAR_TO_UTF8(*SteamVRAppKey));
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Editor Application [%d][%s] identified to SteamVR: %s"), AppProcessId, *SteamVRAppKey, *FString(UTF8_TO_TCHAR(VRApplications()->GetApplicationsErrorNameFromEnum(AppError))));

		// Set Action Manifest
		EVRInputError InputError = VRInput()->SetActionManifestPath(TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath)));
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Reloading Action Manifest Path [%s]"), *ManifestPath);
		GetInputError(InputError, FString(TEXT("Setting Action Manifest Path")));
	}
}
#endif

void FSteamVRInputDevice::GenerateControllerBindings(const FString& BindingsPath, TArray<FControllerType>& InOutControllerTypes, TArray<TSharedPtr<FJsonValue>>& DefaultBindings, TArray<FSteamVRInputAction>& InActionsArray, TArray<FInputMapping>& InInputMapping, bool bDeleteIfExists)
{
	// Create the bindings directory if it doesn't exist
	IFileManager& FileManager = FFileManagerGeneric::Get();
	if (!FileManager.DirectoryExists(*BindingsPath))
	{
		FileManager.MakeDirectory(*BindingsPath);
	}

	// Go through all supported controller types
	for (auto& SupportedController : InOutControllerTypes)
	{
		// If there is no user-defined controller binding or it hasn't been auto-generated yet, generate it
		if (!SupportedController.bIsGenerated)
		{
			// Creating bindings file
			TSharedRef<FJsonObject> BindingsObject = MakeShareable(new FJsonObject());
			BindingsObject->SetStringField(TEXT("name"), TEXT("Default bindings for ") + SupportedController.Description);
			BindingsObject->SetStringField(TEXT("controller_type"), SupportedController.Name.ToString());
			BindingsObject->SetStringField(TEXT("last_edited_by"), FApp::GetEpicProductIdentifier());

			// Create Action Bindings in JSON Format
			TArray<TSharedPtr<FJsonValue>> JsonValuesArray;

			// Create Action Set
			TSharedRef<FJsonObject> ActionSetJsonObject = MakeShareable(new FJsonObject());

			// Do not generate any controller bindings for trackers
			if (!SupportedController.Description.Contains(TEXT("Vive Tracker")))
			{
				GenerateActionBindings(InInputMapping, JsonValuesArray, SupportedController);

				// Ensure we also handle generic UE4 Motion Controllers
				if (!SupportedController.Description.Contains(TEXT("Headset"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					FControllerType GenericController = FControllerType(TEXT("MotionController"), TEXT("MotionController"), TEXT("MotionController"));
					GenerateActionBindings(InInputMapping, JsonValuesArray, SupportedController, true);
				}

				ActionSetJsonObject->SetArrayField(TEXT("sources"), JsonValuesArray);
			}
			else
			{
				// Add tracker poses
				SetTrackerPose(SupportedController, ActionSetJsonObject, SupportedController.OutputPath, SupportedController.RawDriverPath);
			}

			// Do not add any default bindings for headsets and misc devices
			if (!SupportedController.Description.Contains(TEXT("Headset"))
				&& !SupportedController.KeyEquivalent.Equals(TEXT("SteamVR_Gamepads"))
				&& !SupportedController.KeyEquivalent.Contains(TEXT("SteamVR_Vive_Tracker"))
				)
			{
				// Add Controller Pose Mappings
				TArray<TSharedPtr<FJsonValue>> ControllerPoseArray;

				// Add Pose: Left Controller
				TSharedRef<FJsonObject> ControllerLeftJsonObject = MakeShareable(new FJsonObject());
				ControllerLeftJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_CONTROLLER_LEFT));
				ControllerLeftJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_CONT_RAW_LEFT));
				ControllerLeftJsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

				TSharedRef<FJsonValueObject> ControllerLeftJsonValueObject = MakeShareable(new FJsonValueObject(ControllerLeftJsonObject));
				ControllerPoseArray.Add(ControllerLeftJsonValueObject);

				// Add Pose: Right Controller
				TSharedRef<FJsonObject> ControllerRightJsonObject = MakeShareable(new FJsonObject());
				ControllerRightJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_CONTROLLER_RIGHT));
				ControllerRightJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_CONT_RAW_RIGHT));
				ControllerLeftJsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

				TSharedRef<FJsonValueObject> ControllerRightJsonValueObject = MakeShareable(new FJsonValueObject(ControllerRightJsonObject));
				ControllerPoseArray.Add(ControllerRightJsonValueObject);

				// Add Controller Input Array To Action Set
				ActionSetJsonObject->SetArrayField(TEXT("poses"), ControllerPoseArray);

				// Add Skeleton Mappings
				TArray<TSharedPtr<FJsonValue>> SkeletonValuesArray;

				// Add Skeleton: Left Hand
				TSharedRef<FJsonObject> SkeletonLeftJsonObject = MakeShareable(new FJsonObject());
				SkeletonLeftJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_SKELETON_LEFT));
				SkeletonLeftJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_SKEL_LEFT));

				TSharedRef<FJsonValueObject> SkeletonLeftJsonValueObject = MakeShareable(new FJsonValueObject(SkeletonLeftJsonObject));
				SkeletonValuesArray.Add(SkeletonLeftJsonValueObject);

				// Add Skeleton: Right Hand
				TSharedRef<FJsonObject> SkeletonRightJsonObject = MakeShareable(new FJsonObject());
				SkeletonRightJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_SKELETON_RIGHT));
				SkeletonRightJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_SKEL_RIGHT));

				TSharedRef<FJsonValueObject> SkeletonRightJsonValueObject = MakeShareable(new FJsonValueObject(SkeletonRightJsonObject));
				SkeletonValuesArray.Add(SkeletonRightJsonValueObject);

				// Add Skeleton Input Array To Action Set
				ActionSetJsonObject->SetArrayField(TEXT("skeleton"), SkeletonValuesArray);

				// Add Haptic Mappings
				TArray<TSharedPtr<FJsonValue>> HapticValuesArray;

				// Add Haptic: Left Hand
				TSharedRef<FJsonObject> HapticLeftJsonObject = MakeShareable(new FJsonObject());
				HapticLeftJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_VIBRATE_LEFT));
				HapticLeftJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_VIB_LEFT));

				TSharedRef<FJsonValueObject> HapticLeftJsonValueObject = MakeShareable(new FJsonValueObject(HapticLeftJsonObject));
				HapticValuesArray.Add(HapticLeftJsonValueObject);

				// Add Haptic: Right Hand
				TSharedRef<FJsonObject> HapticRightJsonObject = MakeShareable(new FJsonObject());
				HapticRightJsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_VIBRATE_RIGHT));
				HapticRightJsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_USER_VIB_RIGHT));

				TSharedRef<FJsonValueObject> HapticRightJsonValueObject = MakeShareable(new FJsonValueObject(HapticRightJsonObject));
				HapticValuesArray.Add(HapticRightJsonValueObject);

				// Add Haptic Output Array To Action Set
				ActionSetJsonObject->SetArrayField(TEXT("haptics"), HapticValuesArray);
			}

			// Create Bindings File that includes all Action Sets
			TSharedRef<FJsonObject> BindingsJsonObject = MakeShareable(new FJsonObject());
			BindingsJsonObject->SetObjectField(TEXT(ACTION_SET), ActionSetJsonObject);
			BindingsObject->SetObjectField(TEXT("bindings"), BindingsJsonObject);

			// Set description of Bindings file to the Project Name
			BindingsObject->SetStringField(TEXT("description"), GameProjectName);

			// Set Bindings File Path
			FString BindingsFilePath = BindingsPath / SupportedController.Name.ToString() + TEXT(".json");

			// Delete if it exists
			if (FileManager.FileExists(*BindingsFilePath) && bDeleteIfExists)
			{
				FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*BindingsFilePath);
			}

			// Save controller binding
			FString OutputJsonString;
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
			FJsonSerializer::Serialize(BindingsObject, JsonWriter);
			FFileHelper::SaveStringToFile(OutputJsonString, *BindingsFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

			// Create Controller Binding Object for this binding file
			TSharedRef<FJsonObject> ControllerBindingObject = MakeShareable(new FJsonObject());
			TArray<FString> ControllerStringFields = { "controller_type", *SupportedController.Name.ToString(),
											 TEXT("binding_url"), *(SupportedController.Name.ToString() + TEXT(".json")) //*FileManager.ConvertToAbsolutePathForExternalAppForRead(*BindingsFilePath)
			};
			BuildJsonObject(ControllerStringFields, ControllerBindingObject);
			DefaultBindings.Add(MakeShareable(new FJsonValueObject(ControllerBindingObject)));

			// Tag this controller as generated
			SupportedController.bIsGenerated = true;
		}
	}
}

void FSteamVRInputDevice::SetTrackerPose(FControllerType& TrackerType, TSharedRef<FJsonObject> ActionSetJsonObject, const FString& TrackerOutput, const FString& TrackerPath)
{
	TArray<TSharedPtr<FJsonValue>> TrackerPoseArray;

	if (TrackerType.Name.IsEqual(TEXT("vive_tracker_handed")) || TrackerType.Name.IsEqual(TEXT("vive_tracker")))
	{
		// Add Pose: Handed, Pose Left
		TSharedRef<FJsonObject> Special1JsonObject = MakeShareable(new FJsonObject());
		Special1JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_POSE_LEFT));
		Special1JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_CONT_RAW_LEFT));
		Special1JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special1JsonValueObject = MakeShareable(new FJsonValueObject(Special1JsonObject));
		TrackerPoseArray.Add(Special1JsonValueObject);

		// Add Pose: Handed, Pose Right
		TSharedRef<FJsonObject> Special2JsonObject = MakeShareable(new FJsonObject());
		Special2JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_POSE_RIGHT));
		Special2JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_CONT_RAW_RIGHT));
		Special2JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special2JsonObjectJsonValueObject = MakeShareable(new FJsonValueObject(Special2JsonObject));
		TrackerPoseArray.Add(Special2JsonObjectJsonValueObject);

		// Add Pose: Handed, Back Left
		TSharedRef<FJsonObject> Special3JsonObject = MakeShareable(new FJsonObject());
		Special3JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_BACK_LEFT));
		Special3JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_BACK_LEFT));
		Special3JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special3JsonValueObject = MakeShareable(new FJsonValueObject(Special3JsonObject));
		TrackerPoseArray.Add(Special3JsonValueObject);

		// Add Pose: Handed, Back Right
		TSharedRef<FJsonObject> Special4JsonObject = MakeShareable(new FJsonObject());
		Special4JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_BACK_RIGHT));
		Special4JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_BACK_RIGHT));
		Special4JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special4JsonValueObject = MakeShareable(new FJsonValueObject(Special4JsonObject));
		TrackerPoseArray.Add(Special4JsonValueObject);

		// Add Pose: Handed, Front Left
		TSharedRef<FJsonObject> Special5JsonObject = MakeShareable(new FJsonObject());
		Special5JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_LEFT));
		Special5JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_FRONT_LEFT));
		Special5JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special5JsonValueObject = MakeShareable(new FJsonValueObject(Special5JsonObject));
		TrackerPoseArray.Add(Special5JsonValueObject);

		// Add Pose: Handed, Front Right
		TSharedRef<FJsonObject> Special6JsonObject = MakeShareable(new FJsonObject());
		Special6JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_RIGHT));
		Special6JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_RIGHT));
		Special6JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special6JsonValueObject = MakeShareable(new FJsonValueObject(Special6JsonObject));
		TrackerPoseArray.Add(Special6JsonValueObject);

		// Add Pose: Handed, Front Rolled Left
		TSharedRef<FJsonObject> Special7JsonObject = MakeShareable(new FJsonObject());
		Special7JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_LEFT));
		Special7JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_FRONTR_LEFT));
		Special7JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special7JsonValueObject = MakeShareable(new FJsonValueObject(Special7JsonObject));
		TrackerPoseArray.Add(Special7JsonValueObject);

		// Add Pose: Handed, Front Rolled Right
		TSharedRef<FJsonObject> Special8JsonObject = MakeShareable(new FJsonObject());
		Special8JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_RIGHT));
		Special8JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_FRONTR_RIGHT));
		Special8JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special8JsonValueObject = MakeShareable(new FJsonValueObject(Special8JsonObject));
		TrackerPoseArray.Add(Special8JsonValueObject);

		// Add Pose: Handed, Pistol Grip Left
		TSharedRef<FJsonObject> Special9JsonObject = MakeShareable(new FJsonObject());
		Special9JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_LEFT));
		Special9JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_PISTOL_LEFT));
		Special9JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special9JsonValueObject = MakeShareable(new FJsonValueObject(Special9JsonObject));
		TrackerPoseArray.Add(Special9JsonValueObject);

		// Add Pose: Handed, Pistol Grip Right
		TSharedRef<FJsonObject> Special10JsonObject = MakeShareable(new FJsonObject());
		Special10JsonObject->SetStringField(TEXT("output"), TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_RIGHT));
		Special10JsonObject->SetStringField(TEXT("path"), TEXT(ACTION_PATH_SPCL_PISTOL_RIGHT));
		Special10JsonObject->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> Special10JsonValueObject = MakeShareable(new FJsonValueObject(Special10JsonObject));
		TrackerPoseArray.Add(Special10JsonValueObject);

		// Add Tracker Pose Array To Action Set
		ActionSetJsonObject->SetArrayField(TEXT("poses"), TrackerPoseArray);
	}
	else
	{
		// Add Raw Pose
		TSharedRef<FJsonObject> TrackerJsonObjectRaw = MakeShareable(new FJsonObject());
		TrackerJsonObjectRaw->SetStringField(TEXT("output"), TrackerOutput);
		TrackerJsonObjectRaw->SetStringField(TEXT("path"), TrackerPath);
		TrackerJsonObjectRaw->SetStringField(TEXT("requirement"), TEXT("optional"));

		TSharedRef<FJsonValueObject> TrackerJsonValueObjectRaw = MakeShareable(new FJsonValueObject(TrackerJsonObjectRaw));
		TrackerPoseArray.Add(TrackerJsonValueObjectRaw);
	}

	// Add Tracker Pose Array To Action Set
	ActionSetJsonObject->SetArrayField(TEXT("poses"), TrackerPoseArray);
}

void FSteamVRInputDevice::GenerateActionBindings(TArray<FInputMapping>& InInputMapping, TArray<TSharedPtr<FJsonValue>>& JsonValuesArray, FControllerType Controller, bool bIsGenericController)
{
	// Check for headsets
	bool bIsHeadset = Controller.Description.Contains(TEXT("Headset"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	bool bIsGenericControllerCache = bIsGenericController;

	// Process Key Input Mappings
	for (FSteamVRInputKeyMapping SteamVRKeyInputMapping : SteamVRKeyInputMappings)
	{
		// Check if this is a generic UE motion controller key
		bool bHasSteamVRInputs = false;
		if (bIsGenericController)
		{
			// Let's check if there're any SteamVR specific key that already exists for this action
			for (FSteamVRInputKeyMapping SteamVRKeyInputMappingInner : SteamVRKeyInputMappings)
			{
				// Check for generic controllers that have steamvr inputs already defined
				if (SteamVRKeyInputMapping.InputKeyMapping.ActionName.ToString().Equals(SteamVRKeyInputMappingInner.InputKeyMapping.ActionName.ToString())
					&& IsVRKey(SteamVRKeyInputMappingInner.InputKeyMapping.Key.GetFName())
					&& !SteamVRKeyInputMappingInner.InputKeyMapping.Key.GetFName().ToString().Contains(TEXT("SteamVR_HMD_Proximity"))
					)
				{
					bHasSteamVRInputs = true;
					break;
				}
				else
				{
					bHasSteamVRInputs = false;
				}
			}
		}

		if ((bIsGenericController && !bHasSteamVRInputs) || (!bIsGenericController && Controller.KeyEquivalent.Contains(TEXT("SteamVR")) && !SteamVRKeyInputMapping.ControllerName.Contains(TEXT("MotionController"))))
		{
			// Check this input mapping is of the correct controller type
			if (!Controller.KeyEquivalent.Contains(SteamVRKeyInputMapping.ControllerName)
				&& !SteamVRKeyInputMapping.InputKeyMapping.Key.GetFName().ToString().Contains(TEXT("MotionController"))
				&& !SteamVRKeyInputMapping.InputKeyMapping.Key.GetFName().ToString().Contains(TEXT("HMD_Proximity"))
				)
			{
				continue;
			}
			else
			{
				// Process the Key Mapping
				FSteamVRInputState InputState;
				FName CacheMode;
				FString CacheType;
				FString CachePath;

				// Set Axis States
				InputState.bIsAxis = false;
				InputState.bIsAxis2 = false;
				InputState.bIsAxis3 = false;

				// Reset Dpad States
				InputState.bIsDpadUp = false;
				InputState.bIsDpadDown = false;
				InputState.bIsDpadLeft = false;
				InputState.bIsDpadRight = false;

				// Set Input State
				FString CurrentInputKeyName = SteamVRKeyInputMapping.InputKeyMapping.Key.ToString();
				InputState.bIsTrigger = CurrentInputKeyName.Contains(TEXT("Trigger"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsBumper = CurrentInputKeyName.Contains(TEXT("Bumper"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsPress = CurrentInputKeyName.Contains(TEXT("Click"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsThumbstick = InputState.bIsJoystick = CurrentInputKeyName.Contains(TEXT("Thumbstick"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsTrackpad = CurrentInputKeyName.Contains(TEXT("Trackpad"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsGrip = CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsLeft = CurrentInputKeyName.Contains(TEXT("_Left_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				InputState.bIsFaceButton1 = CurrentInputKeyName.Contains(TEXT("FaceButton1"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
					CurrentInputKeyName.Contains(TEXT("_A_"));
				InputState.bIsFaceButton2 = CurrentInputKeyName.Contains(TEXT("FaceButton2"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
					CurrentInputKeyName.Contains(TEXT("_B_"));
				InputState.bIsAppMenu = CurrentInputKeyName.Contains(TEXT("_Menu_"));
				InputState.bIsProximity = CurrentInputKeyName.Contains(TEXT("_HMD_Proximity"));

				// Only handle proximity sensor for headsets
				if ((bIsHeadset && !InputState.bIsProximity)
					|| (!bIsHeadset && InputState.bIsProximity)
					)
				{
					continue;
				}

				// Check cap sense
				InputState.bIsCapSense = CurrentInputKeyName.EndsWith(TEXT("CapSense"))
					|| CurrentInputKeyName.EndsWith(TEXT("_Touch"));

				// Handle Oculus Touch
				InputState.bIsXButton = InputState.bIsYButton = false;
				if (CurrentInputKeyName.StartsWith(TEXT("OculusTouch"))
					|| CurrentInputKeyName.StartsWith(TEXT("Cosmos"))
					|| CurrentInputKeyName.StartsWith(TEXT("HPMixedRealityController"))
					)
				{
					// Check for left X & Y buttons specific to Oculus Touch
					InputState.bIsXButton = CurrentInputKeyName.EndsWith(TEXT("_X_Click")) || 
						CurrentInputKeyName.EndsWith(TEXT("_X_Touch"));
					InputState.bIsYButton = CurrentInputKeyName.Contains(TEXT("_Y_Click")) ||
						CurrentInputKeyName.EndsWith(TEXT("_Y_Touch"));
				}

				// Check for DPad Keys
				if (CurrentInputKeyName.Contains(TEXT("_Up"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					InputState.bIsDpadUp = true;
					InputState.bIsDpadDown = false;
					InputState.bIsDpadLeft = false;
					InputState.bIsDpadRight = false;
				}
				else if (CurrentInputKeyName.Contains(TEXT("_Down"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					InputState.bIsDpadUp = false;
					InputState.bIsDpadDown = true;
					InputState.bIsDpadLeft = false;
					InputState.bIsDpadRight = false;
				}
				else if (CurrentInputKeyName.Contains(TEXT("Trackpad_Left"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) || CurrentInputKeyName.Contains(TEXT("Thumbstick_Left"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					InputState.bIsDpadUp = false;
					InputState.bIsDpadDown = false;
					InputState.bIsDpadLeft = true;
					InputState.bIsDpadRight = false;
				}
				else if (CurrentInputKeyName.Contains(TEXT("Trackpad_Right"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) || CurrentInputKeyName.Contains(TEXT("Thumbstick_Right"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					InputState.bIsDpadUp = false;
					InputState.bIsDpadDown = false;
					InputState.bIsDpadLeft = false;
					InputState.bIsDpadRight = true;
				}
				else if (CurrentInputKeyName.EndsWith(TEXT("_X")))
				{
					InputState.bIsDpadUp = false;
					InputState.bIsDpadDown = false;
					InputState.bIsDpadLeft = true;
					InputState.bIsDpadRight = true;
				}
				else if (CurrentInputKeyName.EndsWith(TEXT("_Y")))
				{
					InputState.bIsDpadUp = true;
					InputState.bIsDpadDown = true;
					InputState.bIsDpadLeft = false;
					InputState.bIsDpadRight = false;
				}

				// Handle Special Grip & Grab actions for supported controllers
				if ((CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					|| CurrentInputKeyName.Contains(TEXT("Cosmos"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
					&& CurrentInputKeyName.Contains(TEXT("Pinch"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					InputState.bIsPinchGrab = true;
					InputState.bIsGripGrab = false;
					InputState.bIsGrip = false;
					InputState.bIsAxis = false;
				}
				else if ((CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					|| CurrentInputKeyName.Contains(TEXT("Cosmos"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
					&& CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
					&& CurrentInputKeyName.Contains(TEXT("Grab"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					InputState.bIsGripGrab = true;
					InputState.bIsPinchGrab = false;
					InputState.bIsGrip = false;
					InputState.bIsAxis = false;
				}
				else
				{
					InputState.bIsPinchGrab = false;
					InputState.bIsGripGrab = false;
				}

				// Handle Vive controllers not having a thumbstick
				if (InputState.bIsThumbstick && Controller.Description.Contains(TEXT("Vive")))
				{
					InputState.bIsTrackpad = true;
					InputState.bIsThumbstick = false;
				}

				// Set Cache Mode
				CacheMode = InputState.bIsTrigger ? FName(TEXT("trigger")) : FName(TEXT("button"));
				CacheMode = InputState.bIsPress && !InputState.bIsTrigger ? FName(TEXT("button")) : CacheMode;
				CacheMode = InputState.bIsTrackpad ? FName(TEXT("trackpad")) : CacheMode;
				CacheMode = InputState.bIsThumbstick ? FName(TEXT("joystick")) : CacheMode;

				CacheMode = InputState.bIsPinchGrab || InputState.bIsGripGrab ? FName(TEXT("grab")) : CacheMode;


				// Set Grip Cache Mode
				if (CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					|| CurrentInputKeyName.Contains(TEXT("Oculus"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
				{
					// Set Grip Cache Mode for Index & Oculus to "trigger"
					CacheMode = InputState.bIsGrip ? FName(TEXT("trigger")) : CacheMode;
				}
				else
				{
					// Any other controller should use the button cache mode
					CacheMode = InputState.bIsGrip ? FName(TEXT("button")) : CacheMode;
				}


				// Set Cache Path
				if (InputState.bIsTrigger)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_TRIGGER_LEFT)) : FString(TEXT(ACTION_PATH_TRIGGER_RIGHT));
				}
				else if (InputState.bIsBumper)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BUMPER_LEFT)) : FString(TEXT(ACTION_PATH_BUMPER_RIGHT));
				}
				else if (InputState.bIsTrackpad)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_TRACKPAD_LEFT)) : FString(TEXT(ACTION_PATH_TRACKPAD_RIGHT));
				}
				else if (InputState.bIsThumbstick)
				{
					// Thumbstick vs Joystick (to conform with new UE naming scheme)
					if (CurrentInputKeyName.Contains(TEXT("ValveIndex")))
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_THUMBSTICK_LEFT)) : FString(TEXT(ACTION_PATH_THUMBSTICK_RIGHT));
					}
					else
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_JOYSTICK_LEFT)) : FString(TEXT(ACTION_PATH_JOYSTICK_RIGHT));
					}
				}
				else if (InputState.bIsGrip)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_GRIP_LEFT)) : FString(TEXT(ACTION_PATH_GRIP_RIGHT));
				}
				else if (InputState.bIsFaceButton1)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BTN_A_LEFT)) : FString(TEXT(ACTION_PATH_BTN_A_RIGHT));
				}
				else if (InputState.bIsFaceButton2)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BTN_B_LEFT)) : FString(TEXT(ACTION_PATH_BTN_B_RIGHT));
				}
				else if (InputState.bIsXButton)
				{
					CachePath = FString(TEXT(ACTION_PATH_BTN_X_LEFT));
				}
				else if (InputState.bIsYButton)
				{
					CachePath = FString(TEXT(ACTION_PATH_BTN_Y_LEFT));
				}
				else if (InputState.bIsAppMenu)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_APPMENU_LEFT)) : FString(TEXT(ACTION_PATH_APPMENU_RIGHT));
				}

				// Handle Special Actions
				if (InputState.bIsPinchGrab)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_PINCH_GRAB_LEFT)) : FString(TEXT(ACTION_PATH_PINCH_GRAB_RIGHT));
				}
				else if (InputState.bIsGripGrab)
				{
					CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_GRIP_GRAB_LEFT)) : FString(TEXT(ACTION_PATH_GRIP_GRAB_RIGHT));
				}
				else if (InputState.bIsProximity)
				{
					CachePath = FString(TEXT(ACTION_PATH_HEAD_PROXIMITY));
				}

				// Override mode if Dpad
				if (InputState.bIsDpadUp || InputState.bIsDpadDown || InputState.bIsDpadLeft || InputState.bIsDpadRight)
				{
					CacheMode = FName(TEXT("dpad"));
				}

				// Override Index Trackpad and Grip Clicks - OpenXR parallels for ease of title migration to OpenXR
				if (CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					&& CurrentInputKeyName.Contains(TEXT("Force"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
					&& (CurrentInputKeyName.Contains(TEXT("Trackpad"), ESearchCase::IgnoreCase, ESearchDir::FromStart) || CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
					)
				{
					CacheMode = FName(TEXT("button"));
					CacheType = FString(TEXT("click"));
				}
				else if (CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					&& CurrentInputKeyName.Contains(TEXT("Axis"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
					&& CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					)
				{
					InputState.bIsTrigger = false;
					InputState.bIsCapSense = true;
					CacheMode = FName(TEXT("button"));
					CacheType = FString(TEXT("touch"));
				}

				// Create Action Source
				FActionSource ActionSource = FActionSource(CacheMode, CachePath);
				TSharedRef<FJsonObject> ActionSourceJsonObject = MakeShareable(new FJsonObject());
				ActionSourceJsonObject->SetStringField(TEXT("mode"), ActionSource.Mode.ToString());

				// Set force_input parameter when dealing with Index Grip Force as a Digital input
				if (CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					&& CurrentInputKeyName.Contains(TEXT("Force"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
					&& CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
					)
				{
					// Create force_input parameter
					TSharedRef<FJsonObject> ParamJsonObject = MakeShareable(new FJsonObject());
					ParamJsonObject->SetStringField(TEXT("force_input"), TEXT("force"));

					// Create Parameter
					TSharedPtr<FJsonObject> ParametersJsonObject = MakeShareable(new FJsonObject());

					// Add to action source json
					ActionSourceJsonObject->SetObjectField(TEXT("parameters"), ParamJsonObject);
				}


				// Set Action Path
				if (!ActionSource.Path.IsEmpty())
				{
					ActionSourceJsonObject->SetStringField(TEXT("path"), ActionSource.Path);
				}
				else
				{
					continue;
				}

				// Add parameters if Dpad
				if (InputState.bIsDpadUp || InputState.bIsDpadDown || InputState.bIsDpadLeft || InputState.bIsDpadRight)
				{
					// Create Submode
					TSharedRef<FJsonObject> SubmodeJsonObject = MakeShareable(new FJsonObject());
					if (CurrentInputKeyName.Contains(TEXT("Trackpad")))
					{
						SubmodeJsonObject->SetStringField( TEXT( "sub_mode" ), TEXT( "click" ) );
					}
					else
					{
						SubmodeJsonObject->SetStringField( TEXT( "sub_mode" ), TEXT( "touch" ) );
					}

					// Create Parameter
					TSharedPtr<FJsonObject> ParametersJsonObject = MakeShareable(new FJsonObject());

					// Set Submode as a parameter
					ActionSourceJsonObject->SetObjectField(TEXT("parameters"), SubmodeJsonObject);
				}

				// Set Key Mappings
				TSharedPtr<FJsonObject> ActionInputJsonObject = MakeShareable(new FJsonObject());

				// Create Action Path
				TSharedRef<FJsonObject> ActionPathJsonObject = MakeShareable(new FJsonObject());
				ActionPathJsonObject->SetStringField(TEXT("output"), SteamVRKeyInputMapping.ActionNameWithPath);

				// Set Cache Type
				if (InputState.bIsAxis && InputState.bIsAxis2)
				{
					if (InputState.bIsGrip)
					{
						CacheType = FString(TEXT("force"));
					}
					else
					{
						if (CacheMode.IsEqual(TEXT("trigger")))
						{
							CacheType = FString(TEXT("pull"));
						}
						else
						{
							CacheType = FString(TEXT("position"));
						}
					}
				}
				else if (InputState.bIsAxis && !InputState.bIsAxis2)
				{
					if (InputState.bIsGrip)
					{
						CacheType = FString(TEXT("force"));
					}
					else if (!InputState.bIsThumbstick && !InputState.bIsTrackpad)
					{
						CacheType = FString(TEXT("pull"));
					}
					else
					{
						CacheType = "";
					}
				}
				else if (!InputState.bIsAxis)
				{
					CacheType = (InputState.bIsCapSense) ? FString(TEXT("touch")) : FString(TEXT("click"));
				}
				else
				{
					CacheType = "";
				}

				// Handle special actions
				if (InputState.bIsPinchGrab || InputState.bIsGripGrab)
				{
					CacheType = FString(TEXT("grab"));
				}

				// Special handling for axes
				if ((CacheMode.IsEqual(TEXT("joystick")) || CacheMode.IsEqual(TEXT("trackpad")))
					&& SteamVRKeyInputMapping.ActionNameWithPath.Right(4) == TEXT("axis")
					&& CacheType == TEXT("position"))
				{
					CacheType = "";
				}

				// Override values in case of hmd proximity
				if (InputState.bIsProximity)
				{
					CachePath = FString(TEXT(ACTION_PATH_HEAD_PROXIMITY));
					CacheMode = FName(TEXT("button"));
					CacheType = FString(TEXT("click"));
				}


				if (!CacheType.IsEmpty())
				{
					// Handle Dpad
					if (InputState.bIsDpadUp || InputState.bIsDpadDown || InputState.bIsDpadLeft || InputState.bIsDpadRight)
					{
						// Handle Dpad values
						if (InputState.bIsDpadUp)
						{
							ActionInputJsonObject->SetObjectField(FString(TEXT("north")), ActionPathJsonObject);
						}

						if (InputState.bIsDpadDown)
						{
							ActionInputJsonObject->SetObjectField(FString(TEXT("south")), ActionPathJsonObject);
						}

						if (InputState.bIsDpadLeft)
						{
							ActionInputJsonObject->SetObjectField(FString(TEXT("west")), ActionPathJsonObject);
						}

						if (InputState.bIsDpadRight)
						{
							ActionInputJsonObject->SetObjectField(FString(TEXT("east")), ActionPathJsonObject);
						}
					}
					else
					{
					// Set Action Input Type
					ActionInputJsonObject->SetObjectField(CacheType, ActionPathJsonObject);
					}

					// Set Inputs
					ActionSourceJsonObject->SetObjectField(TEXT("inputs"), ActionInputJsonObject);

					// Add to Sources Array
					TSharedRef<FJsonValueObject> JsonValueObject = MakeShareable(new FJsonValueObject(ActionSourceJsonObject));
					JsonValuesArray.AddUnique(JsonValueObject);
				}
			}
		}

		bIsGenericController = bIsGenericControllerCache;
	}

	// Process Key Axis Mappings (skip headsets)
	bIsGenericController = bIsGenericControllerCache;
	if (!bIsHeadset)
	{
		for (FSteamVRAxisKeyMapping SteamVRAxisKeyMapping : SteamVRKeyAxisMappings)
		{
			// Check if this is a generic UE motion controller key
			bool bHasSteamVRInputs = false;

			if (bIsGenericController)
			{
				// Let's check if there're any SteamVR specific key that already exists for this action
				for (FSteamVRAxisKeyMapping SteamVRKeyInputMappingInner : SteamVRKeyAxisMappings)
				{
					if (SteamVRAxisKeyMapping.InputAxisKeyMapping.AxisName.ToString().Equals(SteamVRKeyInputMappingInner.InputAxisKeyMapping.AxisName.ToString())
						&& IsVRKey(SteamVRKeyInputMappingInner.InputAxisKeyMapping.Key.GetFName()))
					{
						bHasSteamVRInputs = true;
						break;
					}
					else
					{
						bHasSteamVRInputs = false;
					}
				}
			}

			if ((bIsGenericController && !bHasSteamVRInputs) || (!bIsGenericController && Controller.KeyEquivalent.Contains(TEXT("SteamVR")) && !SteamVRAxisKeyMapping.ControllerName.Contains(TEXT("MotionController"))))
			{
				// Check this input mapping is of the correct controller type
				FString ControllerType = SteamVRAxisKeyMapping.ControllerName;
				if (!Controller.KeyEquivalent.Contains(ControllerType) && !SteamVRAxisKeyMapping.InputAxisKeyMapping.Key.GetFName().ToString().Contains(TEXT("MotionController")))
				{
					continue;
				}
				else
				{
					// Process the Key Mapping
					FSteamVRInputState InputState;
					FName CacheMode;
					FString CacheType;
					FString CachePath;

					// Reset Dpad States
					InputState.bIsDpadUp = false;
					InputState.bIsDpadDown = false;
					InputState.bIsDpadLeft = false;
					InputState.bIsDpadRight = false;

					// Set Axis States
					InputState.bIsAxis = false;
					InputState.bIsAxis2 = false;
					InputState.bIsAxis3 = false;
					if (SteamVRAxisKeyMapping.ActionName.Contains(TEXT("_axis2d")))
					{
						InputState.bIsAxis2 = true;
					}
					else if (SteamVRAxisKeyMapping.ActionName.Contains(TEXT("_axis3d")))
					{
						InputState.bIsAxis3 = true;
					}
					else if (SteamVRAxisKeyMapping.ActionName.Contains(TEXT(" axis")))
					{
						InputState.bIsAxis = true;
					}

					// Set Input State
					FString CurrentInputKeyName = SteamVRAxisKeyMapping.InputAxisKeyMapping.Key.ToString();
					InputState.bIsTrigger = CurrentInputKeyName.Contains(TEXT("Trigger"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsBumper = CurrentInputKeyName.Contains(TEXT("Bumper"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsThumbstick = CurrentInputKeyName.Contains(TEXT("Thumbstick"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsTrackpad = CurrentInputKeyName.Contains(TEXT("Trackpad"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsGrip = CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsLeft = CurrentInputKeyName.Contains(TEXT("_Left_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					InputState.bIsFaceButton1 = CurrentInputKeyName.Contains(TEXT("FaceButton1"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
						CurrentInputKeyName.Contains(TEXT("_A_"));
					InputState.bIsFaceButton2 = CurrentInputKeyName.Contains(TEXT("FaceButton2"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
						CurrentInputKeyName.Contains(TEXT("_B_"));

					// Handle Oculus Touch
					InputState.bIsXButton = InputState.bIsYButton = false;
					if (CurrentInputKeyName.Contains(TEXT("OculusTouch")))
					{
						// Check cap sense
							InputState.bIsCapSense = CurrentInputKeyName.Contains(TEXT("_Touch"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

						// Check for left X & Y buttons specific to Oculus Touch
						InputState.bIsXButton = CurrentInputKeyName.Contains(TEXT("_X_Click")) ||
							CurrentInputKeyName.Contains(TEXT("_X_Touch"));
						InputState.bIsYButton = CurrentInputKeyName.Contains(TEXT("_Y_Click")) ||
							CurrentInputKeyName.Contains(TEXT("_Y_Touch"));
					}
					else
					{
						// Set cap sense input state
						InputState.bIsCapSense = CurrentInputKeyName.Contains(TEXT("CapSense"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) ||
							CurrentInputKeyName.Contains(TEXT("_Touch"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					}

					// Check for DPad Keys
					if (CurrentInputKeyName.Contains(TEXT("_Up"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						InputState.bIsDpadUp = true;
						InputState.bIsDpadDown = false;
						InputState.bIsDpadLeft = false;
						InputState.bIsDpadRight = false;
					}
					else if (CurrentInputKeyName.Contains(TEXT("_Down"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						InputState.bIsDpadUp = false;
						InputState.bIsDpadDown = true;
						InputState.bIsDpadLeft = false;
						InputState.bIsDpadRight = false;
					}
					else if (CurrentInputKeyName.Contains(TEXT("Trackpad_Left"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) || CurrentInputKeyName.Contains(TEXT("Thumbstick_Left"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						InputState.bIsDpadUp = false;
						InputState.bIsDpadDown = false;
						InputState.bIsDpadLeft = true;
						InputState.bIsDpadRight = false;
					}
					else if (CurrentInputKeyName.Contains(TEXT("Trackpad_Right"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) || CurrentInputKeyName.Contains(TEXT("Thumbstick_Right"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						InputState.bIsDpadUp = false;
						InputState.bIsDpadDown = false;
						InputState.bIsDpadLeft = false;
						InputState.bIsDpadRight = true;
					}

					// Handle Special Actions for Knuckles Keys
					if ((CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
						|| CurrentInputKeyName.Contains(TEXT("Cosmos"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
						&& CurrentInputKeyName.Contains(TEXT("Pinch"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
					{
						InputState.bIsPinchGrab = true;
						InputState.bIsGripGrab = false;
						InputState.bIsGrip = false;
						InputState.bIsAxis = false;
					}
					else if ((CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
						|| CurrentInputKeyName.Contains(TEXT("Cosmos"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
						&& CurrentInputKeyName.Contains(TEXT("Grip"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
						&& CurrentInputKeyName.Contains(TEXT("Grab"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
					{
						InputState.bIsGripGrab = true;
						InputState.bIsPinchGrab = false;
						InputState.bIsGrip = false;
						InputState.bIsAxis = false;
					}
					else
					{
						InputState.bIsPinchGrab = false;
						InputState.bIsGripGrab = false;
					}

					// Handle Vive controllers not having a thumbstick
					if (InputState.bIsThumbstick && Controller.Description.Contains(TEXT("Vive")))
					{
						InputState.bIsTrackpad = true;
						InputState.bIsThumbstick = false;
					}

					// Set Cache Mode
					CacheMode = InputState.bIsTrigger || InputState.bIsGrip ? FName(TEXT("trigger")) : FName(TEXT("button"));
					CacheMode = InputState.bIsTrackpad ? FName(TEXT("trackpad")) : CacheMode;
					CacheMode = InputState.bIsGrip ? FName(TEXT("force_sensor")) : CacheMode;
					CacheMode = InputState.bIsThumbstick ? FName(TEXT("joystick")) : CacheMode;
					CacheMode = InputState.bIsPinchGrab || InputState.bIsGripGrab ? FName(TEXT("grab")) : CacheMode;

					// If key being mapped is not an axis key (hardware-wise), set mode as an analog action (scalar_constant to 1.0f)
					// https://github.com/ValveSoftware/steamvr_unreal_plugin/issues/12
					if (!SteamVRAxisKeyMapping.InputAxisKeyMapping.Key.IsAxis1D()
						&& (!CurrentInputKeyName.Contains(TEXT("Trackpad"), ESearchCase::CaseSensitive, ESearchDir::FromEnd)
							&& !CurrentInputKeyName.Contains(TEXT("Touch"), ESearchCase::CaseSensitive, ESearchDir::FromEnd)))
					{
						CacheMode = FName(TEXT("scalar_constant"));
					}

					// Set Cache Path
					if (InputState.bIsTrigger)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_TRIGGER_LEFT)) : FString(TEXT(ACTION_PATH_TRIGGER_RIGHT));
					}
					else if (InputState.bIsBumper)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BUMPER_LEFT)) : FString(TEXT(ACTION_PATH_BUMPER_RIGHT));
					}
					else if (InputState.bIsThumbstick)
					{
						// Thumbstick vs Joystick (to conform with new UE naming scheme)
						if (CurrentInputKeyName.Contains(TEXT("ValveIndex")))
						{
							CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_THUMBSTICK_LEFT)) : FString(TEXT(ACTION_PATH_THUMBSTICK_RIGHT));
						}
						else
						{
							CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_JOYSTICK_LEFT)) : FString(TEXT(ACTION_PATH_JOYSTICK_RIGHT));
						}
					}
					else if (InputState.bIsTrackpad)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_TRACKPAD_LEFT)) : FString(TEXT(ACTION_PATH_TRACKPAD_RIGHT));
					}
					else if (InputState.bIsGrip)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_GRIP_LEFT)) : FString(TEXT(ACTION_PATH_GRIP_RIGHT));

						// For controllers without force sensor support, use trigger value mode
						if (!CurrentInputKeyName.Contains(TEXT("ValveIndex"), ESearchCase::IgnoreCase, ESearchDir::FromStart)
							&& CurrentInputKeyName.Contains(TEXT("Axis"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)
							)
						{
							CacheMode = FName(TEXT("trigger"));
						}
					}
					else if (InputState.bIsFaceButton1)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BTN_A_LEFT)) : FString(TEXT(ACTION_PATH_BTN_A_RIGHT));
					}
					else if (InputState.bIsFaceButton2)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_BTN_B_LEFT)) : FString(TEXT(ACTION_PATH_BTN_B_RIGHT));
					}
					else if (InputState.bIsXButton)
					{
						CachePath = FString(TEXT(ACTION_PATH_BTN_X_LEFT));
					}
					else if (InputState.bIsYButton)
					{
						CachePath = FString(TEXT(ACTION_PATH_BTN_Y_LEFT));
					}

					// Handle Special Actions
					if (InputState.bIsPinchGrab)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_PINCH_GRAB_LEFT)) : FString(TEXT(ACTION_PATH_PINCH_GRAB_RIGHT));
					}
					else if (InputState.bIsGripGrab)
					{
						CachePath = InputState.bIsLeft ? FString(TEXT(ACTION_PATH_GRIP_GRAB_LEFT)) : FString(TEXT(ACTION_PATH_GRIP_GRAB_RIGHT));
					}

					// Override mode if Dpad
					if (InputState.bIsDpadUp || InputState.bIsDpadDown || InputState.bIsDpadLeft || InputState.bIsDpadRight)
					{
						CacheMode = FName(TEXT("dpad"));
					}

					// Create Action Source
					FActionSource ActionSource = FActionSource(CacheMode, CachePath);
					TSharedRef<FJsonObject> ActionSourceJsonObject = MakeShareable(new FJsonObject());
					ActionSourceJsonObject->SetStringField(TEXT("mode"), ActionSource.Mode.ToString());

					// Set Action Path
					if (!ActionSource.Path.IsEmpty())
					{
						ActionSourceJsonObject->SetStringField(TEXT("path"), ActionSource.Path);
					}
					else
					{
						continue;
					}

					// Add parameters if Dpad
					if (InputState.bIsDpadUp || InputState.bIsDpadDown || InputState.bIsDpadLeft || InputState.bIsDpadRight)
					{
						// Set sub mode  
						TSharedRef<FJsonObject> SubmodeJsonObject = MakeShareable(new FJsonObject());

						if (CurrentInputKeyName.Right(5).Equals(TEXT("Touch")))
						{
							SubmodeJsonObject->SetStringField(TEXT("sub_mode"), TEXT("touch"));
						}
						else
						{
							SubmodeJsonObject->SetStringField(TEXT("sub_mode"), TEXT("click"));
						}

						// Create Parameter
						TSharedPtr<FJsonObject> ParametersJsonObject = MakeShareable(new FJsonObject());

						// Set Submode as a parameter
						ActionSourceJsonObject->SetObjectField(TEXT("parameters"), SubmodeJsonObject);
					}

					// Set Key Mappings
					TSharedPtr<FJsonObject> ActionInputJsonObject = MakeShareable(new FJsonObject());

					// Create Action Path
					TSharedRef<FJsonObject> ActionPathJsonObject = MakeShareable(new FJsonObject());
					ActionPathJsonObject->SetStringField(TEXT("output"), SteamVRAxisKeyMapping.ActionNameWithPath);

					// Set Cache Type
					if (CacheMode.IsEqual(TEXT("scalar_constant")))
					{
						CacheType = FString(TEXT("value"));
					}
					else if (CurrentInputKeyName.Contains(TEXT("Trackpad_Touch"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
					{
						CacheType = FString(TEXT("touch"));
					}
					else if (InputState.bIsAxis && InputState.bIsAxis2)
					{
						if (InputState.bIsGrip)
						{
							if (CacheMode == FName("trigger"))
							{
								CacheType = FString(TEXT("pull"));
							}
							else
							{
								CacheType = FString(TEXT("force"));
							}
						}
						else
						{
							if (CacheMode.IsEqual(TEXT("trigger")))
							{
								CacheType = FString(TEXT("pull"));
							}
							else
							{
								CacheType = FString(TEXT("position"));
							}
						}
					}
					else if (InputState.bIsAxis && !InputState.bIsAxis2)
					{
						if (InputState.bIsGrip)
						{
							if (CacheMode == FName("trigger"))
							{
								CacheType = FString(TEXT("pull"));
							}
							else
							{
								CacheType = FString(TEXT("force"));
							}
						}
						else if (!InputState.bIsThumbstick && !InputState.bIsTrackpad)
						{
							CacheType = FString(TEXT("pull"));
						}
						else
						{
							CacheType = "";
						}
					}
					else if (InputState.bIsAxis2)
					{
						CacheType = FString(TEXT("position"));
					}
					else if (!InputState.bIsAxis)
					{
						CacheType = (InputState.bIsCapSense) ? FString(TEXT("touch")) : CacheType;
					}
					else
					{
						CacheType = "";
					}

					// Handle Dpad values
					if (InputState.bIsDpadUp)
					{
						CacheType = "north";
					}
					else if (InputState.bIsDpadDown)
					{
						CacheType = "south";
					}
					else if (InputState.bIsDpadLeft)
					{
						CacheType = "west";
					}
					else if (InputState.bIsDpadRight)
					{
						CacheType = "east";
					}

					// Handle special actions
					if (InputState.bIsPinchGrab || InputState.bIsGripGrab)
					{
						CacheType = FString(TEXT("grab"));
					}

					if (!CacheType.IsEmpty() && !bIsHeadset)
					{
						// Set Action Input Type
						ActionInputJsonObject->SetObjectField(CacheType, ActionPathJsonObject);

						// Set Inputs
						ActionSourceJsonObject->SetObjectField(TEXT("inputs"), ActionInputJsonObject);

						// Add to Sources Array
						TSharedRef<FJsonValueObject> JsonValueObject = MakeShareable(new FJsonValueObject(ActionSourceJsonObject));
						JsonValuesArray.AddUnique(JsonValueObject);
					}
				}
			}
		}
	}
}

void FSteamVRInputDevice::GenerateActionManifest(bool GenerateActions, bool GenerateBindings, bool RegisterApp, bool DeleteIfExists)
{
	// Set Action Manifest Path
	const FString ManifestPath = FPaths::ProjectConfigDir() / CONTROLLER_BINDING_PATH / ACTION_MANIFEST;
	UE_LOG(LogSteamVRInputDevice, Display, TEXT("Action Manifest Path: %s"), *ManifestPath);

	// Create Action Manifest json object
	TSharedRef<FJsonObject> ActionManifestObject = MakeShareable(new FJsonObject());
	TArray<FString> LocalizationFields = { "language_tag", "en_us" };

	// Set where to look for controller binding files and prepare file manager
	const FString ControllerBindingsPath = FPaths::ProjectConfigDir() / CONTROLLER_BINDING_PATH;
	UE_LOG(LogSteamVRInputDevice, Display, TEXT("Controller Bindings Path: %s"), *ControllerBindingsPath);
	IFileManager& FileManager = FFileManagerGeneric::Get();

	// Define Controller Types supported by SteamVR
	TArray<TSharedPtr<FJsonValue>> ControllerBindings;
	ControllerTypes.Empty();

	ControllerTypes.Add(FControllerType(TEXT("knuckles"), TEXT("ValveIndex"), TEXT("SteamVR_ValveIndex")));
	ControllerTypes.Add(FControllerType(TEXT("vive_controller"), TEXT("Vive"), TEXT("SteamVR_Vive")));
	ControllerTypes.Add(FControllerType(TEXT("vive_cosmos_controller"), TEXT("Cosmos"), TEXT("SteamVR_Cosmos")));
	ControllerTypes.Add(FControllerType(TEXT("oculus_touch"), TEXT("OculusTouch"), TEXT("SteamVR_OculusTouch")));
	ControllerTypes.Add(FControllerType(TEXT("holographic_controller"), TEXT("MixedReality"), TEXT("SteamVR_MixedReality")));
	ControllerTypes.Add(FControllerType(TEXT("hpmotioncontroller"), TEXT("HPMixedRealityController"), TEXT("SteamVR_HPMixedRealityController")));

	ControllerTypes.Add(FControllerType(TEXT("indexhmd"), TEXT("Valve Index Headset"), TEXT("SteamVR_Valve_Index_Headset")));
	ControllerTypes.Add(FControllerType(TEXT("vive"), TEXT("Vive Headset"), TEXT("SteamVR_Vive_Headset")));
	ControllerTypes.Add(FControllerType(TEXT("vive_pro"), TEXT("Vive Pro Headset"), TEXT("SteamVR_Vive_Pro_Headset")));
	ControllerTypes.Add(FControllerType(TEXT("rift"), TEXT("Rift Headset"), TEXT("SteamVR_Rift_Headset")));

	ControllerTypes.Add(FControllerType(TEXT("vive_tracker"), TEXT("Vive Tracker"), TEXT("SteamVR_Vive_Tracker")));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_camera"), TEXT("Vive Tracker (Camera)"), TEXT("SteamVR_Vive_Tracker_Camera"), TEXT(ACTION_PATH_TRACKER_CAMERA), TEXT(ACTION_PATH_SPCL_CAMERA)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_waist"), TEXT("Vive Tracker (Waist)"), TEXT("SteamVR_Vive_Tracker_Waist"), TEXT(ACTION_PATH_TRACKER_WAIST), TEXT(ACTION_PATH_SPCL_WAIST)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_left_foot"), TEXT("Vive Tracker (Left Foot)"), TEXT("SteamVR_Vive_Tracker_Left_Foot"), TEXT(ACTION_PATH_TRACKER_FOOT_LEFT), TEXT(ACTION_PATH_SPCL_FOOT_LEFT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_right_foot"), TEXT("Vive Tracker (Right Foot)"), TEXT("SteamVR_Vive_Tracker_Right_Foot"), TEXT(ACTION_PATH_TRACKER_FOOT_RIGHT), TEXT(ACTION_PATH_SPCL_FOOT_RIGHT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_left_shoulder"), TEXT("Vive Tracker (Left Shoulder)"), TEXT("SteamVR_Vive_Tracker_Left_Shoulder"), TEXT(ACTION_PATH_TRACKER_SHOULDER_LEFT), TEXT(ACTION_PATH_SPCL_SHOULDER_LEFT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_right_shoulder"), TEXT("Vive Tracker (Right Shoulder)"), TEXT("SteamVR_Vive_Tracker_Right_Shoulder"), TEXT(ACTION_PATH_TRACKER_SHOULDER_RIGHT), TEXT(ACTION_PATH_SPCL_SHOULDER_RIGHT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_left_elbow"), TEXT("Vive Tracker (Left Elbow)"), TEXT("SteamVR_Vive_Tracker_Left_Elbow"), TEXT(ACTION_PATH_TRACKER_ELBOW_LEFT), TEXT(ACTION_PATH_SPCL_ELBOW_LEFT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_right_elbow"), TEXT("Vive Tracker (Right Elbow)"), TEXT("SteamVR_Vive_Tracker_Right_Elbow"), TEXT(ACTION_PATH_TRACKER_ELBOW_RIGHT), TEXT(ACTION_PATH_SPCL_ELBOW_RIGHT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_left_knee"), TEXT("Vive Tracker (Left knee)"), TEXT("SteamVR_Vive_Tracker_Left_Knee"), TEXT(ACTION_PATH_TRACKER_KNEE_LEFT), TEXT(ACTION_PATH_SPCL_KNEE_LEFT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_right_knee"), TEXT("Vive Tracker (Right Knee)"), TEXT("SteamVR_Vive_Tracker_Right_Knee"), TEXT(ACTION_PATH_TRACKER_KNEE_RIGHT), TEXT(ACTION_PATH_SPCL_KNEE_RIGHT)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_chest"), TEXT("Vive Tracker (Chest)"), TEXT("SteamVR_Vive_Tracker_Chest"), TEXT(ACTION_PATH_TRACKER_CHEST), TEXT(ACTION_PATH_SPCL_CHEST)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_keyboard"), TEXT("Vive Tracker (Keyboard)"), TEXT("SteamVR_Vive_Tracker_Keyboard"), TEXT(ACTION_PATH_TRACKER_KEYBOARD), TEXT(ACTION_PATH_SPCL_KEYBOARD)));
	ControllerTypes.Add(FControllerType(TEXT("vive_tracker_handed"), TEXT("Vive Tracker (Handed)"), TEXT("SteamVR_Vive_Tracker_Handed")));

	ControllerTypes.Add(FControllerType(TEXT("gamepad"), TEXT("Gamepads"), TEXT("SteamVR_Gamepads")));

#pragma region ACTIONS
	// Clear Actions cache
	Actions.Empty();

	// Setup Input Mappings cache
	TArray<FInputMapping> InputMappings;
	TArray<FName> UniqueInputs;

	// Set Input Settings
	auto InputSettings = GetDefault<UInputSettings>();

	// Check if this project have input settings
	if (InputSettings->IsValidLowLevelFast())
	{
		// Process all actions in this project (if any)
		TArray<TSharedPtr<FJsonValue>> InputActionsArray;

		// Setup cache for actions
		TArray<FString> UniqueActions;

		// Controller poses
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_CONTROLLER_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Left Controller [Pose]")), FString(TEXT(ACTION_PATH_CONT_RAW_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_CONTROLLER_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Right Controller [Pose]")), FString(TEXT(ACTION_PATH_CONT_RAW_RIGHT))));
		}

		// Tracker Poses
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_CAMERA));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Camera [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_CAMERA))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_CHEST));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Chest [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_CHEST))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_SHOULDER_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Shoulder Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_SHOULDER_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_SHOULDER_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Shoulder Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_SHOULDER_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_ELBOW_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Elbow Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_ELBOW_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_ELBOW_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Elbow Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_ELBOW_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_KNEE_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Knee Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_KNEE_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_KNEE_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Knee Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_KNEE_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_WAIST));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Waist [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_WAIST))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_FOOT_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Foot Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FOOT_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_FOOT_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Foot Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FOOT_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_KEYBOARD));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Keyboard [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_KEYBOARD))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_POSE_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Raw Pose Left [Tracker]")), FString(TEXT(ACTION_PATH_CONT_RAW_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_POSE_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Raw Pose Right [Tracker]")), FString(TEXT(ACTION_PATH_CONT_RAW_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_BACK_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Back Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_BACK_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_BACK_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Back Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_BACK_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Front Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FRONT_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Front Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FRONT_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Front Rolled Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FRONTR_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Front Rolled Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_FRONTR_RIGHT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Pistol Grip Left [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_PISTOL_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Pose, false,
				FName(TEXT("Handed Pistol Grip Right [Tracker]")), FString(TEXT(ACTION_PATH_SPCL_PISTOL_RIGHT))));
		}

		// Skeletal Data
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_SKELETON_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Skeleton, false,
				FName(TEXT("Skeleton (Left)")), FString(TEXT(ACTION_PATH_SKEL_HAND_LEFT))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_SKELETON_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Skeleton, false,
				FName(TEXT("Skeleton (Right)")), FString(TEXT(ACTION_PATH_SKEL_HAND_RIGHT))));
		}

		// Haptics
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_VIBRATE_LEFT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Vibration, false, FName(TEXT("Haptic (Left)"))));
		}
		{
			FString ConstActionPath = FString(TEXT(ACTION_PATH_VIBRATE_RIGHT));
			Actions.Add(FSteamVRInputAction(ConstActionPath, ESteamVRActionType::Vibration, false, FName(TEXT("Haptic (Right)"))));
		}

		// Add base actions to the action manifest
		ActionManifestObject->SetArrayField(TEXT("actions"), InputActionsArray);

		// Open console
		{
			const FKey* ConsoleKey = InputSettings->ConsoleKeys.FindByPredicate([](FKey& Key) { return Key.IsValid(); });
			if (ConsoleKey != nullptr)
			{
				Actions.Add(FSteamVRInputAction(FString(TEXT(ACTION_PATH_OPEN_CONSOLE)), FName(TEXT("Open Console")), false, ConsoleKey->GetFName(), false));
				UniqueInputs.AddUnique(ConsoleKey->GetFName());
			}
		}

		// Add project's input key mappings to SteamVR's Input Actions
		ProcessKeyInputMappings(InputSettings, UniqueInputs);

		// Add project's input axis mappings to SteamVR's Input Actions
		ProcessKeyAxisMappings(InputSettings, UniqueInputs);

		// Reorganize all unique inputs to SteamVR style Input-to-Actions association
		for (FName UniqueInput : UniqueInputs)
		{
			// Create New Input Mapping from Unique Input Key
			FInputMapping NewInputMapping = FInputMapping();
			FInputMapping NewAxisMapping = FInputMapping();
			NewInputMapping.InputKey = UniqueInput;
			NewAxisMapping.InputKey = UniqueInput;

			// Go through all the project actions
			for (FSteamVRInputAction& Action : Actions)
			{
				//UE_LOG(LogTemp, Warning, TEXT("Action: %s Type: [%s]"), *Action.KeyX.ToString(), *Action.GetActionTypeName());

				// Check for boolean/digital input
				if (Action.Type == ESteamVRActionType::Boolean)
				{
					// Set Key Actions Linked To This Input Key
					TArray<FInputActionKeyMapping> ActionKeyMappings;
					FindActionMappings(InputSettings, Action.Name, ActionKeyMappings);
					for (FInputActionKeyMapping ActionKeyMapping : ActionKeyMappings)
					{
						if (UniqueInput.IsEqual(ActionKeyMapping.Key.GetFName()))
						{
							NewInputMapping.Actions.AddUnique(Action.Path);
						}
					}
				}

				// Check for axes/analog input
				if (Action.Type == ESteamVRActionType::Vector1 || Action.Type == ESteamVRActionType::Vector2 || Action.Type == ESteamVRActionType::Vector3)
				{
					// Set Axis Actions Linked To This Input Key
					FString ActionAxis = Action.Name.ToString();

					// Parse comma delimited action names into an array
					TArray<FString> ActionAxisArray;
					ActionAxis.ParseIntoArray(ActionAxisArray, TEXT(","), true);
					TArray<FInputAxisKeyMapping> FoundAxisMappings;

					for (auto& ActionAxisName : ActionAxisArray)
					{
						FindAxisMappings(InputSettings, FName(*ActionAxisName), FoundAxisMappings);

						for (FInputAxisKeyMapping AxisMapping : FoundAxisMappings)
						{
							if (UniqueInput.IsEqual(AxisMapping.Key.GetFName()))
							{
								// Check for X Axis
								if (!Action.KeyX.IsNone() && Action.KeyX.IsEqual(AxisMapping.Key.GetFName()))
								{
									// Add 1D Action
									NewAxisMapping.Actions.AddUnique(Action.Path);

									FString ActionDimension = Action.Name.ToString().Right(7);

									if (ActionDimension == TEXT("_axis2d"))
									{
										// Add 2D Action
										FString Action2D = Action.Path.LeftChop(11) + TEXT(" X Y_axis2d");
										NewAxisMapping.Actions.AddUnique(Action2D);
									}

									if (ActionDimension == TEXT("_axis3d"))
									{
										// Add 3D Action
										FString Action3D = Action.Path.LeftChop(11) + TEXT(" X Y_axis3d");
										NewAxisMapping.Actions.AddUnique(Action3D);
									}
								}
							}
						}
					}
				}

				// Setup the action fields
				TArray<FString> ActionFields = {
					TEXT("name"), Action.Path,
					TEXT("type"), Action.GetActionTypeName(),
				};

				// Add hand if skeleton
				if (Action.Type == ESteamVRActionType::Skeleton)
				{
					ActionFields.Append({ TEXT("skeleton"), Action.StringPath });
				}

				// Add optional field if this isn't a required field
				if (!Action.bRequirement)
				{
					FString Optional[] = { TEXT("requirement"), TEXT("optional") };
					ActionFields.Append(Optional, 2);
				}

				if (!UniqueActions.Contains(Action.Name.ToString()))
				{
					// Add this action to the array of input actions
					TSharedRef<FJsonObject> ActionObject = MakeShareable(new FJsonObject());
					BuildJsonObject(ActionFields, ActionObject);
					InputActionsArray.AddUnique(MakeShareable(new FJsonValueObject(ActionObject)));

					// Add this action to a cache of unique actions for this project
					UniqueActions.AddUnique(Action.Name.ToString());

					// Set localization text for this action
					FString ActionName = Action.Name.ToString();
					if (ActionName.Contains("axis"))
					{
						if (ActionName.Contains(","))
						{
							TArray<FString> ActionNameArray;
							ActionName.ParseIntoArray(ActionNameArray, TEXT(","), true);

							if (ActionNameArray.Num() > 0)
							{
								// Grab only the first action name & remove _X
								if (ActionNameArray[0].EndsWith(TEXT("_X")))
								{
									ActionName = FString(ActionNameArray[0]).Replace(TEXT("_X"), TEXT(""));
								}
								else if (ActionNameArray[0].EndsWith(TEXT(" X")))
								{
									ActionName = FString(ActionNameArray[0]).Replace(TEXT(" X"), TEXT(""));
								}
								else if (ActionNameArray[0].EndsWith(TEXT("X")))
								{
									ActionName = FString(ActionNameArray[0]).Replace(TEXT("X"), TEXT(""));
								}
							}
						}
						else
						{
							if (ActionName.Right(5).Equals(" axis"))
							{
								ActionName = ActionName.LeftChop(5); // Remove " axis" for the localization string
							}
						}
					}
					FString LocalizationArray[] = { Action.Path, ActionName };
					LocalizationFields.Append(LocalizationArray, 2);
				}

			}

			// Add this Input Mapping to the main Input Mappings array
			if (NewInputMapping.Actions.Num() > 0)
			{
				InputMappings.Add(NewInputMapping);
			}

			// Add this Axis Mapping to the main Input Mappings array
			if (NewAxisMapping.Actions.Num() > 0)
			{
				InputMappings.Add(NewAxisMapping);
			}
		}

		// If there are input actions, add them to the action manifest object
		ActionManifestObject->SetArrayField(TEXT("actions"), InputActionsArray);
	}
	else
	{
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("Error trying to retrieve Input Settings."));
	}
#pragma endregion

#pragma region ACTION SETS
	// Setup action set json objects
	TArray<TSharedPtr<FJsonValue>> ActionSets;
	TSharedRef<FJsonObject> ActionSetObject = MakeShareable(new FJsonObject());

	// Create action set objects
	TArray<FString> StringFields = {
		"name", TEXT(ACTION_SET),
		"usage", TEXT("leftright")
	};

	BuildJsonObject(StringFields, ActionSetObject);

	// Add action sets array to the Action Manifest object
	ActionSets.Add(MakeShareable(new FJsonValueObject(ActionSetObject)));
	ActionManifestObject->SetArrayField(TEXT("action_sets"), ActionSets);

	// Set localization text for the action set
	LocalizationFields.Add(TEXT(ACTION_SET));
	LocalizationFields.Add("Main Game Actions");
#pragma endregion

#pragma region DEFAULT CONTROLLER BINDINGST
	// Start search for controller bindings files
	TArray<FString> ControllerBindingFiles;
	FileManager.FindFiles(ControllerBindingFiles, *ControllerBindingsPath, TEXT("*.json"));
	UE_LOG(LogSteamVRInputDevice, Log, TEXT("Searching for Controller Bindings files at: %s"), *ControllerBindingsPath);

	// Look for existing controller binding files
	uint32 YesNoToAll = EAppReturnType::No;
	for (FString& BindingFile : ControllerBindingFiles)
	{
		// Skip if manifest
		if (BindingFile.Contains(TEXT("steamvr_manifest")))
		{
			continue;
		}

		// Setup cache
		FString StringCache;
		FString ControllerType;
		FString LastEdited;

		// Load Binding File to a string
		FString BindingPath = ControllerBindingsPath / BindingFile;
		FFileHelper::LoadFileToString(StringCache, *BindingPath);

		// Convert string to json object
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(StringCache);
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

		// Attempt to deserialize string cache to a json object
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Invalid json format for controller binding file, skipping: %s"), *(ControllerBindingsPath / BindingFile));
		}
		// Attempt to find what controller this binding file is for
		else if (!JsonObject->TryGetStringField(TEXT("controller_type"), ControllerType) || ControllerType.IsEmpty())
		{
			UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Unable to determine controller type for this binding file, skipping: %s"), *(ControllerBindingsPath / BindingFile));
		}
		else
		{
			bool bIsGenerated = true;

			if (!FileManager.FileExists(*ManifestPath) || DeleteIfExists)
			{
				// Set this binding as one we need to regenerate
				bIsGenerated = false;

				// Throw up a prompt if we did not edit the binding last so the user can abort
				if (!JsonObject->TryGetStringField(TEXT("last_edited_by"), LastEdited) || LastEdited != FApp::GetEpicProductIdentifier())
				{
					if (LastEdited.IsEmpty())
					{
						LastEdited = TEXT("SteamVR");
					}

					if (YesNoToAll != EAppReturnType::NoAll && YesNoToAll != EAppReturnType::YesAll)
					{
						YesNoToAll = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, FText::Format(LOCTEXT("BindingFileAlreadyExists", "Your binding file ({0}) was last edited by {1} do you want to overwrite the changes? You will lose any changes you made outside of the editor!"), FText::FromString(BindingFile), FText::FromString(LastEdited)));
					}

					if (YesNoToAll != EAppReturnType::Yes && YesNoToAll != EAppReturnType::YesAll)
					{
						// Prevent this binding from being regenerated
						bIsGenerated = true;
					}
					else
					{
						FString BackupPath = BindingPath + ".backup";
						FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*BackupPath);
						FPlatformFileManager::Get().GetPlatformFile().CopyFile(*BackupPath, *BindingPath);
					}
				}
			}

			// Create Controller Binding Object for this binding file
			TSharedRef<FJsonObject> ControllerBindingObject = MakeShareable(new FJsonObject());
			TArray<FString> ControllerStringFields = { "controller_type", *ControllerType,
											 TEXT("binding_url"), *BindingFile //*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(ControllerBindingsPath / BindingFile))
			};
			BuildJsonObject(ControllerStringFields, ControllerBindingObject);
			ControllerBindings.Add(MakeShareable(new FJsonValueObject(ControllerBindingObject)));

			// Tag this controller as generated
			for (auto& DefaultControllerType : ControllerTypes)
			{
				if (DefaultControllerType.Name == FName(*ControllerType))
				{
					DefaultControllerType.bIsGenerated = bIsGenerated;
					break;
				}
			}
		}
	}

#if WITH_EDITOR
		// If we're running in the editor, build the controller bindings if they don't exist yet
	if (GenerateBindings)
	{
		GenerateControllerBindings(ControllerBindingsPath, ControllerTypes, ControllerBindings, Actions, InputMappings, DeleteIfExists);
	}
#endif

		// Add the default bindings object to the action manifest
	if (ControllerBindings.Num() == 0)
	{
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("Unable to find and/or generate controller binding files in: %s"), *ControllerBindingsPath);
	}
	else
	{
		ActionManifestObject->SetArrayField(TEXT("default_bindings"), ControllerBindings);
	}
#pragma endregion

#pragma region LOCALIZATION
	// Setup localizations json objects
	TArray<TSharedPtr<FJsonValue>> Localizations;
	TSharedRef<FJsonObject> LocalizationsObject = MakeShareable(new FJsonObject());

	// Build & add localizations to the Action Manifest object
	BuildJsonObject(LocalizationFields, LocalizationsObject);
	Localizations.Add(MakeShareable(new FJsonValueObject(LocalizationsObject)));
	ActionManifestObject->SetArrayField(TEXT("localization"), Localizations);
#pragma endregion

	// Serialize Action Manifest Object
	FString ActionManifest;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ActionManifest);
	FJsonSerializer::Serialize(ActionManifestObject, JsonWriter);

	// Save json as a UTF8 file
	if (GenerateActions)
	{
		if (FileManager.FileExists(*ManifestPath) && DeleteIfExists)
		{
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ManifestPath);
		}

		if (!FileManager.FileExists(*ManifestPath))
		{
			if (!FFileHelper::SaveStringToFile(ActionManifest, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				UE_LOG(LogSteamVRInputDevice, Error, TEXT("Error trying to generate action manifest in: %s"), *ManifestPath);
				return;
			}
		}
	}

	// Register Application to SteamVR
	if (RegisterApp)
	{
		RegisterApplication(ManifestPath);
	}

	TMultiMap<FName, FName> KeyToActionsMap;
	ActionEvents.Empty();
	bool bAlreadyExists = false;

	// Fill-in unique Action Events that will be processed per tick
	for (FSteamVRInputAction& InputAction : Actions)
	{
		// Check if we've already got a similar action event as we won't need the flat action list for processing controller events
		bAlreadyExists = false;
		for (FSteamVRInputAction& ActionEvent : ActionEvents)
		{
			if (ActionEvent.Handle == InputAction.Handle)
			{
				bAlreadyExists = true;
				break;
			}
		}

		// Add unique action handles to action events
		if (!bAlreadyExists && InputAction.Handle != k_ulInvalidActionHandle)
		{
			ActionEvents.Add(FSteamVRInputAction(InputAction));
		}

		// Add key and action mapping
		if (InputAction.KeyX != NAME_None)
		{
			KeyToActionsMap.AddUnique(InputAction.KeyX, InputAction.Name);
		}
	}


	// Find keys that trigger multiple actions
	TArray<FName> ActionKeys;
	KeyToActionsMap.GetKeys(ActionKeys);
	for (auto Key : ActionKeys)
	{
		TArray<FName> MappedActions;
		KeyToActionsMap.MultiFind(Key, MappedActions);
		if (MappedActions.Num() > 1)
		{
			// Remove key-value pairing for keys that trigger multiple actions in map
			KeyToActionsMap.Remove(Key);
		}
	}

	// Finalize keys that will be used to trigger actions per tick
	KeyToActionsMap.Shrink();
	for (FSteamVRInputAction& ActionEvent : ActionEvents)
	{
		// Skip actions with no key mapping (poses, haptics, etc)
		if (ActionEvent.KeyX != NAME_None)
		{
			// Check if this action is in the KeyToActionsMap map
			const FName* PriorityKey = KeyToActionsMap.FindKey(ActionEvent.Name);
			if (PriorityKey != nullptr)
			{
				// Set the key in the KeyToActionsMap as the default key that will be used to trigger actions
				ActionEvent.KeyX = *PriorityKey;
			}
		}

		//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("Action [%s] mapped to key [%s]"), *ActionEvent.Name.ToString(), *ActionEvent.KeyX.ToString());
	}
}

bool FSteamVRInputDevice::BuildJsonObject(TArray<FString> StringFields, TSharedRef<FJsonObject> OutJsonObject)
{
	// Check if StringFields array is even
	if (StringFields.Num() > 1 && StringFields.Num() % 2 == 0)
	{
		// Generate json object of string field pairs
		for (int32 i = 0; i < StringFields.Num(); i += 2)
		{
			OutJsonObject->SetStringField(StringFields[i], StringFields[i + 1]);
		}

		return true;
	}

	return false;
}

void FSteamVRInputDevice::ProcessKeyInputMappings(const UInputSettings* InputSettings, TArray<FName>& InOutUniqueInputs)
{
	// Retrieve key actions setup in this project
	KeyMappings.Empty();
	SteamVRKeyInputMappings.Empty();
	TArray<FName> KeyActionNames;
	InputSettings->GetActionNames(KeyActionNames);

	// Process all key actions found
	for (const FName& KeyActionName : KeyActionNames)
	{
		TArray<FInputActionKeyMapping> KeyInputMappings;

		// Retrieve input keys associated with this action
		FindActionMappings(InputSettings, KeyActionName, KeyInputMappings);

		for (auto& KeyMapping : KeyInputMappings)
		{
			//UE_LOG(LogSteamVRInputDevice, Warning, TEXT("*** Processing Action: %s"), *KeyMapping.ActionName.ToString());

			// Default to "MotionController" Generic UE type
			FString CurrentControllerType = FString(TEXT("MotionController"));

			// Get the string version of the key id we are dealing with for analysis
			FString CurrentKey = KeyMapping.Key.GetFName().ToString();

			// Determine which supported controller type we are working with
			if (CurrentKey.Contains(TEXT("ValveIndex")))
			{
				CurrentControllerType = FString(TEXT("ValveIndex"));
			}
			else if (CurrentKey.Contains(TEXT("Vive")))
			{
				CurrentControllerType = FString(TEXT("Vive"));
			}
			else if (CurrentKey.Contains(TEXT("Cosmos")))
			{
				CurrentControllerType = FString(TEXT("Cosmos"));
			}
			else if (CurrentKey.Contains(TEXT("OculusTouch")))
			{
				CurrentControllerType = FString(TEXT("OculusTouch"));
			}
			else if (CurrentKey.Contains(TEXT("MixedReality")))
			{
				CurrentControllerType = FString(TEXT("MixedReality"));
			}
			else if (CurrentKey.Contains(TEXT("HMD_Proximity")))
			{
				CurrentControllerType = FString(TEXT("HMD_Proximity"));
			}
			else if (CurrentKey.Contains(TEXT("MotionController")))
			{
				// retain default
			}
			else
			{
				continue; // unrecognized controller - will not process
			}

			// Only process Motion Controller if there are no SteamVR actions
			if (KeyMapping.Key.GetFName().ToString().Contains(TEXT("MotionController")))
			{
				bool bFound = false;
				for (FInputActionKeyMapping& KeyMappingInner : KeyInputMappings)
				{
					//UE_LOG(LogTemp, Warning, TEXT("SEARCH: \nKeyMappingKey: %s \nKeyInner:%s \n%s \n%s"),
					//	*KeyMapping.Key.GetFName().ToString(),
					//	*KeyMappingInner.Key.GetFName().ToString(),
					//	*KeyMappingInner.Key.GetFName().ToString(),
					//	*KeyMappingInner.Key.GetFName().ToString());

					if (KeyMapping.Key.GetFName().ToString().Contains(KeyMappingInner.Key.GetFName().ToString())
						&& IsVRKey(KeyMappingInner.Key.GetFName())
						)
					{
						//UE_LOG(LogTemp, Warning, TEXT("SEARCH: MOTION CONTROLLER with STEAMVR Paired action found!"));
						bFound = true;
						break;
					}
				}

				if (bFound)
				{
					continue;
				}
			}

			// If there's a Motion Controller or valid device input, add to the SteamVR Input Actions
			Actions.Add(FSteamVRInputAction(
				FString(ACTION_PATH_IN) / KeyActionName.ToString(),
				KeyActionName,
				KeyMapping.Key.GetFName(),
				false));

			// Add input names here for use in the auto-generation of controller bindings
			InOutUniqueInputs.AddUnique(KeyMapping.Key.GetFName());

			// Add input to Key Bindings Cache
			FSteamVRInputKeyMapping SteamVRInputKeyMap = FSteamVRInputKeyMapping(KeyMapping);
			SteamVRInputKeyMap.ActionName = KeyActionName.ToString();
			SteamVRInputKeyMap.ActionNameWithPath = FString(ACTION_PATH_IN) / KeyActionName.ToString();
			SteamVRInputKeyMap.ControllerName = CurrentControllerType;
			SteamVRKeyInputMappings.Add(SteamVRInputKeyMap);
		}
	}
}

void FSteamVRInputDevice::ProcessKeyAxisMappings(const UInputSettings* InputSettings, TArray<FName>& InOutUniqueInputs)
{
	// Retrieve Key Axis names
	TArray<FName> KeyAxisNames;
	InputSettings->GetAxisNames(KeyAxisNames);

	// Setup caches
	KeyAxisMappings.Empty();
	SteamVRKeyAxisMappings.Empty();

	TArray<FSteamVRAxisKeyMapping> X_AxisMappings;
	TArray<FSteamVRAxisKeyMapping> Y_AxisMappings;

	// (EXTRAPOLATE DEV INTENT STEP 1) Iterate over every axis name found in this project
	for (const FName& AxisAction : KeyAxisNames)
	{
		// (1) Get all input axis mappings for this action
		FindAxisMappings(InputSettings, AxisAction, KeyAxisMappings);

		// (2) Create a SteamVR Axis Key Mapping that holds metadata for us
		GetSteamVRMappings(KeyAxisMappings, SteamVRKeyAxisMappings);

		// (3) Process all SteamVR axis key mappings and look for potential Vector2s
		for (FSteamVRAxisKeyMapping& AxisMapping : SteamVRKeyAxisMappings)
		{
			// (a) Get the string version of the key id we are dealing with
			FString CurrentKey = AxisMapping.InputAxisKeyMapping.Key.GetFName().ToString();

			// (b) Add axis key here for use in the auto-generation of controller bindings
			InOutUniqueInputs.AddUnique(AxisMapping.InputAxisKeyMapping.Key.GetFName());

			// (c) Default to "MotionController" Generic UE type
			FString CurrentControllerType = FString(TEXT("MotionController"));

			// (d) Determine which supported controller type we are working with
			if (CurrentKey.Contains(TEXT("ValveIndex")))
			{
				CurrentControllerType = FString(TEXT("ValveIndex"));;
			}
			else if (CurrentKey.Contains(TEXT("Vive")))
			{
				CurrentControllerType = FString(TEXT("Vive"));
			}
			else if (CurrentKey.Contains(TEXT("Cosmos")))
			{
				CurrentControllerType = FString(TEXT("Cosmos"));
			}
			else if (CurrentKey.Contains(TEXT("OculusTouch")))
			{
				CurrentControllerType = FString(TEXT("OculusTouch"));
			}
			else if (CurrentKey.Contains(TEXT("MixedReality")))
			{
				CurrentControllerType = FString(TEXT("MixedReality"));
			}
			else if (CurrentKey.Contains(TEXT("MotionController")))
			{
				// empty on purpose (readability)
			}
			else
			{
				continue;
			}

			// (e) Set the Controller Type for this axis mapping
			AxisMapping.ControllerName = CurrentControllerType;

			// (f) Sort out potential Vector 2s
			if (CurrentKey.Len() > 2)
			{
				if (CurrentKey.EndsWith(TEXT("_X")))
				{
					// Potential Vector2, holding X axis key
					X_AxisMappings.Add(AxisMapping);
				}
				else if (CurrentKey.EndsWith(TEXT("_Y")))
				{
					// Potential Vector2, holding Y axis key
					Y_AxisMappings.Add(AxisMapping);
				}
			}
		}
	}

	// (EXTRAPOLATE DEV INTENT STEP 2) Go through sorted actions with X & Y input keys and attempt to match them
	for (FSteamVRAxisKeyMapping& X_AxisMapping : X_AxisMappings)
	{
		// (1) Reset vector2 tag for this action
		X_AxisMapping.bIsPartofVector2 = false;

		// (2) Check Y Axis Mappings for an equivalent action name
		for (FSteamVRAxisKeyMapping& Y_AxisMapping : Y_AxisMappings)
		{
			// (2.1) Try to find an action pair
			if (X_AxisMapping.InputAxisKeyMapping.Key.GetFName().ToString().LeftChop(2).Equals(Y_AxisMapping.InputAxisKeyMapping.Key.GetFName().ToString().LeftChop(2)) &&
				X_AxisMapping.InputAxisKeyMapping.AxisName.ToString().LeftChop(2).Equals(Y_AxisMapping.InputAxisKeyMapping.AxisName.ToString().LeftChop(2))
				)
			{
				// (2.1.a) Action pair found with matching X & Y keys for a specific controller
				FString AxisName2D = X_AxisMapping.InputAxisKeyMapping.AxisName.ToString() +
					TEXT(",") +
					Y_AxisMapping.InputAxisKeyMapping.AxisName.ToString() +
					TEXT(" X Y_axis2d");
				FString ActionPath2D = FString(ACTION_PATH_IN) / AxisName2D;

				// (2.1.b) Setup X & Y axis keys for this action
				X_AxisMapping.XAxisName = FName(X_AxisMapping.InputAxisKeyMapping.AxisName);
				X_AxisMapping.YAxisName = FName(Y_AxisMapping.InputAxisKeyMapping.AxisName);
				X_AxisMapping.XAxisKey = X_AxisMapping.InputAxisKeyMapping.Key.GetFName();
				X_AxisMapping.YAxisKey = Y_AxisMapping.InputAxisKeyMapping.Key.GetFName();

				// (2.1.c) Create a Vector 2 Action
				X_AxisMapping.ActionName = FString(AxisName2D);
				X_AxisMapping.ActionNameWithPath = FString(ActionPath2D);

				// (2.1.d) Tag this action as a Vector 2
				X_AxisMapping.bIsPartofVector2 = true;
			}
		}
	}

	// (EXTRAPOLATE DEV INTENT STEP 3) Tag Vector1s from Vector2s
	for (FSteamVRAxisKeyMapping& AxisMapping : SteamVRKeyAxisMappings)
	{
		// Check if this AxisMapping belongs to a Vector2
		bool bIsVector2 = false;
		for (FSteamVRAxisKeyMapping& X_AxisMapping : X_AxisMappings)
		{
			if (X_AxisMapping.bIsPartofVector2 &&
				(AxisMapping.InputAxisKeyMapping.Key.GetFName().IsEqual(X_AxisMapping.XAxisKey) || AxisMapping.InputAxisKeyMapping.Key.GetFName().IsEqual(X_AxisMapping.YAxisKey)))
			{
				// Set X & Y axis keys
				AxisMapping.XAxisName = FName(X_AxisMapping.XAxisName);
				AxisMapping.XAxisKey = FName(X_AxisMapping.XAxisKey);
				AxisMapping.YAxisName = FName(X_AxisMapping.YAxisName);
				AxisMapping.YAxisKey = FName(X_AxisMapping.YAxisKey);

				// Check for duplicates
				bool bIsDuplicate = false;
				for (auto& TestAction : Actions)
				{
					if (TestAction.KeyX.IsEqual(AxisMapping.XAxisKey) && TestAction.KeyY.IsEqual(AxisMapping.YAxisKey))
					{
						bIsDuplicate = true;
						break;
					}
				}

				if (!bIsDuplicate)
				{
					// Set action name and path
					AxisMapping.ActionName = FString(X_AxisMapping.ActionName);
					AxisMapping.ActionNameWithPath = FString(X_AxisMapping.ActionNameWithPath);

					Actions.Add(FSteamVRInputAction(AxisMapping.ActionNameWithPath, FName(*AxisMapping.ActionName), AxisMapping.XAxisKey, AxisMapping.YAxisKey, FVector2D()));
				}

				// Tag this axis mapping as a vector2
				AxisMapping.bIsPartofVector2 = true;
				bIsVector2 = true;
				break;
			}
		}

		// Create a Vector1 action
		if (!bIsVector2)
		{
			// Set axis name and paths
			FString AxisName1D = AxisMapping.InputAxisKeyMapping.AxisName.ToString() + TEXT(" axis");
			FString ActionPath = FString(ACTION_PATH_IN) / AxisName1D;

			// Set X & Y axis keys
			AxisMapping.XAxisName = FName(AxisMapping.InputAxisKeyMapping.AxisName);
			AxisMapping.XAxisKey = AxisMapping.InputAxisKeyMapping.Key.GetFName();

			// Create a Vector 1 action
			Actions.Add(FSteamVRInputAction(ActionPath, FName(*AxisName1D), AxisMapping.XAxisKey, 0.0f));
			AxisMapping.ActionName = FString(AxisName1D);
			AxisMapping.ActionNameWithPath = FString(ActionPath);

			// Ensure this axis mapping is recognized as a Vector1 later on
			AxisMapping.bIsPartofVector2 = false;
			AxisMapping.bIsPartofVector3 = false;
		}
	}

	// Cleanup action set
	SanitizeActions();
}

void FSteamVRInputDevice::SanitizeActions()
{
	UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
	TArray<FInputAxisKeyMapping> DuplicateActions;

	if (InputSettings->IsValidLowLevelFast())
	{
		// Check for duplicates (can be safely removed once full support for Vector2 is up)
		for (auto AxisKeyMapping : InputSettings->GetAxisMappings())
		{
			bool bIsDuplicate = false;
			for (auto DuplicateAction : DuplicateActions)
			{
				if (DuplicateAction.Key == AxisKeyMapping.Key)
				{
					bIsDuplicate = true;
					break;
				}
			}
		}
	}

	// Remove duplicates
	for (auto AxisKeyMapping : DuplicateActions)
	{
		InputSettings->RemoveAxisMapping(AxisKeyMapping);
	}

	// Save to config files and cleanup
	if (DuplicateActions.Num() > 0)
	{
		InputSettings->SaveKeyMappings();
		InputSettings->TryUpdateDefaultConfigFile();
		DuplicateActions.Empty();
	}
}

void FSteamVRInputDevice::RegisterApplication(FString ManifestPath)
{
	if (VRSystem() && VRInput())
	{
		// Get Project Name this plugin is used in
		uint32 AppProcessId = FPlatformProcess::GetCurrentProcessId();
		GameFileName = FPaths::GetCleanFilename(FPlatformProcess::GetApplicationName(AppProcessId));
		FString ProjectName;
		if (GConfig)
		{
			GConfig->GetString(
				TEXT("/Script/EngineSettings.GeneralProjectSettings"),
				TEXT("ProjectName"),
				ProjectName,
				GGameIni
			);
		}

		// Check for empty project name
		if (ProjectName.IsEmpty())
		{
			ProjectName = FApp::GetProjectName();
		}

		GameProjectName = FString::Printf(TEXT("%s-%u"), *ProjectName, FEngineVersion::Current().GetChangelist());

#if WITH_EDITOR
		if (VRApplications())
		{
			// Generate Application Manifest
			FString AppKey, AppManifestPath;

			GenerateAppManifest(ManifestPath, GameFileName, AppKey, AppManifestPath);

			// Load application manifest
			EVRApplicationError AppError = VRApplications()->AddApplicationManifest(TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*AppManifestPath)), true);
			UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Registering Application Manifest %s : %s"), *AppManifestPath, *FString(UTF8_TO_TCHAR(VRApplications()->GetApplicationsErrorNameFromEnum(AppError))));

			// Set AppKey for this Editor Session
			AppError = VRApplications()->IdentifyApplication(AppProcessId, TCHAR_TO_UTF8(*AppKey));
			UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Editor Application [%d][%s] identified to SteamVR: %s"), AppProcessId, *AppKey, *FString(UTF8_TO_TCHAR(VRApplications()->GetApplicationsErrorNameFromEnum(AppError))));
		}
#endif

		// Set Action Manifest
		FString TheActionManifestPath;

#if WITH_EDITOR
		TheActionManifestPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath);
#else
		TheActionManifestPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Config") / TEXT("SteamVRBindings") / TEXT(ACTION_MANIFEST)).Replace(TEXT("/"), TEXT("\\"));
#endif

		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] Trying to load Action Manifest from: %s"), *TheActionManifestPath);
		EVRInputError InputError = VRInput()->SetActionManifestPath(TCHAR_TO_UTF8(*TheActionManifestPath));
		GetInputError(InputError, FString(TEXT("Setting Action Manifest Path Result")));

		// Set Main Action Set
		InputError = VRInput()->GetActionSetHandle(ACTION_SET, &MainActionSet);
		GetInputError(InputError, FString(TEXT("Setting main action set")));

		// Add to action set array
		SteamVRInputActionSets.Empty();
		SteamVRInputActionSets.Add(FSteamVRInputActionSet(0, ACTION_SET, MainActionSet));

		// Populate Active Action sets that will later be used in OpenVR calls
		for (int32 i = 0; i < SteamVRInputActionSets.Num(); i++)
		{
			if (i > MAX_ACTION_SETS - 1) break;	// Skip if more than allocated maximum action sets

			ActiveActionSets[i].nPriority = (int32_t)SteamVRInputActionSets[i].Priority;
			ActiveActionSets[i].ulActionSet = SteamVRInputActionSets[i].Handle;
			ActiveActionSets[i].ulRestrictedToDevice = SteamVRInputActionSets[i].RestrictedToDeviceHandle;
			ActiveActionSets[i].ulSecondaryActionSet = SteamVRInputActionSets[i].SecondaryActionSetHandle;
		}

		// Fill in Action handles for each registered action
		for (auto& Action : Actions)
		{
			VRActionHandle_t Handle;
			InputError = VRInput()->GetActionHandle(TCHAR_TO_UTF8(*Action.Path), &Handle);

			if (InputError != VRInputError_None || Handle == k_ulInvalidActionHandle)
			{
				continue;
			}

			Action.Handle = Handle;

			// Test if this is a pose
			if (Action.Path == TEXT(ACTION_PATH_CONTROLLER_LEFT))
			{
				VRControllerHandleLeft = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_CONTROLLER_RIGHT))
			{
				VRControllerHandleRight = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_CAMERA))
			{
				VRTRackerCamera = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_CHEST))
			{
				VRTrackerChest = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_BACK_LEFT))
			{
				VRTrackerHandedBackL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_BACK_RIGHT))
			{
				VRTrackerHandedBackR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_LEFT))
			{
				VRTrackerHandedFrontL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_FRONT_RIGHT))
			{
				VRTrackerHandedFrontR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_LEFT))
			{
				VRTrackerHandedFrontRL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_FRONTR_RIGHT))
			{
				VRTrackerHandedFrontRR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_LEFT))
			{
				VRTrackerHandedGripL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_GRIP_RIGHT))
			{
				VRTrackerHandedGripR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_POSE_LEFT))
			{
				VRTrackerHandedPoseL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_HANDED_POSE_RIGHT))
			{
				VRTrackerHandedPoseR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_FOOT_LEFT))
			{
				VRTrackerFootL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_FOOT_RIGHT))
			{
				VRTrackerFootR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_SHOULDER_LEFT))
			{
				VRTrackerShoulderL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_SHOULDER_RIGHT))
			{
				VRTrackerShoulderR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_ELBOW_LEFT))
			{
				VRTrackerElbowL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_ELBOW_RIGHT))
			{
				VRTrackerElbowR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_KNEE_LEFT))
			{
				VRTrackerKneeL = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_KNEE_RIGHT))
			{
				VRTrackerKneeR = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_KEYBOARD))
			{
				VRTrackerKeyboard = Action.Handle;
			}
			else if (Action.Path == TEXT(ACTION_PATH_TRACKER_WAIST))
			{
				VRTrackerWaist = Action.Handle;
			}

			UE_LOG(LogSteamVRInputDevice, Display, TEXT("Retrieving Action Handle: %s"), *Action.Path);
			GetInputError(InputError, FString(TEXT("Setting Action Handle Path Result")));
		}
	}
}

bool FSteamVRInputDevice::SetSkeletalHandle(char* ActionPath, VRActionHandle_t& SkeletalHandle)
{
	if (VRSystem() && VRInput())
	{
		// Get Skeletal Handle
		EVRInputError Err = VRInput()->GetActionHandle(ActionPath, &SkeletalHandle);
		if (Err != VRInputError_None || SkeletalHandle == k_ulInvalidActionHandle)
		{
			if (Err != LastInputError)
			{
				GetInputError(Err, TEXT("Couldn't get skeletal action handle for Skeleton."));
			}

			Err = LastInputError;
			return false;
		}
		else
		{
			Err = LastInputError;
			return true;
		}
	}
	return false;
}

void FSteamVRInputDevice::ProcessActionEvents(FSteamVRInputActionSet SteamVRInputActionSet)
{
	FInputDeviceId DeviceId = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
	FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	
	for (auto& Action : ActionEvents)
	{
		if (Action.Handle == k_ulInvalidActionHandle)
		{
			continue;
		}

		if (Action.Type == ESteamVRActionType::Boolean && !Action.Path.Contains(TEXT(" axis")) && !Action.Path.Contains(TEXT("_axis")))
		{
			// Get digital data from SteamVR
			InputDigitalActionData_t DigitalData;
			EVRInputError ActionStateError = VRInput()->GetDigitalActionData(Action.Handle, &DigitalData, sizeof(DigitalData), k_ulInvalidInputValueHandle);

			if (ActionStateError != VRInputError_None)
			{
				//GetInputError(ActionStateError, TEXT("Error encountered retrieving digital data from SteamVR"));
				//UE_LOG(LogSteamVRInputDevice, Error, TEXT("%s"), *Action.Path);
				continue;
			}
			else if (ActionStateError == VRInputError_None && DigitalData.bActive)
			{
				// Send event back to Engine
				if (Action.KeyX != NAME_None)
				{
					// Update the active origin
					Action.ActiveOrigin = DigitalData.activeOrigin;

					if (DigitalData.bState)
					{
						if (!Action.bState)
						{
							MessageHandler->OnControllerButtonPressed(Action.KeyX, UserId, DeviceId, false);
							Action.bState = DigitalData.bState;
							Action.LastUpdated = DigitalData.fUpdateTime;
							Action.bIsRepeat = false;
							//UE_LOG(LogTemp, Warning, TEXT("Handle %s KeyX %s Value %i Changed %i UpdateTime %f"), *Action.Path, *Action.KeyX.ToString(), DigitalData.bState, DigitalData.bChanged, DigitalData.fUpdateTime);
						}
						else
						{
							float EffectiveDelay = Action.bIsRepeat ? REPEAT_DIGITAL_ACTION_DELAY : INITIAL_DIGITAL_ACTION_DELAY;

							if (Action.LastUpdated - DigitalData.fUpdateTime >= EffectiveDelay)
							{
								MessageHandler->OnControllerButtonPressed(Action.KeyX, UserId, DeviceId, true);
								Action.LastUpdated = DigitalData.fUpdateTime;
								Action.bIsRepeat = true;
								//UE_LOG(LogTemp, Warning, TEXT("[REPEAT] Handle %s KeyX %s Value %i Changed %i UpdateTime %f"), *Action.Path, *Action.KeyX.ToString(), DigitalData.bState, DigitalData.bChanged, DigitalData.fUpdateTime);
							}
						}
					}
					else
					{
						if (Action.bState)
						{
							MessageHandler->OnControllerButtonReleased(Action.KeyX, UserId, DeviceId, false);
							//UE_LOG(LogTemp, Display, TEXT("Handle %s KeyX %s Value %i Changed %i UpdateTime %f"), *Action.Path, *Action.KeyX.ToString(), DigitalData.bState, DigitalData.bChanged, DigitalData.fUpdateTime);
						}

						Action.bState = DigitalData.bState;
						Action.bIsRepeat = false;
					}
				}
			}
		}
		else if (Action.Type == ESteamVRActionType::Vector1
			|| Action.Type == ESteamVRActionType::Vector2
			|| Action.Type == ESteamVRActionType::Vector3)
		{
			// Get analog data from SteamVR
			InputAnalogActionData_t AnalogData;
			EVRInputError ActionStateError = VRInput()->GetAnalogActionData(Action.Handle, &AnalogData, sizeof(AnalogData), k_ulInvalidInputValueHandle);

			if (ActionStateError != VRInputError_None)
			{
				//GetInputError(ActionStateError, TEXT("Error encountered retrieving analog data from SteamVR"));
				continue;
			}
			else if (ActionStateError == VRInputError_None && AnalogData.bActive)
			{
				// Update the active origin
				Action.ActiveOrigin = AnalogData.activeOrigin;

				if (Action.KeyX != NAME_None && (FMath::Abs(AnalogData.deltaX) > KINDA_SMALL_NUMBER || Action.Name.IsEqual(FName(CONTROLLER_BINDING_PATH))))
				{
					// Test what we're receiving from SteamVR
					//UE_LOG(LogTemp, Warning, TEXT("Handle %s KeyX %s X-Value [%f]"), *Action.Path, *Action.KeyX.ToString(), AnalogData.x);

					Action.Value.X = AnalogData.x; // ActionCount;
					MessageHandler->OnControllerAnalog(Action.KeyX, UserId, DeviceId, Action.Value.X);
				}

				if (Action.KeyY != NAME_None && (FMath::Abs(AnalogData.deltaY) > KINDA_SMALL_NUMBER))
				{
					// Test what we're receiving from SteamVR
					//UE_LOG(LogTemp, Warning, TEXT("Handle %s KeyY %s Y-Value {%f}"), *Action.Path, *Action.KeyY.ToString(), AnalogData.y);

					FString KeyString = Action.KeyY.ToString();
					if (KeyString.Contains(TEXT("MotionController")) && KeyString.Contains(TEXT("_Y")))
					{
						Action.Value.Y = -AnalogData.y;
					}
					else
					{
						Action.Value.Y = AnalogData.y;
					}
					MessageHandler->OnControllerAnalog(Action.KeyY, UserId, DeviceId, Action.Value.Y);
				}
			}
		}
	}
}

void FSteamVRInputDevice::GetInputError(EVRInputError InputError, FString InputAction)
{
	switch (InputError)
	{
	case VRInputError_None:
		UE_LOG(LogSteamVRInputDevice, Display, TEXT("[STEAMVR INPUT] %s: Success"), *InputAction);
		break;
	case VRInputError_NameNotFound:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Name Not Found"), *InputAction);
		break;
	case VRInputError_WrongType:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Wrong Type"), *InputAction);
		break;
	case VRInputError_InvalidHandle:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Handle"), *InputAction);
		break;
	case VRInputError_InvalidParam:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Param"), *InputAction);
		break;
	case VRInputError_NoSteam:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: No Steam"), *InputAction);
		break;
	case VRInputError_MaxCapacityReached:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s:  Max Capacity Reached"), *InputAction);
		break;
	case VRInputError_IPCError:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: IPC Error"), *InputAction);
		break;
	case VRInputError_NoActiveActionSet:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: No Active Action Set"), *InputAction);
		break;
	case VRInputError_InvalidDevice:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Device"), *InputAction);
		break;
	case VRInputError_InvalidSkeleton:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Skeleton"), *InputAction);
		break;
	case VRInputError_InvalidBoneCount:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Bone Count"), *InputAction);
		break;
	case VRInputError_InvalidCompressedData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Invalid Compressed Data"), *InputAction);
		break;
	case VRInputError_NoData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: No Data"), *InputAction);
		break;
	case VRInputError_BufferTooSmall:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Buffer Too Small"), *InputAction);
		break;
	case VRInputError_MismatchedActionManifest:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Mismatched Action Manifest"), *InputAction);
		break;
	case VRInputError_MissingSkeletonData:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Missing Skeleton Data"), *InputAction);
		break;
	default:
		UE_LOG(LogSteamVRInputDevice, Error, TEXT("[STEAMVR INPUT] %s: Unknown Error"), *InputAction);
		break;
	}

	return;
}

void FSteamVRInputDevice::MirrorSteamVRSkeleton(VRBoneTransform_t* BoneTransformsLS, int32 BoneTransformCount) const
{
	check(BoneTransformsLS != nullptr);
	check(BoneTransformCount == SteamVRSkeleton::GetBoneCount());

	// Mirror the bones whose rotations transfer directly and only the translation needs to be fixed
	{
		const int32 TranslationOnlyBoneCount = sizeof(kMirrorTranslationOnlyBones) / sizeof(kMirrorTranslationOnlyBones[0]);
		for (int32 i = 0; i < TranslationOnlyBoneCount; ++i)
		{
			const int32 BoneIndex = kMirrorTranslationOnlyBones[i];

			vr::HmdVector4_t& Position = BoneTransformsLS[BoneIndex].position;
			Position.v[0] *= -1.f;
			Position.v[1] *= -1.f;
			Position.v[2] *= -1.f;
		}
	}

	// Mirror the metacarpals
	{
		const int32 MetaCarpalBoneCount = sizeof(kMetacarpalBones) / sizeof(kMetacarpalBones[0]);
		for (int32 i = 0; i < MetaCarpalBoneCount; ++i)
		{
			const int32 BoneIndex = kMetacarpalBones[i];

			vr::VRBoneTransform_t& BoneTransform = BoneTransformsLS[BoneIndex];

			BoneTransform.position.v[0] *= -1.f;

			vr::HmdQuaternionf_t OriginalRotation = BoneTransform.orientation;
			BoneTransform.orientation.w = OriginalRotation.x;
			BoneTransform.orientation.x = -OriginalRotation.w;
			BoneTransform.orientation.y = OriginalRotation.z;
			BoneTransform.orientation.z = -OriginalRotation.y;
		}
	}

	// Mirror the children of the root
	{
		const int32 ModelSpaceBoneCount = sizeof(kModelSpaceBones) / sizeof(kModelSpaceBones[0]);
		for (int32 i = 0; i < ModelSpaceBoneCount; ++i)
		{
			const int32 BoneIndex = kModelSpaceBones[i];

			vr::VRBoneTransform_t& BoneTransform = BoneTransformsLS[BoneIndex];
			BoneTransform.position.v[0] *= -1.f;
			BoneTransform.orientation.y *= -1.f;
			BoneTransform.orientation.z *= -1.f;
		}
	}
}

bool FSteamVRInputDevice::IsVRKey(FName InputKey)
{
	FString KeyString = InputKey.ToString();

	if ((KeyString.Contains(TEXT("SteamVR")) && !KeyString.Contains(TEXT("Generic")))
		|| KeyString.Contains(TEXT("ValveIndex"))
		|| KeyString.Contains(TEXT("OculusTouch"))
		|| KeyString.Contains(TEXT("MixedReality"))
		|| KeyString.Contains(TEXT("Vive"))
		|| KeyString.Contains(TEXT("Cosmos"))
		)
	{
		return true;
	}

	return false;
}

bool FSteamVRInputDevice::ProcessVector2D(FSteamVRAxisKeyMapping AxisKeyMapping)
{
	// TODO: For when Vector Actions are a thing
	return false;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE //"SteamVRInputDevice"
