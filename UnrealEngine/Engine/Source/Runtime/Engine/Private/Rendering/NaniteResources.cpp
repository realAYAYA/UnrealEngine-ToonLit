// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "PrimitiveSceneInfo.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/InstancedStaticMesh.h"
#include "Materials/Material.h"
#include "RenderingThread.h"
#include "UnifiedBuffer.h"
#include "CommonRenderResources.h"
#include "StaticMeshResources.h"
#include "DistanceFieldAtlas.h"
#include "RenderGraphUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Misc/Compression.h"
#include "HAL/LowLevelMemStats.h"
#include "Interfaces/ITargetPlatform.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "Elements/SMInstance/SMInstanceManager.h"
#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled
#include "MaterialCachedData.h"
#include "EngineStats.h"

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataValueId.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataCacheRecord.h"
#include "Rendering/StaticLightingSystemInterface.h"
#endif

#if RHI_RAYTRACING
#include "RayTracingInstance.h"
#endif

DEFINE_GPU_STAT(NaniteStreaming);
DEFINE_GPU_STAT(NaniteReadback);

DECLARE_LLM_MEMORY_STAT(TEXT("Nanite"), STAT_NaniteLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Nanite"), STAT_NaniteSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(Nanite, NAME_None, NAME_None, GET_STATFNAME(STAT_NaniteLLM), GET_STATFNAME(STAT_NaniteSummaryLLM));

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Instances"), STAT_NaniteInstanceCount, STATGROUP_Nanite);
DECLARE_MEMORY_STAT(TEXT("Nanite Proxy Instance Memory"), STAT_ProxyInstanceMemory, STATGROUP_Nanite);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Dynamic Data Instances"), STAT_InstanceHasDynamicCount, STATGROUP_Nanite);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("LMSM Data Instances"), STAT_InstanceHasLMSMBiasCount, STATGROUP_Nanite);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Custom Data Instances"), STAT_InstanceHasCustomDataCount, STATGROUP_Nanite);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Random Data Instances"), STAT_InstanceHasRandomCount, STATGROUP_Nanite);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Local Bounds Instances"), STAT_InstanceHasLocalBounds, STATGROUP_Nanite);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Hierarchy Offset Instances"), STAT_InstanceHasHierarchyOffset, STATGROUP_Nanite);

// TODO: Work in progress / experimental - do not use
static TAutoConsoleVariable<int32> CVarNaniteAllowComputeMaterials(
	TEXT("r.Nanite.AllowComputeMaterials"),
	0, // Off by default
	TEXT("Whether to enable support for Nanite compute materials"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

int32 GNaniteOptimizedRelevance = 1;
FAutoConsoleVariableRef CVarNaniteOptimizedRelevance(
	TEXT("r.Nanite.OptimizedRelevance"),
	GNaniteOptimizedRelevance,
	TEXT("Whether to optimize Nanite relevance (outside of editor)."),
	ECVF_RenderThreadSafe
);

int32 GNaniteErrorOnVertexInterpolator = 0;
FAutoConsoleVariableRef CVarNaniteErrorOnVertexInterpolator(
	TEXT("r.Nanite.ErrorOnVertexInterpolator"),
	GNaniteErrorOnVertexInterpolator,
	TEXT("Whether to error and use default material if vertex interpolator is present on a Nanite material."),
	ECVF_RenderThreadSafe
);

int32 GNaniteErrorOnWorldPositionOffset = 0;
FAutoConsoleVariableRef CVarNaniteErrorOnWorldPositionOffset(
	TEXT("r.Nanite.ErrorOnWorldPositionOffset"),
	GNaniteErrorOnWorldPositionOffset,
	TEXT("Whether to error and use default material if world position offset is present on a Nanite material."),
	ECVF_RenderThreadSafe
);

int32 GNaniteErrorOnPixelDepthOffset = 0;
FAutoConsoleVariableRef CVarNaniteErrorOnPixelDepthOffset(
	TEXT("r.Nanite.ErrorOnPixelDepthOffset"),
	GNaniteErrorOnPixelDepthOffset,
	TEXT("Whether to error and use default material if pixel depth offset is present on a Nanite material."),
	ECVF_RenderThreadSafe
);

int32 GNaniteErrorOnMaskedBlendMode = 0;
FAutoConsoleVariableRef CVarNaniteErrorOnMaskedBlendMode(
	TEXT("r.Nanite.ErrorOnMaskedBlendMode"),
	GNaniteErrorOnMaskedBlendMode,
	TEXT("Whether to error and use default material if masked blend mode is specified for a Nanite material."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarRayTracingNaniteProxyMeshes(
	TEXT("r.RayTracing.Geometry.NaniteProxies"),
	1,
	TEXT("Include Nanite proxy meshes in ray tracing effects (default = 1 (Nanite proxy meshes enabled in ray tracing))"));

static int32 GNaniteRayTracingMode = 0;
static FAutoConsoleVariableRef CVarNaniteRayTracingMode(
	TEXT("r.RayTracing.Nanite.Mode"),
	GNaniteRayTracingMode,
	TEXT("0 - fallback mesh (default);\n")
	TEXT("1 - streamed out mesh;")
);

#define VF_NANITE_PROCEDURAL_INTERSECTOR 1

static int32 GNaniteRaytracingProceduralPrimitive = 0;
static FAutoConsoleVariableRef CVarNaniteRaytracingProceduralPrimitive(
	TEXT("r.RayTracing.Nanite.ProceduralPrimitive"),
	GNaniteRaytracingProceduralPrimitive,
	TEXT("Whether to raytrace nanite meshes using procedural primitives instead of a proxy."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

namespace Nanite
{
ERayTracingMode GetRayTracingMode()
{
	return (ERayTracingMode)GNaniteRayTracingMode;
}

bool GetSupportsRayTracingProceduralPrimitive(EShaderPlatform InShaderPlatform)
{
	return GNaniteRaytracingProceduralPrimitive && VF_NANITE_PROCEDURAL_INTERSECTOR && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(InShaderPlatform);
}

static_assert(sizeof(FPackedCluster) == NANITE_NUM_PACKED_CLUSTER_FLOAT4S * 16, "NANITE_NUM_PACKED_CLUSTER_FLOAT4S out of sync with sizeof(FPackedCluster)");

FArchive& operator<<(FArchive& Ar, FPackedHierarchyNode& Node)
{
	for (uint32 i = 0; i < NANITE_MAX_BVH_NODE_FANOUT; i++)
	{
		Ar << Node.LODBounds[ i ];
		Ar << Node.Misc0[ i ].BoxBoundsCenter;
		Ar << Node.Misc0[ i ].MinLODError_MaxParentLODError;
		Ar << Node.Misc1[ i ].BoxBoundsExtent;
		Ar << Node.Misc1[ i ].ChildStartReference;
		Ar << Node.Misc2[ i ].ResourcePageIndex_NumPages_GroupPartSize;
	}
	
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FPageStreamingState& PageStreamingState )
{
	Ar << PageStreamingState.BulkOffset;
	Ar << PageStreamingState.BulkSize;
	Ar << PageStreamingState.PageSize;
	Ar << PageStreamingState.DependenciesStart;
	Ar << PageStreamingState.DependenciesNum;
	Ar << PageStreamingState.Flags;
	return Ar;
}

void FResources::InitResources(const UObject* Owner)
{
	// TODO: Should remove bulk data from built data if platform cannot run Nanite in any capacity
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	if (PageStreamingStates.Num() == 0)
	{
		// Skip resources that have their render data stripped
		return;
	}
	
	// Root pages should be available here. If they aren't, this resource has probably already been initialized and added to the streamer. Investigate!
	check(RootData.Num() > 0);
	PersistentHash = FMath::Max(FCrc::StrCrc32<TCHAR>(*Owner->GetName()), 1u);
#if WITH_EDITOR
	ResourceName = Owner->GetPathName();
#endif
	
	ENQUEUE_RENDER_COMMAND(InitNaniteResources)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			GStreamingManager.Add(this);
		}
	);
}

bool FResources::ReleaseResources()
{
	// TODO: Should remove bulk data from built data if platform cannot run Nanite in any capacity
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return false;
	}

	if (PageStreamingStates.Num() == 0)
	{
		return false;
	}

	ENQUEUE_RENDER_COMMAND(ReleaseNaniteResources)(
		[this]( FRHICommandListImmediate& RHICmdList)
		{
			GStreamingManager.Remove(this);
		}
	);
	return true;
}

void FResources::Serialize(FArchive& Ar, UObject* Owner, bool bCooked)
{
	LLM_SCOPE_BYTAG(Nanite);

	// Note: this is all derived data, native versioning is not needed, but be sure to bump NANITE_DERIVEDDATA_VER when modifying!
	FStripDataFlags StripFlags( Ar, 0 );
	if( !StripFlags.IsDataStrippedForServer() )
	{
		uint32 StoredResourceFlags;
		if (Ar.IsSaving() && bCooked)
		{
			// Disable DDC store when saving out a cooked build
			StoredResourceFlags = ResourceFlags & ~NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC;
			Ar << StoredResourceFlags;
		}
		else
		{
			Ar << ResourceFlags;
			StoredResourceFlags = ResourceFlags;
		}
		
		if (StoredResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC)
		{
#if !WITH_EDITOR
			checkf(false, TEXT("DDC streaming should only happen in editor"));
#endif
		}
		else
		{
			StreamablePages.Serialize(Ar, Owner, 0);
		}

		Ar << RootData;
		Ar << PageStreamingStates;
		Ar << HierarchyNodes;
		Ar << HierarchyRootOffsets;
		Ar << PageDependencies;
		Ar << ImposterAtlas;
		Ar << NumRootPages;
		Ar << PositionPrecision;
		Ar << NumInputTriangles;
		Ar << NumInputVertices;
		Ar << NumInputMeshes;
		Ar << NumInputTexCoords;
		Ar << NumClusters;

#if !WITH_EDITOR
		check(!HasStreamingData() || StreamablePages.GetBulkDataSize() > 0);
#endif
	}
}

bool FResources::HasStreamingData() const
{
	return (uint32)PageStreamingStates.Num() > NumRootPages;
}

#if WITH_EDITOR
void FResources::DropBulkData()
{
	if (!HasStreamingData())
	{
		return;
	}

	if(ResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC)
	{
		StreamablePages.RemoveBulkData();
	}
}

void FResources::RebuildBulkDataFromDDC(const UObject* Owner)
{
	BeginRebuildBulkDataFromCache(Owner);
	EndRebuildBulkDataFromCache();
}

void FResources::BeginRebuildBulkDataFromCache(const UObject* Owner)
{
	check(DDCRebuildState.State.load() == EDDCRebuildState::Initial);
	if (!HasStreamingData() || (ResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC) == 0u)
	{
		return;
	}

	using namespace UE::DerivedData;

	FCacheKey Key;
	Key.Bucket = FCacheBucket(TEXT("StaticMesh"));
	Key.Hash = DDCKeyHash;
	check(!DDCKeyHash.IsZero());

	FCacheGetChunkRequest Request;
	Request.Name = Owner->GetPathName();
	Request.Id = FValueId::FromName("NaniteStreamingData");
	Request.Key = Key;
	Request.RawHash = DDCRawHash;
	check(!DDCRawHash.IsZero());

	FSharedBuffer SharedBuffer;
	*DDCRequestOwner = MakePimpl<FRequestOwner>(EPriority::Normal);
	DDCRebuildState.State.store(EDDCRebuildState::Pending);

	GetCache().GetChunks(MakeArrayView(&Request, 1), **DDCRequestOwner,
		[this](FCacheGetChunkResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				StreamablePages.Lock(LOCK_READ_WRITE);
				uint8* Ptr = (uint8*)StreamablePages.Realloc(Response.RawData.GetSize());
				FMemory::Memcpy(Ptr, Response.RawData.GetData(), Response.RawData.GetSize());
				StreamablePages.Unlock();
				StreamablePages.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
				DDCRebuildState.State.store(EDDCRebuildState::Succeeded);
			}
			else
			{
				DDCRebuildState.State.store(EDDCRebuildState::Failed);
			}
		});
}

void FResources::EndRebuildBulkDataFromCache()
{
	if (*DDCRequestOwner)
	{
		(*DDCRequestOwner)->Wait();
		(*DDCRequestOwner).Reset();
	}
	DDCRebuildState.State.store(EDDCRebuildState::Initial);
}

bool FResources::RebuildBulkDataFromCacheAsync(const UObject* Owner, bool& bFailed)
{
	bFailed = false;

	if (!HasStreamingData() || (ResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC) == 0u)
	{
		return true;
	}

	if (DDCRebuildState.State.load() == EDDCRebuildState::Initial)
	{
		if (StreamablePages.IsBulkDataLoaded())
		{
			return true;
		}

		// Handle Initial state first so we can transition directly to Succeeded/Failed if the data was immediately available from the cache.
		check(!(*DDCRequestOwner).IsValid());
		BeginRebuildBulkDataFromCache(Owner);
	}

	switch (DDCRebuildState.State.load())
	{
	case EDDCRebuildState::Pending:
		return false;
	case EDDCRebuildState::Succeeded:
		check(StreamablePages.GetBulkDataSize() > 0);
		EndRebuildBulkDataFromCache();
		return true;
	case EDDCRebuildState::Failed:
		bFailed = true;
		EndRebuildBulkDataFromCache();
		return true;
	default:
		check(false);
		return true;
	}
}
#endif

void FResources::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RootData.GetAllocatedSize());
	if (StreamablePages.IsBulkDataLoaded())
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(StreamablePages.GetBulkDataSize());
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ImposterAtlas.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HierarchyNodes.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HierarchyRootOffsets.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(PageStreamingStates.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(PageDependencies.GetAllocatedSize());
}

void FVertexFactory::InitRHI()
{
	LLM_SCOPE_BYTAG(Nanite);

	FVertexStream VertexStream;
	VertexStream.VertexBuffer = &GScreenRectangleVertexBuffer;
	VertexStream.Offset = 0;

	Streams.Add(VertexStream);

	SetDeclaration(GFilterVertexDeclaration.VertexDeclarationRHI);
}

bool FVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	bool bShouldCompile =
		(Parameters.MaterialParameters.bIsUsedWithNanite || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		IsSupportedMaterialDomain(Parameters.MaterialParameters.MaterialDomain) &&
		IsSupportedBlendMode(Parameters.MaterialParameters.BlendMode) &&
		(Parameters.ShaderType->GetFrequency() == SF_Pixel || Parameters.ShaderType->GetFrequency() == SF_RayHitGroup) &&
		DoesPlatformSupportNanite(Parameters.Platform);

	return bShouldCompile;
}

void FVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	::FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("IS_NANITE_SHADING_PASS"), 1);
	OutEnvironment.SetDefine(TEXT("IS_NANITE_PASS"), 1);
	OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 1);
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	OutEnvironment.SetDefine(TEXT("NANITE_USE_UNIFORM_BUFFER"), Parameters.ShaderType->GetFrequency() != SF_RayHitGroup);
	OutEnvironment.SetDefine(TEXT("NANITE_USE_RAYTRACING_UNIFORM_BUFFER"), Parameters.ShaderType->GetFrequency() == SF_RayHitGroup);
	OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 1);

	// Get data from GPUSceneParameters rather than View.
	// TODO: Profile this vs view uniform buffer path
	//OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
}

void FVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	GFilterVertexDeclaration.VertexDeclarationRHI->GetInitializer(Elements);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(Nanite::FVertexFactory, "/Engine/Private/Nanite/NaniteVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsNaniteRendering
	| EVertexFactoryFlags::SupportsPSOPrecaching
	| EVertexFactoryFlags::SupportsRayTracing
);

#if WITH_EDITOR
HHitProxy* FSceneProxyBase::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	// Subclasses will have populated OutHitProxies already - update the hit proxy ID before used by GPUScene
	HitProxyIds.SetNumUninitialized(OutHitProxies.Num());
	for (int32 HitProxyId = 0; HitProxyId < HitProxyIds.Num(); ++HitProxyId)
	{
		HitProxyIds[HitProxyId] = OutHitProxies[HitProxyId]->Id;
	}

	// We don't want a default hit proxy, or to output any hit proxies (avoid 2x registration).
	return nullptr;
}
#endif

void FSceneProxyBase::DrawStaticElementsInternal(FStaticPrimitiveDrawInterface* PDI, const FLightCacheInterface* LCI)
{
	LLM_SCOPE_BYTAG(Nanite);

	FMeshBatch MeshBatch;
	MeshBatch.VertexFactory = GVertexFactoryResource.GetVertexFactory();
	MeshBatch.Type = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;
	MeshBatch.ReverseCulling = false;
	MeshBatch.bDisableBackfaceCulling = true;
	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.LODIndex = INDEX_NONE;
	MeshBatch.bWireframe = false;
	MeshBatch.bCanApplyViewModeOverrides = false;
	MeshBatch.LCI = LCI;
	MeshBatch.Elements[0].IndexBuffer = &GScreenRectangleIndexBuffer;
	MeshBatch.Elements[0].NumInstances = 1;
	MeshBatch.Elements[0].PrimitiveIdMode = PrimID_ForceZero;
	MeshBatch.Elements[0].PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	if (GRHISupportsRectTopology)
	{
		MeshBatch.Elements[0].FirstIndex = 9;
		MeshBatch.Elements[0].NumPrimitives = 1;
		MeshBatch.Elements[0].MinVertexIndex = 1;
		MeshBatch.Elements[0].MaxVertexIndex = 3;
	}
	else
	{
		MeshBatch.Elements[0].FirstIndex = 0;
		MeshBatch.Elements[0].NumPrimitives = 2;
		MeshBatch.Elements[0].MinVertexIndex = 0;
		MeshBatch.Elements[0].MaxVertexIndex = 3;
	}

	for (int32 SectionIndex = 0; SectionIndex < MaterialSections.Num(); ++SectionIndex)
	{
		const FMaterialSection& Section = MaterialSections[SectionIndex];
		const FMaterialRenderProxy* MaterialProxy = Section.ShadingMaterialProxy;
		if (!MaterialProxy)
		{
			continue;
		}

		MeshBatch.SegmentIndex = SectionIndex;
		MeshBatch.MaterialRenderProxy = MaterialProxy;

	#if WITH_EDITOR
		HHitProxy* HitProxy = Section.HitProxy;
		PDI->SetHitProxy(HitProxy);
	#endif
		PDI->DrawMesh(MeshBatch, FLT_MAX);
	}
}

