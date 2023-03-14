// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertVirtualCamera.h"

#if VIRTUALCAMERA_WITH_CONCERT
#include "CineCameraComponent.h"
#include "GameFramework/Actor.h"
#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "IConcertSyncClient.h"
#include "IMultiUserClientModule.h"
#include "VirtualCameraCineCameraComponent.h"
#endif


/**
 *
 */
FConcertVirtualCameraCameraFocusData::FConcertVirtualCameraCameraFocusData()
	: ManualFocusDistance(100000.f)
	, FocusSmoothingInterpSpeed(8.f)
	, bSmoothFocusChanges(false)
{}


FConcertVirtualCameraCameraFocusData::FConcertVirtualCameraCameraFocusData(const UCineCameraComponent* CineCamera)
	: ManualFocusDistance(CineCamera->CurrentFocusDistance)
	, FocusSmoothingInterpSpeed(CineCamera->FocusSettings.FocusSmoothingInterpSpeed)
	, bSmoothFocusChanges(CineCamera->FocusSettings.bSmoothFocusChanges)
{}


FCameraFocusSettings FConcertVirtualCameraCameraFocusData::ToCameraFocusSettings() const
{
	FCameraFocusSettings Result;
	Result.FocusMethod = ECameraFocusMethod::Manual;
	Result.ManualFocusDistance = ManualFocusDistance;
	Result.bSmoothFocusChanges = bSmoothFocusChanges;
	Result.FocusSmoothingInterpSpeed = FocusSmoothingInterpSpeed;
	Result.FocusOffset = 0.f;
	return Result;
}


/**
 *
 */
FConcertVirtualCameraCameraData::FConcertVirtualCameraCameraData()
	: CameraActorLocation(FVector::ZeroVector)
	, CameraActorRotation(FRotator::ZeroRotator)
	, CameraComponentLocation(FVector::ZeroVector)
	, CameraComponentRotation(FRotator::ZeroRotator)
	, CurrentAperture(0.f)
	, CurrentFocalLength(0.f)
{}


FConcertVirtualCameraCameraData::FConcertVirtualCameraCameraData(const AActor* InOwner, const UCineCameraComponent* InCineCameraComponent)
{
	CameraActorLocation = InOwner->GetActorLocation();
	CameraActorRotation = InOwner->GetActorRotation();
	CameraComponentLocation = InCineCameraComponent->GetRelativeLocation();
	CameraComponentRotation = InCineCameraComponent->GetRelativeRotation();

	CurrentAperture = InCineCameraComponent->CurrentAperture;
	CurrentFocalLength = InCineCameraComponent->CurrentFocalLength;
	FocusSettings = FConcertVirtualCameraCameraFocusData(InCineCameraComponent);
	LensSettings = InCineCameraComponent->LensSettings;

	if (const UVirtualCameraCineCameraComponent* VCCineCamCmp = Cast<UVirtualCameraCineCameraComponent>(InCineCameraComponent))
	{
		FilmbackSettings = VCCineCamCmp->DesiredFilmbackSettings;
	}
	else
	{
		FilmbackSettings = InCineCameraComponent->Filmback;
	}
}


void FConcertVirtualCameraCameraData::ApplyTo(AActor* InOwner, UCineCameraComponent* InCineCameraComponent, bool bUpdateCameraComponentTransform)
{
	InOwner->SetActorLocationAndRotation(CameraActorLocation, CameraActorRotation);
	if (bUpdateCameraComponentTransform)
	{
		InCineCameraComponent->SetRelativeLocationAndRotation(CameraComponentLocation, CameraComponentRotation);
	}
	InCineCameraComponent->CurrentAperture = CurrentAperture;
	InCineCameraComponent->CurrentFocalLength = CurrentFocalLength;
	InCineCameraComponent->FocusSettings = FocusSettings.ToCameraFocusSettings();
	InCineCameraComponent->LensSettings = LensSettings;
	InCineCameraComponent->Filmback = FilmbackSettings;

	if (UVirtualCameraCineCameraComponent* VCCineCamCmp = Cast<UVirtualCameraCineCameraComponent>(InCineCameraComponent))
	{
		VCCineCamCmp->DesiredFilmbackSettings = FilmbackSettings;
	}
}


/**
 *
 */
FConcertVirtualCameraCameraComponentEvent::FConcertVirtualCameraCameraComponentEvent()
{}


/**
 *
 */
FConcertVirtualCameraControllerEvent::FConcertVirtualCameraControllerEvent()
	: InputSource(ETrackerInputSource::ARKit)
{}



#if VIRTUALCAMERA_WITH_CONCERT

/**
 *
 */
FConcertVirtualCameraManager::FConcertVirtualCameraManager()
	: bIsLatestControllerCameraEventDataValid(false)
{
	if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = IMultiUserClientModule::Get().GetClient())
	{
		IConcertClientRef ConcertClient = ConcertSyncClient->GetConcertClient();

		OnSessionStartupHandle = ConcertClient->OnSessionStartup().AddRaw(this, &FConcertVirtualCameraManager::RegisterConcertSyncHandlers);
		OnSessionShutdownHandle = ConcertClient->OnSessionShutdown().AddRaw(this, &FConcertVirtualCameraManager::UnregisterConcertSyncHandlers);

		TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
		if (ConcertClientSession.IsValid())
		{
			RegisterConcertSyncHandlers(ConcertClientSession.ToSharedRef());
		}
	}
}


