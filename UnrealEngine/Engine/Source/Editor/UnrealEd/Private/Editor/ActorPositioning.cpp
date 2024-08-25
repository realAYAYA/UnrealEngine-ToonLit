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
#include "HAL/IConsoleManager.h" // FAutoConsoleVariableRef
#include "LevelEditorViewport.h"
#include "SnappingUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeComponent.h"
#include "Editor/EditorPerProjectUserSettings.h"

namespace ActorPositioningLocals
{
	bool bAllowNonPrimitiveComponentHits = true;
	static FAutoConsoleVariableRef CVarAllowNonPrimitiveComponentHits(
		TEXT("PlacementMode.AllowNonPrimitiveComponentHits"),
		bAllowNonPrimitiveComponentHits,
		TEXT("When raycasting the world in placement mode, allow hits of physics objects that are not tied to a UPrimitiveComponent (to work with non-actor workflows)."));
}

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

/**
 * Prunes list of hit results for actor positioning calculations based on conditions that could be tested
 * on the game thread and returns a list of primitives for the remaining this.
 * @note If a non-primitive based hit is found and ActorPositioningLocals::bAllowNonPrimitiveComponentHits is true then
 * an empty weak obj ptr will be added to the result to represent the hit.
 */
TArray<TWeakObjectPtr<const UPrimitiveComponent>> FilterHitsGameThread(TArray<FHitResult>& InOutHits)
{
	check(IsInGameThread() || IsInParallelGameThread());
	TArray<TWeakObjectPtr<const UPrimitiveComponent>> WeakPrimitives;
	WeakPrimitives.Reserve(InOutHits.Num());

	InOutHits.RemoveAll([&WeakPrimitives](const FHitResult& Hit)
	{
		if (Hit.bStartPenetrating)
		{
			return true;
		}

		const FActorInstanceHandle& HitObjHandle = Hit.HitObjectHandle;

		// Try and find a primitive component for the hit
		const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(HitObjHandle.GetRootComponent());

		if (!PrimitiveComponent)
		{
			PrimitiveComponent = Hit.Component.Get();
		}
		if (PrimitiveComponent && PrimitiveComponent->IsA(ULandscapeHeightfieldCollisionComponent::StaticClass()))
		{
			PrimitiveComponent = CastChecked<ULandscapeHeightfieldCollisionComponent>(PrimitiveComponent)->GetRenderComponent();
		}

		if (!PrimitiveComponent)
		{
			// If we don't have a primitive component, either ignore the hit, or pass it through if the CVar is set appropriately.
			// i.e. ignoring is the inverse of the "allow non primitive hits" CVar.
			if (ActorPositioningLocals::bAllowNonPrimitiveComponentHits)
			{
				// Keep arrays in sync by adding an invalid weak pointer for that hit.
				WeakPrimitives.AddDefaulted();
				return false;
			}
			return true;
		}

		// Ignore volumes and shapes
		if (HitObjHandle.DoesRepresentClass(AVolume::StaticClass()))
		{
			return true;
		}

		if (PrimitiveComponent->IsA(UShapeComponent::StaticClass()))
		{
			return true;
		}

		WeakPrimitives.Add(PrimitiveComponent);
		return false;
	});

	return MoveTemp(WeakPrimitives);
}

/** Check to see if the specified hit result should be ignored from actor positioning calculations for the specified scene view */
bool IsHitIgnoredRenderingThread(const TWeakObjectPtr<const UPrimitiveComponent>& InWeakPrimitiveComponent, const FSceneView& InSceneView)
{
	// We're using the SceneProxy and ViewRelevance here, we should execute from the render thread
	check(IsInParallelRenderingThread());

	const UPrimitiveComponent* PrimitiveComponent = InWeakPrimitiveComponent.Get();
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		const bool bConsiderInvisibleComponentForPlacement = PrimitiveComponent->bConsiderForActorPlacementWhenHidden;

		// Only use this component if it is visible in the specified scene views
		const FPrimitiveViewRelevance ViewRelevance = PrimitiveComponent->SceneProxy->GetViewRelevance(&InSceneView);
		// BSP is a bit special in that its bDrawRelevance is false even when drawn as wireframe because InSceneView.Family->EngineShowFlags.BSPTriangles is off
		const bool bIsRenderedOnScreen = ViewRelevance.bDrawRelevance || (PrimitiveComponent->IsA(UModelComponent::StaticClass()) && InSceneView.Family->EngineShowFlags.BSP);
		const bool bIgnoreTranslucentPrimitive = ViewRelevance.HasTranslucency() && !GetDefault<UEditorPerProjectUserSettings>()->bAllowSelectTranslucent;
		
		return (!bIsRenderedOnScreen && !bConsiderInvisibleComponentForPlacement) || bIgnoreTranslucentPrimitive;
	}

	return false;
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
			// Filter out anything that should be ignored based on information accessible on the game thread
			// and build list of remaining weak primitive components that need to be filtered on the rendering thread 
			TArray<TWeakObjectPtr<const UPrimitiveComponent>> WeakPrimitives = FilterHitsGameThread(Hits);
			ensure(Hits.Num() == WeakPrimitives.Num());

			// Send IsHitIgnoredRenderingThread on the render thread since we're accessing view relevance
			ENQUEUE_RENDER_COMMAND(TraceWorldForPosition_FilterHitsByViewRelevance)(
				[&Hits, &WeakPrimitives, &InSceneView](FRHICommandListImmediate& RHICmdList)
				{
					// Filter out anything that should be ignored
					int32 Index = 0;
					Hits.RemoveAll([&Index, &InSceneView, &WeakPrimitives](const FHitResult&)
					{
						return IsHitIgnoredRenderingThread(WeakPrimitives[Index++], InSceneView);
					});
				}
			);
			
			// We need the result to come back before continuing
			FRenderCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();
		}

		// Go through all hits and find closest
		double ClosestHitDistanceSqr = std::numeric_limits<double>::max();

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
