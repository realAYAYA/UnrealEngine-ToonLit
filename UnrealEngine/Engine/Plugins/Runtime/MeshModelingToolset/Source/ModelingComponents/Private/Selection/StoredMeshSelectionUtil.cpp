// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/StoredMeshSelectionUtil.h"
#include "Selection/PersistentMeshSelection.h"
#include "Selection/PersistentMeshSelectionManager.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "GroupTopology.h"
#include "Selection/GeometrySelectionManager.h"

using namespace UE::Geometry;




bool UE::Geometry::GetCurrentGeometrySelectionForTarget(const FToolBuilderState& SceneState, UToolTarget* Target, FGeometrySelection& SelectionOut)
{
	UGeometrySelectionManager* SelectionManager = SceneState.ToolManager->GetContextObjectStore()->FindContext<UGeometrySelectionManager>();
	if (SelectionManager == nullptr)
	{
		return false;
	}
	IPrimitiveComponentBackedTarget* TargetInterface = Cast<IPrimitiveComponentBackedTarget>(Target);
	if (TargetInterface == nullptr)
	{
		return false;
	}

	bool bHaveSelection = SelectionManager->GetSelectionForComponent(TargetInterface->GetOwnerComponent(), SelectionOut);
	return bHaveSelection;
}


const UPersistentMeshSelection* UE::Geometry::GetCurrentToolInputSelection(const FToolBuilderState& SceneState, UToolTarget* Target)
{
	UGeometrySelectionManager* SelectionManager = SceneState.ToolManager->GetContextObjectStore()->FindContext<UGeometrySelectionManager>();
	if (SelectionManager == nullptr)
	{
		return nullptr;
	}
	IPrimitiveComponentBackedTarget* TargetInterface = Cast<IPrimitiveComponentBackedTarget>(Target);
	if (TargetInterface == nullptr)
	{
		return nullptr;
	}

	UPersistentMeshSelection* ActiveSelection = SelectionManager->GetActiveSingleSelectionConverted_Legacy(TargetInterface->GetOwnerComponent());
	if (ActiveSelection == nullptr)
	{
		return nullptr;
	}

	if (TargetInterface->GetOwnerComponent() != ActiveSelection->GetTargetComponent())
	{
		return nullptr;
	}

	return ActiveSelection;
}



bool UE::Geometry::GetStoredSelectionAsTriangles(
	const UPersistentMeshSelection* Selection,
	const FDynamicMesh3& Mesh,
	TArray<int32>& TrianglesOut)
{
	// don't support UV selection currently - unclear how reproducible this is?
	if (Selection->GetSelectionType() == FGenericMeshSelection::ETopologyType::FUVGroupTopology)
	{
		return false;
	}

	if (Selection->GetSelection().FaceIDs.Num() == 0)
	{
		return false;
	}

	if (Selection->GetSelectionType() == FGenericMeshSelection::ETopologyType::FGroupTopology)
	{
		TSet<int32> SelectedGroups;
		for (int32 gid : Selection->GetSelection().FaceIDs)
		{
			SelectedGroups.Add(gid);
		}

		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			int32 gid = Mesh.GetTriangleGroup(tid);
			if (SelectedGroups.Contains(gid))
			{
				TrianglesOut.Add(tid);
			}
		}
	}
	else if (Selection->GetSelectionType() == FGenericMeshSelection::ETopologyType::FTriangleGroupTopology)
	{
		for (int gid : Selection->GetSelection().FaceIDs)
		{
			if (Mesh.IsTriangle(gid))
			{
				TrianglesOut.Add(gid);
			}
		}
	}

	return true;
}




void UE::Geometry::ClearActiveToolSelection(UInteractiveToolManager* ToolManager)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UDEPRECATED_PersistentMeshSelectionManager* SelectionManager = FindPersistentMeshSelectionManager(ToolManager);
	if (SelectionManager != nullptr)
	{
		SelectionManager->ClearActiveSelection();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


bool UE::Geometry::SetToolOutputSelection(UInteractiveTool* Tool, UPersistentMeshSelection* Selection)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UDEPRECATED_PersistentMeshSelectionManager* SelectionManager = FindPersistentMeshSelectionManager(Tool->GetToolManager());
	if (SelectionManager == nullptr)
	{
		return false;
	}

	if (Selection != nullptr)
	{
		SelectionManager->SetNewActiveSelection(Selection);
	}
	else
	{
		SelectionManager->ClearActiveSelection();
	}

	return true;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}





