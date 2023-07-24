// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformationOps/LatticeDeformerOp.h"
#include "Operations/FFDLattice.h"

using namespace UE::Geometry;

void FLatticeDeformerOp::CalculateResult(FProgressCancel* Progress)
{
	if (Progress && Progress->Cancelled())
	{
		return;
	}

	ResultMesh->Copy(*OriginalMesh);

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	TArray<FVector3d> DeformedPositions;
	FLatticeExecutionInfo ExecutionInfo = FLatticeExecutionInfo();
	ExecutionInfo.bParallel = true;
	Lattice->GetDeformedMeshVertexPositions(LatticeControlPoints, DeformedPositions, InterpolationType, ExecutionInfo, Progress);
	check(ResultMesh->VertexCount() == DeformedPositions.Num());

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	for (int vid : ResultMesh->VertexIndicesItr())
	{
		ResultMesh->SetVertex(vid, DeformedPositions[vid]);
	}

	if (bDeformNormals)
	{
		if (ResultMesh->HasAttributes())
		{
			FDynamicMeshNormalOverlay* NormalOverlay = ResultMesh->Attributes()->PrimaryNormals();
			check(NormalOverlay != nullptr);

			TArray<FVector3f> DeformedNormals;
			Lattice->GetRotatedOverlayNormals(LatticeControlPoints,
											  NormalOverlay,
											  DeformedNormals,
											  InterpolationType,
											  ExecutionInfo,
											  Progress);

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			for (int ElementID : NormalOverlay->ElementIndicesItr())
			{
				NormalOverlay->SetElement(ElementID, DeformedNormals[ElementID]);
			}
		}
		else if (ResultMesh->HasVertexNormals())
		{
			TArray<FVector3f> OriginalNormals;
			OriginalNormals.SetNum(ResultMesh->MaxVertexID());
			for (int VertexID : ResultMesh->VertexIndicesItr())
			{
				OriginalNormals[VertexID] = ResultMesh->GetVertexNormal(VertexID);
			}

			TArray<FVector3f> RotatedNormals;
			Lattice->GetRotatedMeshVertexNormals(LatticeControlPoints,
												 OriginalNormals,
												 RotatedNormals,
												 InterpolationType,
												 ExecutionInfo,
												 Progress);

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			for (int vid : ResultMesh->VertexIndicesItr())
			{
				ResultMesh->SetVertexNormal(vid, RotatedNormals[vid]);
			}
		}
	}
}

FLatticeDeformerOp::FLatticeDeformerOp(TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> InOriginalMesh,
									   TSharedPtr<FFFDLattice, ESPMode::ThreadSafe> InLattice,
									   const TArray<FVector3d>& InLatticeControlPoints,
									   ELatticeInterpolation InInterpolationType,
									   bool bInDeformNormals) :
	Lattice(InLattice),
	OriginalMesh(InOriginalMesh),
	LatticeControlPoints(InLatticeControlPoints),
	InterpolationType(InInterpolationType),
	bDeformNormals(bInDeformNormals)
{}
