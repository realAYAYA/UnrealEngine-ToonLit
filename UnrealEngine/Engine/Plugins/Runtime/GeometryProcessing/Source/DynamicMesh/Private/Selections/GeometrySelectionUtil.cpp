// Copyright Epic Games, Inc. All Rights Reserved.


#include "Selections/GeometrySelectionUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/ColliderMesh.h"
#include "DynamicMesh/MeshNormals.h"
#include "TriangleTypes.h"
#include "SegmentTypes.h"
#include "GroupTopology.h"
#include "Selections/MeshConnectedComponents.h"
#include "Algo/Find.h"


bool UE::Geometry::FindInSelectionByTopologyID(
	const FGeometrySelection& GeometrySelection,
	uint32 TopologyID,
	uint64& FoundValue)
{
	const uint64* Found = Algo::FindByPredicate(GeometrySelection.Selection, [&](uint64 Item)
	{
		return FGeoSelectionID(Item).TopologyID == TopologyID;
	});
	if (Found != nullptr)
	{
		FoundValue = *Found;
		return true;
	}
	FoundValue = FGeoSelectionID().Encoded();
	return false;
}


void UE::Geometry::UpdateTriangleSelectionViaRaycast(
	const FColliderMesh* ColliderMesh,
	FGeometrySelectionEditor* Editor,
	const FRay3d& LocalRay,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut )
{
	check(Editor->GetTopologyType() == EGeometryTopologyType::Triangle);

	ResultOut.bSelectionMissed = true;

	double RayHitT; int32 HitTriangleID; FVector3d HitBaryCoords;
	if (ColliderMesh->FindNearestHitTriangle(LocalRay, RayHitT, HitTriangleID, HitBaryCoords))
	{
		HitTriangleID = ColliderMesh->GetSourceTriangleID(HitTriangleID);
		if (HitTriangleID == IndexConstants::InvalidID)
		{
			return;
		}

		if (Editor->GetElementType() == EGeometryElementType::Face)
		{
			ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{(uint64)HitTriangleID}, &ResultOut.SelectionDelta);
			ResultOut.bSelectionMissed = false;
		}
		else if (Editor->GetElementType() == EGeometryElementType::Vertex)
		{
			FVector3d HitPos = LocalRay.PointAt(RayHitT);
			FIndex3i TriVerts = ColliderMesh->GetTriangle(HitTriangleID);
			int32 NearestIdx = 0;
			double NearestDistSqr = DistanceSquared(ColliderMesh->GetVertex(TriVerts[0]), HitPos);
			for (int32 k = 1; k < 3; ++k)
			{
				double DistSqr = DistanceSquared(ColliderMesh->GetVertex(TriVerts[k]), HitPos);
				if (DistSqr < NearestDistSqr)
				{
					NearestDistSqr = DistSqr;
					NearestIdx = k;
				}
			}

			ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{(uint64)TriVerts[NearestIdx]}, &ResultOut.SelectionDelta);
			ResultOut.bSelectionMissed = false;
		}
		else if (Editor->GetElementType() == EGeometryElementType::Edge)
		{
			FVector3d HitPos = LocalRay.PointAt(RayHitT);
			FIndex3i TriVerts = ColliderMesh->GetTriangle(HitTriangleID);
			FVector3d Positions[3];
			ColliderMesh->GetTriVertices(HitTriangleID, Positions[0], Positions[1], Positions[2]);
			int32 NearestIdx = 0;
			double NearestDistSqr = FSegment3d(Positions[0], Positions[1]).DistanceSquared(HitPos);
			for (int32 k = 1; k < 3; ++k)
			{
				double DistSqr = FSegment3d(Positions[k], Positions[(k+1)%3]).DistanceSquared(HitPos);
				if (DistSqr < NearestDistSqr)
				{
					NearestDistSqr = DistSqr;
					NearestIdx = k;
				}
			}
			FMeshTriEdgeID TriEdgeID(HitTriangleID, NearestIdx);

			ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{(uint64)TriEdgeID.Encoded()}, &ResultOut.SelectionDelta);
			ResultOut.bSelectionMissed = false;
		}
	}
}





