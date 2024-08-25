// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "CineCameraSettings.h"

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "VCamMultiUser.generated.h"

struct FConcertSessionContext;
class AActor;
class IConcertClientSession;
class UCineCameraComponent;

USTRUCT()
struct VCAMCORE_API FMultiUserVCamCameraFocusData
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

	UPROPERTY()
	FString ActorToTrack;

	UPROPERTY()
	FVector RelativeOffset = FVector::ZeroVector;

	UPROPERTY()
	ECameraFocusMethod FocusMethod = ECameraFocusMethod::DoNotOverride;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint8 bDrawDebugFocusPlane : 1;
#endif
};

USTRUCT()
struct VCAMCORE_API FMultiUserVCamCameraData
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
		, bOverride_NearClipPlane(false)
		, bOverride_CameraISO(false)
		, CustomNearClipPlane(1.0f)
		, CameraISO(100.0f)
		, AutoExposureBias(1.0f)
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

	/** Post process settings */
	UPROPERTY()
	uint8 bOverride_NearClipPlane : 1;
	UPROPERTY()
	uint8 bOverride_CameraISO:1;
	UPROPERTY()
	float CustomNearClipPlane;
	UPROPERTY()
	float CameraISO;
	UPROPERTY()
	float AutoExposureBias;
};


/**
 *
 */
USTRUCT()
struct VCAMCORE_API FMultiUserVCamCameraComponentEvent
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

