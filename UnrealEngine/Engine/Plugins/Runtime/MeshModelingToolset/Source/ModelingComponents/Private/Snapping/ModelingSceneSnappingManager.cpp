// Copyright Epic Games, Inc. All Rights Reserved.

#include "Snapping/ModelingSceneSnappingManager.h"

#include "InteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolObjects.h"
#include "ContextObjectStore.h"

#include "Scene/SceneGeometrySpatialCache.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h" // for TActorIterator<>
#include "Engine/StaticMesh.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModelingSceneSnappingManager)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "USceneSnappingManager"



// defined in SceneGeometrySpatialCache.cpp
extern TAutoConsoleVariable<bool> CVarEnableModelingVolumeSnapping;


static double SnapToIncrement(double fValue, double fIncrement, double offset = 0)
{
	if (!FMath::IsFinite(fValue))
	{
		return 0;
	}
	fValue -= offset;
	double sign = FMath::Sign(fValue);
	fValue = FMath::Abs(fValue);
	int64 nInc = (int64)(fValue / fIncrement);
	double fRem = (double)fmod(fValue, fIncrement);
	if (fRem > fIncrement / 2.0)
	{
		++nInc;
	}
	return sign * (double)nInc * fIncrement + offset;
}

//@ todo this are mirrored from GeometryProcessing, which is still experimental...replace w/ direct calls once GP component is standardized
static double OpeningAngleDeg(FVector3d A, FVector3d B, const FVector3d& P)
{
	A -= P;
	A.Normalize();
	B -= P;
	B.Normalize();
	double Dot = FMath::Clamp(FVector3d::DotProduct(A,B), -1.0, 1.0);
	return FMathd::ACos(Dot) * (180.0 / 3.141592653589);
}

static FVector3d NearestSegmentPt(FVector3d A, FVector3d B, const FVector3d& P)
{
	FVector3d Direction = (B - A);
	double Length = Direction.Size();
	Direction /= Length;
	double t = FVector3d::DotProduct( (P - A), Direction);
	if (t >= Length)
	{
		return B;
	}
	if (t <= 0)
	{
		return A;
	}
	return A + t * Direction;
}




void UModelingSceneSnappingManager::Initialize(TObjectPtr<UInteractiveToolsContext> ToolsContext)
{
	ParentContext = ToolsContext;

	QueriesAPI = (ParentContext && ParentContext->ToolManager) ?
		ParentContext->ToolManager->GetContextQueriesAPI() : nullptr;

	SpatialCache = MakeShared<FSceneGeometrySpatialCache>();

#if WITH_EDITOR
	OnObjectModifiedHandler = FCoreUObjectDelegates::OnObjectModified.AddLambda([this](UObject* Object) { 
		HandleGlobalObjectModifiedDelegate(Object);
	});

	if (GEngine)
	{
		OnComponentTransformChangedHandle = GEngine->OnComponentTransformChanged().AddLambda([this](USceneComponent* Component, ETeleportType) {
			HandleGlobalComponentTransformChangedDelegate(Component);
		});
	}
#endif
}

void UModelingSceneSnappingManager::Shutdown()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandler);

	if (GEngine)
	{
		GEngine->OnComponentTransformChanged().Remove(OnComponentTransformChangedHandle);
	}
#endif
}


bool UModelingSceneSnappingManager::IsComponentTypeSupported(const UPrimitiveComponent* Component) const
{
	// supported types
	return ( Cast<UStaticMeshComponent>(Component) != nullptr ||
			 Cast<UBrushComponent>(Component) != nullptr ||
			 Cast<UDynamicMeshComponent>(Component) != nullptr );
}

void UModelingSceneSnappingManager::OnActorAdded(AActor* Actor, TFunctionRef<bool(UPrimitiveComponent*)> PrimitiveFilter)
{
	// TODO: additional filtering here?

	// ignore internal tool framework Actors
	if (Cast<AInternalToolFrameworkActor>(Actor) != nullptr)
	{
		return;
	}

	TArray<AActor*> AllActors;
	Actor->GetAllChildActors(AllActors, true);
	AllActors.Add(Actor);
	for (AActor* ProcessActor : AllActors)
	{
		for (UActorComponent* Component : ProcessActor->GetComponents())
		{
			UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Component);
			if (PrimComponent && PrimitiveFilter(PrimComponent) )
			{
				OnComponentAdded(PrimComponent);
			}
		}
	}
}

