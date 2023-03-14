// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/MeshBevel.h"

#include "GroupTopology.h"
#include "EdgeLoop.h"
#include "DynamicMesh/DynamicMeshChangeTracker.h"
#include "MeshWeights.h"
#include "CompGeom/PolygonTriangulation.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "MeshRegionBoundaryLoops.h"
#include "Operations/PolyEditingEdgeUtil.h"
#include "Operations/PolyEditingUVUtil.h"
#include "Algo/Count.h"
#include "Distance/DistLine3Line3.h"

using namespace UE::Geometry;

// Uncomment to enable various checks and ensures in the Bevel code.
// We cannot default-enable these checks even in Debug builds, because 
// Bevel still frequently hits some of these cases, and those checks/ensures 
// make it very difficult to debug a game using Bevel in Geometry Script (eg Lyra sample)
//#define ENABLE_MESH_BEVEL_DEBUG

#ifdef ENABLE_MESH_BEVEL_DEBUG
#define MESH_BEVEL_DEBUG_CHECK(Expr) checkSlow(Expr)
#define MESH_BEVEL_DEBUG_ENSURE(Expr) ensure(Expr)
#else
#define MESH_BEVEL_DEBUG_CHECK(Expr) 
#define MESH_BEVEL_DEBUG_ENSURE(Expr) (Expr)
#endif


namespace UELocal
{
	static void QuadsToTris(const FDynamicMesh3& Mesh, const TArray<FIndex2i>& Quads, TArray<int32>& TrisOut, bool bReset)
	{
		int32 N = Quads.Num();
		if (bReset)
		{
			TrisOut.Reset();
			TrisOut.Reserve(2 * N);
		}
		for (const FIndex2i& Quad : Quads)
		{
			if (Mesh.IsTriangle(Quad.A))
			{
				TrisOut.Add(Quad.A);
			}
			if (Mesh.IsTriangle(Quad.B))
			{
				TrisOut.Add(Quad.B);
			}
		}
	};

}




void FMeshBevel::InitializeFromGroupTopology(const FDynamicMesh3& Mesh, const FGroupTopology& Topology)
{
	ResultInfo = FGeometryResult(EGeometryResultType::InProgress);

	// set up initial problem inputs
	for (int32 TopoEdgeID = 0; TopoEdgeID < Topology.Edges.Num(); ++TopoEdgeID)
	{
		if (Topology.IsIsolatedLoop(TopoEdgeID))
		{
			FEdgeLoop NewLoop;
			NewLoop.InitializeFromEdges(&Mesh, Topology.Edges[TopoEdgeID].Span.Edges);
			AddBevelEdgeLoop(Mesh, NewLoop);
		}
		else
		{
			AddBevelGroupEdge(Mesh, Topology, TopoEdgeID);
		}

		if (ResultInfo.CheckAndSetCancelled(Progress))
		{
			return;
		}
	}

	// precompute topological information necessary to apply bevel to vertices/edges/loops
	BuildVertexSets(Mesh);
}


void FMeshBevel::InitializeFromGroupTopologyEdges(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, const TArray<int32>& GroupEdges)
{
	ResultInfo = FGeometryResult(EGeometryResultType::InProgress);

	// set up initial problem inputs
	for (int32 TopoEdgeID : GroupEdges)
	{
		if (Topology.IsIsolatedLoop(TopoEdgeID))
		{
			FEdgeLoop NewLoop;
			NewLoop.InitializeFromEdges(&Mesh, Topology.Edges[TopoEdgeID].Span.Edges);
			AddBevelEdgeLoop(Mesh, NewLoop);
		}
		else
		{
			AddBevelGroupEdge(Mesh, Topology, TopoEdgeID);
		}

		if (ResultInfo.CheckAndSetCancelled(Progress))
		{
			return;
		}
	}

	// precompute topological information necessary to apply bevel to vertices/edges/loops
	BuildVertexSets(Mesh);
}


bool FMeshBevel::InitializeFromGroupTopologyFaces(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, const TArray<int32>& GroupFaces)
{
	FGroupTopologySelection Selection;
	Selection.SelectedGroupIDs.Append(GroupFaces);
	TArray<int32> Triangles;
	Topology.GetSelectedTriangles(Selection, Triangles);
	return InitializeFromTriangleSet(Mesh, Triangles);
}

bool FMeshBevel::InitializeFromTriangleSet(const FDynamicMesh3& Mesh, const TArray<int32>& Triangles)
{
	ResultInfo = FGeometryResult(EGeometryResultType::InProgress);

	FMeshRegionBoundaryLoops RegionLoops(&Mesh, Triangles, true);
	if (RegionLoops.bFailed)
	{
		ResultInfo.SetFailed();
		return false;
	}

	// cannot bevel a selection-bowtie vertex, so we have to check for those and fail here
	TSet<int32> AllVertices;
	for (const FEdgeLoop& Loop : RegionLoops.Loops)
	{
		for (int32 vid : Loop.Vertices)
		{
			if (AllVertices.Contains(vid))
			{
				return false;
			}
			AllVertices.Add(vid);
		}
	}

	for (const FEdgeLoop& Loop : RegionLoops.Loops)
	{
		AddBevelEdgeLoop(Mesh, Loop);
	}

	// precompute topological information necessary to apply bevel to vertices/edges/loops
	BuildVertexSets(Mesh);

	return true;
}



bool FMeshBevel::Apply(FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker)
{
	// disconnect along bevel graph edges/vertices and save necessary info
	UnlinkEdges(Mesh, ChangeTracker);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}
	UnlinkLoops(Mesh, ChangeTracker);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}
	UnlinkVertices(Mesh, ChangeTracker);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}
	FixUpUnlinkedBevelEdges(Mesh);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	// update vertex positions
	DisplaceVertices(Mesh, InsetDistance);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	// mesh the bevel corners and edges
	CreateBevelMeshing(Mesh);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	// build output list of triangles so it can be re-used in operations below if useful
	NewTriangles.Reset();
	for (FBevelVertex& Vertex : Vertices)
	{
		NewTriangles.Append(Vertex.NewTriangles);
	}
	for (FBevelEdge& Edge : Edges)
	{
		UELocal::QuadsToTris(Mesh, Edge.StripQuads, NewTriangles, false);
	}
	for (FBevelLoop& Loop : Loops)
	{
		UELocal::QuadsToTris(Mesh, Loop.StripQuads, NewTriangles, false);
	}


	// compute normals
	ComputeNormals(Mesh);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	// compute UVs
	ComputeUVs(Mesh);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	ComputeMaterialIDs(Mesh);
	if (ResultInfo.CheckAndSetCancelled(Progress))
	{
		return false;
	}

	// todo: compute other attribs

	ResultInfo.SetSuccess(true, Progress);
	return true;
}



FMeshBevel::FBevelVertex* FMeshBevel::GetBevelVertexFromVertexID(int32 VertexID)
{
	int32* FoundIndex = VertexIDToIndexMap.Find(VertexID);
	if (FoundIndex == nullptr)
	{
		return nullptr;
	}
	return &Vertices[*FoundIndex];
}



void FMeshBevel::AddBevelGroupEdge(const FDynamicMesh3& Mesh, const FGroupTopology& Topology, int32 GroupEdgeID)
{
	const TArray<int32>& MeshEdgeList = Topology.Edges[GroupEdgeID].Span.Edges;

	// currently cannot handle boundary edges
	if ( Algo::CountIf(MeshEdgeList, [&Mesh](int EdgeID) { return Mesh.IsBoundaryEdge(EdgeID); }) > 0 )
	{
		return;
	}

	FIndex2i EdgeCornerIDs = Topology.Edges[GroupEdgeID].EndpointCorners;

	FBevelEdge Edge;

	for (int32 ci = 0; ci < 2; ++ci)
	{
		int32 CornerID = EdgeCornerIDs[ci];
		FGroupTopology::FCorner Corner = Topology.Corners[CornerID];
		int32 VertexID = Corner.VertexID;
		Edge.bEndpointBoundaryFlag[ci] = Mesh.IsBoundaryVertex(VertexID);
		int32 IncomingEdgeID = (ci == 0) ? MeshEdgeList[0] : MeshEdgeList.Last();

		FBevelVertex* VertInfo = GetBevelVertexFromVertexID(VertexID);
		if (VertInfo == nullptr)
		{
			FBevelVertex NewVertex;
			NewVertex.CornerID = CornerID;
			NewVertex.VertexID = VertexID;
			int32 NewIndex = Vertices.Num();
			Vertices.Add(NewVertex);
			VertexIDToIndexMap.Add(VertexID, NewIndex);
			VertInfo = &Vertices[NewIndex];
		}
		VertInfo->IncomingBevelMeshEdges.Add(IncomingEdgeID);
		VertInfo->IncomingBevelTopoEdges.Add(GroupEdgeID);
	}

	Edge.MeshEdges.Append(MeshEdgeList);
	Edge.MeshVertices.Append(Topology.Edges[GroupEdgeID].Span.Vertices);
	Edge.GroupEdgeID = GroupEdgeID;
	Edge.GroupIDs = Topology.Edges[GroupEdgeID].Groups;

	Edge.MeshEdgeTris.Reserve(Edge.MeshEdges.Num());
	for (int32 eid : Edge.MeshEdges)
	{
		Edge.MeshEdgeTris.Add(Mesh.GetEdgeT(eid));
	}

	Edges.Add(MoveTemp(Edge));
}





