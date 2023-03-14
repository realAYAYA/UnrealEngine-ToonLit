// Copyright Epic Games, Inc. All Rights Reserved.

#include "CleaningOps/SimplifyMeshOp.h"

#include "MeshConstraints.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshAttributeUtil.h"
#include "MeshSimplification.h"
#include "MeshConstraintsUtil.h"
#include "ProjectionTargets.h"

#include "IMeshReductionInterfaces.h"
#include "MeshDescription.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "GroupTopology.h"
#include "Operations/PolygroupRemesh.h"
#include "ConstrainedDelaunay2.h"
#include "OverlappingCorners.h"
#include "StaticMeshOperations.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimplifyMeshOp)

using namespace UE::Geometry;

template <typename SimplificationType>
void ComputeSimplify(FDynamicMesh3* TargetMesh, const bool bReproject,
					 int OriginalTriCount, FDynamicMesh3& OriginalMesh, FDynamicMeshAABBTree3& OriginalMeshSpatial,
					 EEdgeRefineFlags MeshBoundaryConstraint,
					 EEdgeRefineFlags GroupBoundaryConstraint,
					 EEdgeRefineFlags MaterialBoundaryConstraint,
					 bool bPreserveSharpEdges, bool bAllowSeamCollapse,
					 const ESimplifyTargetType TargetMode,
					 const float TargetPercentage, const int TargetCount, const float TargetEdgeLength,
					 const float AngleThreshold,
	                 typename SimplificationType::ESimplificationCollapseModes CollapseMode,
					 bool bUseQuadricMemory,
					 float GeometricTolerance )
{
	SimplificationType Reducer(TargetMesh);

	Reducer.ProjectionMode = (bReproject) ?
		SimplificationType::ETargetProjectionMode::AfterRefinement : SimplificationType::ETargetProjectionMode::NoProjection;

	Reducer.DEBUG_CHECK_LEVEL = 0;
	//Reducer.ENABLE_PROFILING = true;

	Reducer.bAllowSeamCollapse = bAllowSeamCollapse;
	Reducer.bRetainQuadricMemory = bUseQuadricMemory;

	if (bAllowSeamCollapse)
	{
		Reducer.SetEdgeFlipTolerance(1.e-5);

		// eliminate any bowties that might have formed on UV seams.
		if (TargetMesh->Attributes())
		{
			TargetMesh->Attributes()->SplitAllBowties();
		}
	}

	FMeshConstraints constraints;
	FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(constraints, *TargetMesh,
														 MeshBoundaryConstraint,
														 GroupBoundaryConstraint,
														 MaterialBoundaryConstraint,
														 true, !bPreserveSharpEdges, bAllowSeamCollapse);
	Reducer.SetExternalConstraints(MoveTemp(constraints));
	
	// transfer constraint setting to the simplifier, these are used to update the constraints as edges collapse.	
	Reducer.MeshBoundaryConstraint = MeshBoundaryConstraint;
	Reducer.GroupBoundaryConstraint = GroupBoundaryConstraint;
	Reducer.MaterialBoundaryConstraint = MaterialBoundaryConstraint;
	
	if (TargetMode == ESimplifyTargetType::MinimalPlanar)
	{
		Reducer.CollapseMode = SimplificationType::ESimplificationCollapseModes::AverageVertexPosition;
		GeometricTolerance = 0;		// MinimalPlanar does not allow vertices to move off the input surface
	}
	else
	{
		Reducer.CollapseMode = CollapseMode;
	}

	// use projection target if we are reprojecting or doing geometric error checking
	FMeshProjectionTarget ProjTarget(&OriginalMesh, &OriginalMeshSpatial);
	if (bReproject || GeometricTolerance > 0)
	{
		Reducer.SetProjectionTarget(&ProjTarget);
	}

	// configure geometric error settings
	if (GeometricTolerance > 0)
	{
		Reducer.GeometricErrorConstraint = SimplificationType::EGeometricErrorCriteria::PredictedPointToProjectionTarget;
		Reducer.GeometricErrorTolerance = GeometricTolerance;
	}

	if (TargetMode == ESimplifyTargetType::Percentage)
	{
		double Ratio = (double)TargetPercentage / 100.0;
		int UseTarget = FMath::Max(4, (int)(Ratio * (double)OriginalTriCount));
		Reducer.SimplifyToTriangleCount(UseTarget);
	}
	else if (TargetMode == ESimplifyTargetType::TriangleCount)
	{
		Reducer.SimplifyToTriangleCount(TargetCount);
	}
	else if (TargetMode == ESimplifyTargetType::VertexCount)
	{
		Reducer.SimplifyToVertexCount(TargetCount);
	}
	else if (TargetMode == ESimplifyTargetType::EdgeLength)
	{
		Reducer.SimplifyToEdgeLength(TargetEdgeLength);
	}
	else if (TargetMode == ESimplifyTargetType::MinimalPlanar)
	{
		Reducer.SimplifyToMinimalPlanar(AngleThreshold);
	}
}



