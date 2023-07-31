// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "MeshProcessingNodes/MeshTangentsNodes.h"
#include "MeshBakingNodes/BakeMeshTextureImageNode.h"
#include "DataTypes/MeshImageBakingData.h"
#include "DataTypes/TextureImageData.h"

class UTexture2D;

namespace UE
{
namespace GeometryFlow
{

struct GEOMETRYFLOWMESHPROCESSING_API FBakeMeshMultiTextureSettings : public FBakeMeshTextureImageSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::BakeMultiTextureSettings);
};

GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FBakeMeshMultiTextureSettings, BakeMeshMultiTexture);


class GEOMETRYFLOWMESHPROCESSING_API FBakeMeshMultiTextureNode : public FNode
{
protected:
	using SettingsDataType = TMovableData<FBakeMeshMultiTextureSettings, FBakeMeshMultiTextureSettings::DataTypeIdentifier>;

public:
	static const FString InParamBakeCache() { return TEXT("BakeCache"); }
	static const FString InParamMaterialTextures() { return TEXT("MaterialIDToTextureMap"); }
	static const FString InParamSettings() { return TEXT("Settings"); }
	static const FString OutParamTextureImage() { return TEXT("TextureImage"); }

public:
	FBakeMeshMultiTextureNode()
	{
		AddInput(InParamBakeCache(), MakeUnique<TImmutableNodeInput<FMeshBakingCache, FMeshBakingCache::DataTypeIdentifier>>());
		AddInput(InParamMaterialTextures(), MakeUnique<TBasicNodeInput<FMaterialIDToTextureMap, FMaterialIDToTextureMap::DataTypeIdentifier>>());
		AddInput(InParamSettings(), MakeUnique<TBasicNodeInput<FBakeMeshMultiTextureSettings, FBakeMeshMultiTextureSettings::DataTypeIdentifier>>());
		AddOutput(OutParamTextureImage(), MakeUnique<TBasicNodeOutput<FTextureImage, FTextureImage::DataTypeIdentifier>>());
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;
};

}	// end namespace GeometryFlow
}	// end namespace UE