void FMeshBevel::AddBevelEdgeLoop(const FDynamicMesh3& Mesh, const FEdgeLoop& MeshEdgeLoop)
{
	// currently cannot handle boundary edges
	if ( Algo::CountIf(MeshEdgeLoop.Edges, [&Mesh](int EdgeID) { return Mesh.IsBoundaryEdge(EdgeID); }) > 0 )
	{
		return;
	}

	FBevelLoop Loop;
	Loop.MeshEdges = MeshEdgeLoop.Edges;
	Loop.MeshVertices = MeshEdgeLoop.Vertices;

	Loop.MeshEdgeTris.Reserve(Loop.MeshEdges.Num());
	for (int32 eid : Loop.MeshEdges)
	{
		Loop.MeshEdgeTris.Add(Mesh.GetEdgeT(eid));
	}

	Loops.Add(Loop);
}




void FMeshBevel::BuildVertexSets(const FDynamicMesh3& Mesh)
{
	// can be parallel
	for (FBevelVertex& Vertex : Vertices)
	{
		// get sorted list of triangles around the vertex
		TArray<int> GroupLengths;
		TArray<bool> bGroupIsLoop;
		EMeshResult Result = Mesh.GetVtxContiguousTriangles(Vertex.VertexID, Vertex.SortedTriangles, GroupLengths, bGroupIsLoop);
		if ( Result != EMeshResult::Ok || GroupLengths.Num() != 1 || Vertex.SortedTriangles.Num() < 2)
		{
			Vertex.VertexType = EBevelVertexType::Unknown;
			continue;
		}

		// GetVtxContiguousTriangles does not return triangles sorted in a consistent direction. This check will
		// reverse the ordering such that it is consistently walking counter-clockwise around the vertex (I think...)
		FIndex3i Tri0 = Mesh.GetTriangle(Vertex.SortedTriangles[0]).GetCycled(Vertex.VertexID);
		FIndex3i Tri1 = Mesh.GetTriangle(Vertex.SortedTriangles[1]).GetCycled(Vertex.VertexID);
		if (Tri0.C == Tri1.B)
		{
			Algo::Reverse(Vertex.SortedTriangles);
		}

		if (Mesh.IsBoundaryVertex(Vertex.VertexID))
		{
			Vertex.VertexType = EBevelVertexType::BoundaryVertex;
			continue;
		}


		MESH_BEVEL_DEBUG_CHECK(Vertex.IncomingBevelMeshEdges.Num() != 0);		// shouldn't ever happen
		if (Vertex.IncomingBevelMeshEdges.Num() == 1)
		{
			BuildTerminatorVertex(Vertex, Mesh);
		}
		else
		{
			BuildJunctionVertex(Vertex, Mesh);
		}

		if (ResultInfo.CheckAndSetCancelled(Progress))
		{
			return;
		}
	}

	// At a 'Terminator' vertex we are going to split the one-ring and fill the bevel-side with a quad, which
	// will usually leave a single-triangle hole. However if two Terminator vertices are directly connected
	// via a non-bevel mesh edge, the two triangles will be connected, ie the hole is quad-shaped. It's easier
	// to detect this case here /before/ we split the mesh up into disconnected parts...
	for (FBevelVertex& Vertex : Vertices)
	{
		if (Vertex.VertexType == EBevelVertexType::TerminatorVertex)
		{
			int32 OtherVertexID = Vertex.TerminatorInfo.B;
			int32* OtherBevelVtxIdx = VertexIDToIndexMap.Find(OtherVertexID);
			if (OtherBevelVtxIdx != nullptr)
			{
				// does other vertex have to be a terminator? or can this also happen w/ a junction?
				FBevelVertex& OtherVertex = Vertices[*OtherBevelVtxIdx];
				if (OtherVertex.VertexType == EBevelVertexType::TerminatorVertex)
				{
					// want to skip this if the ring-split edge is already a bevel edge
					int32 MeshEdgeID = Mesh.FindEdge(Vertex.VertexID, OtherVertex.VertexID);
					MESH_BEVEL_DEBUG_CHECK(MeshEdgeID >= 0);
					if (Mesh.IsEdge(MeshEdgeID) && 
						Vertex.TerminatorInfo.A == MeshEdgeID &&
						OtherVertex.TerminatorInfo.A == MeshEdgeID &&		// do we need the other vertex to use the same edge here?  (is this actually a hard constraint on that edge that we should be enforcing??)
						Vertex.IncomingBevelMeshEdges.Contains(MeshEdgeID) == false )
					{
						Vertex.ConnectedBevelVertex = *OtherBevelVtxIdx;
					}
				}

			}
		}
	}

}



void FMeshBevel::BuildJunctionVertex(FBevelVertex& Vertex, const FDynamicMesh3& Mesh)
{
	//
	// Now split up the single contiguous one-ring into "Wedges" created by the incoming split-edges
	//

	// find first split edge, and the triangle/index "after" that first split edge
	int32 NT = Vertex.SortedTriangles.Num();
	int32 StartTriIndex = -1;
	for (int32 k = 0; k < NT; ++k)
	{
		if (FindSharedEdgeInTriangles(Mesh, Vertex.SortedTriangles[k], Vertex.SortedTriangles[(k + 1) % NT]) == Vertex.IncomingBevelMeshEdges[0])
		{
			StartTriIndex = (k + 1) % NT;		// start at second tri, so that bevel-edge is first edge in wedge
			break;
		}
	}
	if (StartTriIndex == -1)
	{
		Vertex.VertexType = EBevelVertexType::Unknown;
		return;
	}

	// now walk around the one-ring tris, accumulating into current Wedge until we hit another split-edge,
	// at which point a new Wedge is spawned
	int32 CurTriIndex = StartTriIndex;
	FOneRingWedge CurWedge;
	CurWedge.WedgeVertex = Vertex.VertexID;
	CurWedge.Triangles.Add(Vertex.SortedTriangles[CurTriIndex]);
	CurWedge.BorderEdges.A = Vertex.IncomingBevelMeshEdges[0];
	for (int32 k = 0; k < NT; ++k)
	{
		int32 CurTri = Vertex.SortedTriangles[CurTriIndex % NT];
		int32 NextTri = Vertex.SortedTriangles[(CurTriIndex + 1) % NT];
		int32 SharedEdge = FindSharedEdgeInTriangles(Mesh, CurTri, NextTri);
		MESH_BEVEL_DEBUG_CHECK(SharedEdge != -1);
		if (Vertex.IncomingBevelMeshEdges.Contains(SharedEdge))
		{
			// if we found a bevel-edge, close the current wedge and start a new one
			CurWedge.BorderEdges.B = SharedEdge;
			Vertex.Wedges.Add(CurWedge);
			CurWedge = FOneRingWedge();
			CurWedge.WedgeVertex = Vertex.VertexID;
			CurWedge.BorderEdges.A = SharedEdge;
		}
		CurWedge.Triangles.Add(NextTri);
		CurTriIndex++;
	}
	// ?? is there a chance that we have a final open wedge here? we iterate one extra time so it shouldn't happen (or could we get an extra wedge then??)

	for (FOneRingWedge& Wedge : Vertex.Wedges)
	{
		Wedge.BorderEdgeTriEdgeIndices.A = Mesh.GetTriEdges(Wedge.Triangles[0]).IndexOf(Wedge.BorderEdges.A);
		Wedge.BorderEdgeTriEdgeIndices.B = Mesh.GetTriEdges(Wedge.Triangles.Last()).IndexOf(Wedge.BorderEdges.B);
	}

	if (Vertex.Wedges.Num() > 1)
	{
		Vertex.VertexType = EBevelVertexType::JunctionVertex;
	}
	else
	{
		Vertex.VertexType = EBevelVertexType::Unknown;
	}
}


