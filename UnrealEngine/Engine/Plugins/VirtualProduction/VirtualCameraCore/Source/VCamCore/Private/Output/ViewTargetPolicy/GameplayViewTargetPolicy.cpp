// Copyright Epic Games, Inc. All Rights Reserved.

#include "Output/ViewTargetPolicy/GameplayViewTargetPolicy.h"

#include "Camera/PlayerCameraManager.h"
#include "CineCameraComponent.h"
#include "GameFramework/PlayerController.h"

TArray<APlayerController*> UGameplayViewTargetPolicy::DeterminePlayerControllers_Implementation(const FDeterminePlayerControllersTargetPolicyParams& Params)
{
	return {};
}

void UGameplayViewTargetPolicy::UpdateViewTarget_Implementation(const FUpdateViewTargetPolicyParams& Params)
{
	checkf(Params.CameraToAffect, TEXT("The CameraToAffect must be valid!"));
	for (APlayerController* PlayerController : Params.PlayerControllers)
	{
		checkf(PlayerController, TEXT("Every element in PlayerControllers should have been filtered to only contain valid values!"));
		PlayerController->SetViewTarget(Params.CameraToAffect->GetOwner(), FViewTargetTransitionParams());
	}
}
