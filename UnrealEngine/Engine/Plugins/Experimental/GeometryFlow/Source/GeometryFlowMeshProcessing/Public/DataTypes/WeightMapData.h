// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"


namespace UE
{
namespace GeometryFlow
{


struct FWeightMap
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::WeightMap);

	TArray<float> Weights;

	// TODO: Sparse map from vertex ID to weight value?
};


GEOMETRYFLOW_DECLARE_BASIC_TYPES(WeightMap, FWeightMap, (int)EMeshProcessingDataTypes::WeightMap)


}	// end namespace GeometryFlow
}	// end namespace UE
