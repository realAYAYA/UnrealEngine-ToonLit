// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/WeldEdgeSequence.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"

using namespace UE::Geometry;

FWeldEdgeSequence::EWeldResult FWeldEdgeSequence::Weld()
{
	// As soon as any of these helper functions return a non-OK value,
	// we want to forward that to the user and stop operating.

	EWeldResult Result = EWeldResult::Ok;

	if ((Result = CheckInput()) != EWeldResult::Ok)
	{
		return Result;
	}

	if ((Result = SplitSmallerSpan()) != EWeldResult::Ok)
	{
		return Result;
	}

	if ((Result = CheckForAndCollapseSideTriangles()) != EWeldResult::Ok)
	{
		return Result;
	}

	if ((Result = WeldEdgeSequence()) != EWeldResult::Ok)
	{
		return Result;
	}

	return Result;
}



/** Protected Functions */

FWeldEdgeSequence::EWeldResult FWeldEdgeSequence::CheckInput()
{
	// Selected edges must be boundary edges
	for (int Edge : EdgeSpanToDiscard.Edges)
	{
		if (Mesh->IsBoundaryEdge(Edge) == false)
		{
			return EWeldResult::Failed_EdgesNotBoundaryEdges;
		}
	}

	for (int Edge : EdgeSpanToKeep.Edges)
	{
		if (Mesh->IsBoundaryEdge(Edge) == false)
		{
			return EWeldResult::Failed_EdgesNotBoundaryEdges;
		}
	}

	// Ensure that the two input spans are oriented according to mesh boundary
	// Guaranteed to be on boundary after two for loops above
	EdgeSpanToDiscard.SetCorrectOrientation();
	EdgeSpanToKeep.SetCorrectOrientation();

	return EWeldResult::Ok;
}

FWeldEdgeSequence::EWeldResult FWeldEdgeSequence::SplitSmallerSpan()
{
	// For each new vertex that must be created:
	// The longest simple edge is found and split
	// The newly generated vertex is inserted into the span
	// The newly generated edge is inserted into the span

	FEdgeSpan& SpanToSplit = (EdgeSpanToDiscard.Vertices.Num() < EdgeSpanToKeep.Vertices.Num()) ? EdgeSpanToDiscard : EdgeSpanToKeep;
	int TotalSplits = FMath::Abs(EdgeSpanToDiscard.Vertices.Num() - EdgeSpanToKeep.Vertices.Num());
	for (int SplitCount = 0; SplitCount < TotalSplits; ++SplitCount)
	{
		double MaxLength = 0.0;
		int LongestEID = -1;
		int LongestIndex = -1;

		// Find longest edge and store length, ID, and index
		for (int EdgeIndex = 0; EdgeIndex < SpanToSplit.Edges.Num(); ++EdgeIndex)
		{
			FVector3d VertA = Mesh->GetVertex(Mesh->GetEdge(SpanToSplit.Edges[EdgeIndex]).Vert.A);
			FVector3d VertB = Mesh->GetVertex(Mesh->GetEdge(SpanToSplit.Edges[EdgeIndex]).Vert.B);
			double EdgeLength = DistanceSquared(VertA, VertB);

			if (MaxLength < EdgeLength)
			{
				MaxLength = EdgeLength;
				LongestEID = SpanToSplit.Edges[EdgeIndex];
				LongestIndex = EdgeIndex;
			}
		}

		// Split longest edge
		FDynamicMesh3::FEdgeSplitInfo SplitInfo;
		EMeshResult Result = Mesh->SplitEdge(LongestEID, SplitInfo);
		if (Result != EMeshResult::Ok)
		{
			return EWeldResult::Failed_CannotSplitEdge;
		}

		// Correctly insert new vertex (between vertices of split edge)
		SpanToSplit.Vertices.Insert(SplitInfo.NewVertex, LongestIndex + 1);

		// Correctly insert new edge
		// OriginalVertices.B is the non-new vertex of the newly inserted edge- use this
		// to determine whether the edge goes before or after the original in our span
		if (SplitInfo.OriginalVertices.B == SpanToSplit.Vertices[LongestIndex])
		{
			SpanToSplit.Edges.Insert(SplitInfo.NewEdges.A, LongestIndex);
		}
		else
		{
			SpanToSplit.Edges.Insert(SplitInfo.NewEdges.A, LongestIndex + 1);
		}
	}

	return EWeldResult::Ok;
}