void FMeshBevel::BuildTerminatorVertex(FBevelVertex& Vertex, const FDynamicMesh3& Mesh)
{
	Vertex.VertexType = EBevelVertexType::Unknown;
	if (MESH_BEVEL_DEBUG_ENSURE(Vertex.IncomingBevelMeshEdges.Num() == 1) == false)
	{
		return;
	}

	int32 IncomingEdgeID = Vertex.IncomingBevelMeshEdges[0];
	int32 NumTris = Vertex.SortedTriangles.Num();

	// We have one edge coming into the vertex one ring, our main problem is to pick a second
	// edge to split the one-ring with. There is no obvious right answer in many cases. 

	// "other" split edge that we pick
	int32 RingSplitEdgeID = -1;

	// NOTE: code below assumes we have polygroups on the mesh. In theory we could also support not having polygroups,
	// by (eg) picking an arbitrary edge "furthest" from IncomingEdgeID in the exponential map

	// Find the ordered set of triangles that are not in either of the groups connected to IncomingEdgeID. Eg imagine
	// a cube corner, if we want to bevel IncomingEdgeID along one of the cube edges, we want to add the new edge
	// in the "furthest" face (perpendicular to the edge)
	FIndex2i IncomingEdgeT = Mesh.GetEdgeT(IncomingEdgeID);
	FIndex2i IncomingEdgeGroups(Mesh.GetTriangleGroup(IncomingEdgeT.A), Mesh.GetTriangleGroup(IncomingEdgeT.B));

	// Find the first index of Vertex.SortedTriangles that we want to remove.
	// This should not be able to fail but if it does, we will just start at 0 
	// and maybe end up with some bevel failures below
	int32 NumTriangles = Vertex.SortedTriangles.Num();
	int32 StartIndex = 0;
	for (int32 k = 0; k < NumTriangles; ++k)
	{
		int32 gid = Mesh.GetTriangleGroup(Vertex.SortedTriangles[k]);
		if (IncomingEdgeGroups.Contains(gid))
		{
			StartIndex = k;
			break;
		}
	}

	TArray<int32> OtherGroupTris;	// sorted wedge of triangles that are not in either of the groups connected to incoming edge
	TArray<int32> OtherGroups;		// list of group IDs encountered, in-order
	for ( int32 k = 0; k < NumTriangles; ++k )
	{
		// Vertex.SortedTriangles was ordered, ie sequential triangles were connected, but if we filter
		// out some triangles it may no longer be sequential in OtherGroupTris, leading to badness. 
		// But the two groups we are removing should be contiguous, so if we start there, then the
		// remaining tris should be contiguous. StartIndex found above should give us that triangle.
		int32 ShiftedIndex = (StartIndex + k) % NumTriangles;
		int32 tid = Vertex.SortedTriangles[ShiftedIndex];
		int32 gid = Mesh.GetTriangleGroup(tid);
		if (IncomingEdgeGroups.Contains(gid) == false)
		{
			OtherGroupTris.Add(tid);
			OtherGroups.AddUnique(gid);
		}
	}
	int32 NumRemainingTris = OtherGroupTris.Num();

	// Determine which edge to split at in the "other" group triangles. If we only have one group
	// then we can try to pick the "middlest" edge. The worst case is when there is only one triangle,
	// then we are picking a not-very-good edge no matter what (potentially we should do an edge split or
	// face poke in that situation). If we have multiple groups then we probably want to pick one
	// of the group-boundary edges inside the triangle-span, ideally the "middlest" but currently we
	// are just picking one arbitrarily...
	if (OtherGroups.Num() == 1)
	{
		Vertex.NewGroupID = OtherGroups[0];
		if (OtherGroupTris.Num() == 1)
		{
			FIndex3i TriEdges = Mesh.GetTriEdges(OtherGroupTris[0]);
			for (int32 j = 0; j < 3; ++j)
			{
				if (Mesh.GetEdgeV(TriEdges[j]).Contains(Vertex.VertexID))
				{
					RingSplitEdgeID = TriEdges[j];
					break;
				}
			}
		}
		else if (OtherGroupTris.Num() == 2)
		{
			RingSplitEdgeID = FindSharedEdgeInTriangles(Mesh, OtherGroupTris[0], OtherGroupTris[1]);
		}
		else
		{
			// try using the 'middlest' triangle as the 'middlest' edge
			// TODO: should compute opening angles here and pick the edge closest to the middle of the angular span!!
			int32 j = OtherGroupTris.Num() / 2;
			RingSplitEdgeID = FindSharedEdgeInTriangles(Mesh, OtherGroupTris[j], OtherGroupTris[j+1]);

			// If the OtherGroupTris list ended up being not contiguous (see above for how that can happen), then
			// it's possible that (j) and (j+1) are not connected and the share-edge search will fail. In that case
			// we will just linear-search for two connected triangles. If this fails then this vertex will not be bevelled.
			if (RingSplitEdgeID == -1)
			{
				for (int32 k = 0; k < NumRemainingTris; ++k)
				{
					RingSplitEdgeID = FindSharedEdgeInTriangles(Mesh, OtherGroupTris[k], OtherGroupTris[(k+1)%NumRemainingTris]);
					if (RingSplitEdgeID != -1)
					{
						break;
					}
				}
			}
		}
	}
	else
	{
		// Search for two adjacent triangles in different groups that have a shared edge. This search
		// may need to wrap around if we got unlucky in the triangle ordering in OtherGroupTris
		for (int32 k = 0; k < OtherGroupTris.Num(); ++k)
		{
			int32 TriangleA = OtherGroupTris[k], TriangleB = OtherGroupTris[(k+1)%NumRemainingTris];
			if (Mesh.GetTriangleGroup(TriangleA) != Mesh.GetTriangleGroup(TriangleB))
			{
				RingSplitEdgeID = FindSharedEdgeInTriangles(Mesh, TriangleA, TriangleB);
				Vertex.NewGroupID = -1;		// allocate a new group for this triangle, this is usually what one would want
				break;
			}
		}
	}

	if (RingSplitEdgeID == -1)
	{
		return;
	}

	// save edgeid/vertexid for the 'terminator edge' that we will disconnect at
	FIndex2i SplitEdgeV = Mesh.GetEdgeV(RingSplitEdgeID);
	Vertex.TerminatorInfo = FIndex2i(RingSplitEdgeID, SplitEdgeV.OtherElement(Vertex.VertexID));

	// split the terminator vertex into two wedges
	TArray<int32> SplitTriSets[2];
	if (SplitInteriorVertexTrianglesIntoSubsets(&Mesh, Vertex.VertexID, IncomingEdgeID, RingSplitEdgeID, SplitTriSets[0], SplitTriSets[1]) == false)
	{
		return;
	}

	// make the wedge list
	Vertex.Wedges.SetNum(2);
	Vertex.Wedges[0].WedgeVertex = Vertex.VertexID;
	Vertex.Wedges[0].Triangles.Append(SplitTriSets[0]);
	Vertex.Wedges[1].WedgeVertex = Vertex.VertexID;
	Vertex.Wedges[1].Triangles.Append(SplitTriSets[1]);

	// We need to know the two border edges of each wedge, because we will use this information
	// in later stages. Note that this block is not specific to the TerminatorVertex case
	for (FOneRingWedge& Wedge : Vertex.Wedges)
	{
		int32 NumWedgeTris = Wedge.Triangles.Num();

		FIndex2i VtxEdges0 = IndexUtil::FindVertexEdgesInTriangle(Mesh, Wedge.Triangles[0], Vertex.VertexID);
		if (NumWedgeTris == 1)
		{
			// If the wedge only has one tri, both edges are the border edges. 
			// Note that these are not sorted, ie B might not be the edge shared with the next Wedge. 
			// Currently this does not matter but it might in the future?
			Wedge.BorderEdges.A = VtxEdges0.A;
			Wedge.BorderEdges.B = VtxEdges0.B;
		}
		else
		{
			// the wedge-border-edge is *not* the edge connected to the next triangle in the wedge-triangle-list
			Wedge.BorderEdges.A = ( Mesh.GetEdgeT(VtxEdges0.A).Contains(Wedge.Triangles[1]) ) ? VtxEdges0.B : VtxEdges0.A;
			// final wedge-border-edge is the same case, with the last and second-last tris
			FIndex2i VtxEdges1 = IndexUtil::FindVertexEdgesInTriangle(Mesh, Wedge.Triangles[NumWedgeTris-1], Vertex.VertexID);
			Wedge.BorderEdges.B = ( Mesh.GetEdgeT(VtxEdges1.A).Contains(Wedge.Triangles[NumWedgeTris-2]) ) ? VtxEdges1.B : VtxEdges1.A;
		}
		// save the index of the border edge in it's triangle, so that when we disconnect the wedges later,
		// we can find the new edge ID
		Wedge.BorderEdgeTriEdgeIndices.A = Mesh.GetTriEdges(Wedge.Triangles[0]).IndexOf(Wedge.BorderEdges.A);
		Wedge.BorderEdgeTriEdgeIndices.B = Mesh.GetTriEdges(Wedge.Triangles.Last()).IndexOf(Wedge.BorderEdges.B);
	}

	Vertex.VertexType = EBevelVertexType::TerminatorVertex;
}


void FMeshBevel::UnlinkEdges(FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker)
{
	for (FBevelEdge& Edge : Edges)
	{
		UnlinkBevelEdgeInterior(Mesh, Edge, ChangeTracker);
	}
}



namespace UELocal
{
	// decomposition of a vertex one-ring into two connected triangle subsets
	struct FVertexSplit
	{
		int32 VertexID;
		bool bOK;
		TArray<int32> TriSets[2];
	};

	// walk along a sequence of vertex-splits and make sure that the split triangle sets
	// maintain consistent "sides" (see call in UnlinkBevelEdgeInterior for more details)
	static void ReconcileTriangleSets(TArray<FVertexSplit>& SplitSequence)
	{
		int32 N = SplitSequence.Num();
		TArray<int32> PrevTriSet0;
		for (int32 k = 0; k < N; ++k)
		{
			if (PrevTriSet0.Num() == 0 && SplitSequence[k].TriSets[0].Num() > 0)
			{
				PrevTriSet0 = SplitSequence[k].TriSets[0];
			}
			else
			{
				bool bFoundInSet0 = false;
				for (int32 tid : SplitSequence[k].TriSets[0])
				{
					if (PrevTriSet0.Contains(tid))
					{
						bFoundInSet0 = true;
						break;
					}
				}
				if (!bFoundInSet0)
				{
					Swap(SplitSequence[k].TriSets[0], SplitSequence[k].TriSets[1]);
				}
				PrevTriSet0 = SplitSequence[k].TriSets[0];
			}
		}
	}

};


