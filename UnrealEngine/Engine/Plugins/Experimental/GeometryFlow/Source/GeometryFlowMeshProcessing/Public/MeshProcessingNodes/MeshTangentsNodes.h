// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseNodes/TransformerWithSettingsNode.h"
#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "DynamicMesh/MeshTangents.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

enum class EComputeTangentsType
{
	PerTriangle = 0,
	FastMikkT = 1
};


struct FMeshTangentsSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::TangentsSettings);

	EComputeTangentsType TangentsType = EComputeTangentsType::FastMikkT;

	int UVLayer = 0;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshTangentsSettings, Tangents);


typedef TTransferNode<FMeshTangentsd, (int)EMeshProcessingDataTypes::MeshTangentSet> FMeshTangentsTransferNode;


class FComputeMeshTangentsNode : public TTransformerWithSettingsNode<
	FDynamicMesh3, (int)EMeshProcessingDataTypes::DynamicMesh,
	FMeshTangentsSettings, (int)EMeshProcessingDataTypes::TangentsSettings,
	FMeshTangentsd, (int)EMeshProcessingDataTypes::MeshTangentSet>
{
public:
	FComputeMeshTangentsNode() : TTransformerWithSettingsNode()
	{
	}

	virtual void ComputeOutput(
		const FNamedDataMap& DatasIn,
		const FMeshTangentsSettings& Settings,
		const FDynamicMesh3& MeshIn,
		FMeshTangentsd& TangentsOut)
	{
		if (ensure(MeshIn.HasAttributes()) == false) return;

		// handle UV layer issues, missing attributes, etc
		int UseUVLayer = Settings.UVLayer;
		bool bValidUVLayer = (UseUVLayer >= 0 && UseUVLayer < MeshIn.Attributes()->NumUVLayers());
		if (ensure(bValidUVLayer) == false)
		{
			UseUVLayer = FMath::Clamp(UseUVLayer, 0, MeshIn.Attributes()->NumUVLayers() - 1);
		}

		FComputeTangentsOptions Options;
		Options.bAveraged = (Settings.TangentsType == EComputeTangentsType::FastMikkT);

		TangentsOut.SetMesh(&MeshIn);
		TangentsOut.ComputeTriVertexTangents(
			MeshIn.Attributes()->PrimaryNormals(),
			MeshIn.Attributes()->GetUVLayer(UseUVLayer),
			Options);

		// clear output mesh reference
		TangentsOut.SetMesh(nullptr);
	}


};





}	// end namespace GeometryFlow
}	// end namespace UE