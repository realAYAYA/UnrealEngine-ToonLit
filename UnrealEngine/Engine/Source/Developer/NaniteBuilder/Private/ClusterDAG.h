// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster.h"
#include "Containers/BinaryHeap.h"
#include "Containers/BitArray.h"

// Log CRCs to test for deterministic building
#if 0
	#define LOG_CRC( Array ) UE_LOG( LogStaticMesh, Log, TEXT(#Array " CRC %u"), FCrc::MemCrc32( Array.GetData(), Array.Num() * Array.GetTypeSize() ) )
#else
	#define LOG_CRC( Array )
#endif

namespace Nanite
{

struct FClusterGroup
{
	FSphere3f			Bounds;
	FSphere3f			LODBounds;
	float				MinLODError;
	float				MaxParentLODError;
	int32				MipLevel;
	uint32				MeshIndex;
	bool				bTrimmed;
	
	uint32				PageIndexStart;
	uint32				PageIndexNum;
	TArray< uint32 >	Children;

	friend FArchive& operator<<(FArchive& Ar, FClusterGroup& Group);
};

// Performs DAG reduction and appends the resulting clusters and groups
void BuildDAG( TArray< FClusterGroup >& Groups, TArray< FCluster >& Cluster, uint32 ClusterBaseStart, uint32 ClusterBaseNum, uint32 MeshIndex, FBounds3f& MeshBounds );

FBinaryHeap< float > FindDAGCut(
	const TArray< FClusterGroup >& Groups,
	const TArray< FCluster >& Clusters,
	uint32 TargetNumTris,
	float  TargetError,
	uint32 TargetOvershoot,
	TBitArray<>* SelectedGroupsMask );

} // namespace Nanite