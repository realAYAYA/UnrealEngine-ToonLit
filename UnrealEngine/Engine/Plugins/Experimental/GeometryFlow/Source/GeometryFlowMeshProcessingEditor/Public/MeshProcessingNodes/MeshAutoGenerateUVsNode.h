// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypesEditor.h"

namespace UE
{
namespace GeometryFlow
{


enum class EAutoUVMethod : uint8
{
	PatchBuilder = 0,
	UVAtlas = 1,
	XAtlas = 2
};



struct GEOMETRYFLOWMESHPROCESSINGEDITOR_API FMeshAutoGenerateUVsSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypesEditor::MeshAutoGenerateUVsSettings);

	EAutoUVMethod Method = EAutoUVMethod::PatchBuilder;

	// UVAtlas parameters
	double UVAtlasStretch = 0.5;
	int UVAtlasNumCharts = 0;

	// XAtlas parameters
	int XAtlasMaxIterations = 1;

	// PatchBuilder parameters
	int NumInitialPatches = 100;
	double CurvatureAlignment = 1.0;
	double MergingThreshold = 1.5;
	double MaxAngleDeviationDeg = 45.0;
	int SmoothingSteps = 5;
	double SmoothingAlpha = 0.25;

	bool bAutoPack = false;
	int PackingTargetWidth = 512;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshAutoGenerateUVsSettings, MeshAutoGenerateUVs);



class GEOMETRYFLOWMESHPROCESSINGEDITOR_API FMeshAutoGenerateUVsNode : public TProcessMeshWithSettingsBaseNode<FMeshAutoGenerateUVsSettings>
{
public:
	FMeshAutoGenerateUVsNode() : TProcessMeshWithSettingsBaseNode<FMeshAutoGenerateUVsSettings>()
	{
	}

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshAutoGenerateUVsSettings& Settings,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		GenerateUVs(MeshIn, Settings, MeshOut, EvaluationInfo);
	}

	virtual void GenerateUVs(const FDynamicMesh3& MeshIn, 
		const FMeshAutoGenerateUVsSettings& Settings, 
		FDynamicMesh3& MeshOut, 
		TUniquePtr<FEvaluationInfo>& EvaluationInfo);

};





}	// end namespace GeometryFlow
}	// end namespace UE