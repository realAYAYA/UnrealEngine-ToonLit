// Copyright Epic Games, Inc. All Rights Reserved.


#include "Selection/DynamicMeshPolygroupTransformer.h"
#include "Selections/GeometrySelectionUtil.h"
#include "ToolContextInterfaces.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMesh/MeshNormals.h"
#include "Changes/MeshVertexChange.h"
#include "GroupTopology.h"
#include "Operations/GroupTopologyDeformer.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FDynamicMeshPolygroupTransformer"

void FDynamicMeshPolygroupTransformer::BeginTransform(const FGeometrySelection& Selection)
{
	if (!ensure(Selection.TopologyType == EGeometryTopologyType::Polygroup))
	{
		bFallbackToSimpleTransform = true;
		FBasicDynamicMeshSelectionTransformer::BeginTransform(Selection);
		return;
	}

	const FGroupTopology* UseTopology = Selector->GetGroupTopology();

	LinearDeformer = MakePimpl<FGroupTopologyDeformer>();

	TSet<int32> VertexIDs;
	Selector->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& SourceMesh)
	{
		// danger! this currently holds onto SourceMesh pointer...
		// Also this is expensive, would be nice if we could save it across multiple transforms...
		LinearDeformer->Initialize(&SourceMesh, UseTopology);

		LinearDeformer->SetActiveHandleFromSelection(Selection);

		// get set of selected vertex IDs, and then vector (only converting to vector for VertexToTriangleOneRing...)
		UE::Geometry::EnumeratePolygroupSelectionVertices(Selection, SourceMesh, UseTopology, FTransform::Identity,
			[&](uint32 VertexID, const FVector3d& Position) { VertexIDs.Add((int32)VertexID); }
		);
		MeshVertices = VertexIDs.Array();
		int32 NumVertices = MeshVertices.Num();
		InitialPositions.SetNum(NumVertices);
		for (int32 k = 0; k < NumVertices; ++k)
		{
			InitialPositions[k] = SourceMesh.GetVertex(MeshVertices[k]);
			ROIMap.Add(MeshVertices[k], k);
		}

		//UpdatedPositions = InitialPositions;
	});

	ActiveVertexChange = MakePimpl<FMeshVertexChangeBuilder>(EMeshVertexChangeComponents::VertexPositions | EMeshVertexChangeComponents::OverlayNormals);

	//UpdatePendingVertexChange(false);
	Selector->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& SourceMesh)
	{
		ActiveVertexChange->SaveVertices(&SourceMesh, LinearDeformer->GetModifiedVertices(), true);
		ActiveVertexChange->SaveOverlayNormals(&SourceMesh, LinearDeformer->GetModifiedOverlayNormals(), true);
	});
}

void FDynamicMeshPolygroupTransformer::UpdateTransform( 
	TFunctionRef<FVector3d(int32 VertexID, const FVector3d& InitialPosition, const FTransform& WorldTransform)> PositionTransformFunc  )
{
	if (bFallbackToSimpleTransform)
	{
		FBasicDynamicMeshSelectionTransformer::UpdateTransform(PositionTransformFunc);
		return;
	}


	int32 N = MeshVertices.Num();
	FTransform WorldTransform = Selector->GetLocalToWorldTransform();

	//for (int32 k = 0; k < N; ++k)
	//{
	//	UpdatedPositions[k] = PositionTransformFunc(MeshVertices[k], InitialPositions[k], WorldTransform);
	//}

	Selector->GetDynamicMesh()->EditMesh([&](FDynamicMesh3& EditMesh)
	{

		// UEditMeshPolygonsTool::ComputeUpdate_Gizmo() calls ClearSolution if the transform was zero...

		LinearDeformer->UpdateSolution(&EditMesh, [&](FDynamicMesh3* TargetMesh, int VertIdx)
		{
			if ( int32* ArrayIndex = ROIMap.Find(VertIdx) )
			{
				return PositionTransformFunc( VertIdx, InitialPositions[*ArrayIndex], WorldTransform );
			}
			else
			{
				return TargetMesh->GetVertex(VertIdx);
			}
		});


		// TODO: the right thing to do here would be to only recompute the modified overlay normals...but we have no utility for that??
		//FMeshNormals::QuickRecomputeOverlayNormals(EditMesh);

	}, EDynamicMeshChangeType::DeformationEdit,
		//EDynamicMeshAttributeChangeFlags::VertexPositions, false);
	   EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents, false);


	// why does simple deformer do this every time??
	//UpdatePendingVertexChange(false);
}


void FDynamicMeshPolygroupTransformer::EndTransform(IToolsContextTransactionsAPI* TransactionsAPI)
{
	if (bFallbackToSimpleTransform)
	{
		FBasicDynamicMeshSelectionTransformer::EndTransform(TransactionsAPI);
		return;
	}

	//UpdatePendingVertexChange(true);
	Selector->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& SourceMesh)
	{
		ActiveVertexChange->SaveVertices(&SourceMesh, LinearDeformer->GetModifiedVertices(), false);
		ActiveVertexChange->SaveOverlayNormals(&SourceMesh, LinearDeformer->GetModifiedOverlayNormals(), false);
	});

	if (TransactionsAPI != nullptr)
	{
		TUniquePtr<FToolCommandChange> Result = MoveTemp(ActiveVertexChange->Change);
		TransactionsAPI->AppendChange(Selector->GetDynamicMesh(), MoveTemp(Result), LOCTEXT("DynamicMeshTransformChange", "Transform"));
	}

	ActiveVertexChange.Reset();

	if (OnEndTransformFunc)
	{
		OnEndTransformFunc(TransactionsAPI);
	}
}


#undef LOCTEXT_NAMESPACE 