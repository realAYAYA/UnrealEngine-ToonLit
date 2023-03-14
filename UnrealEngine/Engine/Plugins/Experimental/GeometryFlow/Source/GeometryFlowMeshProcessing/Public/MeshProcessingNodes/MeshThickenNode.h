// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "DataTypes/WeightMapData.h"

namespace UE
{
namespace GeometryFlow
{

struct GEOMETRYFLOWMESHPROCESSING_API FMeshThickenSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::ThickenSettings);

	float ThickenAmount = 1.0f;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshThickenSettings, Thicken);


class GEOMETRYFLOWMESHPROCESSING_API FMeshThickenNode : public TProcessMeshWithSettingsBaseNode<FMeshThickenSettings>
{
public:

	static const FString InParamWeightMap() { return TEXT("WeightMap"); }

	FMeshThickenNode()
	{
		AddInput(InParamWeightMap(), MakeUnique<FWeightMapInput>());
	}

	void CheckAdditionalInputs(const FNamedDataMap& DatasIn,
							   bool& bRecomputeRequired,
							   bool& bAllInputsValid) override
	{
		FindAndUpdateInputForEvaluate(InParamWeightMap(), DatasIn, bRecomputeRequired, bAllInputsValid);
	}

	void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshThickenSettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

	void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshThickenSettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

protected:

	void ApplyThicken(FDynamicMesh3& MeshInOut, const FMeshThickenSettings& Settings, const TArray<float>& VertexWeights);
	
};


}	// end namespace GeometryFlow
}	// end namespace UE
