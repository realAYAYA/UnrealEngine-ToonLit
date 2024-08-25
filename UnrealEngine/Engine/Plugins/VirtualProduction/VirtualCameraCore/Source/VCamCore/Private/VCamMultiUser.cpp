// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamMultiUser.h"

#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

FMultiUserVCamCameraFocusData::FMultiUserVCamCameraFocusData(const UCineCameraComponent* CineCamera)
	: ManualFocusDistance(CineCamera->CurrentFocusDistance)
	, FocusSmoothingInterpSpeed(CineCamera->FocusSettings.FocusSmoothingInterpSpeed)
	, bSmoothFocusChanges(CineCamera->FocusSettings.bSmoothFocusChanges)
	, RelativeOffset(CineCamera->FocusSettings.TrackingFocusSettings.RelativeOffset)
	, FocusMethod(CineCamera->FocusSettings.FocusMethod)
#if WITH_EDITORONLY_DATA
	, bDrawDebugFocusPlane(CineCamera->FocusSettings.bDrawDebugFocusPlane)
#endif
{
	ActorToTrack = CineCamera->FocusSettings.TrackingFocusSettings.ActorToTrack.ToSoftObjectPath().ToString();
}


FCameraFocusSettings FMultiUserVCamCameraFocusData::ToCameraFocusSettings() const
{
	FCameraFocusSettings Result;
	Result.FocusMethod = FocusMethod;
	Result.ManualFocusDistance = ManualFocusDistance;
	Result.bSmoothFocusChanges = bSmoothFocusChanges;
	Result.FocusSmoothingInterpSpeed = FocusSmoothingInterpSpeed;
	Result.FocusOffset = 0.f;

	Result.TrackingFocusSettings.ActorToTrack = FSoftObjectPath(ActorToTrack).ResolveObject();
	Result.TrackingFocusSettings.RelativeOffset = RelativeOffset;

#if WITH_EDITORONLY_DATA
	Result.bDrawDebugFocusPlane = bDrawDebugFocusPlane;
#endif
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

	CameraISO = InCineCameraComponent->PostProcessSettings.CameraISO;
	bOverride_CameraISO = InCineCameraComponent->PostProcessSettings.bOverride_CameraISO;

	CustomNearClipPlane = InCineCameraComponent->CustomNearClippingPlane;
	bOverride_NearClipPlane = InCineCameraComponent->bOverride_CustomNearClippingPlane;

	AutoExposureBias = InCineCameraComponent->PostProcessSettings.AutoExposureBias;
}


void FMultiUserVCamCameraData::ApplyTo(AActor* InOwner, UCineCameraComponent* InCineCameraComponent) const
{
	InOwner->Modify();
	InCineCameraComponent->Modify();

	InOwner->SetActorLocationAndRotation(CameraActorLocation, CameraActorRotation);
	InCineCameraComponent->SetRelativeLocationAndRotation(CameraComponentLocation, CameraComponentRotation);
	InCineCameraComponent->CurrentAperture = CurrentAperture;
	InCineCameraComponent->CurrentFocalLength = CurrentFocalLength;
	InCineCameraComponent->FocusSettings = FocusSettings.ToCameraFocusSettings();
	InCineCameraComponent->LensSettings = LensSettings;
	InCineCameraComponent->Filmback = FilmbackSettings;

	InCineCameraComponent->PostProcessSettings.bOverride_CameraISO = bOverride_CameraISO;
	InCineCameraComponent->PostProcessSettings.CameraISO = CameraISO;

	InCineCameraComponent->bOverride_CustomNearClippingPlane = bOverride_NearClipPlane;
	InCineCameraComponent->CustomNearClippingPlane = CustomNearClipPlane;

	InCineCameraComponent->PostProcessSettings.AutoExposureBias = AutoExposureBias;
}


