// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Physics/ComponentCollisionUtil.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "ShapeApproximation/SimpleShapeSet3.h"
#include "DynamicMeshEditor.h"
#include "DynamicMesh/MeshNormals.h"
#include "Generators/SphereGenerator.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Generators/CapsuleGenerator.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Parameterization/DynamicMeshUVEditor.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"

#include "Physics/PhysicsDataCollection.h"

#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"

using namespace UE::Geometry;

bool UE::Geometry::ComponentTypeSupportsCollision(
	const UPrimitiveComponent* Component)
{
	// currently only supporting StaticMeshComponent
	const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	return (StaticMeshComponent != nullptr);
}


FComponentCollisionSettings UE::Geometry::GetCollisionSettings(const UPrimitiveComponent* Component)
{
	FComponentCollisionSettings Settings;

	const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	if (ensure(StaticMeshComponent))
	{
		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (ensure(StaticMesh))
		{
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
			if (ensure(BodySetup))
			{
				Settings.CollisionTypeFlag = (int32)BodySetup->CollisionTraceFlag;
			}
		}
	}
	return Settings;
}


void UE::Geometry::UpdateSimpleCollision(
	UBodySetup* BodySetup, 
	const FKAggregateGeom* NewGeometry,
	UStaticMesh* StaticMesh,
	FComponentCollisionSettings CollisionSettings)
{
	BodySetup->Modify();
	BodySetup->RemoveSimpleCollision();

	// set new collision geometry
	BodySetup->AggGeom = *NewGeometry;

	// update collision type
	BodySetup->CollisionTraceFlag = (ECollisionTraceFlag)CollisionSettings.CollisionTypeFlag;

	// rebuild physics meshes
	BodySetup->CreatePhysicsMeshes();

	// rebuild nav collision (? StaticMeshEditor does this)
	StaticMesh->CreateNavCollision(/*bIsUpdate=*/true);

	// update physics state on all components using this StaticMesh
	for (FThreadSafeObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
	{
		UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(*Iter);
		if (SMComponent->GetStaticMesh() == StaticMesh)
		{
			if (SMComponent->IsPhysicsStateCreated())
			{
				SMComponent->RecreatePhysicsState();
			}
		}
	}

	// mark static mesh as dirty so it gets resaved?
	[[maybe_unused]] bool MarkedDirty = StaticMesh->MarkPackageDirty();

#if WITH_EDITORONLY_DATA
	// mark the static mesh as having customized collision so it is not regenerated on reimport
	StaticMesh->bCustomizedCollision = CollisionSettings.bIsGeneratedCollision;
#endif // WITH_EDITORONLY_DATA
}


const UBodySetup* UE::Geometry::GetBodySetup(const UPrimitiveComponent* Component)
{
	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			return StaticMesh->GetBodySetup();
		}
	}
	else if (const UBrushComponent* BrushComponent = Cast<UBrushComponent>(Component))
	{
		return BrushComponent->BrushBodySetup;
	}
	else if (const UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
	{
		return DynamicMeshComponent->GetBodySetup();
	}

	return nullptr;
}

UBodySetup* UE::Geometry::GetBodySetup(UPrimitiveComponent* Component)
{
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			return StaticMesh->GetBodySetup();
		}
	}
	else if (UBrushComponent* BrushComponent = Cast<UBrushComponent>(Component))
	{
		return BrushComponent->BrushBodySetup;
	}
	else if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
	{
		return DynamicMeshComponent->GetBodySetup();
	}

	return nullptr;
}



