// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshProcessingNodes/MeshProcessingBaseNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "MeshProcessingNodes/MeshTangentsNodes.h"
#include "DataTypes/MeshImageBakingData.h"
#include "DataTypes/TextureImageData.h"


namespace UE
{
namespace GeometryFlow
{


struct GEOMETRYFLOWMESHPROCESSING_API FBakeMeshTextureImageSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::BakeTextureImageSettings);

	int32 DetailUVLayer = 0;

	double MaxDistance = 0.0;
};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FBakeMeshTextureImageSettings, BakeMeshTextureImage);



class GEOMETRYFLOWMESHPROCESSING_API FBakeMeshTextureImageNode : public FNode
{
protected:
	using SettingsDataType = TMovableData<FBakeMeshTextureImageSettings, FBakeMeshTextureImageSettings::DataTypeIdentifier>;

public:
	static const FString InParamBakeCache() { return TEXT("BakeCache"); }
	static const FString InParamImage() { return TEXT("TextureImage"); }
	static const FString InParamSettings() { return TEXT("Settings"); }

	static const FString OutParamTextureImage() { return TEXT("TextureImage"); }

public:
	FBakeMeshTextureImageNode()
	{
		AddInput(InParamBakeCache(), MakeUnique<TImmutableNodeInput<FMeshBakingCache, FMeshBakingCache::DataTypeIdentifier>>());
		AddInput(InParamImage(), MakeUnique<TBasicNodeInput<FTextureImage, FTextureImage::DataTypeIdentifier>>());
		AddInput(InParamSettings(), MakeUnique<TBasicNodeInput<FBakeMeshTextureImageSettings, FBakeMeshTextureImageSettings::DataTypeIdentifier>>());

		AddOutput(OutParamTextureImage(), MakeUnique<TBasicNodeOutput<FTextureImage, FTextureImage::DataTypeIdentifier>>());
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;
};






}	// end namespace GeometryFlow
}	// end namespace UE