void UModelingSceneSnappingManager::OnActorRemoved(AActor* Actor)
{
	// cannot assume actor is still valid...
	TArray<UPrimitiveComponent*> ToRemove;
	for (TPair<UPrimitiveComponent*, AActor*> Pair : ComponentToActorMap)
	{
		if (Pair.Value == Actor)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (UPrimitiveComponent* Component : ToRemove)
	{
		OnComponentRemoved(Component);
	}
}

void UModelingSceneSnappingManager::OnComponentAdded(UPrimitiveComponent* Component)
{
	if (Component->GetOwner() == nullptr)
	{
		return;
	}
	if (IsComponentTypeSupported(Component) == false)
	{
		return;
	}

	UE::Geometry::FSceneGeometryID GeometryID;
	SpatialCache->EnableComponentTracking(Component, GeometryID);

	ComponentToActorMap.Add(Component, Component->GetOwner());

	// UDynamicMeshComponent gets special treatment because it has an explicit mesh-modified handler
	// we can rely on to detect geometry changes
	if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
	{
		DynamicMeshComponent->OnMeshChanged.AddUObject(this, &UModelingSceneSnappingManager::HandleDynamicMeshModifiedDelegate, DynamicMeshComponent);

		DynamicMeshComponents.Add(DynamicMeshComponent, TWeakObjectPtr<UDynamicMeshComponent>(DynamicMeshComponent) );
	}

}

void UModelingSceneSnappingManager::OnComponentRemoved(UPrimitiveComponent* Component)
{
	SpatialCache->DisableComponentTracking(Component);
	ComponentToActorMap.Remove(Component);

	// If this is a DynamicMeshComponent, we want to stop listening to mesh change events, but
	// we need to make sure it is still alive first.
	TWeakObjectPtr<UDynamicMeshComponent>* FoundDynamicMesh = DynamicMeshComponents.Find(Component);
	if (FoundDynamicMesh != nullptr)
	{
		if (FoundDynamicMesh->IsValid())
		{
			UDynamicMeshComponent* DynamicMeshComponent = FoundDynamicMesh->Get();
			DynamicMeshComponent->OnMeshChanged.RemoveAll(this);
		}

		DynamicMeshComponents.Remove(Component);
	}
}


void UModelingSceneSnappingManager::HandleGlobalObjectModifiedDelegate(UObject* Object)
{
	if (Cast<UDynamicMeshComponent>(Object))
	{
		return;		// have special-case handling
	}

	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Object);
	if (PrimComponent)
	{
		OnComponentModified(PrimComponent);
	}
}

void UModelingSceneSnappingManager::OnComponentModified(UActorComponent* Component)
{
	const bool bDeferRebuild = false;
	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Component);
	if (PrimComponent)
	{
		SpatialCache->NotifyGeometryUpdate(PrimComponent, bDeferRebuild);
	}
}


void UModelingSceneSnappingManager::HandleGlobalComponentTransformChangedDelegate(USceneComponent* Component)
{
	UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Component);
	if (PrimComponent && ComponentToActorMap.Contains(PrimComponent))
	{
		SpatialCache->NotifyTransformUpdate(PrimComponent);
	}
}


void UModelingSceneSnappingManager::HandleDynamicMeshModifiedDelegate(UDynamicMeshComponent* Component)
{
	if (bQueueModifiedDynamicMeshUpdates)
	{
		PendingModifiedDynamicMeshes.Add(Component);
	}
	else
	{
		const bool bDeferRebuild = false;
		SpatialCache->NotifyGeometryUpdate(Component, bDeferRebuild);
	}
}





void UModelingSceneSnappingManager::PauseSceneGeometryUpdates()
{
	ensure(bQueueModifiedDynamicMeshUpdates == false);
	bQueueModifiedDynamicMeshUpdates = true;
}


void UModelingSceneSnappingManager::UnPauseSceneGeometryUpdates(bool bImmediateRebuilds)
{
	if (ensure(bQueueModifiedDynamicMeshUpdates))
	{
		for (UDynamicMeshComponent* Component : PendingModifiedDynamicMeshes)
		{
			SpatialCache->NotifyGeometryUpdate(Component, !bImmediateRebuilds);
		}
		PendingModifiedDynamicMeshes.Reset();
		bQueueModifiedDynamicMeshUpdates = false;
	}
}



