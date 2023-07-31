// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "GeometryFlowNodeUtil.h"
#include "ShapeApproximation/MeshSimpleShapeApproximation.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "DataTypes/DynamicMeshData.h"
#include "DataTypes/CollisionGeometryData.h"
#include "DataTypes/IndexSetsData.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

enum class ESimpleCollisionGeometryType : uint8
{
	// NOTE: This must be kept in sync with EGenerateStaticMeshLODSimpleCollisionGeometryType in GenerateStaticMeshLODProcess.h

	AlignedBoxes,
	OrientedBoxes,
	MinimalSpheres,
	Capsules,
	ConvexHulls,
	SweptHulls,
	MinVolume,
	None
};


struct GEOMETRYFLOWMESHPROCESSING_API FGenerateSimpleCollisionSettings
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::GenerateSimpleCollisionSettings);

	ESimpleCollisionGeometryType Type = ESimpleCollisionGeometryType::ConvexHulls;

	struct FGenerateConvexHullSettings
	{
		int32 SimplifyToTriangleCount = 50;
		bool bPrefilterVertices = true;
		int PrefilterGridResolution = 10;
	} ConvexHullSettings;

	struct FGenerateSweptHullSettings
	{
		bool bSimplifyPolygons = true;
		FMeshSimpleShapeApproximation::EProjectedHullAxisMode SweepAxis = FMeshSimpleShapeApproximation::EProjectedHullAxisMode::SmallestVolume;
		float HullTolerance = 0.1;
	} SweptHullSettings;
};

GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(FGenerateSimpleCollisionSettings, GenerateSimpleCollision);


class GEOMETRYFLOWMESHPROCESSING_API FGenerateSimpleCollisionNode : public FNode
{
protected:
	using SettingsDataType = TMovableData<FGenerateSimpleCollisionSettings, FGenerateSimpleCollisionSettings::DataTypeIdentifier>;

public:
	static const FString InParamMesh() { return TEXT("Mesh"); }
	static const FString InParamIndexSets() { return TEXT("TriangleSets"); }	
	static const FString InParamSettings() { return TEXT("Settings"); }
	static const FString OutParamGeometry() { return TEXT("Geometry"); }

public:

	FGenerateSimpleCollisionNode()
	{
		AddInput(InParamMesh(), MakeUnique<FDynamicMeshInput>());
		AddInput(InParamIndexSets(), MakeBasicInput<FIndexSets>());
		AddInput(InParamSettings(), MakeBasicInput<FGenerateSimpleCollisionSettings>());

		AddOutput(OutParamGeometry(), MakeBasicOutput<FCollisionGeometry>());
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override;

protected:

	EGeometryFlowResult EvaluateInternal(const FDynamicMesh3& Mesh,
										 const FIndexSets& IndexData,
										 const FGenerateSimpleCollisionSettings& Settings,
										 TUniquePtr<FEvaluationInfo>& EvaluationInfo,
										 FCollisionGeometry& OutCollisionGeometry);

};

}	// end namespace GeometryFlow
}	// end namespace UE
