// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraConcertCameraComponent.h"

#include "CineCameraComponent.h"
#include "ConcertVirtualCamera.h"
#include "VirtualCamera.h"


UDEPRECATED_VirtualCameraConcertCameraComponent::UDEPRECATED_VirtualCameraConcertCameraComponent(const FObjectInitializer& ObjectInitializer)
{ 
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bTickEvenWhenPaused = true;

	bUpdateCameraComponentTransform = true;
}


void UDEPRECATED_VirtualCameraConcertCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ComponentToTrack == nullptr)
	{
		if (GetOwner())
		{
			ComponentToTrack = GetOwner()->FindComponentByClass<UCineCameraComponent>();
		}
	}

	if (ComponentToTrack == nullptr)
	{
		UE_LOG(LogVirtualCamera, Error, TEXT("There is no camera to track for %s"), *GetFullName());
	}
	else if (ComponentToTrack->GetOwner())
	{
		UE_LOG(LogVirtualCamera, Warning, TEXT("The track camera %s doesn't have an owner."), *ComponentToTrack->GetFullName());
	}

	if (TrackingName.IsNone())
	{
		UE_LOG(LogVirtualCamera, Error, TEXT("%s doesn't have a tacking name."));
	}
}


void UDEPRECATED_VirtualCameraConcertCameraComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ComponentToTrack == nullptr || ComponentToTrack->GetOwner() == nullptr)
	{
		return;
	}

	FName CurrentTrackingName = TrackingName.IsNone() ? GetFName() : TrackingName;

	if (!bHasAuthority)
	{
		FConcertVirtualCameraCameraData CameraData;
		if (IVirtualCameraModule::Get().GetConcertVirtualCameraManager()->GetLatestCameraComponentEvent(CurrentTrackingName, CameraData))
		{
			CameraData.ApplyTo(ComponentToTrack->GetOwner(), ComponentToTrack, bUpdateCameraComponentTransform);
		}
	}
	else if (TickType != LEVELTICK_ViewportsOnly || bSendUpdateInEditor)
	{
		FConcertVirtualCameraCameraData CameraData = FConcertVirtualCameraCameraData(ComponentToTrack->GetOwner(), ComponentToTrack);
		IVirtualCameraModule::Get().GetConcertVirtualCameraManager()->SendCameraCompoentEvent(CurrentTrackingName, CameraData);
	}
}