void UModelingSceneSnappingManager::BuildSpatialCacheForWorld(
	UWorld* World,
	TFunctionRef<bool(AActor*)> ActorFilter,
	TFunctionRef<bool(UPrimitiveComponent*)> PrimitiveFilter)
{
	for ( TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		if (ActorFilter(*ActorItr))
		{
			OnActorAdded(*ActorItr, PrimitiveFilter);
		}
	}
}


bool UModelingSceneSnappingManager::ExecuteSceneSnapQuery(const FSceneSnapQueryRequest& Request, TArray<FSceneSnapQueryResult>& Results) const
{
	switch (Request.RequestType)
	{
	case ESceneSnapQueryType::Position:
		return ExecuteSceneSnapQueryPosition(Request, Results);
	case ESceneSnapQueryType::Rotation:
		return ExecuteSceneSnapQueryRotation(Request, Results);
	default:
		check(!"Only Position and Rotation Snap Queries are supported");
	}
	return false;
}


bool UModelingSceneSnappingManager::ExecuteSceneSnapQueryRotation(const FSceneSnapQueryRequest& Request, TArray<FSceneSnapQueryResult>& Results) const
{
	if (!QueriesAPI)
	{
		return false;
	}

	if ((Request.TargetTypes & ESceneSnapQueryTargetType::Grid) != ESceneSnapQueryTargetType::None)
	{
		FToolContextSnappingConfiguration SnappingConfig = QueriesAPI->GetCurrentSnappingSettings();
		if (SnappingConfig.bEnableRotationGridSnapping)
		{
			FRotator Rotator(Request.DeltaRotation);
			FRotator RotGrid = Request.RotGridSize.Get(SnappingConfig.RotationGridAngles);
			Rotator = Rotator.GridSnap(RotGrid);

			FSceneSnapQueryResult SnapResult;
			SnapResult.TargetType = ESceneSnapQueryTargetType::Grid;
			SnapResult.DeltaRotation = Rotator.Quaternion();
			Results.Add(SnapResult);
			return true;
		}
	}
	return false;
}





static bool FindNearestVisibleObjectHit_Internal(
	UWorld* World, 
	FHitResult& HitResultOut, 
	const FRay3d& Ray,
	bool bIsSceneGeometrySnapQuery, 
	const FSceneQueryVisibilityFilter& VisibilityFilter,
	UE::Geometry::FSceneGeometrySpatialCache* SpatialCache)
{
	FCollisionObjectQueryParams ObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
	FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = bIsSceneGeometrySnapQuery;

	// TODO: QueryParams.AddIgnoredComponents ?

	TArray<FHitResult> OutHits;
	FVector RayEnd = (FVector)(Ray.PointAt(HALF_WORLD_MAX));
	if (World->LineTraceMultiByObjectType(OutHits, (FVector)Ray.Origin, RayEnd, ObjectQueryParams, QueryParams) == false)
	{
		return false;
	}

	bool bEnableVolumes = CVarEnableModelingVolumeSnapping.GetValueOnAnyThread();

	double NearestVisible = TNumericLimits<double>::Max();
	for (const FHitResult& CurResult : OutHits)
	{
		// if we have hit Component in the SpatialCache, prefer to use that
		if (SpatialCache && SpatialCache->HaveCacheForComponent(CurResult.GetComponent()) )
		{
			continue;
		}

		// filtering out any volume hits here will disable volume snapping
		if (bEnableVolumes == false && Cast<UBrushComponent>(CurResult.GetComponent()) != nullptr)
		{
			continue;
		}

		if (CurResult.Distance < NearestVisible && VisibilityFilter.IsVisible(CurResult.Component.Get()) )
		{
			HitResultOut = CurResult;
			NearestVisible = CurResult.Distance;
		}
	}

	return NearestVisible < TNumericLimits<double>::Max();
}


