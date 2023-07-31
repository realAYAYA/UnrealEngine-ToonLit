// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "MeshProcessingNodes/MeshTangentsNodes.h"
#include "DataTypes/MeshImageBakingData.h"
#include "DataTypes/NormalMapData.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct GEOMETRYFLOWMESHPROCESSING_API FBakeMeshNormalMapSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::BakeNormalMapSettings);

	double MaxDistance = 0.0;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FBakeMeshNormalMapSettings, BakeMeshNormalMap);



class GEOMETRYFLOWMESHPROCESSING_API FBakeMeshNormalMapNode : public FNode
{
protected:
	using SettingsDataType = TMovableData<FBakeMeshNormalMapSettings, FBakeMeshNormalMapSettings::DataTypeIdentifier>;

public:
	static const FString InParamBakeCache() { return TEXT("BakeCache"); }
	static const FString InParamTangents() { return TEXT("Tangents"); }
	static const FString InParamSettings() { return TEXT("Settings"); }

	static const FString OutParamNormalMap() { return TEXT("NormalMap"); }

public:
	FBakeMeshNormalMapNode()
	{
		AddInput(InParamBakeCache(), MakeUnique<TImmutableNodeInput<FMeshBakingCache, FMeshBakingCache::DataTypeIdentifier>>());
		AddInput(InParamTangents(), MakeUnique<TBasicNodeInput<FMeshTangentsd, (int)EMeshProcessingDataTypes::MeshTangentSet>>());
		AddInput(InParamSettings(), MakeUnique<TBasicNodeInput<FBakeMeshNormalMapSettings, FBakeMeshNormalMapSettings::DataTypeIdentifier>>());

		AddOutput(OutParamNormalMap(), MakeUnique<TBasicNodeOutput<FNormalMapImage, FNormalMapImage::DataTypeIdentifier>>());
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;
};






}	// end namespace GeometryFlow
}	// end namespace UE