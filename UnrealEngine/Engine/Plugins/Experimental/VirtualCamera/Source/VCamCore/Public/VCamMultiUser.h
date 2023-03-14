// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "VCamMultiUser.generated.h"

struct FConcertSessionContext;
class AActor;
class IConcertClientSession;
class UCineCameraComponent;

USTRUCT()
struct FMultiUserVCamCameraFocusData
{
public:
	GENERATED_BODY()

	FMultiUserVCamCameraFocusData() = default;
	FMultiUserVCamCameraFocusData(const UCineCameraComponent* InCineCameraComponent);

	FCameraFocusSettings ToCameraFocusSettings() const;

	UPROPERTY()
	float ManualFocusDistance = 100000.f;
	UPROPERTY()
	float FocusSmoothingInterpSpeed = 8.f;
	UPROPERTY()
	bool bSmoothFocusChanges = false;
};

USTRUCT()
struct FMultiUserVCamCameraData
{
public:
	GENERATED_BODY()

	FMultiUserVCamCameraData()
		: CameraActorLocation(FVector::ZeroVector)
		, CameraActorRotation(FRotator::ZeroRotator)
		, CameraComponentLocation(FVector::ZeroVector)
		, CameraComponentRotation(FRotator::ZeroRotator)
		, CurrentAperture(0.f)
		, CurrentFocalLength(0.f)
	{}
	FMultiUserVCamCameraData(const AActor* InOwner, const UCineCameraComponent* InCineCameraComponent);

	void ApplyTo(AActor* InOwner, UCineCameraComponent* InCineCameraComponent) const;

	/** Camera transform */
	UPROPERTY()
	FVector CameraActorLocation;
	UPROPERTY()
	FRotator CameraActorRotation;
	UPROPERTY()
	FVector CameraComponentLocation;
	UPROPERTY()
	FRotator CameraComponentRotation;

	/** Camera settings */
	UPROPERTY()
	float CurrentAperture;
	UPROPERTY()
	float CurrentFocalLength;
	UPROPERTY()
	FMultiUserVCamCameraFocusData FocusSettings;
	UPROPERTY()
	FCameraLensSettings LensSettings;
	UPROPERTY()
	FCameraFilmbackSettings FilmbackSettings;
};


/**
 *
 */
USTRUCT()
struct FMultiUserVCamCameraComponentEvent
{
public:
	GENERATED_BODY()

	/** Name of the tracking camera */
	UPROPERTY()
	FString TrackingName;

	/** Camera data */
	UPROPERTY()
	FMultiUserVCamCameraData CameraData;
};

