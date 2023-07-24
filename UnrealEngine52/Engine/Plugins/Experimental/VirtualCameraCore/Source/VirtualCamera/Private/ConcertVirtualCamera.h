// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraComponent.h"
#include "VirtualCameraPlayerControllerBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "ConcertVirtualCamera.generated.h"

struct FConcertSessionContext;
class AActor;
class IConcertClientSession;
class UCineCameraComponent;

/**
 * Can't use FCameraFocusSettings since it use a reference to an actor
 * The camera will always be in Manual, and will transfer the CurrentFocusDistance.
 */
USTRUCT()
struct FConcertVirtualCameraCameraFocusData
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraCameraFocusData();
	FConcertVirtualCameraCameraFocusData(const UCineCameraComponent* InCineCameraComponent);

	FCameraFocusSettings ToCameraFocusSettings() const;

	UPROPERTY()
	float ManualFocusDistance;
	UPROPERTY()
	float FocusSmoothingInterpSpeed;
	UPROPERTY()
	uint8 bSmoothFocusChanges : 1;
};


/**
 *
 */
USTRUCT()
struct FConcertVirtualCameraCameraData
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraCameraData();
	FConcertVirtualCameraCameraData(const AActor* InOwner, const UCineCameraComponent* InCineCameraComponent);

	void ApplyTo(AActor* InOwner, UCineCameraComponent* InCineCameraComponent, bool bUpdateCameraComponentTransform);

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
	FConcertVirtualCameraCameraFocusData FocusSettings;
	UPROPERTY()
	FCameraLensSettings LensSettings;
	UPROPERTY()
	FCameraFilmbackSettings FilmbackSettings;
};


/**
 *
 */
USTRUCT()
struct FConcertVirtualCameraCameraComponentEvent
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraCameraComponentEvent();

	/** Name of the tracking camera */
	UPROPERTY()
	FName TrackingName;

	/** Camera data */
	UPROPERTY()
	FConcertVirtualCameraCameraData CameraData;
};


/**
 *
 */
USTRUCT()
struct FConcertVirtualCameraControllerEvent
{
public:
	GENERATED_BODY()

	FConcertVirtualCameraControllerEvent();

	/** Controller settings */
	UPROPERTY()
	ETrackerInputSource InputSource;

	/** Camera data */
	UPROPERTY()
	FConcertVirtualCameraCameraData CameraData;
};



#if VIRTUALCAMERA_WITH_CONCERT // Concert is only available in development mode

/**
 *
 */
class FConcertVirtualCameraManager
{
public:
	FConcertVirtualCameraManager();
	FConcertVirtualCameraManager(const FConcertVirtualCameraManager&) = delete;
	FConcertVirtualCameraManager& operator=(const FConcertVirtualCameraManager&) = delete;
	~FConcertVirtualCameraManager();

public:
	bool GetLatestCameraComponentEvent(FName TrackingName, FConcertVirtualCameraCameraData& OutCameraData) const;
	void SendCameraCompoentEvent(FName TrackingName, const FConcertVirtualCameraCameraData& InCameraData);

	bool GetLatestControllerCameraEvent(FConcertVirtualCameraControllerEvent& OutCameraEvent) const;
	void SendControllerCameraEvent(const FConcertVirtualCameraControllerEvent& InCameraEvent);

private:
	void RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);
	void UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession);

	void HandleCameraComponentEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraCameraComponentEvent& InEvent);
	void HandleControllerCameraEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraControllerEvent& InEvent);

private:
	/** Latest event data */
	TArray<FConcertVirtualCameraCameraComponentEvent> LatestCameraEventDatas;
	FConcertVirtualCameraControllerEvent LatestControllerCameraEventData;
	bool bIsLatestControllerCameraEventDataValid;

	/** Delegate handle for a the callback when a session starts up */
	FDelegateHandle OnSessionStartupHandle;

	/** Delegate handle for a the callback when a session shuts down */
	FDelegateHandle OnSessionShutdownHandle;

	/** Weak pointer to the client session with which to send events. May be null or stale. */
	TWeakPtr<IConcertClientSession> WeakSession;
};

#else //VIRTUALCAMERA_WITH_CONCERT

class FConcertVirtualCameraManager
{
public:
	bool GetLatestCameraComponentEvent(FName TrackingName, FConcertVirtualCameraCameraData& OutCameraData) const;
	void SendCameraCompoentEvent(FName TrackingName, const FConcertVirtualCameraCameraData& InCameraData);

	bool GetLatestControllerCameraEvent(FConcertVirtualCameraControllerEvent& OutCameraEvent) const;
	void SendControllerCameraEvent(const FConcertVirtualCameraControllerEvent& InCameraEvent);
};

#endif //VIRTUALCAMERA_WITH_CONCERT