void FMeshBevel::UnlinkBevelEdgeInterior(
	FDynamicMesh3& Mesh,
	FBevelEdge& BevelEdge,
	FDynamicMeshChangeTracker* ChangeTracker)
{
	// figure out what sets of triangles to split each vertex into
	int32 N = BevelEdge.MeshVertices.Num();

	TArray<UELocal::FVertexSplit> SplitsToProcess;
	SplitsToProcess.SetNum(N);

	// precompute triangle sets for each vertex we want to split, by "cutting" the one ring into two halves based
	// on edges - 2 edges for interior vertices, and 1 edge for a boundary vertex at the start/end of the edge-span

	SplitsToProcess[0] = UELocal::FVertexSplit{ BevelEdge.MeshVertices[0], false };
	if (BevelEdge.bEndpointBoundaryFlag[0])
	{
		SplitsToProcess[0].bOK = SplitBoundaryVertexTrianglesIntoSubsets(&Mesh, SplitsToProcess[0].VertexID, BevelEdge.MeshEdges[0], 
			SplitsToProcess[0].TriSets[0], SplitsToProcess[0].TriSets[1]);
	}
	for (int32 k = 1; k < N - 1; ++k)
	{
		SplitsToProcess[k].VertexID = BevelEdge.MeshVertices[k];
		if (Mesh.IsBoundaryVertex(SplitsToProcess[k].VertexID))
		{
			SplitsToProcess[k].bOK = false;
		}
		else
		{
			SplitsToProcess[k].bOK = SplitInteriorVertexTrianglesIntoSubsets(&Mesh, SplitsToProcess[k].VertexID,
				BevelEdge.MeshEdges[k-1], BevelEdge.MeshEdges[k], SplitsToProcess[k].TriSets[0], SplitsToProcess[k].TriSets[1]);
		}
	}
	SplitsToProcess[N-1] = UELocal::FVertexSplit{ BevelEdge.MeshVertices[N - 1], false };
	if (BevelEdge.bEndpointBoundaryFlag[1])
	{
		SplitsToProcess[N-1].bOK = SplitBoundaryVertexTrianglesIntoSubsets(&Mesh, SplitsToProcess[N-1].VertexID, BevelEdge.MeshEdges[N-2], 
			SplitsToProcess[N-1].TriSets[0], SplitsToProcess[N-1].TriSets[1]);
	}

	// SplitInteriorVertexTrianglesIntoSubsets does not consistently order its output sets, ie, if you imagine [Edge0,Edge1] as a path
	// cutting through the one ring, the "side" that Set0 and Set1 end up is arbitrary, and depends on the ordering of edges in the triangles of Edge1.
	// This might ideally be fixed in the future, but for the time being, all we need is consistency. So we walk from the start of the 
	// edge to the end, checking for overlap between each tri-one-ring-wedge. If Split[k].TriSet0 does not overlap with Split[k-1].TriSet0, then
	// we want to swap TriSet0 and TriSet1 at Split[k].
	UELocal::ReconcileTriangleSets(SplitsToProcess);

	// apply vertex splits and accumulate new list
	N = SplitsToProcess.Num();
	for (int32 k = 0; k < N; ++k)
	{
		const UELocal::FVertexSplit& Split = SplitsToProcess[k];
		if (ChangeTracker)
		{
			ChangeTracker->SaveVertexOneRingTriangles(Split.VertexID, true);
		}

		bool bDone = false;
		if (Split.bOK)
		{
			FDynamicMesh3::FVertexSplitInfo SplitInfo;
			EMeshResult Result = Mesh.SplitVertex(Split.VertexID, Split.TriSets[0], SplitInfo);
			if (MESH_BEVEL_DEBUG_ENSURE(Result == EMeshResult::Ok))
			{
				BevelEdge.NewMeshVertices.Add(SplitInfo.NewVertex);
				bDone = true;
			}
		}
		if (!bDone)
		{
			BevelEdge.NewMeshVertices.Add(Split.VertexID);
		}
	}

	// now build edge correspondences
	N = BevelEdge.MeshVertices.Num();
	MESH_BEVEL_DEBUG_CHECK(N == BevelEdge.NewMeshVertices.Num());
	for (int32 k = 0; k < N-1; ++k)
	{
		int32 Edge0 = BevelEdge.MeshEdges[k];
		int32 Edge1 = Mesh.FindEdge(BevelEdge.NewMeshVertices[k], BevelEdge.NewMeshVertices[k + 1]);
		BevelEdge.NewMeshEdges.Add(Edge1);
		MESH_BEVEL_DEBUG_CHECK(Edge1 >= 0);
		if ( Mesh.IsEdge(Edge1) && Edge0 != Edge1 && MeshEdgePairs.Contains(Edge0) == false )
		{
			MeshEdgePairs.Add(Edge0, Edge1);
			MeshEdgePairs.Add(Edge1, Edge0);
		}
	}
}




void FMeshBevel::UnlinkBevelLoop(FDynamicMesh3& Mesh, FBevelLoop& BevelLoop, FDynamicMeshChangeTracker* ChangeTracker)
{
	int32 N = BevelLoop.MeshVertices.Num();

	TArray<UELocal::FVertexSplit> SplitsToProcess;
	SplitsToProcess.SetNum(N);

	// precompute triangle sets for each vertex we want to split
	for (int32 k = 0; k < N; ++k)
	{
		SplitsToProcess[k].VertexID = BevelLoop.MeshVertices[k];
		if (Mesh.IsBoundaryVertex(SplitsToProcess[k].VertexID))
		{
			// cannot split boundary vertex
			SplitsToProcess[k].bOK = false;
		}
		else
		{
			int32 PrevEdge = (k == 0) ? BevelLoop.MeshEdges.Last() : BevelLoop.MeshEdges[k-1];
			int32 CurEdge = BevelLoop.MeshEdges[k];
			SplitsToProcess[k].bOK = SplitInteriorVertexTrianglesIntoSubsets(&Mesh, SplitsToProcess[k].VertexID,
				PrevEdge, CurEdge, SplitsToProcess[k].TriSets[0], SplitsToProcess[k].TriSets[1]);
		}
	}

	// fix up triangle sets - see call in UnlinkBevelEdgeInterior() for more info
	UELocal::ReconcileTriangleSets(SplitsToProcess);

	// apply vertex splits and accumulate new list
	N = SplitsToProcess.Num();
	for (int32 k = 0; k < N; ++k)
	{
		const UELocal::FVertexSplit& Split = SplitsToProcess[k];
		if (ChangeTracker)
		{
			ChangeTracker->SaveVertexOneRingTriangles(Split.VertexID, true);
		}

		bool bDone = false;
		if (Split.bOK)
		{
			FDynamicMesh3::FVertexSplitInfo SplitInfo;
			EMeshResult Result = Mesh.SplitVertex(Split.VertexID, Split.TriSets[1], SplitInfo);
			if (MESH_BEVEL_DEBUG_ENSURE(Result == EMeshResult::Ok))
			{
				BevelLoop.NewMeshVertices.Add(SplitInfo.NewVertex);
				bDone = true;
			}
		}
		if (!bDone)
		{
			BevelLoop.NewMeshVertices.Add(Split.VertexID);		// failed to split, so we have a shared vertex on both "sides"
		}
	}

	// now build edge correspondences
	N = BevelLoop.MeshVertices.Num();
	MESH_BEVEL_DEBUG_CHECK(N == BevelLoop.NewMeshVertices.Num());
	for (int32 k = 0; k < N; ++k)
	{
		int32 Edge0 = BevelLoop.MeshEdges[k];
		int32 Edge1 = Mesh.FindEdge(BevelLoop.NewMeshVertices[k], BevelLoop.NewMeshVertices[(k + 1)%N]);
		BevelLoop.NewMeshEdges.Add(Edge1);
		MESH_BEVEL_DEBUG_CHECK(Edge1 >= 0);
		if (Mesh.IsEdge(Edge1) && Edge0 != Edge1 && MeshEdgePairs.Contains(Edge0) == false )
		{
			MeshEdgePairs.Add(Edge0, Edge1);
			MeshEdgePairs.Add(Edge1, Edge0);
		}
	}
}

void FMeshBevel::UnlinkLoops(FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker)
{
	for (FBevelLoop& Loop : Loops)
	{
		UnlinkBevelLoop(Mesh, Loop, ChangeTracker);
	}
}




void FMeshBevel::UnlinkVertices(FDynamicMesh3& Mesh, FDynamicMeshChangeTracker* ChangeTracker)
{
	// TODO: currently have to do terminator vertices first because we do some of the 
	// determination inside the unlink code...

	for (FBevelVertex& Vertex : Vertices)
	{
		if (Vertex.VertexType == EBevelVertexType::TerminatorVertex)
		{
			UnlinkTerminatorVertex(Mesh, Vertex, ChangeTracker);
		}
	}

	for (FBevelVertex& Vertex : Vertices)
	{
		if (Vertex.VertexType == EBevelVertexType::JunctionVertex)
		{
			UnlinkJunctionVertex(Mesh, Vertex, ChangeTracker);
		}
	}
}


