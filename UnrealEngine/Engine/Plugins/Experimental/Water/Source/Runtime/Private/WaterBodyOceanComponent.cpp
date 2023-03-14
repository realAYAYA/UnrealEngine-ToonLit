// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyOceanComponent.h"
#include "WaterModule.h"
#include "WaterSplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#include "OceanCollisionComponent.h"
#include "WaterBodyOceanActor.h"
#include "WaterBooleanUtils.h"
#include "WaterSubsystem.h"
#include "WaterZoneActor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "Polygon2.h"
#include "ConstrainedDelaunay2.h"
#include "Operations/InsetMeshRegion.h"
#include "DynamicMesh/DynamicMeshChangeTracker.h"
#include "Algo/Transform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyOceanComponent)

#if WITH_EDITOR
#include "WaterIconHelper.h"
#endif

// ----------------------------------------------------------------------------------

UWaterBodyOceanComponent::UWaterBodyOceanComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CollisionExtents = FVector(50000.f, 50000.f, 10000.f);

	// @todo_water : Remove these checks (Once AWaterBody is no more Blueprintable, these methods should become PURE_VIRTUAL and this class should overload them)
	check(IsFlatSurface());
	check(IsWaterSplineClosedLoop());
	check(IsHeightOffsetSupported());
}

TArray<UPrimitiveComponent*> UWaterBodyOceanComponent::GetCollisionComponents(bool bInOnlyEnabledComponents) const
{
	TArray<UPrimitiveComponent*> Result;
	Result.Reserve(CollisionBoxes.Num() + CollisionHullSets.Num());

	Algo::TransformIf(CollisionBoxes, Result,
		[bInOnlyEnabledComponents](UOceanBoxCollisionComponent* Comp) { return ((Comp != nullptr) && (!bInOnlyEnabledComponents || (Comp->GetCollisionEnabled() != ECollisionEnabled::NoCollision))); },
		[](UOceanBoxCollisionComponent* Comp) { return Comp; });

	Algo::TransformIf(CollisionHullSets, Result,
		[bInOnlyEnabledComponents](UOceanCollisionComponent* Comp) { return ((Comp != nullptr) && (!bInOnlyEnabledComponents || (Comp->GetCollisionEnabled() != ECollisionEnabled::NoCollision))); },
		[](UOceanCollisionComponent* Comp) { return Comp; });

	return Result;
}

void UWaterBodyOceanComponent::SetHeightOffset(float InHeightOffset)
{
	const float ClampedHeightOffset = FMath::Max(0.0f, InHeightOffset);

	if (HeightOffset != ClampedHeightOffset)
	{
		HeightOffset = ClampedHeightOffset;

		// the physics volume needs to be adjusted : 
		FOnWaterBodyChangedParams Params;
		Params.bShapeOrPositionChanged = true;
		OnWaterBodyChanged(Params);
	}
}

void UWaterBodyOceanComponent::BeginUpdateWaterBody()
{
	Super::BeginUpdateWaterBody();

	// Update WaterSubsystem's OceanActor
	if (UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld()))
	{
		WaterSubsystem->SetOceanBodyComponent(this);
	}
}

#if WITH_EDITOR
void UWaterBodyOceanComponent::OnPostEditChangeProperty(FOnWaterBodyChangedParams& InOutOnWaterBodyChangedParams)
{
	Super::OnPostEditChangeProperty(InOutOnWaterBodyChangedParams);

	const FPropertyChangedEvent& PropertyChangedEvent = InOutOnWaterBodyChangedParams.PropertyChangedEvent;
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UWaterBodyOceanComponent, CollisionExtents))
	{
		// Affects the physics shape
		InOutOnWaterBodyChangedParams.bShapeOrPositionChanged = true;
	}
}
const TCHAR* UWaterBodyOceanComponent::GetWaterSpriteTextureName() const
{
	return TEXT("/Water/Icons/WaterBodyOceanSprite");
}
#endif

void UWaterBodyOceanComponent::Reset()
{
	for (UBoxComponent* Component : CollisionBoxes)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	CollisionBoxes.Reset();
	for (UOceanCollisionComponent* Component : CollisionHullSets)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	CollisionHullSets.Reset();
}

