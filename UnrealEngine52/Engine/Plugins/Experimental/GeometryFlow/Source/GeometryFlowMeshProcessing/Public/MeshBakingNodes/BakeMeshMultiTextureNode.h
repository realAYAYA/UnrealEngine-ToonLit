// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshBakingNodes/BakeMeshTextureImageNode.h"

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshTangentsNodes.h"
#endif