template<typename VectorType>
bool GetComponentHitTriangle_Internal(FHitResult HitResult, VectorType* TriVertices)
{
	if (HitResult.Component.IsValid() == false || HitResult.FaceIndex < 0)
	{
		return false;
	}

	if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(HitResult.Component.Get()))
	{
		// physics collision data is created from StaticMesh RenderData
		// so use HitResult.FaceIndex to extract triangle from the LOD0 mesh
		// (note: this may be incorrect if there are multiple sections...in that case I think we have to
		//  first find section whose accumulated index range would contain .FaceIndexX)
		UStaticMesh* StaticMesh = Component->GetStaticMesh();
		FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
		FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
		int32 TriIdx = 3 * HitResult.FaceIndex;
		FVector Positions[3];
		Positions[0] = (FVector)LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[TriIdx]);
		Positions[1] = (FVector)LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[TriIdx + 1]);
		Positions[2] = (FVector)LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[TriIdx + 2]);

		// transform to world space
		FTransform ComponentTransform = Component->GetComponentTransform();
		Positions[0] = ComponentTransform.TransformPosition(Positions[0]);
		Positions[1] = ComponentTransform.TransformPosition(Positions[1]);
		Positions[2] = ComponentTransform.TransformPosition(Positions[2]);

		TriVertices[0] = (VectorType)Positions[0];
		TriVertices[1] = (VectorType)Positions[1];
		TriVertices[2] = (VectorType)Positions[2];
		return true;
	}

	return false;
}




bool UModelingSceneSnappingManager::ExecuteSceneHitQuery(const FSceneHitQueryRequest& Request, FSceneHitQueryResult& ResultOut) const
{
	if (!QueriesAPI)
	{
		return false;
	}

	FRay3d WorldRay(Request.WorldRay);

	double MinCurrentHitDist = TNumericLimits<double>::Max();

	FViewCameraState ViewState;
	QueriesAPI->GetCurrentViewState(ViewState);

	FSceneGeometryPoint HitPoint;
	FSceneGeometryID HitIdentifier;
	bool bHitSceneCache = SpatialCache->FindNearestHit(WorldRay, HitPoint, HitIdentifier, &Request.VisibilityFilter );
	if (bHitSceneCache)
	{
		FVector3d A, B, C;
		SpatialCache->GetGeometry(HitIdentifier, HitPoint.GeometryType, HitPoint.GeometryIndex, true, A, B, C);
		MinCurrentHitDist = HitPoint.RayDistance;
		ResultOut.TargetActor = HitPoint.Actor;
		ResultOut.TargetComponent = HitPoint.Component;
		ResultOut.HitTriIndex = HitPoint.GeometryIndex;
		ResultOut.Position = HitPoint.WorldPoint;
		ResultOut.Normal = VectorUtil::Normal(A, B, C);
		ResultOut.TriVertices[0] = A;
		ResultOut.TriVertices[1] = B;
		ResultOut.TriVertices[2] = C;

		ResultOut.InitializeHitResult(Request);
	}


	FHitResult HitResult;
	bool bHitWorld = FindNearestVisibleObjectHit_Internal(
		QueriesAPI->GetCurrentEditingWorld(), 
		HitResult, WorldRay, 
		true, Request.VisibilityFilter, SpatialCache.Get());

	if (bHitWorld && HitResult.Distance < MinCurrentHitDist )
	{
		MinCurrentHitDist =  HitResult.Distance;
		ResultOut.TargetActor = HitResult.GetActor();
		ResultOut.TargetComponent = HitResult.GetComponent();
		ResultOut.HitTriIndex = HitResult.FaceIndex;
		ResultOut.Position = (FVector3d)HitResult.Location;
		ResultOut.Normal = (FVector3d)HitResult.Normal;
		ResultOut.HitResult = HitResult;
		if (Request.bWantHitGeometryInfo)
		{ 
			GetComponentHitTriangle_Internal<FVector3d>(HitResult, ResultOut.TriVertices);
		}
	}

	return (MinCurrentHitDist < TNumericLimits<double>::Max());
}



