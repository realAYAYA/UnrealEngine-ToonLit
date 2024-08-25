// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshSimplifyFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "MeshSimplification.h"
#include "MeshConstraintsUtil.h"
#include "ProjectionTargets.h"
#include "Polygroups/PolygroupSet.h"
#include "GroupTopology.h"
#include "Operations/PolygroupRemesh.h"
#include "ConstrainedDelaunay2.h"

#include "UDynamicMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSimplifyFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshSimplifyFunctions"


UDynamicMesh* UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToPlanar(
	UDynamicMesh* TargetMesh,
	FGeometryScriptPlanarSimplifyOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToPlanar_InvalidInput", "ApplySimplifyToPlanar: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		FQEMSimplification Simplifier(&EditMesh);

		// todo: set up seam collapse etc?

		Simplifier.CollapseMode = FQEMSimplification::ESimplificationCollapseModes::AverageVertexPosition;
		Simplifier.SimplifyToMinimalPlanar( FMath::Max(0.00001, Options.AngleThreshold) );

		if (Options.bAutoCompact)
		{
			EditMesh.CompactInPlace();
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToPolygroupTopology(
	UDynamicMesh* TargetMesh,
	FGeometryScriptPolygroupSimplifyOptions Options,
	FGeometryScriptGroupLayer GroupLayer,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToPolygroupTopology_InvalidInput", "ApplySimplifyToPolygroupTopology: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		FPolygroupLayer InputGroupLayer{ GroupLayer.bDefaultLayer, GroupLayer.ExtendedLayerIndex };
		if (InputGroupLayer.CheckExists(&EditMesh) == false)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToPolygroupTopology_MissingGroups", "ApplySimplifyToPolygroupTopology: Target Polygroup Layer does not exist"));
			return;
		}

		TUniquePtr<FGroupTopology> Topo;
		if (GroupLayer.bDefaultLayer)
		{
			Topo = MakeUnique<FGroupTopology>(&EditMesh, true);
		}
		else
		{
			Topo = MakeUnique<FGroupTopology>(&EditMesh, EditMesh.Attributes()->GetPolygroupLayer(GroupLayer.ExtendedLayerIndex), true);
		}

		FPolygroupRemesh Simplifier(&EditMesh, Topo.Get(), ConstrainedDelaunayTriangulate<double>);
		Simplifier.SimplificationAngleTolerance = Options.AngleThreshold;
		Simplifier.Compute();

		if (Options.bAutoCompact)
		{
			EditMesh.CompactInPlace();
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}




template<typename SimplificationType>
void DoSimplifyMesh(
	FDynamicMesh3& EditMesh, 
	FGeometryScriptSimplifyMeshOptions Options,
	bool bTargetIsTriCount,
	int32 TargetCount,
	FMeshProjectionTarget* ProjectionTarget = nullptr,
	double GeometricTolerance = 0)
{
	SimplificationType Simplifier(&EditMesh);

	Simplifier.ProjectionMode = SimplificationType::ETargetProjectionMode::NoProjection;
	if (ProjectionTarget != nullptr)
	{
		Simplifier.SetProjectionTarget(ProjectionTarget);
	}

	Simplifier.DEBUG_CHECK_LEVEL = 0;
	Simplifier.bRetainQuadricMemory = Options.bRetainQuadricMemory; 
	Simplifier.bAllowSeamCollapse = Options.bAllowSeamCollapse;
	if (Options.bAllowSeamCollapse)
	{
		Simplifier.SetEdgeFlipTolerance(1.e-5);
		if (EditMesh.HasAttributes())
		{
			EditMesh.Attributes()->SplitAllBowties();	// eliminate any bowties that might have formed on attribute seams.
		}
	}

	// do these flags matter here since we are not flipping??
	EEdgeRefineFlags MeshBoundaryConstraints = EEdgeRefineFlags::NoFlip;
	EEdgeRefineFlags GroupBorderConstraints = EEdgeRefineFlags::NoConstraint;
	EEdgeRefineFlags MaterialBorderConstraints = EEdgeRefineFlags::NoConstraint;

	FMeshConstraints Constraints;
	FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(Constraints, EditMesh,
		MeshBoundaryConstraints, GroupBorderConstraints, MaterialBorderConstraints,
		Options.bAllowSeamSplits, Options.bAllowSeamSmoothing, Options.bAllowSeamCollapse);
	Simplifier.SetExternalConstraints(MoveTemp(Constraints));

	if ( ProjectionTarget != nullptr && GeometricTolerance > 0)
	{
		Simplifier.GeometricErrorConstraint = SimplificationType::EGeometricErrorCriteria::PredictedPointToProjectionTarget;
		Simplifier.GeometricErrorTolerance = GeometricTolerance;
	}

	if (bTargetIsTriCount)
	{
		Simplifier.SimplifyToTriangleCount( FMath::Max(1,TargetCount) );
	}
	else
	{
		Simplifier.SimplifyToVertexCount( FMath::Max(1,TargetCount) );
	}

	if (Options.bAutoCompact)
	{
		EditMesh.CompactInPlace();
	}
}



UDynamicMesh* UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
	UDynamicMesh* TargetMesh,
	int32 TriangleCount,
	FGeometryScriptSimplifyMeshOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToTriangleCount_InvalidInput", "ApplySimplifyToTriangleCount: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::AttributeAware)
		{
			DoSimplifyMesh<FAttrMeshSimplification>(EditMesh, Options, true, TriangleCount);
		}
		else if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::VolumePreserving)
		{
			DoSimplifyMesh<FVolPresMeshSimplification>(EditMesh, Options, true, TriangleCount);
		}
		else
		{
			DoSimplifyMesh<FQEMSimplification>(EditMesh, Options, true, TriangleCount);
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}




