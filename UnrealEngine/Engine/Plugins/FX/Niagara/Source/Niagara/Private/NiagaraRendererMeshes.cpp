// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererMeshes.h"
#include "Async/ParallelFor.h"
#include "Engine/StaticMesh.h"
#include "FXRenderingUtils.h"
#include "IXRTrackingSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraCullProxyComponent.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraGPURayTracingTransformsShader.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraSortingGPU.h"
#include "NiagaraStats.h"
#include "ParticleResources.h"
#include "PipelineStateCache.h"
#include "RayTracingDefinitions.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RayTracingInstance.h"
#include "ScenePrivate.h"

DECLARE_CYCLE_STAT(TEXT("Generate Mesh Vertex Data [GT]"), STAT_NiagaraGenMeshVertexData, STATGROUP_Niagara);

DECLARE_DWORD_COUNTER_STAT(TEXT("NumMeshesRenderer"), STAT_NiagaraNumMeshes, STATGROUP_Niagara);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumMeshVerts"), STAT_NiagaraNumMeshVerts, STATGROUP_Niagara);

static int32 GbEnableNiagaraMeshRendering = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraMeshRendering(
	TEXT("fx.EnableNiagaraMeshRendering"),
	GbEnableNiagaraMeshRendering,
	TEXT("If == 0, Niagara Mesh Renderers are disabled. \n"),
	ECVF_Default
);

#if RHI_RAYTRACING
static TAutoConsoleVariable<int32> CVarRayTracingNiagaraMeshes(
	TEXT("r.RayTracing.Geometry.NiagaraMeshes"),
	1,
	TEXT("Include Niagara meshes in ray tracing effects (default = 1 (Niagara meshes enabled in ray tracing))"));
#endif

struct FNiagaraDynamicDataMesh : public FNiagaraDynamicDataBase
{
	FNiagaraDynamicDataMesh(const FNiagaraEmitterInstance* InEmitter)
		: FNiagaraDynamicDataBase(InEmitter)
	{
	}

	virtual void ApplyMaterialOverride(int32 MaterialIndex, UMaterialInterface* MaterialOverride) override
	{
		if (Materials.IsValidIndex(MaterialIndex) && MaterialOverride)
		{
			Materials[MaterialIndex] = MaterialOverride->GetRenderProxy();
		}
	}

	TArray<FMaterialRenderProxy*, TInlineAllocator<8>> Materials;
	TArray<UNiagaraDataInterface*> DataInterfacesBound;
	TArray<UObject*> ObjectsBound;
	TArray<uint8> ParameterDataBound;
};

//////////////////////////////////////////////////////////////////////////

FNiagaraRendererMeshes::FNiagaraRendererMeshes(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties* Props, const FNiagaraEmitterInstance* Emitter)
	: FNiagaraRenderer(FeatureLevel, Props, Emitter)
	, MaterialParamValidMask(0)
{
	check(Emitter);
	check(Props);

	const UNiagaraMeshRendererProperties* Properties = CastChecked<const UNiagaraMeshRendererProperties>(Props);
	SourceMode = Properties->SourceMode;
	FacingMode = Properties->FacingMode;
	bLockedAxisEnable = Properties->bLockedAxisEnable;
	LockedAxis = Properties->LockedAxis;
	LockedAxisSpace = Properties->LockedAxisSpace;
	SortMode = Properties->SortMode;
	bSortHighPrecision = UNiagaraRendererProperties::IsSortHighPrecision(Properties->SortPrecision);
	bSortOnlyWhenTranslucent = Properties->bSortOnlyWhenTranslucent;
	bGpuLowLatencyTranslucency = UNiagaraRendererProperties::IsGpuTranslucentThisFrame(Properties->GpuTranslucentLatency);
	bOverrideMaterials = Properties->bOverrideMaterials;
	SubImageSize = FVector2f(Properties->SubImageSize);	// LWC_TODO: Precision loss
	bSubImageBlend = Properties->bSubImageBlend;
	bEnableFrustumCulling = Properties->bEnableFrustumCulling;
	bEnableCulling = bEnableFrustumCulling;
	DistanceCullRange = FVector2f(0, FLT_MAX);
	DistanceCullRangeSquared = FVector2f(0, FLT_MAX);
	RendererVisibility = Properties->RendererVisibility;
	bAccurateMotionVectors = Properties->NeedsPreciseMotionVectors();
	MaxSectionCount = 0;

	if (Properties->bEnableCameraDistanceCulling)
	{
		DistanceCullRange = FVector2f(Properties->MinCameraDistance, Properties->MaxCameraDistance);
		DistanceCullRangeSquared = DistanceCullRange* DistanceCullRange;
		bEnableCulling = true;
	}

	// Ensure valid value for the locked axis
	if (!LockedAxis.Normalize())
	{
		LockedAxis.Set(0.0f, 0.0f, 1.0f);
	}

	const FNiagaraDataSet& Data = Emitter->GetData();

	int32 FloatOffset;
	int32 HalfOffset;

	ParticleRendererVisTagOffset = INDEX_NONE;
	EmitterRendererVisTagOffset = INDEX_NONE;
	if (Properties->RendererVisibilityTagBinding.CanBindToHostParameterMap())
	{
		EmitterRendererVisTagOffset = Emitter->GetRendererBoundVariables().IndexOf(Properties->RendererVisibilityTagBinding.GetParamMapBindableVariable());
	}
	else if (Data.GetVariableComponentOffsets(Properties->RendererVisibilityTagBinding.GetDataSetBindableVariable(), FloatOffset, ParticleRendererVisTagOffset, HalfOffset))
	{
		bEnableCulling |= ParticleRendererVisTagOffset != INDEX_NONE;
	}

	ParticleMeshIndexOffset = INDEX_NONE;
	EmitterMeshIndexOffset = INDEX_NONE;
	if (Properties->MeshIndexBinding.CanBindToHostParameterMap())
	{
		EmitterMeshIndexOffset = Emitter->GetRendererBoundVariables().IndexOf(Properties->MeshIndexBinding.GetParamMapBindableVariable());
	}
	else if (Data.GetVariableComponentOffsets(Properties->MeshIndexBinding.GetDataSetBindableVariable(), FloatOffset, ParticleMeshIndexOffset, HalfOffset))
	{
		bEnableCulling = ParticleMeshIndexOffset != INDEX_NONE;
	}

	MaterialParamValidMask = Properties->MaterialParamValidMask;

	RendererLayoutWithCustomSorting = &Properties->RendererLayoutWithCustomSorting;
	RendererLayoutWithoutCustomSorting = &Properties->RendererLayoutWithoutCustomSorting;

	bSetAnyBoundVars = false;
	if (Emitter->GetRendererBoundVariables().IsEmpty() == false)
	{
		const TArray< const FNiagaraVariableAttributeBinding*>& VFBindings = Properties->GetAttributeBindings();
		const int32 NumBindings = bAccurateMotionVectors ? ENiagaraMeshVFLayout::Type::Num_Max : ENiagaraMeshVFLayout::Type::Num_Default;
		check(VFBindings.Num() >= ENiagaraMeshVFLayout::Type::Num_Max);
		for (int32 i = 0; i < ENiagaraMeshVFLayout::Type::Num_Max; i++)
		{
			VFBoundOffsetsInParamStore[i] = INDEX_NONE;
			if (i < NumBindings && VFBindings[i] && VFBindings[i]->CanBindToHostParameterMap())
			{
				VFBoundOffsetsInParamStore[i] = Emitter->GetRendererBoundVariables().IndexOf(VFBindings[i]->GetParamMapBindableVariable());
				if (VFBoundOffsetsInParamStore[i] != INDEX_NONE)
					bSetAnyBoundVars = true;
			}
		}
	}
	else
	{
		for (int32 i = 0; i < ENiagaraMeshVFLayout::Type::Num_Max; i++)
		{
			VFBoundOffsetsInParamStore[i] = INDEX_NONE;
		}
	}
}

FNiagaraRendererMeshes::~FNiagaraRendererMeshes()
{
}

void FNiagaraRendererMeshes::Initialize(const UNiagaraRendererProperties* InProps, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController)
{
	FNiagaraRenderer::Initialize(InProps, Emitter, InController);

	check(Emitter);
	check(InProps);

	const UNiagaraMeshRendererProperties* Properties = CastChecked<const UNiagaraMeshRendererProperties>(InProps);

	MaxSectionCount = 0;

	// Initialize the valid mesh slots, and prep them with the data for every mesh, LOD, and section we'll be needing over the lifetime of the renderer
	const uint32 MaxMeshes = Properties->Meshes.Num();
	Meshes.Empty(MaxMeshes);
	for (uint32 SourceMeshIndex = 0; SourceMeshIndex < MaxMeshes; ++SourceMeshIndex)
	{
		const auto& MeshProperties = Properties->Meshes[SourceMeshIndex];
		UStaticMesh* Mesh = MeshProperties.ResolveStaticMesh(Emitter);
		
		if (Mesh)
		{
			FMeshData& MeshData = Meshes.AddDefaulted_GetRef();
			MeshData.RenderData = Mesh->GetRenderData();
			MeshData.MinimumLOD = Mesh->GetMinLODIdx();
			MeshData.SourceMeshIndex = SourceMeshIndex;
			MeshData.PivotOffset = (FVector3f)MeshProperties.PivotOffset;
			MeshData.PivotOffsetSpace = MeshProperties.PivotOffsetSpace;
			MeshData.Scale = (FVector3f)MeshProperties.Scale;
			MeshData.Rotation = FQuat4f(MeshProperties.Rotation.Quaternion());
			MeshData.LocalBounds = Mesh->GetExtendedBounds().GetBox();

			// Create an index remap from mesh material index to it's index in the base material list
			TArray<UMaterialInterface*> MeshMaterials;
			Properties->GetUsedMeshMaterials(SourceMeshIndex, Emitter, MeshMaterials);
			for (auto MeshMaterial : MeshMaterials)
			{
				MeshData.MaterialRemapTable.Add(BaseMaterials_GT.IndexOfByPredicate([&](UMaterialInterface* LookMat)
				{
					if (LookMat == MeshMaterial)
					{
						return true;
					}
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(LookMat))
					{
						return MeshMaterial == MID->Parent;
					}
					return false;
				}));
			}

			// Determine the max section count for all LODs of this mesh and accumulate it on the max for all meshes
			uint32 MaxSectionCountThisMesh = 0;
			for (const auto& LODModel : MeshData.RenderData->LODResources)
			{
				MaxSectionCountThisMesh = FMath::Max<uint32>(MaxSectionCountThisMesh, LODModel.Sections.Num());
			}
			MaxSectionCount += MaxSectionCountThisMesh;
		}
	}

	checkf(Meshes.Num() > 0, TEXT("At least one valid mesh is required to instantiate a mesh renderer"));
}

void FNiagaraRendererMeshes::ReleaseRenderThreadResources()
{
}

void FNiagaraRendererMeshes::SetupVertexFactory(FNiagaraMeshVertexFactory& InVertexFactory, const FStaticMeshLODResources& LODResources) const
{
	FStaticMeshDataType Data;

	LODResources.VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&InVertexFactory, Data);
	LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&InVertexFactory, Data);
	LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer(&InVertexFactory, Data, MAX_TEXCOORDS);
	LODResources.VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&InVertexFactory, Data);
	InVertexFactory.SetData(Data);
}

int32 FNiagaraRendererMeshes::GetLODIndex(int32 MeshIndex) const
{
	check(IsInRenderingThread());
	check(Meshes.IsValidIndex(MeshIndex));

	const FMeshData& MeshData = Meshes[MeshIndex];
	const int32 LODIndex = MeshData.RenderData->GetCurrentFirstLODIdx(MeshData.MinimumLOD);

	return MeshData.RenderData->LODResources.IsValidIndex(LODIndex) ? LODIndex : INDEX_NONE;
}

