// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCorePointCloudRendererComponent.h"
#include "DrawDebugHelpers.h"
#include "GoogleARCoreTypes.h"
#include "ARBlueprintLibrary.h"


UDEPRECATED_GoogleARCorePointCloudRendererComponent::UDEPRECATED_GoogleARCorePointCloudRendererComponent()
	: PointColor(FColor::Red)
	, PointSize(0.1f)
{
	PreviousPointCloudTimestamp = 0.0;
	PrimaryComponentTick.bCanEverTick = true;
}

void UDEPRECATED_GoogleARCorePointCloudRendererComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	DrawPointCloud();
}

void UDEPRECATED_GoogleARCorePointCloudRendererComponent::DrawPointCloud()
{
	UWorld* World = GetWorld();
	if (UARBlueprintLibrary::GetTrackingQuality() != EARTrackingQuality::NotTracking)
	{
		const auto PointCloud = UARBlueprintLibrary::GetPointCloud();
		for (const auto& PointPosition : PointCloud)
		{
			DrawDebugPoint(World, PointPosition, PointSize, PointColor, false);
		}
	}
}