FSceneProxy::FSceneProxy(UStaticMeshComponent* Component)
: FSceneProxyBase(Component)
, MeshInfo(Component)
, RenderData(Component->GetStaticMesh()->GetRenderData())
, bReverseCulling(Component->bReverseCulling)
, StaticMesh(Component->GetStaticMesh())
#if WITH_EDITOR
, bHasSelectedInstances(false)
#endif
#if NANITE_ENABLE_DEBUG_RENDERING
, Owner(Component->GetOwner())
, LightMapResolution(Component->GetStaticLightMapResolution())
, BodySetup(Component->GetBodySetup())
, CollisionTraceFlag(ECollisionTraceFlag::CTF_UseSimpleAndComplex)
, CollisionResponse(Component->GetCollisionResponseToChannels())
, ForcedLodModel(Component->ForcedLodModel)
, LODForCollision(Component->GetStaticMesh()->LODForCollision)
, bDrawMeshCollisionIfComplex(Component->bDrawMeshCollisionIfComplex)
, bDrawMeshCollisionIfSimple(Component->bDrawMeshCollisionIfSimple)
#endif
{
	LLM_SCOPE_BYTAG(Nanite);

	// Nanite requires GPUScene
	checkSlow(UseGPUScene(GMaxRHIShaderPlatform, GetScene().GetFeatureLevel()));
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));
	
	Resources = Component->GetNaniteResources();

	// This should always be valid.
	check(Resources && Resources->PageStreamingStates.Num() > 0);

	FMaterialAudit MaterialAudit;
	AuditMaterials(Component, MaterialAudit);
	FixupMaterials(MaterialAudit);

	// Nanite supports the GPUScene instance data buffer.
	bSupportsInstanceDataBuffer = true;

	// Nanite supports mesh card representation.
	bSupportsMeshCardRepresentation = true;
	DistanceFieldSelfShadowBias = FMath::Max(Component->bOverrideDistanceFieldSelfShadowBias ? Component->DistanceFieldSelfShadowBias : Component->GetStaticMesh()->DistanceFieldSelfShadowBias, 0.0f);

	// Use fast path that does not update static draw lists.
	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	// Nanite always uses GPUScene, so we can skip expensive primitive uniform buffer updates.
	bVFRequiresPrimitiveUniformBuffer = false;

	// Indicates if 1 or more materials contain settings not supported by Nanite.
	bHasMaterialErrors = false;

	InstanceWPODisableDistance = Component->WorldPositionOffsetDisableDistance;

	SetLevelColor(FLinearColor::White);
	SetPropertyColor(FLinearColor::White);
	SetWireframeColor(Component->GetWireframeColor());

	const bool bHasSurfaceStaticLighting = MeshInfo.GetLightMap() != nullptr || MeshInfo.GetShadowMap() != nullptr;

	const uint32 FirstLODIndex = 0; // Only data from LOD0 is used.
	const FStaticMeshLODResources& MeshResources = RenderData->LODResources[FirstLODIndex];
	const FStaticMeshSectionArray& MeshSections = MeshResources.Sections;

	// Copy the pointer to the volume data, async building of the data may modify the one on FStaticMeshLODResources while we are rendering
	DistanceFieldData = MeshResources.DistanceFieldData;
	CardRepresentationData = MeshResources.CardRepresentationData;

	bEvaluateWorldPositionOffset = !IsOptimizedWPO() || Component->bEvaluateWorldPositionOffset;
	
	MaterialSections.SetNumZeroed(MeshSections.Num());

	const bool bIsInstancedMesh = Component->IsA<UInstancedStaticMeshComponent>();

	for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& MeshSection = MeshSections[SectionIndex];
		FMaterialSection& MaterialSection = MaterialSections[SectionIndex];
		MaterialSection.MaterialIndex = MeshSection.MaterialIndex;

		// Keep track of highest observed material index.
		MaterialMaxIndex = FMath::Max(MaterialSection.MaterialIndex, MaterialMaxIndex);

		UMaterialInterface* ShadingMaterial = MaterialAudit.GetMaterial(MaterialSection.MaterialIndex);

		// Copy over per-instance material flags for this section
		MaterialSection.bHasPerInstanceRandomID = MaterialAudit.HasPerInstanceRandomID(MaterialSection.MaterialIndex);
		MaterialSection.bHasPerInstanceCustomData = MaterialAudit.HasPerInstanceCustomData(MaterialSection.MaterialIndex);

		// Set the IsUsedWithInstancedStaticMeshes usage so per instance random and custom data get compiled
		// in by the HLSL translator in cases where only Nanite scene proxies have rendered with this material
		// which would result in this usage not being set by FInstancedStaticMeshSceneProxy::SetupProxy()
		if (bIsInstancedMesh && ShadingMaterial && !ShadingMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes))
		{
			ShadingMaterial = nullptr;
		}

		if (bHasSurfaceStaticLighting && ShadingMaterial && !ShadingMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_StaticLighting))
		{
			ShadingMaterial = nullptr;
		}

		if (ShadingMaterial == nullptr)
		{
			ShadingMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		MaterialSection.MaterialRelevance = ShadingMaterial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
		CombinedMaterialRelevance |= MaterialSection.MaterialRelevance;

		MaterialSection.ShadingMaterialProxy = ShadingMaterial->GetRenderProxy();

		if (bEvaluateWorldPositionOffset)
		{
			bHasProgrammableRaster |= MaterialSection.MaterialRelevance.bUsesWorldPositionOffset;
		}

		bHasProgrammableRaster |= MaterialSection.MaterialRelevance.bUsesPixelDepthOffset;
		bHasProgrammableRaster |= MaterialSection.MaterialRelevance.bMasked;

		// NOTE: MaterialRelevance.bTwoSided does not go into bHasProgrammableRaster because we want only want this flag to control culling, not a full shader graph bin
		MaterialSection.RasterMaterialProxy = bHasProgrammableRaster ? MaterialSection.ShadingMaterialProxy : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	}

	// Nanite supports distance field representation for fully opaque meshes.
	bSupportsDistanceFieldRepresentation = CombinedMaterialRelevance.bOpaque;

#if RHI_RAYTRACING
	int32 ValidLODIndex = GetFirstValidRaytracingGeometryLODIndex();
	if (ValidLODIndex != INDEX_NONE && RenderData->LODResources[ValidLODIndex].RayTracingGeometry.Initializer.GeometryType == RTGT_Procedural)
	{
		// Currently we only support 1 material when using procedural ray tracing primitive
		CachedRayTracingMaterials.SetNum(1);
	}
	else
	{
		CachedRayTracingMaterials.SetNum(MaterialSections.Num());
	}

	if (IsRayTracingEnabled())
	{
		CoarseMeshStreamingHandle = (Nanite::CoarseMeshStreamingHandle)Component->GetStaticMesh()->GetStreamingIndex();
		if (MeshResources.GetNumVertices())
		{
			bHasRayTracingInstances = true;
		}

		// This will be filled later (on the render thread) and cached.
		CachedRayTracingInstanceMaskAndFlags.Mask = 0;
	}
#endif

#if NANITE_ENABLE_DEBUG_RENDERING
	// Find the first LOD with any vertices (ie that haven't been stripped)
	int32 FirstAvailableLOD = 0;
	for (; FirstAvailableLOD < RenderData->LODResources.Num(); FirstAvailableLOD++)
	{
		if (RenderData->LODResources[FirstAvailableLOD].GetNumVertices() > 0)
		{
			break;
		}
	}

	const int32 SMCurrentMinLOD = Component->GetStaticMesh()->GetMinLODIdx();
	int32 EffectiveMinLOD = Component->bOverrideMinLOD ? Component->MinLOD : SMCurrentMinLOD; 
	
	ClampedMinLOD = FMath::Clamp(EffectiveMinLOD, FirstAvailableLOD, RenderData->LODResources.Num() - 1);

	// Pre-allocate FallbackLODs. Dynamic resize is unsafe as the FFallbackLODInfo constructor queues up a rendering command with a reference to itself.
	FallbackLODs.SetNumUninitialized(RenderData->LODResources.Num());

	for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++)
	{
		FFallbackLODInfo* NewLODInfo = new (&FallbackLODs[LODIndex]) FFallbackLODInfo(Component, RenderData->LODVertexFactories, LODIndex, ClampedMinLOD);
	}

	if (BodySetup)
	{
		CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
	}
#endif

	FPrimitiveInstance& Instance = InstanceSceneData.Emplace_GetRef();
	Instance.LocalToPrimitive.SetIdentity();

	FilterFlags = EFilterFlags::StaticMesh;
}

FSceneProxy::FSceneProxy(UInstancedStaticMeshComponent* Component)
: FSceneProxy(static_cast<UStaticMeshComponent*>(Component))
{
	LLM_SCOPE_BYTAG(Nanite);

	// Nanite meshes do not deform internally
	bHasDeformableMesh = false;

	PerInstanceRenderData = Component->PerInstanceRenderData;
	check(PerInstanceRenderData.IsValid());

#if WITH_EDITOR
	const bool bSupportInstancePicking = Component->bHasPerInstanceHitProxies && SMInstanceElementDataUtil::SMInstanceElementsEnabled();
	HitProxyMode = bSupportInstancePicking ? EHitProxyMode::PerInstance : EHitProxyMode::MaterialSection;

	if (HitProxyMode == EHitProxyMode::PerInstance)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < Component->SelectedInstances.Num() && !bHasSelectedInstances; ++InstanceIndex)
		{
			bHasSelectedInstances |= Component->SelectedInstances[InstanceIndex];
		}

		if (bHasSelectedInstances)
		{
			// If we have selected indices, mark scene proxy as selected.
			SetSelection_GameThread(true);
		}
	}
#endif

	InstanceSceneData.SetNum(Component->GetInstanceCount());

	bHasPerInstanceLocalBounds = false;
	bHasPerInstanceHierarchyOffset = false;

	UpdateMaterialDynamicDataUsage();

	bHasPerInstanceDynamicData = Component->PerInstancePrevTransform.Num() == Component->GetInstanceCount();
	InstanceDynamicData.SetNumUninitialized(bHasPerInstanceDynamicData ? Component->GetInstanceCount() : 0);

	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);
	bHasPerInstanceLMSMUVBias = bAllowStaticLighting;
	InstanceLightShadowUVBias.SetNumZeroed(bHasPerInstanceLMSMUVBias ? Component->GetInstanceCount() : 0);

	InstanceRandomID.SetNumZeroed(bHasPerInstanceRandom ? Component->GetInstanceCount() : 0); // Only allocate if material bound which uses this

#if WITH_EDITOR
	bHasPerInstanceEditorData = bSupportInstancePicking;// TODO: Would be good to decouple typed element picking from has editor data (i.e. always set this true, regardless)
	InstanceEditorData.SetNumZeroed(bHasPerInstanceEditorData ? Component->GetInstanceCount() : 0);
#endif

	// Only allocate if material bound which uses this
	if (bHasPerInstanceCustomData && Component->NumCustomDataFloats > 0)
	{
		InstanceCustomData = Component->PerInstanceSMCustomData;
		check(InstanceCustomData.Num() / Component->NumCustomDataFloats == Component->GetInstanceCount()); // Sanity check on the data packing
	}
	else
	{
		bHasPerInstanceCustomData = false;
	}

	FVector TranslatedSpaceOffset = -Component->GetTranslatedInstanceSpaceOrigin();

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); ++InstanceIndex)
	{
		FPrimitiveInstance& SceneData = InstanceSceneData[InstanceIndex];

		FTransform InstanceTransform;
		Component->GetInstanceTransform(InstanceIndex, InstanceTransform);
		InstanceTransform.AddToTranslation(TranslatedSpaceOffset);
		SceneData.LocalToPrimitive = InstanceTransform.ToMatrixWithScale();

		if (bHasPerInstanceDynamicData)
		{
			FTransform InstancePrevTransform;
			const bool bHasPrevTransform = Component->GetInstancePrevTransform(InstanceIndex, InstancePrevTransform);
			ensure(bHasPrevTransform); // Should always be true here
			InstancePrevTransform.AddToTranslation(TranslatedSpaceOffset);
			InstanceDynamicData[InstanceIndex].PrevLocalToPrimitive = InstancePrevTransform.ToMatrixWithScale();
		}
	}

	check(bHasPerInstanceRandom == false || InstanceRandomID.Num() == InstanceSceneData.Num());
#if WITH_EDITOR
	const bool bHasEditorData = bHasPerInstanceEditorData && InstanceEditorData.Num() == InstanceSceneData.Num();
