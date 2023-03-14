// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "DynamicMesh/MeshNormals.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

enum class EComputeNormalsType
{
	PerTriangle = 0,
	PerVertex = 1,
	RecomputeExistingTopology = 2,
	FromFaceAngleThreshold = 3,
	FromGroups = 4
};

struct FMeshNormalsSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::NormalsSettings);

	EComputeNormalsType NormalsType = EComputeNormalsType::FromFaceAngleThreshold;

	bool bInvert = false;

	bool bAreaWeighted = true;
	bool bAngleWeighted = true;

	// for FromAngleThreshold type
	double AngleThresholdDeg = 180.0;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshNormalsSettings, Normals);


/**
 * Recompute Normals overlay for input Mesh. Can apply in-place.
 */
class FComputeMeshNormalsNode : public TProcessMeshWithSettingsBaseNode<FMeshNormalsSettings>
{
public:
	FComputeMeshNormalsNode() : TProcessMeshWithSettingsBaseNode<FMeshNormalsSettings>()
	{
		// we can mutate input mesh
		ConfigureInputFlags(InParamMesh(), FNodeInputFlags::Transformable());
	}


	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshNormalsSettings& Settings,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		MeshOut = MeshIn;
		ComputeNormals(Settings, MeshOut);
	}

	virtual void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshNormalsSettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		ComputeNormals(Settings, MeshInOut);
	}


	void ComputeNormals(const FMeshNormalsSettings& Settings, FDynamicMesh3& MeshInOut)
	{
		if (MeshInOut.HasAttributes() == false)
		{
			MeshInOut.EnableAttributes();
		}
		FDynamicMeshNormalOverlay* Normals = MeshInOut.Attributes()->PrimaryNormals();
		if (Normals == nullptr)
		{
			MeshInOut.Attributes()->SetNumNormalLayers(1);
			Normals = MeshInOut.Attributes()->PrimaryNormals();
		}

		if (Settings.NormalsType == EComputeNormalsType::PerTriangle)
		{
			ensure(Settings.bInvert == false);		// not supported
			FMeshNormals::InitializeMeshToPerTriangleNormals(&MeshInOut);
			return;
		}
		else if (Settings.NormalsType == EComputeNormalsType::PerVertex)
		{
			ensure(Settings.bInvert == false);		// not supported
			FMeshNormals::InitializeOverlayToPerVertexNormals(Normals, false);
			return;
		}


		if (Settings.NormalsType == EComputeNormalsType::FromFaceAngleThreshold)
		{
			FMeshNormals::InitializeOverlayTopologyFromOpeningAngle(&MeshInOut, Normals, Settings.AngleThresholdDeg);
		}
		else if (Settings.NormalsType == EComputeNormalsType::FromGroups)
		{
			FMeshNormals::InitializeOverlayTopologyFromFaceGroups(&MeshInOut, Normals);
		}
		else if (Settings.NormalsType != EComputeNormalsType::RecomputeExistingTopology)
		{
			ensure(false);
		}

		FMeshNormals MeshNormals(&MeshInOut);
		MeshNormals.RecomputeOverlayNormals(Normals, Settings.bAreaWeighted, Settings.bAngleWeighted);
		MeshNormals.CopyToOverlay(Normals, Settings.bInvert);
	}

};



/**
 * Recompute per-vertex normals in Normals Overlay for input mesh. Can apply in-place.
 */
class FComputeMeshPerVertexOverlayNormalsNode : public FSimpleInPlaceProcessMeshBaseNode
{
public:
	virtual void ApplyNodeToMesh(FDynamicMesh3& MeshInOut, TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		if (MeshInOut.HasAttributes() == false)
		{
			MeshInOut.EnableAttributes();
		}
		FDynamicMeshNormalOverlay* Normals = MeshInOut.Attributes()->PrimaryNormals();
		if (Normals == nullptr)
		{
			MeshInOut.Attributes()->SetNumNormalLayers(1);
			Normals = MeshInOut.Attributes()->PrimaryNormals();
		}
		FMeshNormals::InitializeOverlayToPerVertexNormals(Normals, false);
	}
};



/**
 * Recompute per-vertex normals stored directly on the mesh. Can apply in-place.
 */
class FComputeMeshPerVertexNormalsNode : public FSimpleInPlaceProcessMeshBaseNode
{
public:
	virtual void ApplyNodeToMesh(FDynamicMesh3& MeshInOut, TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		FMeshNormals::QuickComputeVertexNormals(MeshInOut, false);
	}
};




}	// end namespace GeometryFlow
}	// end namespace UE