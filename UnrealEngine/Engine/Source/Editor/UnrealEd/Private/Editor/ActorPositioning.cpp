// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ActorPositioning.h"
#include "EngineDefines.h"
#include "CollisionQueryParams.h"
#include "PrimitiveViewRelevance.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Volume.h"
#include "Components/ModelComponent.h"
#include "Editor.h"
#include "ActorFactories/ActorFactory.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "SnappingUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeComponent.h"
#include "Editor/EditorPerProjectUserSettings.h"

FActorPositionTraceResult FActorPositioning::TraceWorldForPositionWithDefault(const FViewportCursorLocation& Cursor, const FSceneView& View, const TArray<AActor*>* IgnoreActors)
{
	FActorPositionTraceResult Results = TraceWorldForPosition(Cursor, View, IgnoreActors);
	if (Results.State == FActorPositionTraceResult::Failed)
	{
		Results.State = FActorPositionTraceResult::Default;

		// And put it in front of the camera
		const float DistanceMultiplier = ( Cursor.GetViewportType() == LVT_Perspective ) ? GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance : 0.0f;
		Results.Location = Cursor.GetOrigin() + Cursor.GetDirection() * DistanceMultiplier;
	}
	return Results;
}

FActorPositionTraceResult FActorPositioning::TraceWorldForPosition(const FViewportCursorLocation& Cursor, const FSceneView& View, const TArray<AActor*>* IgnoreActors)
{
	const auto* ViewportClient = Cursor.GetViewportClient();

	FActorPositionTraceResult Results;

	const auto ViewportType = ViewportClient->GetViewportType();

	// Start with a ray that encapsulates the entire world
	FVector RayStart = Cursor.GetOrigin();
	if (ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoXZ || ViewportType == LVT_OrthoYZ ||
		ViewportType == LVT_OrthoNegativeXY || ViewportType == LVT_OrthoNegativeXZ || ViewportType == LVT_OrthoNegativeYZ)
	{
		RayStart -= Cursor.GetDirection() * HALF_WORLD_MAX/2;
	}

	const FVector RayEnd = RayStart + Cursor.GetDirection() * HALF_WORLD_MAX;
	return TraceWorldForPosition(*ViewportClient->GetWorld(), View, RayStart, RayEnd, IgnoreActors);
}

/** Check to see if the specified hit result should be ignored from actor positioning calculations for the specified scene view */
bool IsHitIgnored(const FHitResult& InHit, const FSceneView& InSceneView)
{
	// We're using the SceneProxy and ViewRelevance here, we should execute from the render thread but since
	// we're also accessing UObjects, the game thread should be blocked during this call.
	check(IsInParallelRenderingThread());
	const FActorInstanceHandle& HitObjHandle = InHit.HitObjectHandle;
	
	// Try and find a primitive component for the hit
	const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(HitObjHandle.GetRootComponent());

	if (!PrimitiveComponent)
	{
		PrimitiveComponent = InHit.Component.Get();
	}
	if (PrimitiveComponent && PrimitiveComponent->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass()))
	{
		PrimitiveComponent = CastChecked<ULandscapeHeightfieldCollisionComponent>(PrimitiveComponent)->RenderComponent.Get();
	}

	if (InHit.bStartPenetrating || !PrimitiveComponent)
	{
		return true;
	}

	// Ignore volumes and shapes
	if (HitObjHandle.DoesRepresentClass(AVolume::StaticClass()))
	{
		return true;
	}
	else if (PrimitiveComponent->IsA(UShapeComponent::StaticClass()))
	{
		return true;
	}

	// Only use this component if it is visible in the specified scene views
	bool bIsRenderedOnScreen = false;
	bool bIgnoreTranslucentPrimitive = false;
	const bool bConsiderInvisibleComponentForPlacement = PrimitiveComponent->bConsiderForActorPlacementWhenHidden;
	{				
		if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
		{
			const FPrimitiveViewRelevance ViewRelevance = PrimitiveComponent->SceneProxy->GetViewRelevance(&InSceneView);
			// BSP is a bit special in that its bDrawRelevance is false even when drawn as wireframe because InSceneView.Family->EngineShowFlags.BSPTriangles is off
			bIsRenderedOnScreen = ViewRelevance.bDrawRelevance || (PrimitiveComponent->IsA(UModelComponent::StaticClass()) && InSceneView.Family->EngineShowFlags.BSP);
			bIgnoreTranslucentPrimitive = ViewRelevance.HasTranslucency() && !GetDefault<UEditorPerProjectUserSettings>()->bAllowSelectTranslucent;
		}
	}

	return (!bIsRenderedOnScreen && !bConsiderInvisibleComponentForPlacement) || bIgnoreTranslucentPrimitive;
}

