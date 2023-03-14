// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraRenderer.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraMeshVertexFactory.h"
#include "NiagaraGPUSortInfo.h"
#include "StaticMeshResources.h"
#include "NiagaraGPUSceneUtils.h"

class FNiagaraDataSet;
struct FNiagaraDynamicDataMesh;

/**
* NiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API FNiagaraRendererMeshes : public FNiagaraRenderer
{
public:
	FNiagaraRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);
	virtual ~FNiagaraRendererMeshes() override;

	//FNiagaraRenderer Interface
	virtual void Initialize(const UNiagaraRendererProperties* InProps, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController) override;
	virtual void ReleaseRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy* SceneProxy) const override;
	virtual FNiagaraDynamicDataBase* GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const override;
	virtual int32 GetDynamicDataSize()const override;
	virtual bool IsMaterialValid(const UMaterialInterface* Mat)const override;
#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances, const FNiagaraSceneProxy* Proxy) final override;
#endif
	//FNiagaraRenderer Interface END

	void SetupVertexFactory(FNiagaraMeshVertexFactory& InVertexFactory, const FStaticMeshLODResources& LODResources) const;

protected:
	struct FParticleMeshRenderData
	{
		class FMeshElementCollector*	Collector = nullptr;
		const FNiagaraDynamicDataMesh*	DynamicDataMesh = nullptr;
		class FNiagaraDataBuffer*		SourceParticleData = nullptr;

		bool 							bUseGPUScene = false;
		bool							bHasTranslucentMaterials = false;
		bool							bSortCullOnGpu = false;
		bool							bNeedsSort = false;
		bool							bNeedsCull = false;
		bool							bIsGpuLowLatencyTranslucency = false;

		const FNiagaraRendererLayout*	RendererLayout = nullptr;
		ENiagaraMeshVFLayout::Type		SortVariable = ENiagaraMeshVFLayout::Type(INDEX_NONE);

		FRHIShaderResourceView*			ParticleFloatSRV = nullptr;
		FRHIShaderResourceView*			ParticleHalfSRV = nullptr;
		FRHIShaderResourceView*			ParticleIntSRV = nullptr;
		uint32							ParticleFloatDataStride = 0;
		uint32							ParticleHalfDataStride = 0;
		uint32							ParticleIntDataStride = 0;
		FRHIShaderResourceView*			ParticleSortedIndicesSRV = nullptr;
		uint32							ParticleSortedIndicesOffset = 0xffffffff;

		FIntVector4						VertexFetch_Parameters = FIntVector4(INDEX_NONE);
		FRHIShaderResourceView*			TexCoordBufferSrv = nullptr;
		FRHIShaderResourceView*			PackedTangentsBufferSrv = nullptr;
		FRHIShaderResourceView*			ColorComponentsBufferSrv = nullptr;

		uint32							RendererVisTagOffset = INDEX_NONE;
		uint32							MeshIndexOffset = INDEX_NONE;

		FVector							WorldSpacePivotOffset = FVector::ZeroVector;
		FSphere							CullingSphere = FSphere(EForceInit::ForceInit);
	};

	struct FMeshData
	{
		FStaticMeshRenderData* RenderData = nullptr;
		int32 MinimumLOD = 0;
		uint32 SourceMeshIndex = INDEX_NONE;
		FVector3f PivotOffset = FVector3f::ZeroVector;
		ENiagaraMeshPivotOffsetSpace PivotOffsetSpace = ENiagaraMeshPivotOffsetSpace::Mesh;
		FVector3f Scale = FVector3f(1.0f, 1.0f, 1.0f);
		FQuat4f Rotation = FQuat4f::Identity;
		FBox LocalBounds = FBox(ForceInitToZero);
		TArray<uint32, TInlineAllocator<4>> MaterialRemapTable;
	};

	class FMeshCollectorResources : public FOneFrameResource
	{
	public:
		~FMeshCollectorResources() override { VertexFactory.ReleaseResource(); }

		FNiagaraMeshVertexFactory VertexFactory;
		FNiagaraMeshUniformBufferRef UniformBuffer;
	};

	struct FEmitterSourceInstanceData
	{
		FPrimitiveInstance InstanceSceneData;
		FPrimitiveInstanceDynamicData InstanceDynamicData;
		float CustomData;
	};

	class FGPUSceneUpdateResource : public FOneFrameResource
	{
	public:
		ERHIFeatureLevel::Type FeatureLevel;
		bool bPreciseMotionVectors;
		FMeshBatchDynamicPrimitiveData DynamicPrimitiveData;
		FNiagaraGPUSceneUtils::FUpdateMeshParticleInstancesParams GPUWriteParams;
		FEmitterSourceInstanceData EmitterSourceData;

		FGPUSceneUpdateResource(ERHIFeatureLevel::Type InFeatureLevel, bool bInPreciseMotionVectors) : FeatureLevel(InFeatureLevel), bPreciseMotionVectors(bInPreciseMotionVectors) {}
		virtual ~FGPUSceneUpdateResource() {}
	};

	int32 GetLODIndex(int32 MeshIndex) const;

	void PrepareParticleMeshRenderData(FParticleMeshRenderData& ParticleMeshRenderData, const FSceneViewFamily& ViewFamily, FMeshElementCollector& Collector, FNiagaraDynamicDataBase* InDynamicData, const FNiagaraSceneProxy* SceneProxy, bool bRayTracing) const;
	void PrepareParticleRenderBuffers(FParticleMeshRenderData& ParticleMeshRenderData, FGlobalDynamicReadBuffer& DynamicReadBuffer) const;
	void InitializeSortInfo(const FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraSceneProxy& SceneProxy, const FSceneView& View, int32 ViewIndex, bool bIsInstancedStereo, FNiagaraGPUSortInfo& OutSortInfo) const;
	void PreparePerMeshData(FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraMeshVertexFactory& VertexFactory, const FNiagaraSceneProxy& SceneProxy, const FMeshData& MeshData) const;
	uint32 PerformSortAndCull(FParticleMeshRenderData& ParticleMeshRenderData, FGlobalDynamicReadBuffer& ReadBuffer, FNiagaraGPUSortInfo& SortInfo, class FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface, int32 MeshIndex) const;
	FNiagaraMeshCommonParameters CreateCommonShaderParams(const FParticleMeshRenderData& ParticleMeshRenderData, const FSceneView& View, const FMeshData& MeshData, const FNiagaraSceneProxy& SceneProxy) const;
	FNiagaraMeshUniformBufferRef CreateVFUniformBuffer(const FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraMeshCommonParameters& CommonParams) const;

	void SetupElementForGPUScene(
		const FParticleMeshRenderData& ParticleMeshRenderData,
		const FNiagaraMeshCommonParameters& CommonParameters,
		const FNiagaraSceneProxy& SceneProxy,
		const FMeshData& MeshData,
		const FSceneView& View,
		uint32 NumInstances,
		bool bNeedsPrevTransform,
		FMeshBatchElement& OutMeshBatchElement
	) const;

	void CreateMeshBatchForSection(
		const FParticleMeshRenderData& ParticleMeshRenderData,
		const FNiagaraMeshCommonParameters& CommonParams,
		FMeshBatch& MeshBatch,
		FVertexFactory& VertexFactory,
		FMaterialRenderProxy& MaterialProxy,
		const FNiagaraSceneProxy& SceneProxy,
		const FMeshData& MeshData,
		const FStaticMeshLODResources& LODModel,
		const FStaticMeshSection& Section,
		const FSceneView& View,
		int32 ViewIndex,
		uint32 NumInstances,
		uint32 GPUCountBufferOffset,
		bool bIsWireframe,
		bool bIsInstancedStereo,
		bool bNeedsPrevTransform
	) const;

private:
	TArray<FMeshData, TInlineAllocator<1>> Meshes;

	ENiagaraRendererSourceDataMode SourceMode;
	ENiagaraSortMode SortMode;
	ENiagaraMeshFacingMode FacingMode;
	uint32 bOverrideMaterials : 1;
	uint32 bSortHighPrecision : 1;
	uint32 bSortOnlyWhenTranslucent : 1;
	uint32 bGpuLowLatencyTranslucency : 1;
	uint32 bLockedAxisEnable : 1;
	uint32 bEnableCulling : 1;
	uint32 bEnableFrustumCulling : 1;
	uint32 bAccurateMotionVectors : 1;

	uint32 bSubImageBlend : 1;
	FVector2f SubImageSize;

	FVector LockedAxis;
	ENiagaraMeshLockedAxisSpace LockedAxisSpace;

	FVector2f DistanceCullRange;
	FVector2f DistanceCullRangeSquared;
	int32 ParticleRendererVisTagOffset = INDEX_NONE;
	int32 ParticleMeshIndexOffset = INDEX_NONE;
	int32 RendererVisibility = 0;
	int32 EmitterRendererVisTagOffset = INDEX_NONE;
	int32 EmitterMeshIndexOffset = INDEX_NONE;
	uint32 MaterialParamValidMask;
	uint32 MaxSectionCount;

	int32 VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Num_Max];
	uint32 bSetAnyBoundVars : 1;

	const FNiagaraRendererLayout* RendererLayoutWithCustomSorting;
	const FNiagaraRendererLayout* RendererLayoutWithoutCustomSorting;
};