void UE::Geometry::UpdateGroupSelectionViaRaycast(
	const FColliderMesh* ColliderMesh,
	const FGroupTopology* GroupTopology,
	FGeometrySelectionEditor* Editor,
	const FRay3d& LocalRay,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut)
{
	check(Editor->GetTopologyType() == EGeometryTopologyType::Polygroup);

	ResultOut.bSelectionMissed = true;

	double RayHitT; int32 HitTriangleID; FVector3d HitBaryCoords;
	if (ColliderMesh->FindNearestHitTriangle(LocalRay, RayHitT, HitTriangleID, HitBaryCoords))
	{
		HitTriangleID = ColliderMesh->GetSourceTriangleID(HitTriangleID);
		if (HitTriangleID == IndexConstants::InvalidID)
		{
			return;
		}
		int32 GroupID = GroupTopology->GetGroupID(HitTriangleID);

		if (Editor->GetElementType() == EGeometryElementType::Face)
		{
			FGeoSelectionID GroupTriID((uint32)HitTriangleID, (uint32)GroupID);
			ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{GroupTriID.Encoded()}, &ResultOut.SelectionDelta);
			ResultOut.bSelectionMissed = false;
		}
		else if (Editor->GetElementType() == EGeometryElementType::Vertex)
		{
			FVector3d HitPos = LocalRay.PointAt(RayHitT);
			FIndex3i TriVerts = ColliderMesh->GetTriangle(HitTriangleID);
			int32 NearestIdx = -1;
			double NearestDistSqr = DistanceSquared(ColliderMesh->GetVertex(TriVerts[0]), HitPos);
			for (int32 k = 0; k < 3; ++k)
			{
				if (GroupTopology->GetCornerIDFromVertexID(TriVerts[k]) != IndexConstants::InvalidID)
				{
					double DistSqr = DistanceSquared(ColliderMesh->GetVertex(TriVerts[k]), HitPos);
					if (DistSqr < NearestDistSqr)
					{
						NearestDistSqr = DistSqr;
						NearestIdx = k;
					}
				}
			}
			if (NearestIdx >= 0)
			{
				// do we need a group here?
				int32 VertexID = TriVerts[NearestIdx];
				ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{(uint64)VertexID}, &ResultOut.SelectionDelta);
				ResultOut.bSelectionMissed = false;
			}
		}
		else if (Editor->GetElementType() == EGeometryElementType::Edge)
		{
			FVector3d HitPos = LocalRay.PointAt(RayHitT);
			FIndex3i TriVerts = ColliderMesh->GetTriangle(HitTriangleID);
			FVector3d Positions[3];
			ColliderMesh->GetTriVertices(HitTriangleID, Positions[0], Positions[1], Positions[2]);
			int32 NearestIdx = -1;
			double NearestDistSqr = TNumericLimits<double>::Max();
			for (int32 k = 0; k < 3; ++k)
			{
				if ( GroupTopology->IsGroupEdge(FMeshTriEdgeID(HitTriangleID, k), true) )
				{
					double DistSqr = FSegment3d(Positions[k], Positions[(k + 1) % 3]).DistanceSquared(HitPos);
					if (DistSqr < NearestDistSqr)
					{
						NearestDistSqr = DistSqr;
						NearestIdx = k;
					}
				}
			}
			if ( NearestIdx >= 0 )
			{
				// do we need a group here?
				FMeshTriEdgeID TriEdgeID(HitTriangleID, NearestIdx);
				ResultOut.bSelectionModified = UpdateSelectionWithNewElements(Editor, UpdateConfig.ChangeType, TArray<uint64>{(uint64)TriEdgeID.Encoded()}, & ResultOut.SelectionDelta);
				ResultOut.bSelectionMissed = false;
			}
		}
	}
}







