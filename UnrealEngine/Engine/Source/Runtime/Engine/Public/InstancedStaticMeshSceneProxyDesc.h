// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StaticMeshSceneProxyDesc.h"
#include "Engine/InstancedStaticMesh.h"

class UInstancedStaticMeshComponent;

struct FInstancedStaticMeshSceneProxyDesc : public FStaticMeshSceneProxyDesc
{		
	FInstancedStaticMeshSceneProxyDesc() = default;
	ENGINE_API FInstancedStaticMeshSceneProxyDesc(UInstancedStaticMeshComponent*);
	void InitializeFrom(UInstancedStaticMeshComponent*);

	TSharedPtr<FISMCInstanceDataSceneProxy, ESPMode::ThreadSafe> InstanceDataSceneProxy;
#if WITH_EDITOR
	TBitArray<>	SelectedInstances;
#endif

	int32 InstanceStartCullDistance = 0;
	int32 InstanceEndCullDistance = 0;
	FVector MinScale = FVector(0);
	FVector MaxScale = FVector(0);
	float InstanceLODDistanceScale = 1.0f;

	bool bUseGpuLodSelection = false;

	void GetInstancesMinMaxScale(FVector& InMinScale, FVector& InMaxScale) const
	{
		InMinScale = MinScale;
		InMaxScale = MaxScale;
	}
};