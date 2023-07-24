// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "ShapeApproximation/SimpleShapeSet3.h"

namespace UE::GeometryFlow { template <typename T, int StorageTypeIdentifier> class TTransferNode; }


namespace UE
{
namespace GeometryFlow
{


struct FCollisionGeometry
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::CollisionGeometry);

	UE::Geometry::FSimpleShapeSet3d Geometry;
};


// declares FDataCollisionGeometry, FCollisionGeometryInput, FCollisionGeometryOutput, FCollisionGeometrySourceNode
GEOMETRYFLOW_DECLARE_BASIC_TYPES(CollisionGeometry, FCollisionGeometry, (int)EMeshProcessingDataTypes::CollisionGeometry);


typedef TTransferNode<FCollisionGeometry, (int)EMeshProcessingDataTypes::CollisionGeometry> FCollisionGeometryTransferNode;



}	// end namespace GeometryFlow
}	// end namespace UE

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "BaseNodes/TransferNode.h"
#include "GeometryFlowMovableData.h"
#endif
