// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrassInstancedStaticMeshComponent.h"

UGrassInstancedStaticMeshComponent::UGrassInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ViewRelevanceType = EHISMViewRelevanceType::Grass;
}

void UGrassInstancedStaticMeshComponent::BuildTreeAnyThread(
	TArray<FMatrix>& InstanceTransforms, 
	TArray<float>& InstanceCustomDataFloats,
	int32 NumCustomDataFloats,
	const FBox& MeshBox,
	TArray<FClusterNode>& OutClusterTree,
	TArray<int32>& OutSortedInstances,
	TArray<int32>& OutInstanceReorderTable,
	int32& OutOcclusionLayerNum,
	int32 MaxInstancesPerLeaf,
	bool InGenerateInstanceScalingRange
	)
{
	// TODO: Move the implementation of this function in the base here in 5.2
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Super::BuildTreeAnyThread(
		InstanceTransforms,
		InstanceCustomDataFloats,
		NumCustomDataFloats,
		MeshBox,
		OutClusterTree,
		OutSortedInstances,
		OutInstanceReorderTable,
		OutOcclusionLayerNum,
		MaxInstancesPerLeaf,
		InGenerateInstanceScalingRange);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UGrassInstancedStaticMeshComponent::AcceptPrebuiltTree(TArray<FInstancedStaticMeshInstanceData>& InInstanceData, TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances)
{
	// TODO: Move the implementation of this function in the base to here in 5.2
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Super::AcceptPrebuiltTree(
		InInstanceData,
		InClusterTree,
		InOcclusionLayerNumNodes,
		InNumBuiltRenderInstances);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