void FSimplifyMeshOp::CalculateResult(FProgressCancel* Progress)
{
	if (Progress && Progress->Cancelled())
	{
		return;
	}

	// Need access to the source mesh:
	FDynamicMesh3* TargetMesh = ResultMesh.Get();

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	int OriginalTriCount = OriginalMesh->TriangleCount();
	double UseGeometricTolerance = (bGeometricDeviationConstraint) ? GeometricTolerance : 0.0;
	if (SimplifierType == ESimplifyType::QEM)
	{
		bool bUseQuadricMemory = true;
		ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
		ComputeSimplify<FQEMSimplification>(TargetMesh, bReproject, OriginalTriCount, *OriginalMesh, *OriginalMeshSpatial,
											MeshBoundaryConstraint,
											GroupBoundaryConstraint,
											MaterialBoundaryConstraint,
											bPreserveSharpEdges, bAllowSeamCollapse,
											TargetMode, TargetPercentage, TargetCount, TargetEdgeLength, MinimalPlanarAngleThresh,
											FQEMSimplification::ESimplificationCollapseModes::MinimalQuadricPositionError, bUseQuadricMemory, 
											UseGeometricTolerance);
	}
	else if (SimplifierType == ESimplifyType::Attribute)
	{
		bool bUseQuadricMemory = false;
		ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
		if (!ResultMesh->HasAttributes() && !ResultMesh->HasVertexNormals())
		{
			FMeshNormals::QuickComputeVertexNormals(*ResultMesh, false);
		}
		ComputeSimplify<FAttrMeshSimplification>(TargetMesh, bReproject, OriginalTriCount, *OriginalMesh, *OriginalMeshSpatial,
													MeshBoundaryConstraint,
													GroupBoundaryConstraint,
													MaterialBoundaryConstraint,
													bPreserveSharpEdges, bAllowSeamCollapse,
													TargetMode, TargetPercentage, TargetCount, TargetEdgeLength, MinimalPlanarAngleThresh,
													FAttrMeshSimplification::ESimplificationCollapseModes::MinimalQuadricPositionError, bUseQuadricMemory,
													UseGeometricTolerance);
	}
	else if (SimplifierType == ESimplifyType::MinimalPlanar)
	{
		bool bUseQuadricMemory = false;
		ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
		if (!ResultMesh->HasAttributes() && !ResultMesh->HasVertexNormals())
		{
			FMeshNormals::QuickComputeVertexNormals(*ResultMesh, false);
		}
		ComputeSimplify<FQEMSimplification>(TargetMesh, bReproject, OriginalTriCount, *OriginalMesh, *OriginalMeshSpatial,
			MeshBoundaryConstraint,
			GroupBoundaryConstraint,
			MaterialBoundaryConstraint,
			bPreserveSharpEdges, bAllowSeamCollapse,
			ESimplifyTargetType::MinimalPlanar, TargetPercentage, TargetCount, TargetEdgeLength, MinimalPlanarAngleThresh,
			FQEMSimplification::ESimplificationCollapseModes::MinimalQuadricPositionError, bUseQuadricMemory,
			UseGeometricTolerance);
	}
	else if (SimplifierType == ESimplifyType::MinimalExistingVertex)
	{
		bool bUseQuadricMemory = true;
		ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
		ComputeSimplify<FQEMSimplification>(TargetMesh, bReproject, OriginalTriCount, *OriginalMesh, *OriginalMeshSpatial,
			MeshBoundaryConstraint,
			GroupBoundaryConstraint,
			MaterialBoundaryConstraint,
			bPreserveSharpEdges, bAllowSeamCollapse,
			TargetMode, TargetPercentage, TargetCount, TargetEdgeLength, MinimalPlanarAngleThresh, 
			FQEMSimplification::ESimplificationCollapseModes::MinimalExistingVertexError, bUseQuadricMemory,
			UseGeometricTolerance);
	}
	else if (SimplifierType == ESimplifyType::MinimalPolygroup)
	{
		ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
		FGroupTopology Topology(ResultMesh.Get(), true);
		if (Progress && Progress->Cancelled())
		{
			return;
		}

		FPolygroupRemesh Remesh(ResultMesh.Get(), &Topology, ConstrainedDelaunayTriangulate<double>);
		Remesh.SimplificationAngleTolerance = PolyEdgeAngleTolerance;
		Remesh.Compute();
	}
	else // SimplifierType == ESimplifyType::UEStandard
	{
		const FMeshDescription* SrcMeshDescription = OriginalMeshDescription.Get();
		FMeshDescription DstMeshDescription(*SrcMeshDescription);

		if (Progress && Progress->Cancelled())
		{
			return;
		}

		FOverlappingCorners OverlappingCorners;
		FStaticMeshOperations::FindOverlappingCorners(OverlappingCorners, *SrcMeshDescription, 1.e-5);

		if (Progress && Progress->Cancelled())
		{
			return;
		}

		FMeshReductionSettings ReductionSettings;
		if (TargetMode == ESimplifyTargetType::Percentage)
		{
			ReductionSettings.PercentTriangles = FMath::Max(TargetPercentage / 100., .001);  // Only support triangle percentage and count, but not edge length
			ReductionSettings.TerminationCriterion = EStaticMeshReductionTerimationCriterion::Triangles;
		}
		else if (TargetMode == ESimplifyTargetType::TriangleCount)
		{
			int32 NumTris = SrcMeshDescription->Polygons().Num();
			ReductionSettings.PercentTriangles = (float)TargetCount / (float)NumTris;
			ReductionSettings.TerminationCriterion = EStaticMeshReductionTerimationCriterion::Triangles;
		}
		else if (TargetMode == ESimplifyTargetType::VertexCount)
		{
			int32 NumVerts = SrcMeshDescription->Vertices().Num();
			ReductionSettings.PercentVertices = (float)TargetCount / (float)NumVerts;
			ReductionSettings.TerminationCriterion = EStaticMeshReductionTerimationCriterion::Vertices;
		}

		float Error;
		{
			if (!MeshReduction)
			{
				// no reduction possible, failed to load the required interface
				Error = 0.f;
				ResultMesh->Copy(*OriginalMesh, true, true, true, !bDiscardAttributes);
				return;
			}

			Error = ReductionSettings.MaxDeviation;
			MeshReduction->ReduceMeshDescription(DstMeshDescription, Error, *SrcMeshDescription, OverlappingCorners, ReductionSettings);
		}

		if (Progress && Progress->Cancelled())
		{
			return;
		}

		// Put the reduced mesh into the target...
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(&DstMeshDescription, *ResultMesh);
		if (bDiscardAttributes)
		{
			ResultMesh->DiscardAttributes();
		}


		bool bFailedModifyNeedsRegen = false;
		// The UEStandard simplifier will split the UV boundaries.  Need to weld this.
		{
			FDynamicMesh3* ComponentMesh = ResultMesh.Get();

			FMergeCoincidentMeshEdges Merger(ComponentMesh);
			Merger.MergeSearchTolerance = 10.0f * FMathf::ZeroTolerance;
			Merger.OnlyUniquePairs = false;
			if (Merger.Apply() == false)
			{
				bFailedModifyNeedsRegen = true;
			}

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			if (ResultMesh->CheckValidity(true, EValidityCheckFailMode::ReturnOnly) == false)
			{
				bFailedModifyNeedsRegen = true;
			}

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			// in the fallback case where merge edges failed, give up and reset it to what it was before the attempted merger (w/ split UV boundaries everywhere, oh well)
			if (bFailedModifyNeedsRegen)
			{
				ResultMesh->Clear();
				Converter.Convert(&DstMeshDescription, *ResultMesh);
				if (bDiscardAttributes)
				{
					ResultMesh->DiscardAttributes();
				}
			}
		}
	}


	if (Progress && Progress->Cancelled())
	{
		return;
	}

	if (!ResultMesh->HasAttributes())
	{
		FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
	}

	if (!TargetMesh->HasAttributes() && bResultMustHaveAttributesEnabled)
	{
		TargetMesh->EnableAttributes();
		if (TargetMesh->HasVertexUVs())
		{
			CopyVertexUVsToOverlay(*TargetMesh, *TargetMesh->Attributes()->PrimaryUV());
		}
		if (TargetMesh->HasVertexNormals())
		{
			CopyVertexNormalsToOverlay(*TargetMesh, *TargetMesh->Attributes()->PrimaryNormals());
		}
	}
}