UDynamicMesh* UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToVertexCount(
	UDynamicMesh* TargetMesh,
	int32 VertexCount,
	FGeometryScriptSimplifyMeshOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToVertexCount_InvalidInput", "ApplySimplifyToVertexCount: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::AttributeAware)
		{
			DoSimplifyMesh<FAttrMeshSimplification>(EditMesh, Options, false, VertexCount);
		}
		else if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::VolumePreserving)
		{
			DoSimplifyMesh<FVolPresMeshSimplification>(EditMesh, Options, false, VertexCount);
		}
		else
		{
			DoSimplifyMesh<FQEMSimplification>(EditMesh, Options, false, VertexCount);
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}



UDynamicMesh* UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTolerance(
	UDynamicMesh* TargetMesh,
	float Tolerance,
	FGeometryScriptSimplifyMeshOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ApplySimplifyToTolerance_InvalidInput", "ApplySimplifyToTolerance: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
	{
		FDynamicMesh3 TempCopy;
		TempCopy.Copy(EditMesh, false, false, false, false);
		FDynamicMeshAABBTree3 Spatial(&TempCopy, true);
		FMeshProjectionTarget ProjTarget(&TempCopy, &Spatial);
		float UseTolerance = FMath::Max(0.0, Tolerance);

		if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::AttributeAware)
		{
			DoSimplifyMesh<FAttrMeshSimplification>(EditMesh, Options, true, 1, &ProjTarget, UseTolerance);
		}
		else if (Options.Method == EGeometryScriptRemoveMeshSimplificationType::VolumePreserving)
		{
			DoSimplifyMesh<FVolPresMeshSimplification>(EditMesh, Options, true, 1, &ProjTarget, UseTolerance);
		}
		else
		{
			DoSimplifyMesh<FQEMSimplification>(EditMesh, Options, true, 1, &ProjTarget, UseTolerance);
		}

	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	return TargetMesh;
}




#undef LOCTEXT_NAMESPACE
