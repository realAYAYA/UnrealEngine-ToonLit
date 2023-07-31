// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultCameraShakeBase.h"
#include "PerlinNoiseCameraShakePattern.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultCameraShakeBase)

UDefaultCameraShakeBase::UDefaultCameraShakeBase(const FObjectInitializer& ObjInit)
	: Super(ObjInit
				.SetDefaultSubobjectClass<UPerlinNoiseCameraShakePattern>(TEXT("RootShakePattern")))
{
}