bool UE::Geometry::SetSimpleCollision(
	UPrimitiveComponent* Component,
	const FSimpleShapeSet3d* ShapeSet,
	FComponentCollisionSettings CollisionSettings)
{
	FPhysicsDataCollection PhysicsData;
	PhysicsData.InitializeFromComponent(Component, false);
	if (ensure(PhysicsData.SourceComponent.IsValid()) == false || ensure(ShapeSet != nullptr) == false )
	{
		return false;
	}

	PhysicsData.Geometry = *ShapeSet;
	PhysicsData.CopyGeometryToAggregate();

	// FPhysicsDataCollection stores its references as const, but the input Component was non-const so this is ok to do
	UStaticMesh* StaticMesh = const_cast<UStaticMesh*>(PhysicsData.SourceStaticMesh.Get());
	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	UpdateSimpleCollision(BodySetup, &PhysicsData.AggGeom, StaticMesh, CollisionSettings);

	return true;
}



bool UE::Geometry::TransformSimpleCollision(
	UPrimitiveComponent* Component,
	const FTransform3d& Transform)
{
	FPhysicsDataCollection PhysicsData;
	PhysicsData.InitializeFromComponent(Component, true);
	if ( ensure(PhysicsData.SourceComponent.IsValid()) == false)
	{
		return false;
	}

	FComponentCollisionSettings Settings = GetCollisionSettings(Component);
	Settings.bIsGeneratedCollision = false;

	PhysicsData.Geometry.ApplyTransform(Transform);
	PhysicsData.ClearAggregate();
	PhysicsData.CopyGeometryToAggregate();

	// FPhysicsDataCollection stores its references as const, but the input Component was non-const so this is ok to do
	UStaticMesh* StaticMesh = const_cast<UStaticMesh*>(PhysicsData.SourceStaticMesh.Get());
	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	UpdateSimpleCollision(BodySetup, &PhysicsData.AggGeom, StaticMesh, Settings);

	return true;
}




bool UE::Geometry::AppendSimpleCollision(
	const UPrimitiveComponent* Component,
	FSimpleShapeSet3d* ShapeSetOut,
	const FTransform3d& Transform)
{
	FPhysicsDataCollection PhysicsData;
	PhysicsData.InitializeFromComponent(Component, true);
	if (ensure(PhysicsData.SourceComponent.IsValid()) == false || ensure(ShapeSetOut != nullptr) == false)
	{
		return false;
	}

	ShapeSetOut->Append(PhysicsData.Geometry, Transform);
	return true;
}


bool UE::Geometry::AppendSimpleCollision(
	const UPrimitiveComponent* Component,
	FSimpleShapeSet3d* ShapeSetOut,
	const TArray<FTransform3d>& TransformSeqeuence)
{
	FPhysicsDataCollection PhysicsData;
	PhysicsData.InitializeFromComponent(Component, true);
	if (ensure(PhysicsData.SourceComponent.IsValid()) == false || ensure(ShapeSetOut != nullptr) == false)
	{
		return false;
	}

	ShapeSetOut->Append(PhysicsData.Geometry, TransformSeqeuence);

	return true;
}