void FMeshBevel::UnlinkJunctionVertex(FDynamicMesh3& Mesh, FBevelVertex& Vertex, FDynamicMeshChangeTracker* ChangeTracker)
{
	MESH_BEVEL_DEBUG_CHECK(Vertex.VertexType == EBevelVertexType::JunctionVertex);

	if (ChangeTracker)
	{
		ChangeTracker->SaveVertexOneRingTriangles(Vertex.VertexID, true);
	}

	int32 NumWedges = Vertex.Wedges.Num();
	MESH_BEVEL_DEBUG_CHECK(NumWedges > 1);

	// Split triangles around vertex into separate tri-sets based on wedges.
	// This will create a new vertex for each wedge.
	for (int32 k = 1; k < NumWedges; ++k)
	{
		FOneRingWedge& Wedge = Vertex.Wedges[k];

		FDynamicMesh3::FVertexSplitInfo SplitInfo;
		EMeshResult Result = Mesh.SplitVertex(Vertex.VertexID, Wedge.Triangles, SplitInfo);
		if (MESH_BEVEL_DEBUG_ENSURE(Result == EMeshResult::Ok))
		{
			Wedge.WedgeVertex = SplitInfo.NewVertex;
		}
	}

	// update end start/end pairs for each wedge. If we created new edges above, this is
	// the first time we will encounter them, so save in edge correspondence map
	for (int32 k = 0; k < NumWedges; ++k)
	{
		FOneRingWedge& Wedge = Vertex.Wedges[k];
		for (int32 j = 0; j < 2; ++j)
		{
			int32 OldWedgeEdgeID = Wedge.BorderEdges[j];
			int32 OldWedgeEdgeIndex = Wedge.BorderEdgeTriEdgeIndices[j];
			int32 TriangleID = (j == 0) ? Wedge.Triangles[0] : Wedge.Triangles.Last();
			int32 CurWedgeEdgeID = Mesh.GetTriEdges(TriangleID)[OldWedgeEdgeIndex];
			if (OldWedgeEdgeID != CurWedgeEdgeID)
			{
				if (MeshEdgePairs.Contains(OldWedgeEdgeID) == false)
				{
					MeshEdgePairs.Add(OldWedgeEdgeID, CurWedgeEdgeID);
					MeshEdgePairs.Add(CurWedgeEdgeID, OldWedgeEdgeID);
				}
				Wedge.BorderEdges[j] = CurWedgeEdgeID;
			}
		}
	}

}




void FMeshBevel::UnlinkTerminatorVertex(FDynamicMesh3& Mesh, FBevelVertex& BevelVertex, FDynamicMeshChangeTracker* ChangeTracker)
{
	MESH_BEVEL_DEBUG_CHECK(BevelVertex.VertexType == EBevelVertexType::TerminatorVertex);
	MESH_BEVEL_DEBUG_CHECK(BevelVertex.Wedges.Num() == 2);

	if (ChangeTracker)
	{
		ChangeTracker->SaveVertexOneRingTriangles(BevelVertex.VertexID, true);
	}

	// split the vertex
	FDynamicMesh3::FVertexSplitInfo SplitInfo;
	EMeshResult Result = Mesh.SplitVertex(BevelVertex.VertexID, BevelVertex.Wedges[1].Triangles, SplitInfo);
	if (MESH_BEVEL_DEBUG_ENSURE(Result == EMeshResult::Ok))
	{
		BevelVertex.Wedges[1].WedgeVertex = SplitInfo.NewVertex;

		// update end start/end pairs for each wedge, and save in the edge correspondence map.
		// Note that this is the same block as in UnlinkJunctionVertex
		int32 NumWedges = BevelVertex.Wedges.Num();
		for (int32 k = 0; k < NumWedges; ++k)
		{
			FOneRingWedge& Wedge = BevelVertex.Wedges[k];
			for (int32 j = 0; j < 2; ++j)
			{
				int32 OldWedgeEdgeID = Wedge.BorderEdges[j];
				int32 OldWedgeEdgeIndex = Wedge.BorderEdgeTriEdgeIndices[j];
				int32 TriangleID = (j == 0) ? Wedge.Triangles[0] : Wedge.Triangles.Last();
				int32 CurWedgeEdgeID = Mesh.GetTriEdges(TriangleID)[OldWedgeEdgeIndex];
				if (OldWedgeEdgeID != CurWedgeEdgeID)
				{
					if (MeshEdgePairs.Contains(OldWedgeEdgeID) == false)
					{
						MeshEdgePairs.Add(OldWedgeEdgeID, CurWedgeEdgeID);
						MeshEdgePairs.Add(CurWedgeEdgeID, OldWedgeEdgeID);
					}
					Wedge.BorderEdges[j] = CurWedgeEdgeID;
				}
			}
		}

	}


}


void FMeshBevel::FixUpUnlinkedBevelEdges(FDynamicMesh3& Mesh)
{
	// Rewrite vertex IDs in BevelEdge vertex lists to correctly match the vertices in the new unlinked wedges.
	// We did not know these new vertices in UnlinkBevelEdgeInterior() because we didn't unlink the vertices
	// into wedges until afterwards. 
	for (FBevelEdge& Edge : Edges)
	{
		// If this is a one-mesh-edge edge, then Edge.NewMeshEdges is incorrect because we could
		// not actually unlink the edge in UnlinkBevelEdgeInterior (since there were no interior vertices).
		// But now that we have unlinked the vertices, the other edge now exists, and we can find it here.
		if (Edge.MeshEdges.Num() == 1)
		{
			int32* FoundOtherEdge = MeshEdgePairs.Find(Edge.MeshEdges[0]);
			MESH_BEVEL_DEBUG_CHECK(FoundOtherEdge != nullptr);
			if (FoundOtherEdge != nullptr)
			{
				Edge.NewMeshEdges[0] = *FoundOtherEdge;
			}
			else
			{
				continue;		// something went wrong, loop below will break things
			}
		}

		// process start and end vertices of the path
		for (int32 j = 0; j < 2; ++j)
		{
			int32 vi = (j == 0) ? 0 : (Edge.MeshVertices.Num()-1);
			int32 ei = (j == 0) ? 0 : (Edge.MeshEdges.Num()-1);
			const FBevelVertex* BevelVertex = GetBevelVertexFromVertexID(Edge.MeshVertices[vi]);
			int32& V0 = Edge.MeshVertices[vi];
			int32& V1 = Edge.NewMeshVertices[vi];
			int32 E0 = Edge.MeshEdges[ei], E1 = Edge.NewMeshEdges[ei];
			bool bFoundV0 = false, bFoundV1 = false;
			for (const FOneRingWedge& Wedge : BevelVertex->Wedges)
			{
				for (int32 tid : Wedge.Triangles)
				{
					FIndex3i TriEdges = Mesh.GetTriEdges(tid);
					if (TriEdges.Contains(E0) && bFoundV0 == false)
					{
						MESH_BEVEL_DEBUG_CHECK( Mesh.GetEdgeV(E0).Contains(Wedge.WedgeVertex) );
						V0 = Wedge.WedgeVertex;
						bFoundV0 = true;
						break;
					}
					else if (TriEdges.Contains(E1) && bFoundV1 == false)
					{
						MESH_BEVEL_DEBUG_CHECK( Mesh.GetEdgeV(E1).Contains(Wedge.WedgeVertex) );
						V1 = Wedge.WedgeVertex;
						bFoundV1 = true;
						break;
					}
				}
			}
		}
	}
}