void FNiagaraRendererMeshes::PrepareParticleMeshRenderData(FParticleMeshRenderData& ParticleMeshRenderData, const FSceneViewFamily& ViewFamily, FMeshElementCollector& Collector, FNiagaraDynamicDataBase* InDynamicData, const FNiagaraSceneProxy* SceneProxy, bool bRayTracing) const
{
	ParticleMeshRenderData.Collector = &Collector;

#if NIAGARA_ENABLE_GPU_SCENE_MESHES
	ParticleMeshRenderData.bUseGPUScene = !bRayTracing
		&& FeatureLevel > ERHIFeatureLevel::ES3_1	
		&& UseGPUScene(SceneProxy->GetScene().GetShaderPlatform(), FeatureLevel);
#else
	ParticleMeshRenderData.bUseGPUScene = false;
#endif

	// Anything to render?
	ParticleMeshRenderData.DynamicDataMesh = static_cast<FNiagaraDynamicDataMesh*>(InDynamicData);
	if (!ParticleMeshRenderData.DynamicDataMesh || !SceneProxy->GetComputeDispatchInterface())
	{
		return;
	}

	// Early out if we have no data or instances, this must be done before we read the material
	FNiagaraDataBuffer* CurrentParticleData = ParticleMeshRenderData.DynamicDataMesh->GetParticleDataToRender(bGpuLowLatencyTranslucency);
	if (!CurrentParticleData || (SourceMode == ENiagaraRendererSourceDataMode::Particles && CurrentParticleData->GetNumInstances() == 0) || !Meshes.Num())
	{
		return;
	}

	// Check if any materials are translucent and if we can pickup the low latency data
	ParticleMeshRenderData.bIsGpuLowLatencyTranslucency =
		bGpuLowLatencyTranslucency &&
		!SceneProxy->CastsVolumetricTranslucentShadow() &&
		ParticleMeshRenderData.DynamicDataMesh->Materials.Num() > 0 &&
		ParticleMeshRenderData.DynamicDataMesh->IsGpuLowLatencyTranslucencyEnabled();
	ParticleMeshRenderData.bHasTranslucentMaterials = false;
	for (FMaterialRenderProxy* MaterialProxy : ParticleMeshRenderData.DynamicDataMesh->Materials)
	{
		check(MaterialProxy);
		const FMaterial& Material = MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
		const EBlendMode BlendMode = Material.GetBlendMode();
		const bool bTranslucent = IsTranslucentBlendMode(BlendMode);

		ParticleMeshRenderData.bHasTranslucentMaterials |= bTranslucent;

		// If even one material can cause the mesh to render before FFXSystemInterface::PostRenderOpaque, we cannot use low latency data
		ParticleMeshRenderData.bIsGpuLowLatencyTranslucency =
			ParticleMeshRenderData.bIsGpuLowLatencyTranslucency &&
			!FFXRenderingUtils::CanMaterialRenderBeforeFXPostOpaque(ViewFamily, *SceneProxy, Material);
	}
	
	ParticleMeshRenderData.SourceParticleData = ParticleMeshRenderData.DynamicDataMesh->GetParticleDataToRender(ParticleMeshRenderData.bIsGpuLowLatencyTranslucency);
	
	// Anything to render?
	if ((ParticleMeshRenderData.SourceParticleData == nullptr) ||
		(SourceMode == ENiagaraRendererSourceDataMode::Particles && ParticleMeshRenderData.SourceParticleData->GetNumInstances() == 0) ||
		(Meshes.Num() == 0)
	)
	{
		ParticleMeshRenderData.SourceParticleData = nullptr;
		return;
	}

	// If the visibility tag comes from a parameter map, so we can evaluate it here and just early out if it doesn't match up
	if (EmitterRendererVisTagOffset != INDEX_NONE && ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.IsValidIndex(EmitterRendererVisTagOffset))
	{
		int32 VisTag = 0;
		FMemory::Memcpy(&VisTag, ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.GetData() + EmitterRendererVisTagOffset, sizeof(int32));
		if (RendererVisibility != VisTag)
		{
			ParticleMeshRenderData.SourceParticleData = nullptr;
			return;
		}
	}

	// Particle source mode
	if (SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		const EShaderPlatform ShaderPlatform = SceneProxy->GetComputeDispatchInterface()->GetShaderPlatform();

		// Determine if we need sorting
		ParticleMeshRenderData.bNeedsSort = SortMode != ENiagaraSortMode::None && (ParticleMeshRenderData.bHasTranslucentMaterials || !bSortOnlyWhenTranslucent);
		const bool bNeedCustomSort = ParticleMeshRenderData.bNeedsSort && (SortMode == ENiagaraSortMode::CustomAscending || SortMode == ENiagaraSortMode::CustomDecending);
		ParticleMeshRenderData.RendererLayout = bNeedCustomSort ? RendererLayoutWithCustomSorting : RendererLayoutWithoutCustomSorting;
		ParticleMeshRenderData.SortVariable = bNeedCustomSort ? ENiagaraMeshVFLayout::CustomSorting : ENiagaraMeshVFLayout::Position;
		if (ParticleMeshRenderData.bNeedsSort)
		{
			TConstArrayView<FNiagaraRendererVariableInfo> VFVariables = ParticleMeshRenderData.RendererLayout->GetVFVariables_RenderThread();
			const FNiagaraRendererVariableInfo& SortVariable = VFVariables[ParticleMeshRenderData.SortVariable];
			ParticleMeshRenderData.bNeedsSort = SortVariable.GetGPUOffset() != INDEX_NONE;
		}

		// Do we need culling? (When using GPU Scene, we don't cull in the sort passes)
		ParticleMeshRenderData.bNeedsCull = bEnableCulling && !ParticleMeshRenderData.bUseGPUScene;
		ParticleMeshRenderData.bSortCullOnGpu = (ParticleMeshRenderData.bNeedsSort && FNiagaraUtilities::AllowGPUSorting(ShaderPlatform)) || (ParticleMeshRenderData.bNeedsCull && FNiagaraUtilities::AllowGPUCulling(ShaderPlatform));

		// Validate what we setup
		if (SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			if (!ensureMsgf(!ParticleMeshRenderData.bNeedsCull || ParticleMeshRenderData.bSortCullOnGpu, TEXT("Culling is requested on GPU but we don't support sorting, this will result in incorrect rendering.")))
			{
				ParticleMeshRenderData.bNeedsCull = false;
			}
			ParticleMeshRenderData.bNeedsSort &= ParticleMeshRenderData.bSortCullOnGpu;

			//-TODO: Culling and sorting from InitViewsAfterPrePass can not be respected if the culled entries have already been acquired
			if ((ParticleMeshRenderData.bNeedsSort || ParticleMeshRenderData.bNeedsCull) && !SceneProxy->GetComputeDispatchInterface()->GetGPUInstanceCounterManager().CanAcquireCulledEntry())
			{
				//ensureMsgf(false, TEXT("Culling & sorting is not supported once the culled counts have been acquired, sorting & culling will be disabled for these draws."));
				ParticleMeshRenderData.bNeedsSort = false;
				ParticleMeshRenderData.bNeedsCull = false;
			}
		}
		else
		{
			//-TODO: Culling and sorting from InitViewsAfterPrePass can not be respected if the culled entries have already been acquired
			if (ParticleMeshRenderData.bSortCullOnGpu)
			{
				//ensureMsgf(false, TEXT("Culling & sorting is not supported once the culled counts have been acquired, sorting & culling will be disabled for these draws."));
				ParticleMeshRenderData.bSortCullOnGpu &= SceneProxy->GetComputeDispatchInterface()->GetGPUInstanceCounterManager().CanAcquireCulledEntry();
			}

			// For CPU sims, decide if we should sort / cull on the GPU or not
			if ( ParticleMeshRenderData.bSortCullOnGpu )
			{
				const int32 NumInstances = ParticleMeshRenderData.SourceParticleData->GetNumInstances();

				const int32 SortThreshold = GNiagaraGPUSortingCPUToGPUThreshold;
				const bool bSortMoveToGpu = (SortThreshold >= 0) && (NumInstances >= SortThreshold);

				const int32 CullThreshold = GNiagaraGPUCullingCPUToGPUThreshold;
				const bool bCullMoveToGpu = (CullThreshold >= 0) && (NumInstances >= CullThreshold);

				ParticleMeshRenderData.bSortCullOnGpu = bSortMoveToGpu || bCullMoveToGpu;
			}
		}

		// Update layout as it could have changed
		ParticleMeshRenderData.RendererLayout = bNeedCustomSort ? RendererLayoutWithCustomSorting : RendererLayoutWithoutCustomSorting;
	}
}

void FNiagaraRendererMeshes::PrepareParticleRenderBuffers(FParticleMeshRenderData& ParticleMeshRenderData, FGlobalDynamicReadBuffer& DynamicReadBuffer) const
{
	if ( SourceMode == ENiagaraRendererSourceDataMode::Particles )
	{
		if ( SimTarget == ENiagaraSimTarget::CPUSim )
		{
			// Determine what integer attributes we need to copy to the GPU for culling
			TArray<uint32, TInlineAllocator<2>> IntParamsToCopy;
			if (ParticleMeshRenderData.bNeedsCull || ParticleMeshRenderData.bUseGPUScene)
			{
				if (ParticleMeshRenderData.bSortCullOnGpu || ParticleMeshRenderData.bUseGPUScene)
				{
					if (ParticleRendererVisTagOffset != INDEX_NONE)
					{
						ParticleMeshRenderData.RendererVisTagOffset = IntParamsToCopy.Add(ParticleRendererVisTagOffset);
					}
					if (ParticleMeshIndexOffset != INDEX_NONE)
					{
						ParticleMeshRenderData.MeshIndexOffset = IntParamsToCopy.Add(ParticleMeshIndexOffset);
					}
				}
				else
				{
					ParticleMeshRenderData.RendererVisTagOffset = ParticleRendererVisTagOffset;
					ParticleMeshRenderData.MeshIndexOffset = ParticleMeshIndexOffset;
				}
			}

			FParticleRenderData ParticleRenderData = TransferDataToGPU(DynamicReadBuffer, ParticleMeshRenderData.RendererLayout, IntParamsToCopy, ParticleMeshRenderData.SourceParticleData);
			const uint32 NumInstances = ParticleMeshRenderData.SourceParticleData->GetNumInstances();

			ParticleMeshRenderData.ParticleFloatSRV = GetSrvOrDefaultFloat(ParticleRenderData.FloatData);
			ParticleMeshRenderData.ParticleHalfSRV = GetSrvOrDefaultHalf(ParticleRenderData.HalfData);
			ParticleMeshRenderData.ParticleIntSRV = GetSrvOrDefaultInt(ParticleRenderData.IntData);
			ParticleMeshRenderData.ParticleFloatDataStride = ParticleRenderData.FloatStride / sizeof(float);
			ParticleMeshRenderData.ParticleHalfDataStride = ParticleRenderData.HalfStride / sizeof(FFloat16);
			ParticleMeshRenderData.ParticleIntDataStride = ParticleRenderData.IntStride / sizeof(int32);
		}
		else
		{
			ParticleMeshRenderData.ParticleFloatSRV = GetSrvOrDefaultFloat(ParticleMeshRenderData.SourceParticleData->GetGPUBufferFloat());
			ParticleMeshRenderData.ParticleHalfSRV = GetSrvOrDefaultHalf(ParticleMeshRenderData.SourceParticleData->GetGPUBufferHalf());
			ParticleMeshRenderData.ParticleIntSRV = GetSrvOrDefaultInt(ParticleMeshRenderData.SourceParticleData->GetGPUBufferInt());
			ParticleMeshRenderData.ParticleFloatDataStride = ParticleMeshRenderData.SourceParticleData->GetFloatStride() / sizeof(float);
			ParticleMeshRenderData.ParticleHalfDataStride = ParticleMeshRenderData.SourceParticleData->GetHalfStride() / sizeof(FFloat16);
			ParticleMeshRenderData.ParticleIntDataStride = ParticleMeshRenderData.SourceParticleData->GetInt32Stride() / sizeof(int32);

			ParticleMeshRenderData.RendererVisTagOffset = ParticleRendererVisTagOffset;
			ParticleMeshRenderData.MeshIndexOffset = ParticleMeshIndexOffset;
		}
	}
	else
	{
		ParticleMeshRenderData.ParticleFloatSRV = FNiagaraRenderer::GetDummyFloatBuffer();
		ParticleMeshRenderData.ParticleHalfSRV = FNiagaraRenderer::GetDummyHalfBuffer();
		ParticleMeshRenderData.ParticleIntSRV = FNiagaraRenderer::GetDummyIntBuffer();
		ParticleMeshRenderData.ParticleFloatDataStride = 0;
		ParticleMeshRenderData.ParticleHalfDataStride = 0;
		ParticleMeshRenderData.ParticleIntDataStride = 0;
	}
}

