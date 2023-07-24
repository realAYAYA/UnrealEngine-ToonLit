// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheUSDComponent.h"
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
