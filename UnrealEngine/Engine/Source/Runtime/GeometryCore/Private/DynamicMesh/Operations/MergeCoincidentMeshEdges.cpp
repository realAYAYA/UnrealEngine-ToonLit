// Copyright Epic Games, Inc. All Rights Reserved.


#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "DynamicMesh/Operations/SplitAttributeWelder.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshAdapterUtil.h"
#include "Spatial/PointSetHashTable.h"
#include "Util/IndexPriorityQueue.h"
#include "Util/IndexUtil.h"

using namespace UE::Geometry;

const double FMergeCoincidentMeshEdges::DEFAULT_TOLERANCE = FMathf::ZeroTolerance;


bool FMergeCoincidentMeshEdges::Apply()
{
	MergeVtxDistSqr = MergeVertexTolerance * MergeVertexTolerance;
	double UseMergeSearchTol = (MergeSearchTolerance > 0) ? MergeSearchTolerance : 2*MergeVertexTolerance;

	//
	// construct hash table for edge midpoints
	//

	FPointSetAdapterd EdgeMidpoints = UE::Geometry::MakeBoundaryEdgeMidpointsAdapter(Mesh);
	FPointSetHashtable MidpointsHash(&EdgeMidpoints);

	// use denser grid as vertex count increases
	int hashN = 64;
	if (Mesh->TriangleCount() > 100000)   hashN = 128;
	if (Mesh->TriangleCount() > 1000000)  hashN = 256;
	FAxisAlignedBox3d Bounds = Mesh->GetBounds(true);
	double CellSize = FMath::Max(FMathd::ZeroTolerance, Bounds.MaxDim() / (double)hashN);
	MidpointsHash.Build(CellSize, Bounds.Min);

	UseMergeSearchTol = FMathd::Min(CellSize, UseMergeSearchTol);

	// temp values and buffers
	FVector3d A, B, C, D;
	TArray<int> equivBuffer;
	TArray<int> SearchMatches;
	SearchMatches.SetNum(1024); SearchMatches.Reset();  // allocate buffer

	//
	// construct edge equivalence sets. First we find all other edges with same
	// midpoint, and then we form equivalence set for edge from subset that also
	// has same endpoints
	//

	typedef TArray<int> EdgesList;
	TArray<EdgesList*> EquivalenceSets;
	EquivalenceSets.Init(nullptr, Mesh->MaxEdgeID());
	TSet<int> RemainingEdges;

	// @todo equivalence sets should be symmetric. this neither enforces that,
	// nor takes advantage of it.
	InitialNumBoundaryEdges = 0;
	for (int eid : Mesh->BoundaryEdgeIndicesItr()) 
	{
		InitialNumBoundaryEdges++;

		FVector3d midpt = Mesh->GetEdgePoint(eid, 0.5);

		// find all other edges with same midpoint in query sphere
		SearchMatches.Reset();
		MidpointsHash.FindPointsInBall(midpt, UseMergeSearchTol, SearchMatches);

		int N = SearchMatches.Num();
		if (N == 1 && SearchMatches[0] != eid)
		{
			check(false);	// how could this happen?!
		}
		if (N <= 1)
		{
			continue;		// edge has no matches
		}

		Mesh->GetEdgeV(eid, A, B);

		// if same endpoints, add to equivalence set for this edge
		equivBuffer.Reset();
		for (int i = 0; i < N; ++i) 
		{
			if (SearchMatches[i] != eid) 
			{
				Mesh->GetEdgeV(SearchMatches[i], C, D);
				if ( IsSameEdge(A, B, C, D) ) 
				{
					equivBuffer.Add(SearchMatches[i]);
				}
			}
		}
		if (equivBuffer.Num() > 0)
		{
			EquivalenceSets[eid] = new EdgesList(equivBuffer);
			RemainingEdges.Add(eid);
		}
	}


	//
	// add potential duplicate edges to priority queue, sorted by number of possible matches. 
	//

	// [TODO] could replace remaining hashset w/ PQ, and use conservative count?
	// [TODO] Does this need to be a PQ? Not updating PQ below anyway...
	FIndexPriorityQueue DuplicatesQueue;
	DuplicatesQueue.Initialize(Mesh->MaxEdgeID());
	for (int eid : RemainingEdges) 
	{
		if (OnlyUniquePairs) 
		{
			if (EquivalenceSets[eid]->Num() != 1)
			{
				continue;
			}

			// check that reverse match is the same and unique
			int other_eid = (*EquivalenceSets[eid])[0];
			if (EquivalenceSets[other_eid]->Num() != 1 || (*EquivalenceSets[other_eid])[0] != eid)
			{
				continue;
			}
		}
		const float Priority = (float)EquivalenceSets[eid]->Num();
		DuplicatesQueue.Insert(eid, Priority);
	}

	//
	// process all potential matches, merging edges as we go in a greedy fashion.
	//

	while (DuplicatesQueue.GetCount() > 0) 
	{
		int eid = DuplicatesQueue.Dequeue();
		
		if (Mesh->IsEdge(eid) == false || EquivalenceSets[eid] == nullptr || RemainingEdges.Contains(eid) == false)
		{
			continue;               // dealt with this edge already
		}
		if (Mesh->IsBoundaryEdge(eid) == false)
		{
			continue;				// this edge got merged already
		}

		EdgesList& Matches = *EquivalenceSets[eid];

		// select best viable match (currently just "first"...)
		// @todo could we make better decisions here? prefer planarity?
		bool bMerged = false;
		int FailedCount = 0;
		for (int i = 0; i < Matches.Num() && bMerged == false; ++i) 
		{
			int other_eid = Matches[i];
			if (Mesh->IsEdge(other_eid) == false || Mesh->IsBoundaryEdge(other_eid) == false)
			{
				continue;
			}

			FDynamicMesh3::FMergeEdgesInfo mergeInfo;
			EMeshResult result = Mesh->MergeEdges(eid, other_eid, mergeInfo);
			if (result != EMeshResult::Ok) 
			{
				// if the operation failed we remove this edge from the equivalence set
				Matches.RemoveAt(i);
				i--;

				EquivalenceSets[other_eid]->Remove(eid);
				//DuplicatesQueue.UpdatePriority(...);  // should we do this?

				FailedCount++;
			}
			else 
			{
				// ok we merged, other edge is no longer free
				bMerged = true;
				delete EquivalenceSets[other_eid];
				EquivalenceSets[other_eid] = nullptr;
				RemainingEdges.Remove(other_eid);

				// weld attributes 
				if (bWeldAttrsOnMergedEdges)
				{ 
					SplitAttributeWelder.WeldSplitElements(*Mesh, mergeInfo.KeptVerts[0]);
					SplitAttributeWelder.WeldSplitElements(*Mesh, mergeInfo.KeptVerts[1]);
				}
			}
		}

		// Removing branch with two identical cases to fix static analysis warning.
		// However, these two branches are *not* the same...we're just not sure 
		// what the right thing to do is in the else case
		//if (bMerged) 
		//{
			delete EquivalenceSets[eid];
			EquivalenceSets[eid] = nullptr;
			RemainingEdges.Remove(eid);
		//}
		//else 
		//{
		//	// should we do something else here? doesn't make sense to put
		//	// back into Q, as it should be at the top, right?
		//	delete EquivalenceSets[eid];
		//	EquivalenceSets[eid] = nullptr;
		//	RemainingEdges.Remove(eid);
		//}

	}

	FinalNumBoundaryEdges = 0;
	for (int eid : Mesh->BoundaryEdgeIndicesItr())
	{
		FinalNumBoundaryEdges++;
	}

	return true;
}

