// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parameterization/MeshRegionGraph.h"

#include "Async/ParallelFor.h"

using namespace UE::Geometry;


void FMeshRegionGraph::BuildFromComponents(
	const FDynamicMesh3& Mesh, 
	const FMeshConnectedComponents& Components, 
	TFunctionRef<int32(int32)> ExternalIDFunc,
	TFunctionRef<bool(int32, int32)> TrisConnectedPredicate)
{
	int32 N = Components.Num();
	Regions.SetNum(N);
	TriangleToRegionMap.Init(-1, Mesh.MaxTriangleID());
	TriangleNbrTris.Init(FIndex3i(-1, -1, -1), Mesh.MaxTriangleID());

	for (int32 k = 0; k < N; ++k)
	{
		Regions[k].Triangles = Components[k].Indices;
		Regions[k].ExternalID = ExternalIDFunc(k);

		for (int32 tid : Components[k].Indices)
		{
			TriangleToRegionMap[tid] = k;
			FIndex3i NbrTris = Mesh.GetTriNeighbourTris(tid);;
			for (int j = 0; j < 3; ++j)
			{
				if (NbrTris[j] >= 0 && TrisConnectedPredicate(tid, NbrTris[j]) == false)
				{
					NbrTris[j] = -1;
				}
			}
			TriangleNbrTris[tid] = NbrTris;
		}
	}

	for (int32 k = 0; k < N; ++k)
	{
		BuildNeigbours(k);
	}
}


void FMeshRegionGraph::BuildFromTriangleSets(const FDynamicMesh3& Mesh,
	const TArray<TArray<int32>>& TriangleSets,
	TFunctionRef<int32(int32)> ExternalIDFunc,
	TFunctionRef<bool(int32, int32)> TrisConnectedPredicate)
{
	int32 N = TriangleSets.Num();
	Regions.SetNum(N);
	TriangleToRegionMap.Init(-1, Mesh.MaxTriangleID());
	TriangleNbrTris.Init(FIndex3i(-1, -1, -1), Mesh.MaxTriangleID());

	for (int32 k = 0; k < N; ++k)
	{
		Regions[k].Triangles = TriangleSets[k];
		Regions[k].ExternalID = ExternalIDFunc(k);

		for (int32 tid : TriangleSets[k])
		{
			TriangleToRegionMap[tid] = k;
			FIndex3i NbrTris = Mesh.GetTriNeighbourTris(tid);
			for (int j = 0; j < 3; ++j)
			{
				if (NbrTris[j] >= 0 && TrisConnectedPredicate(tid, NbrTris[j]) == false)
				{
					NbrTris[j] = -1;
				}
			}
			TriangleNbrTris[tid] = NbrTris;
		}
	}

	for (int32 k = 0; k < N; ++k)
	{
		BuildNeigbours(k);
	}
}



TArray<int32> FMeshRegionGraph::GetNeighbours(int32 RegionIdx) const
{
	TArray<int32> Nbrs;
	if (IsRegion(RegionIdx))
	{
		for (const FNeighbour& Nbr : Regions[RegionIdx].Neighbours)
		{
			Nbrs.Add(Nbr.RegionIndex);
		}
	}
	return Nbrs;
}


bool FMeshRegionGraph::AreRegionsConnected(int32 RegionAIndex, int32 RegionBIndex) const
{
	if (IsRegion(RegionAIndex) == false || IsRegion(RegionBIndex) == false) return false;

	for (const FNeighbour& Nbr : Regions[RegionAIndex].Neighbours)
	{
		if (Nbr.RegionIndex == RegionBIndex)
		{
			return true;
		}
	}
	return false;
}



bool FMeshRegionGraph::MergeSmallRegions(int32 SmallThreshold,
										 TFunctionRef<float(int32 SmallRgnIdx, int32 NbrRgnIdx)> RegionSimilarityFunc)
{
	bool bMergedAny = false;
	int32 UseSmallThreshold = 1;
	while (UseSmallThreshold < SmallThreshold || MergeSmallRegionsPass(UseSmallThreshold, RegionSimilarityFunc))
	{
		bMergedAny = true;
		UseSmallThreshold = FMath::Min(UseSmallThreshold + 1, SmallThreshold);
	}
	return bMergedAny;
}



bool FMeshRegionGraph::MergeSmallRegionsPass(int32 SmallThreshold,
										 TFunctionRef<float(int32 SmallRgnIdx, int32 NbrRgnIdx)> RegionSimilarityFunc)
{
	bool bMergedAny = false;

	TArray<int32> SmallRegions;
	for (int32 k = 0; k < MaxRegionIndex(); ++k)
	{
		if (IsRegion(k) && Regions[k].Triangles.Num() <= SmallThreshold)
		{
			SmallRegions.Add(k);
		}
	}
	if (SmallRegions.Num() == 0)
	{
		return false;
	}

	SmallRegions.Sort([this](int a, int b) { return Regions[a].Triangles.Num() < Regions[b].Triangles.Num(); });

	struct FMatch 
	{
		float Score = 0.0f;
		int Index = -1;
	};

	for (int32 j = 0; j < SmallRegions.Num(); ++j)
	{
		int32 SmallRegionIdx = SmallRegions[j];
		if (IsRegion(SmallRegionIdx) == false) continue;
		const FRegion& SmallRegion = Regions[SmallRegionIdx];

		TArray<FMatch> Matches;
		for (const FNeighbour& Nbr : SmallRegion.Neighbours)
		{
			float Score = RegionSimilarityFunc(SmallRegionIdx, Nbr.RegionIndex);
			Matches.Add(FMatch{ Score, Nbr.RegionIndex });
		}
		if (Matches.Num() == 0)
		{
			continue;
		}

		Matches.Sort([](const FMatch& A, const FMatch& B) { return A.Score > B.Score; });

		if (MergeRegion(SmallRegionIdx, Matches[0].Index))
		{
			bMergedAny = true;
		}
	}

	return bMergedAny;
}




