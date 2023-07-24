// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Particles/ParticlePerfStats.h"
#include "PrimitiveSceneProxy.h"

class FNiagaraGpuComputeDispatchInterface;
class FNiagaraSystemRenderData;
class UNiagaraComponent;

/**
* Scene proxy for drawing niagara particle simulations.
*/
class NIAGARA_API FNiagaraSceneProxy : public FPrimitiveSceneProxy
{
public:
	virtual SIZE_T GetTypeHash() const override;

	FNiagaraSceneProxy(UNiagaraComponent* InComponent);
	~FNiagaraSceneProxy();

	/** Retrieves the render data for a single system */
	FNiagaraSystemRenderData* GetSystemRenderData() { return RenderData; }

	/** Called to allow renderers to free render state */
	void DestroyRenderState_Concurrent();

	/** Gets whether or not this scene proxy should be rendered. */
	bool GetRenderingEnabled() const;

	/** Sets whether or not this scene proxy should be rendered. */
	void SetRenderingEnabled(bool bInRenderingEnabled);

	FNiagaraGpuComputeDispatchInterface* GetComputeDispatchInterface() const { return ComputeDispatchInterface; }

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;
	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual bool HasRayTracingRepresentation() const override { return true; }
#endif

	FORCEINLINE const FMatrix& GetLocalToWorldInverse() const { return LocalToWorldInverse; }

	const FVector3f& GetLWCRenderTile() const;

	TUniformBuffer<FPrimitiveUniformShaderParameters>* GetCustomUniformBufferResource(bool bHasVelocity, const FBox& InstanceBounds = FBox(ForceInitToZero)) const;
	FRHIUniformBuffer* GetCustomUniformBuffer(bool bHasVelocity, const FBox& InstanceBounds = FBox(ForceInitToZero)) const;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	/** Some proxy wide dynamic settings passed down with the emitter dynamic data. */
	struct FDynamicData
	{
		bool bUseCullProxy = false;
		float LODDistanceOverride = -1.0f;
#if WITH_PARTICLE_PERF_STATS
		FParticlePerfStatsContext PerfStatsContext;
#endif
	};
	const FDynamicData& GetProxyDynamicData()const { return DynamicData; }
	void SetProxyDynamicData(const FDynamicData& NewData) { DynamicData = NewData; }

private:
	void ReleaseRenderThreadResources();

	void ReleaseUniformBuffers(bool bEmpty);

	//~ Begin FPrimitiveSceneProxy Interface.
	virtual void CreateRenderThreadResources() override;

	//virtual void OnActorPositionChanged() override;
	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;


	virtual bool CanBeOccluded() const override
	{
		// TODO account for MaterialRelevance.bDisableDepthTest and MaterialRelevance.bPostMotionBlurTranslucency as well
		return !ShouldRenderCustomDepth();
	}


	/** Callback from the renderer to gather simple lights that this proxy wants renderered. */
	virtual void GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const override;

	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;

private:
	/** Custom Uniform Buffers, allows us to have renderer specific data packed inside such as pre-skinned bounds. */
	mutable TMap<uint32, TUniformBuffer<FPrimitiveUniformShaderParameters>*> CustomUniformBuffers;

	/** The data required to render a single instance of a NiagaraSystem */
	FNiagaraSystemRenderData* RenderData = nullptr;

	FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = nullptr;

	FMatrix LocalToWorldInverse;

	TStatId SystemStatID;

	FDynamicData DynamicData;
};

