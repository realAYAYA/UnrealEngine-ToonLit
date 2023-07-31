// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

namespace UE
{
namespace GeometryFlow
{



struct GEOMETRYFLOWMESHPROCESSING_API FMeshRepackUVsSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::RepackUVsSettings);

	int32 UVLayer = 0;

	int32 TextureResolution = 512;
	int32 GutterSize = 1;
	bool bAllowFlips = false;

	FVector2f UVScale = FVector2f::One();
	FVector2f UVTranslation = FVector2f::Zero();
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshRepackUVsSettings, MeshRepackUVs);



class GEOMETRYFLOWMESHPROCESSING_API FMeshRepackUVsNode : public TProcessMeshWithSettingsBaseNode<FMeshRepackUVsSettings>
{
public:
	FMeshRepackUVsNode() : TProcessMeshWithSettingsBaseNode<FMeshRepackUVsSettings>()
	{
		// we can mutate input mesh
		ConfigureInputFlags(InParamMesh(), FNodeInputFlags::Transformable());
	}

	virtual void ProcessMesh(
		const FNamedDataMap& DatasIn,
		const FMeshRepackUVsSettings& Settings,
		const FDynamicMesh3& MeshIn,
		FDynamicMesh3& MeshOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		MeshOut = MeshIn;
		RepackUVsForMesh(MeshOut, Settings);
	}

	virtual void ProcessMeshInPlace(
		const FNamedDataMap& DatasIn,
		const FMeshRepackUVsSettings& Settings,
		FDynamicMesh3& MeshInOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo)
	{
		RepackUVsForMesh(MeshInOut, Settings);
	}

	void RepackUVsForMesh(FDynamicMesh3& EditMesh, const FMeshRepackUVsSettings& Settings);
};





}	// end namespace GeometryFlow
}	// end 