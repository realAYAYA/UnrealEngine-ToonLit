// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "BaseNodes/TransferNode.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

// declares FDataDynamicMesh, FDynamicMeshInput, FDynamicMeshOutput, FDynamicMeshSourceNode
GEOMETRYFLOW_DECLARE_BASIC_TYPES(DynamicMesh, FDynamicMesh3, (int)EMeshProcessingDataTypes::DynamicMesh)


typedef TTransferNode<FDynamicMesh3, (int)EMeshProcessingDataTypes::DynamicMesh> FDynamicMeshTransferNode;


}	// end namespace GeometryFlow
}	// end namespace UE