FWeldEdgeSequence::EWeldResult FWeldEdgeSequence::CheckForAndCollapseSideTriangles()
{
	// Checks for and deletes edge between VertA and VertB
	auto CheckForAndHandleEdge = [this](int VertA, int VertB)
	{
		int Edge = Mesh->FindEdge(VertA, VertB);
		if (Edge != IndexConstants::InvalidID)
		{
			if (bAllowIntermediateTriangleDeletion == false)
			{
				return EWeldResult::Failed_TriangleDeletionDisabled;
			}

			FIndex2i TrianglePair = Mesh->GetEdgeT(Edge);
			EMeshResult Result = Mesh->RemoveTriangle(TrianglePair.A);
			if (Result != EMeshResult::Ok)
			{
				return EWeldResult::Failed_CannotDeleteTriangle;
			}

			if (Mesh->IsTriangle(TrianglePair.B))
			{
				Result = Mesh->RemoveTriangle(TrianglePair.B);
				if (Result != EMeshResult::Ok)
				{
					return EWeldResult::Failed_CannotDeleteTriangle;
				}
			}
		}

		return EWeldResult::Ok;
	};

	EWeldResult Result = CheckForAndHandleEdge(EdgeSpanToDiscard.Vertices[0], EdgeSpanToKeep.Vertices.Last());
	if (Result != EWeldResult::Ok)
	{
		return Result;
	}
	
	Result = CheckForAndHandleEdge(EdgeSpanToDiscard.Vertices.Last(), EdgeSpanToKeep.Vertices[0]);
	if (Result != EWeldResult::Ok)
	{
		return Result;
	}

	return EWeldResult::Ok;
}

FWeldEdgeSequence::EWeldResult FWeldEdgeSequence::WeldEdgeSequence()
{
	if (!ensure(EdgeSpanToDiscard.Edges.Num() == EdgeSpanToKeep.Edges.Num()))
	{
		return EWeldResult::Failed_Other;
	}

	for (int EdgePairIndex = 0; EdgePairIndex < EdgeSpanToDiscard.Edges.Num(); ++EdgePairIndex)
	{
		int EdgeA = EdgeSpanToDiscard.Edges[EdgeSpanToDiscard.Edges.Num() - 1 - EdgePairIndex];
		int EdgeB = EdgeSpanToKeep.Edges[EdgePairIndex];

		// We need to check this because MergeEdges() can implicitly
		// merge adjacent edges when adjacent edges share a vertex.
		if (Mesh->IsEdge(EdgeA) && Mesh->IsEdge(EdgeB))
		{
			FDynamicMesh3::FMergeEdgesInfo MergeInfo;
			EMeshResult Result = Mesh->MergeEdges(EdgeB, EdgeA, MergeInfo);
			if (Result != EMeshResult::Ok)
			{
				if (Result == EMeshResult::Failed_InvalidNeighbourhood && bAllowFailedMerge == true)
				{
					// Get properly oriented vertex pairs for edges which can't be merged
					FIndex2i EdgeAVerts = Mesh->GetEdgeV(EdgeA);
					FIndex2i EdgeBVerts = Mesh->GetEdgeV(EdgeB);
					IndexUtil::OrientTriEdge(EdgeAVerts.A, EdgeAVerts.B, Mesh->GetTriangle(Mesh->GetEdgeT(EdgeA).A));
					IndexUtil::OrientTriEdge(EdgeBVerts.A, EdgeBVerts.B, Mesh->GetTriangle(Mesh->GetEdgeT(EdgeB).A));

					// Move verts from EdgeA to EdgeB, leaving an invisible seam
					Mesh->SetVertex(EdgeAVerts.A, Mesh->GetVertex(EdgeBVerts.B));
					Mesh->SetVertex(EdgeAVerts.B, Mesh->GetVertex(EdgeBVerts.A));

					// Add eids to UnmergedEdgePairsOut so user can easily identify offending edges
					UnmergedEdgePairsOut.Add(TPair<int, int>(EdgeA, EdgeB));
				}
				else
				{
					return EWeldResult::Failed_Other;
				}
			}
		}
	}

	return EWeldResult::Ok;
}
