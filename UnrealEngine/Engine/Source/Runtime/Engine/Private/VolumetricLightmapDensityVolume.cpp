// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lightmass/VolumetricLightmapDensityVolume.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VolumetricLightmapDensityVolume)


AVolumetricLightmapDensityVolume::AVolumetricLightmapDensityVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bColored = true;
	BrushColor.R = 155;
	BrushColor.G = 185;
	BrushColor.B = 25;
	BrushColor.A = 255;

	AllowedMipLevelRange = FInt32Interval(1, 3);
}


