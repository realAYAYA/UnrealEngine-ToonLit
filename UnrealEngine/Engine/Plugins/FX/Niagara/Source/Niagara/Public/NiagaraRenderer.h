// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Materials/MaterialInterface.h"
#include "UniformBuffer.h"
#include "Materials/Material.h"
#include "PrimitiveViewRelevance.h"
#include "ParticleHelper.h"
#include "NiagaraRendererProperties.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "NiagaraCutoutVertexBuffer.h"
#include "NiagaraBoundsCalculator.h"
#include "RayTracingGeometry.h"

struct INiagaraComputeDataBufferInterface;
class FNiagaraDataSet;
class FNiagaraSceneProxy;
class FNiagaraGPURendererCount;
class FNiagaraSystemInstanceController;

/** Struct used to pass dynamic data from game thread to render thread */
struct FNiagaraDynamicDataBase
{
	NIAGARA_API explicit FNiagaraDynamicDataBase(const FNiagaraEmitterInstance* InEmitter);
	NIAGARA_API virtual ~FNiagaraDynamicDataBase();

	UE_NONCOPYABLE(FNiagaraDynamicDataBase);

	NIAGARA_API bool IsGpuLowLatencyTranslucencyEnabled() const;
	NIAGARA_API FNiagaraDataBuffer* GetParticleDataToRender(bool bIsLowLatencyTranslucent = false) const;
	FORCEINLINE FMaterialRelevance GetMaterialRelevance() const { return MaterialRelevance; }

	FORCEINLINE void SetMaterialRelevance(FMaterialRelevance NewRelevance) { MaterialRelevance = NewRelevance; }

	FORCEINLINE FNiagaraSystemInstanceID GetSystemInstanceID() const { return SystemInstanceID; }

	virtual void ApplyMaterialOverride(int32 MaterialIndex, UMaterialInterface* MaterialOverride) {};

protected:
	FMaterialRelevance MaterialRelevance;
	FNiagaraSystemInstanceID SystemInstanceID;

	FNiagaraDataBufferRef CPUParticleData;
	INiagaraComputeDataBufferInterface* ComputeDataBufferInterface = nullptr;
};

//////////////////////////////////////////////////////////////////////////

struct FParticleRenderData
{
	FGlobalDynamicReadBuffer::FAllocation FloatData;
	FGlobalDynamicReadBuffer::FAllocation HalfData;
	FGlobalDynamicReadBuffer::FAllocation IntData;
	uint32 FloatStride = 0;
	uint32 HalfStride = 0;
	uint32 IntStride = 0;
};

/**
* Base class for Niagara System renderers.
*/
class FNiagaraRenderer
{
public:
	NIAGARA_API FNiagaraRenderer(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);
	NIAGARA_API virtual ~FNiagaraRenderer();

	FNiagaraRenderer(const FNiagaraRenderer& Other) = delete;
	FNiagaraRenderer& operator=(const FNiagaraRenderer& Other) = delete;