#else
	const bool bHasEditorData = false;
#endif

	if (PerInstanceRenderData && (bHasPerInstanceRandom || bHasPerInstanceLMSMUVBias || bHasEditorData))
	{
		ENQUEUE_RENDER_COMMAND(SetNanitePerInstanceData)(
			[this](FRHICommandList& RHICmdList)
			{
				check(bHasPerInstanceRandom == false || InstanceRandomID.Num() == InstanceSceneData.Num());

				if (PerInstanceRenderData != nullptr &&
					PerInstanceRenderData->InstanceBuffer.GetNumInstances() == InstanceSceneData.Num())
				{
					for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); ++InstanceIndex)
					{
						if (bHasPerInstanceRandom)
						{
							PerInstanceRenderData->InstanceBuffer.GetInstanceRandomID(InstanceIndex, InstanceRandomID[InstanceIndex]);
						}

						if (bHasPerInstanceLMSMUVBias)
						{
							PerInstanceRenderData->InstanceBuffer.GetInstanceLightMapData(InstanceIndex, InstanceLightShadowUVBias[InstanceIndex]);
						}

					#if WITH_EDITOR
						if (bHasPerInstanceEditorData)
						{
							FColor HitProxyColor;
							bool bSelected;
							PerInstanceRenderData->InstanceBuffer.GetInstanceEditorData(InstanceIndex, HitProxyColor, bSelected);
							InstanceEditorData[InstanceIndex] = FInstanceUpdateCmdBuffer::PackEditorData(HitProxyColor, bSelected);
						}
					#endif
					}
				}

				check(bHasPerInstanceRandom == false || InstanceRandomID.Num() == InstanceSceneData.Num());
			}
		);
	}

	// TODO: Should report much finer granularity than what this code is doing (i.e. dynamic vs static, per stream sizes, etc..)
	// TODO: Also should be reporting this for all proxies, not just the Nanite ones
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceSceneData.GetAllocatedSize());
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceDynamicData.GetAllocatedSize());
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceCustomData.GetAllocatedSize());
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceRandomID.GetAllocatedSize());
#if WITH_EDITOR
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceEditorData.GetAllocatedSize());
#endif
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceLightShadowUVBias.GetAllocatedSize());
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceLocalBounds.GetAllocatedSize());
	INC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceHierarchyOffset.GetAllocatedSize());

	INC_DWORD_STAT_BY(STAT_NaniteInstanceCount, InstanceSceneData.Num());

	INC_DWORD_STAT_BY(STAT_InstanceHasDynamicCount, bHasPerInstanceDynamicData ? InstanceSceneData.Num() : 0);
	INC_DWORD_STAT_BY(STAT_InstanceHasLMSMBiasCount, bHasPerInstanceLMSMUVBias ? InstanceSceneData.Num() : 0);
	INC_DWORD_STAT_BY(STAT_InstanceHasCustomDataCount, bHasPerInstanceCustomData ? InstanceSceneData.Num() : 0);
	INC_DWORD_STAT_BY(STAT_InstanceHasRandomCount, bHasPerInstanceRandom ? InstanceSceneData.Num() : 0);
	INC_DWORD_STAT_BY(STAT_InstanceHasLocalBounds, bHasPerInstanceLocalBounds ? InstanceSceneData.Num() : 0);
	INC_DWORD_STAT_BY(STAT_InstanceHasHierarchyOffset, bHasPerInstanceHierarchyOffset ? InstanceSceneData.Num() : 0);

#if RHI_RAYTRACING
	if (InstanceSceneData.Num() == 0)
	{
		bHasRayTracingInstances = false;
	}
#endif

	EndCullDistance = Component->InstanceEndCullDistance;

	FilterFlags = EFilterFlags::InstancedStaticMesh;
}

FSceneProxy::FSceneProxy(UHierarchicalInstancedStaticMeshComponent* Component)
: FSceneProxy(static_cast<UInstancedStaticMeshComponent*>(Component))
{
	bIsHierarchicalInstancedStaticMesh = true;

	switch (Component->GetViewRelevanceType())
	{
	case EHISMViewRelevanceType::Grass:
		FilterFlags = EFilterFlags::Grass;
		bIsLandscapeGrass = true;
		break;
	case EHISMViewRelevanceType::Foliage:
		FilterFlags = EFilterFlags::Foliage;
		break;
	default:
		FilterFlags = EFilterFlags::InstancedStaticMesh;
		break;
	}
}

FSceneProxy::~FSceneProxy()
{
	// TODO: Should report much finer granularity than what this code is doing (i.e. dynamic vs static, per stream sizes, etc..)
	// TODO: Also should be reporting this for all proxies, not just the Nanite ones
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceSceneData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceDynamicData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceCustomData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceRandomID.GetAllocatedSize());
#if WITH_EDITOR
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceEditorData.GetAllocatedSize());
#endif
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceLightShadowUVBias.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceLocalBounds.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_ProxyInstanceMemory, InstanceHierarchyOffset.GetAllocatedSize());

	DEC_DWORD_STAT_BY(STAT_NaniteInstanceCount, InstanceSceneData.Num());

	DEC_DWORD_STAT_BY(STAT_InstanceHasDynamicCount, bHasPerInstanceDynamicData ? InstanceSceneData.Num() : 0);
	DEC_DWORD_STAT_BY(STAT_InstanceHasLMSMBiasCount, bHasPerInstanceLMSMUVBias ? InstanceSceneData.Num() : 0);
	DEC_DWORD_STAT_BY(STAT_InstanceHasCustomDataCount, bHasPerInstanceCustomData ? InstanceSceneData.Num() : 0);
	DEC_DWORD_STAT_BY(STAT_InstanceHasRandomCount, bHasPerInstanceRandom ? InstanceSceneData.Num() : 0);
	DEC_DWORD_STAT_BY(STAT_InstanceHasLocalBounds, bHasPerInstanceLocalBounds ? InstanceSceneData.Num() : 0);
	DEC_DWORD_STAT_BY(STAT_InstanceHasHierarchyOffset, bHasPerInstanceHierarchyOffset ? InstanceSceneData.Num() : 0);
}

void FSceneProxy::CreateRenderThreadResources()
{
	check(Resources->RuntimeResourceID != INDEX_NONE && Resources->HierarchyOffset != INDEX_NONE);
}

SIZE_T FSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FPrimitiveViewRelevance FSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	LLM_SCOPE_BYTAG(Nanite);

#if WITH_EDITOR
	const bool bOptimizedRelevance = false;
#else
	const bool bOptimizedRelevance = GNaniteOptimizedRelevance != 0;
#endif

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.NaniteMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

	// Always render the Nanite mesh data with static relevance.
	Result.bStaticRelevance = true;

	// Should always be covered by constructor of Nanite scene proxy.
	Result.bRenderInMainPass = true;

	if (bOptimizedRelevance) // No dynamic relevance if optimized.
	{
		CombinedMaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity();
	}
	else
	{
	#if WITH_EDITOR
		//only check these in the editor
		Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
		Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered());
	#endif

	#if NANITE_ENABLE_DEBUG_RENDERING
		bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
		const bool bInCollisionView = IsCollisionView(View->Family->EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
	#else
		bool bInCollisionView = false;
	#endif

		// Set dynamic relevance for overlays like collision and bounds.
		bool bSetDynamicRelevance = false;
	#if !(UE_BUILD_SHIPPING) || WITH_EDITOR
		bSetDynamicRelevance |= (IsRichView(*View->Family) ||
			View->Family->EngineShowFlags.Collision ||
			bInCollisionView ||
			View->Family->EngineShowFlags.Bounds ||
			View->Family->EngineShowFlags.VisualizeInstanceUpdates);
	#endif
	#if WITH_EDITOR
		bSetDynamicRelevance |= (IsSelected() && View->Family->EngineShowFlags.VertexColors);
	#endif
	#if NANITE_ENABLE_DEBUG_RENDERING
		bSetDynamicRelevance |= bDrawMeshCollisionIfComplex || bDrawMeshCollisionIfSimple;
	#endif

		if (bSetDynamicRelevance)
		{
			Result.bDynamicRelevance = true;

		#if NANITE_ENABLE_DEBUG_RENDERING
			// If we want to draw collision, needs to make sure we are considered relevant even if hidden
			if (View->Family->EngineShowFlags.Collision || bInCollisionView)
			{
				Result.bDrawRelevance = true;
			}
		#endif
		}

		if (!View->Family->EngineShowFlags.Materials
		#if NANITE_ENABLE_DEBUG_RENDERING
			|| bInCollisionView
		#endif
			)
		{
			Result.bOpaque = true;
		}

		CombinedMaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = Result.bOpaque && Result.bRenderInMainPass && DrawsVelocity();
	}

	return Result;
}

void FSceneProxy::GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
{
	// Attach the light to the primitive's static meshes.
	const ELightInteractionType InteractionType = MeshInfo.GetInteraction(LightSceneProxy).GetType();
	bRelevant     = (InteractionType != LIT_CachedIrrelevant);
	bDynamic      = (InteractionType == LIT_Dynamic);
	bLightMapped  = (InteractionType == LIT_CachedLightMap || InteractionType == LIT_CachedIrrelevant);
	bShadowMapped = (InteractionType == LIT_CachedSignedDistanceFieldShadowMap2D);
}

#if WITH_EDITOR

HHitProxy* FSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	LLM_SCOPE_BYTAG(Nanite);

	switch (HitProxyMode)
	{
		case FSceneProxyBase::EHitProxyMode::MaterialSection:
		{
			if (Component->GetOwner())
			{
				// Generate separate hit proxies for each material section, so that we can perform hit tests against each one.
				for (int32 SectionIndex = 0; SectionIndex < MaterialSections.Num(); ++SectionIndex)
				{
					FMaterialSection& Section = MaterialSections[SectionIndex];
					HHitProxy* ActorHitProxy = new HActor(Component->GetOwner(), Component, SectionIndex, SectionIndex);
					check(!Section.HitProxy);
					Section.HitProxy = ActorHitProxy;
					OutHitProxies.Add(ActorHitProxy);
				}
			}
			break;
		}

		case FSceneProxyBase::EHitProxyMode::PerInstance:
		{
			if (PerInstanceRenderData.IsValid() && PerInstanceRenderData->HitProxies.Num() > 0)
			{
				// Add any per-instance hit proxies.
				OutHitProxies += PerInstanceRenderData->HitProxies;
			}
			break;
		}

		default:
			break;
	}

	return Super::CreateHitProxies(Component, OutHitProxies);
}

