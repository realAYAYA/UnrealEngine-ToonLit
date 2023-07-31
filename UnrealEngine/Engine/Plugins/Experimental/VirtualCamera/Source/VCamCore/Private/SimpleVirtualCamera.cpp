// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleVirtualCamera.h"

void ASimpleVirtualCamera::PostActorCreated()
{
	Super::PostActorCreated();

	// Don't run on CDO
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		VCamComponent = NewObject<UVCamComponent>(this, TEXT("VCamComponent"));
		if (VCamComponent)
		{
			VCamComponent->AttachToComponent(GetCineCameraComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			VCamComponent->RegisterComponent();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SimpleVirtualCamera - unable to create VCamComponent"));
		}
	}
}