FConcertVirtualCameraManager::~FConcertVirtualCameraManager()
{
	if (IMultiUserClientModule::IsAvailable())
	{
		if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = IMultiUserClientModule::Get().GetClient())
		{
			IConcertClientRef ConcertClient = ConcertSyncClient->GetConcertClient();

			TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
			if (ConcertClientSession.IsValid())
			{
				UnregisterConcertSyncHandlers(ConcertClientSession.ToSharedRef());
			}

			ConcertClient->OnSessionStartup().Remove(OnSessionStartupHandle);
			OnSessionStartupHandle.Reset();

			ConcertClient->OnSessionShutdown().Remove(OnSessionShutdownHandle);
			OnSessionShutdownHandle.Reset();
		}
	}
}


bool FConcertVirtualCameraManager::GetLatestCameraComponentEvent(FName InTrackingName, FConcertVirtualCameraCameraData& OutCameraEvent) const
{
	for (const FConcertVirtualCameraCameraComponentEvent& CameraEvent : LatestCameraEventDatas)
	{
		if (CameraEvent.TrackingName == InTrackingName)
		{
			OutCameraEvent = CameraEvent.CameraData;
			return true;
		}
	}

	return false;
}


void FConcertVirtualCameraManager::SendCameraCompoentEvent(FName InTrackingName, const FConcertVirtualCameraCameraData& InCameraEvent)
{
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid() && InTrackingName!= NAME_None)
	{
		FConcertVirtualCameraCameraComponentEvent CameraEvent;
		CameraEvent.TrackingName = InTrackingName;
		CameraEvent.CameraData = InCameraEvent;
		TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
		Session->SendCustomEvent(CameraEvent, ClientIds, EConcertMessageFlags::None);
	}
}


bool FConcertVirtualCameraManager::GetLatestControllerCameraEvent(FConcertVirtualCameraControllerEvent& OutCameraEvent) const
{
	OutCameraEvent = LatestControllerCameraEventData;
	return bIsLatestControllerCameraEventDataValid;
}


void FConcertVirtualCameraManager::SendControllerCameraEvent(const FConcertVirtualCameraControllerEvent& InCameraEvent)
{
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
		Session->SendCustomEvent(InCameraEvent, ClientIds, EConcertMessageFlags::None);
	}
}


void FConcertVirtualCameraManager::RegisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
{
	// Hold onto the session so we can trigger events
	WeakSession = InSession;

	// Register our events
	InSession->RegisterCustomEventHandler<FConcertVirtualCameraCameraComponentEvent>(this, &FConcertVirtualCameraManager::HandleCameraComponentEventData);
	InSession->RegisterCustomEventHandler<FConcertVirtualCameraControllerEvent>(this, &FConcertVirtualCameraManager::HandleControllerCameraEventData);
}


void FConcertVirtualCameraManager::UnregisterConcertSyncHandlers(TSharedRef<IConcertClientSession> InSession)
{
	// Unregister our events and explicitly reset the session ptr
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		Session->UnregisterCustomEventHandler<FConcertVirtualCameraControllerEvent>(this);
		Session->UnregisterCustomEventHandler<FConcertVirtualCameraCameraComponentEvent>(this);
	}

	WeakSession.Reset();
}


void FConcertVirtualCameraManager::HandleCameraComponentEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraCameraComponentEvent& InEvent)
{
	if (InEvent.TrackingName == NAME_None)
	{
		return;
	}

	for (FConcertVirtualCameraCameraComponentEvent& CameraEvent : LatestCameraEventDatas)
	{
		if (CameraEvent.TrackingName == InEvent.TrackingName)
		{
			CameraEvent.CameraData = InEvent.CameraData;
			return;
		}
	}

	LatestCameraEventDatas.Add(InEvent);
}


void FConcertVirtualCameraManager::HandleControllerCameraEventData(const FConcertSessionContext& InEventContext, const FConcertVirtualCameraControllerEvent& InEvent)
{
	LatestControllerCameraEventData = InEvent;
	bIsLatestControllerCameraEventDataValid = true;
}


#else //#if VIRTUALCAMERA_WITH_CONCERT


bool FConcertVirtualCameraManager::GetLatestCameraComponentEvent(FName TrackingName, FConcertVirtualCameraCameraData& OutCameraEvent) const
{
	return false;
}


void FConcertVirtualCameraManager::SendCameraCompoentEvent(FName InTrackingName, const FConcertVirtualCameraCameraData& InCameraEvent)
{
}


bool FConcertVirtualCameraManager::GetLatestControllerCameraEvent(FConcertVirtualCameraControllerEvent& OutCameraEvent) const
{
	return false;
}


void FConcertVirtualCameraManager::SendControllerCameraEvent(const FConcertVirtualCameraControllerEvent& InCameraEvent)
{
}


#endif //#if VIRTUALCAMERA_WITH_CONCERT
