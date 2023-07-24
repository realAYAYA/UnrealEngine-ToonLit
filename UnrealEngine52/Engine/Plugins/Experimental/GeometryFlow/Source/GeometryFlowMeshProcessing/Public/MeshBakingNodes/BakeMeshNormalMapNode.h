// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataTypes/MeshImageBakingData.h"
#include "DataTypes/NormalMapData.h"
#include "DynamicMesh/MeshTangents.h"


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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshTangentsNodes.h"
#endif