#endif

FSceneProxy::FMeshInfo::FMeshInfo(const UStaticMeshComponent* InComponent)
{
	LLM_SCOPE_BYTAG(Nanite);

	if (InComponent->LightmapType == ELightmapType::ForceVolumetric)
	{
		SetGlobalVolumeLightmap(true);
	}
#if WITH_EDITOR
	else if (FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(InComponent, 0))
	{
		const FMeshMapBuildData* MeshMapBuildData = FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(InComponent, 0);
		if (MeshMapBuildData)
		{
			SetLightMap(MeshMapBuildData->LightMap);
			SetShadowMap(MeshMapBuildData->ShadowMap);
			SetResourceCluster(MeshMapBuildData->ResourceCluster);
			bCanUsePrecomputedLightingParametersFromGPUScene = true;
			IrrelevantLights = MeshMapBuildData->IrrelevantLights;
		}
	}
#endif
	else if (InComponent->LODData.Num() > 0)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[0];

		const FMeshMapBuildData* MeshMapBuildData = InComponent->GetMeshMapBuildData(ComponentLODInfo);
		if (MeshMapBuildData)
		{
			SetLightMap(MeshMapBuildData->LightMap);
			SetShadowMap(MeshMapBuildData->ShadowMap);
			SetResourceCluster(MeshMapBuildData->ResourceCluster);
			bCanUsePrecomputedLightingParametersFromGPUScene = true;
			IrrelevantLights = MeshMapBuildData->IrrelevantLights;
		}
	}
}

FLightInteraction FSceneProxy::FMeshInfo::GetInteraction(const FLightSceneProxy* LightSceneProxy) const
{
	// Ask base class
	ELightInteractionType LightInteraction = GetStaticInteraction(LightSceneProxy, IrrelevantLights);

	if (LightInteraction != LIT_MAX)
	{
		return FLightInteraction(LightInteraction);
	}

	// Use dynamic lighting if the light doesn't have static lighting.
	return FLightInteraction::Dynamic();
}

#if NANITE_ENABLE_DEBUG_RENDERING

// Loosely copied from FStaticMeshSceneProxy::FLODInfo::FLODInfo and modified for Nanite fallback
// TODO: Refactor all this to share common code with Nanite and regular SM scene proxy
FSceneProxy::FFallbackLODInfo::FFallbackLODInfo(
	const UStaticMeshComponent* InComponent,
	const FStaticMeshVertexFactoriesArray& InLODVertexFactories,
	int32 LODIndex,
	int32 InClampedMinLOD
)
{
	const auto FeatureLevel = InComponent->GetWorld()->FeatureLevel;

	FStaticMeshRenderData* MeshRenderData = InComponent->GetStaticMesh()->GetRenderData();
	FStaticMeshLODResources& LODModel = MeshRenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = InLODVertexFactories[LODIndex];

	if (LODIndex < InComponent->LODData.Num() && LODIndex >= InClampedMinLOD)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[LODIndex];

		// Initialize this LOD's overridden vertex colors, if it has any
		if (ComponentLODInfo.OverrideVertexColors)
		{
			bool bBroken = false;
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				if (Section.MaxVertexIndex >= ComponentLODInfo.OverrideVertexColors->GetNumVertices())
				{
					bBroken = true;
					break;
				}
			}
			if (!bBroken)
			{
				// the instance should point to the loaded data to avoid copy and memory waste
				OverrideColorVertexBuffer = ComponentLODInfo.OverrideVertexColors;
				check(OverrideColorVertexBuffer->GetStride() == sizeof(FColor)); //assumed when we set up the stream

				if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
				{
					TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters>* UniformBufferPtr = &OverrideColorVFUniformBuffer;
					const FLocalVertexFactory* LocalVF = &VFs.VertexFactoryOverrideColorVertexBuffer;
					FColorVertexBuffer* VertexBuffer = OverrideColorVertexBuffer;

					//temp measure to identify nullptr crashes deep in the renderer
					FString ComponentPathName = InComponent->GetPathName();
					checkf(LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices() > 0, TEXT("LOD: %i of PathName: %s has an empty position stream."), LODIndex, *ComponentPathName);

					ENQUEUE_RENDER_COMMAND(FLocalVertexFactoryCopyData)(
						[UniformBufferPtr, LocalVF, LODIndex, VertexBuffer, ComponentPathName](FRHICommandListImmediate& RHICmdList)
						{
							checkf(LocalVF->GetTangentsSRV(), TEXT("LOD: %i of PathName: %s has a null tangents srv."), LODIndex, *ComponentPathName);
							checkf(LocalVF->GetTextureCoordinatesSRV(), TEXT("LOD: %i of PathName: %s has a null texcoord srv."), LODIndex, *ComponentPathName);
							*UniformBufferPtr = CreateLocalVFUniformBuffer(LocalVF, LODIndex, VertexBuffer, 0, 0);
						});
				}
			}
		}
	}

	// Gather the materials applied to the LOD.
	Sections.Empty(MeshRenderData->LODResources[LODIndex].Sections.Num());
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
		FSectionInfo SectionInfo;

		// Determine the material applied to this element of the LOD.
		SectionInfo.Material = InComponent->GetMaterial(Section.MaterialIndex);
#if WITH_EDITORONLY_DATA
		SectionInfo.MaterialIndex = Section.MaterialIndex;
#endif

		if (!SectionInfo.Material)
		{
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// Per-section selection for the editor.
#if WITH_EDITORONLY_DATA
		if (GIsEditor)
		{
			if (InComponent->SelectedEditorMaterial >= 0)
			{
				SectionInfo.bSelected = (InComponent->SelectedEditorMaterial == Section.MaterialIndex);
			}
			else
			{
				SectionInfo.bSelected = (InComponent->SelectedEditorSection == SectionIndex);
			}
		}
#endif

		// Store the element info.
		Sections.Add(SectionInfo);
	}
}

#endif

void FSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	const FLightCacheInterface* LCI = &MeshInfo;
	DrawStaticElementsInternal(PDI, LCI);
}

// Loosely copied from FStaticMeshSceneProxy::GetDynamicMeshElements and modified for Nanite fallback
// TODO: Refactor all this to share common code with Nanite and regular SM scene proxy
void FSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
#if !WITH_EDITOR
	if (GNaniteOptimizedRelevance != 0)
	{
		// No dynamic relevance.
		return;
	}
#endif

	LLM_SCOPE_BYTAG(Nanite);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NaniteSceneProxy_GetMeshElements);
	checkSlow(IsInRenderingThread());

	const bool bIsLightmapSettingError = HasStaticLighting() && !HasValidSettingsForStaticLighting();
	const bool bProxyIsSelected = IsSelected();
	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

	bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
	const bool bInCollisionView = IsCollisionView(EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);

#if NANITE_ENABLE_DEBUG_RENDERING
	// Collision and bounds drawing
	FColor SimpleCollisionColor = FColor(157, 149, 223, 255);
	FColor ComplexCollisionColor = FColor(0, 255, 255, 255);


	// Make material for drawing complex collision mesh
	UMaterial* ComplexCollisionMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FLinearColor DrawCollisionColor = GetWireframeColor();

	// Collision view modes draw collision mesh as solid
	if (bInCollisionView)
	{
		ComplexCollisionMaterial = GEngine->ShadedLevelColorationUnlitMaterial;
	}
	// Wireframe, choose color based on complex or simple
	else
	{
		ComplexCollisionMaterial = GEngine->WireframeMaterial;
		DrawCollisionColor = (CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple) ? SimpleCollisionColor : ComplexCollisionColor;
	}

	// Create colored proxy
	FColoredMaterialRenderProxy* ComplexCollisionMaterialInstance = new FColoredMaterialRenderProxy(ComplexCollisionMaterial->GetRenderProxy(), DrawCollisionColor);
	Collector.RegisterOneFrameMaterialProxy(ComplexCollisionMaterialInstance);


	// Make a material for drawing simple solid collision stuff
	auto SimpleCollisionMaterialInstance = new FColoredMaterialRenderProxy(
		GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(),
		GetWireframeColor()
	);

	Collector.RegisterOneFrameMaterialProxy(SimpleCollisionMaterialInstance);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if (AllowDebugViewmodes())
			{
				// Should we draw the mesh wireframe to indicate we are using the mesh as collision
				bool bDrawComplexWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
				
				// Requested drawing complex in wireframe, but check that we are not using simple as complex
				bDrawComplexWireframeCollision |= (bDrawMeshCollisionIfComplex && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex);
				
				// Requested drawing simple in wireframe, and we are using complex as simple
				bDrawComplexWireframeCollision |= (bDrawMeshCollisionIfSimple && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);

				// If drawing complex collision as solid or wireframe
				if (bDrawComplexWireframeCollision || (bInCollisionView && bDrawComplexCollision))
				{
					// If we have at least one valid LOD to draw
					if (RenderData->LODResources.Num() > 0)
					{
						// Get LOD used for collision
						int32 DrawLOD = FMath::Clamp(LODForCollision, 0, RenderData->LODResources.Num() - 1);
						const FStaticMeshLODResources& LODModel = RenderData->LODResources[DrawLOD];

						// Iterate over sections of that LOD
						for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
						{
							// If this section has collision enabled
							if (LODModel.Sections[SectionIndex].bEnableCollision)
							{
							#if WITH_EDITOR
								// See if we are selected
								const bool bSectionIsSelected = FallbackLODs[DrawLOD].Sections[SectionIndex].bSelected;
							#else
								const bool bSectionIsSelected = false;
							#endif

								// Iterate over batches
								const int32 NumMeshBatches = 1; // TODO: GetNumMeshBatches()
								for (int32 BatchIndex = 0; BatchIndex < NumMeshBatches; BatchIndex++)
								{
									FMeshBatch& CollisionElement = Collector.AllocateMesh();
									if (GetCollisionMeshElement(DrawLOD, BatchIndex, SectionIndex, SDPG_World, ComplexCollisionMaterialInstance, CollisionElement))
									{
										Collector.AddMesh(ViewIndex, CollisionElement);
										INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, CollisionElement.GetNumPrimitives());
									}
								}
							}
						}
					}
				}
			}

			// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
			const bool bDrawSimpleWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple); 

			const FRenderTransform PrimitiveToWorld = (FMatrix44f)GetLocalToWorld();
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); InstanceIndex++)
			{
				FRenderTransform InstanceToWorld = InstanceSceneData[InstanceIndex].ComputeLocalToWorld(PrimitiveToWorld);
				FMatrix InstanceToWorldMatrix = InstanceToWorld.ToMatrix();

				if ((bDrawSimpleCollision || bDrawSimpleWireframeCollision) && BodySetup)
				{
					if (FMath::Abs(InstanceToWorldMatrix.Determinant()) < UE_SMALL_NUMBER)
					{
						// Catch this here or otherwise GeomTransform below will assert
						// This spams so commented out
						//UE_LOG(LogNanite, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
					}
					else
					{
						const bool bDrawSolid = !bDrawSimpleWireframeCollision;

						if (AllowDebugViewmodes() && bDrawSolid)
						{
							FTransform GeomTransform(InstanceToWorldMatrix);
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetWireframeColor().ToFColor(true), SimpleCollisionMaterialInstance, false, true, AlwaysHasVelocity(), ViewIndex, Collector);
						}
						// wireframe
						else
						{
							FTransform GeomTransform(InstanceToWorldMatrix);
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(SimpleCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), nullptr, (Owner == nullptr), false, AlwaysHasVelocity(), ViewIndex, Collector);
						}

						// The simple nav geometry is only used by dynamic obstacles for now
						if (StaticMesh->GetNavCollision() && StaticMesh->GetNavCollision()->IsDynamicObstacle())
						{
							// Draw the static mesh's body setup (simple collision)
							FTransform GeomTransform(InstanceToWorldMatrix);
							FColor NavCollisionColor = FColor(118, 84, 255, 255);
							StaticMesh->GetNavCollision()->DrawSimpleGeom(Collector.GetPDI(ViewIndex), GeomTransform, GetSelectionColor(NavCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true));
						}
					}
				}

				if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
				{
					DebugMassData[0].DrawDebugMass(Collector.GetPDI(ViewIndex), FTransform(InstanceToWorldMatrix));
				}

				if (EngineShowFlags.StaticMeshes)
				{
					RenderBounds(Collector.GetPDI(ViewIndex), EngineShowFlags, GetBounds(), !Owner || IsSelected());
				}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (EngineShowFlags.VisualizeInstanceUpdates && HasInstanceDebugData())
				{
					DrawWireStar(Collector.GetPDI(ViewIndex), (FVector)InstanceToWorld.Origin, 40.0f, WasInstanceXFormUpdatedThisFrame(InstanceIndex) ? FColor::Red : FColor::Green, EngineShowFlags.Game ? SDPG_World : SDPG_Foreground);

					Collector.GetPDI(ViewIndex)->DrawLine((FVector)InstanceToWorld.Origin, (FVector)InstanceToWorld.Origin + 40.0f * FVector(0, 0, 1), FColor::Blue, EngineShowFlags.Game ? SDPG_World : SDPG_Foreground);

					if (WasInstanceCustomDataUpdatedThisFrame(InstanceIndex))
					{
						DrawCircle(Collector.GetPDI(ViewIndex), (FVector)InstanceToWorld.Origin, FVector(1, 0, 0), FVector(0, 1, 0), FColor::Orange, 40.0f, 32, EngineShowFlags.Game ? SDPG_World : SDPG_Foreground);
					}
				}