FActorPositionTraceResult FActorPositioning::TraceWorldForPosition(const UWorld& InWorld, const FSceneView& InSceneView, const FVector& RayStart, const FVector& RayEnd, const TArray<AActor*>* IgnoreActors)
{
	TArray<FHitResult> Hits;

	FCollisionQueryParams Param(SCENE_QUERY_STAT(DragDropTrace), true);
	
	if (IgnoreActors)
	{
		Param.AddIgnoredActors(*IgnoreActors);
	}

	FActorPositionTraceResult Results;
	if ( InWorld.LineTraceMultiByObjectType(Hits, RayStart, RayEnd, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects), Param) )
	{
		{
			// Send IsHitIgnored on the render thread since we're accessing view relevance
			ENQUEUE_RENDER_COMMAND(TraceWorldForPosition_FilterHitsByViewRelevance)(
				[&Hits, &InSceneView](FRHICommandListImmediate& RHICmdList)
				{
					// Filter out anything that should be ignored
					Hits.RemoveAll([&InSceneView](const FHitResult& Hit){
						return IsHitIgnored(Hit, InSceneView);
					});
				}
			);
			
			// We need the result to come back before continuing
			FRenderCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();
		}

		// Go through all hits and find closest
		float ClosestHitDistanceSqr = TNumericLimits<float>::Max();

		for (const FHitResult& Hit : Hits)
		{
			const double DistanceToHitSqr = (Hit.ImpactPoint - RayStart).SizeSquared();
			if (DistanceToHitSqr < ClosestHitDistanceSqr)
			{
				ClosestHitDistanceSqr = DistanceToHitSqr;
				Results.Location = Hit.Location;
				Results.SurfaceNormal = Hit.Normal.GetSafeNormal();
				Results.State = FActorPositionTraceResult::HitSuccess;
				Results.HitActor = Hit.HitObjectHandle.GetManagingActor();
			}
		}
	}

	return Results;
}

FTransform FActorPositioning::GetCurrentViewportPlacementTransform(const AActor& Actor, bool bSnap, const FViewportCursorLocation* InCursor)
{
	FTransform ActorTransform = FTransform::Identity;
	if (GCurrentLevelEditingViewportClient)
	{
		// Get cursor origin and direction in world space.
		FViewportCursorLocation CursorLocation = InCursor ? *InCursor : GCurrentLevelEditingViewportClient->GetCursorWorldLocationFromMousePos();
		const auto CursorPos = CursorLocation.GetCursorPos();

		if (CursorLocation.GetViewportType() == LVT_Perspective && !GCurrentLevelEditingViewportClient->Viewport->GetHitProxy(CursorPos.X, CursorPos.Y))
		{
			ActorTransform.SetTranslation(GetActorPositionInFrontOfCamera(Actor, CursorLocation.GetOrigin(), CursorLocation.GetDirection()));
			return ActorTransform;
		}
	}

	const FSnappedPositioningData PositioningData = FSnappedPositioningData(nullptr, GEditor->ClickLocation, GEditor->ClickPlane)
		.DrawSnapHelpers(true)
		.UseFactory(GEditor->FindActorFactoryForActorClass(Actor.GetClass()))
		.UsePlacementExtent(Actor.GetPlacementExtent());

	ActorTransform = bSnap ? GetSnappedSurfaceAlignedTransform(PositioningData) : GetSurfaceAlignedTransform(PositioningData);

	if (GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled)
	{
		// HACK: If we are aligning rotation to surfaces, we have to factor in the inverse of the actor's rotation and translation so that the resulting transform after SpawnActor is correct.

		if (auto* RootComponent = Actor.GetRootComponent())
		{
			RootComponent->UpdateComponentToWorld();
		}

		FVector OrigActorScale3D = ActorTransform.GetScale3D();
		ActorTransform = Actor.GetTransform().Inverse() * ActorTransform;
		ActorTransform.SetScale3D(OrigActorScale3D);
	}

	return ActorTransform;
}

