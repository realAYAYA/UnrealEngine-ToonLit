// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Implicit/Solidify.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct FMeshSolidifySettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::SolidifySettings);

	int VoxelResolution = 64;
	float WindingThreshold = 0.5;
	int SurfaceConvergeSteps = 5;
	float ExtendBounds = 2.0f;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshSolidifySettings, Solidify);


class FSolidifyMeshNode : public TProcessMeshWithSettingsBaseNode<FMeshSolidifySettings>
{
public:

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshSolidifySettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		ApplySolidify(MeshIn, SettingsIn, MeshOut);
	}

	void ApplySolidify(const FDynamicMesh3& MeshIn, const FMeshSolidifySettings& Settings, FDynamicMesh3& MeshOut)
	{
		FAxisAlignedBox3d Bounds = MeshIn.GetBounds();

		FDynamicMeshAABBTree3 MeshBVTree(&MeshIn);
		TFastWindingTree<FDynamicMesh3> FastWinding(&MeshBVTree);

		TImplicitSolidify<FDynamicMesh3> SolidifyCalc(&MeshIn, &MeshBVTree, &FastWinding);

		SolidifyCalc.SetCellSizeAndExtendBounds(Bounds, Settings.ExtendBounds, Settings.VoxelResolution);
		SolidifyCalc.WindingThreshold = Settings.WindingThreshold;
		SolidifyCalc.SurfaceSearchSteps = Settings.SurfaceConvergeSteps;
		SolidifyCalc.bSolidAtBoundaries = true;
		SolidifyCalc.ExtendBounds = Settings.ExtendBounds;

		MeshOut.Copy(&SolidifyCalc.Generate());
	}
};


}	// end namespace GeometryFlow
}	// end namespace UE