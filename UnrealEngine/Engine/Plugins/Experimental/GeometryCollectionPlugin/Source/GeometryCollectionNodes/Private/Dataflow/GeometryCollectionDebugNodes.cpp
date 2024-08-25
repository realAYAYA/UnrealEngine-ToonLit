// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionDebugNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Containers/UnrealString.h"

#include "Misc/FileHelper.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshEditor.h"
#include "Generators/BoxSphereGenerator.h"
#include "FractureEngineConvex.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionDebugNodes)

namespace Dataflow
{
	void GeometryCollectionDebugNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConvexHullToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSphereCoveringToMeshDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMeshToOBJStringDebugDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSphereCoveringCountSpheresNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FWriteStringToFile);


		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection|Development", FLinearColor(1.f, 0.f, 0.f), CDefaultNodeBodyTintColor);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection|Mesh", FLinearColor(1.f, 0.16f, 0.05f), CDefaultNodeBodyTintColor);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("SphereCovering", FLinearColor(1.f, 0.16f, 0.05f), CDefaultNodeBodyTintColor);
	}
}

void FConvexHullToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		TArray<int32> TransformSelection;
		bool bHasSelection = IsConnected(&OptionalSelectionFilter);
		if (bHasSelection)
		{
			const FDataflowTransformSelection& InOptionalSelectionFilter = GetValue(Context, &OptionalSelectionFilter);
			TransformSelection = InOptionalSelectionFilter.AsArray();
		}

		FDynamicMesh3 HullsMesh;
		UE::FractureEngine::Convex::GetConvexHullsAsDynamicMesh(InCollection, HullsMesh, bHasSelection, TransformSelection);

		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->SetMesh(MoveTemp(HullsMesh));
		SetValue(Context, NewMesh, &Mesh);
	}
}

void FSphereCoveringToMeshDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Geometry;

	if (Out->IsA(&Mesh))
	{
		TObjectPtr<UDynamicMesh> NewMesh = NewObject<UDynamicMesh>();
		NewMesh->Reset();

		FDynamicMesh3 Accum;
		FDynamicMeshEditor Editor(&Accum);
		FMeshIndexMappings IndexMaps_Unused;
		FDataflowSphereCovering InSphereCovering = GetValue(Context, &SphereCovering);
		for (int32 SphereIdx = 0; SphereIdx < InSphereCovering.Spheres.Num(); ++SphereIdx)
		{
			FBoxSphereGenerator SphereGen;
			SphereGen.EdgeVertices.A = SphereGen.EdgeVertices.B = SphereGen.EdgeVertices.C = FMath::Max(2, VerticesAlongEachSide);
			SphereGen.Radius = InSphereCovering.Spheres.GetRadius(SphereIdx);
			FVector Center = InSphereCovering.Spheres.GetCenter(SphereIdx);
			FDynamicMesh3 Sphere(&SphereGen.Generate());
			Editor.AppendMesh(&Sphere, IndexMaps_Unused,
				[Center](int VID, const FVector3d& Pos) { return Pos + Center; });
			IndexMaps_Unused.Reset();
		}
		NewMesh->SetMesh(MoveTemp(Accum));

		SetValue(Context, NewMesh, &Mesh);

	}
}

void FMeshToOBJStringDebugDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&StringOBJ))
	{
		const UDynamicMesh* InMesh = GetValue(Context, &Mesh);
		const UE::Geometry::FDynamicMesh3& MeshRef = InMesh->GetMeshRef();
		FString Build;
		Build.Reset(MeshRef.MaxVertexID() * 40 + MeshRef.TriangleCount() * 24);
		for (int32 VID = 0; VID < MeshRef.MaxVertexID(); ++VID)
		{
			FVector V = FVector::ZeroVector;
			if (MeshRef.IsVertex(VID))
			{
				V = MeshRef.GetVertex(VID);
			}
			Build += FString::Printf(TEXT("v %f %f %f\n"), V.X, V.Y, V.Z);
		}
		bool bInInvertFaces = GetValue(Context, &bInvertFaces);
		for (int32 TID = 0; TID < MeshRef.MaxTriangleID(); ++TID)
		{
			if (MeshRef.IsTriangle(TID))
			{
				UE::Geometry::FIndex3i T = MeshRef.GetTriangle(TID);
				// Note: OBJ viewers generally expect the opposite triangle winding from UE meshes, so inverted == the UE order
				if (!bInInvertFaces)
				{
					Swap(T.B, T.C);
				}
				Build += FString::Printf(TEXT("f %d %d %d\n"), T.A + 1, T.B + 1, T.C + 1);
			}
		}
		SetValue(Context, MoveTemp(Build), &StringOBJ);
	}
}

void FWriteStringToFile::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FString InContents = GetValue(Context, &FileContents);
	FString InPath = GetValue(Context, &FilePath);

	bool bSuccess = FFileHelper::SaveStringToFile(InContents, *InPath);
	if (!bSuccess)
	{
		UE_LOG(LogChaos, Warning, TEXT("Failed to write to file %s:\n\n%s"), *InPath, *InContents);
	}
}
