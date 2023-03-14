// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "MeshProcessingBaseNodes.h"

namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct GEOMETRYFLOWMESHPROCESSING_API FGenerateConvexHullMeshSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::GenerateConvexHullMeshSettings);
	bool bPrefilterVertices = true;
	int PrefilterGridResolution = 10;
};

GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FGenerateConvexHullMeshSettings, GenerateConvexHullMesh);


class GEOMETRYFLOWMESHPROCESSING_API FGenerateConvexHullMeshNode : public TProcessMeshWithSettingsBaseNode<FGenerateConvexHullMeshSettings>
{

public:

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FGenerateConvexHullMeshSettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		MakeConvexHullMesh(MeshIn, SettingsIn, MeshOut, EvaluationInfo);
	}

	EGeometryFlowResult MakeConvexHullMesh(const FDynamicMesh3& MeshIn,
		const FGenerateConvexHullMeshSettings& Settings,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo);

};

}	// end namespace GeometryFlow
}	// end namespace UE
