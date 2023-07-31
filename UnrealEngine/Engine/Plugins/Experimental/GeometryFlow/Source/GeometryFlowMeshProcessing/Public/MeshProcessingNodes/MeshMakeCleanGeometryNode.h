// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

namespace UE
{
namespace GeometryFlow
{

struct GEOMETRYFLOWMESHPROCESSING_API FMeshMakeCleanGeometrySettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::MakeCleanGeometrySettings);

	int FillHolesEdgeCountThresh = 8;
	double FillHolesEstimatedAreaFraction = 0.001;

	bool bDiscardAllAttributes = false;
	bool bClearUVs = true;
	bool bClearNormals = true;
	bool bClearTangents = true;
	bool bClearVertexColors = true;
	bool bClearMaterialIDs = false;

	bool bOutputMeshVertexNormals = true;
	bool bOutputOverlayVertexNormals = true;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshMakeCleanGeometrySettings, MeshMakeCleanGeometry);


class GEOMETRYFLOWMESHPROCESSING_API FMeshMakeCleanGeometryNode : public TProcessMeshWithSettingsBaseNode<FMeshMakeCleanGeometrySettings>
{
public:

	void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshMakeCleanGeometrySettings& SettingsIn,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

	void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshMakeCleanGeometrySettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

protected:

	void ApplyMakeCleanGeometry(FDynamicMesh3& MeshInOut, const FMeshMakeCleanGeometrySettings& Settings);
	
};


}	// end namespace GeometryFlow
}	// end namespace UE
