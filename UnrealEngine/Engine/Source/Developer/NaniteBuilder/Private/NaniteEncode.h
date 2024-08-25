// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NaniteDefinitions.h"
#include "Math/Bounds.h"

struct FMeshNaniteSettings;

namespace Nanite
{
	struct FResources;
	struct FMaterialTriangle;
	struct FMaterialRange;
	class FCluster;
	struct FClusterGroup;
	
	void BuildRayTracingData(FResources& Resources, TArray<FCluster>& Clusters);
	void BuildMaterialRanges(const TArray<uint32>& TriangleIndices, const TArray<int32>& MaterialIndices, TArray<FMaterialTriangle, TInlineAllocator<128>>& MaterialTris, TArray<FMaterialRange, TInlineAllocator<4>>& MaterialRanges);
	void Encode(FResources& Resources, const FMeshNaniteSettings& Settings, TArray<FCluster>& Clusters, TArray<FClusterGroup>& Groups, const FBounds3f& MeshBounds, uint32 NumMeshes, uint32 NumTexCoords, bool bHasTangents, bool bHasColors, uint32* OutTotalGPUSize);
}
