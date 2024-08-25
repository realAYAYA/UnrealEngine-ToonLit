// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheUSDComponent.h"

#include "GeometryCache.h"
#include "GeometryCacheTrackUSD.h"
#include "GeometryCacheUSDSceneProxy.h"

FPrimitiveSceneProxy* UGeometryCacheUsdComponent::CreateSceneProxy()
{
	return new FGeometryCacheUsdSceneProxy(this);
}

void UGeometryCacheUsdComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Setup the members that are not included in the duplication
	PlayDirection = 1.f;

	SetupTrackData();
}

void UGeometryCacheUsdComponent::OnRegister()
{
	if (GeometryCache != nullptr)
	{
		for (UGeometryCacheTrack* Track : GeometryCache->Tracks)
		{
			if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
			{
				UsdTrack->RegisterStream();
			}
		}
	}

	ClearTrackData();
	SetupTrackData();

	// Skip code for UGeometryCacheComponent::OnRegister
	UMeshComponent::OnRegister();
}

void UGeometryCacheUsdComponent::OnUnregister()
{
	if (GeometryCache != nullptr)
	{
		for (UGeometryCacheTrack* Track : GeometryCache->Tracks)
		{
			if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
			{
				UsdTrack->UnregisterStream();
			}
		}
	}

	ClearTrackData();

	// Skip code for UGeometryCacheComponent::OnUnregister
	UMeshComponent::OnUnregister();
}