namespace UELocal
{
	// Helper function for GetGroupEdgeRepresentativeVerts(). Given a loop as a list of vids, with
	// the first and last vid the same, returns the lowest vid and its lower-vid neighbor.
	FIndex2i GetLoopRepresentativeVerts(const TArray<int32>& Verts)
	{
		int32 NumVerts = Verts.Num();

		int32 MinVid = Verts[0];
		// Last vert is a repeat of first vert, so neighbor is second to last.
		int32 MinNeighbor = FMath::Min(Verts[1], Verts[NumVerts - 2]);

		for (int32 i = 1; i < NumVerts - 1; ++i)
		{
			if (Verts[i] < MinVid)
			{
				MinVid = Verts[i];
				MinNeighbor = FMath::Min(Verts[i - 1], Verts[i + 1]);
			}
		}
		return FIndex2i(MinVid, MinNeighbor); // we know that MinVid is smaller
	}
}//end namespace UELocal
using namespace UELocal;





UE::Geometry::FIndex2i UE::Geometry::GetGroupEdgeRepresentativeVerts(const FGroupTopology& TopologyIn, int GroupEdgeID, const FCompactMaps& CompactMaps)
{
	check(GroupEdgeID >= 0 && GroupEdgeID < TopologyIn.Edges.Num());

	const FGroupTopology::FGroupEdge& GroupEdge = TopologyIn.Edges[GroupEdgeID];
	const TArray<int32>& Verts = GroupEdge.Span.Vertices;

	if (GroupEdge.EndpointCorners.A != IndexConstants::InvalidID)
	{
		// Use remapped vids
		int32 FirstVid = CompactMaps.GetVertexMapping(Verts[0]);
		int32 FirstNeighbor = CompactMaps.GetVertexMapping(Verts[1]);
		int32 LastVid = CompactMaps.GetVertexMapping(Verts.Last());
		int32 LastNeighbor = CompactMaps.GetVertexMapping(Verts[Verts.Num() - 2]);

		return FirstVid < LastVid ?
			FIndex2i(FMath::Min(FirstVid, FirstNeighbor), FMath::Max(FirstVid, FirstNeighbor))
			: FIndex2i(FMath::Min(LastVid, LastNeighbor), FMath::Max(LastVid, LastNeighbor));
	}
	else
	{
		TArray<int32> RemappedVerts;
		RemappedVerts.SetNum(Verts.Num());
		for (int32 i = 0; i < Verts.Num(); ++i)
		{
			RemappedVerts[i] = CompactMaps.GetVertexMapping(Verts[i]);
		}
		return GetLoopRepresentativeVerts(RemappedVerts);
	}
}

UE::Geometry::FIndex2i UE::Geometry::GetGroupEdgeRepresentativeVerts(const FGroupTopology& TopologyIn, int GroupEdgeID)
{
	check(GroupEdgeID >= 0 && GroupEdgeID < TopologyIn.Edges.Num());

	const FGroupTopology::FGroupEdge& GroupEdge = TopologyIn.Edges[GroupEdgeID];
	const TArray<int32>& Verts = GroupEdge.Span.Vertices;

	if (GroupEdge.EndpointCorners.A != IndexConstants::InvalidID)
	{
		int32 FirstVid = Verts[0];
		int32 FirstNeighbor = Verts[1];
		int32 LastVid = Verts.Last();
		int32 LastNeighbor = Verts[Verts.Num() - 2];

		return FirstVid < LastVid ?
			FIndex2i(FMath::Min(FirstVid, FirstNeighbor), FMath::Max(FirstVid, FirstNeighbor))
			: FIndex2i(FMath::Min(LastVid, LastNeighbor), FMath::Max(LastVid, LastNeighbor));
	}
	else
	{
		return GetLoopRepresentativeVerts(Verts);
	}
}
