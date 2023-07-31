// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGVolume.h"
#include "PCGComponent.h"
#include "Engine/World.h"

APCGVolume::APCGVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGComponent = ObjectInitializer.CreateDefaultSubobject<UPCGComponent>(this, TEXT("PCG Component"));
}