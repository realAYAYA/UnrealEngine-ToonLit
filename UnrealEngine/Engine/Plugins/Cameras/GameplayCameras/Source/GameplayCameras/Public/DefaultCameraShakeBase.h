// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "DefaultCameraShakeBase.generated.h"

/**
 * Like UCameraShakeBase but with a perlin noise shake pattern by default, for convenience.
 */
UCLASS()
class GAMEPLAYCAMERAS_API UDefaultCameraShakeBase : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UDefaultCameraShakeBase(const FObjectInitializer& ObjInit);
};

