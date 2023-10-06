// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCamerasSubsystem.h"
#include "CameraAnimationCameraModifier.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayCamerasSubsystem)

#define LOCTEXT_NAMESPACE "GameplayCamerasSubsystem"

UGameplayCamerasSubsystem* UGameplayCamerasSubsystem::GetGameplayCamerasSubsystem(const UWorld* InWorld)
{
	if (InWorld)
	{
		return InWorld->GetSubsystem<UGameplayCamerasSubsystem>();
	}

	return nullptr;
}

FCameraAnimationHandle UGameplayCamerasSubsystem::PlayCameraAnimation(APlayerController* PlayerController, UCameraAnimationSequence* Sequence, FCameraAnimationParams Params)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensureMsgf(CameraModifier, TEXT("No camera modifier found on the player controller")))
	{
		return CameraModifier->PlayCameraAnimation(Sequence, Params);
	}
	FFrame::KismetExecutionMessage(TEXT("Can't play camera animation: no camera animation modifier found"), ELogVerbosity::Error);
	return FCameraAnimationHandle::Invalid;
}

bool UGameplayCamerasSubsystem::IsCameraAnimationActive(APlayerController* PlayerController, const FCameraAnimationHandle& Handle) const
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (CameraModifier)
	{
		return CameraModifier->IsCameraAnimationActive(Handle);
	}
	return false;
}

void UGameplayCamerasSubsystem::StopCameraAnimation(APlayerController* PlayerController, const FCameraAnimationHandle& Handle, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensureMsgf(CameraModifier, TEXT("No camera modifier found on the player controller")))
	{
		return CameraModifier->StopCameraAnimation(Handle, bImmediate);
	}
	FFrame::KismetExecutionMessage(TEXT("Can't stop camera animation: no camera animation modifier found"), ELogVerbosity::Error);
}

void UGameplayCamerasSubsystem::StopAllCameraAnimationsOf(APlayerController* PlayerController, UCameraAnimationSequence* Sequence, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensureMsgf(CameraModifier, TEXT("No camera modifier found on the player controller")))
	{
		CameraModifier->StopAllCameraAnimationsOf(Sequence, bImmediate);
	}
	FFrame::KismetExecutionMessage(TEXT("Can't stop camera animations: no camera animation modifier found"), ELogVerbosity::Error);
}

void UGameplayCamerasSubsystem::StopAllCameraAnimations(APlayerController* PlayerController, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensureMsgf(CameraModifier, TEXT("No camera modifier found on the player controller")))
	{
		CameraModifier->StopAllCameraAnimations(bImmediate);
	}
	FFrame::KismetExecutionMessage(TEXT("Can't stop all camera animation: no camera animation modifier found"), ELogVerbosity::Error);
}

#undef LOCTEXT_NAMESPACE


