// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

namespace UE
{
namespace GeometryFlow
{

enum class ERecalculateUVsUnwrapType : uint8
{
	Auto = 0,
	ExpMap = 1,
	Conformal = 2
};


struct GEOMETRYFLOWMESHPROCESSING_API FMeshRecalculateUVsSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::RecalculateUVsSettings);

	ERecalculateUVsUnwrapType UnwrapType = ERecalculateUVsUnwrapType::Auto;
	int32 UVLayer = 0;

};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshRecalculateUVsSettings, MeshRecalculateUVs);



class GEOMETRYFLOWMESHPROCESSING_API FMeshRecalculateUVsNode : public TProcessMeshWithSettingsBaseNode<FMeshRecalculateUVsSettings>
{
public:
	FMeshRecalculateUVsNode() : TProcessMeshWithSettingsBaseNode<FMeshRecalculateUVsSettings>()
	{
		// we can mutate input mesh
		ConfigureInputFlags(InParamMesh(), FNodeInputFlags::Transformable());
	}

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshRecalculateUVsSettings& Settings,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		MeshOut = MeshIn;
		RecalculateUVsOnMesh(MeshOut, Settings);
	}

	virtual void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshRecalculateUVsSettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		RecalculateUVsOnMesh(MeshInOut, Settings);
	}

	void RecalculateUVsOnMesh(FDynamicMesh3& EditMesh, const FMeshRecalculateUVsSettings& Settings);
};





}	// end namespace GeometryFlow
}	// end 