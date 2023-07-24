// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureEngineUtility.h"
#include "DynamicMesh/DynamicMesh3.h"


void FFractureEngineUtility::ConvertBoxToVertexAndTriangleData(const FBox& InBox, TArray<FVector3f>& OutVertices, TArray<FIntVector>& OutTriangles)
{
	const int32 NumVertices = 8;
	const int32 NumTriangles = 12;

	OutVertices.AddUninitialized(NumVertices);
	OutTriangles.AddUninitialized(NumTriangles);

	FVector Min = InBox.Min;
	FVector Max = InBox.Max;

	// Add vertices
	OutVertices[0] = FVector3f(Min);
	OutVertices[1] = FVector3f(Max.X, Min.Y, Min.Z);
	OutVertices[2] = FVector3f(Max.X, Max.Y, Min.Z);
	OutVertices[3] = FVector3f(Min.X, Max.Y, Min.Z);
	OutVertices[4] = FVector3f(Min.X, Min.Y, Max.Z);
	OutVertices[5] = FVector3f(Max.X, Min.Y, Max.Z);
	OutVertices[6] = FVector3f(Max);
	OutVertices[7] = FVector3f(Min.X, Max.Y, Max.Z);

	// Add triangles
	OutTriangles[0] = FIntVector(0, 1, 3); OutTriangles[1] = FIntVector(1, 2, 3);
	OutTriangles[2] = FIntVector(0, 4, 1); OutTriangles[3] = FIntVector(4, 5, 1);
	OutTriangles[4] = FIntVector(5, 2, 1); OutTriangles[5] = FIntVector(5, 6, 2);
	OutTriangles[6] = FIntVector(3, 2, 6); OutTriangles[7] = FIntVector(7, 3, 6);
	OutTriangles[8] = FIntVector(0, 3, 7); OutTriangles[9] = FIntVector(4, 0, 7);
	OutTriangles[10] = FIntVector(5, 4, 7); OutTriangles[11] = FIntVector(5, 7, 6);
}


void FFractureEngineUtility::ConstructMesh(UE::Geometry::FDynamicMesh3& OutMesh, const TArray<FVector3f>& InVertices, const TArray<FIntVector>& InTriangles)
{
	for (int32 VertexIdx = 0; VertexIdx < InVertices.Num(); ++VertexIdx)
	{
		OutMesh.AppendVertex(FVector(InVertices[VertexIdx]));
	}

	int GroupID = 0;
	for (int32 TriangleIdx = 0; TriangleIdx < InTriangles.Num(); ++TriangleIdx)
	{
		OutMesh.AppendTriangle(InTriangles[TriangleIdx].X, InTriangles[TriangleIdx].Y, InTriangles[TriangleIdx].Z, GroupID);
	}
}


void FFractureEngineUtility::DeconstructMesh(const UE::Geometry::FDynamicMesh3& InMesh, TArray<FVector3f>& OutVertices, TArray<FIntVector>& OutTriangles)
{
	const int32 NumVertices = InMesh.VertexCount();
	const int32 NumTriangles = InMesh.TriangleCount();

	if (NumVertices > 0 && NumTriangles > 0)
	{
		// This will contain the valid triangles only
		OutTriangles.Reserve(InMesh.TriangleCount());

		// DynamicMesh.TrianglesItr() returns the valid triangles only
		for (UE::Geometry::FIndex3i Tri : InMesh.TrianglesItr())
		{
			OutTriangles.Add(FIntVector(Tri.A, Tri.B, Tri.C));
		}

		// This will contain all the vertices (invalid ones too)
		// Otherwise the IDs need to be remaped
		OutVertices.AddZeroed(InMesh.MaxVertexID());

		// DynamicMesh.VertexIndicesItr() returns the valid vertices only
		for (int32 VertexID : InMesh.VertexIndicesItr())
		{
			OutVertices[VertexID] = (FVector3f)InMesh.GetVertex(VertexID);
		}
	}
}