bool UModelingSceneSnappingManager::ExecuteSceneSnapQueryPosition(const FSceneSnapQueryRequest& Request, TArray<FSceneSnapQueryResult>& Results) const
{
	if (!QueriesAPI)
	{
		return false;
	}

	int FoundResultCount = 0;

	if ((Request.TargetTypes & ESceneSnapQueryTargetType::Grid) != ESceneSnapQueryTargetType::None)
	{
		FToolContextSnappingConfiguration SnappingConfig = QueriesAPI->GetCurrentSnappingSettings();

		if (SnappingConfig.bEnablePositionGridSnapping)
		{
			FSceneSnapQueryResult SnapResult;
			SnapResult.TargetType = ESceneSnapQueryTargetType::Grid;

			FVector GridSize = Request.GridSize.Get(SnappingConfig.PositionGridDimensions);

			SnapResult.Position.X = (FVector::FReal)SnapToIncrement(Request.Position.X, GridSize.X);
			SnapResult.Position.Y = (FVector::FReal)SnapToIncrement(Request.Position.Y, GridSize.Y);
			SnapResult.Position.Z = (FVector::FReal)SnapToIncrement(Request.Position.Z, GridSize.Z);

			Results.Add(SnapResult);
			FoundResultCount++;
		}
	}

	FViewCameraState ViewState;
	QueriesAPI->GetCurrentViewState(ViewState);

	//
	// Run a snap query by casting ray into the world.
	// If a hit is found, we look up what triangle was hit, and then test its vertices and edges
	//


	// try snapping to vertices
	auto TrySnapToVertex = [](const FSceneSnapQueryRequest& RequestIn, const FRay3d& WorldRay, FSceneSnapQueryResult& SnapResult, double& SmallestSnapAngle)
	{
		if ((RequestIn.TargetTypes & ESceneSnapQueryTargetType::MeshVertex) != ESceneSnapQueryTargetType::None)
		{
			for (int j = 0; j < 3; ++j)
			{
				double VisualAngle = OpeningAngleDeg((FVector3d)RequestIn.Position, (FVector3d)SnapResult.TriVertices[j], WorldRay.Origin);
				if (VisualAngle < SmallestSnapAngle)
				{
					SmallestSnapAngle = VisualAngle;
					SnapResult.Position = SnapResult.TriVertices[j];
					SnapResult.TargetType = ESceneSnapQueryTargetType::MeshVertex;
					SnapResult.TriSnapIndex = j;
				}
			}
		}
	};


	auto TrySnapToEdge = [](const FSceneSnapQueryRequest& RequestIn, const FRay3d& WorldRay, FSceneSnapQueryResult& SnapResult, double& SmallestSnapAngle)
	{
		if ( ((RequestIn.TargetTypes & ESceneSnapQueryTargetType::MeshEdge) != ESceneSnapQueryTargetType::None) &&
			  (SnapResult.TargetType != ESceneSnapQueryTargetType::MeshVertex) )
		{
			for (int j = 0; j < 3; ++j)
			{
				FVector3d EdgeNearestPt = NearestSegmentPt((FVector3d)SnapResult.TriVertices[j], (FVector3d)SnapResult.TriVertices[(j+1)%3], (FVector3d)RequestIn.Position);
				double VisualAngle = OpeningAngleDeg((FVector3d)RequestIn.Position, EdgeNearestPt, WorldRay.Origin);
				if (VisualAngle < SmallestSnapAngle )
				{
					SmallestSnapAngle = VisualAngle;
					SnapResult.Position = EdgeNearestPt;
					SnapResult.TargetType = ESceneSnapQueryTargetType::MeshEdge;
					SnapResult.TriSnapIndex = j;
				}
			}
		}
	};


	FVector3d RayStart = (FVector3d)ViewState.Position;
	FVector3d RayDirection = (FVector3d)Request.Position - RayStart;
	FRay3d WorldRay(RayStart, Normalized(RayDirection));

	FSceneQueryVisibilityFilter TmpFilter;
	TmpFilter.ComponentsToIgnore = Request.ComponentsToIgnore;
	TmpFilter.InvisibleComponentsToInclude = Request.InvisibleComponentsToInclude;

	// cast ray into scene cache
	FSceneGeometryPoint HitPoint;
	FSceneGeometryID HitIdentifier;
	bool bHitSceneCache = SpatialCache->FindNearestHit(WorldRay, HitPoint, HitIdentifier, &TmpFilter);

	// cast ray into world
	FHitResult HitResult;
	bool bHitWorld = FindNearestVisibleObjectHit_Internal(
		QueriesAPI->GetCurrentEditingWorld(), 
		HitResult, WorldRay, 
		true, TmpFilter, SpatialCache.Get());

	// if we hit both, determine which is closer
	if (bHitSceneCache && bHitWorld)
	{
		if (HitPoint.RayDistance < HitResult.Distance)
		{
			bHitWorld = false;
		}
		else
		{
			bHitSceneCache = false;
		}
	}

	// Try to snap based on whichever thing was hit.
	// Might be possible to combine parts of these blocks.

	if (bHitSceneCache)
	{
		// is this check necessary? isn't this always going to be zero for a scene hit??
		double VisualAngle = OpeningAngleDeg((FVector3d)Request.Position, HitPoint.WorldPoint, RayStart);
		if (VisualAngle < (double)Request.VisualAngleThresholdDegrees)
		{
			FVector3d A, B, C;
			SpatialCache->GetGeometry(HitIdentifier, HitPoint.GeometryType, HitPoint.GeometryIndex, true, A, B, C);

			FSceneSnapQueryResult SnapResult;
			SnapResult.TriVertices[0] = (FVector)A;
			SnapResult.TriVertices[1] = (FVector)B;
			SnapResult.TriVertices[2] = (FVector)C;

			double SmallestAngle = Request.VisualAngleThresholdDegrees;

			// try snapping to vertices
			TrySnapToVertex(Request, WorldRay, SnapResult, SmallestAngle);

			// try snapping to nearest points on edges
			TrySnapToEdge(Request, WorldRay, SnapResult, SmallestAngle);

			// if we found a valid snap, return it
			if (SmallestAngle < (double)Request.VisualAngleThresholdDegrees)
			{
				SnapResult.TargetActor = HitPoint.Actor;
				SnapResult.TargetComponent = HitPoint.Component;
				Results.Add(SnapResult);
				FoundResultCount++;
			}

		}
	}


	if (bHitWorld)
	{
		// is this check necessary? isn't this always going to be zero for a scene hit??
		double VisualAngle = OpeningAngleDeg((FVector3d)Request.Position, (FVector3d)HitResult.ImpactPoint, RayStart);
		if (VisualAngle < (double)Request.VisualAngleThresholdDegrees)
		{
			FSceneSnapQueryResult SnapResult;
			if ( GetComponentHitTriangle_Internal<FVector>(HitResult, SnapResult.TriVertices) )
			{
				double SmallestAngle = Request.VisualAngleThresholdDegrees;

				// try snapping to vertices
				TrySnapToVertex(Request, WorldRay, SnapResult, SmallestAngle);

				// try snapping to nearest points on edges
				TrySnapToEdge(Request, WorldRay, SnapResult, SmallestAngle);

				// if we found a valid snap, return it
				if (SmallestAngle < (double)Request.VisualAngleThresholdDegrees)
				{
					SnapResult.TargetActor = HitResult.HitObjectHandle.FetchActor();
					SnapResult.TargetComponent = HitResult.Component.Get();
					Results.Add(SnapResult);
					FoundResultCount++;
				}
			}
		}
	}

	return (FoundResultCount > 0);
}