bool UE::Geometry::UpdateSelectionWithNewElements(
	FGeometrySelectionEditor* Editor,
	EGeometrySelectionChangeType ChangeType,
	const TArray<uint64>& NewIDs,
	FGeometrySelectionDelta* Delta)
{
	FGeometrySelectionDelta LocalDelta;
	FGeometrySelectionDelta& UseDelta = (Delta != nullptr) ? (*Delta) : LocalDelta;

	if (ChangeType == EGeometrySelectionChangeType::Replace)
	{
		// [TODO] this could be optimized...
		Editor->ClearSelection(UseDelta);
		return Editor->Select(NewIDs, UseDelta);
	}
	else if (ChangeType == EGeometrySelectionChangeType::Add)
	{
		return Editor->Select(NewIDs, UseDelta);
	}
	else if (ChangeType == EGeometrySelectionChangeType::Remove)
	{
		return Editor->Deselect(NewIDs, UseDelta);
	}
	else
	{
		ensure(false);
		return false;
	}
	
}



bool UE::Geometry::EnumerateTriangleSelectionVertices(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FTransform& ApplyTransform,
	TFunctionRef<void(uint64, const FVector3d&)> VertexFunc)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Triangle ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 TriangleID : MeshSelection.Selection)
		{
			if (Mesh.IsTriangle((int32)TriangleID))
			{
				FIndex3i Triangle = Mesh.GetTriangle((int32)TriangleID);
				VertexFunc((uint64)Triangle.A, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.A)));
				VertexFunc((uint64)Triangle.B, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.B)));
				VertexFunc((uint64)Triangle.C, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.C)));
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID((uint32)EncodedID);
			int32 EdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if (Mesh.IsEdge(EdgeID))
			{
				FIndex2i EdgeV = Mesh.GetEdgeV(EdgeID);
				VertexFunc((uint64)EdgeV.A, ApplyTransform.TransformPosition(Mesh.GetVertex(EdgeV.A)));
				VertexFunc((uint64)EdgeV.B, ApplyTransform.TransformPosition(Mesh.GetVertex(EdgeV.B)));
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 VertexID : MeshSelection.Selection)
		{
			if (Mesh.IsVertex((int32)VertexID))
			{
				VertexFunc((uint64)VertexID, ApplyTransform.TransformPosition(Mesh.GetVertex((int32)VertexID)));
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}



bool UE::Geometry::EnumeratePolygroupSelectionVertices(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	const FTransform& ApplyTransform,
	TFunctionRef<void(uint64, const FVector3d&)> VertexFunc)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Polygroup ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FGeoSelectionID GroupTriID(EncodedID);
			int32 SeedTriangleID = (int32)GroupTriID.GeometryID, GroupID = (int32)GroupTriID.TopologyID;
			if (Mesh.IsTriangle(SeedTriangleID))
			{
				for (int32 TriangleID : GroupTopology->GetGroupFaces(GroupID))
				{
					FIndex3i Triangle = Mesh.GetTriangle((int32)TriangleID);
					VertexFunc((uint64)Triangle.A, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.A)));
					VertexFunc((uint64)Triangle.B, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.B)));
					VertexFunc((uint64)Triangle.C, ApplyTransform.TransformPosition(Mesh.GetVertex(Triangle.C)));
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 SeedEdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if (Mesh.IsEdge(SeedEdgeID))
			{
				int32 GroupEdgeID = GroupTopology->FindGroupEdgeID(SeedEdgeID);
				for (int32 VertexID : GroupTopology->GetGroupEdgeVertices(GroupEdgeID))
				{
					FVector3d V = Mesh.GetVertex(VertexID);
					VertexFunc((uint64)VertexID, ApplyTransform.TransformPosition(V));
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsVertex(VertexID))
			{
				VertexFunc((uint64)VertexID, ApplyTransform.TransformPosition(Mesh.GetVertex(VertexID)));
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}



bool UE::Geometry::EnumerateSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32)> TriangleFunc,
	const UE::Geometry::FPolygroupSet* UseGroupSet
)
{
	if (MeshSelection.TopologyType == EGeometryTopologyType::Triangle)
	{
		return EnumerateTriangleSelectionTriangles(MeshSelection, Mesh, TriangleFunc);
	}
	else if (MeshSelection.TopologyType == EGeometryTopologyType::Polygroup)
	{
		return EnumeratePolygroupSelectionTriangles(MeshSelection, Mesh, 
			(UseGroupSet != nullptr) ? *UseGroupSet : FPolygroupSet(&Mesh), 
			TriangleFunc);
	}
	return false;
}