void FNiagaraRendererMeshes::InitializeSortInfo(const FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraSceneProxy& SceneProxy, const FSceneView& View, int32 ViewIndex, bool bIsInstancedStereo, FNiagaraGPUSortInfo& OutSortInfo) const
{
	TConstArrayView<FNiagaraRendererVariableInfo> VFVariables = ParticleMeshRenderData.RendererLayout->GetVFVariables_RenderThread();

	OutSortInfo.ParticleCount = ParticleMeshRenderData.SourceParticleData->GetNumInstances();
	OutSortInfo.SortMode = SortMode;
	OutSortInfo.SetSortFlags(bSortHighPrecision, ParticleMeshRenderData.SourceParticleData->GetGPUDataReadyStage());
	OutSortInfo.bEnableCulling = ParticleMeshRenderData.bNeedsCull;
	OutSortInfo.RendererVisTagAttributeOffset = ParticleMeshRenderData.RendererVisTagOffset;
	OutSortInfo.MeshIndexAttributeOffset = ParticleMeshRenderData.MeshIndexOffset;
	OutSortInfo.RendererVisibility = RendererVisibility;
	OutSortInfo.DistanceCullRange = DistanceCullRange;
	OutSortInfo.SystemLWCTile = UseLocalSpace(&SceneProxy) ? FVector3f::Zero() :SceneProxy.GetLWCRenderTile();

	if (ParticleMeshRenderData.bSortCullOnGpu)
	{
		FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = SceneProxy.GetComputeDispatchInterface();

		OutSortInfo.ParticleDataFloatSRV = ParticleMeshRenderData.ParticleFloatSRV;
		OutSortInfo.ParticleDataHalfSRV = ParticleMeshRenderData.ParticleHalfSRV;
		OutSortInfo.ParticleDataIntSRV = ParticleMeshRenderData.ParticleIntSRV;
		OutSortInfo.FloatDataStride = ParticleMeshRenderData.ParticleFloatDataStride;
		OutSortInfo.HalfDataStride = ParticleMeshRenderData.ParticleHalfDataStride;
		OutSortInfo.IntDataStride = ParticleMeshRenderData.ParticleIntDataStride;
		OutSortInfo.GPUParticleCountSRV = GetSrvOrDefaultUInt(ComputeDispatchInterface->GetGPUInstanceCounterManager().GetInstanceCountBuffer());
		OutSortInfo.GPUParticleCountOffset = ParticleMeshRenderData.SourceParticleData->GetGPUInstanceCountBufferOffset();
	}

	if (ParticleMeshRenderData.SortVariable != INDEX_NONE)
	{
		const FNiagaraRendererVariableInfo& SortVariable = VFVariables[ParticleMeshRenderData.SortVariable];
		OutSortInfo.SortAttributeOffset = ParticleMeshRenderData.bSortCullOnGpu ? SortVariable.GetGPUOffset() : SortVariable.GetEncodedDatasetOffset();
	}

	auto GetViewMatrices = [](const FSceneView& View, FVector& OutViewOrigin) -> const FViewMatrices&
	{
		OutViewOrigin = View.ViewMatrices.GetViewOrigin();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FSceneViewState* ViewState = View.State != nullptr ? View.State->GetConcreteViewState() : nullptr;
		if (ViewState && ViewState->bIsFrozen && ViewState->bIsFrozenViewMatricesCached)
		{
			// Use the frozen view for culling so we can test that it's working
			OutViewOrigin = ViewState->CachedViewMatrices.GetViewOrigin();

			// Don't retrieve the cached matrices for shadow views
			bool bIsShadowView = View.GetDynamicMeshElementsShadowCullFrustum() != nullptr;
			if (!bIsShadowView)
			{
				return ViewState->CachedViewMatrices;
			}
		}
#endif

		return View.ViewMatrices;
	};

	const TArray<const FSceneView*>& AllViewsInFamily = View.Family->Views;
	const FViewMatrices& ViewMatrices = GetViewMatrices(View, OutSortInfo.ViewOrigin);
	OutSortInfo.ViewDirection = ViewMatrices.GetViewMatrix().GetColumn(2);

	if (bIsInstancedStereo)
	{
		// For VR, do distance culling and sorting from a central eye position to prevent differences between views
		OutSortInfo.ViewOrigin = View.CullingOrigin;
	}

	if (bEnableFrustumCulling)
	{
		if (const FConvexVolume* ShadowFrustum = View.GetDynamicMeshElementsShadowCullFrustum())
		{
			// Ensure we don't break the maximum number of planes here
			// (For an accurate shadow frustum, a tight hull is formed from the silhouette and back-facing planes of the view frustum)
			check(ShadowFrustum->Planes.Num() <= FNiagaraGPUSortInfo::MaxCullPlanes);
			OutSortInfo.CullPlanes = ShadowFrustum->Planes;

			// Remove pre-shadow translation to get the planes in world space
			const FVector PreShadowTranslation = View.GetPreShadowTranslation();
			for (FPlane& Plane : OutSortInfo.CullPlanes)
			{
				Plane.W -= FVector::DotProduct(FVector(Plane), PreShadowTranslation);
			}
		}
		else
		{
			if (bIsInstancedStereo)
			{
				// For Instanced Stereo, cull using an extended frustum that encompasses both eyes
				OutSortInfo.CullPlanes = View.CullingFrustum.Planes;
			}
			else
			{
				OutSortInfo.CullPlanes = View.ViewFrustum.Planes;
			}
		}
	}

	if (UseLocalSpace(&SceneProxy))
	{
		OutSortInfo.ViewOrigin = SceneProxy.GetLocalToWorldInverse().TransformPosition(OutSortInfo.ViewOrigin);
		OutSortInfo.ViewDirection = SceneProxy.GetLocalToWorld().GetTransposed().TransformVector(OutSortInfo.ViewDirection);
		if (bEnableFrustumCulling)
		{
			for (FPlane& Plane : OutSortInfo.CullPlanes)
			{
				Plane = Plane.TransformBy(SceneProxy.GetLocalToWorldInverse());
			}
		}
	}

	if (ParticleMeshRenderData.bNeedsCull)
	{
		if ( ParticleMeshRenderData.bSortCullOnGpu )
		{
			OutSortInfo.CullPositionAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Position].GetGPUOffset();
			OutSortInfo.CullOrientationAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Rotation].GetGPUOffset();
			OutSortInfo.CullScaleAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Scale].GetGPUOffset();
		}
		else
		{
			OutSortInfo.CullPositionAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Position].GetEncodedDatasetOffset();
			OutSortInfo.CullOrientationAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Rotation].GetEncodedDatasetOffset();
			OutSortInfo.CullScaleAttributeOffset = VFVariables[ENiagaraMeshVFLayout::Scale].GetEncodedDatasetOffset();
		}
	}
}

void FNiagaraRendererMeshes::PreparePerMeshData(FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraMeshVertexFactory& VertexFactory, const FNiagaraSceneProxy& SceneProxy, const FMeshData& MeshData) const
{
	// Calculate pivot offset / culling sphere
	FBox MeshLocalBounds = MeshData.LocalBounds;
	MeshLocalBounds.Min *= (FVector)MeshData.Scale;
	MeshLocalBounds.Max *= (FVector)MeshData.Scale;
	ParticleMeshRenderData.CullingSphere.Center = MeshLocalBounds.GetCenter();
	ParticleMeshRenderData.CullingSphere.W = MeshLocalBounds.GetExtent().Length();

	if (MeshData.PivotOffsetSpace == ENiagaraMeshPivotOffsetSpace::Mesh)
	{
		ParticleMeshRenderData.WorldSpacePivotOffset = FVector::ZeroVector;
		ParticleMeshRenderData.CullingSphere.Center += (FVector)MeshData.PivotOffset;
	}
	else
	{
		ParticleMeshRenderData.WorldSpacePivotOffset = (FVector)MeshData.PivotOffset;
		if (MeshData.PivotOffsetSpace == ENiagaraMeshPivotOffsetSpace::Local ||
			(bLocalSpace && MeshData.PivotOffsetSpace == ENiagaraMeshPivotOffsetSpace::Simulation))
		{
			// The offset is in local space, transform it to world
			ParticleMeshRenderData.WorldSpacePivotOffset = SceneProxy.GetLocalToWorld().TransformVector(ParticleMeshRenderData.WorldSpacePivotOffset);
		}
	}

	if (RHISupportsManualVertexFetch(GShaderPlatformForFeatureLevel[FeatureLevel]))
	{
		const int32 NumTexCoords = VertexFactory.GetNumTexcoords();
		const int32 ColorIndexMask = VertexFactory.GetColorIndexMask();

		ParticleMeshRenderData.VertexFetch_Parameters = { ColorIndexMask, NumTexCoords, INDEX_NONE, INDEX_NONE };
		ParticleMeshRenderData.TexCoordBufferSrv = VertexFactory.GetTextureCoordinatesSRV();
		ParticleMeshRenderData.PackedTangentsBufferSrv = VertexFactory.GetTangentsSRV();
		ParticleMeshRenderData.ColorComponentsBufferSrv = VertexFactory.GetColorComponentsSRV();
	}
	else
	{
		ParticleMeshRenderData.VertexFetch_Parameters = FIntVector4(INDEX_NONE);
		ParticleMeshRenderData.TexCoordBufferSrv = FNiagaraRenderer::GetDummyFloat2Buffer();
		ParticleMeshRenderData.PackedTangentsBufferSrv = FNiagaraRenderer::GetDummyFloat4Buffer();
		ParticleMeshRenderData.ColorComponentsBufferSrv = FNiagaraRenderer::GetDummyFloat4Buffer();
	}
}

uint32 FNiagaraRendererMeshes::PerformSortAndCull(FParticleMeshRenderData& ParticleMeshRenderData, FGlobalDynamicReadBuffer& ReadBuffer, FNiagaraGPUSortInfo& SortInfo, FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface, int32 MeshIndex) const
{
	// Emitter mode culls earlier on
	if (SourceMode == ENiagaraRendererSourceDataMode::Emitter)
	{
		ParticleMeshRenderData.ParticleSortedIndicesSRV = GFNiagaraNullSortedIndicesVertexBuffer.VertexBufferSRV.GetReference();
		ParticleMeshRenderData.ParticleSortedIndicesOffset = 0xffffffff;
		return 1;
	}

	uint32 NumInstances = ParticleMeshRenderData.SourceParticleData->GetNumInstances();
	if (ParticleMeshRenderData.bNeedsCull || ParticleMeshRenderData.bNeedsSort)
	{
		SortInfo.LocalBSphere = ParticleMeshRenderData.CullingSphere;
		SortInfo.CullingWorldSpaceOffset = ParticleMeshRenderData.WorldSpacePivotOffset;
		SortInfo.MeshIndex = MeshIndex;
		if (ParticleMeshRenderData.bSortCullOnGpu)
		{
			SortInfo.CulledGPUParticleCountOffset = ParticleMeshRenderData.bNeedsCull ? ComputeDispatchInterface->GetGPUInstanceCounterManager().AcquireCulledEntry() : INDEX_NONE;
			if (ComputeDispatchInterface->AddSortedGPUSimulation(SortInfo))
			{
				ParticleMeshRenderData.ParticleSortedIndicesSRV = SortInfo.AllocationInfo.BufferSRV;
				ParticleMeshRenderData.ParticleSortedIndicesOffset = SortInfo.AllocationInfo.BufferOffset;
			}
		}
		else
		{
			FGlobalDynamicReadBuffer::FAllocation SortedIndices;
			SortedIndices = ReadBuffer.AllocateUInt32(NumInstances);
			NumInstances = SortAndCullIndices(SortInfo, *ParticleMeshRenderData.SourceParticleData, SortedIndices);
			ParticleMeshRenderData.ParticleSortedIndicesSRV = SortedIndices.SRV;
			ParticleMeshRenderData.ParticleSortedIndicesOffset = 0;
		}
	}
	else
	{
		ParticleMeshRenderData.ParticleSortedIndicesSRV = GFNiagaraNullSortedIndicesVertexBuffer.VertexBufferSRV.GetReference();
		ParticleMeshRenderData.ParticleSortedIndicesOffset = 0xffffffff;
	}
	return NumInstances;
}

