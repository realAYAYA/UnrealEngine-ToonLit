// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCorePlaneRendererComponent.h"
#include "ARBlueprintLibrary.h"
#include "ARSystem.h"
#include "ARTrackable.h"
#include "DrawDebugHelpers.h"
#include "Templates/Casts.h"

UDEPRECATED_GoogleARCorePlaneRendererComponent::UDEPRECATED_GoogleARCorePlaneRendererComponent()
	: bRenderPlane(true)
	, bRenderBoundaryPolygon(true)
	, PlaneColor(FColor::Green)
	, BoundaryPolygonColor(FColor::Blue)
	, BoundaryPolygonThickness(0.5f)
{
	PrimaryComponentTick.bCanEverTick = true;

	PlaneIndices.AddUninitialized(6);
	PlaneIndices[0] = 0; PlaneIndices[1] = 1; PlaneIndices[2] = 2;
	PlaneIndices[3] = 0; PlaneIndices[4] = 2; PlaneIndices[5] = 3;
}

void UDEPRECATED_GoogleARCorePlaneRendererComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	DrawPlanes();
}

void UDEPRECATED_GoogleARCorePlaneRendererComponent::DrawPlanes()
{
	UWorld* World = GetWorld();
	if (UARBlueprintLibrary::GetTrackingQuality() == EARTrackingQuality::OrientationAndPosition)
	{
		TArray<UARTrackedGeometry*> PlaneList;
		PlaneList = UARBlueprintLibrary::GetAllGeometries();
		for (UARTrackedGeometry* TrackedGeometry : PlaneList)
		{
			if (!TrackedGeometry->IsA(UARPlaneGeometry::StaticClass()))
			{
				continue;
			}

			if (TrackedGeometry->GetTrackingState() != EARTrackingState::Tracking)
			{
				continue;
			}

			UARPlaneGeometry* Plane = Cast<UARPlaneGeometry>(TrackedGeometry);
			if (bRenderPlane)
			{
				FTransform BoundingBoxTransform = Plane->GetLocalToWorldTransform();
				FVector BoundingBoxLocation = BoundingBoxTransform.GetLocation();
				FVector BoundingBoxSize = Plane->GetExtent();

				PlaneVertices.Empty();
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxSize.X, -BoundingBoxSize.Y, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(-BoundingBoxSize.X, BoundingBoxSize.Y, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxSize.X, BoundingBoxSize.Y, 0)));
				PlaneVertices.Add(BoundingBoxTransform.TransformPosition(FVector(BoundingBoxSize.X, -BoundingBoxSize.Y, 0)));
				// plane quad
				DrawDebugMesh(World, PlaneVertices, PlaneIndices, PlaneColor);
			}

			if (bRenderBoundaryPolygon)
			{
				const TArray<FVector>& BoundaryPolygonData = Plane->GetBoundaryPolygonInLocalSpace();
				FTransform PlaneCenter = Plane->GetLocalToWorldTransform();
				for (int i = 0; i < BoundaryPolygonData.Num(); i++)
				{
					FVector Start = PlaneCenter.TransformPosition(BoundaryPolygonData[i]);
					FVector End = PlaneCenter.TransformPosition(BoundaryPolygonData[(i + 1) % BoundaryPolygonData.Num()]);
					DrawDebugLine(World, Start, End, BoundaryPolygonColor, false, -1.f, 0, BoundaryPolygonThickness);
				}
			}

		}
	}
}
