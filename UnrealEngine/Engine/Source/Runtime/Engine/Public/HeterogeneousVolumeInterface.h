// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneManagement.h"
#include "PrimitiveSceneProxy.h"

class IHeterogeneousVolumeInterface
{
public:
	virtual ~IHeterogeneousVolumeInterface() {}
	virtual const FPrimitiveSceneProxy* GetPrimitiveSceneProxy() const = 0;
	
	// Local-Space
	virtual const FBoxSphereBounds& GetBounds() const = 0;
	virtual const FBoxSphereBounds& GetLocalBounds() const = 0;
	virtual const FMatrix& GetLocalToWorld() const = 0;

	// Instance-Space
	virtual const FMatrix& GetInstanceToLocal() const = 0;
	virtual const FMatrix GetInstanceToWorld() const = 0;

	// Volume
	virtual FIntVector GetVoxelResolution() const = 0;
	virtual float GetMinimumVoxelSize() const = 0;

	// Lighting
	virtual float GetStepFactor() const = 0;
	virtual float GetShadowStepFactor() const = 0;
	virtual float GetShadowBiasFactor() const = 0;
	virtual float GetLightingDownsampleFactor() const = 0;
};

class FPrimitiveSceneProxy;

class FHeterogeneousVolumeData : public IHeterogeneousVolumeInterface, FOneFrameResource
{
public:
	explicit FHeterogeneousVolumeData(const FPrimitiveSceneProxy* SceneProxy)
		: PrimitiveSceneProxy(SceneProxy)
		, InstanceToLocal(FMatrix::Identity)
		, VoxelResolution(FIntVector::ZeroValue)
		, MinimumVoxelSize(0.1)
		, StepFactor(1.0)
		, ShadowStepFactor(8.0)
		, ShadowBiasFactor(0.0)
		, LightingDownsampleFactor(1.0)
	{}
	virtual ~FHeterogeneousVolumeData() {}

	virtual const FPrimitiveSceneProxy* GetPrimitiveSceneProxy() const { return PrimitiveSceneProxy; }

	// Local-space
	virtual const FBoxSphereBounds& GetBounds() const { return PrimitiveSceneProxy->GetBounds(); }
	virtual const FBoxSphereBounds& GetLocalBounds() const { return PrimitiveSceneProxy->GetLocalBounds(); }
	virtual const FMatrix& GetLocalToWorld() const { return PrimitiveSceneProxy->GetLocalToWorld(); }
	virtual const FMatrix& GetInstanceToLocal() const { return InstanceToLocal; }
	virtual const FMatrix GetInstanceToWorld() const { return InstanceToLocal * PrimitiveSceneProxy->GetLocalToWorld(); }

	// Volume
	virtual FIntVector GetVoxelResolution() const { return VoxelResolution; }
	virtual float GetMinimumVoxelSize() const { return MinimumVoxelSize; }

	// Lighting
	virtual float GetStepFactor() const { return StepFactor; }
	virtual float GetShadowStepFactor() const { return ShadowStepFactor; }
	virtual float GetShadowBiasFactor() const { return ShadowBiasFactor; }
	virtual float GetLightingDownsampleFactor() const { return LightingDownsampleFactor; }

	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	FMatrix InstanceToLocal;
	FIntVector VoxelResolution;
	float MinimumVoxelSize;
	float StepFactor;
	float ShadowStepFactor;
	float ShadowBiasFactor;
	float LightingDownsampleFactor;
};
