// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmoothingOps/SmoothingOpBase.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"

using namespace UE::Geometry;

FSmoothingOpBase::FSmoothingOpBase(const FDynamicMesh3* Mesh, const FOptions& OptionsIn) :
	FDynamicMeshOperator(),
	SmoothOptions(OptionsIn)
{
	// deep copy the src mesh into the result mesh.  This ResultMesh will be directly updated by the smoothing.
	ResultMesh->Copy(*Mesh);
	
	PositionBuffer.SetNum(ResultMesh->MaxVertexID());

	for (int vid : ResultMesh->VertexIndicesItr())
	{
		PositionBuffer[vid] = ResultMesh->GetVertex(vid);
	}
}

void FSmoothingOpBase::SetTransform(const FTransformSRT3d& XForm)
{
	ResultTransform = XForm;
}


void FSmoothingOpBase::UpdateResultMesh()
{
	// move the verts to the new location
	for (int32 vid : ResultMesh->VertexIndicesItr())
	{
		const FVector3d Pos = PositionBuffer[vid];
		ResultMesh->SetVertex(vid, Pos);
	}

	// recalculate normals
	if (ResultMesh->HasAttributes())
	{
		FMeshNormals Normals(ResultMesh.Get());
		FDynamicMeshNormalOverlay* NormalOverlay = ResultMesh->Attributes()->PrimaryNormals();
		Normals.RecomputeOverlayNormals(NormalOverlay);
		Normals.CopyToOverlay(NormalOverlay);
	}
	else
	{
		FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	}
}