bool UE::Geometry::EnumerateTriangleSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32)> TriangleFunc)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Triangle ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 TriangleID : MeshSelection.Selection)
		{
			if (Mesh.IsTriangle((int32)TriangleID))
			{
				TriangleFunc((int32)TriangleID);
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 EdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			Mesh.EnumerateEdgeTriangles(EdgeID, [&](int32 TriangleID)
			{
				TriangleFunc(TriangleID);
			});
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 VertexID : MeshSelection.Selection)
		{
			Mesh.EnumerateVertexTriangles((int32)VertexID, [&](int32 TriangleID)
			{
				TriangleFunc(TriangleID);
			});
		}
	}
	else
	{
		return false;
	}

	return true;
}


bool UE::Geometry::EnumeratePolygroupSelectionTriangles(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const UE::Geometry::FPolygroupSet& GroupSet,
	TFunctionRef<void(int32)> TriangleFunc
)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Polygroup ) == false )
	{
		return false;
	}

	TArray<int32> SeedGroups;
	TArray<int32> SeedTriangles;
	TSet<int32> UniqueSeedGroups;

	// TODO: the code below will not work correctly if the selection contains
	// multiple disconnected-components with the same GroupID. They will be
	// filtered out by the UniqueSeedGroups test. Seems like it will be necessary
	// to detect this case up-front and do something more expensive, like filtering
	// out duplicates inside the connected-components loop instead of up-front

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FGeoSelectionID SelectionID(EncodedID);
			int32 SeedTriangleID = (int32)SelectionID.GeometryID;
			if (Mesh.IsTriangle(SeedTriangleID))
			{
				int32 GroupID = GroupSet.GetGroup(SeedTriangleID);
				check(GroupID == (int32)SelectionID.TopologyID);		// sanity-check that we are using the right group
				if ( GroupID >= 0 && UniqueSeedGroups.Contains(GroupID) == false)
				{
					UniqueSeedGroups.Add(GroupID);
					SeedGroups.Add(GroupID);
					SeedTriangles.Add(SeedTriangleID);
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 SeedEdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if (Mesh.IsEdge(SeedEdgeID))
			{
				Mesh.EnumerateEdgeTriangles(SeedEdgeID, [&](int32 TriangleID)
				{
					int32 GroupID = GroupSet.GetGroup(TriangleID);
					if (GroupID >= 0 && UniqueSeedGroups.Contains(GroupID) == false)
					{
						UniqueSeedGroups.Add(GroupID);
						SeedGroups.Add(GroupID);
						SeedTriangles.Add(TriangleID);
					}
				});
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsVertex(VertexID))
			{
				Mesh.EnumerateVertexTriangles(VertexID, [&](int32 TriangleID)
				{
					int32 GroupID = GroupSet.GetGroup(TriangleID);
					if (GroupID >= 0 && UniqueSeedGroups.Contains(GroupID) == false)
					{
						UniqueSeedGroups.Add(GroupID);
						SeedGroups.Add(GroupID);
						SeedTriangles.Add(TriangleID);
					}
				});
			}
		}
	}
	else
	{
		return false;
	}


	TSet<int> TempROI;		// if we could provide this as input we would not need a temporary roi...
	TArray<int32> QueueBuffer;
	int32 NumGroups = SeedGroups.Num();
	for (int32 k = 0; k < NumGroups; ++k)
	{
		check(GroupSet.GetGroup(SeedTriangles[k]) == SeedGroups[k]);
		int32 GroupID = SeedGroups[k];
		FMeshConnectedComponents::GrowToConnectedTriangles(&Mesh, 
			TArray<int>{SeedTriangles[k]}, TempROI, &QueueBuffer, 
			[&](int32 T1, int32 T2) { return GroupSet.GetGroup(T2) == GroupID; });
		for (int32 tid : TempROI)
		{
			TriangleFunc(tid);
		}
	}

	return true;
}




