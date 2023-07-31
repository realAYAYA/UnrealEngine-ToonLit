// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraRenderer.h"
#include "PrimitiveUniformShaderParameters.h"
#include "PrimitiveViewRelevance.h"

#if RHI_RAYTRACING
#include "RayTracingInstance.h"
#endif

class FNiagaraSystemInstance;
class FNiagaraSystemRenderData;
class FMeshElementCollector;
struct FNiagaraDynamicDataBase;
class FNiagaraSceneProxy;
class FSceneView;

/**
 * This class wraps all data and functionality needed by a scene render proxy to render a single Niagara System Instance.
 */
class FNiagaraSystemRenderData
{
public:
	struct FMaterialOverride
	{
		UMaterialInterface* Material = nullptr;
		const UNiagaraRendererProperties* EmitterRendererProperty = nullptr; // Pointer only used for lookups, don't actually dereference
		uint32 MaterialSubIndex = 0;
	};

	struct FSetDynamicDataCommand
	{
		FNiagaraRenderer* Renderer = nullptr;
		FNiagaraDynamicDataBase* DynamicData = nullptr;

		FSetDynamicDataCommand() {}
		FSetDynamicDataCommand(FNiagaraRenderer* InRenderer, FNiagaraDynamicDataBase* InDynamicData) : Renderer(InRenderer), DynamicData(InDynamicData) {}
		void Execute() const { Renderer->SetDynamicData_RenderThread(DynamicData); }
	};

	using FSetDynamicDataCommandList = TArray<FSetDynamicDataCommand, TInlineAllocator<8>>;

	static void ExecuteDynamicDataCommands_RenderThread(const FSetDynamicDataCommandList& Commands);

public:
	FNiagaraSystemRenderData(const FNiagaraSystemInstanceController& SystemInstanceController, const FNiagaraSystemInstance& SystemInstance, ERHIFeatureLevel::Type FeatureLevel);
	FNiagaraSystemRenderData(const FNiagaraSystemRenderData&) = delete;
	~FNiagaraSystemRenderData();

	/** Called from the render thread to give renderers an opportunity to create resources needed for rendering. */
	void CreateRenderThreadResources();
	/** Called from the render thread to give renderers an opportunity to release their resources. */
	void ReleaseRenderThreadResources();
	/** Gives the system's renderers an opportunity to free resources */
	void DestroyRenderState_Concurrent();
	/** This must be called on the RenderThread before destruction to properly clean up resources without data race issues (i.e. from a scene proxy's destructor) */
	void Destroy_RenderThread();

	/** Gets the relevance of this NiagaraSystem for this view */
	FPrimitiveViewRelevance GetViewRelevance(const FSceneView& View, const FNiagaraSceneProxy& SceneProxy) const;
	/** Gets the total size of dynamic data allocated */
	uint32 GetDynamicDataSize() const;
	/** Called at the end of the frame, before rendering when necessary to provide renderers with render thread data */
	void GenerateSetDynamicDataCommands(FSetDynamicDataCommandList& Commands, const FNiagaraSceneProxy& SceneProxy, const FNiagaraSystemInstance* SystemInstance, TConstArrayView<FMaterialOverride> MaterialOverrides);
	/** Gets the dynamic mesh elements from all renderers */
	void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy& SceneProxy);
#if RHI_RAYTRACING
	void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances, const FNiagaraSceneProxy& SceneProxy);
#endif
	void GatherSimpleLights(FSimpleLightArray& OutParticleLights) const;

	void PostTickRenderers(const FNiagaraSystemInstance& SystemInstance);
	void OnSystemComplete(const FNiagaraSystemInstance& SystemInstance);
	void RecacheRenderers(const FNiagaraSystemInstance& SystemInstance, const FNiagaraSystemInstanceController& Controller);

	FORCEINLINE bool IsRenderingEnabled() const { return bRenderingEnabled && (IsInRenderingThread() ? EmitterRenderers_RT.Num() > 0 : EmitterRenderers_GT.Num() > 0); }
	FORCEINLINE void SetRenderingEnabled(bool bInEnabled) { bRenderingEnabled = bInEnabled; }
	FORCEINLINE bool HasAnyMotionBlurEnabled() const { return bAnyMotionBlurEnabled; }

	FORCEINLINE int32 GetNumRenderers() const { return RendererDrawOrder.Num(); }

	FVector3f LWCRenderTile = FVector3f::ZeroVector;
private:
	/** Emitter Renderers in the order they appear in the emitters. To be accessed by the GameThread */
	TArray<FNiagaraRenderer*> EmitterRenderers_GT;
	/** Emitter Renderers in the order they appear in the emitters. To be accessed by the RenderThread */
	TArray<FNiagaraRenderer*> EmitterRenderers_RT;

	/** Indices of renderers in the order they should be rendered. */
	TArray<int32> RendererDrawOrder;

	bool bRenderingEnabled = true;
	bool bAnyMotionBlurEnabled = false;

	ERHIFeatureLevel::Type FeatureLevel;
};