void FMeshBevel::DisplaceVertices(FDynamicMesh3& Mesh, double Distance)
{
	// fallback (very bad) technique to compute an inset vertex
	auto GetDisplacedVertexPos = [Distance](const FDynamicMesh3& Mesh, int32 VertexID) -> FVector3d
	{
		FVector3d CurPos = Mesh.GetVertex(VertexID);
		FVector3d Centroid = FMeshWeights::MeanValueCentroid(Mesh, VertexID);
		return CurPos + Distance * Normalized(Centroid - CurPos);
	};

	// Basically want to inset any beveled edges inwards into the existing poly-faces. 
	// To do this we will solve using our standard inset technique, eg similar to FInsetMeshRegion,
	// which involves computing 'inset lines' for each edge and then finding nearest-points between
	// pairs of lines (which will be the intersection point if the face is planar).

	// Need to keep track of the inset line sets for open paths because at the corner vertices we
	// will need to combine data from multiple path-line-sets (possibly could do this more efficiently 
	// as we only ever need the first and last...but small in context)
	struct FEdgePathInsetLines
	{
		TArray<FLine3d> InsetLines0;
		TArray<FLine3d> InsetLines1;
	};
	TArray<FEdgePathInsetLines> AllInsetLines;
	AllInsetLines.SetNum(Edges.Num());

	// solve open paths
	for ( int32 k = 0; k < Edges.Num(); ++k)
	{
		FBevelEdge& Edge = Edges[k];
		UE::Geometry::ComputeInsetLineSegmentsFromEdges(Mesh, Edge.MeshEdges, InsetDistance, AllInsetLines[k].InsetLines0);
		UE::Geometry::SolveInsetVertexPositionsFromInsetLines(Mesh, AllInsetLines[k].InsetLines0, Edge.MeshVertices, Edge.NewPositions0, false);

		UE::Geometry::ComputeInsetLineSegmentsFromEdges(Mesh, Edge.NewMeshEdges, InsetDistance, AllInsetLines[k].InsetLines1);
		UE::Geometry::SolveInsetVertexPositionsFromInsetLines(Mesh, AllInsetLines[k].InsetLines1, Edge.NewMeshVertices, Edge.NewPositions1, false);
	}

	// solve loops
	for (FBevelLoop& Loop : Loops)
	{
		TArray<FLine3d> InsetLines;
		UE::Geometry::ComputeInsetLineSegmentsFromEdges(Mesh, Loop.MeshEdges, InsetDistance, InsetLines);
		UE::Geometry::SolveInsetVertexPositionsFromInsetLines(Mesh, InsetLines, Loop.MeshVertices, Loop.NewPositions0, true);

		UE::Geometry::ComputeInsetLineSegmentsFromEdges(Mesh, Loop.NewMeshEdges, InsetDistance, InsetLines);
		UE::Geometry::SolveInsetVertexPositionsFromInsetLines(Mesh, InsetLines, Loop.NewMeshVertices, Loop.NewPositions1, true);
	}


	// Now solve corners. For corners, we want to find the 1 or 2 inset-lines corresponding
	// to the outgoing bevel-edges at each bevel-vertex-wedge. Unfortunately we do not have
	// a precomputed mapping for this so we currently linear-search over the full edge set for 
	// each wedge. Could do in parallel (eg make list of valid wedges first)
	for (FBevelVertex& Vertex : Vertices)
	{
		if ( (Vertex.VertexType == EBevelVertexType::JunctionVertex)
			|| (Vertex.VertexType == EBevelVertexType::TerminatorVertex) )
		{
			int32 NumWedges = Vertex.Wedges.Num();
			for (int32 k = 0; k < NumWedges; ++k)
			{
				FOneRingWedge& Wedge = Vertex.Wedges[k];

				// collect up set of inset lines relevant to this vertex
				TArray<FLine3d> SolveLines;
				for (int32 j = 0; j < Edges.Num(); ++j)
				{
					if (Edges[j].MeshVertices[0] == Wedge.WedgeVertex)
					{
						SolveLines.Add(AllInsetLines[j].InsetLines0[0]);
					}
					else if (Edges[j].MeshVertices.Last() == Wedge.WedgeVertex)
					{
						SolveLines.Add(AllInsetLines[j].InsetLines0.Last());
					}
					else if (Edges[j].NewMeshVertices[0] == Wedge.WedgeVertex)
					{
						SolveLines.Add(AllInsetLines[j].InsetLines1[0]);
					}
					else if (Edges[j].NewMeshVertices.Last() == Wedge.WedgeVertex)
					{
						SolveLines.Add(AllInsetLines[j].InsetLines1.Last());
					}
				}

				// now that we have our line set, use it to solve inset position
				FVector3d CurPos = Mesh.GetVertex(Wedge.WedgeVertex);
				if (SolveLines.Num() == 1)
				{
					Wedge.NewPosition = SolveLines[0].NearestPoint(CurPos);
				}
				else if (SolveLines.Num() == 2)
				{
					Wedge.NewPosition = UE::Geometry::SolveInsetVertexPositionFromLinePair(CurPos, SolveLines[0], SolveLines[1]);
				}
				else
				{
					// Is this even possible? #SolveLines should equal #BoundaryEdges, how can we have more than 2 at a vertex??
					// fall back to not-very-good inset technique
					Wedge.NewPosition = GetDisplacedVertexPos(Mesh, Wedge.WedgeVertex);
				}
			}
		}
	}


	auto SetDisplacedPositions = [&GetDisplacedVertexPos](FDynamicMesh3& Mesh, TArray<int32>& VerticesIn, TArray<FVector3d>& PositionsIn, int32 InsetStart, int32 InsetEnd)
	{
		int32 NumVertices = VerticesIn.Num();
		if (PositionsIn.Num() == NumVertices)
		{
			int32 Stop = NumVertices - InsetEnd;
			for (int32 k = InsetStart; k < Stop; ++k)
			{
				Mesh.SetVertex(VerticesIn[k], PositionsIn[k]);
			}
		}
	};


	// now bake in new positions
	for (FBevelEdge& Edge : Edges)
	{
		SetDisplacedPositions(Mesh, Edge.MeshVertices, Edge.NewPositions0, Edge.bEndpointBoundaryFlag[0]?0:1, Edge.bEndpointBoundaryFlag[1]?0:1);
		SetDisplacedPositions(Mesh, Edge.NewMeshVertices, Edge.NewPositions1, Edge.bEndpointBoundaryFlag[0]?0:1, Edge.bEndpointBoundaryFlag[1]?0:1);
	}
	for (FBevelLoop& Loop : Loops)
	{
		SetDisplacedPositions(Mesh, Loop.MeshVertices, Loop.NewPositions0, 0, 0);
		SetDisplacedPositions(Mesh, Loop.NewMeshVertices, Loop.NewPositions1, 0, 0);
	}
	for (FBevelVertex& Vertex : Vertices)
	{
		if ( (Vertex.VertexType == EBevelVertexType::JunctionVertex)
			|| (Vertex.VertexType == EBevelVertexType::TerminatorVertex) )
		{
			for (FOneRingWedge& Wedge : Vertex.Wedges)
			{
				Mesh.SetVertex(Wedge.WedgeVertex, Wedge.NewPosition);
			}
		}
	}
}




void FMeshBevel::AppendJunctionVertexPolygon(FDynamicMesh3& Mesh, FBevelVertex& Vertex)
{
	MESH_BEVEL_DEBUG_CHECK(Vertex.VertexType == EBevelVertexType::JunctionVertex);

	// UnlinkJunctionVertex() split the terminator vertex into N vertices, one for each
	// (now disconnected) triangle-wedge. The wedges are ordered such that their wedge-vertices
	// define a polygon with correct winding, so we can just mesh it and append the triangles

	TArray<FVector3d> PolygonPoints;
	for (FOneRingWedge& Wedge : Vertex.Wedges)
	{
		PolygonPoints.Add(Mesh.GetVertex(Wedge.WedgeVertex));
	}

	TArray<FIndex3i> Triangles;
	PolygonTriangulation::TriangulateSimplePolygon<double>(PolygonPoints, Triangles);
	Vertex.NewGroupID = Mesh.AllocateTriangleGroup();
	for (FIndex3i Tri : Triangles)
	{
		int32 A = Vertex.Wedges[Tri.A].WedgeVertex;
		int32 B = Vertex.Wedges[Tri.B].WedgeVertex;
		int32 C = Vertex.Wedges[Tri.C].WedgeVertex;
		int32 tid = Mesh.AppendTriangle(A, B, C, Vertex.NewGroupID);
		if (Mesh.IsTriangle(tid))
		{
			Vertex.NewTriangles.Add(tid);
		}
	}
}


void FMeshBevel::AppendTerminatorVertexTriangle(FDynamicMesh3& Mesh, FBevelVertex& Vertex)
{
	MESH_BEVEL_DEBUG_CHECK(Vertex.VertexType == EBevelVertexType::TerminatorVertex);

	// UnlinkTerminatorVertex() opened up a triangle-shaped hole adjacent to the incoming edge quad-strip
	// at the terminator vertex. The Wedges of the terminator vertex contain the vertex IDs of the two
	// verts on the quad-strip edge. We need the third vertex. We stored [SplitEdge, FarVertexID] in
	// .TerminatorInfo, however FarVertexID may have become a different vertex when we unlinked other
	// vertices. So, we will try to use SplitEdge to find it.
	// If this turns out to have problems, basically the QuadEdgeID is on the boundary of a 3-edge hole,
	// and so it should be straightforward to find the two other boundary edges and that gives the vertex.
	int32 RingSplitEdgeID = Vertex.TerminatorInfo.A;
	if (Mesh.IsEdge(RingSplitEdgeID))
	{
		FIndex2i SplitEdgeV = Mesh.GetEdgeV(RingSplitEdgeID);
		int32 FarVertexID = SplitEdgeV.OtherElement(Vertex.VertexID);

		int32 QuadEdgeID = Mesh.FindEdge(Vertex.Wedges[0].WedgeVertex, Vertex.Wedges[1].WedgeVertex);
		if (Mesh.IsEdge(QuadEdgeID))
		{
			FIndex2i QuadEdgeV = Mesh.GetOrientedBoundaryEdgeV(QuadEdgeID);
			// should have computed this GroupID in initial setup
			int32 UseGroupID = (Vertex.NewGroupID >= 0) ? Vertex.NewGroupID : Mesh.AllocateTriangleGroup();
			int32 tid = Mesh.AppendTriangle(QuadEdgeV.B, QuadEdgeV.A, FarVertexID, UseGroupID);
			if (Mesh.IsTriangle(tid))
			{
				Vertex.NewTriangles.Add(tid);
			}
			else
			{
				MESH_BEVEL_DEBUG_CHECK(false);
			}
		}
		else
		{
			MESH_BEVEL_DEBUG_CHECK(false);
		}
	}
}



