// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Structure/Mesh.h"

#include "CADKernel/Geo/Sampling/PolylineTools.h"
#include "CADKernel/Mesh/Structure/EdgeMesh.h"
#include "CADKernel/Mesh/Structure/ModelMesh.h"
#include "CADKernel/Topo/TopologicalEntity.h"
#include "CADKernel/Topo/TopologicalEdge.h"

namespace UE::CADKernel
{

TArray<double> FEdgeMesh::GetElementLengths() const
{
	const FTopologicalEdge& Edge = static_cast<const FTopologicalEdge&>(TopologicalEntity);
	const TArray<FPoint>& MeshInnerNodes = GetNodeCoordinates();

	const FPoint& StartNode = Edge.GetStartVertex()->GetCoordinates();
	const FPoint& EndNode = Edge.GetEndVertex()->GetCoordinates();
	return PolylineTools::ComputePolylineSegmentLengths(StartNode, MeshInnerNodes, EndNode);
}

int32 FMesh::RegisterCoordinates()
{
	ModelMesh.RegisterCoordinates(NodeCoordinates, StartNodeId, MeshModelIndex);
	LastNodeIndex = StartNodeId + (int32)NodeCoordinates.Num();
	return StartNodeId;
}


#ifdef CADKERNEL_DEV
FInfoEntity& FMesh::GetInfo(FInfoEntity& Info) const
{
	return FEntityGeom::GetInfo(Info)
		.Add(TEXT("Geometric Entity"), (FEntity&) GetGeometricEntity())
		.Add(TEXT("Mesh model"), (FEntity&) GetMeshModel())
		.Add(TEXT("Node Num"), (int32) NodeCoordinates.Num());
}
#endif

}