// Adds a quad to a dynamic mesh reusing passed vertex indices if they are not INDEX_NONE
static void AddAABBQuadToDynamicMesh(UE::Geometry::FDynamicMesh3& EditMesh, FVector3d Min, FVector3d Max, TArray<int32, TInlineAllocator<4>>& InOutVertices)
{
	using namespace UE::Geometry;

	int32 VertexAIndex = InOutVertices[0];
	int32 VertexBIndex = InOutVertices[1];
	int32 VertexCIndex = InOutVertices[2];
	int32 VertexDIndex = InOutVertices[3];

	if (VertexAIndex == INDEX_NONE)
	{ 
		FVertexInfo VertexA;
		VertexA.Position = Min;
		VertexA.Color = FVector3f(0.0);
		VertexA.bHaveC = true;
		VertexAIndex = EditMesh.AppendVertex(VertexA);
	}
	
	if (VertexBIndex == INDEX_NONE)
	{
		FVertexInfo VertexB;
		VertexB.Position = FVector3d(Max.X, Min.Y, 0);
		VertexB.Color = FVector3f(0.0);
		VertexB.bHaveC = true;
		VertexBIndex = EditMesh.AppendVertex(VertexB);
	}

	if (VertexCIndex == INDEX_NONE)
	{
		FVertexInfo VertexC;
		VertexC.Position = Max;
		VertexC.Color = FVector3f(0.0);
		VertexC.bHaveC = true;
		VertexCIndex = EditMesh.AppendVertex(VertexC);
	}

	if (VertexDIndex == INDEX_NONE)
	{
		FVertexInfo VertexD;
		VertexD.Position = FVector3d(Min.X, Max.Y, 0);
		VertexD.Color = FVector3f(0.0);
		VertexD.bHaveC = true;
		VertexDIndex = EditMesh.AppendVertex(VertexD);
	}

	const int TriOne = EditMesh.AppendTriangle(VertexAIndex, VertexDIndex, VertexBIndex);
	const int TriTwo = EditMesh.AppendTriangle(VertexBIndex, VertexDIndex, VertexCIndex);
	check(TriOne != INDEX_NONE && TriTwo != INDEX_NONE);

	InOutVertices = {VertexAIndex, VertexBIndex, VertexCIndex, VertexDIndex};
}

