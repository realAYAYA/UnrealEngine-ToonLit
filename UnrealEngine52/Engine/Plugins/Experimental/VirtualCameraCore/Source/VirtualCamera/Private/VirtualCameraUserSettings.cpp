// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraUserSettings.h"
#include "UObject/SoftObjectPath.h"

UVirtualCameraUserSettings::UVirtualCameraUserSettings()
{

	FSoftObjectPath DefaultVCamSoftPath = FSoftObjectPath(TEXT("/VirtualCamera/V2/VcamActor.VcamActor_C"));

	if (DefaultVCamSoftPath.IsNull())
	{
		return;
	}

	DefaultVCamClass = TSoftClassPtr<AActor>(DefaultVCamSoftPath);

}

float UVirtualCameraUserSettings::GetFocusInterpSpeed()
{
	return FocusInterpSpeed;
}

void UVirtualCameraUserSettings::SetFocusInterpSpeed(const float InFocusInterpSpeed)
{
	FocusInterpSpeed = InFocusInterpSpeed;
	SaveConfig();
}

float UVirtualCameraUserSettings::GetJoysticksSpeed()
{
	return JoysticksSpeed;
}

void UVirtualCameraUserSettings::SetJoysticksSpeed(const float InJoysticksSpeed)
{
	JoysticksSpeed = InJoysticksSpeed;
	SaveConfig();
}

float UVirtualCameraUserSettings::GetMaxJoysticksSpeed()
{
    return MaxJoysticksSpeed;
}

void UVirtualCameraUserSettings::SetMaxJoysticksSpeed(const float InMaxJoysticksSpeed)
{
	MaxJoysticksSpeed = InMaxJoysticksSpeed;
	SaveConfig();
}

bool UVirtualCameraUserSettings::IsMapGrayscle()
{
	return bIsMapGrayscale;
}

void UVirtualCameraUserSettings::SetIsMapGrayscle(const bool bInIsMapGrayscle)
{
	bIsMapGrayscale = bInIsMapGrayscle;
	SaveConfig();
}

bool UVirtualCameraUserSettings::GetShouldOverrideCameraSettingsOnTeleport()
{
	return bOverrideCameraSettingsOnTeleportToScreenshot;
}

void UVirtualCameraUserSettings::SetShouldOverrideCameraSettingsOnTeleport(const bool bInOverrideCameraSettings)
{
	bOverrideCameraSettingsOnTeleportToScreenshot = bInOverrideCameraSettings;
	SaveConfig();
}

FString UVirtualCameraUserSettings::GetSavedVirtualCameraFilmbackPresetName()
{
	return GetDefault<UVirtualCameraUserSettings>()->VirtualCameraFilmback;
}

void UVirtualCameraUserSettings::SetSavedVirtualCameraFilmbackPresetName(const FString& InFilmback)
{
	VirtualCameraFilmback = InFilmback;
	SaveConfig();
}

bool UVirtualCameraUserSettings::GetShouldDisplayFilmLeader()
{
	return bDisplayFilmLeader;
}

void UVirtualCameraUserSettings::SetShouldDisplayFilmLeader(const bool bInDisplayFilmLeader)
{
	bDisplayFilmLeader = bInDisplayFilmLeader;
	SaveConfig();
}

bool UVirtualCameraUserSettings::GetTeleportOnStart()
{
    return bTeleportOnStart;
}

void UVirtualCameraUserSettings::SetTeleportOnStart(const bool bInTeleportOnStart)
{
	bTeleportOnStart = bInTeleportOnStart;
	SaveConfig();
}