bool UE::Geometry::EnumerateTriangleSelectionElements(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	TFunctionRef<void(int32, const FVector3d&)> VertexFunc,
	TFunctionRef<void(int32, const FSegment3d&)> EdgeFunc,
	TFunctionRef<void(int32, const FTriangle3d&)> TriangleFunc,
	const FTransform* ApplyTransform
)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Triangle ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 TriangleID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsTriangle(TriangleID))
			{
				FVector3d A, B, C;
				Mesh.GetTriVertices((int32)TriangleID, A, B, C);
				if (ApplyTransform != nullptr)
				{
					A = ApplyTransform->TransformPosition(A);
					B = ApplyTransform->TransformPosition(B);
					C = ApplyTransform->TransformPosition(C);
				}
				TriangleFunc(TriangleID, FTriangle3d(A, B, C));
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 EdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if (Mesh.IsEdge(EdgeID))
			{
				FVector3d A, B;
				Mesh.GetEdgeV(EdgeID, A, B);
				if (ApplyTransform != nullptr)
				{
					A = ApplyTransform->TransformPosition(A);
					B = ApplyTransform->TransformPosition(B);
				}
				EdgeFunc(EdgeID, FSegment3d(A, B));
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsVertex(VertexID))
			{
				FVector3d A = Mesh.GetVertex(VertexID);
				VertexFunc(VertexID, (ApplyTransform != nullptr) ? ApplyTransform->TransformPosition(A) : A);
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}





bool UE::Geometry::EnumeratePolygroupSelectionElements(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	TFunctionRef<void(int32, const FVector3d&)> VertexFunc,
	TFunctionRef<void(int32, const FSegment3d&)> EdgeFunc,
	TFunctionRef<void(int32, const FTriangle3d&)> TriangleFunc,
	const FTransform* ApplyTransform
)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Polygroup ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FGeoSelectionID SelectionID(EncodedID);
			int32 SeedTriangleID = (int32)SelectionID.GeometryID, GroupID = (int32)SelectionID.TopologyID;
			if (Mesh.IsTriangle(SeedTriangleID))
			{
				for (int32 TriangleID : GroupTopology->GetGroupFaces(GroupID))
				{
					FVector3d A, B, C;
					Mesh.GetTriVertices((int32)TriangleID, A, B, C);
					if (ApplyTransform != nullptr)
					{
						A = ApplyTransform->TransformPosition(A);
						B = ApplyTransform->TransformPosition(B);
						C = ApplyTransform->TransformPosition(C);
					}
					TriangleFunc(TriangleID, FTriangle3d(A, B, C));
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 SeedEdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if (Mesh.IsEdge(SeedEdgeID))
			{
				int32 GroupEdgeID = GroupTopology->FindGroupEdgeID(SeedEdgeID);
				for (int32 EdgeID : GroupTopology->GetGroupEdgeEdges(GroupEdgeID))
				{
					FVector3d A, B;
					Mesh.GetEdgeV(EdgeID, A, B);
					if (ApplyTransform != nullptr)
					{
						A = ApplyTransform->TransformPosition(A);
						B = ApplyTransform->TransformPosition(B);
					}
					EdgeFunc(EdgeID, FSegment3d(A, B));
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsVertex(VertexID))
			{
				FVector3d A = Mesh.GetVertex(VertexID);
				VertexFunc(VertexID, (ApplyTransform != nullptr) ? ApplyTransform->TransformPosition(A) : A);
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}




bool UE::Geometry::ConvertPolygroupSelectionToTopologySelection(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	const FGroupTopology* GroupTopology,
	FGroupTopologySelection& TopologySelectionOut)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Polygroup ) == false )
	{
		return false;
	}

	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 GroupID = FGeoSelectionID(EncodedID).TopologyID;
			if (GroupTopology->FindGroupByID(GroupID) != nullptr)
			{
				TopologySelectionOut.SelectedGroupIDs.Add(GroupID);
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID MeshEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			if ( Mesh.IsTriangle((int32)MeshEdgeID.TriangleID) )
			{
				int32 GroupEdgeID = GroupTopology->FindGroupEdgeID(MeshEdgeID);
				if (GroupEdgeID >= 0)
				{
					TopologySelectionOut.SelectedEdgeIDs.Add(GroupEdgeID);
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if ( Mesh.IsVertex(VertexID) )
			{
				int32 CornerID = GroupTopology->GetCornerIDFromVertexID(VertexID);
				if (CornerID >= 0)
				{
					TopologySelectionOut.SelectedCornerIDs.Add(CornerID);
				}
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}




bool UE::Geometry::GetTriangleSelectionFrame(
	const FGeometrySelection& MeshSelection,
	const UE::Geometry::FDynamicMesh3& Mesh,
	FFrame3d& SelectionFrameOut)
{
	if ( ensure( MeshSelection.TopologyType == EGeometryTopologyType::Triangle ) == false )
	{
		return false;
	}

	FVector3d AccumulatedOrigin = FVector3d::Zero();
	FVector3d AccumulatedNormal = FVector3d::Zero();
	FVector3d AxisHint = FVector3d::Zero();
	double AccumWeight = 0;
	
	if (MeshSelection.ElementType == EGeometryElementType::Face)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 TriangleID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsTriangle(TriangleID))
			{
				FVector3d Normal, Centroid; double Area; 
				Mesh.GetTriInfo(TriangleID, Normal, Area, Centroid);
				if (Normal.SquaredLength() > 0.9)
				{
					Area = FMath::Max(Area, 0.000001);
					AccumulatedOrigin += Area * Centroid;
					AccumulatedNormal += Area * Normal;
					AccumWeight += Area;
				}
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Edge)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			FMeshTriEdgeID TriEdgeID( FGeoSelectionID(EncodedID).GeometryID );
			int32 EdgeID = Mesh.IsTriangle(TriEdgeID.TriangleID) ? Mesh.GetTriEdge(TriEdgeID.TriangleID, TriEdgeID.TriEdgeIndex) : IndexConstants::InvalidID;
			if ( Mesh.IsEdge(EdgeID) )
			{
				FVector3d A, B;
				Mesh.GetEdgeV(EdgeID, A, B);
				AccumulatedOrigin += (A + B) * 0.5;
				AccumulatedNormal += Mesh.GetEdgeNormal(EdgeID);
				AxisHint += Normalized(B - A);
				AccumWeight += 1.0;
			}
		}
	}
	else if (MeshSelection.ElementType == EGeometryElementType::Vertex)
	{
		for (uint64 EncodedID : MeshSelection.Selection)
		{
			int32 VertexID = (int32)FGeoSelectionID(EncodedID).GeometryID;
			if (Mesh.IsVertex(VertexID))
			{
				AccumulatedOrigin += Mesh.GetVertex(VertexID);
				AccumulatedNormal += FMeshNormals::ComputeVertexNormal(Mesh, VertexID);		// this could return area!
				AccumWeight += 1.0;
			}
		}
	}
	else
	{
		return false;
	}

	// todo use AxisHint!

	SelectionFrameOut = FFrame3d();
	if (AccumWeight > 0)
	{
		AccumulatedOrigin /= (double)AccumWeight;
		Normalize(AccumulatedNormal);

		// We set our frame Z to be accumulated normal, and the other two axes are unconstrained, so
		// we want to set them to something that will make our frame generally more useful. If the normal
		// is aligned with world Z, then the entire frame might as well be aligned with world.
		if (1 - AccumulatedNormal.Dot(FVector3d::UnitZ()) < KINDA_SMALL_NUMBER)
		{
			SelectionFrameOut = FFrame3d(AccumulatedOrigin, FQuaterniond::Identity());
		}
		else
		{
			// Otherwise, let's place one of the other axes into the XY plane so that the frame is more
			// useful for translation. We somewhat arbitrarily choose Y for this. 
			FVector3d FrameY = Normalized(AccumulatedNormal.Cross(FVector3d::UnitZ())); // orthogonal to world Z and frame Z 
			FVector3d FrameX = FrameY.Cross(AccumulatedNormal); // safe to not normalize because already orthogonal
			SelectionFrameOut = FFrame3d(AccumulatedOrigin, FrameX, FrameY, AccumulatedNormal);
		}
	}

	return true;
}