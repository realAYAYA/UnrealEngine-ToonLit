// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

namespace UE::Geometry { class FDynamicMesh3; }
namespace UE::GeometryFlow { template <typename T, int StorageTypeIdentifier> class TTransferNode; }

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "BaseNodes/TransferNode.h"
#include "DynamicMesh/DynamicMesh3.h"
#endif
