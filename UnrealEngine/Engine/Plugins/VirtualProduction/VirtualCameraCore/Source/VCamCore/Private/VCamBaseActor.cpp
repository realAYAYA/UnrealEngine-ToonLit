// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamBaseActor.h"

#include "VCamComponent.h"

AVCamBaseActor::AVCamBaseActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VCamComponent = ObjectInitializer.CreateDefaultSubobject<UVCamComponent>(this, TEXT("VCam"));
	VCamComponent->SetupAttachment(GetCameraComponent());
}