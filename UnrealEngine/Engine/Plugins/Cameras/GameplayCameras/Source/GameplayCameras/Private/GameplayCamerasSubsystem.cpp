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
	if (ensure(CameraModifier))
	{
		return CameraModifier->PlayCameraAnimation(Sequence, Params);
	}
	return FCameraAnimationHandle::Invalid;
}

bool UGameplayCamerasSubsystem::IsCameraAnimationActive(APlayerController* PlayerController, const FCameraAnimationHandle& Handle) const
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensure(CameraModifier))
	{
		return CameraModifier->IsCameraAnimationActive(Handle);
	}
	return false;
}

void UGameplayCamerasSubsystem::StopCameraAnimation(APlayerController* PlayerController, const FCameraAnimationHandle& Handle, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensure(CameraModifier))
	{
		return CameraModifier->StopCameraAnimation(Handle, bImmediate);
	}
}

void UGameplayCamerasSubsystem::StopAllCameraAnimationsOf(APlayerController* PlayerController, UCameraAnimationSequence* Sequence, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensure(CameraModifier))
	{
		CameraModifier->StopAllCameraAnimationsOf(Sequence, bImmediate);
	}
}

void UGameplayCamerasSubsystem::StopAllCameraAnimations(APlayerController* PlayerController, bool bImmediate)
{
	UCameraAnimationCameraModifier* CameraModifier = UCameraAnimationCameraModifier::GetCameraAnimationCameraModifierFromPlayerController(PlayerController);
	if (ensure(CameraModifier))
	{
		CameraModifier->StopAllCameraAnimations(bImmediate);
	}
}

#undef LOCTEXT_NAMESPACE


