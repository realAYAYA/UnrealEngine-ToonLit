// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "MeshSimplification.h"
#include "MeshConstraintsUtil.h"
#include "ProjectionTargets.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

enum class EMeshSimplifyType
{
	Standard = 0,
	VolumePreserving = 1,
	AttributeAware = 2
};

enum class EMeshSimplifyTargetType
{
	TriangleCount = 0,
	VertexCount = 1,
	TrianglePercentage = 2,
	GeometricDeviation = 3
};

struct FMeshSimplifySettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::SimplifySettings);

	EMeshSimplifyType SimplifyType = EMeshSimplifyType::AttributeAware;
	EMeshSimplifyTargetType TargetType = EMeshSimplifyTargetType::TrianglePercentage;

	int TargetCount = 100;
	float TargetFraction = 0.5;
	float GeometricTolerance = 0.5;

	bool bDiscardAttributes = false;
	bool bPreventNormalFlips = true;
	bool bPreserveSharpEdges = false;
	bool bAllowSeamCollapse = true;
	bool bAllowSeamSplits = true;

	EEdgeRefineFlags MeshBoundaryConstraints = EEdgeRefineFlags::NoConstraint;
	EEdgeRefineFlags GroupBorderConstraints = EEdgeRefineFlags::NoConstraint;
	EEdgeRefineFlags MaterialBorderConstraints = EEdgeRefineFlags::NoConstraint;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshSimplifySettings, Simplify);


class FSimplifyMeshNode : public TProcessMeshWithSettingsBaseNode<FMeshSimplifySettings>
{
public:
	FSimplifyMeshNode() : TProcessMeshWithSettingsBaseNode<FMeshSimplifySettings>()
	{
		// we can mutate input mesh
		ConfigureInputFlags(InParamMesh(), FNodeInputFlags::Transformable());
	}

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshSimplifySettings& Settings,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		MeshOut.Copy(MeshIn, true, true, true, !Settings.bDiscardAttributes);
		ApplySimplify(Settings, MeshOut, EvaluationInfo);
	}

	virtual void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshSimplifySettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		ApplySimplify(Settings, MeshInOut, EvaluationInfo);
	}


	template<typename SimplifierType> 
	void DoSimplifyOfType(const FMeshSimplifySettings& Settings, FDynamicMesh3* TargetMesh, TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		SimplifierType Simplifier(TargetMesh);

		if (EvaluationInfo && EvaluationInfo->Progress)
		{
			Simplifier.Progress = EvaluationInfo->Progress;
		}

		Simplifier.ProjectionMode = SimplifierType::ETargetProjectionMode::NoProjection;
		Simplifier.DEBUG_CHECK_LEVEL = 0;

		Simplifier.bAllowSeamCollapse = Settings.bAllowSeamCollapse;
		if (Simplifier.bAllowSeamCollapse)
		{
			Simplifier.SetEdgeFlipTolerance(1.e-5);

			// eliminate any bowties in attribute layers
			if (TargetMesh->Attributes())
			{
				TargetMesh->Attributes()->SplitAllBowties();
			}
		}

		FMeshConstraints Constraints;
		FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(Constraints, *TargetMesh,
			Settings.MeshBoundaryConstraints,
			Settings.GroupBorderConstraints,
			Settings.MaterialBorderConstraints,
			Settings.bAllowSeamSplits, !Settings.bPreserveSharpEdges, Settings.bAllowSeamCollapse);
		Simplifier.SetExternalConstraints(MoveTemp(Constraints));

		if (Settings.TargetType == EMeshSimplifyTargetType::TrianglePercentage)
		{
			int32 UseTarget = FMath::Max(4, (int)(Settings.TargetFraction * (double)TargetMesh->TriangleCount()));
			Simplifier.SimplifyToTriangleCount(UseTarget);
		}
		else if (Settings.TargetType == EMeshSimplifyTargetType::TriangleCount)
		{
			Simplifier.SimplifyToTriangleCount( FMath::Max(1,Settings.TargetCount) );
		}
		else if (Settings.TargetType == EMeshSimplifyTargetType::VertexCount)
		{
			Simplifier.SimplifyToVertexCount( FMath::Max(3, Settings.TargetCount) );
		}
		else if (Settings.TargetType == EMeshSimplifyTargetType::GeometricDeviation)
		{
			// need to create projection target to measure error
			FDynamicMesh3 MeshCopy;
			MeshCopy.Copy(*TargetMesh, false, false, false, false);
			FDynamicMeshAABBTree3 MeshCopySpatial(&MeshCopy, true);
			FMeshProjectionTarget ProjTarget(&MeshCopy, &MeshCopySpatial);
			Simplifier.SetProjectionTarget(&ProjTarget);

			Simplifier.GeometricErrorConstraint = SimplifierType::EGeometricErrorCriteria::PredictedPointToProjectionTarget;
			Simplifier.GeometricErrorTolerance = Settings.GeometricTolerance;
			Simplifier.SimplifyToVertexCount(3);
		}
		else
		{
			check(false);
		}
	}


	void ApplySimplify(const FMeshSimplifySettings& Settings, FDynamicMesh3& MeshInOut, TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		if (Settings.bDiscardAttributes)
		{
			MeshInOut.DiscardAttributes();
		}

		if (Settings.SimplifyType == EMeshSimplifyType::Standard)
		{
			DoSimplifyOfType<FQEMSimplification>(Settings, &MeshInOut, EvaluationInfo);
		}
		else if (Settings.SimplifyType == EMeshSimplifyType::VolumePreserving)
		{
			DoSimplifyOfType<FVolPresMeshSimplification>(Settings, &MeshInOut, EvaluationInfo);
		}
		else if (Settings.SimplifyType == EMeshSimplifyType::AttributeAware)
		{
			DoSimplifyOfType<FAttrMeshSimplification>(Settings, &MeshInOut, EvaluationInfo);
		}
		else
		{
			check(false);
		}
	}

};



}	// end namespace GeometryFlow
}	// end namespace UE