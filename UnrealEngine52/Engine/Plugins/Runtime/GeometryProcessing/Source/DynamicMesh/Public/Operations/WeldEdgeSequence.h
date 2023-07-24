// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EdgeSpan.h"

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

/**
* Weld a pair of group edges.
* 
* User can optionally allow triangle deletion which handles cases
* where the group edges are connected by an edge at the end points.
*/

class DYNAMICMESH_API FWeldEdgeSequence
{
public:
	enum class EWeldResult
	{
		Ok								= 0,

		Failed_EdgesNotBoundaryEdges	= 10,		// Occurs when any edge in either input span isn't a boundary edge.

		Failed_CannotSplitEdge			= 21,		// Occurs when SplitEdge() fails, haven't encountered this and I'm not sure what would cause it
		Failed_TriangleDeletionDisabled = 22,		// Occurs when bAllowIntermediateTriangleDeletion is false and edge spans are connected by an edge
		Failed_CannotDeleteTriangle		= 23,		// Occurs when bAllowIntermediateTriangleDeletion is true, edge spans are connected, but edge deletion fails

		Failed_Other					= 100,		// Catch all for general failure
	};

public:
	/**
	* Inputs/Outputs
	*/
	FDynamicMesh3* Mesh;
	FEdgeSpan EdgeSpanToDiscard;	// This data is junk once Weld() is called
	FEdgeSpan EdgeSpanToKeep;		// This is the updated edge span which can be used once Weld() is called

	// Whether triangle deletion is allowed in order to merge edges which are connected by a different edge
	// This specifically determines how CheckForAndCollapseSideTriangles() behaves
	bool bAllowIntermediateTriangleDeletion = false;

	// When true, failed calls to MergeEdges() will be handled by moving the edges without merging such
	// that the final result appears to be welded but has invisible seam(s) instead of just failing.
	bool bAllowFailedMerge = false;

	// This is populated with pairs of eids which were not able to be merged.
	// Only valid when bAllowFailedMerge is true
	TArray<TPair<int, int>> UnmergedEdgePairsOut;

public:
	FWeldEdgeSequence(FDynamicMesh3* Mesh, FEdgeSpan SpanDiscard, FEdgeSpan SpanKeep)
		: Mesh(Mesh)
		, EdgeSpanToDiscard(SpanDiscard)
		, EdgeSpanToKeep(SpanKeep)
	{
	}
	virtual ~FWeldEdgeSequence() {}


	/**
	* Alters the existing mesh by welding two edge sequences, preserving sequence A.
	* Conditions the mesh by splitting edges and optionally deleting triangles.
	*
	* @return EWeldResult::OK on success
	*/
	EWeldResult Weld();

protected:
	/**
	* Verifies validity of input edges by ensuring they are
	* correctly-oriented boundary edges
	*
	* @return EWeldResult::OK on success
	*/
	EWeldResult CheckInput();

	/**
	* Splits largest edges of the span with fewest vertices so that
	* both input spans have an equal number of vertices and edges after 
	* 
	* @return EWeldResult::OK on success
	*/
	EWeldResult SplitSmallerSpan();

	/**
	* Checks for edges between terminating vertices of the input spans and attempts to delete them if possible
	* by deleting the adjacent triangle. These cases would prevent MergeEdges() from succeeding because it won't
	* implicitly collapse/delete triangles which connect the two edges being merged.
	* 
	* @return EWeldResult::OK on success
	*/
	EWeldResult CheckForAndCollapseSideTriangles();

	/**
	* Welds edge sequence together
	*
	* @return EWeldResult::OK on success
	*/
	EWeldResult WeldEdgeSequence();
};

} // end namespace UE::Geometry
} // end namespace UE
