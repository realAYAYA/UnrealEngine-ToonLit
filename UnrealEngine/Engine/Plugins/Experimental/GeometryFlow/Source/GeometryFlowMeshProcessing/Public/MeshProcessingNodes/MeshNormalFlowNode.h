// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "MeshProcessingNodes/MeshSimplifyNode.h"
#include "Remesher.h"

namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct FMeshNormalFlowSettings : public FMeshSimplifySettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::NormalFlowSettings);

	int MaxRemeshIterations = 20;
	int NumExtraProjectionIterations = 5;
	bool bFlips = true;
	bool bSplits = true;
	bool bCollapses = true;
	FRemesher::ESmoothTypes SmoothingType = FRemesher::ESmoothTypes::Uniform;
	float SmoothingStrength = 0.25f;
};

GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshNormalFlowSettings, MeshNormalFlow);


class FMeshNormalFlowNode : public TProcessMeshWithSettingsBaseNode<FMeshNormalFlowSettings>
{
public:

	// Beside the intput mesh and settings, we also take a "target mesh" that will be the projection target.
	static const FString InParamTargetMesh() { return TEXT("TargetMesh"); }

	FMeshNormalFlowNode() : TProcessMeshWithSettingsBaseNode<FMeshNormalFlowSettings>()
	{
		AddInput(InParamTargetMesh(), MakeUnique<FDynamicMeshInput>());
	}

	void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshNormalFlowSettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

	void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshNormalFlowSettings& SettingsIn,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

	void CheckAdditionalInputs(const FNamedDataMap& DatasIn, 
							   bool& bRecomputeRequired, 
							   bool& bAllInputsValid) override;


protected:

	void DoNormalFlow(const FMeshNormalFlowSettings& SettingsIn,
					  const FDynamicMesh3& ProjectionTargetMesh,
					  bool bAttributesHaveBeenDiscarded,
					  FDynamicMesh3& EditMesh);

};


}	// end namespace GeometryFlow
}	// end namespace UE