bool FMeshRegionGraph::MergeRegion(int32 RemoveRgnIdx, int32 MergeToRgnIdx)
{
	if (AreRegionsConnected(RemoveRgnIdx, MergeToRgnIdx) == false)
	{
		ensure(false);
		return false;
	}

	FRegion& RemoveRegion = Regions[RemoveRgnIdx];
	FRegion& AppendToRegion = Regions[MergeToRgnIdx];

	// append triangles to the new region
	for (int32 tid : RemoveRegion.Triangles)
	{
		AppendToRegion.Triangles.Add(tid);
		TriangleToRegionMap[tid] = MergeToRgnIdx;
	}
	RemoveRegion.Triangles.Reset();
	RemoveRegion.bValid = false;

	// rebuild all affected regions, which should just be all nbrs of the removed region
	// (which will include the merge-to region)
	// TODO: this can be more efficient. At minimum can update all nbr sets and then rebuild counts
	for (FNeighbour& Nbr : RemoveRegion.Neighbours)
	{
		BuildNeigbours(Nbr.RegionIndex);
	}

	return true;
}



bool FMeshRegionGraph::OptimizeBorders(int32 MaxRounds)
{
	auto GetSwapNbrRegionIndex = [this](int32 tid) -> int32
	{
		int32 Index = TriangleToRegionMap[tid];
		FIndex3i NbrTris = TriangleNbrTris[tid];
		int32 NbrCount = 0;
		TArray<int32, TFixedAllocator<3>> UniqueNbrs;
		for (int32 j = 0; j < 3; ++j)
		{
			if (NbrTris[j] >= 0)
			{
				int32 NbrRegionIndex = TriangleToRegionMap[NbrTris[j]];
				if (IsRegion(NbrRegionIndex) && NbrRegionIndex != Index)
				{
					NbrCount++;
					UniqueNbrs.AddUnique(NbrRegionIndex);
				}
			}
		}
		if (NbrCount >= 2 && UniqueNbrs.Num() == 1)
		{
			return UniqueNbrs[0];
		}
		return -1;
	};

	bool bModified = false;

	bool bDone = false;
	int32 RoundCounter = 0;
	while (bDone == false && RoundCounter++ < MaxRounds)
	{
		TArray<FIndex2i> PotentialSwaps;
		for (const FRegion& Region : Regions)
		{
			for (int32 tid : Region.Triangles)
			{
				int32 SwapToNbrIndex = GetSwapNbrRegionIndex(tid);
				if (IsRegion(SwapToNbrIndex))
				{
					PotentialSwaps.Add( FIndex2i(tid, SwapToNbrIndex) );
				}
			}
		}
		bDone = (PotentialSwaps.Num() == 0);

		for (const FIndex2i& Swap : PotentialSwaps)
		{
			int32 tid = Swap.A;
			int32 Index = TriangleToRegionMap[tid];
			int32 SwapToNbrIndex = Swap.B;

			int32 CheckNbrIndex = GetSwapNbrRegionIndex(tid);
			if (CheckNbrIndex == SwapToNbrIndex)
			{
				Regions[Index].Triangles.RemoveSwap(tid, false);
				Regions[SwapToNbrIndex].Triangles.Add(tid);
				TriangleToRegionMap[tid] = SwapToNbrIndex;
				bModified = true;
			}
		}
	}

	return bModified;
}





void FMeshRegionGraph::BuildNeigbours(int32 RegionIdx)
{
	auto CountNeighbourFunc = [](FRegion& Region, int32 NbrRegionIndex)
	{
		for (FNeighbour& Nbr : Region.Neighbours)
		{
			if (Nbr.RegionIndex == NbrRegionIndex)
			{
				Nbr.Count++;
				return;
			}
		}
		check(false);
	};


	FRegion& Region = Regions[RegionIdx];
	Region.Neighbours.Reset();

	TSet<int32> NeighbourSet;

	for (int32 tid : Region.Triangles)
	{
		int32 RegionIndex = TriangleToRegionMap[tid];
		check(RegionIndex == RegionIdx);

		FIndex3i Nbrs = TriangleNbrTris[tid];
		for (int32 j = 0; j < 3; ++j)
		{
			if (Nbrs[j] == FDynamicMesh3::InvalidID)
			{
				Region.bIsOnMeshBoundary = true;
			}
			else
			{
				int32 NbrRegionIndex = TriangleToRegionMap[Nbrs[j]];
				if (NbrRegionIndex == -1)
				{
					Region.bIsOnROIBoundary = true;
				}
				else
				{
					if (NbrRegionIndex != RegionIndex)
					{
						if (NeighbourSet.Contains(NbrRegionIndex) == false)
						{
							NeighbourSet.Add(NbrRegionIndex);
							FNeighbour Nbr;
							Nbr.RegionIndex = NbrRegionIndex;
							Region.Neighbours.Add(Nbr);
						}

						CountNeighbourFunc(Region, NbrRegionIndex);
					}
				}
			}
		}
	}
}