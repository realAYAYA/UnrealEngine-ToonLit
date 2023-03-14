// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "GeometryFlowImmutableData.h"
#include "DataTypes/DynamicMeshData.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Sampling/MeshImageBakingCache.h"

namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct GEOMETRYFLOWMESHPROCESSING_API FMeshBakingCache
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::BakingCache);

	// have to make copies of these - boo!
	FDynamicMesh3 DetailMesh;
	FDynamicMeshAABBTree3 DetailSpatial;
	FDynamicMesh3 TargetMesh;

	UE::Geometry::FMeshImageBakingCache BakeCache;
};


typedef TImmutableData<FMeshBakingCache, FMeshBakingCache::DataTypeIdentifier> FMeshBakingCacheData;




struct GEOMETRYFLOWMESHPROCESSING_API FMeshMakeBakingCacheSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::MakeBakingCacheSettings);

	FImageDimensions Dimensions;
	int UVLayer = 0;
	float Thickness = 0.1;

};
GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FMeshMakeBakingCacheSettings, MeshMakeBakingCache);




class GEOMETRYFLOWMESHPROCESSING_API FMakeMeshBakingCacheNode : public FNode
{
protected:
	using SettingsDataType = TMovableData<FMeshMakeBakingCacheSettings, FMeshMakeBakingCacheSettings::DataTypeIdentifier>;

public:
	static const FString InParamDetailMesh() { return TEXT("DetailMesh"); }
	static const FString InParamTargetMesh() { return TEXT("TargetMesh"); }
	static const FString InParamSettings() { return TEXT("Settings"); }

	static const FString OutParamCache() { return TEXT("BakeCache"); }

public:
	FMakeMeshBakingCacheNode()
	{
		AddInput(InParamDetailMesh(), MakeUnique<FDynamicMeshInput>());
		AddInput(InParamTargetMesh(), MakeUnique<FDynamicMeshInput>());
		AddInput(InParamSettings(), MakeUnique<TBasicNodeInput<FMeshMakeBakingCacheSettings, FMeshMakeBakingCacheSettings::DataTypeIdentifier>>());

		AddOutput(OutParamCache(), MakeUnique<TImmutableNodeOutput<FMeshBakingCache, FMeshBakingCache::DataTypeIdentifier>>() );
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;
};








}	// end namespace GeometryFlow
}	// end 