FNiagaraMeshCommonParameters FNiagaraRendererMeshes::CreateCommonShaderParams(const FParticleMeshRenderData& ParticleMeshRenderData, const FSceneView& View, const FMeshData& MeshData, const FNiagaraSceneProxy& SceneProxy) const
{
	const bool bUseLocalSpace = UseLocalSpace(&SceneProxy);

	FNiagaraMeshCommonParameters Params;
	Params.NiagaraParticleDataFloat	= ParticleMeshRenderData.ParticleFloatSRV;
	Params.NiagaraParticleDataHalf 	= ParticleMeshRenderData.ParticleHalfSRV;
	Params.NiagaraParticleDataInt 	= ParticleMeshRenderData.ParticleIntSRV;
	Params.NiagaraFloatDataStride 	= FMath::Max(ParticleMeshRenderData.ParticleFloatDataStride, ParticleMeshRenderData.ParticleHalfDataStride);
	Params.NiagaraIntDataStride 	= ParticleMeshRenderData.ParticleIntDataStride;
	
	Params.SortedIndices 			= ParticleMeshRenderData.ParticleSortedIndicesSRV;
	Params.SortedIndicesOffset 		= ParticleMeshRenderData.ParticleSortedIndicesOffset;	

	Params.SystemLWCTile			= SceneProxy.GetLWCRenderTile();
	Params.bLocalSpace 				= bUseLocalSpace;
	Params.AccurateMotionVectors	= bAccurateMotionVectors;
	Params.DeltaSeconds 			= View.Family->Time.GetDeltaWorldTimeSeconds();
	Params.FacingMode 				= (uint32)FacingMode;	

	Params.MeshScale				= MeshData.Scale;	
	Params.MeshRotation				= FVector4f(MeshData.Rotation.X, MeshData.Rotation.Y, MeshData.Rotation.Z, MeshData.Rotation.W);
	if (MeshData.PivotOffsetSpace == ENiagaraMeshPivotOffsetSpace::Mesh)
	{
		Params.MeshOffset 				= MeshData.PivotOffset;
		Params.bMeshOffsetIsWorldSpace	= false;
	}
	else
	{
		Params.MeshOffset 				= (FVector3f)ParticleMeshRenderData.WorldSpacePivotOffset;
		Params.bMeshOffsetIsWorldSpace	= true;
	}

	Params.bLockedAxisEnable			= bLockedAxisEnable;
	Params.LockedAxis 					= (FVector3f)LockedAxis;
	Params.LockedAxisSpace				= (uint32)LockedAxisSpace;

	if (bUseLocalSpace)
	{
		Params.DefaultPosition = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		Params.DefaultPosition = FVector3f(SceneProxy.GetLocalToWorld().GetOrigin() - FVector(SceneProxy.GetLWCRenderTile()) * FLargeWorldRenderScalar::GetTileSize());
	}

	Params.DefaultPrevPosition			= Params.DefaultPosition;
	Params.DefaultVelocity 				= FVector3f(0.f, 0.0f, 0.0f);
	Params.DefaultPrevVelocity			= Params.DefaultVelocity;	
	Params.DefaultScale					= FVector3f(1.0f, 1.0f, 1.0f);
	Params.DefaultPrevScale 			= Params.DefaultScale;
	Params.DefaultRotation 				= FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
	Params.DefaultPrevRotation 			= Params.DefaultRotation;
	Params.DefaultCameraOffset 			= 0.0f;
	Params.DefaultPrevCameraOffset 		= Params.DefaultCameraOffset;

	Params.PrevScaleDataOffset 			= INDEX_NONE;
	Params.PrevRotationDataOffset 		= INDEX_NONE;
	Params.PrevPositionDataOffset 		= INDEX_NONE;
	Params.PrevVelocityDataOffset 		= INDEX_NONE;
	Params.PrevCameraOffsetDataOffset	= INDEX_NONE;

	if (SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		TConstArrayView<FNiagaraRendererVariableInfo> VFVariables = ParticleMeshRenderData.RendererLayout->GetVFVariables_RenderThread();
		Params.ScaleDataOffset 			= VFVariables[ENiagaraMeshVFLayout::Scale].GetGPUOffset();
		Params.RotationDataOffset 		= VFVariables[ENiagaraMeshVFLayout::Rotation].GetGPUOffset();
		Params.PositionDataOffset 		= VFVariables[ENiagaraMeshVFLayout::Position].GetGPUOffset();
		Params.VelocityDataOffset 		= VFVariables[ENiagaraMeshVFLayout::Velocity].GetGPUOffset();
		Params.CameraOffsetDataOffset 	= VFVariables[ENiagaraMeshVFLayout::CameraOffset].GetGPUOffset();
		
		if (bAccurateMotionVectors)
		{
			Params.PrevScaleDataOffset			= VFVariables[ENiagaraMeshVFLayout::PrevScale].GetGPUOffset();
			Params.PrevRotationDataOffset 		= VFVariables[ENiagaraMeshVFLayout::PrevRotation].GetGPUOffset();
			Params.PrevPositionDataOffset 		= VFVariables[ENiagaraMeshVFLayout::PrevPosition].GetGPUOffset();
			Params.PrevVelocityDataOffset 		= VFVariables[ENiagaraMeshVFLayout::PrevVelocity].GetGPUOffset();
			Params.PrevCameraOffsetDataOffset 	= VFVariables[ENiagaraMeshVFLayout::PrevCameraOffset].GetGPUOffset();
		}
	}
	else if (SourceMode == ENiagaraRendererSourceDataMode::Emitter) // Clear all these out because we will be using the defaults to specify them
	{
		Params.ScaleDataOffset 			= INDEX_NONE;
		Params.RotationDataOffset 		= INDEX_NONE;
		Params.PositionDataOffset 		= INDEX_NONE;
		Params.VelocityDataOffset 		= INDEX_NONE;
		Params.CameraOffsetDataOffset 	= INDEX_NONE;
	}
	else
	{
		// Unsupported source data mode detected
		check(SourceMode <= ENiagaraRendererSourceDataMode::Emitter);
	}

	if (bSetAnyBoundVars)
	{
		const FNiagaraDynamicDataMesh* DynamicDataMesh = ParticleMeshRenderData.DynamicDataMesh;
		const uint8* ParameterBoundData = DynamicDataMesh->ParameterDataBound.GetData();

		const int32 NumVFOffsets = bAccurateMotionVectors ? ENiagaraMeshVFLayout::Type::Num_Max : ENiagaraMeshVFLayout::Type::Num_Default;
		for (int32 i = 0; i < NumVFOffsets; i++)
		{
			if (VFBoundOffsetsInParamStore[i] != INDEX_NONE && DynamicDataMesh->ParameterDataBound.IsValidIndex(VFBoundOffsetsInParamStore[i]))
			{
				switch (i)
				{
				case ENiagaraMeshVFLayout::Type::Position:
					FMemory::Memcpy(&Params.DefaultPosition, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector3f));
					break;
				case ENiagaraMeshVFLayout::Type::Velocity:
					FMemory::Memcpy(&Params.DefaultVelocity, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector3f));
					break;
				case ENiagaraMeshVFLayout::Type::Scale:
					FMemory::Memcpy(&Params.DefaultScale, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector3f));
					break;
				case ENiagaraMeshVFLayout::Type::Rotation:
					FMemory::Memcpy(&Params.DefaultRotation, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					break;
				case ENiagaraMeshVFLayout::Type::CameraOffset:
					FMemory::Memcpy(&Params.DefaultCameraOffset, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(float));
					break;
				case ENiagaraMeshVFLayout::Type::PrevPosition:
					FMemory::Memcpy(&Params.DefaultPrevPosition, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector3f));
					break;
				case ENiagaraMeshVFLayout::Type::PrevScale:
					FMemory::Memcpy(&Params.DefaultPrevScale, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector3f));
					break;
				case ENiagaraMeshVFLayout::Type::PrevRotation:
					FMemory::Memcpy(&Params.DefaultPrevRotation, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					break;
				case ENiagaraMeshVFLayout::Type::PrevCameraOffset:
					FMemory::Memcpy(&Params.DefaultPrevCameraOffset, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(float));
					break;
				default:
					break;
				}
			}
			else
			{
				// If these prev values aren't bound to the host parameters, but their current values are, copy them
				switch (i)
				{
				case ENiagaraMeshVFLayout::Type::PrevPosition:
					Params.DefaultPrevPosition = Params.DefaultPosition;
					break;
				case ENiagaraMeshVFLayout::Type::PrevScale:
					Params.DefaultPrevScale = Params.DefaultScale;
					break;
				case ENiagaraMeshVFLayout::Type::PrevRotation:
					Params.DefaultPrevRotation = Params.DefaultRotation;
					break;
				case ENiagaraMeshVFLayout::Type::PrevCameraOffset:
					Params.DefaultPrevCameraOffset = Params.DefaultCameraOffset;
					break;
				default:
					break;
				}
			}
		}
	}

	return Params;
}