void UE::Geometry::ConvertSimpleCollisionToMeshes(
	const FKAggregateGeom& AggGeom,
	FDynamicMesh3& MeshOut,
	const FTransformSequence3d& TransformSeqeuence,
	int32 SphereResolution,
	bool bSetToPerTriangleNormals,
	bool bInitializeConvexUVs,
	TFunction<void(int, const FDynamicMesh3&)> PerElementMeshCallback )
{
	FDynamicMeshEditor Editor(&MeshOut);

	bool bTransformInverts = TransformSeqeuence.WillInvert();

	for (const FKSphereElem& Sphere : AggGeom.SphereElems)
	{
		FSphereGenerator SphereGen;
		SphereGen.Radius = Sphere.Radius;
		SphereGen.NumPhi = SphereGen.NumTheta = SphereResolution;
		SphereGen.bPolygroupPerQuad = false;
		SphereGen.Generate();
		FDynamicMesh3 SphereMesh(&SphereGen);

		MeshTransforms::Translate(SphereMesh, FVector3d(Sphere.Center));

		if (bTransformInverts)
		{
			SphereMesh.ReverseOrientation(false);
		}

		FMeshIndexMappings Mappings;
		Editor.AppendMesh(&SphereMesh, Mappings,
			[&](int32 vid, const FVector3d& P) { return TransformSeqeuence.TransformPosition(P); },
			[&](int32 vid, const FVector3d& N) { return TransformSeqeuence.TransformNormal(N); });

		if (PerElementMeshCallback)
		{
			PerElementMeshCallback(0, SphereMesh);
		}
	}

	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		FMinimalBoxMeshGenerator BoxGen;
		BoxGen.Box = UE::Geometry::FOrientedBox3d(
			FFrame3d(FVector3d(Box.Center), FQuaterniond(Box.Rotation.Quaternion())),
			0.5*FVector3d(Box.X, Box.Y, Box.Z));
		BoxGen.Generate();
		FDynamicMesh3 BoxMesh(&BoxGen);

		// transform not applied because it is just the Center/Rotation

		if (bTransformInverts)
		{
			BoxMesh.ReverseOrientation(false);
		}

		FMeshIndexMappings Mappings;
		Editor.AppendMesh(&BoxMesh, Mappings,
			[&](int32 vid, const FVector3d& P) { return TransformSeqeuence.TransformPosition(P); },
			[&](int32 vid, const FVector3d& N) { return TransformSeqeuence.TransformNormal(N); });	

		if (PerElementMeshCallback)
		{
			PerElementMeshCallback(1, BoxMesh);
		}
	}


	for (const FKSphylElem& Capsule: AggGeom.SphylElems)
	{
		FCapsuleGenerator CapsuleGen;
		CapsuleGen.Radius = Capsule.Radius;
		CapsuleGen.SegmentLength = Capsule.Length;
		CapsuleGen.NumHemisphereArcSteps = SphereResolution/4+1;
		CapsuleGen.NumCircleSteps = SphereResolution;
		CapsuleGen.bPolygroupPerQuad = false;
		CapsuleGen.Generate();
		FDynamicMesh3 CapsuleMesh(&CapsuleGen);

		MeshTransforms::Translate(CapsuleMesh, FVector3d(0,0,-0.5*Capsule.Length) );

		FTransformSRT3d Transform(Capsule.GetTransform());
		MeshTransforms::ApplyTransform(CapsuleMesh, Transform, false);
		if (Transform.GetDeterminant() < 0 != bTransformInverts)
		{
			CapsuleMesh.ReverseOrientation(false);
		}

		FMeshIndexMappings Mappings;
		Editor.AppendMesh(&CapsuleMesh, Mappings,
			[&](int32 vid, const FVector3d& P) { return TransformSeqeuence.TransformPosition(P); },
			[&](int32 vid, const FVector3d& N) { return TransformSeqeuence.TransformNormal(N); });	

		if (PerElementMeshCallback)
		{
			PerElementMeshCallback(2, CapsuleMesh);
		}
	}


	for (const FKConvexElem& Convex : AggGeom.ConvexElems)
	{
		FTransformSRT3d ElemTransform(Convex.GetTransform());
		FDynamicMesh3 ConvexMesh(EMeshComponents::None);
		int32 NumVertices = Convex.VertexData.Num();
		for (int32 k = 0; k < NumVertices; ++k)
		{
			ConvexMesh.AppendVertex(ElemTransform.TransformPosition(FVector3d(Convex.VertexData[k])) );
		}
		int32 NumTriangles = Convex.IndexData.Num() / 3;
		for (int32 k = 0; k < NumTriangles; ++k)
		{
			ConvexMesh.AppendTriangle(Convex.IndexData[3*k], Convex.IndexData[3*k+1], Convex.IndexData[3*k+2]);
		}
		
		// Note we need to reverse the orientation if no transform inverts, or if both invert,
		// because ConvexMesh has reversed-orientation triangles normally
		if (ElemTransform.GetDeterminant() < 0 == bTransformInverts)
		{
			ConvexMesh.ReverseOrientation();
		}
		ConvexMesh.EnableTriangleGroups(0);
		ConvexMesh.EnableAttributes();
		if (bInitializeConvexUVs)
		{
			FDynamicMeshUVEditor UVEditor(&ConvexMesh, 0, true);
			UVEditor.SetPerTriangleUVs();
		}

		FMeshIndexMappings Mappings;
		Editor.AppendMesh(&ConvexMesh, Mappings,
			[&](int32 vid, const FVector3d& P) { return TransformSeqeuence.TransformPosition(P); },
			[&](int32 vid, const FVector3d& N) { return TransformSeqeuence.TransformNormal(N); });	

		if (PerElementMeshCallback)
		{
			PerElementMeshCallback(3, ConvexMesh);
		}
	}

	if (MeshOut.HasAttributes() && MeshOut.Attributes()->PrimaryNormals() != nullptr)
	{
		if (bSetToPerTriangleNormals)
		{
			FMeshNormals::InitializeMeshToPerTriangleNormals(&MeshOut);
		}
		else
		{
			FMeshNormals::InitializeOverlayToPerVertexNormals(MeshOut.Attributes()->PrimaryNormals(), false);
		}
	}
}