	NIAGARA_API virtual void Initialize(const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InComponent);
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) {}
	virtual void ReleaseRenderThreadResources() {}
	virtual void DestroyRenderState_Concurrent() {}

	NIAGARA_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const {}
	virtual FNiagaraDynamicDataBase* GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitteride) const { return nullptr; }

	virtual void GatherSimpleLights(FSimpleLightArray& OutParticleLights)const {}
	virtual int32 GetDynamicDataSize()const { return 0; }
	virtual bool IsMaterialValid(const UMaterialInterface* Mat)const { return Mat != nullptr; }

	// Determine if we are rendering into an opaque only view
	static NIAGARA_API bool IsViewRenderingOpaqueOnly(const FSceneView* View, bool bCastsVolumetricTranslucentShadow);
	static NIAGARA_API bool AreViewsRenderingOpaqueOnly(const TArray<const FSceneView*>& Views, int32 ViewVisibilityMask, bool bCastsVolumetricTranslucentShadow);

	UE_DEPRECATED(5.4, "SortIndices method should be replaced with SortAndCullIndices")
	static NIAGARA_API void SortIndices(const struct FNiagaraGPUSortInfo& SortInfo, const FNiagaraRendererVariableInfo& SortVariable, const FNiagaraDataBuffer& Buffer, FGlobalDynamicReadBuffer::FAllocation& OutIndices);
	static NIAGARA_API int32 SortAndCullIndices(const FNiagaraGPUSortInfo& SortInfo, const FNiagaraDataBuffer& Buffer, FGlobalDynamicReadBuffer::FAllocation& OutIndices);

	static NIAGARA_API FVector4f CalcMacroUVParameters(const FSceneView& View, FVector MacroUVPosition, float MacroUVRadius);

	NIAGARA_API void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData);
	FORCEINLINE FNiagaraDynamicDataBase *GetDynamicData() const { return DynamicDataRender; }
	FORCEINLINE bool HasDynamicData() const { return DynamicDataRender != nullptr; }
	FORCEINLINE bool HasLights() const { return bHasLights; }
	FORCEINLINE bool IsMotionBlurEnabled() const { return bMotionBlurEnabled; }

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances, const FNiagaraSceneProxy* Proxy) {}
#endif

	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultFloat(const FRWBuffer& RWBuffer) { return RWBuffer.SRV.IsValid() ? (FRHIShaderResourceView*)RWBuffer.SRV : GetDummyFloatBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultHalf(const FRWBuffer& RWBuffer) { return RWBuffer.SRV.IsValid() ? (FRHIShaderResourceView*)RWBuffer.SRV : GetDummyHalfBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultUInt(const FRWBuffer& RWBuffer) { return RWBuffer.SRV.IsValid() ? (FRHIShaderResourceView*)RWBuffer.SRV : GetDummyUIntBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultInt(const FRWBuffer& RWBuffer) { return RWBuffer.SRV.IsValid() ? (FRHIShaderResourceView*)RWBuffer.SRV : GetDummyIntBuffer(); }

	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultFloat(FGlobalDynamicReadBuffer::FAllocation& Allocation) { return Allocation.IsValid() ? (FRHIShaderResourceView*)Allocation.SRV : GetDummyFloatBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultHalf(FGlobalDynamicReadBuffer::FAllocation& Allocation) { return Allocation.IsValid() ? (FRHIShaderResourceView*)Allocation.SRV : GetDummyHalfBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultUInt(FGlobalDynamicReadBuffer::FAllocation& Allocation) { return Allocation.IsValid() ? (FRHIShaderResourceView*)Allocation.SRV : GetDummyUIntBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultInt(FGlobalDynamicReadBuffer::FAllocation& Allocation) { return Allocation.IsValid() ? (FRHIShaderResourceView*)Allocation.SRV : GetDummyIntBuffer(); }

	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultFloat(FRHIShaderResourceView* InSRV) { return InSRV ? InSRV : GetDummyFloatBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultFloat2(FRHIShaderResourceView* InSRV) { return InSRV ? InSRV : GetDummyFloat2Buffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultFloat4(FRHIShaderResourceView* InSRV) { return InSRV ? InSRV : GetDummyFloat4Buffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultUInt(FRHIShaderResourceView* InSRV) { return InSRV ? InSRV : GetDummyUIntBuffer(); }
	FORCEINLINE static FRHIShaderResourceView* GetSrvOrDefaultInt(FRHIShaderResourceView* InSRV) { return InSRV ? InSRV : GetDummyIntBuffer(); }

	static NIAGARA_API FRHIShaderResourceView* GetDummyFloatBuffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyFloat2Buffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyFloat4Buffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyWhiteColorBuffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyIntBuffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyUIntBuffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyUInt2Buffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyUInt4Buffer();
	static NIAGARA_API FRHIShaderResourceView* GetDummyHalfBuffer();

	FORCEINLINE ENiagaraSimTarget GetSimTarget() const { return SimTarget; }

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& UsedMaterials, bool bGetDebugMaterials) { UsedMaterials.Append(BaseMaterials_GT); }

	virtual void PostSystemTick_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) { }
	virtual void OnSystemComplete_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) { }

protected:

	NIAGARA_API virtual void ProcessMaterialParameterBindings(const FNiagaraRendererMaterialParameters& MaterialParameters, const FNiagaraEmitterInstance* InEmitter, TConstArrayView<UMaterialInterface*> InMaterials) const;

	NIAGARA_API bool IsRendererEnabled(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const;
	NIAGARA_API bool UseLocalSpace(const FNiagaraSceneProxy* Proxy) const;

	static NIAGARA_API bool ViewFamilySupportLowLatencyTranslucency(const FSceneViewFamily& ViewFamily);

	struct FNiagaraDynamicDataBase *DynamicDataRender;

#if RHI_RAYTRACING
	FRWBuffer RayTracingDynamicVertexBuffer;
	FRayTracingGeometry RayTracingGeometry;
#endif

	uint32 bLocalSpace : 1;
	uint32 bHasLights : 1;
	uint32 bMotionBlurEnabled : 1;
	uint32 bRendersInSecondaryDepthPass : 1;
	const ENiagaraSimTarget SimTarget;

	ERHIFeatureLevel::Type FeatureLevel;

#if STATS
	TStatId EmitterStatID;
#endif


	static NIAGARA_API FParticleRenderData TransferDataToGPU(FRHICommandListBase& RHICmdList, FGlobalDynamicReadBuffer& DynamicReadBuffer, const FNiagaraRendererLayout* RendererLayout, TConstArrayView<uint32> IntComponents, const FNiagaraDataBuffer* SrcData);

	/** Cached array of materials used from the properties data. Validated with usage flags etc. */
	TArray<UMaterialInterface*> BaseMaterials_GT;
	FMaterialRelevance BaseMaterialRelevance_GT;
};