FNiagaraMeshUniformBufferRef FNiagaraRendererMeshes::CreateVFUniformBuffer(const FParticleMeshRenderData& ParticleMeshRenderData, const FNiagaraMeshCommonParameters& CommonParameters) const
{
	// Compute the vertex factory uniform buffer.
	FNiagaraMeshUniformParameters Params;
	FMemory::Memzero(&Params, sizeof(Params)); // Clear unset bytes

	Params.Common 								= CommonParameters;

	Params.MaterialParamValidMask 				= MaterialParamValidMask;
	Params.SubImageSize 						= FVector4f(SubImageSize.X, SubImageSize.Y, 1.0f / SubImageSize.X, 1.0f / SubImageSize.Y);
	Params.SubImageBlendMode 					= bSubImageBlend;

	Params.DefaultNormAge 						= 0.0f;
	Params.DefaultSubImage 						= 0.0f;
	Params.DefaultMatRandom 					= 0.0f;
	Params.DefaultColor							= FVector4f(1.0f, 1.0f, 1.0f, 1.0f);				
	Params.DefaultDynamicMaterialParameter0 	= FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
	Params.DefaultDynamicMaterialParameter1 	= FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
	Params.DefaultDynamicMaterialParameter2 	= FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
	Params.DefaultDynamicMaterialParameter3 	= FVector4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		TConstArrayView<FNiagaraRendererVariableInfo> VFVariables = ParticleMeshRenderData.RendererLayout->GetVFVariables_RenderThread();
		Params.ColorDataOffset 			= VFVariables[ENiagaraMeshVFLayout::Color].GetGPUOffset();
		Params.MaterialRandomDataOffset = VFVariables[ENiagaraMeshVFLayout::MaterialRandom].GetGPUOffset();
		Params.NormalizedAgeDataOffset 	= VFVariables[ENiagaraMeshVFLayout::NormalizedAge].GetGPUOffset();
		Params.SubImageDataOffset 		= VFVariables[ENiagaraMeshVFLayout::SubImage].GetGPUOffset();
		Params.MaterialParamDataOffset 	= VFVariables[ENiagaraMeshVFLayout::DynamicParam0].GetGPUOffset();
		Params.MaterialParam1DataOffset	= VFVariables[ENiagaraMeshVFLayout::DynamicParam1].GetGPUOffset();
		Params.MaterialParam2DataOffset	= VFVariables[ENiagaraMeshVFLayout::DynamicParam2].GetGPUOffset();
		Params.MaterialParam3DataOffset	= VFVariables[ENiagaraMeshVFLayout::DynamicParam3].GetGPUOffset();
	}	
	else if (SourceMode == ENiagaraRendererSourceDataMode::Emitter) // Clear all these out because we will be using the defaults to specify them
	{
		Params.ColorDataOffset 			= INDEX_NONE;
		Params.MaterialRandomDataOffset = INDEX_NONE;
		Params.NormalizedAgeDataOffset 	= INDEX_NONE;
		Params.SubImageDataOffset 		= INDEX_NONE;
		Params.MaterialParamDataOffset 	= INDEX_NONE;
		Params.MaterialParam1DataOffset = INDEX_NONE;
		Params.MaterialParam2DataOffset = INDEX_NONE;
		Params.MaterialParam3DataOffset = INDEX_NONE;
	}
	else
	{
		// Unsupported source data mode detected
		check(SourceMode <= ENiagaraRendererSourceDataMode::Emitter);
	}

	Params.VertexFetch_Parameters = ParticleMeshRenderData.VertexFetch_Parameters;
	Params.VertexFetch_TexCoordBuffer = ParticleMeshRenderData.TexCoordBufferSrv;
	Params.VertexFetch_PackedTangentsBuffer = ParticleMeshRenderData.PackedTangentsBufferSrv;
	Params.VertexFetch_ColorComponentsBuffer = ParticleMeshRenderData.ColorComponentsBufferSrv;

	if (bSetAnyBoundVars)
	{
		const FNiagaraDynamicDataMesh* DynamicDataMesh = ParticleMeshRenderData.DynamicDataMesh;
		const uint8* ParameterBoundData = DynamicDataMesh->ParameterDataBound.GetData();

		const int32 NumVFOffsets = bAccurateMotionVectors ? ENiagaraMeshVFLayout::Type::Num_Max : ENiagaraMeshVFLayout::Type::Num_Default;
		for (int32 i = 0; i < NumVFOffsets; i++)
		{
			if (VFBoundOffsetsInParamStore[i] != INDEX_NONE && DynamicDataMesh->ParameterDataBound.IsValidIndex(VFBoundOffsetsInParamStore[i]))
			{
				switch (i)
				{
				case ENiagaraMeshVFLayout::Type::NormalizedAge:
					FMemory::Memcpy(&Params.DefaultNormAge, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(float));
					break;				
				case ENiagaraMeshVFLayout::Type::SubImage:
					FMemory::Memcpy(&Params.DefaultSubImage, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(float));
					break;
				case ENiagaraMeshVFLayout::Type::Color:
					FMemory::Memcpy(&Params.DefaultColor, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FLinearColor));
					break;
				case ENiagaraMeshVFLayout::Type::DynamicParam0:
					FMemory::Memcpy(&Params.DefaultDynamicMaterialParameter0, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					Params.MaterialParamValidMask |= 0x1;
					break;
				case ENiagaraMeshVFLayout::Type::DynamicParam1:
					FMemory::Memcpy(&Params.DefaultDynamicMaterialParameter1, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					Params.MaterialParamValidMask |= 0x2;
					break;
				case ENiagaraMeshVFLayout::Type::DynamicParam2:
					FMemory::Memcpy(&Params.DefaultDynamicMaterialParameter2, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					Params.MaterialParamValidMask |= 0x4;
					break;
				case ENiagaraMeshVFLayout::Type::DynamicParam3:
					FMemory::Memcpy(&Params.DefaultDynamicMaterialParameter3, ParameterBoundData + VFBoundOffsetsInParamStore[i], sizeof(FVector4f));
					Params.MaterialParamValidMask |= 0x8;
					break;
				case ENiagaraMeshVFLayout::Type::CustomSorting:
					// unsupported for now...
					break;
				default:
					break;
				}
			}
		}
	}

	return FNiagaraMeshUniformBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_SingleFrame);
}

void FNiagaraRendererMeshes::SetupElementForGPUScene(
	const FParticleMeshRenderData& ParticleMeshRenderData,
	const FNiagaraMeshCommonParameters& CommonParameters,
	const FNiagaraSceneProxy& SceneProxy,
	const FMeshData& MeshData,
	const FSceneView& View,
	uint32 NumInstances,
	bool bNeedsPrevTransform,
	FMeshBatchElement& OutMeshBatchElement
) const
{
	FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = SceneProxy.GetComputeDispatchInterface();
	check(ComputeDispatchInterface);

	FGPUSceneUpdateResource& GPUSceneRes = ParticleMeshRenderData.Collector->AllocateOneFrameResource<FGPUSceneUpdateResource>(FeatureLevel, bAccurateMotionVectors != 0);

	GPUSceneRes.DynamicPrimitiveData.EnableInstanceDynamicData(bNeedsPrevTransform);
	GPUSceneRes.DynamicPrimitiveData.SetNumInstanceCustomDataFloats(1);

	if (SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		FMemory::Memzero(&GPUSceneRes.GPUWriteParams, sizeof(GPUSceneRes.GPUWriteParams));

		GPUSceneRes.GPUWriteParams.Common 					= CommonParameters;
		GPUSceneRes.GPUWriteParams.ParticleCount 			= NumInstances;
		GPUSceneRes.GPUWriteParams.GPUParticleCountBuffer 	= GetSrvOrDefaultUInt(ComputeDispatchInterface->GetGPUInstanceCounterManager().GetInstanceCountBuffer());
		GPUSceneRes.GPUWriteParams.GPUParticleCountOffset 	= ParticleMeshRenderData.SourceParticleData->GetGPUInstanceCountBufferOffset();
		GPUSceneRes.GPUWriteParams.MeshIndex 				= MeshData.SourceMeshIndex;
		GPUSceneRes.GPUWriteParams.MeshIndexDataOffset 		= ParticleMeshRenderData.MeshIndexOffset;
		GPUSceneRes.GPUWriteParams.RendererVisibility 		= RendererVisibility;
		GPUSceneRes.GPUWriteParams.VisibilityTagDataOffset 	= ParticleMeshRenderData.RendererVisTagOffset;
		GPUSceneRes.GPUWriteParams.LocalBoundingCenter		= (FVector3f)MeshData.LocalBounds.GetCenter();
		GPUSceneRes.GPUWriteParams.DistanceCullRangeSquared = DistanceCullRangeSquared;
		GPUSceneRes.GPUWriteParams.bNeedsPrevTransform		= bNeedsPrevTransform ? 1 : 0;

		// We need to set this flag to force the system to always cull individual instances, because we may need to discard instances that are:
		// * Not tied to any live particles
		// * Are distance culled
		// * Are culled because of mismatched MeshIndex
		// * Are culled because of mismatched VisibilityTag
		OutMeshBatchElement.bForceInstanceCulling = true;

		// Force it to preserve instance order if we are sorted so that GPU Scene's instance culling doesn't scramble them
		OutMeshBatchElement.bPreserveInstanceOrder = ParticleMeshRenderData.bNeedsSort;

		GPUSceneRes.DynamicPrimitiveData.DataWriterGPU = FGPUSceneWriteDelegate::CreateLambda(				
			[&GPUSceneRes](FRDGBuilder& GraphBuilder, const FGPUSceneWriteDelegateParams& Params)
			{
				GPUSceneRes.GPUWriteParams.GPUSceneWriterParameters	= Params.GPUWriteParams;
				GPUSceneRes.GPUWriteParams.View						= Params.View->ViewUniformBuffer; // NOTE: Set here, not outside lambda
				GPUSceneRes.GPUWriteParams.PrimitiveId 				= Params.PrimitiveId;
		
				FNiagaraGPUSceneUtils::AddUpdateMeshParticleInstancesPass(
					GraphBuilder,
					GPUSceneRes.GPUWriteParams,
					GPUSceneRes.FeatureLevel,
					GPUSceneRes.bPreciseMotionVectors
				);
			}
		);

		if (ParticleMeshRenderData.bIsGpuLowLatencyTranslucency)
		{
			GPUSceneRes.DynamicPrimitiveData.DataWriterGPUPass = EGPUSceneGPUWritePass::PostOpaqueRendering;
		}
	}
	else
	{
		// No point in writing the data for a single instance on the GPU - we can just set this info here
		const FVector OriginOffset = FVector(SceneProxy.GetLWCRenderTile()) * FLargeWorldRenderScalar::GetTileSize();
		
		FEmitterSourceInstanceData& EmitterSourceData = GPUSceneRes.EmitterSourceData;
		EmitterSourceData.CustomData = 0.0f;		

		const FVector4 Rot = FVector4(CommonParameters.DefaultRotation);
		FMatrix LocalToPrimitive = FTransform(FQuat(Rot.X, Rot.Y, Rot.Z, Rot.W), FVector(CommonParameters.DefaultPosition), FVector(CommonParameters.DefaultScale)).ToMatrixWithScale();

		if (!bLocalSpace)
		{
			// The instance transforms are expected to be in primitive local space, so we have to transform them from world space
			LocalToPrimitive.SetOrigin(LocalToPrimitive.GetOrigin() + OriginOffset);
			LocalToPrimitive *= SceneProxy.GetLocalToWorldInverse();				
		}

		EmitterSourceData.InstanceSceneData.LocalToPrimitive = LocalToPrimitive;

		GPUSceneRes.DynamicPrimitiveData.InstanceSceneData = TConstArrayView<FPrimitiveInstance>(&EmitterSourceData.InstanceSceneData, 1);
		GPUSceneRes.DynamicPrimitiveData.InstanceCustomData = TConstArrayView<float>(&EmitterSourceData.CustomData, 1);
		
		if (bNeedsPrevTransform)
		{
			const FVector4 PrevRot = FVector4(CommonParameters.DefaultPrevRotation);
			FMatrix PrevLocalToPrimitive = FTransform(FQuat(PrevRot.X, PrevRot.Y, PrevRot.Z, PrevRot.W), FVector(CommonParameters.DefaultPrevPosition), FVector(CommonParameters.DefaultPrevScale)).ToMatrixWithScale();

			if (!bLocalSpace)
			{
				// The instance transforms are expected to be in primitive local space, so we have to transform them from world space
				PrevLocalToPrimitive.SetOrigin(PrevLocalToPrimitive.GetOrigin() + OriginOffset);

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousPrimitiveToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				FPrimitiveSceneInfo* PrimitiveSceneInfo = SceneProxy.GetPrimitiveSceneInfo();
				SceneProxy.GetScene().GetPrimitiveUniformShaderParameters_RenderThread(PrimitiveSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousPrimitiveToWorld, SingleCaptureIndex, bOutputVelocity);

				PrevLocalToPrimitive *= PreviousPrimitiveToWorld.Inverse();
			}

			EmitterSourceData.InstanceDynamicData.PrevLocalToPrimitive = PrevLocalToPrimitive;
			
			GPUSceneRes.DynamicPrimitiveData.InstanceDynamicData = TConstArrayView<FPrimitiveInstanceDynamicData>(&EmitterSourceData.InstanceDynamicData, 1);
		}
	}

	OutMeshBatchElement.DynamicPrimitiveData = &GPUSceneRes.DynamicPrimitiveData;
}