#endif
			}
		}
	}
#endif // NANITE_ENABLE_DEBUG_RENDERING
}

#if NANITE_ENABLE_DEBUG_RENDERING

// Loosely copied from FStaticMeshSceneProxy::GetCollisionMeshElement and modified for Nanite fallback
// TODO: Refactor all this to share common code with Nanite and regular SM scene proxy
bool FSceneProxy::GetCollisionMeshElement(
	int32 LODIndex,
	int32 BatchIndex,
	int32 SectionIndex,
	uint8 InDepthPriorityGroup,
	const FMaterialRenderProxy* RenderProxy,
	FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

	if (Section.NumTriangles == 0)
	{
		return false;
	}

	const ::FVertexFactory* VertexFactory = nullptr;

	const FFallbackLODInfo& ProxyLODInfo = FallbackLODs[LODIndex];

	const bool bWireframe = false;
	const bool bUseReversedIndices = false;
	const bool bDitheredLODTransition = false;

	SetMeshElementGeometrySource(LODIndex, SectionIndex, bWireframe, bUseReversedIndices, VertexFactory, OutMeshBatch);

	FMeshBatchElement& OutMeshBatchElement = OutMeshBatch.Elements[0];

	if (ProxyLODInfo.OverrideColorVertexBuffer)
	{
		VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;
	
		OutMeshBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
	}
	else
	{
		VertexFactory = &VFs.VertexFactory;

		OutMeshBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
	}

	if (OutMeshBatchElement.NumPrimitives > 0)
	{
		OutMeshBatch.LODIndex = LODIndex;
		OutMeshBatch.VisualizeLODIndex = LODIndex;
		OutMeshBatch.VisualizeHLODIndex = 0;// HierarchicalLODIndex;
		OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
		OutMeshBatch.CastShadow = false;
		OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
		OutMeshBatch.LCI = &MeshInfo;// &ProxyLODInfo;
		OutMeshBatch.VertexFactory = VertexFactory;
		OutMeshBatch.MaterialRenderProxy = RenderProxy;
		OutMeshBatchElement.MinVertexIndex = Section.MinVertexIndex;
		OutMeshBatchElement.MaxVertexIndex = Section.MaxVertexIndex;
		OutMeshBatchElement.VisualizeElementIndex = SectionIndex;

		if (ForcedLodModel > 0)
		{
			OutMeshBatch.bDitheredLODTransition = false;

			OutMeshBatchElement.MaxScreenSize = 0.0f;
			OutMeshBatchElement.MinScreenSize = -1.0f;
		}
		else
		{
			OutMeshBatch.bDitheredLODTransition = bDitheredLODTransition;

			OutMeshBatchElement.MaxScreenSize = RenderData->ScreenSize[LODIndex].GetValue();
			OutMeshBatchElement.MinScreenSize = 0.0f;
			if (LODIndex < MAX_STATIC_MESH_LODS - 1)
			{
				OutMeshBatchElement.MinScreenSize = RenderData->ScreenSize[LODIndex + 1].GetValue();
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

#endif

void FSceneProxy::OnTransformChanged()
{
#if RHI_RAYTRACING
	bCachedRayTracingInstanceTransformsValid = false;
#endif

	if (!bHasPerInstanceLocalBounds)
	{
		check(InstanceLocalBounds.Num() <= 1);
		InstanceLocalBounds.SetNumUninitialized(1);
		if (StaticMesh != nullptr)
		{
			InstanceLocalBounds[0] = StaticMesh->GetBounds();
		}
		else
		{
			InstanceLocalBounds[0] = GetLocalBounds();
		}
	}
}

bool FSceneProxy::GetInstanceDrawDistanceMinMax(FVector2f& OutDistanceMinMax) const
{
	if (EndCullDistance > 0)
	{
		OutDistanceMinMax = FVector2f(0.0f, float(EndCullDistance));
		return true;
	}
	else
	{
		OutDistanceMinMax = FVector2f(0.0f);
		return false;
	}
}

bool FSceneProxy::GetInstanceWorldPositionOffsetDisableDistance(float& OutWPODisableDistance) const
{
	OutWPODisableDistance = float(InstanceWPODisableDistance);
	return InstanceWPODisableDistance != 0;
}

#if RHI_RAYTRACING
bool FSceneProxy::HasRayTracingRepresentation() const
{
	return bHasRayTracingInstances;
}

int32 FSceneProxy::GetFirstValidRaytracingGeometryLODIndex() const
{
	int32 NumLODs = RenderData->LODResources.Num();
	int LODIndex = 0;

#if WITH_EDITOR
	// If coarse mesh streaming mode is set to 2 then we force use the lowest LOD to visualize streamed out coarse meshes
	if (Nanite::FCoarseMeshStreamingManager::GetStreamingMode() == 2)
	{
		LODIndex = NumLODs - 1;
	}
#endif // WITH_EDITOR

	// find the first valid RT geometry index
	for (; LODIndex < NumLODs; ++LODIndex)
	{
		if (RenderData->LODResources[LODIndex].RayTracingGeometry.IsValid())
		{
			return LODIndex;
		}
	}

	return INDEX_NONE;
}

void FSceneProxy::SetupRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& Materials, bool bUseNaniteVertexFactory) const
{
	check(Materials.Num() <= MaterialSections.Num());
	for (int32 SectionIndex = 0; SectionIndex < Materials.Num(); ++SectionIndex)
	{
		const FMaterialSection& MaterialSection = MaterialSections[SectionIndex];
		FMeshBatch& MeshBatch = Materials[SectionIndex];
		if (bUseNaniteVertexFactory)
		{
			MeshBatch.VertexFactory = GVertexFactoryResource.GetVertexFactory();
		}
		else
		{
			MeshBatch.VertexFactory = &RenderData->LODVertexFactories[LODIndex].VertexFactory;
		}
		MeshBatch.MaterialRenderProxy = MaterialSection.ShadingMaterialProxy;
		MeshBatch.bWireframe = false;
		MeshBatch.SegmentIndex = SectionIndex;
		MeshBatch.LODIndex = 0;
#if RHI_RAYTRACING
		MeshBatch.CastRayTracedShadow = CastsDynamicShadow(); // Relying on BuildRayTracingInstanceMaskAndFlags(...) to check Material.CastsRayTracedShadows()
#endif
		MeshBatch.Elements[0].PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}
}

void FSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	if (CVarRayTracingNaniteProxyMeshes.GetValueOnRenderThread() == 0 || !bHasRayTracingInstances)
	{
		return;
	}

	// try and find the first valid RT geometry index
	int32 ValidLODIndex = GetFirstValidRaytracingGeometryLODIndex();
	if (ValidLODIndex == INDEX_NONE)
	{
		return;
	}

	// Setup a new instance
	FRayTracingInstance& RayTracingInstance = OutRayTracingInstances.Emplace_GetRef();
	RayTracingInstance.Geometry = &RenderData->LODResources[ValidLODIndex].RayTracingGeometry;
	RayTracingInstance.bApplyLocalBoundsTransform = RayTracingInstance.Geometry->RayTracingGeometryRHI->GetInitializer().GeometryType == RTGT_Procedural;

	const int32 InstanceCount = InstanceSceneData.Num();
	if (CachedRayTracingInstanceTransforms.Num() != InstanceCount || !bCachedRayTracingInstanceTransformsValid)
	{
		const FRenderTransform PrimitiveToWorld = (FMatrix44f)GetLocalToWorld();

		CachedRayTracingInstanceTransforms.SetNumUninitialized(InstanceCount);
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
		{
			const FPrimitiveInstance& Instance = InstanceSceneData[InstanceIndex];
			const FRenderTransform InstanceLocalToWorld = Instance.ComputeLocalToWorld(PrimitiveToWorld);
			CachedRayTracingInstanceTransforms[InstanceIndex] = InstanceLocalToWorld.ToMatrix();
		}
		bCachedRayTracingInstanceTransformsValid = true;
	}

	// Transforms are persistently allocated, so we can just return them by pointer.
	RayTracingInstance.InstanceTransformsView = CachedRayTracingInstanceTransforms;
	RayTracingInstance.NumTransforms = CachedRayTracingInstanceTransforms.Num();

	// Setup the cached materials again when the LOD changes
	if (ValidLODIndex != CachedRayTracingMaterialsLODIndex)
	{
		const bool bUseNaniteVertexFactory = GetRayTracingMode() != ERayTracingMode::Fallback;
		SetupRayTracingMaterials(ValidLODIndex, CachedRayTracingMaterials, bUseNaniteVertexFactory);
		CachedRayTracingMaterialsLODIndex = ValidLODIndex;

		// Request rebuild
		CachedRayTracingInstanceMaskAndFlags.Mask = 0;
	}
	RayTracingInstance.MaterialsView = CachedRayTracingMaterials;

	if (CachedRayTracingInstanceMaskAndFlags.Mask == 0)
	{
		CachedRayTracingInstanceMaskAndFlags = BuildRayTracingInstanceMaskAndFlags(CachedRayTracingMaterials, GetScene().GetFeatureLevel());
	}
	RayTracingInstance.Mask = CachedRayTracingInstanceMaskAndFlags.Mask;
	RayTracingInstance.bForceOpaque = CachedRayTracingInstanceMaskAndFlags.bForceOpaque;
	RayTracingInstance.bDoubleSided = CachedRayTracingInstanceMaskAndFlags.bDoubleSided;
}

ERayTracingPrimitiveFlags FSceneProxy::GetCachedRayTracingInstance(FRayTracingInstance& RayTracingInstance)
{
	if (!(IsVisibleInRayTracing() && ShouldRenderInMainPass() && (IsDrawnInGame() || AffectsIndirectLightingWhileHidden())) && !IsRayTracingFarField())
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	if (CVarRayTracingNaniteProxyMeshes.GetValueOnRenderThread() == 0 || !bHasRayTracingInstances)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));

	if (bIsHierarchicalInstancedStaticMesh && RayTracingHISMCVar && RayTracingHISMCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	if (bIsLandscapeGrass && RayTracingLandscapeGrassCVar && RayTracingLandscapeGrassCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Excluded;
	}

	// try and find the first valid RT geometry index
	int32 ValidLODIndex = GetFirstValidRaytracingGeometryLODIndex();
	if (ValidLODIndex == INDEX_NONE)
	{
		// If there is a streaming handle (but no valid LOD available), then give the streaming flag to make sure it's not excluded
		// It's still needs to be processed during TLAS build because this will drive the streaming of these resources.
		return (CoarseMeshStreamingHandle != INDEX_NONE) ? ERayTracingPrimitiveFlags::Streaming : ERayTracingPrimitiveFlags::Excluded;
	}

	RayTracingInstance.Geometry = &RenderData->LODResources[ValidLODIndex].RayTracingGeometry;
	RayTracingInstance.bApplyLocalBoundsTransform = RayTracingInstance.Geometry->RayTracingGeometryRHI->GetInitializer().GeometryType == RTGT_Procedural;

	checkf(SupportsInstanceDataBuffer() && InstanceSceneData.Num() <= GetPrimitiveSceneInfo()->GetNumInstanceSceneDataEntries(),
		TEXT("Primitives using ERayTracingPrimitiveFlags::CacheInstances require instance transforms available in GPUScene"));

	RayTracingInstance.NumTransforms = InstanceSceneData.Num();
	// When ERayTracingPrimitiveFlags::CacheInstances is used, instance transforms are copied from GPUScene while building ray tracing instance buffer.

	if (RayTracingInstance.Geometry->Initializer.GeometryType == RTGT_Procedural)
	{
		// Currently we only support 1 material when using procedural ray tracing primitive
		RayTracingInstance.Materials.SetNum(1);
	}
	else
	{
		RayTracingInstance.Materials.SetNum(MaterialSections.Num());
	}

	const bool bUseNaniteVertexFactory = GetRayTracingMode() != ERayTracingMode::Fallback;
	SetupRayTracingMaterials(ValidLODIndex, RayTracingInstance.Materials, bUseNaniteVertexFactory);

	const bool bIsRayTracingFarField = IsRayTracingFarField();

	RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel(), bIsRayTracingFarField ? ERayTracingInstanceLayer::FarField : ERayTracingInstanceLayer::NearField);

	// setup the flags
	ERayTracingPrimitiveFlags ResultFlags = ERayTracingPrimitiveFlags::StaticMesh | ERayTracingPrimitiveFlags::CacheMeshCommands | ERayTracingPrimitiveFlags::CacheInstances;

	if (CoarseMeshStreamingHandle != INDEX_NONE)
	{
		ResultFlags |= ERayTracingPrimitiveFlags::Streaming;
	}

	if (bIsRayTracingFarField)
	{
		ResultFlags |= ERayTracingPrimitiveFlags::FarField;
	}

	return ResultFlags;
}