void FMeshBevel::AppendTerminatorVertexPairQuad(FDynamicMesh3& Mesh, FBevelVertex& Vertex0, FBevelVertex& Vertex1)
{
	MESH_BEVEL_DEBUG_CHECK(Vertex0.VertexType == EBevelVertexType::TerminatorVertex);
	MESH_BEVEL_DEBUG_CHECK(Vertex1.VertexType == EBevelVertexType::TerminatorVertex);

	// This is a variant of AppendTerminatorVertexTriangle that handles the case where basically two
	// Terminator Vertices are directly connected by a non-beveled mesh edge that was used as the ring-split-edge.
	// Since both sides were opened, we have a quad-shaped hole instead of a triangle-shaped hole, with a quad-edge
	// at each end. The quad can be filled directly, we just need to sort out ordering/etc

	// does not seem like we need to do anything w/ the TerminatorInfo here, we can get everything from wedges
	int32 QuadEdgeID0 = Mesh.FindEdge(Vertex0.Wedges[0].WedgeVertex, Vertex0.Wedges[1].WedgeVertex);
	int32 QuadEdgeID1 = Mesh.FindEdge(Vertex1.Wedges[0].WedgeVertex, Vertex1.Wedges[1].WedgeVertex);
	MESH_BEVEL_DEBUG_CHECK(Mesh.IsEdge(QuadEdgeID0) && Mesh.IsEdge(QuadEdgeID1));
	FIndex2i QuadEdgeV0 = Mesh.GetOrientedBoundaryEdgeV(QuadEdgeID0);
	FIndex2i QuadEdgeV1 = Mesh.GetOrientedBoundaryEdgeV(QuadEdgeID1);

	// make sure that the two opposing/connecting edges exist
	MESH_BEVEL_DEBUG_CHECK(
		Mesh.FindEdge(QuadEdgeV0.A, QuadEdgeV1.B) != IndexConstants::InvalidID &&
		Mesh.FindEdge(QuadEdgeV0.B, QuadEdgeV1.A) != IndexConstants::InvalidID);

	// BIASED? should have computed this GroupID in initial setup
	int32 UseGroupID = (Vertex0.NewGroupID >= 0) ? Vertex0.NewGroupID : Mesh.AllocateTriangleGroup();

	// quad order is V0.B, V0.A, V1.B, V1.A
	int32 tid0 = Mesh.AppendTriangle(QuadEdgeV0.B, QuadEdgeV0.A, QuadEdgeV1.B, UseGroupID);
	MESH_BEVEL_DEBUG_CHECK(tid0 >= 0);
	if (Mesh.IsTriangle(tid0))
	{
		Vertex0.NewTriangles.Add(tid0);
	}
	int32 tid1 = Mesh.AppendTriangle(QuadEdgeV0.B, QuadEdgeV1.B, QuadEdgeV1.A, UseGroupID);
	MESH_BEVEL_DEBUG_CHECK(tid1 >= 0);
	if (Mesh.IsTriangle(tid1))
	{
		Vertex1.NewTriangles.Add(tid1);
	}
}


void FMeshBevel::AppendEdgeQuads(FDynamicMesh3& Mesh, FBevelEdge& Edge)
{
	int32 NumEdges = Edge.MeshEdges.Num();
	if (NumEdges != Edge.NewMeshEdges.Num())
	{
		return;
	}

	Edge.NewGroupID = Mesh.AllocateTriangleGroup();

	// At this point each edge-span should be fully disconnected into a set of paired edges, 
	// so we can trivially join each edge pair with a quad.
	for (int32 k = 0; k < NumEdges; ++k)
	{
		int32 EdgeID0 = Edge.MeshEdges[k];
		int32 EdgeID1 = Edge.NewMeshEdges[k];

		// in certain cases, like bevel topo-edges with a single mesh-edge, we would not
		// have been able to construct the "other" mesh edge when processing the topo-edge
		// (where .NewMeshEdges is computed), it would only have been created when processing the
		// junction vertex. Currently we do not go back and update .NewMeshEdges in that case, but
		// we do store the edge-pair-correspondence in the MeshEdgePairs map. 
		if (EdgeID0 == EdgeID1)
		{
			int32* FoundEdgeID1 = MeshEdgePairs.Find(EdgeID0);
			if (FoundEdgeID1 != nullptr)
			{
				EdgeID1 = *FoundEdgeID1;
			}
		}

		FIndex2i QuadTris(IndexConstants::InvalidID, IndexConstants::InvalidID);
		if (EdgeID0 != EdgeID1 && Mesh.IsEdge(EdgeID1) )
		{
			FIndex2i EdgeV0 = Mesh.GetOrientedBoundaryEdgeV(EdgeID0);
			FIndex2i EdgeV1 = Mesh.GetOrientedBoundaryEdgeV(EdgeID1);
			if (EdgeV0.Contains(EdgeV1.A) || EdgeV0.Contains(EdgeV1.B))
			{
				// If we hit this case, it means that Edge0 and Edge1 are still connected at one end, and
				// so cannot be connected by a Quad, they can only be connected by a single triangle.
				// It is unclear how we end up in this situation, it does occur somewhat regularly in complex
				// geometry scripts though (eg see UE-157531 for a potential repro).
				int32 OtherV = EdgeV0.Contains(EdgeV1.A) ? EdgeV1.B : EdgeV1.A;
				QuadTris.A = Mesh.AppendTriangle(EdgeV0.B, EdgeV0.A, OtherV, Edge.NewGroupID);
				QuadTris.B = IndexConstants::InvalidID;
			}
			else
			{
				QuadTris.A = Mesh.AppendTriangle(EdgeV0.B, EdgeV0.A, EdgeV1.B, Edge.NewGroupID);
				QuadTris.B = Mesh.AppendTriangle(EdgeV1.B, EdgeV1.A, EdgeV0.B, Edge.NewGroupID);
			}
		}
		Edge.StripQuads.Add(QuadTris);
	}
}



void FMeshBevel::AppendLoopQuads(FDynamicMesh3& Mesh, FBevelLoop& Loop)
{
	int32 NumEdges = Loop.MeshEdges.Num();
	if (NumEdges != Loop.NewMeshEdges.Num())
	{
		return;
	}

	auto GetGroupKey = [&Mesh, &Loop](int32 k)
	{
		FIndex2i EdgeTris = Loop.MeshEdgeTris[k];
		int32 Group0 = Mesh.GetTriangleGroup(EdgeTris.A);
		int32 Group1 = Mesh.IsTriangle(EdgeTris.B) ? Mesh.GetTriangleGroup(EdgeTris.B) : -1;
		return FIndex2i(FMath::Max(Group0, Group1), FMath::Min(Group0, Group1));
	};

	TMap<FIndex2i, int> NewGroupIDs;
	for (int32 k = 0; k < NumEdges; ++k)
	{
		FIndex2i GroupKey = GetGroupKey(k);
		if (NewGroupIDs.Contains(GroupKey) == false)
		{
			NewGroupIDs.Add(GroupKey, Mesh.AllocateTriangleGroup());
			Loop.NewGroupIDs.Add(NewGroupIDs[GroupKey]);
		}
	}

	// At this point each edge-span should be fully disconnected into a set of paired edges, 
	// so we can trivially join each edge pair with a quad.
	for (int32 k = 0; k < NumEdges; ++k)
	{
		int32 EdgeID0 = Loop.MeshEdges[k];
		int32 EdgeID1 = Loop.NewMeshEdges[k];

		// case that happens in AppendEdgeQuads() should never happen for loops...

		FIndex2i QuadTris(IndexConstants::InvalidID, IndexConstants::InvalidID);
		if (EdgeID0 != EdgeID1 && Mesh.IsEdge(EdgeID1))
		{
			FIndex2i GroupKey = GetGroupKey(k);
			int32 NewGroupID = NewGroupIDs[GroupKey];

			FIndex2i EdgeV0 = Mesh.GetOrientedBoundaryEdgeV(EdgeID0);
			FIndex2i EdgeV1 = Mesh.GetOrientedBoundaryEdgeV(EdgeID1);
			QuadTris.A = Mesh.AppendTriangle(EdgeV0.B, EdgeV0.A, EdgeV1.B, NewGroupID);
			QuadTris.B = Mesh.AppendTriangle(EdgeV1.B, EdgeV1.A, EdgeV0.B, NewGroupID);
		}
		Loop.StripQuads.Add(QuadTris);
	}
}