FVector FActorPositioning::GetActorPositionInFrontOfCamera(const AActor& InActor, const FVector& InCameraOrigin, const FVector& InCameraDirection)
{
	// Get the  radius of the actors bounding cylinder.  Height is not needed.
	float CylRadius, CylHeight;
	InActor.GetComponentsBoundingCylinder(CylRadius, CylHeight);

	// a default cylinder radius if no bounding cylinder exists.  
	const float	DefaultCylinderRadius = 50.0f;

	if( CylRadius == 0.0f )
	{
		// If the actor does not have a bounding cylinder, use a default value.
		CylRadius = DefaultCylinderRadius;
	}

	// The new location the cameras origin offset by the actors bounding cylinder radius down the direction of the cameras view. 
	FVector NewLocation = InCameraOrigin + InCameraDirection * CylRadius + InCameraDirection * GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance;

	// Snap the new location if snapping is enabled
	FSnappingUtils::SnapPointToGrid( NewLocation, FVector::ZeroVector );
	return NewLocation;
}

FTransform FActorPositioning::GetSurfaceAlignedTransform(const FPositioningData& Data)
{
	// Sort out the rotation first, then do the location
	FQuat RotatorQuat = Data.StartTransform.GetRotation();

	if (Data.ActorFactory)
	{
		RotatorQuat = Data.ActorFactory->AlignObjectToSurfaceNormal(Data.SurfaceNormal, RotatorQuat);
	}

	// Choose the largest location offset of the various options (global viewport settings, collision, factory offset)
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const float SnapOffsetExtent = (ViewportSettings->SnapToSurface.bEnabled) ? (ViewportSettings->SnapToSurface.SnapOffsetExtent) : (0.0f);
	const FVector PlacementExtent = (Data.ActorFactory && !Data.ActorFactory->bUsePlacementExtent) ? FVector::ZeroVector : Data.PlacementExtent;
	const double CollisionOffsetExtent = FVector::BoxPushOut(Data.SurfaceNormal, PlacementExtent);

	FVector LocationOffset = Data.SurfaceNormal * FMath::Max(SnapOffsetExtent, CollisionOffsetExtent);
	if (Data.ActorFactory && LocationOffset.SizeSquared() < Data.ActorFactory->SpawnPositionOffset.SizeSquared())
	{
		// Rotate the Spawn Position Offset to match our rotation
		LocationOffset = RotatorQuat.RotateVector(-Data.ActorFactory->SpawnPositionOffset);
	}

	return FTransform(Data.bAlignRotation ? RotatorQuat : Data.StartTransform.GetRotation(), Data.SurfaceLocation + LocationOffset);
}

FTransform FActorPositioning::GetSnappedSurfaceAlignedTransform(const FSnappedPositioningData& Data)
{
	FVector SnappedLocation = Data.SurfaceLocation;
	FSnappingUtils::SnapPointToGrid(SnappedLocation, FVector(0.f));

	// Secondly, attempt vertex snapping
	FVector AlignToNormal;
	if (!Data.LevelViewportClient || !FSnappingUtils::SnapLocationToNearestVertex( SnappedLocation, Data.LevelViewportClient->GetDropPreviewLocation(), Data.LevelViewportClient, AlignToNormal, Data.bDrawSnapHelpers ))
	{
		AlignToNormal = Data.SurfaceNormal;
	}

	return GetSurfaceAlignedTransform(Data);
}