void FNiagaraRendererMeshes::CreateMeshBatchForSection(
	const FParticleMeshRenderData& ParticleMeshRenderData,
	const FNiagaraMeshCommonParameters& CommonParameters,
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
) const
{
	MeshBatch.VertexFactory = &VertexFactory;
	MeshBatch.LCI = NULL;
	MeshBatch.ReverseCulling = SceneProxy.IsLocalToWorldDeterminantNegative();
	MeshBatch.CastShadow = SceneProxy.CastsDynamicShadow();
#if RHI_RAYTRACING
	MeshBatch.CastRayTracedShadow = SceneProxy.CastsDynamicShadow();
#endif
	MeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)SceneProxy.GetDepthPriorityGroup(&View);

	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	if (ParticleMeshRenderData.bUseGPUScene)
	{
		BatchElement.PrimitiveUniformBufferResource = SceneProxy.GetCustomUniformBufferResource(IsMotionBlurEnabled(), MeshData.LocalBounds);
	}
	else
	{
		BatchElement.PrimitiveUniformBuffer = SceneProxy.GetCustomUniformBuffer(IsMotionBlurEnabled(), MeshData.LocalBounds);
	}
	BatchElement.FirstIndex = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = 0;
	BatchElement.NumInstances = NumInstances;

	if (ParticleMeshRenderData.bUseGPUScene)
	{
		SetupElementForGPUScene(
			ParticleMeshRenderData,
			CommonParameters,
			SceneProxy,
			MeshData,
			View,
			NumInstances,
			bNeedsPrevTransform,
			BatchElement
		);		
	}

	if (bIsWireframe)
	{
		if (LODModel.AdditionalIndexBuffers && LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.IsInitialized())
		{
			MeshBatch.Type = PT_LineList;
			MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			BatchElement.FirstIndex = 0;
			BatchElement.IndexBuffer = &LODModel.AdditionalIndexBuffers->WireframeIndexBuffer;
			BatchElement.NumPrimitives = LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.GetNumIndices() / 2;
		}
		else
		{
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.MaterialRenderProxy = &MaterialProxy;
			MeshBatch.bWireframe = true;
			BatchElement.FirstIndex = 0;
			BatchElement.IndexBuffer = &LODModel.IndexBuffer;
			BatchElement.NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;
		}
	}
	else
	{
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.MaterialRenderProxy = &MaterialProxy;
		BatchElement.IndexBuffer = &LODModel.IndexBuffer;
		BatchElement.FirstIndex = Section.FirstIndex;
		BatchElement.NumPrimitives = Section.NumTriangles;
	}

	if ((SourceMode == ENiagaraRendererSourceDataMode::Particles) && (GPUCountBufferOffset != INDEX_NONE))
	{
		// We need to use indirect draw args, because the number of actual instances is coming from the GPU
		auto ComputeDispatchInterface = SceneProxy.GetComputeDispatchInterface();
		check(ComputeDispatchInterface);

		auto& CountManager = ComputeDispatchInterface->GetGPUInstanceCounterManager();
		auto IndirectDraw = CountManager.AddDrawIndirect(
			GPUCountBufferOffset,
			Section.NumTriangles * 3,
			Section.FirstIndex,
			bIsInstancedStereo,
			ParticleMeshRenderData.bNeedsCull && ParticleMeshRenderData.bSortCullOnGpu,
			ParticleMeshRenderData.SourceParticleData->GetGPUDataReadyStage()
		);

		BatchElement.NumPrimitives = 0;
		BatchElement.IndirectArgsBuffer = IndirectDraw.Buffer;
		BatchElement.IndirectArgsOffset = IndirectDraw.Offset;
	}
	else
	{
		check(BatchElement.NumPrimitives > 0);
	}

	MeshBatch.bCanApplyViewModeOverrides = true;
	MeshBatch.bUseWireframeSelectionColoring = SceneProxy.IsSelected();
}

void FNiagaraRendererMeshes::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy* SceneProxy) const
{
	check(SceneProxy);
	PARTICLE_PERF_STAT_CYCLES_RT(SceneProxy->GetProxyDynamicData().PerfStatsContext, GetDynamicMeshElements);

	// Prepare our particle render data
	// This will also determine if we have anything to render
	FParticleMeshRenderData ParticleMeshRenderData;
	PrepareParticleMeshRenderData(ParticleMeshRenderData, ViewFamily, Collector, DynamicDataRender, SceneProxy, false);

	if (ParticleMeshRenderData.SourceParticleData == nullptr )
	{
		return;
	}

#if STATS
	FScopeCycleCounter EmitterStatsCounter(EmitterStatID);
#endif

	PrepareParticleRenderBuffers(ParticleMeshRenderData, Collector.GetDynamicReadBuffer());

	// If mesh index comes from the parameter store grab the information now
	int32 EmitterModeMeshIndex = INDEX_NONE;
	if (EmitterMeshIndexOffset != INDEX_NONE && ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.IsValidIndex(EmitterMeshIndexOffset))
	{
		FMemory::Memcpy(&EmitterModeMeshIndex, ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.GetData() + EmitterMeshIndexOffset, sizeof(int32));
	}

	// Generate mesh batches per view
	const int32 NumViews = Views.Num();
	for (int32 ViewIndex = 0; ViewIndex < NumViews; ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			const bool bIsInstancedStereo = View->bIsInstancedStereoEnabled && IStereoRendering::IsStereoEyeView(*View);
			const bool bIsShadowView = View->GetDynamicMeshElementsShadowCullFrustum() != nullptr;

			if (bIsInstancedStereo && !IStereoRendering::IsAPrimaryView(*View))
			{
				// One eye renders everything, so we can skip non-primaries
				continue;
			}				

			if (SourceMode == ENiagaraRendererSourceDataMode::Emitter && bEnableCulling)
			{
				FVector ViewOrigin = View->ViewMatrices.GetViewOrigin();
				FVector RefPosition = SceneProxy->GetLocalToWorld().GetOrigin();

				double DistSquared = SceneProxy->GetProxyDynamicData().LODDistanceOverride >= 0.0f ? FMath::Square(SceneProxy->GetProxyDynamicData().LODDistanceOverride) : FVector::DistSquared(RefPosition, ViewOrigin);
				if (DistSquared < DistanceCullRange.X * DistanceCullRange.X || DistSquared > DistanceCullRange.Y * DistanceCullRange.Y)
				{
					// Distance cull the whole emitter
					continue;
				}
			}

			// Initialize sort parameters that are mesh/section invariant
			FNiagaraGPUSortInfo SortInfo;
			if (ParticleMeshRenderData.bNeedsSort || ParticleMeshRenderData.bNeedsCull)
			{
				InitializeSortInfo(ParticleMeshRenderData, *SceneProxy, *View, ViewIndex, bIsInstancedStereo, SortInfo);
			}

			FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = SceneProxy->GetComputeDispatchInterface();
			for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
			{
				// No binding for mesh index or we don't allow culling we will only render the first mesh for all particles
				if (MeshIndex > 0 && (ParticleMeshRenderData.MeshIndexOffset == INDEX_NONE || (!ParticleMeshRenderData.bNeedsCull && !ParticleMeshRenderData.bUseGPUScene)))
				{
					break;
				}

				// Emitter mode mesh index binding
				if (EmitterModeMeshIndex != INDEX_NONE && MeshIndex != EmitterModeMeshIndex)
				{
					continue;
				}

				const FMeshData& MeshData = Meshes[MeshIndex];

				// @TODO : support multiple LOD
				const int32 LODIndex = GetLODIndex(MeshIndex);
				if (LODIndex == INDEX_NONE)
				{
					continue;
				}

				const FStaticMeshLODResources& LODModel = MeshData.RenderData->LODResources[LODIndex];
				const int32 SectionCount = LODModel.Sections.Num();

				FMeshCollectorResources* CollectorResources = &Collector.AllocateOneFrameResource<FMeshCollectorResources>();

				// Get the next vertex factory to use
				// TODO: Find a way to safely pool these such that they won't be concurrently accessed by multiple views
				FNiagaraMeshVertexFactory& VertexFactory = CollectorResources->VertexFactory;
				VertexFactory.SetParticleFactoryType(NVFT_Mesh);
				VertexFactory.SetMeshIndex(MeshIndex);
				VertexFactory.SetLODIndex(LODIndex);
				VertexFactory.EnablePrimitiveIDElement(ParticleMeshRenderData.bUseGPUScene);
				VertexFactory.InitResource();
				SetupVertexFactory(VertexFactory, LODModel);

				PreparePerMeshData(ParticleMeshRenderData, VertexFactory, *SceneProxy, MeshData);

				// Sort/Cull particles if needed.
				const uint32 NumInstances = PerformSortAndCull(ParticleMeshRenderData, Collector.GetDynamicReadBuffer(), SortInfo, ComputeDispatchInterface, MeshData.SourceMeshIndex);
				if ( NumInstances > 0 )
				{
					// Increment stats
					INC_DWORD_STAT_BY(STAT_NiagaraNumMeshVerts, NumInstances * LODModel.GetNumVertices());
					INC_DWORD_STAT_BY(STAT_NiagaraNumMeshes, NumInstances);

					FNiagaraMeshCommonParameters CommonParameters = CreateCommonShaderParams(ParticleMeshRenderData, *View, MeshData, *SceneProxy);
					FNiagaraMeshUniformBufferRef VFUniformBuffer = CreateVFUniformBuffer(ParticleMeshRenderData, CommonParameters);
					VertexFactory.SetUniformBuffer(VFUniformBuffer);
					CollectorResources->UniformBuffer = VFUniformBuffer;

					const bool bIsWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
					for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
					{
						const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
						const uint32 RemappedMaterialIndex = MeshData.MaterialRemapTable[Section.MaterialIndex];
						FMaterialRenderProxy* MaterialProxy = ParticleMeshRenderData.DynamicDataMesh->Materials.IsValidIndex(RemappedMaterialIndex) ? ParticleMeshRenderData.DynamicDataMesh->Materials[RemappedMaterialIndex] : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
						if (Section.NumTriangles == 0 || MaterialProxy == nullptr)
						{
							//@todo. This should never occur, but it does occasionally.
							continue;
						}

						const FMaterial& Material = MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
						const EBlendMode BlendMode = Material.GetBlendMode();
						const bool bTranslucent = IsTranslucentBlendMode(BlendMode);
						const bool bNeedsPrevTransform = !bTranslucent || Material.IsTranslucencyWritingVelocity();
						if (bIsShadowView && bTranslucent && !SceneProxy->CastsVolumetricTranslucentShadow())
						{
							// Don't add translucent mesh batches for shadows
							// TODO: Need a way to know if it's a volumetric translucent shadow view to make this logic better
							continue;
						}

						// When using GPU scene, the indirect draw is managed, so prevent creating indirect draw args
						uint32 GPUCountBufferOffset = INDEX_NONE;
						if (!ParticleMeshRenderData.bUseGPUScene)
						{
							GPUCountBufferOffset = SortInfo.CulledGPUParticleCountOffset != INDEX_NONE ? SortInfo.CulledGPUParticleCountOffset : ParticleMeshRenderData.SourceParticleData->GetGPUInstanceCountBufferOffset();
						}

						FMeshBatch& MeshBatch = Collector.AllocateMesh();
						CreateMeshBatchForSection(
							ParticleMeshRenderData,
							CommonParameters,
							MeshBatch,
							VertexFactory,
							*MaterialProxy,
							*SceneProxy,
							MeshData,
							LODModel,
							Section,
							*View,
							ViewIndex,
							NumInstances,
							GPUCountBufferOffset,
							bIsWireframe,
							bIsInstancedStereo,
							bNeedsPrevTransform
						);

						Collector.AddMesh(ViewIndex, MeshBatch);
					}
				}
			}
		}
	}
}

#if RHI_RAYTRACING

