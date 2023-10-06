// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/MeshConvexHull.h"
#include "Solvers/MeshLinearization.h"
#include "MeshSimplification.h"
#include "DynamicMesh/MeshNormals.h"
#include "CompGeom/ConvexHull3.h"
#include "Util/GridIndexing3.h"

using namespace UE::Geometry;

bool FMeshConvexHull::Compute(FProgressCancel* Progress)
{
	bool bOK = false;
	if (VertexSet.Num() > 0)
	{
		bOK = Compute_VertexSubset(Progress);
	}
	else
	{
		bOK = Compute_FullMesh(Progress);
	}
	if (!bOK)
	{
		return false;
	}


	if (bPostSimplify)
	{
		bOK = SimplifyHull(ConvexHull, MaxTargetFaceCount, Progress);
	}

	return bOK;
}


bool FMeshConvexHull::SimplifyHull(FDynamicMesh3& HullMesh, int32 MaxTargetFaceCount, FProgressCancel* Progress)
{
	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	check(MaxTargetFaceCount > 0);
	bool bSimplified = false;
	if (HullMesh.TriangleCount() > MaxTargetFaceCount)
	{
		FVolPresMeshSimplification Simplifier(&HullMesh);
		Simplifier.CollapseMode = FVolPresMeshSimplification::ESimplificationCollapseModes::MinimalExistingVertexError;
		Simplifier.SimplifyToTriangleCount(MaxTargetFaceCount);
		bSimplified = true;
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (bSimplified)
	{
		// recalculate convex hull
		// TODO: test if simplified mesh is convex first, can just re-use in that case!!
		FMeshConvexHull SimplifiedHull(&HullMesh);
		if (SimplifiedHull.Compute(Progress))
		{
			HullMesh = MoveTemp(SimplifiedHull.ConvexHull);
		}
	}

	return true;
}



bool FMeshConvexHull::Compute_FullMesh(FProgressCancel* Progress)
{
	FConvexHull3d HullCompute;
	HullCompute.Progress = Progress;
	bool bOK = HullCompute.Solve(Mesh->MaxVertexID(),
		[this](int32 Index) { return Mesh->GetVertex(Index); },
		[this](int32 Index) { return Mesh->IsVertex(Index); });
	if (!bOK)
	{
		return false;
	}

	TMap<int32, int32> HullVertMap;

	ConvexHull = FDynamicMesh3(EMeshComponents::None);
	HullCompute.GetTriangles([&](FIndex3i Triangle)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			int32 Index = Triangle[j];
			if (HullVertMap.Contains(Index) == false)
			{
				FVector3d OrigPos = Mesh->GetVertex(Index);
				int32 NewVID = ConvexHull.AppendVertex(OrigPos);
				HullVertMap.Add(Index, NewVID);
				Triangle[j] = NewVID;
			}
			else
			{
				Triangle[j] = HullVertMap[Index];
			}
		}

		ConvexHull.AppendTriangle(Triangle);
	});

	return true;
}



bool FMeshConvexHull::Compute_VertexSubset(FProgressCancel* Progress)
{
	FConvexHull3d HullCompute;
	HullCompute.Progress = Progress;
	bool bOK = HullCompute.Solve(VertexSet.Num(),
		[this](int32 Index) { return Mesh->GetVertex(VertexSet[Index]); });
	if (!bOK)
	{
		return false;
	}

	TMap<int32, int32> HullVertMap;

	ConvexHull = FDynamicMesh3(EMeshComponents::None);
	HullCompute.GetTriangles([&](FIndex3i Triangle)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			int32 Index = Triangle[j];
			if (HullVertMap.Contains(Index) == false)
			{
				FVector3d OrigPos = Mesh->GetVertex(VertexSet[Index]);
				int32 NewVID = ConvexHull.AppendVertex(OrigPos);
				HullVertMap.Add(Index, NewVID);
				Triangle[j] = NewVID;
			}
			else
			{
				Triangle[j] = HullVertMap[Index];
			}
		}

		ConvexHull.AppendTriangle(Triangle);
	});

	return true;
}


FVector3i FMeshConvexHull::DebugGetCellIndex(const FDynamicMesh3& Mesh,
											 int GridResolutionMaxAxis,
											 int VertexIndex)
{
	FAxisAlignedBox3d Bounds = Mesh.GetBounds();
	Bounds.Min = Bounds.Min - 1e-4;			// Pad to avoid problems with vertices lying exactly on bounding box
	Bounds.Max = Bounds.Max + 1e-4;

	const double GridCellSize = Bounds.MaxDim() / (double)GridResolutionMaxAxis;

	FBoundsGridIndexer3d Indexer(Bounds, GridCellSize);

	return Indexer.ToGrid(Mesh.GetVertex(VertexIndex));
}


void FMeshConvexHull::GridSample(const FDynamicMesh3& Mesh, 
								 int GridResolutionMaxAxis, 
								 TArray<int32>& OutSamples)
{
	// Simple spatial hash to find a representative vertex for each occupied grid cell

	FAxisAlignedBox3d Bounds = Mesh.GetBounds();
	Bounds.Min = Bounds.Min - 1e-4;			// Pad to avoid problems with vertices lying exactly on bounding box
	Bounds.Max = Bounds.Max + 1e-4;
	const double GridCellSize = Bounds.MaxDim() / (double)GridResolutionMaxAxis;

	FBoundsGridIndexer3d Indexer(Bounds, GridCellSize);
	const FVector3i GridResolution = Indexer.GridResolution();

	// TODO: If the grid resolution is too high, use a TMap from grid cell index to vertex index instead of an array.
	// For smallish grids the array is more efficient.
	int TotalNumberGridCells = GridResolution.X * GridResolution.Y * GridResolution.Z;
	TArray<int32> GridCellVertex;
	GridCellVertex.Init(-1, TotalNumberGridCells);

	for (int VertexIndex : Mesh.VertexIndicesItr())
	{
		FVector3i CellIndex = Indexer.ToGrid(Mesh.GetVertex(VertexIndex));
		check(CellIndex.X >= 0 && CellIndex.X < GridResolution.X);
		check(CellIndex.Y >= 0 && CellIndex.Y < GridResolution.Y);
		check(CellIndex.Z >= 0 && CellIndex.Z < GridResolution.Z);

		int Key = CellIndex.X + CellIndex.Y * GridResolution.X + CellIndex.Z * GridResolution.X * GridResolution.Y;

		GridCellVertex[Key] = VertexIndex;
	}

	for (const int32 VertexIndex : GridCellVertex)
	{
		if (VertexIndex >= 0)
		{
			OutSamples.Add(VertexIndex);
		}
	}

}