bool UWaterBodyOceanComponent::GenerateWaterBodyMesh(UE::Geometry::FDynamicMesh3& OutMesh, UE::Geometry::FDynamicMesh3* OutDilatedMesh) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GenerateOceanMesh);

	using namespace UE::Geometry;

	const UWaterSplineComponent* SplineComp = GetWaterSpline();
	const FVector OceanLocation = GetComponentLocation();
	const AWaterZone* WaterZone = GetWaterZone();
	
	if (SplineComp->GetNumberOfSplineSegments() < 3)
	{
		return false;
	}

	if (!WaterZone)
	{
		return false;
	}

	FPolygon2d Island;
	TArray<FVector> PolyLineVertices;
	SplineComp->ConvertSplineToPolyLine(ESplineCoordinateSpace::Local, FMath::Square(10.f), PolyLineVertices);

	FAxisAlignedBox2d IslandBounds = FAxisAlignedBox2d(FVector2d(0.f), FVector2d(0.f));
	// Construct a 2D polygon describing the central island
	for (int32 i = PolyLineVertices.Num() - 2; i >= 0; --i) // skip the last vertex since it's the same as the first vertex
	{
		Island.AppendVertex(FVector2D(PolyLineVertices[i]));
		IslandBounds.Contain(FVector2d(PolyLineVertices[i]));
	}
	// Expand the island slightly so we aren't intersecting with the spline
	IslandBounds.Expand(1);

	// #todo: account for scale
	const FVector2d OceanExtents = WaterZone->GetZoneExtent(); // file the entire water zone with an ocean
	const FAxisAlignedBox2d OceanBounds = FAxisAlignedBox2d(FVector2d(GetComponentTransform().InverseTransformPositionNoScale(WaterZone->GetActorLocation())), OceanExtents.X / 2.0);
	FPolygon2d IslandBoundingPolygon = FPolygon2d::MakeRectangle(IslandBounds.Center(), IslandBounds.Extents().X * 2., IslandBounds.Extents().Y * 2.);

	FConstrainedDelaunay2d Triangulation;
	Triangulation.FillRule = FConstrainedDelaunay2d::EFillRule::Positive;
	Triangulation.Add(IslandBoundingPolygon);

	if (!Island.IsClockwise())
	{
		Island.Reverse();
	}
	Triangulation.Add(Island);
	if (!Triangulation.Triangulate())
	{
		UE_LOG(LogWater, Error, TEXT("Failed to triangulate Ocean mesh for %s. Ensure that the Ocean's spline does not form any loops."), *GetOwner()->GetActorNameOrLabel());
	}

	if (Triangulation.Triangles.Num() == 0)
	{
		return false;
	}
	
	int32 IslandBottomLeft = INDEX_NONE;
	FVector3d IslandBottomLeftVertex;

	int32 IslandBottomRight = INDEX_NONE;
	FVector3d IslandBottomRightVertex;
	
	int32 IslandTopRight = INDEX_NONE;
	FVector3d IslandTopRightVertex;

	int32 IslandTopLeft = INDEX_NONE;
	FVector3d IslandTopLeftVertex;

	for (const FVector2d& Vertex : Triangulation.Vertices)
	{
		FVertexInfo VertexInfo;
		VertexInfo.Position = FVector3d(Vertex, 0.f);
		VertexInfo.Color = FVector3f(0.0);
		VertexInfo.bHaveC = true;

		int32 Index = OutMesh.AppendVertex(VertexInfo);

		// Collect the corner vertices of the island bounding box so we can stitch the outer quads to them.
		if ((IslandBottomLeft == INDEX_NONE) || (VertexInfo.Position.X < IslandBottomLeftVertex.X || VertexInfo.Position.Y < IslandBottomLeftVertex.Y))
		{
			IslandBottomLeft = Index;
			IslandBottomLeftVertex = VertexInfo.Position;
		}
		if ((IslandBottomRight == INDEX_NONE) || (VertexInfo.Position.X > IslandBottomRightVertex.X || VertexInfo.Position.Y < IslandBottomRightVertex.Y))
		{
			IslandBottomRight = Index;
			IslandBottomRightVertex = VertexInfo.Position;
		}
		if ((IslandTopRight == INDEX_NONE) || (VertexInfo.Position.X > IslandTopRightVertex.X || VertexInfo.Position.Y > IslandTopRightVertex.Y))
		{
			IslandTopRight = Index;
			IslandTopRightVertex = VertexInfo.Position;
		}
		if ((IslandTopLeft == INDEX_NONE) || (VertexInfo.Position.X < IslandTopLeftVertex.X || VertexInfo.Position.Y > IslandTopLeftVertex.Y))
		{
			IslandTopLeft = Index;
			IslandTopLeftVertex = VertexInfo.Position;
		}
	}
	check(IslandBottomLeft != INDEX_NONE &&
		  IslandBottomRight != INDEX_NONE &&
		  IslandTopRight != INDEX_NONE &&
		  IslandTopLeft != INDEX_NONE);

	for (const FIndex3i& Triangle : Triangulation.Triangles)
	{
		OutMesh.AppendTriangle(Triangle);
	}

	// Add the bounding quads:

	// Each quad has four vertices laid out like so
	// D --- C
	// |  \  |
	// A --- B
	
	TArray<int32, TInlineAllocator<4>> Vertices({ INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE });

	// Bottom left quad
	Vertices[2] = IslandBottomLeft;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(OceanBounds.Min, 0), FVector3d(IslandBounds.Min, 0), Vertices);
	// We need the B and C vertices of the first quad to attach to the last quad
	int32 FirstQuadB = Vertices[1];
	int32 FirstQuadC = Vertices[2];

	// Left middle quad
	Vertices[0] = Vertices[3];
	Vertices[1] = Vertices[2];
	Vertices[2] = IslandTopLeft;
	Vertices[3] = INDEX_NONE;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(OceanBounds.Min.X, IslandBounds.Min.Y, 0), FVector3d(IslandBounds.Min.X, IslandBounds.Max.Y, 0), Vertices);

	// Top left quad
	Vertices[0] = Vertices[3];
	Vertices[1] = Vertices[2];
	Vertices[2] = INDEX_NONE;
	Vertices[3] = INDEX_NONE;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(OceanBounds.Min.X, IslandBounds.Max.Y, 0), FVector3d(IslandBounds.Min.X, OceanBounds.Max.Y, 0), Vertices);

	// Top middle quad
	Vertices[0] = Vertices[1];
	Vertices[3] = Vertices[2];
	Vertices[1] = IslandTopRight;
	Vertices[2] = INDEX_NONE;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(IslandBounds.Min.X, IslandBounds.Max.Y, 0), FVector3d(IslandBounds.Max.X, OceanBounds.Max.Y, 0), Vertices);

	// Top right quad
	Vertices[0] = Vertices[1];
	Vertices[3] = Vertices[2];
	Vertices[1] = INDEX_NONE;
	Vertices[2] = INDEX_NONE;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(IslandBounds.Max, 0), FVector3d(OceanBounds.Max, 0), Vertices);

	// Middle right quad
	Vertices[3] = Vertices[0];
	Vertices[2] = Vertices[1];
	Vertices[1] = INDEX_NONE;
	Vertices[0] = IslandBottomRight;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(IslandBounds.Max.X, IslandBounds.Min.Y, 0), FVector3d(OceanBounds.Max.X, IslandBounds.Max.Y, 0), Vertices);

	// Bottom right quad
	Vertices[3] = Vertices[0];
	Vertices[2] = Vertices[1];
	Vertices[1] = INDEX_NONE;
	Vertices[0] = INDEX_NONE;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(IslandBounds.Max.X, OceanBounds.Min.Y, 0), FVector3d(OceanBounds.Max.X, IslandBounds.Min.Y, 0), Vertices);

	// Middle bottom quad
	Vertices[1] = Vertices[0];
	Vertices[2] = Vertices[3];
	Vertices[0] = FirstQuadB;
	Vertices[3] = FirstQuadC;
	AddAABBQuadToDynamicMesh(OutMesh, FVector3d(IslandBounds.Min.X, OceanBounds.Min.Y, 0), FVector3d(IslandBounds.Max.X, IslandBounds.Min.Y, 0), Vertices);
	
	if (ShapeDilation > 0.f && OutDilatedMesh)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(DilateOceanMesh);

		// Inset the mesh by -ShapeDilation to effectively expand the mesh
		OutDilatedMesh->Copy(OutMesh);
		FInsetMeshRegion Inset(OutDilatedMesh);
		Inset.InsetDistance = -1 * ShapeDilation / 2.f;

		Inset.Triangles.Reserve(OutDilatedMesh->TriangleCount());
		for (int32 Idx : OutDilatedMesh->TriangleIndicesItr())
		{
			Inset.Triangles.Add(Idx);
		}
		
		if (!Inset.Apply())
		{
			UE_LOG(LogWater, Warning, TEXT("Failed to apply mesh inset for shape dilation (%s)"), *GetOwner()->GetActorNameOrLabel());
			return false;
		}
	}	
	return true;
}

