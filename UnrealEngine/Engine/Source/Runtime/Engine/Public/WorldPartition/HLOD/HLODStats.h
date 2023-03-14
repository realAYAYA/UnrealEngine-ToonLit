// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


struct ENGINE_API FWorldPartitionHLODStats
{
	// Input
	static const FName InputActorCount;
	static const FName InputTriangleCount;
	static const FName InputVertexCount;

	// Mesh
	static const FName MeshInstanceCount;
	static const FName MeshNaniteTriangleCount;
	static const FName MeshNaniteVertexCount;
	static const FName MeshTriangleCount;
	static const FName MeshVertexCount;
	static const FName MeshUVChannelCount;

	// Material
	static const FName MaterialBaseColorTextureSize;
	static const FName MaterialNormalTextureSize;
	static const FName MaterialEmissiveTextureSize;
	static const FName MaterialMetallicTextureSize;
	static const FName MaterialRoughnessTextureSize;
	static const FName MaterialSpecularTextureSize;
	
	// Memory
	static const FName MemoryMeshResourceSizeBytes;
	static const FName MemoryTexturesResourceSizeBytes;
	static const FName MemoryDiskSizeBytes;

	// Build
	static const FName BuildTimeLoadMilliseconds;
	static const FName BuildTimeBuildMilliseconds;
	static const FName BuildTimeTotalMilliseconds;
};