void FNiagaraRendererMeshes::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances, const FNiagaraSceneProxy* SceneProxy)
{
	if (!CVarRayTracingNiagaraMeshes.GetValueOnRenderThread())
	{
		return;
	}

	check(SceneProxy);

	const int32 ViewIndex = 0;
	const FSceneView* View = Context.ReferenceView;
	const bool bIsInstancedStereo = View->bIsInstancedStereoEnabled && IStereoRendering::IsStereoEyeView(*View);

	check(View->Family);

	// Prepare our particle render data
	// This will also determine if we have anything to render
	FParticleMeshRenderData ParticleMeshRenderData;
	PrepareParticleMeshRenderData(ParticleMeshRenderData, *View->Family, Context.RayTracingMeshResourceCollector, DynamicDataRender, SceneProxy, true);

	if (ParticleMeshRenderData.SourceParticleData == nullptr || Meshes.Num() == 0)
	{
		return;
	}

	// Disable sorting and culling as we manage this ourself
	ParticleMeshRenderData.bNeedsSort = false;
	ParticleMeshRenderData.bNeedsCull = false;
	ParticleMeshRenderData.bSortCullOnGpu = false;

	PrepareParticleRenderBuffers(ParticleMeshRenderData, Context.RayTracingMeshResourceCollector.GetDynamicReadBuffer());


	// Initialize sort parameters that are mesh/section invariant
	FNiagaraGPUSortInfo SortInfo;
	if (ParticleMeshRenderData.bNeedsSort || ParticleMeshRenderData.bNeedsCull)
	{
		InitializeSortInfo(ParticleMeshRenderData, *SceneProxy, *View, ViewIndex, bIsInstancedStereo, SortInfo);
	}

	OutRayTracingInstances.Reserve(Meshes.Num());

	for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
	{
		// No binding for mesh index we only render the first mesh not all of them
		if ( (MeshIndex > 0) && (ParticleMeshIndexOffset == INDEX_NONE) )
		{
			break;
		}

		const FMeshData& MeshData = Meshes[MeshIndex];
		const int32 LODIndex = GetLODIndex(MeshIndex);
		if (LODIndex == INDEX_NONE)
		{
			continue;
		}

		const FStaticMeshLODResources& LODModel = MeshData.RenderData->LODResources[LODIndex];
		const FStaticMeshVertexFactories& VFs = MeshData.RenderData->LODVertexFactories[LODIndex];
		FRayTracingGeometry& Geometry = MeshData.RenderData->LODResources[LODIndex].RayTracingGeometry;
		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &Geometry;

		FMeshCollectorResources* CollectorResources = &Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FMeshCollectorResources>();

		// Get the next vertex factory to use
		// TODO: Find a way to safely pool these such that they won't be concurrently accessed by multiple views
		FNiagaraMeshVertexFactory& VertexFactory = CollectorResources->VertexFactory;
		VertexFactory.SetParticleFactoryType(NVFT_Mesh);
		VertexFactory.SetMeshIndex(MeshIndex);
		VertexFactory.SetLODIndex(LODIndex);
		VertexFactory.EnablePrimitiveIDElement(ParticleMeshRenderData.bUseGPUScene);
		VertexFactory.InitResource();
		SetupVertexFactory(VertexFactory, LODModel);

		PreparePerMeshData(ParticleMeshRenderData, VertexFactory, *SceneProxy, MeshData);

		// Sort/Cull particles if needed.
		FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = SceneProxy->GetComputeDispatchInterface();
		const uint32 NumInstances = PerformSortAndCull(ParticleMeshRenderData, Context.RayTracingMeshResourceCollector.GetDynamicReadBuffer(), SortInfo, ComputeDispatchInterface, MeshData.SourceMeshIndex);
		if ( NumInstances == 0 )
		{
			continue;
		}

		FNiagaraMeshCommonParameters CommonParameters = CreateCommonShaderParams(ParticleMeshRenderData, *View, MeshData, *SceneProxy);
		FNiagaraMeshUniformBufferRef VFUniformBuffer = CreateVFUniformBuffer(ParticleMeshRenderData, CommonParameters);
		VertexFactory.SetUniformBuffer(VFUniformBuffer);
		CollectorResources->UniformBuffer = VFUniformBuffer;

		RayTracingInstance.Materials.Reserve(LODModel.Sections.Num());

		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
			if (Section.NumTriangles == 0)
			{
				continue;
			}

			const uint32 RemappedMaterialIndex = MeshData.MaterialRemapTable[Section.MaterialIndex];
			if (!ParticleMeshRenderData.DynamicDataMesh->Materials.IsValidIndex(RemappedMaterialIndex))
			{
				// This should never occur. Otherwise, the section data changed since initialization
				continue;
			}

			FMaterialRenderProxy* MaterialProxy = ParticleMeshRenderData.DynamicDataMesh->Materials[RemappedMaterialIndex];
			if (MaterialProxy == nullptr)
			{
				continue;
			}

			FMeshBatch MeshBatch;
			CreateMeshBatchForSection(
				ParticleMeshRenderData,
				CommonParameters,
				MeshBatch,
				VertexFactory,
				*MaterialProxy,
				*SceneProxy,
				MeshData,
				LODModel,
				Section,
				*View,
				ViewIndex,
				NumInstances,
				INDEX_NONE,
				false, // bIsWireframe
				bIsInstancedStereo,
				false // bNeedsPrevTransform
			);
			MeshBatch.SegmentIndex = SectionIndex;
			MeshBatch.LODIndex = LODIndex;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			MeshBatch.VisualizeLODIndex = LODIndex;
#endif

			MeshBatch.bCanApplyViewModeOverrides = false;

			MeshBatch.Elements[0].VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			MeshBatch.Elements[0].VisualizeElementIndex = SectionIndex;
#endif

			RayTracingInstance.Materials.Add(MoveTemp(MeshBatch));
		}

		if (RayTracingInstance.Materials.Num() == 0 || LODModel.Sections.Num() != RayTracingInstance.Materials.Num())
		{
			continue;
		}

		// Emitter source mode?
		const FMatrix LocalTransform(SceneProxy->GetLocalToWorld());
		const bool bUseLocalSpace = UseLocalSpace(SceneProxy);
		if (SourceMode == ENiagaraRendererSourceDataMode::Emitter)
		{
			FVector Pos = bUseLocalSpace ? FVector::ZeroVector : LocalTransform.GetOrigin();
			FVector3f Scale{ 1.0f, 1.0f, 1.0f };
			FQuat Rot = FQuat::Identity;

			if (bSetAnyBoundVars)
			{
				const uint8* ParameterBoundData = ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.GetData();
				if (VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Position] != INDEX_NONE
					&& ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.IsValidIndex(VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Position]))
				{
					FVector3f TempPos;
					FMemory::Memcpy(&TempPos, ParameterBoundData + VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Position], sizeof(FVector3f));
					Pos = (FVector)TempPos;
					if (!bUseLocalSpace)
					{
						// Handle LWC
						Pos += FVector(SceneProxy->GetLWCRenderTile()) * FLargeWorldRenderScalar::GetTileSize();
					}
				}
				if (VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Scale] != INDEX_NONE
					&& ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.IsValidIndex(VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Scale]))
				{
					FMemory::Memcpy(&Scale, ParameterBoundData + VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Scale], sizeof(FVector3f));
				}
				if (VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Rotation] != INDEX_NONE
					&& ParticleMeshRenderData.DynamicDataMesh->ParameterDataBound.IsValidIndex(VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Rotation]))
				{
					FMemory::Memcpy(&Rot, ParameterBoundData + VFBoundOffsetsInParamStore[ENiagaraMeshVFLayout::Type::Rotation], sizeof(FVector4f));
				}
			}

			FVector4 Transform1 = FVector4(1.0f, 0.0f, 0.0f, Pos.X);
			FVector4 Transform2 = FVector4(0.0f, 1.0f, 0.0f, Pos.Y);
			FVector4 Transform3 = FVector4(0.0f, 0.0f, 1.0f, Pos.Z);

			FTransform RotationTransform(Rot.GetNormalized());
			FMatrix RotationMatrix = RotationTransform.ToMatrixWithScale();

			Transform1.X = RotationMatrix.M[0][0];
			Transform1.Y = RotationMatrix.M[0][1];
			Transform1.Z = RotationMatrix.M[0][2];

			Transform2.X = RotationMatrix.M[1][0];
			Transform2.Y = RotationMatrix.M[1][1];
			Transform2.Z = RotationMatrix.M[1][2];

			Transform3.X = RotationMatrix.M[2][0];
			Transform3.Y = RotationMatrix.M[2][1];
			Transform3.Z = RotationMatrix.M[2][2];

			FMatrix ScaleMatrix(FMatrix::Identity);
			ScaleMatrix.M[0][0] *= Scale.X;
			ScaleMatrix.M[1][1] *= Scale.Y;
			ScaleMatrix.M[2][2] *= Scale.Z;

			FMatrix InstanceTransform = FMatrix(FPlane(Transform1), FPlane(Transform2), FPlane(Transform3), FPlane(0.0, 0.0, 0.0, 1.0));
			InstanceTransform = InstanceTransform * ScaleMatrix;
			InstanceTransform = InstanceTransform.GetTransposed();

			if (bUseLocalSpace)
			{
				InstanceTransform = InstanceTransform * LocalTransform;
			}

			RayTracingInstance.InstanceTransforms.Add(InstanceTransform);
		}
		else
		{
			TConstArrayView<FNiagaraRendererVariableInfo> VFVariables = ParticleMeshRenderData.RendererLayout->GetVFVariables_RenderThread();
			if (SimTarget == ENiagaraSimTarget::CPUSim)
			{
				const int32 TotalFloatSize = ParticleMeshRenderData.RendererLayout->GetTotalFloatComponents_RenderThread() * ParticleMeshRenderData.SourceParticleData->GetNumInstances();
				const int32 ComponentStrideDest = ParticleMeshRenderData.SourceParticleData->GetNumInstances() * sizeof(float);

				//ENiagaraMeshVFLayout::Transform just contains a Quat, not the whole transform
				const FNiagaraRendererVariableInfo& VarPositionInfo = VFVariables[ENiagaraMeshVFLayout::Position];
				const FNiagaraRendererVariableInfo& VarScaleInfo = VFVariables[ENiagaraMeshVFLayout::Scale];
				const FNiagaraRendererVariableInfo& VarTransformInfo = VFVariables[ENiagaraMeshVFLayout::Rotation];

				const int32 PositionBaseCompOffset = VarPositionInfo.DatasetOffset;
				const int32 ScaleBaseCompOffset = VarScaleInfo.DatasetOffset;
				const int32 TransformBaseCompOffset = VarTransformInfo.DatasetOffset;

				const float* RESTRICT PositionX = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(PositionBaseCompOffset));
				const float* RESTRICT PositionY = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(PositionBaseCompOffset + 1));
				const float* RESTRICT PositionZ = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(PositionBaseCompOffset + 2));

				const float* RESTRICT ScaleX = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(ScaleBaseCompOffset));
				const float* RESTRICT ScaleY = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(ScaleBaseCompOffset + 1));
				const float* RESTRICT ScaleZ = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(ScaleBaseCompOffset + 2));

				const float* RESTRICT QuatArrayX = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(TransformBaseCompOffset));
				const float* RESTRICT QuatArrayY = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(TransformBaseCompOffset + 1));
				const float* RESTRICT QuatArrayZ = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(TransformBaseCompOffset + 2));
				const float* RESTRICT QuatArrayW = reinterpret_cast<const float*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrFloat(TransformBaseCompOffset + 3));

				const int32* RESTRICT RenderVisibilityData = ParticleRendererVisTagOffset == INDEX_NONE ? nullptr : reinterpret_cast<const int32*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrInt32(ParticleRendererVisTagOffset));
				const int32* RESTRICT MeshIndexData = ParticleMeshIndexOffset == INDEX_NONE ? nullptr : reinterpret_cast<const int32*>(ParticleMeshRenderData.SourceParticleData->GetComponentPtrInt32(ParticleMeshIndexOffset));

				auto GetInstancePosition = [&PositionX, &PositionY, &PositionZ](int32 Idx)
				{
					return FVector(PositionX[Idx], PositionY[Idx], PositionZ[Idx]);
				};

				auto GetInstanceScale = [&ScaleX, &ScaleY, &ScaleZ](int32 Idx)
				{
					return FVector(ScaleX[Idx], ScaleY[Idx], ScaleZ[Idx]);
				};

				auto GetInstanceQuat = [&QuatArrayX, &QuatArrayY, &QuatArrayZ, &QuatArrayW](int32 Idx)
				{
					return FQuat(QuatArrayX[Idx], QuatArrayY[Idx], QuatArrayZ[Idx], QuatArrayW[Idx]);
				};

				//#dxr_todo: handle MESH_FACING_VELOCITY, MESH_FACING_CAMERA_POSITION, MESH_FACING_CAMERA_PLANE
				//#dxr_todo: handle half floats
				const bool bHasPosition = PositionBaseCompOffset > 0;
				const bool bHasRotation = TransformBaseCompOffset > 0;
				const bool bHasScale = ScaleBaseCompOffset > 0;

				const FMatrix NullInstanceTransform(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
				const FQuat MeshRotation = FQuat(MeshData.Rotation);
				const FVector MeshScale = FVector(MeshData.Scale);
				for (uint32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
				{
					if ( RenderVisibilityData && (RenderVisibilityData[InstanceIndex] != RendererVisibility) )
					{
						RayTracingInstance.InstanceTransforms.Add(NullInstanceTransform);
						continue;
					}
					if ( MeshIndexData && (MeshIndexData[InstanceIndex] != MeshData.SourceMeshIndex) )
					{
						RayTracingInstance.InstanceTransforms.Add(NullInstanceTransform);
						continue;
					}

					FVector InstancePosition = bHasPosition ? GetInstancePosition(InstanceIndex) : FVector::ZeroVector;
					if (!bLocalSpace)
					{
						// Handle LWC
						InstancePosition += FVector(SceneProxy->GetLWCRenderTile()) * FLargeWorldRenderScalar::GetTileSize();
					}

					const FQuat InstanceRotation = bHasRotation ? GetInstanceQuat(InstanceIndex).GetNormalized() * MeshRotation : MeshRotation;
					FMatrix InstanceTransform = FQuatRotationTranslationMatrix::Make(InstanceRotation, InstancePosition);

					const FVector InstanceScale = bHasScale ? GetInstanceScale(InstanceIndex) * MeshScale : MeshScale;
					InstanceTransform = FScaleMatrix(InstanceScale) * InstanceTransform;

					if (bLocalSpace)
					{
						InstanceTransform = InstanceTransform * LocalTransform;
					}

					RayTracingInstance.InstanceTransforms.Add(InstanceTransform);
				}
			}
			// Gpu Target
			else if (FNiagaraUtilities::AllowComputeShaders(GShaderPlatformForFeatureLevel[FeatureLevel]) && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingIndirectInstanceData(GShaderPlatformForFeatureLevel[FeatureLevel]) )
			{
				FRHICommandListImmediate& RHICmdList = Context.GraphBuilder.RHICmdList;

				RayTracingInstance.NumTransforms = NumInstances;

				FRDGBufferRef InstanceGPUTransformsBufferRef = Context.GraphBuilder.CreateBuffer(
					FRDGBufferDesc::CreateStructuredDesc(4 * sizeof(float), 3 * NumInstances),
					TEXT("InstanceGPUTransformsBuffer"));

				const TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer = Context.GraphBuilder.ConvertToExternalBuffer(InstanceGPUTransformsBufferRef);
				RayTracingInstance.InstanceGPUTransformsSRV = ExternalBuffer->GetOrCreateSRV(FRHIBufferSRVCreateInfo());

				const FLargeWorldRenderPosition AbsoluteViewOrigin(View->ViewMatrices.GetViewOrigin());
				const FVector ViewTileOffset = AbsoluteViewOrigin.GetTileOffset();
				const FVector RelativePreViewTranslation = View->ViewMatrices.GetPreViewTranslation() + ViewTileOffset;
				const FVector3f ViewTilePosition = AbsoluteViewOrigin.GetTile();

				const FLargeWorldRenderPosition LocalTransformOrigin(LocalTransform.GetOrigin());

				FNiagaraGPURayTracingTransformsCS::FParameters* PassParameters = Context.GraphBuilder.AllocParameters< FNiagaraGPURayTracingTransformsCS::FParameters>();
				{
					PassParameters->ParticleDataFloatStride		= ParticleMeshRenderData.ParticleFloatDataStride;
					//PassParameters.ParticleDataHalfStride		= ParticleMeshRenderData.ParticleHalfDataStride;
					PassParameters->ParticleDataIntStride		= ParticleMeshRenderData.ParticleIntDataStride;
					PassParameters->CPUNumInstances				= NumInstances;
					PassParameters->InstanceCountOffset			= ParticleMeshRenderData.SourceParticleData->GetGPUInstanceCountBufferOffset();
					PassParameters->SystemLWCTile				= SceneProxy->GetLWCRenderTile();
					PassParameters->PositionDataOffset			= VFVariables[ENiagaraMeshVFLayout::Position].GetGPUOffset();
					PassParameters->RotationDataOffset			= VFVariables[ENiagaraMeshVFLayout::Rotation].GetGPUOffset();
					PassParameters->ScaleDataOffset				= VFVariables[ENiagaraMeshVFLayout::Scale].GetGPUOffset();
					PassParameters->bLocalSpace					= bUseLocalSpace ? 1 : 0;
					PassParameters->RenderVisibilityOffset		= ParticleRendererVisTagOffset;
					PassParameters->MeshIndexOffset				= ParticleMeshIndexOffset;
					PassParameters->RenderVisibilityValue		= RendererVisibility;
					PassParameters->MeshIndexValue				= MeshData.SourceMeshIndex;
					PassParameters->LocalTransform				= FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(LocalTransformOrigin.GetTileOffset(), LocalTransform);
					PassParameters->LocalTransformTile			= LocalTransformOrigin.GetTile();
					PassParameters->DefaultPosition				= CommonParameters.DefaultPosition;
					PassParameters->DefaultRotation				= FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
					PassParameters->DefaultScale				= FVector3f(1.0f, 1.0f, 1.0f);
					PassParameters->MeshScale					= MeshData.Scale;
					PassParameters->MeshRotation				= FVector4f(MeshData.Rotation.X, MeshData.Rotation.Y, MeshData.Rotation.Z, MeshData.Rotation.W);
					PassParameters->ParticleDataFloatBuffer		= ParticleMeshRenderData.ParticleFloatSRV;
					//PassParameters.ParticleDataHalfBuffer		= ParticleMeshRenderData.ParticleHalfSRV;
					PassParameters->ParticleDataIntBuffer		= ParticleMeshRenderData.ParticleIntSRV;
					PassParameters->GPUInstanceCountBuffer		= ComputeDispatchInterface->GetGPUInstanceCounterManager().GetInstanceCountBuffer().SRV;
					PassParameters->TLASTransforms				= ExternalBuffer->GetOrCreateUAV(FRHIBufferUAVCreateInfo());

					PassParameters->ViewTilePosition			= ViewTilePosition;
					PassParameters->RelativePreViewTranslation	= FVector3f(RelativePreViewTranslation);
				}

				Context.GraphBuilder.AddPass(
					RDG_EVENT_NAME("NiagaraGPURayTracingTransforms"),
					PassParameters,
					ERDGPassFlags::Compute,
					[PassParameters, Pass_FeatureLevel = FeatureLevel, NumInstances](FRHICommandList& RHICmdList)
				{
					RHICmdList.Transition(FRHITransitionInfo(PassParameters->TLASTransforms, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

					FNiagaraGPURayTracingTransformsCS::FPermutationDomain PermutationVector;
					TShaderMapRef<FNiagaraGPURayTracingTransformsCS> ComputeShader(GetGlobalShaderMap(Pass_FeatureLevel), PermutationVector);
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FComputeShaderUtils::GetGroupCount(int32(NumInstances), int32(FNiagaraGPURayTracingTransformsCS::ThreadGroupSize)));

					RHICmdList.Transition(FRHITransitionInfo(PassParameters->TLASTransforms, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
				});
			}
		}

		RayTracingInstance.BuildInstanceMaskAndFlags(FeatureLevel);
		OutRayTracingInstances.Add(MoveTemp(RayTracingInstance));
	}
}
#endif


FNiagaraDynamicDataBase* FNiagaraRendererMeshes::GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenMeshVertexData);

	const UNiagaraMeshRendererProperties* Properties = CastChecked<const UNiagaraMeshRendererProperties>(InProperties);
	
	if (!IsRendererEnabled(Properties, Emitter))
	{
		return nullptr;
	}

	if (Properties->bAllowInCullProxies == false)
	{
		check(Emitter);

		FNiagaraSystemInstance* Inst = Emitter->GetParentSystemInstance();
		check(Emitter->GetParentSystemInstance());

		//TODO: Probably should push some state into the system instance for this?
		const bool bIsCullProxy = Cast<UNiagaraCullProxyComponent>(Inst->GetAttachComponent()) != nullptr;
		if (bIsCullProxy)
		{
			return nullptr;
		}
	}

	
	FNiagaraDataBuffer* DataToRender = Emitter->GetData().GetCurrentData();
	if (!DataToRender || 
		Meshes.Num() == 0 ||
		(SourceMode == ENiagaraRendererSourceDataMode::Particles && SimTarget != ENiagaraSimTarget::GPUComputeSim && DataToRender->GetNumInstances() == 0))
	{
		return nullptr;
	}

	// Bail if we have cached mesh render data for any meshes that are no longer valid
	for (const auto& MeshData : Meshes)
	{
		if ( !Properties->Meshes.IsValidIndex(MeshData.SourceMeshIndex) )
		{
			return nullptr;
		}
	}

	FNiagaraDynamicDataMesh* DynamicData = new FNiagaraDynamicDataMesh(Emitter);
	DynamicData->SetMaterialRelevance(BaseMaterialRelevance_GT);

	DynamicData->Materials.Reset(BaseMaterials_GT.Num());
	for (UMaterialInterface* Mat : BaseMaterials_GT)
	{
		//In preparation for a material override feature, we pass our material(s) and relevance in via dynamic data.
		//The renderer ensures we have the correct usage and relevance for materials in BaseMaterials_GT.
		//Any override feature must also do the same for materials that are set.
		check(Mat->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraMeshParticles));
		DynamicData->Materials.Add(Mat->GetRenderProxy());
	}

	if (DynamicData)
	{
		const FNiagaraParameterStore& ParameterData = Emitter->GetRendererBoundVariables();
		DynamicData->DataInterfacesBound = ParameterData.GetDataInterfaces();
		DynamicData->ObjectsBound = ParameterData.GetUObjects();
		DynamicData->ParameterDataBound = ParameterData.GetParameterDataArray();
	}

	if (DynamicData && Properties->MaterialParameters.HasAnyBindings())
	{
		ProcessMaterialParameterBindings(Properties->MaterialParameters, Emitter, MakeArrayView(BaseMaterials_GT));
	}

	return DynamicData;
}