void UVirtualCameraUserSettings::InjectGamepadKeybinds()
{
	ActionMappings = {
		{TEXT("VirtualCamera_Home_EarSelection_Up"), EKeys::Gamepad_DPad_Up},
		{TEXT("VirtualCamera_Home_EarSelection_Down"), EKeys::Gamepad_DPad_Down},
		{TEXT("VirtualCamera_Home_EarSelection_Left"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_Home_EarSelection_Right"), EKeys::Gamepad_DPad_Right},
		{TEXT("VirtualCamera_Home_Selection"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_Home_FStop"), EKeys::Gamepad_LeftTrigger},
		{TEXT("VirtualCamera_Home_LensesMenu"), EKeys::Gamepad_RightTrigger},
		{TEXT("VirtualCamera_Home_Screenshot"), EKeys::Gamepad_RightShoulder},
		{TEXT("VirtualCamera_Home_ToggleStats"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Home_ToggleFlight"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_Home_ToggleRecord"), EKeys::Gamepad_LeftShoulder},
		{TEXT("VirtualCamera_Playback_SelectsSwitch"), EKeys::Gamepad_LeftShoulder},
		{TEXT("VirtualCamera_Playback_SelectClipsUp"), EKeys::Gamepad_DPad_Up},
		{TEXT("VirtualCamera_Playback_SelectClipsDown"), EKeys::Gamepad_DPad_Down},
		{TEXT("VirtualCamera_Playback_PlayPause"), EKeys::Gamepad_RightShoulder},
		{TEXT("VirtualCamera_Playback_Fullscreen"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_Playback_MakeClipSelect"), EKeys::Gamepad_FaceButton_Left},
		{TEXT("VirtualCamera_Playback_TargetClip"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_Playback_BackStart"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_Playback_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Map_SwitchFilterSnapshotFlagLeft"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_Map_SwitchFilterSnapshotFlagRight"), EKeys::Gamepad_DPad_Right},
		{TEXT("VirtualCamera_Map_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Map_TeleportHome"), EKeys::Gamepad_FaceButton_Left},
		{TEXT("VirtualCamera_Map_ToggleFullscreen"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_Recording_InitiateRecord"), EKeys::Gamepad_LeftTrigger},
		{TEXT("VirtualCamera_Interaction_Undo"), EKeys::Gamepad_LeftShoulder},
		{TEXT("VirtualCamera_Interaction_Redo"), EKeys::Gamepad_RightShoulder},
		{TEXT("VirtualCamera_Interaction_SRTSwitchUp"), EKeys::Gamepad_DPad_Up},
		{TEXT("VirtualCamera_Interaction_SRTSwitchDown"), EKeys::Gamepad_DPad_Down},
		{TEXT("VirtualCamera_Interaction_XYZSwitchLeft"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_Interaction_XYZSwitchRight"), EKeys::Gamepad_DPad_Right},
		{TEXT("VirtualCamera_Interaction_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Interaction_LocalWorldToggle"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_Interaction_SelectSpawnNumpad"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_Focus_CloseMenu"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Focus_Selection"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_Focus_ToggleAuto"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_Playback_Back"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_Map_Back"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_Interaction_Back"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_Settings_Close"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_Focus_CloseMenu"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_MotionAdjustments_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Settings_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Focus_AdjustSelectorDown"), EKeys::Gamepad_DPad_Up},
		{TEXT("VirtualCamera_Focus_AdjustSelectorUp"), EKeys::Gamepad_DPad_Down},
		{TEXT("VirtualCamera_Focus_ButtonSelect"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_AnimationPreview_PlayPause"), EKeys::Gamepad_RightShoulder},
		{TEXT("VirtualCamera_AnimationPreview_Close"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_AnimationPreview_BackStart"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_AnimationPreview_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Reposition_ToggleTiltOffset"), EKeys::Gamepad_FaceButton_Left},
		{TEXT("VirtualCamera_Reposition_Close"), EKeys::Gamepad_Special_Right},
		{TEXT("VirtualCamera_Reposition_Close"), EKeys::Gamepad_FaceButton_Right},
		{TEXT("VirtualCamera_MotionAdjustments_MoveAxisUp"), EKeys::Gamepad_DPad_Up},
		{TEXT("VirtualCamera_MotionAdjustments_MoveAxisDown"), EKeys::Gamepad_DPad_Down},
		{TEXT("VirtualCamera_MotionAdjustments_OpenNumpad"), EKeys::Gamepad_FaceButton_Bottom},
		{TEXT("VirtualCamera_MotionAdjustments_SelectionSwitchLeft"), EKeys::Gamepad_LeftShoulder},
		{TEXT("VirtualCamera_MotionAdjustments_SelectionSwitchRight"), EKeys::Gamepad_RightShoulder},
		{TEXT("VirtualCamera_MotionAdjustments_LockAxis"), EKeys::Gamepad_FaceButton_Top},
		{TEXT("VirtualCamera_MotionAdjustments_ResetDutch"), EKeys::Gamepad_FaceButton_Left},
		{TEXT("VirtualCamera_MotionAdjustments_NumpadLeft"), EKeys::Gamepad_DPad_Left},
		{TEXT("VirtualCamera_MotionAdjustments_NumpadRight"), EKeys::Gamepad_DPad_Right},
		{TEXT("VirtualCamera_MotionAdjustments_Back"), EKeys::Gamepad_FaceButton_Right},

	};
	
	AxisMappings = {
		{TEXT("VirtualCamera_Home_MoveFocalLengthSelection"), EKeys::Gamepad_RightY,1.0f},
		{TEXT("VirtualCamera_Home_MoveFStopSelection"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_Playback_FastScroll"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_Playback_FastScrub"), EKeys::Gamepad_RightX,1.0f},
		{TEXT("VirtualCamera_Playback_BackwardFrame"), EKeys::Gamepad_LeftTriggerAxis,1.0f},
		{TEXT("VirtualCamera_Playback_ForwardFrame"), EKeys::Gamepad_RightTriggerAxis,1.0f},
		{TEXT("VirtualCamera_Map_Zoom"), EKeys::Gamepad_RightY,1.0f},
		{TEXT("VirtualCamera_Map_XAxisPan"), EKeys::Gamepad_LeftX,1.0f},
		{TEXT("VirtualCamera_Map_YAxisPan"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_Interaction_Value_Change"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_Interaction_Value_Change"), EKeys::Gamepad_RightY,1.0f},
		{TEXT("VirtualCamera_Focus_ReticleMovementY"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_Focus_ReticleMovementX"), EKeys::Gamepad_LeftX,1.0f},
		{TEXT("VirtualCamera_Focus_AdjustFocalDistance"), EKeys::Gamepad_RightY,1.0f},
		{TEXT("VirtualCamera_AnimationPreview_FastScrub"), EKeys::Gamepad_RightX,1.0f},
		{TEXT("VirtualCamera_AnimationPreview_ForwardFrame"), EKeys::Gamepad_RightTriggerAxis,1.0f},
		{TEXT("VirtualCamera_AnimationPreview_BackwardFrame"), EKeys::Gamepad_LeftTriggerAxis,1.0f},
		{TEXT("VirtualCamera_MotionAdjustments_AdjustRadial"), EKeys::Gamepad_LeftY,1.0f},
		{TEXT("VirtualCamera_MotionAdjustments_AdjustRadial"), EKeys::Gamepad_RightY,1.0f},
		{TEXT("VirtualCamera_MotionAdjustments_AdjustRadial"), EKeys::Gamepad_LeftX,1.0f},
		{TEXT("VirtualCamera_MotionAdjustments_AdjustRadial"), EKeys::Gamepad_RightX,1.0f},
		{TEXT("VirtualCamera_Home_LeftMoveHorizontal"), EKeys::Gamepad_LeftX,1.0f},
		{TEXT("VirtualCamera_Home_RightMoveHorizontal"), EKeys::Gamepad_RightX,1.0f},
	};
}

void UVirtualCameraUserSettings::GetActionMappingsByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const
{
	if (InActionName.IsValid())
	{
		OutMappings = ActionMappings.FilterByPredicate([InActionName](const FInputActionKeyMapping& Mapping) 
		{
			return Mapping.ActionName == InActionName;
		});

	}
}

void UVirtualCameraUserSettings::GetAxisMappingsByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const
{
	if (InAxisName.IsValid())
	{
		OutMappings = AxisMappings.FilterByPredicate([InAxisName](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.AxisName == InAxisName;
		});
	}
}
