// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FVolumetricCloudSceneProxy implementation.
=============================================================================*/

#include "LocalHeightFogSceneProxy.h"
#include "Components/LocalHeightFogComponent.h"



FLocalHeightFogSceneProxy::FLocalHeightFogSceneProxy(const ULocalHeightFogComponent* InComponent)
	: FogTransform(InComponent->GetComponentTransform())
	, FogDensity(InComponent->FogDensity)
	, FogHeightFalloff(InComponent->FogHeightFalloff)
	, FogHeightOffset(InComponent->FogHeightOffset)
	, FogRadialAttenuation(InComponent->FogRadialAttenuation)
	, FogMode((uint8)InComponent->FogMode)
	, FogSortPriority(uint8(127 - int8(InComponent->FogSortPriority))) // FogSortPriority on the component is in [-127,127] and needs to be negated to match expected priority behavior.
	, FogPhaseG(InComponent->FogPhaseG)
	, FogAlbedo(InComponent->FogAlbedo)
	, FogEmissive(InComponent->FogEmissive)
{
}

FLocalHeightFogSceneProxy::~FLocalHeightFogSceneProxy()
{
}


