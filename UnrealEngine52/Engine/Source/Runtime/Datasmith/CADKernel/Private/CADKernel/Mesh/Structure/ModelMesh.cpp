// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Structure/ModelMesh.h"

#include "CADKernel/Mesh/Criteria/Criterion.h"
#include "CADKernel/Mesh/Structure/EdgeMesh.h"
#include "CADKernel/Mesh/Structure/Mesh.h"
#include "CADKernel/Mesh/Structure/FaceMesh.h"
#include "CADKernel/Mesh/Structure/VertexMesh.h"
#include "CADKernel/Topo/TopologicalEntity.h"

namespace UE::CADKernel
{

void FModelMesh::AddCriterion(TSharedPtr<FCriterion>& Criterion)
{
	Criteria.Add(Criterion);
	switch (Criterion->GetCriterionType())
	{
	case ECriterion::MinSize:
		MinSize = Criterion->Value();
		break;
	case ECriterion::MaxSize:
		MaxSize = Criterion->Value();
		break;
	case ECriterion::Angle:
		MaxAngle = Criterion->Value();
		break;
	case ECriterion::Sag:
		Sag = Criterion->Value();
		break;
	case ECriterion::CADCurvature:
		QuadAnalyse = true;
		break;
	}
}

#ifdef CADKERNEL_DEV
FInfoEntity& FModelMesh::GetInfo(FInfoEntity& Info) const
{
	return FEntityGeom::GetInfo(Info)
		.Add(TEXT("Surface Meshes"), FaceMeshes)
		.Add(TEXT("Edge Meshes"), EdgeMeshes)
		.Add(TEXT("Vertex Meshes"), VertexMeshes);
}
#endif

const int32 FModelMesh::GetIndexOfVertexFromId(const int32 Ident) const
{
	for (const TSharedPtr<FVertexMesh>& VertexMesh : VertexMeshes)
	{
		if (Ident == VertexMesh->GetStartVertexId())
		{
			return VertexMesh->GetIndexInMeshModel();
		}
	}
	return -1;
}

const TSharedPtr<FVertexMesh> FModelMesh::GetMeshOfVertexNodeId(const int32 Ident) const
{
	for (const TSharedPtr<FVertexMesh>& VertexMesh : VertexMeshes)
	{
		if (Ident == VertexMesh->GetStartVertexId())
		{
			return VertexMesh;
		}
	}
	return nullptr;
}

const int32 FModelMesh::GetIndexOfEdgeFromId(const int32 Ident) const
{
	for (const TSharedPtr<FEdgeMesh>& EdgeMesh : EdgeMeshes)
	{
		if (Ident >= EdgeMesh->GetStartVertexId())
		{
			if (Ident <= EdgeMesh->GetLastVertexIndex())
			{
				return EdgeMesh->GetIndexInMeshModel();
			}
		}
	}
	return -1;
}

const int32 FModelMesh::GetIndexOfSurfaceFromId(const int32 Ident) const
{
	for (const TSharedPtr<FFaceMesh>& FaceMesh : FaceMeshes)
	{
		if (Ident >= FaceMesh->GetStartVertexId())
		{
			if (Ident <= FaceMesh->GetLastVertexIndex())
			{
				return FaceMesh->GetIndexInMeshModel();
			}
		}
	}
	return -1;
}

void FModelMesh::GetNodeCoordinates(TArray<FPoint>& NodeCoordinates) const
{
	NodeCoordinates.Reserve(LastIdUsed + 1);

	for (const TArray<FPoint>* PointArray : GlobalPointCloud)
	{
		NodeCoordinates.Insert(*PointArray, NodeCoordinates.Num());
	}
}

void FModelMesh::GetNodeCoordinates(TArray<FVector3f>& NodeCoordinates) const
{
	NodeCoordinates.Reserve(LastIdUsed);

	for (const TArray<FPoint>* PointArray : GlobalPointCloud)
	{
		for (const FPoint& Point : *PointArray)
		{
			NodeCoordinates.Emplace(Point.X, Point.Y, Point.Z);
		}
	}
}

const TArray<TSharedPtr<FMesh>>& FModelMesh::GetMeshes() const
{
	if (FaceMeshes.Num())
	{
		return (TArray<TSharedPtr<FMesh>>&) FaceMeshes;
	}
	if (EdgeMeshes.Num())
	{
		return (TArray<TSharedPtr<FMesh>>&) EdgeMeshes;
	}
	return (TArray<TSharedPtr<FMesh>>&) VertexMeshes;
}

int32 FModelMesh::GetTriangleCount() const
{
	int32 TriangleCount = 0;
	for (const TSharedPtr<FFaceMesh>& FaceMesh : FaceMeshes)
	{
		TriangleCount += FaceMesh->TrianglesVerticesIndex.Num() / 3;
	}
	return TriangleCount;
}

} // namespace UE::CADKernel