int FNiagaraRendererMeshes::GetDynamicDataSize()const
{
	uint32 Size = sizeof(FNiagaraDynamicDataMesh);
	return Size;
}

bool FNiagaraRendererMeshes::IsMaterialValid(const UMaterialInterface* Mat)const
{
	return Mat && Mat->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraMeshParticles);
}


//////////////////////////////////////////////////////////////////////////
// Proposed class for ensuring Niagara/Cascade components who's proxies reference render data of other objects (Materials, Meshes etc) do not have data freed from under them.
// Our components register themselves with the referenced component which then calls InvalidateRenderDependencies() whenever it's render data is changed or when it is destroyed.
// UNTESTED - DO NOT USE.
struct FComponentRenderDependencyHandler
{
	void AddDependency(UPrimitiveComponent* Component)
	{
		DependentComponents.Add(Component);
	}

	void RemoveDependancy(UPrimitiveComponent* Component)
	{
		DependentComponents.RemoveSwap(Component);
	}

	void InvalidateRenderDependencies()
	{
		int32 i = DependentComponents.Num();
		while (--i >= 0)
		{
			if (UPrimitiveComponent* Comp = DependentComponents[i].Get())
			{
				Comp->MarkRenderStateDirty();
			}
			else
			{
				DependentComponents.RemoveAtSwap(i);
			}
		}
	}

	TArray<TWeakObjectPtr<UPrimitiveComponent>> DependentComponents;
};