bool UE::Geometry::ConvertComplexCollisionToMeshes(
	IInterface_CollisionDataProvider* CollisionProvider,
	UE::Geometry::FDynamicMesh3& MeshOut,
	const FTransformSequence3d& TransformSeqeuence,
	bool& bFoundMeshErrorsOut,
	bool bWeldEdges,
	bool bSetToPerTriangleNormals)
{
	bFoundMeshErrorsOut = false;
	if (CollisionProvider && CollisionProvider->ContainsPhysicsTriMeshData(true))
	{
		FTriMeshCollisionData CollisionData;
		if (CollisionProvider->GetPhysicsTriMeshData(&CollisionData, true))
		{
			bool bWillInvert = TransformSeqeuence.WillInvert();
			TArray<int32> VertexIDMap;
			for (int32 k = 0; k < CollisionData.Vertices.Num(); ++k)
			{
				int32 VertexID = MeshOut.AppendVertex(TransformSeqeuence.TransformPosition((FVector)CollisionData.Vertices[k]));
				VertexIDMap.Add(VertexID);
			}
			for (const FTriIndices& TriIndices : CollisionData.Indices)
			{
				FIndex3i Triangle(TriIndices.v0, TriIndices.v1, TriIndices.v2);
				int32 TriangleID = MeshOut.AppendTriangle(Triangle);
				if (TriangleID < 0)
				{
					bFoundMeshErrorsOut = true;
					// create new vertices for this triangle
					int32 A = MeshOut.AppendVertex(TransformSeqeuence.TransformPosition(MeshOut.GetVertex(TriIndices.v0)));
					int32 B = MeshOut.AppendVertex(TransformSeqeuence.TransformPosition(MeshOut.GetVertex(TriIndices.v1)));
					int32 C = MeshOut.AppendVertex(TransformSeqeuence.TransformPosition(MeshOut.GetVertex(TriIndices.v2)));
					MeshOut.AppendTriangle(FIndex3i(A,B,C));
				}
			}

			if (bWeldEdges)
			{
				FMergeCoincidentMeshEdges Weld(&MeshOut);
				Weld.OnlyUniquePairs = true;
				Weld.Apply();
				Weld.OnlyUniquePairs = false;
				Weld.Apply();
			}

			if (!CollisionData.bFlipNormals != bWillInvert)		// collision mesh has reversed orientation
			{
				MeshOut.ReverseOrientation(false);
			}
		}
	}

	if (MeshOut.HasAttributes() && MeshOut.Attributes()->PrimaryNormals() != nullptr)
	{
		if (bSetToPerTriangleNormals)
		{
			FMeshNormals::InitializeMeshToPerTriangleNormals(&MeshOut);
		}
		else
		{
			FMeshNormals::InitializeOverlayToPerVertexNormals(MeshOut.Attributes()->PrimaryNormals(), false);
		}
	}

	return (MeshOut.TriangleCount() > 0);
}
