// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "GrassInstancedStaticMeshComponent.generated.h"

UCLASS(ClassGroup = Foliage, MinimalAPI)
class UGrassInstancedStaticMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	static FOLIAGE_API void BuildTreeAnyThread(TArray<FMatrix>& InstanceTransforms, TArray<float>& InstanceCustomDataFloats, int32 NumCustomDataFloats, const FBox& MeshBox, TArray<FClusterNode>& OutClusterTree, TArray<int32>& OutSortedInstances, TArray<int32>& OutInstanceReorderTable, int32& OutOcclusionLayerNum, int32 MaxInstancesPerLeaf, bool InGenerateInstanceScalingRange);
	
	UE_DEPRECATED(5.4, "Use the new version of AcceptPrebuiltTree below.")
	FOLIAGE_API void AcceptPrebuiltTree(TArray<FInstancedStaticMeshInstanceData>& InInstanceData, TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances);

	FOLIAGE_API void AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int32 InOcclusionLayerNumNodes, int32 InNumBuiltRenderInstances, FStaticMeshInstanceData* InSharedInstanceBufferData);

	FOLIAGE_API bool SupportsWorldPositionOffsetVelocity() const override;

	int32 GetNumRenderInstances() const override { return NumBuiltRenderInstances; }

private:
	virtual void BuildComponentInstanceData(ERHIFeatureLevel::Type FeatureLevel, FInstanceUpdateComponentDesc& OutData) override final;
};

