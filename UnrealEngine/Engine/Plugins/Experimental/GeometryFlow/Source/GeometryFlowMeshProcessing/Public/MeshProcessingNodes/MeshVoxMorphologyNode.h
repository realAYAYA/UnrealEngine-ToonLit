// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Implicit/Morphology.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct FVoxMorphologyOpSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::VoxMorphologyOpSettings);

	int VoxelResolution = 64;
	double Distance = 1.0;
};
typedef FVoxMorphologyOpSettings FVoxOffsetSettings;
typedef FVoxMorphologyOpSettings FVoxClosureSettings;
typedef FVoxMorphologyOpSettings FVoxOpeningSettings;
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FVoxMorphologyOpSettings, VoxOffset);
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FVoxMorphologyOpSettings, VoxClosure);
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FVoxMorphologyOpSettings, VoxOpening);


template<TImplicitMorphology<FDynamicMesh3>::EMorphologyOp MorphologyOp>
class TVoxMorphologyMeshNode : public TProcessMeshWithSettingsBaseNode<FVoxMorphologyOpSettings>
{
public:


	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FVoxMorphologyOpSettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		ApplyMorphology(MeshIn, SettingsIn, MeshOut);
	}

	void ApplyMorphology(const FDynamicMesh3& MeshIn, const FVoxMorphologyOpSettings& Settings, FDynamicMesh3& MeshOut)
	{
		if (Settings.Distance == 0.0f)
		{
			MeshOut = MeshIn;
			return;
		}

		FAxisAlignedBox3d Bounds = MeshIn.GetBounds();
		FDynamicMeshAABBTree3 MeshBVTree(&MeshIn);

		TImplicitMorphology<FDynamicMesh3> ImplicitMorphology;
		if (MorphologyOp == TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Dilate && Settings.Distance < 0)
		{
			ImplicitMorphology.MorphologyOp = TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Contract;
		}
		else
		{
			ImplicitMorphology.MorphologyOp = MorphologyOp;
		}
		ImplicitMorphology.Source = &MeshIn;
		ImplicitMorphology.SourceSpatial = &MeshBVTree;
		ImplicitMorphology.SetCellSizesAndDistance(Bounds, Settings.Distance, Settings.VoxelResolution, Settings.VoxelResolution);

		MeshOut.Copy(&ImplicitMorphology.Generate());
	}

};

typedef TVoxMorphologyMeshNode<TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Dilate> FVoxDilateMeshNode;
typedef TVoxMorphologyMeshNode<TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Close> FVoxClosureMeshNode;
typedef TVoxMorphologyMeshNode<TImplicitMorphology<FDynamicMesh3>::EMorphologyOp::Open> FVoxOpeningMeshNode;


}	// end namespace GeometryFlow
}	// end namespace UE