#endif // RHI_RAYTRACING

#if NANITE_ENABLE_DEBUG_RENDERING

// Loosely copied from FStaticMeshSceneProxy::SetMeshElementGeometrySource and modified for Nanite fallback
// TODO: Refactor all this to share common code with Nanite and regular SM scene proxy
uint32 FSceneProxy::SetMeshElementGeometrySource(
	int32 LODIndex,
	int32 SectionIndex,
	bool bWireframe,
	bool bUseReversedIndices,
	const ::FVertexFactory* VertexFactory,
	FMeshBatch& OutMeshElement) const
{
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

	const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
	if (Section.NumTriangles == 0)
	{
		return 0;
	}

	const FFallbackLODInfo& LODInfo = FallbackLODs[LODIndex];
	const FFallbackLODInfo::FSectionInfo& SectionInfo = LODInfo.Sections[SectionIndex];

	FMeshBatchElement& OutMeshBatchElement = OutMeshElement.Elements[0];
	uint32 NumPrimitives = 0;

	if (bWireframe)
	{
		if (LODModel.AdditionalIndexBuffers && LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.IsInitialized())
		{
			OutMeshElement.Type = PT_LineList;
			OutMeshBatchElement.FirstIndex = 0;
			OutMeshBatchElement.IndexBuffer = &LODModel.AdditionalIndexBuffers->WireframeIndexBuffer;
			NumPrimitives = LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.GetNumIndices() / 2;
		}
		else
		{
			OutMeshBatchElement.FirstIndex = 0;
			OutMeshBatchElement.IndexBuffer = &LODModel.IndexBuffer;
			NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;

			OutMeshElement.Type = PT_TriangleList;
			OutMeshElement.bWireframe = true;
			OutMeshElement.bDisableBackfaceCulling = true;
		}
	}
	else
	{
		OutMeshElement.Type = PT_TriangleList;

		OutMeshBatchElement.IndexBuffer = bUseReversedIndices ? &LODModel.AdditionalIndexBuffers->ReversedIndexBuffer : &LODModel.IndexBuffer;
		OutMeshBatchElement.FirstIndex = Section.FirstIndex;
		NumPrimitives = Section.NumTriangles;
	}

	OutMeshBatchElement.NumPrimitives = NumPrimitives;
	OutMeshElement.VertexFactory = VertexFactory;

	return NumPrimitives;
}

bool FSceneProxy::IsReversedCullingNeeded(bool bUseReversedIndices) const
{
	return (bReverseCulling || IsLocalToWorldDeterminantNegative()) && !bUseReversedIndices;
}

#endif

FResourceMeshInfo FSceneProxy::GetResourceMeshInfo() const
{
	FResourceMeshInfo OutInfo;

	OutInfo.NumClusters = Resources->NumClusters;
	OutInfo.NumNodes = Resources->NumHierarchyNodes;
	OutInfo.NumVertices = Resources->NumInputVertices;
	OutInfo.NumTriangles = Resources->NumInputTriangles;
	OutInfo.NumMaterials = MaterialMaxIndex + 1;
	OutInfo.DebugName = StaticMesh->GetFName();

	{
		const uint32 FirstLODIndex = 0; // Only data from LOD0 is used.
		const FStaticMeshLODResources& MeshResources = RenderData->LODResources[FirstLODIndex];
		const FStaticMeshSectionArray& MeshSections = MeshResources.Sections;

		OutInfo.NumSegments = MeshSections.Num();

		OutInfo.SegmentMapping.Init(INDEX_NONE, MaterialMaxIndex + 1);

		for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
		{
			const FStaticMeshSection& MeshSection = MeshSections[SectionIndex];
			OutInfo.SegmentMapping[MeshSection.MaterialIndex] = SectionIndex;
		}
	}

	return MoveTemp(OutInfo);
}

const FCardRepresentationData* FSceneProxy::GetMeshCardRepresentation() const
{
	return CardRepresentationData;
}

void FSceneProxy::GetDistanceFieldAtlasData(const FDistanceFieldVolumeData*& OutDistanceFieldData, float& SelfShadowBias) const
{
	OutDistanceFieldData = DistanceFieldData;
	SelfShadowBias = DistanceFieldSelfShadowBias;
}

void FSceneProxy::GetDistanceFieldInstanceData(TArray<FRenderTransform>& InstanceLocalToPrimitiveTransforms) const
{
	check(InstanceLocalToPrimitiveTransforms.IsEmpty());

	if (DistanceFieldData)
	{
		InstanceLocalToPrimitiveTransforms.SetNumUninitialized(InstanceSceneData.Num());
		for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); ++InstanceIndex)
		{
			const FPrimitiveInstance& Instance = InstanceSceneData[InstanceIndex];
			InstanceLocalToPrimitiveTransforms[InstanceIndex] = Instance.LocalToPrimitive;
		}
	}
}