bool UE::Geometry::RegisterSceneSnappingManager(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		UModelingSceneSnappingManager* Found = ToolsContext->ContextObjectStore->FindContext<UModelingSceneSnappingManager>();
		if (Found == nullptr)
		{
			UModelingSceneSnappingManager* SelectionManager = NewObject<UModelingSceneSnappingManager>(ToolsContext->ToolManager);
			if (ensure(SelectionManager))
			{
				SelectionManager->Initialize(ToolsContext);
				ToolsContext->ContextObjectStore->AddContextObject(SelectionManager);
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}
	return false;
}



bool UE::Geometry::DeregisterSceneSnappingManager(UInteractiveToolsContext* ToolsContext)
{
	if (ensure(ToolsContext))
	{
		UModelingSceneSnappingManager* Found = ToolsContext->ContextObjectStore->FindContext<UModelingSceneSnappingManager>();
		if (Found != nullptr)
		{
			Found->Shutdown();
			ToolsContext->ContextObjectStore->RemoveContextObject(Found);
		}
		return true;
	}
	return false;
}


UModelingSceneSnappingManager* UE::Geometry::FindModelingSceneSnappingManager(UInteractiveToolManager* ToolManager)
{
	if (ensure(ToolManager))
	{
		UModelingSceneSnappingManager* Found = ToolManager->GetContextObjectStore()->FindContext<UModelingSceneSnappingManager>();
		if (Found != nullptr)
		{
			return Found;
		}
	}
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