void UWaterBodyOceanComponent::PostLoad()
{
	Super::PostLoad();

#ifdef WITH_EDITORONLY_DATA
#endif // WITH_EDITORONLY_DATA
}

FBoxSphereBounds UWaterBodyOceanComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (AWaterZone* WaterZone = GetWaterZone())
	{
		const FVector2D OceanExtents = WaterZone->GetZoneExtent();
		const FVector Min(FVector(-OceanExtents / 2.0, -1.0 * GetChannelDepth()));
		const FVector Max(FVector(OceanExtents / 2.0, 0.0));
		return FBoxSphereBounds(FBox(Min, Max)).TransformBy(LocalToWorld);
	}
	return FBoxSphereBounds().TransformBy(LocalToWorld);
}

void UWaterBodyOceanComponent::OnUpdateBody(bool bWithExclusionVolumes)
{
	AActor* OwnerActor = GetOwner();
	check(OwnerActor);

	if (GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		FVector OceanCollisionExtents = GetCollisionExtents();
		OceanCollisionExtents.Z += CollisionHeightOffset / 2;

		// The volume's top is located on the ocean actor's height + the additional ocean level + the collision height offset: 
		// and the volume's bottom is deeper by a value == OceanCollisionExtents.Z :
		FVector OceanBoxLocation = FVector(0, 0, GetHeightOffset() - OceanCollisionExtents.Z + CollisionHeightOffset);
		// No matter the scale, OceanCollisionExtents is always specified in world-space : 
		FVector OceanBoxExtent = OceanCollisionExtents; 

		// get our box information and exclusion volumes
		FTransform ComponentTransform = GetComponentTransform();
		FBoxSphereBounds WorldBounds;
		WorldBounds.Origin = ComponentTransform.TransformPositionNoScale(OceanBoxLocation);
		WorldBounds.BoxExtent = OceanBoxExtent;
		TArray<AWaterBodyExclusionVolume*> Exclusions = bWithExclusionVolumes ? GetExclusionVolumes() : TArray<AWaterBodyExclusionVolume*>();

		// Calculate a set of boxes and meshes that are Difference(Box, Union(ExclusionVolumes))
		// Output is calculated in World space and then transformed into Actor space, ie by inverse of ActorTransform
		TArray<FBoxSphereBounds> Boxes;
		TArray<TArray<FKConvexElem>> ConvexSets;
		double WorldMeshBufferWidth = 1000.0;		// extra space left around exclusion meshes
		double WorldBoxOverlap = 10.0;				// output boxes overlap each other and meshes by this amount
		FWaterBooleanUtils::BuildOceanCollisionComponents(WorldBounds, ComponentTransform, Exclusions,
			Boxes, ConvexSets, WorldMeshBufferWidth, WorldBoxOverlap);

		// Don't delete components unless we have to : this generates determinism issues because UOceanCollisionComponent has a UBodySetup with a GUID :
		if ((CollisionBoxes.Num() != Boxes.Num()) || (CollisionHullSets.Num() != ConvexSets.Num()))
		{
			Reset();
		}

		// create the box components
		for (int32 i = 0; i < Boxes.Num(); ++i)
		{
			const FBoxSphereBounds& Box = Boxes[i];
			// We want a deterministic name within this water body component's outer to avoid non-deterministic cook issues but we also want to avoid reusing a component that might have been deleted
			//  prior to that (in order to avoid potentially stalls caused by the primitive component not having been FinishDestroy-ed) (because OnUpdateBody runs 2 times in a row, 
			//  once with bWithExclusionVolumes == false, once with bWithExclusionVolumes == true) so we use MakeUniqueObjectName for the name here :
			FName Name = MakeUniqueObjectName(OwnerActor, UOceanCollisionComponent::StaticClass(), *FString::Printf(TEXT("OceanCollisionBoxComponent_%d"), i));
			UOceanBoxCollisionComponent* BoxComponent = nullptr;
			if (CollisionBoxes.IsValidIndex(i) && (CollisionBoxes[i] != nullptr))
			{
				BoxComponent = CollisionBoxes[i];
			}
			else
			{
				BoxComponent = NewObject<UOceanBoxCollisionComponent>(OwnerActor, Name, RF_Transactional);
				BoxComponent->SetupAttachment(this);
				CollisionBoxes.Add(BoxComponent);
			}

			if (!BoxComponent->IsRegistered())
			{
				BoxComponent->RegisterComponent();
			}
			BoxComponent->SetNetAddressable(); // it's deterministically named so it's addressable over network (needed for collision)
			BoxComponent->bDrawOnlyIfSelected = true;
			BoxComponent->SetRelativeLocation(FVector::ZeroVector);

			CopySharedCollisionSettingsToComponent(BoxComponent);
			CopySharedNavigationSettingsToComponent(BoxComponent);

			FVector RelativePosition = Box.Origin;			// boxes are calculated in space of actor
			BoxComponent->SetRelativeLocation(RelativePosition);
			BoxComponent->SetBoxExtent(Box.BoxExtent);

		}

		// create the convex-hull components
		for (int32 i = 0; i < ConvexSets.Num(); ++i)
		{
			const TArray<FKConvexElem>& ConvexSet = ConvexSets[i];
			// We want a deterministic name within this water body component's outer to avoid non-deterministic cook issues but we also want to avoid reusing a component that might have been deleted
			//  prior to that (in order to avoid potentially stalls caused by the primitive component not having been FinishDestroy-ed) (because OnUpdateBody runs 2 times in a row, 
			//  once with bWithExclusionVolumes == false, once with bWithExclusionVolumes == true) so we use MakeUniqueObjectName for the name here :
			FName Name = MakeUniqueObjectName(OwnerActor, UOceanCollisionComponent::StaticClass(), *FString::Printf(TEXT("OceanCollisionComponent_%d"), i));
			UOceanCollisionComponent* CollisionComponent = nullptr;
			if (CollisionHullSets.IsValidIndex(i) && (CollisionHullSets[i] != nullptr))
			{
				CollisionComponent = CollisionHullSets[i];
			}
			else
			{
				CollisionComponent = NewObject<UOceanCollisionComponent>(OwnerActor, Name, RF_Transactional);
				CollisionComponent->SetupAttachment(this);
				CollisionHullSets.Add(CollisionComponent);
			}

			if (!CollisionComponent->IsRegistered())
			{
				CollisionComponent->RegisterComponent();
			}
			CollisionComponent->SetNetAddressable(); // it's deterministically named so it's addressable over network (needed for collision)
			CollisionComponent->SetRelativeLocation(FVector::ZeroVector);

			CopySharedCollisionSettingsToComponent(CollisionComponent);
			CopySharedNavigationSettingsToComponent(CollisionComponent);

			CollisionComponent->InitializeFromConvexElements(ConvexSet);
		}
	}
	else
	{
		// clear existing
		Reset();
	}
}
