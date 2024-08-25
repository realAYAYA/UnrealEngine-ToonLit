// Copyright Epic Games, Inc. All Rights Reserved.

#include "Output/ViewTargetPolicy/FocusFirstPlayerViewTargetPolicy.h"

#include "CineCameraComponent.h"
#include "Engine/World.h"

TArray<APlayerController*> UFocusFirstPlayerViewTargetPolicy::DeterminePlayerControllers_Implementation(const FDeterminePlayerControllersTargetPolicyParams& Params)
{
	checkf(Params.CameraToAffect, TEXT("CameraToAffect must be valid!"));
	
	// Null entries are allowed: they're filtered by the calling output provider
	return { Params.CameraToAffect->GetWorld()->GetFirstPlayerController() };
}
