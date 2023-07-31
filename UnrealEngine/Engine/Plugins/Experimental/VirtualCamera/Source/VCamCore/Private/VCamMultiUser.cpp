// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamMultiUser.h"

#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"

FMultiUserVCamCameraFocusData::FMultiUserVCamCameraFocusData(const UCineCameraComponent* CineCamera)
	: ManualFocusDistance(CineCamera->CurrentFocusDistance)
	, FocusSmoothingInterpSpeed(CineCamera->FocusSettings.FocusSmoothingInterpSpeed)
	, bSmoothFocusChanges(CineCamera->FocusSettings.bSmoothFocusChanges)
{}


FCameraFocusSettings FMultiUserVCamCameraFocusData::ToCameraFocusSettings() const
{
	FCameraFocusSettings Result;
	Result.FocusMethod = ECameraFocusMethod::Manual;
	Result.ManualFocusDistance = ManualFocusDistance;
	Result.bSmoothFocusChanges = bSmoothFocusChanges;
	Result.FocusSmoothingInterpSpeed = FocusSmoothingInterpSpeed;
	Result.FocusOffset = 0.f;
	return Result;
}

FMultiUserVCamCameraData::FMultiUserVCamCameraData(const AActor* InOwner, const UCineCameraComponent* InCineCameraComponent)
{
	CameraActorLocation = InOwner->GetActorLocation();
	CameraActorRotation = InOwner->GetActorRotation();
	CameraComponentLocation = InCineCameraComponent->GetRelativeLocation();
	CameraComponentRotation = InCineCameraComponent->GetRelativeRotation();

	CurrentAperture = InCineCameraComponent->CurrentAperture;
	CurrentFocalLength = InCineCameraComponent->CurrentFocalLength;
	FocusSettings = FMultiUserVCamCameraFocusData(InCineCameraComponent);
	LensSettings = InCineCameraComponent->LensSettings;

	FilmbackSettings = InCineCameraComponent->Filmback;
}


void FMultiUserVCamCameraData::ApplyTo(AActor* InOwner, UCineCameraComponent* InCineCameraComponent) const
{
	InOwner->SetActorLocationAndRotation(CameraActorLocation, CameraActorRotation);
	InCineCameraComponent->SetRelativeLocationAndRotation(CameraComponentLocation, CameraComponentRotation);
	InCineCameraComponent->CurrentAperture = CurrentAperture;
	InCineCameraComponent->CurrentFocalLength = CurrentFocalLength;
	InCineCameraComponent->FocusSettings = FocusSettings.ToCameraFocusSettings();
	InCineCameraComponent->LensSettings = LensSettings;
	InCineCameraComponent->Filmback = FilmbackSettings;
}


