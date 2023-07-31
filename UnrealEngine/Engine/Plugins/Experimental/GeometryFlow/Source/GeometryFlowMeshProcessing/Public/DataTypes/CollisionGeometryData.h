// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "GeometryFlowMovableData.h"
#include "BaseNodes/TransferNode.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "ShapeApproximation/SimpleShapeSet3.h"


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