void FMeshBevel::CreateBevelMeshing(FDynamicMesh3& Mesh)
{
	for (FBevelVertex& Vertex : Vertices)
	{
		if (Vertex.VertexType == EBevelVertexType::JunctionVertex)
		{
			if (Vertex.Wedges.Num() > 2)
			{
				AppendJunctionVertexPolygon(Mesh, Vertex);
			}
		}
	}

	for (FBevelEdge& Edge : Edges)
	{
		AppendEdgeQuads(Mesh, Edge);
	}
	for (FBevelLoop& Loop : Loops)
	{
		AppendLoopQuads(Mesh, Loop);
	}

	// easier to do terminators last so that we can use quad edge to orient the triangle
	TSet<FIndex2i> HandledQuadVtxPairs;
	for (FBevelVertex& Vertex : Vertices)
	{
		if (Vertex.VertexType == EBevelVertexType::TerminatorVertex)
		{
			if (Vertex.ConnectedBevelVertex >= 0)
			{
				FBevelVertex& OtherVertex = Vertices[Vertex.ConnectedBevelVertex];
				FIndex2i VtxPair(Vertex.VertexID, OtherVertex.VertexID);
				VtxPair.Sort();
				if (HandledQuadVtxPairs.Contains(VtxPair) == false)
				{
					AppendTerminatorVertexPairQuad(Mesh, Vertex, OtherVertex);
					HandledQuadVtxPairs.Add(VtxPair);
				}
			}
			else
			{
				AppendTerminatorVertexTriangle(Mesh, Vertex);
			}
		}
	}
}






void FMeshBevel::ComputeNormals(FDynamicMesh3& Mesh)
{
	if (Mesh.HasAttributes() == false)
	{
		return;
	}
	FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();

	auto SetNormalsOnTriRegion = [NormalOverlay](const TArray<int32>& Triangles)
	{
		if (Triangles.Num() > 0)
		{
			FMeshNormals::InitializeOverlayRegionToPerVertexNormals(NormalOverlay, Triangles);
		}
	};

	for (FBevelVertex& Vertex : Vertices)
	{
		SetNormalsOnTriRegion(Vertex.NewTriangles);
	}

	TArray<int32> TriList;
	for (FBevelEdge& Edge : Edges)
	{
		UELocal::QuadsToTris(Mesh, Edge.StripQuads, TriList, true);
		SetNormalsOnTriRegion(TriList);
	}
	for (FBevelLoop& Loop : Loops)
	{
		UELocal::QuadsToTris(Mesh, Loop.StripQuads, TriList, true);
		SetNormalsOnTriRegion(TriList);
	}
}



void FMeshBevel::ComputeUVs(FDynamicMesh3& Mesh)
{
	if (Mesh.HasAttributes() == false)
	{
		return;
	}
	FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();

	auto SetUVsOnTriRegion = [&Mesh, UVOverlay](const TArray<int32>& Triangles)
	{
		if (Triangles.Num() > 0)
		{
			UE::Geometry::ComputeArbitraryTrianglePatchUVs(Mesh, *UVOverlay, Triangles);
		}
	};


	TArray<int32> TriList;
	for (FBevelEdge& Edge : Edges)
	{
		UELocal::QuadsToTris(Mesh, Edge.StripQuads, TriList, true);
		SetUVsOnTriRegion(TriList);
	}
	for (FBevelLoop& Loop : Loops)
	{
		UELocal::QuadsToTris(Mesh, Loop.StripQuads, TriList, true);
		SetUVsOnTriRegion(TriList);
	}

	// do vertices last because until edges have UVs, the vertex polygons have no neighbour UVs islands
	for (FBevelVertex& Vertex : Vertices)
	{
		SetUVsOnTriRegion(Vertex.NewTriangles);
	}
}



void FMeshBevel::ComputeMaterialIDs(FDynamicMesh3& Mesh)
{
	if (Mesh.HasAttributes() == false || Mesh.Attributes()->HasMaterialID() == false)
	{
		return;
	}
	FDynamicMeshMaterialAttribute* MaterialIDs = Mesh.Attributes()->GetMaterialID();

	if (MaterialIDMode == EMaterialIDMode::ConstantMaterialID)
	{
		for (int32 tid : NewTriangles)
		{
			MaterialIDs->SetValue(tid, SetConstantMaterialID);
		}
	}
	else
	{
		auto SetQuadMaterial = [&Mesh, MaterialIDs](const FIndex2i& Quad, int32 MaterialID)
		{
			if (Quad.A >= 0)
			{
				MaterialIDs->SetValue(Quad.A, MaterialID);
			}
			if (Quad.B >= 0)
			{
				MaterialIDs->SetValue(Quad.B, MaterialID);
			}
		};

		// Try to set materials on the new triangles along a beveled edge based on the adjacent pre-bevel triangles.
		// Could be improved to at least be more consistent in ambiguous cases
		auto SetEdgeMaterials = [&Mesh, MaterialIDs, this, SetQuadMaterial](const TArray<FIndex2i>& StripQuads, const TArray<FIndex2i>& EdgeTris)
		{
			int32 NumEdges = EdgeTris.Num();
			if (StripQuads.Num() == NumEdges)
			{
				TArray<int32> SawMaterialIDs;
				TArray<int32> AmbiguousEdges;
				for (int32 k = 0; k < StripQuads.Num(); ++k)
				{
					FIndex2i NbrTris = EdgeTris[k];
					int MatIDA = MaterialIDs->GetValue(NbrTris.A);
					int MatIDB = (NbrTris.A >= 0) ? MaterialIDs->GetValue(NbrTris.B) : MatIDA;
					SawMaterialIDs.AddUnique(MatIDA);
					SawMaterialIDs.AddUnique(MatIDB);
					int SetMaterialID = (MatIDA == MatIDB) ? MatIDA : SetConstantMaterialID;
					if (MatIDA != MatIDB)
					{
						if (MaterialIDMode == EMaterialIDMode::InferMaterialID_ConstantIfAmbiguous)
						{
							SetMaterialID = SetConstantMaterialID;
						}
						else
						{
							AmbiguousEdges.Add(k);
						}
					}
					SetQuadMaterial(StripQuads[k], SetMaterialID);
				}

				if (AmbiguousEdges.Num() > 0)
				{
					SawMaterialIDs.Sort();
					if (AmbiguousEdges.Num() == NumEdges)		// if all ambigous, just pick one
					{
						for (int32 k : AmbiguousEdges)
						{
							SetQuadMaterial(StripQuads[k], SawMaterialIDs[0]);
						}
					}
					else
					{
						// TODO: what we probably want to do here is "infill" from known neighbours. 
						// for now we will just punt and pick one
						for (int32 k : AmbiguousEdges)
						{
							SetQuadMaterial(StripQuads[k], SawMaterialIDs[0]);
						}
					}
				}

			}
			else
			{
				for (const FIndex2i& Quad : StripQuads)
				{
					SetQuadMaterial(Quad, SetConstantMaterialID);
				}
			}
		};

		for (FBevelEdge& Edge : Edges)
		{
			SetEdgeMaterials(Edge.StripQuads, Edge.MeshEdgeTris);
		}
		for (FBevelLoop& Loop : Loops)
		{
			SetEdgeMaterials(Loop.StripQuads, Loop.MeshEdgeTris);
		}


		// find all the unique material IDs of neighbours of the Triangles list (that are not in Triangles list) and
		// return (MaterialID, NbrTriCount) tuples as a pair of lists
		auto CountUniqueBorderMaterialIDs = [&](const FDynamicMesh3& Mesh, const FDynamicMeshMaterialAttribute& MaterialAttrib, const TArray<int32>& Triangles, TArray<int32>& MaterialIDs, TArray<int32>& Counts)
		{
			MaterialIDs.Reset(); Counts.Reset();
			for (int32 tid : Triangles)
			{
				FIndex3i TriNbrs = Mesh.GetTriNeighbourTris(tid);
				for (int32 j = 0; j < 3; ++j)
				{
					int32 NbrTriangleID = TriNbrs[j];
					if ( Mesh.IsTriangle(NbrTriangleID) == false || Triangles.Contains(NbrTriangleID) )
					{
						continue;
					}
					int MatID = MaterialAttrib.GetValue(NbrTriangleID);
					int32 Index = MaterialIDs.AddUnique(MatID);
					if (Counts.Num() != MaterialIDs.Num())
					{
						Counts.Add(0);
						Counts[Index]++;
					}
				}
			}
		};

		// For each bevel-vertex-polygon, pick the nbr material ID that was most frequent.
		// Terminator vertices are also handled this way, which is not ideal, should probably
		// ignore the new 'edge' faces for the terminator vertex
		for (FBevelVertex& Vertex : Vertices)
		{
			TArray<int32> NbrMaterialIDs, NbrMaterialIDCounts;
			CountUniqueBorderMaterialIDs(Mesh, *MaterialIDs, Vertex.NewTriangles, NbrMaterialIDs, NbrMaterialIDCounts);
			int32 SetMaterialID = SetConstantMaterialID;
			if (NbrMaterialIDs.Num() > 0)
			{
				int32 MinIndex = 0;
				for (int32 k = 1; k < NbrMaterialIDs.Num(); ++k)
				{
					if (NbrMaterialIDCounts[k] < NbrMaterialIDCounts[MinIndex])
					{
						MinIndex = k;
					}
				}
				SetMaterialID = NbrMaterialIDs[MinIndex];
			}
			for (int32 tid : Vertex.NewTriangles)
			{
				MaterialIDs->SetValue(tid, SetMaterialID);
			}

		}
	}

}
