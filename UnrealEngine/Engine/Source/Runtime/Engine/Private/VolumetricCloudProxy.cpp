// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FVolumetricCloudSceneProxy implementation.
=============================================================================*/

#include "VolumetricCloudProxy.h"
#include "Components/VolumetricCloudComponent.h"



FVolumetricCloudSceneProxy::FVolumetricCloudSceneProxy(const UVolumetricCloudComponent* InComponent)
	: LayerBottomAltitudeKm(InComponent->LayerBottomAltitude)
	, LayerHeightKm(InComponent->LayerHeight)
	, TracingStartMaxDistance(InComponent->TracingStartMaxDistance)
	, TracingMaxDistanceMode(uint8(InComponent->TracingMaxDistanceMode))
	, TracingMaxDistance(InComponent->TracingMaxDistance)
	, PlanetRadiusKm(InComponent->PlanetRadius)
	, GroundAlbedo(InComponent->GroundAlbedo)
	, bUsePerSampleAtmosphericLightTransmittance(InComponent->bUsePerSampleAtmosphericLightTransmittance)
	, SkyLightCloudBottomOcclusion(InComponent->SkyLightCloudBottomOcclusion)
	, ViewSampleCountScale(InComponent->ViewSampleCountScale)
	, ReflectionViewSampleCountScale(InComponent->ReflectionViewSampleCountScaleValue)
	, ShadowViewSampleCountScale(InComponent->ShadowViewSampleCountScale)
	, ShadowReflectionViewSampleCountScale(InComponent->ShadowReflectionViewSampleCountScaleValue)
	, ShadowTracingDistance(InComponent->ShadowTracingDistance)
	, StopTracingTransmittanceThreshold(InComponent->StopTracingTransmittanceThreshold)
	, CloudVolumeMaterial(InComponent->Material)
{
}

FVolumetricCloudSceneProxy::~FVolumetricCloudSceneProxy()
{
}


