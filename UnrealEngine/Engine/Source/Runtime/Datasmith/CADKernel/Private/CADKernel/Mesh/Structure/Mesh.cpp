// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Mesh/Structure/Mesh.h"

#include "CADKernel/Mesh/Structure/ModelMesh.h"
#include "CADKernel/Topo/TopologicalEntity.h"

namespace UE::CADKernel
{

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