bool FSceneProxy::HasDistanceFieldRepresentation() const
{
	return CastsDynamicShadow() && AffectsDistanceFieldLighting() && DistanceFieldData;
}

int32 FSceneProxy::GetLightMapCoordinateIndex() const
{
	const int32 LightMapCoordinateIndex = StaticMesh != nullptr ? StaticMesh->GetLightMapCoordinateIndex() : INDEX_NONE;
	return LightMapCoordinateIndex;
}

bool FSceneProxy::IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const
{
	bDrawSimpleCollision = bDrawComplexCollision = false;

	const bool bInCollisionView = EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;

#if NANITE_ENABLE_DEBUG_RENDERING
	// If in a 'collision view' and collision is enabled
	if (bInCollisionView && IsCollisionEnabled())
	{
		// See if we have a response to the interested channel
		bool bHasResponse = EngineShowFlags.CollisionPawn && CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore;
		bHasResponse |= EngineShowFlags.CollisionVisibility && CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore;

		if (bHasResponse)
		{
			// Visibility uses complex and pawn uses simple. However, if UseSimpleAsComplex or UseComplexAsSimple is used we need to adjust accordingly
			bDrawComplexCollision = (EngineShowFlags.CollisionVisibility && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex) || (EngineShowFlags.CollisionPawn && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
			bDrawSimpleCollision = (EngineShowFlags.CollisionPawn && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple) || (EngineShowFlags.CollisionVisibility && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseSimpleAsComplex);
		}
	}
#endif
	return bInCollisionView;
}

uint32 FSceneProxy::GetMemoryFootprint() const
{
	return sizeof( *this ) + GetAllocatedSize();
}

void AuditMaterials(const UStaticMeshComponent* Component, FMaterialAudit& Audit)
{
	Audit.bHasAnyError = false;
	Audit.Entries.Reset();

	if (Component != nullptr)
	{
		const int32 NumMaterials = Component->GetNumMaterials();
		const TArray<FName> MaterialSlotNames = Component->GetMaterialSlotNames();
		for (const FName& SlotName : MaterialSlotNames)
		{
			FMaterialAuditEntry& Entry = Audit.Entries.AddDefaulted_GetRef();
			Entry.MaterialSlotName = SlotName;
			Entry.MaterialIndex = Component->GetMaterialIndex(SlotName);
			Entry.Material = Component->GetMaterial(Entry.MaterialIndex);
			Entry.bHasNullMaterial = Entry.Material == nullptr;

			if (Entry.Material != nullptr)
			{
				const UMaterial* Material = Entry.Material->GetMaterial_Concurrent();
				check(Material != nullptr); // Should always be valid here

				const FMaterialCachedExpressionData& CachedMaterialData = Material->GetCachedExpressionData();
				Entry.bHasVertexInterpolator	= CachedMaterialData.bHasVertexInterpolator;
				Entry.bHasPerInstanceRandomID	= CachedMaterialData.bHasPerInstanceRandom;
				Entry.bHasPerInstanceCustomData	= CachedMaterialData.bHasPerInstanceCustomData;
				Entry.bHasPixelDepthOffset		= Material->HasPixelDepthOffsetConnected();
				Entry.bHasWorldPositionOffset	= Material->HasVertexPositionOffsetConnected();
				Entry.bHasUnsupportedBlendMode	= !IsSupportedBlendMode(Entry.Material->GetBlendMode());
				Entry.bHasInvalidUsage			= !Material->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);
			}

			Entry.bHasAnyError =
				Entry.bHasNullMaterial |
				Entry.bHasUnsupportedBlendMode |
				Entry.bHasInvalidUsage;

			if (GNaniteErrorOnWorldPositionOffset != 0)
			{
				Entry.bHasAnyError |= Entry.bHasWorldPositionOffset;
			}

			if (GNaniteErrorOnPixelDepthOffset != 0)
			{
				Entry.bHasAnyError |= Entry.bHasPixelDepthOffset;
			}

			if (GNaniteErrorOnVertexInterpolator != 0)
			{
				Entry.bHasAnyError |= Entry.bHasVertexInterpolator;
			}

			Audit.bHasAnyError |= Entry.bHasAnyError;
		}
	}

	if (Audit.bHasAnyError)
	{
		// Only populate on error for performance/memory reasons
		Audit.AssetName = Component->GetStaticMesh()->GetName();
	}
}

void FixupMaterials(FMaterialAudit& Audit)
{
	if (!Audit.bHasAnyError)
	{
		return;
	}

	for (FMaterialAuditEntry& Entry : Audit.Entries)
	{
		if (!Entry.bHasAnyError)
		{
			check(Entry.Material != nullptr); // Should always be valid here
			continue;
		}

		if (Entry.bHasNullMaterial)
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("Invalid material [null] used on Nanite static mesh [%s] - forcing default material instead."), *Audit.AssetName);
		}
		else if (Entry.bHasInvalidUsage)
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("Invalid material usage for Nanite static mesh [%s] - forcing default material instead."), *Audit.AssetName);
		}
		else if (Entry.bHasUnsupportedBlendMode)
		{
			const FString BlendModeName = GetBlendModeString(Entry.Material->GetBlendMode());
			UE_LOG
			(
				LogStaticMesh, Warning,
				TEXT("Invalid material [%s] used on Nanite static mesh [%s] - forcing default material instead. Only opaque blend mode is currently supported, [%s] blend mode was specified."),
				*Entry.Material->GetName(),
				*Audit.AssetName,
				*BlendModeName
			);
		}
		else if (Entry.bHasWorldPositionOffset && GNaniteErrorOnWorldPositionOffset != 0)
		{
			UE_LOG
			(
				LogStaticMesh, Warning,
				TEXT("Invalid material [%s] used on Nanite static mesh [%s] - forcing default material instead. World position offset is not supported by Nanite."),
				*Entry.Material->GetName(),
				*Audit.AssetName
			);
		}
		else if (Entry.bHasPixelDepthOffset && GNaniteErrorOnPixelDepthOffset != 0)
		{
			UE_LOG
			(
				LogStaticMesh, Warning,
				TEXT("Invalid material [%s] used on Nanite static mesh [%s] - forcing default material instead. Pixel depth offset is not supported by Nanite."),
				*Entry.Material->GetName(),
				*Audit.AssetName
			);
		}
		else if (Entry.bHasVertexInterpolator && GNaniteErrorOnVertexInterpolator != 0)
		{
			UE_LOG
			(
				LogStaticMesh, Warning,
				TEXT("Invalid material [%s] used on Nanite static mesh [%s] - forcing default material instead. Vertex interpolator nodes are not supported by Nanite."),
				*Entry.Material->GetName(),
				*Audit.AssetName
			);
		}
		else
		{
			check(false);
		}

		// Route all invalid materials to default material
		Entry.Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

bool IsSupportedBlendMode(EBlendMode Mode)
{
	if (GNaniteErrorOnMaskedBlendMode != 0)
	{
		return Mode == EBlendMode::BLEND_Opaque;
	}
	else
	{
		return Mode == EBlendMode::BLEND_Opaque || Mode == EBlendMode::BLEND_Masked;
	}
}

bool IsSupportedMaterialDomain(EMaterialDomain Domain)
{
	return Domain == EMaterialDomain::MD_Surface;
}

bool IsWorldPositionOffsetSupported()
{
	return true;
}

void FVertexFactoryResource::InitRHI()
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);
		VertexFactory = new FVertexFactory(ERHIFeatureLevel::SM5);
		VertexFactory->InitResource();

		if (CVarNaniteAllowComputeMaterials.GetValueOnRenderThread() != 0)
		{
			VertexFactory2 = new FNaniteVertexFactory(ERHIFeatureLevel::SM6);
			VertexFactory2->InitResource();
		}
	}
}

void FVertexFactoryResource::ReleaseRHI()
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);

		delete VertexFactory;
		VertexFactory = nullptr;

		if (CVarNaniteAllowComputeMaterials.GetValueOnRenderThread() != 0)
		{
			delete VertexFactory2;
			VertexFactory2 = nullptr;
		}
	}
}

TGlobalResource< FVertexFactoryResource > GVertexFactoryResource;

} // namespace Nanite

FNaniteVertexFactory::FNaniteVertexFactory(ERHIFeatureLevel::Type FeatureLevel) : ::FVertexFactory(FeatureLevel)
{
	// We do not want a vertex declaration since this factory is pure compute
	bNeedsDeclaration = false;
}

FNaniteVertexFactory::~FNaniteVertexFactory()
{
	ReleaseResource();
}

void FNaniteVertexFactory::InitRHI()
{
	LLM_SCOPE_BYTAG(Nanite);
}

bool FNaniteVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	static const bool bAllowComputeMaterials = CVarNaniteAllowComputeMaterials.GetValueOnAnyThread() != 0;

	bool bShouldCompile =
		bAllowComputeMaterials &&
		Parameters.ShaderType->GetFrequency() == SF_Compute &&
		(Parameters.MaterialParameters.bIsUsedWithNanite || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		Nanite::IsSupportedMaterialDomain(Parameters.MaterialParameters.MaterialDomain) &&
		Nanite::IsSupportedBlendMode(Parameters.MaterialParameters.BlendMode) &&
		DoesPlatformSupportNanite(Parameters.Platform);

	return bShouldCompile;
}

void FNaniteVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("IS_NANITE_SHADING_PASS"), 1);
	OutEnvironment.SetDefine(TEXT("IS_NANITE_PASS"), 1);
	OutEnvironment.SetDefine(TEXT("USE_ANALYTIC_DERIVATIVES"), 1);
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	OutEnvironment.SetDefine(TEXT("NANITE_USE_UNIFORM_BUFFER"), 1);
	OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 1);
	OutEnvironment.SetDefine(TEXT("NANITE_COMPUTE_SHADE"), 1);

	// Get data from GPUSceneParameters rather than View.
	// TODO: Profile this vs view uniform buffer path
	//OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNaniteVertexFactory, "/Engine/Private/Nanite/NaniteVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsNaniteRendering
	| EVertexFactoryFlags::SupportsComputeShading
	| EVertexFactoryFlags::SupportsManualVertexFetch
);
