// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/PersistentMeshSelection.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Util/CompactMaps.h"
#include "GroupTopology.h"
#include "Selection/StoredMeshSelectionUtil.h"
#include "MeshRegionBoundaryLoops.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PersistentMeshSelection)

using namespace UE::Geometry;


void UPersistentMeshSelection::SetSelection(const FGroupTopology& TopologyIn, const FGroupTopologySelection& SelectionIn,
											const FCompactMaps* CompactMaps)
{
	FGenericMeshSelection& Data = this->Selection;

	Data.FaceIDs = SelectionIn.SelectedGroupIDs.Array();
	Data.VertexIDs.Reset();
	Data.EdgeIDs.Reset();

	const FDynamicMesh3* GroupMesh = TopologyIn.GetMesh();

	for (int32 CornerID : SelectionIn.SelectedCornerIDs)
	{
		int32 CornerVID = TopologyIn.GetCornerVertexID(CornerID);
		if (CompactMaps != nullptr)
		{
			CornerVID = CompactMaps->GetVertexMapping(CornerVID);
		}
		Data.VertexIDs.Add(CornerVID);

		Data.RenderVertices.Add(GroupMesh->GetVertex(CornerVID));
	}

	for (int32 GroupEdgeID : SelectionIn.SelectedEdgeIDs)
	{
		FIndex2i EdgeVerts = (CompactMaps != nullptr) ?
			GetGroupEdgeRepresentativeVerts(TopologyIn, GroupEdgeID, *CompactMaps) : GetGroupEdgeRepresentativeVerts(TopologyIn, GroupEdgeID);
		Data.EdgeIDs.Add(EdgeVerts);

		const FGroupTopology::FGroupEdge& GroupEdge = TopologyIn.Edges[GroupEdgeID];
		const TArray<int32>& GroupEdgeVertices = GroupEdge.Span.Vertices;
		int32 N = GroupEdgeVertices.Num();
		int32 EdgeVertA = (CompactMaps) ? CompactMaps->GetVertexMapping(GroupEdgeVertices[0]) : GroupEdgeVertices[0];
		for (int32 k = 1; k < N; ++k)
		{
			int32 EdgeVertB = (CompactMaps) ? CompactMaps->GetVertexMapping(GroupEdgeVertices[k]) : GroupEdgeVertices[k];
			Data.RenderEdges.Add( FSegment3d(GroupMesh->GetVertex(EdgeVertA), GroupMesh->GetVertex(EdgeVertB)) );
			EdgeVertA = EdgeVertB;
		}
	}

	if (Data.FaceIDs.Num() > 0)
	{
		TArray<int32> TriangleIDs;
		TopologyIn.GetSelectedTriangles(SelectionIn, TriangleIDs);

		if (CompactMaps)
		{
			for (int32& tid : TriangleIDs)
			{
				tid = CompactMaps->GetTriangleMapping(tid);
			}
		}
		
		FMeshRegionBoundaryLoops RegionLoops(GroupMesh, TriangleIDs, true);
		for (const FEdgeLoop& Loop : RegionLoops.Loops)
		{
			int32 N = Loop.GetVertexCount();
			for (int32 k = 0; k < N; ++k )
			{
				Data.RenderEdges.Add( FSegment3d(GroupMesh->GetVertex(Loop.Vertices[k]), GroupMesh->GetVertex(Loop.Vertices[(k+1)%N])) );
			}
		}
	}


}


void UPersistentMeshSelection::ExtractIntoSelectionObject(const FGroupTopology& TopologyIn, FGroupTopologySelection& SelectionOut) const
{
	const FGenericMeshSelection& Data = this->Selection;

	SelectionOut.Clear();

	const FDynamicMesh3* Mesh = TopologyIn.GetMesh();
	if (!Mesh)
	{
		ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology must have valid underlying mesh. "));
		return;
	}

	SelectionOut.SelectedGroupIDs = TSet<int32>(Data.FaceIDs);

	for (int32 Vid : Data.VertexIDs)
	{
		if (!Mesh->IsVertex(Vid))
		{
			ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology's mesh was missing a vertex ID. "
				"Perhaps the mesh was compacted without updating the stored selection?"));
			continue;
		}
		int32 CornerID = TopologyIn.GetCornerIDFromVertexID(Vid);
		if (CornerID == IndexConstants::InvalidID)
		{
			ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology did not have an expected vert as a corner. "
				"Is the topology initialized, and based on the same mesh?"));
			continue;
		}
		SelectionOut.SelectedCornerIDs.Add(CornerID);
	}
	for (const FIndex2i& EdgeVerts : Data.EdgeIDs)
	{
		if (!Mesh->IsVertex(EdgeVerts.A) || !Mesh->IsVertex(EdgeVerts.B))
		{
			ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology's mesh was missing a vertex ID. "
				"Perhaps the mesh was compacted without updating the stored selection?"));
			continue;
		}
		int32 Eid = Mesh->FindEdge(EdgeVerts.A, EdgeVerts.B);
		if (Eid == IndexConstants::InvalidID)
		{
			ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology's mesh was missing an expected edge."));
			continue;
		}
		int32 GroupEdgeID = TopologyIn.FindGroupEdgeID(Eid);
		if (Eid == IndexConstants::InvalidID)
		{
			ensureMsgf(false, TEXT("FStoredGroupTopologySelection::ExtractIntoSelectionObject: target topology did not have an expected group edge."
				"Is the topology initialized, and based on the same mesh?"));
			continue;
		}

		SelectionOut.SelectedEdgeIDs.Add(GroupEdgeID);
	}
}
