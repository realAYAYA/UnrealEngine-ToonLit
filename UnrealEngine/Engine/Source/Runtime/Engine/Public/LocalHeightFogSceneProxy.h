// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

class ULocalHeightFogComponent;

/** Represents a UVolumetricCloudComponent to the rendering thread, created game side from the component. */
class FLocalHeightFogSceneProxy
{
public:

	// Initialization constructor.
	ENGINE_API FLocalHeightFogSceneProxy(const ULocalHeightFogComponent* InComponent);
	ENGINE_API ~FLocalHeightFogSceneProxy();

	FTransform FogTransform;

	float FogDensity;
	float FogHeightFalloff;
	float FogHeightOffset;
	float FogRadialAttenuation;

	uint8 FogMode;
	uint8 FogSortPriority;

	float FogPhaseG;
	FLinearColor FogAlbedo;
	FLinearColor FogEmissive;
private:
};
