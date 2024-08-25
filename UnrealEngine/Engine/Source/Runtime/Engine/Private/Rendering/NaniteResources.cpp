// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/NaniteResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Engine.h"
#include "EngineLogs.h"
#include "EngineModule.h"
#include "HAL/LowLevelMemStats.h"
#include "Rendering/NaniteStreamingManager.h"
#include "Rendering/RayTracingGeometryManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/InstancedStaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "CommonRenderResources.h"
#include "DistanceFieldAtlas.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled
#include "MaterialCachedData.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "PrimitiveSceneInfo.h"
#include "SceneInterface.h"
#include "StaticMeshComponentLODInfo.h"
#include "Stats/StatsTrace.h"

#include "ComponentRecreateRenderStateContext.h"
#include "StaticMeshSceneProxyDesc.h"
#include "InstancedStaticMeshSceneProxyDesc.h"

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Rendering/StaticLightingSystemInterface.h"
#endif

#if WITH_EDITORONLY_DATA
#include "UObject/Package.h"
#endif

#if NANITE_ENABLE_DEBUG_RENDERING
#include "AI/Navigation/NavCollisionBase.h"
#include "PhysicsEngine/BodySetup.h"
#endif

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

DEFINE_GPU_STAT(NaniteStreaming);
DEFINE_GPU_STAT(NaniteReadback);

DECLARE_LLM_MEMORY_STAT(TEXT("Nanite"), STAT_NaniteLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Nanite"), STAT_NaniteSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(Nanite, NAME_None, NAME_None, GET_STATFNAME(STAT_NaniteLLM), GET_STATFNAME(STAT_NaniteSummaryLLM));

static TAutoConsoleVariable<int32> CVarNaniteAllowComputeMaterials(
	TEXT("r.Nanite.AllowComputeMaterials"),
	1,
	TEXT("Whether to enable support for Nanite compute materials"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarNaniteAllowLegacyMaterials(
	TEXT("r.Nanite.AllowLegacyMaterials"),
	1,
	TEXT("Whether to enable support for Nanite legacy materials"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarNaniteUseComputeMaterials(
	TEXT("r.Nanite.ComputeMaterials"),
	1,
	TEXT("Whether to enable Nanite compute materials"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// Force recaching of Nanite draw commands when toggled.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteAllowTessellation(
	TEXT("r.Nanite.AllowTessellation"),
	0, // Off by default
	TEXT("Whether to enable support for (highly experimental) Nanite runtime tessellation"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarNaniteAllowSplineMeshes(
	TEXT("r.Nanite.AllowSplineMeshes"),
	1,
	TEXT("Whether to enable support for Nanite spline meshes"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

int32 GNaniteAllowMaskedMaterials = 1;
FAutoConsoleVariableRef CVarNaniteAllowMaskedMaterials(
	TEXT("r.Nanite.AllowMaskedMaterials"),
	GNaniteAllowMaskedMaterials,
	TEXT("Whether to allow meshes using masked materials to render using Nanite."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
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
	TEXT("1 - streamed out mesh;"),
	ECVF_RenderThreadSafe
);

int32 GNaniteCustomDepthEnabled = 1;
static FAutoConsoleVariableRef CVarNaniteCustomDepthStencil(
	TEXT("r.Nanite.CustomDepth"),
	GNaniteCustomDepthEnabled,
	TEXT("Whether to allow Nanite to render in the CustomDepth pass"),
	ECVF_RenderThreadSafe
);

namespace Nanite
{
ERayTracingMode GetRayTracingMode()
{
	return (ERayTracingMode)GNaniteRayTracingMode;
}

bool GetSupportsCustomDepthRendering()
{
	return GNaniteCustomDepthEnabled != 0;
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
	Ar << PageStreamingState.MaxHierarchyDepth;
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
	if( !StripFlags.IsAudioVisualDataStripped() )
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
		Ar << NormalPrecision;
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

void FVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
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
		NaniteLegacyMaterialsSupported() &&
		(Parameters.MaterialParameters.bIsUsedWithNanite || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		IsSupportedMaterialDomain(Parameters.MaterialParameters.MaterialDomain) &&
		IsSupportedBlendMode(Parameters.MaterialParameters) &&
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
	OutEnvironment.SetDefine(TEXT("ALWAYS_EVALUATE_WORLD_POSITION_OFFSET"),
		Parameters.MaterialParameters.bAlwaysEvaluateWorldPositionOffset ? 1 : 0);

	if (NaniteSplineMeshesSupported())
	{
		if (Parameters.MaterialParameters.bIsUsedWithSplineMeshes || Parameters.MaterialParameters.bIsDefaultMaterial)
		{
			// NOTE: This effectively means the logic to deform vertices will be added to the barycentrics calculation in the
			// Nanite shading PS, but will be branched over on instances that do not supply spline mesh parameters. If that
			// frequently causes occupancy issues, we may want to consider ways to split the spline meshes into their own
			// shading bin and permute the PS.
			OutEnvironment.SetDefine(TEXT("USE_SPLINEDEFORM"), 1);
			OutEnvironment.SetDefine(TEXT("USE_SPLINE_MESH_SCENE_RESOURCES"), UseSplineMeshSceneResources(Parameters.Platform));
		}
	}

	OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
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
	| EVertexFactoryFlags::SupportsLumenMeshCards
	| EVertexFactoryFlags::SupportsLandscape
);

void FSceneProxyBase::FMaterialSection::ResetToDefaultMaterial(bool bShading, bool bRaster)
{
	UMaterialInterface* ShadingMaterial = bHidden ? GEngine->NaniteHiddenSectionMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface);
	FMaterialRenderProxy* DefaultRP = ShadingMaterial->GetRenderProxy();
	if (bShading)
	{
		ShadingMaterialProxy = DefaultRP;
	}
	if (bRaster)
	{
		RasterMaterialProxy = DefaultRP;
	}
}

#if WITH_EDITOR
HHitProxy* FSceneProxyBase::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	return FSceneProxyBase::CreateHitProxies(Component->GetPrimitiveComponentInterface(), OutHitProxies);
}

HHitProxy* FSceneProxyBase::CreateHitProxies(IPrimitiveComponent* ComponentInterface,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{	
	// Subclasses will have populated OutHitProxies already - update the hit proxy ID before used by GPUScene
	HitProxyIds.SetNumUninitialized(OutHitProxies.Num());
	for (int32 HitProxyId = 0; HitProxyId < HitProxyIds.Num(); ++HitProxyId)
	{
		HitProxyIds[HitProxyId] = OutHitProxies[HitProxyId]->Id;
	}

	// Create a default hit proxy, but don't add it to our internal list (needed for proper collision mesh selection)
	return FPrimitiveSceneProxy::CreateHitProxies(ComponentInterface, OutHitProxies);
}
#endif

void FSceneProxyBase::DrawStaticElementsInternal(FStaticPrimitiveDrawInterface* PDI, const FLightCacheInterface* LCI)
{
	LLM_SCOPE_BYTAG(Nanite);

	FMeshBatch MeshBatch;
	if (NaniteLegacyMaterialsSupported())
	{
		MeshBatch.VertexFactory = GVertexFactoryResource.GetVertexFactory();
	}
	else
	{
		// TODO: Remove
		// Dummy factory that will be ignored later on
		MeshBatch.VertexFactory = GVertexFactoryResource.GetVertexFactory2();
	}

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

void FSceneProxyBase::OnMaterialsUpdated()
{
	CombinedMaterialRelevance = FMaterialRelevance();
	MaxWPOExtent = 0.0f;
	MinMaxMaterialDisplacement = FVector2f::Zero();
	bHasProgrammableRaster = false;
	bHasDynamicDisplacement = false;
	bAnyMaterialAlwaysEvaluatesWorldPositionOffset = false;
	bAnyMaterialHasPixelAnimation = false;

	const bool bUseTessellation = UseNaniteTessellation();

	EShaderPlatform ShaderPlatform = GetScene().GetShaderPlatform();
	bool bVelocityEncodeHasPixelAnimation = VelocityEncodeHasPixelAnimation(ShaderPlatform);

	for (auto& MaterialSection : MaterialSections)
	{
		const UMaterialInterface* ShadingMaterial = MaterialSection.ShadingMaterialProxy->GetMaterialInterface();

		// Update section relevance and combined material relevance
		MaterialSection.MaterialRelevance = ShadingMaterial->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
		CombinedMaterialRelevance |= MaterialSection.MaterialRelevance;

		// Now that the material relevance is updated, determine if any material has programmable raster
		const bool bProgrammableRaster = MaterialSection.IsProgrammableRaster(bEvaluateWorldPositionOffset);
		bHasProgrammableRaster |= bProgrammableRaster;
		
		// Update the RasterMaterialProxy, which is dependent on hidden status and programmable rasterization
		if (MaterialSection.bHidden)
		{
			MaterialSection.RasterMaterialProxy = GEngine->NaniteHiddenSectionMaterial.Get()->GetRenderProxy();
		}
		else if (bProgrammableRaster)
		{
			MaterialSection.RasterMaterialProxy = MaterialSection.ShadingMaterialProxy;
		}
		else
		{
			MaterialSection.RasterMaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		}

		// Determine if we need to always evaluate WPO for this material slot.
		const bool bHasWPO = MaterialSection.MaterialRelevance.bUsesWorldPositionOffset;
		MaterialSection.bAlwaysEvaluateWPO = bHasWPO && ShadingMaterial->ShouldAlwaysEvaluateWorldPositionOffset();
		bAnyMaterialAlwaysEvaluatesWorldPositionOffset |= MaterialSection.bAlwaysEvaluateWPO;

		// Determine if has any pixel animation.
		bAnyMaterialHasPixelAnimation |= ShadingMaterial->HasPixelAnimation() && bVelocityEncodeHasPixelAnimation && IsOpaqueOrMaskedBlendMode(ShadingMaterial->GetBlendMode());

		// Determine max extent of WPO
		if (MaterialSection.bAlwaysEvaluateWPO || (bEvaluateWorldPositionOffset && bHasWPO))
		{
			MaterialSection.MaxWPOExtent = ShadingMaterial->GetMaxWorldPositionOffsetDisplacement();
			MaxWPOExtent = FMath::Max(MaxWPOExtent, MaterialSection.MaxWPOExtent);
		}
		else
		{
			MaterialSection.MaxWPOExtent = 0.0f;
		}

		// Determine min/max tessellation displacement
		if (bUseTessellation && MaterialSection.MaterialRelevance.bUsesDisplacement)
		{
			MaterialSection.DisplacementScaling = ShadingMaterial->GetDisplacementScaling();
			
			const float MinDisplacement = (0.0f - MaterialSection.DisplacementScaling.Center) * MaterialSection.DisplacementScaling.Magnitude;
			const float MaxDisplacement = (1.0f - MaterialSection.DisplacementScaling.Center) * MaterialSection.DisplacementScaling.Magnitude;

			MinMaxMaterialDisplacement.X = FMath::Min(MinMaxMaterialDisplacement.X, MinDisplacement);
			MinMaxMaterialDisplacement.Y = FMath::Max(MinMaxMaterialDisplacement.Y, MaxDisplacement);

			bHasDynamicDisplacement = true;
		}
		else
		{
			MaterialSection.DisplacementScaling = FDisplacementScaling();
		}
	}
}

bool FSceneProxyBase::SupportsAlwaysVisible() const
{
#if WITH_EDITOR
	// Right now we never use the always visible optimization
	// in editor builds due to dynamic relevance, hit proxies, etc..
	return false;
#else
	if (Nanite::GetSupportsCustomDepthRendering() && ShouldRenderCustomDepth())
	{
		// Custom depth/stencil is not supported yet.
		return false;
	}

	if (GetLightingChannelMask() != GetDefaultLightingChannelMask())
	{
		// Lighting channels are not supported yet.
		return false;
	}

	static bool bAllowStaticLighting = FReadOnlyCVARCache::AllowStaticLighting();
	if (bAllowStaticLighting)
	{
		// Static lighting is not supported
		return false;
	}

	// Always visible
	return true;
#endif
}

FSceneProxy::FSceneProxy(const FMaterialAudit& MaterialAudit, const FStaticMeshSceneProxyDesc& ProxyDesc, bool InbIsInstancedMesh)
: FSceneProxyBase(ProxyDesc)
, MeshInfo(ProxyDesc)
, RenderData(ProxyDesc.GetStaticMesh()->GetRenderData())
, StaticMesh(ProxyDesc.GetStaticMesh())
#if NANITE_ENABLE_DEBUG_RENDERING
, Owner(ProxyDesc.GetOwner())
, LightMapResolution(ProxyDesc.GetStaticLightMapResolution())
, BodySetup(ProxyDesc.GetBodySetup())
, CollisionTraceFlag(ECollisionTraceFlag::CTF_UseSimpleAndComplex)
, CollisionResponse(ProxyDesc.GetCollisionResponseToChannels())
, ForcedLodModel(ProxyDesc.ForcedLodModel)
, LODForCollision(ProxyDesc.GetStaticMesh()->LODForCollision)
, bDrawMeshCollisionIfComplex(ProxyDesc.bDrawMeshCollisionIfComplex)
, bDrawMeshCollisionIfSimple(ProxyDesc.bDrawMeshCollisionIfSimple)
#endif
{
	LLM_SCOPE_BYTAG(Nanite);

	Resources = ProxyDesc.GetNaniteResources();

	// This should always be valid.
	checkSlow(Resources && Resources->PageStreamingStates.Num() > 0);

	DistanceFieldSelfShadowBias = FMath::Max(ProxyDesc.bOverrideDistanceFieldSelfShadowBias ? ProxyDesc.DistanceFieldSelfShadowBias : ProxyDesc.GetStaticMesh()->DistanceFieldSelfShadowBias, 0.0f);

	// Use fast path that does not update static draw lists.
	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	// Nanite always uses GPUScene, so we can skip expensive primitive uniform buffer updates.
	bVFRequiresPrimitiveUniformBuffer = false;

	// Indicates if 1 or more materials contain settings not supported by Nanite.
	bHasMaterialErrors = false;

	InstanceWPODisableDistance = ProxyDesc.WorldPositionOffsetDisableDistance;

	SetWireframeColor(ProxyDesc.GetWireframeColor());

	const bool bHasSurfaceStaticLighting = MeshInfo.GetLightMap() != nullptr || MeshInfo.GetShadowMap() != nullptr;

	const uint32 FirstLODIndex = 0; // Only data from LOD0 is used.
	const FStaticMeshLODResources& MeshResources = RenderData->LODResources[FirstLODIndex];
	const FStaticMeshSectionArray& MeshSections = MeshResources.Sections;

	// Copy the pointer to the volume data, async building of the data may modify the one on FStaticMeshLODResources while we are rendering
	DistanceFieldData = MeshResources.DistanceFieldData;
	CardRepresentationData = MeshResources.CardRepresentationData;

	bEvaluateWorldPositionOffset = ProxyDesc.bEvaluateWorldPositionOffset;
	
	MaterialSections.SetNumZeroed(MeshSections.Num());

	const bool bIsInstancedMesh = InbIsInstancedMesh;

	NaniteMaterialMask = FUint32Vector2(0u, 0u);

	for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& MeshSection = MeshSections[SectionIndex];
		FMaterialSection& MaterialSection = MaterialSections[SectionIndex];
		MaterialSection.MaterialIndex = MeshSection.MaterialIndex;
		MaterialSection.bHidden = false;
	#if WITH_EDITORONLY_DATA
		MaterialSection.bSelected = false;
		if (GIsEditor)
		{
			if (ProxyDesc.SelectedEditorMaterial != INDEX_NONE)
			{
				MaterialSection.bSelected = (ProxyDesc.SelectedEditorMaterial == MaterialSection.MaterialIndex);
			}
			else if (ProxyDesc.SelectedEditorSection != INDEX_NONE)
			{
				MaterialSection.bSelected = (ProxyDesc.SelectedEditorSection == SectionIndex);
			}

			// If material is hidden, then skip the raster
			if ((ProxyDesc.MaterialIndexPreview != INDEX_NONE) && (ProxyDesc.MaterialIndexPreview != MaterialSection.MaterialIndex))
			{
				MaterialSection.bHidden = true;
			}

			// If section is hidden, then skip the raster
			if ((ProxyDesc.SectionIndexPreview != INDEX_NONE) && (ProxyDesc.SectionIndexPreview != SectionIndex))
			{
				MaterialSection.bHidden = true;
			}
		}
	#endif

		// Keep track of highest observed material index.
		MaterialMaxIndex = FMath::Max(MaterialSection.MaterialIndex, MaterialMaxIndex);

		UMaterialInterface* ShadingMaterial = nullptr;
		if (!MaterialSection.bHidden)
		{
			// Mark the material mask
			if (MaterialSection.MaterialIndex >= 32u)
			{
				NaniteMaterialMask.Y |= (1u << (MaterialSection.MaterialIndex - 32u));
			}
			else
			{
				NaniteMaterialMask.X |= (1u << MaterialSection.MaterialIndex);
			}

			// Get the shading material
			ShadingMaterial = MaterialAudit.GetMaterial(MaterialSection.MaterialIndex);

			MaterialSection.LocalUVDensities = MaterialAudit.GetLocalUVDensities(MaterialSection.MaterialIndex);

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
		}

		if (ShadingMaterial == nullptr || ProxyDesc.ShouldRenderProxyFallbackToDefaultMaterial())
		{
			ShadingMaterial = MaterialSection.bHidden ? GEngine->NaniteHiddenSectionMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface);
		}

		MaterialSection.ShadingMaterialProxy = ShadingMaterial->GetRenderProxy();
	}

	// Now that the material sections are initialized, we can make material-dependent calculations
	OnMaterialsUpdated();

	// Nanite supports distance field representation for fully opaque meshes.
	bSupportsDistanceFieldRepresentation = CombinedMaterialRelevance.bOpaque && DistanceFieldData && DistanceFieldData->IsValid();;

	// Find the first LOD with any vertices (ie that haven't been stripped)
	int32 FirstAvailableLOD = 0;
	for (; FirstAvailableLOD < RenderData->LODResources.Num(); FirstAvailableLOD++)
	{
		if (RenderData->LODResources[FirstAvailableLOD].GetNumVertices() > 0)
		{
			break;
		}
	}

	const int32 SMCurrentMinLOD = ProxyDesc.GetStaticMesh()->GetMinLODIdx();
	int32 EffectiveMinLOD = ProxyDesc.bOverrideMinLOD ? ProxyDesc.MinLOD : SMCurrentMinLOD;
	ClampedMinLOD = FMath::Clamp(EffectiveMinLOD, FirstAvailableLOD, RenderData->LODResources.Num() - 1);

#if RHI_RAYTRACING
	if (IsRayTracingAllowed() && ProxyDesc.GetStaticMesh()->bSupportRayTracing && RenderData->LODResources[ClampedMinLOD].GetNumVertices())
	{
		bHasRayTracingInstances = true;

		CoarseMeshStreamingHandle = (Nanite::CoarseMeshStreamingHandle)ProxyDesc.GetStaticMesh()->GetStreamingIndex();
	}
#endif

#if RHI_RAYTRACING || NANITE_ENABLE_DEBUG_RENDERING
	bool bInitializeFallBackLODs = false;
#	if RHI_RAYTRACING
		bInitializeFallBackLODs |= bHasRayTracingInstances;
#	endif
#	if NANITE_ENABLE_DEBUG_RENDERING
		bInitializeFallBackLODs |= true;
#	endif

	if (bInitializeFallBackLODs)
	{
		// Pre-allocate FallbackLODs. Dynamic resize is unsafe as the FFallbackLODInfo constructor queues up a rendering command with a reference to itself.
		FallbackLODs.SetNumUninitialized(RenderData->LODResources.Num());

		for (int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++)
		{
			FFallbackLODInfo* NewLODInfo = new (&FallbackLODs[LODIndex]) FFallbackLODInfo(&ProxyDesc, RenderData->LODVertexFactories, LODIndex, ClampedMinLOD);
		}
	}
#endif

#if NANITE_ENABLE_DEBUG_RENDERING
	if (BodySetup)
	{
		CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
	}
#endif

	FilterFlags = EFilterFlags::StaticMesh;
	FilterFlags |= ProxyDesc.Mobility == EComponentMobility::Static ? EFilterFlags::StaticMobility : EFilterFlags::NonStaticMobility;

	bReverseCulling = ProxyDesc.bReverseCulling;

	bOpaqueOrMasked = true; // Nanite only supports opaque
	UpdateVisibleInLumenScene();
}

FSceneProxy::FSceneProxy(const FMaterialAudit& MaterialAudit, const FInstancedStaticMeshSceneProxyDesc& InProxyDesc)
	: FSceneProxy(MaterialAudit, InProxyDesc, true)
{
	LLM_SCOPE_BYTAG(Nanite);

	// Nanite meshes do not deform internally
	bHasDeformableMesh = false;

	// Nanite supports the GPUScene instance data buffer.
	InstanceDataSceneProxy = InProxyDesc.InstanceDataSceneProxy;
	SetupInstanceSceneDataBuffers(InstanceDataSceneProxy->GeInstanceSceneDataBuffers());

#if WITH_EDITOR
	const bool bSupportInstancePicking = HasPerInstanceHitProxies() && SMInstanceElementDataUtil::SMInstanceElementsEnabled();
	HitProxyMode = bSupportInstancePicking ? EHitProxyMode::PerInstance : EHitProxyMode::MaterialSection;

	if (HitProxyMode == EHitProxyMode::PerInstance)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < InProxyDesc.SelectedInstances.Num() && !bHasSelectedInstances; ++InstanceIndex)
		{
			bHasSelectedInstances |= InProxyDesc.SelectedInstances[InstanceIndex];
		}

		if (bHasSelectedInstances)
		{
			// If we have selected indices, mark scene proxy as selected.
			SetSelection_GameThread(true);
		}
	}
#endif

	EndCullDistance = InProxyDesc.InstanceEndCullDistance;

	FilterFlags = EFilterFlags::InstancedStaticMesh;
	FilterFlags |= InProxyDesc.Mobility == EComponentMobility::Static ? EFilterFlags::StaticMobility : EFilterFlags::NonStaticMobility;
}

FSceneProxy::FSceneProxy(const FMaterialAudit& MaterialAudit, UStaticMeshComponent* Component)
	: FSceneProxy(MaterialAudit, FStaticMeshSceneProxyDesc(Component))
{
}

FSceneProxy::FSceneProxy(const FMaterialAudit& MaterialAudit, UInstancedStaticMeshComponent* Component)
	: FSceneProxy(MaterialAudit, FInstancedStaticMeshSceneProxyDesc(Component))
{
}

FSceneProxy::FSceneProxy(const FMaterialAudit& MaterialAudit, UHierarchicalInstancedStaticMeshComponent* Component)
: FSceneProxy(MaterialAudit, static_cast<UInstancedStaticMeshComponent*>(Component))
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
	FilterFlags |= Component->Mobility == EComponentMobility::Static ? EFilterFlags::StaticMobility : EFilterFlags::NonStaticMobility;
}

FSceneProxy::~FSceneProxy()
{
#if RHI_RAYTRACING
	ReleaseDynamicRayTracingGeometries();
#endif
}

void FSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	check(Resources->RuntimeResourceID != INDEX_NONE && Resources->HierarchyOffset != INDEX_NONE);

#if RHI_RAYTRACING
	if (IsRayTracingAllowed())
	{
		// copy RayTracingGeometryGroupHandle from FStaticMeshRenderData since UStaticMesh can be released before the proxy is destroyed
		RayTracingGeometryGroupHandle = RenderData->RayTracingGeometryGroupHandle;
	}

	if (IsRayTracingAllowed() && bNeedsDynamicRayTracingGeometries)
	{
		CreateDynamicRayTracingGeometries(RHICmdList);
	}
#endif
}

void FSceneProxy::OnEvaluateWorldPositionOffsetChanged_RenderThread()
{
	bHasProgrammableRaster = false;
	for (FMaterialSection& MaterialSection : MaterialSections)
	{
		if (MaterialSection.IsProgrammableRaster(bEvaluateWorldPositionOffset))
		{
			MaterialSection.RasterMaterialProxy = MaterialSection.ShadingMaterialProxy;
			bHasProgrammableRaster = true;
		}
		else
		{
			MaterialSection.ResetToDefaultMaterial(false, true);
		}
	}

	GetRendererModule().RequestStaticMeshUpdate(GetPrimitiveSceneInfo());
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
	const bool bOptimizedRelevance = true;
#endif

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.NaniteMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bRenderCustomDepth = Nanite::GetSupportsCustomDepthRendering() && ShouldRenderCustomDepth();
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
		bSetDynamicRelevance |= (
			// Nanite doesn't respect rich view enabling dynamic relevancy.
			//IsRichView(*View->Family) ||
			View->Family->EngineShowFlags.Collision ||
			bInCollisionView ||
			View->Family->EngineShowFlags.Bounds ||
			View->Family->EngineShowFlags.VisualizeInstanceUpdates
		);
	#endif
	#if WITH_EDITOR
		// Nanite doesn't render debug vertex colors.
		//bSetDynamicRelevance |= (IsSelected() && View->Family->EngineShowFlags.VertexColors);
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

FORCENOINLINE HHitProxy* FSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	return FSceneProxy::CreateHitProxies(Component->GetPrimitiveComponentInterface(), OutHitProxies);
}

FORCENOINLINE HHitProxy* FSceneProxy::CreateHitProxies(IPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
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
					HHitProxy* ActorHitProxy = Component->CreateMeshHitProxy(SectionIndex, SectionIndex);

					if (ActorHitProxy)
					{
						check(!Section.HitProxy);
						Section.HitProxy = ActorHitProxy;
						OutHitProxies.Add(ActorHitProxy);
					}
				}
			}
			break;
		}

		case FSceneProxyBase::EHitProxyMode::PerInstance:
		{
			// Note: the instance data proxy handles the hitproxy lifetimes internally as the update cadence does not match FPrimitiveSceneInfo ctor cadence
			break;
		}

		default:
			break;
	}

	return Super::CreateHitProxies(Component, OutHitProxies);
}

#endif

FSceneProxy::FMeshInfo::FMeshInfo(const FStaticMeshSceneProxyDesc& InProxyDesc)
{
	LLM_SCOPE_BYTAG(Nanite);

	// StaticLighting only supported by UStaticMeshComponents & derived classes for the moment
	const UStaticMeshComponent* Component =  InProxyDesc.GetUStaticMeshComponent();
	if (!Component)
	{
		return;
	}

	if (Component->LightmapType == ELightmapType::ForceVolumetric)
	{
		SetGlobalVolumeLightmap(true);
	}
#if WITH_EDITOR
	else if (Component && FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(Component, 0))
	{
		const FMeshMapBuildData* MeshMapBuildData = FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(Component, 0);
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
	else if (InProxyDesc.LODData.Num() > 0)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InProxyDesc.LODData[0];

		const FMeshMapBuildData* MeshMapBuildData = Component->GetMeshMapBuildData(ComponentLODInfo);
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

#if RHI_RAYTRACING || NANITE_ENABLE_DEBUG_RENDERING

// Loosely copied from FStaticMeshSceneProxy::FLODInfo::FLODInfo and modified for Nanite fallback
// TODO: Refactor all this to share common code with Nanite and regular SM scene proxy
FSceneProxy::FFallbackLODInfo::FFallbackLODInfo(
	const FStaticMeshSceneProxyDesc* InProxyDesc,
	const FStaticMeshVertexFactoriesArray& InLODVertexFactories,
	int32 LODIndex,
	int32 InClampedMinLOD
)
{
	const auto FeatureLevel = InProxyDesc->GetScene()->GetFeatureLevel();

	FStaticMeshRenderData* MeshRenderData = InProxyDesc->GetStaticMesh()->GetRenderData();
	FStaticMeshLODResources& LODModel = MeshRenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = InLODVertexFactories[LODIndex];

	if (LODIndex < InProxyDesc->LODData.Num() && LODIndex >= InClampedMinLOD)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InProxyDesc->LODData[LODIndex];

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
					FString ComponentPathName = InProxyDesc->GetPathName();
					checkf(LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices() > 0, TEXT("LOD: %i of PathName: %s has an empty position stream."), LODIndex, *ComponentPathName);

					ENQUEUE_RENDER_COMMAND(FLocalVertexFactoryCopyData)(
						[UniformBufferPtr, LocalVF, LODIndex, VertexBuffer, ComponentPathName] (FRHICommandListBase&)
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
		UMaterialInterface* Material = InProxyDesc->GetMaterial(Section.MaterialIndex);
#if WITH_EDITORONLY_DATA
		SectionInfo.MaterialIndex = Section.MaterialIndex;
#endif

		if (Material == nullptr)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		SectionInfo.MaterialProxy = Material->GetRenderProxy();

		// Per-section selection for the editor.
#if WITH_EDITORONLY_DATA
		if (GIsEditor)
		{
			if (InProxyDesc->SelectedEditorMaterial >= 0)
			{
				SectionInfo.bSelected = (InProxyDesc->SelectedEditorMaterial == Section.MaterialIndex);
			}
			else
			{
				SectionInfo.bSelected = (InProxyDesc->SelectedEditorSection == SectionIndex);
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
	// Nanite only has dynamic relevance in the editor for certain debug modes
#if WITH_EDITOR
	LLM_SCOPE_BYTAG(Nanite);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NaniteSceneProxy_GetMeshElements);

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

#if STATICMESH_ENABLE_DEBUG_RENDERING
#if WITH_EDITORONLY_DATA
	FLinearColor NewVertexMaterialColor = FLinearColor::White;
	// Override the mesh's material with our material that draws the vertex colors
	switch (GVertexColorViewMode)
	{
	case EVertexColorViewMode::Color:
		NewVertexMaterialColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
		break;

	case EVertexColorViewMode::Alpha:
		NewVertexMaterialColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		break;

	case EVertexColorViewMode::Red:
		NewVertexMaterialColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
		break;

	case EVertexColorViewMode::Green:
		NewVertexMaterialColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
		break;

	case EVertexColorViewMode::Blue:
		NewVertexMaterialColor = FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
		break;
	}
	FColoredTexturedMaterialRenderProxy* NewVertexColorVisualizationMaterialInstance = new FColoredTexturedMaterialRenderProxy(
		GEngine->TexturePaintingMaskMaterial->GetRenderProxy(),
		NewVertexMaterialColor,
		NAME_Color,
		GVertexViewModeOverrideTexture.Get(),
		NAME_LinearColor);

	NewVertexColorVisualizationMaterialInstance->UVChannel = GVertexViewModeOverrideUVChannel;
	NewVertexColorVisualizationMaterialInstance->UVChannelParamName = FName(TEXT("UVChannel"));

	Collector.RegisterOneFrameMaterialProxy(NewVertexColorVisualizationMaterialInstance);
#endif
#endif // STATICMESH_ENABLE_DEBUG_RENDERING

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

			const FInstanceSceneDataBuffers *InstanceSceneDataBuffers = GetInstanceSceneDataBuffers();
			// Note: this will return 1 for the non-instanced case.
			const int32 InstanceCount = InstanceSceneDataBuffers ? InstanceSceneDataBuffers->GetNumInstances() : 1;

			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++)
			{
				FMatrix InstanceToWorld = InstanceSceneDataBuffers ? InstanceSceneDataBuffers->GetInstanceToWorld(InstanceIndex) : GetLocalToWorld();

				if ((bDrawSimpleCollision || bDrawSimpleWireframeCollision) && BodySetup)
				{
					if (FMath::Abs(InstanceToWorld.Determinant()) < UE_SMALL_NUMBER)
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
							FTransform GeomTransform(InstanceToWorld);
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetWireframeColor().ToFColor(true), SimpleCollisionMaterialInstance, false, true, AlwaysHasVelocity(), ViewIndex, Collector);
						}
						// wireframe
						else
						{
							FTransform GeomTransform(InstanceToWorld);
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(SimpleCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), nullptr, (Owner == nullptr), false, AlwaysHasVelocity(), ViewIndex, Collector);
						}

						// The simple nav geometry is only used by dynamic obstacles for now
						if (StaticMesh->GetNavCollision() && StaticMesh->GetNavCollision()->IsDynamicObstacle())
						{
							// Draw the static mesh's body setup (simple collision)
							FTransform GeomTransform(InstanceToWorld);
							FColor NavCollisionColor = FColor(118, 84, 255, 255);
							StaticMesh->GetNavCollision()->DrawSimpleGeom(Collector.GetPDI(ViewIndex), GeomTransform, GetSelectionColor(NavCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true));
						}
					}
				}

#if STATICMESH_ENABLE_DEBUG_RENDERING
#if WITH_EDITORONLY_DATA
				// Only render for texture painting; vertex painting is not supported for Nanite meshes
				if (bProxyIsSelected && EngineShowFlags.VertexColors && AllowDebugViewmodes() && GVertexViewModeOverrideTexture.IsValid() && ShouldProxyUseVertexColorVisualization(GetOwnerName()))
				{
					FTransform GeomTransform(InstanceToWorld);
					BodySetup->AggGeom.GetAggGeom(GeomTransform, NewVertexMaterialColor.ToFColor(false), NewVertexColorVisualizationMaterialInstance, false, true, DrawsVelocity(), ViewIndex, Collector);
				}
#endif
#endif // STATICMESH_ENABLE_DEBUG_RENDERING

				if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
				{
					DebugMassData[0].DrawDebugMass(Collector.GetPDI(ViewIndex), FTransform(InstanceToWorld));
				}

				if (EngineShowFlags.StaticMeshes)
				{
					RenderBounds(Collector.GetPDI(ViewIndex), EngineShowFlags, GetBounds(), !Owner || IsSelected());
				}
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (EngineShowFlags.VisualizeInstanceUpdates && InstanceDataSceneProxy)
			{
				InstanceDataSceneProxy->DebugDrawInstanceChanges(Collector.GetPDI(ViewIndex), EngineShowFlags.Game ? SDPG_World : SDPG_Foreground);
			}
#endif
		}
	}
#endif // NANITE_ENABLE_DEBUG_RENDERING
#endif // WITH_EDITOR
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

void FSceneProxy::SetWorldPositionOffsetDisableDistance_GameThread(int32 NewValue)
{
	ENQUEUE_RENDER_COMMAND(CmdSetWPODisableDistance)(
		[this, NewValue](FRHICommandList&)
		{
			const bool bUpdatePrimitiveData = InstanceWPODisableDistance != NewValue;
			const bool bUpdateDrawCmds = bUpdatePrimitiveData && (InstanceWPODisableDistance == 0 || NewValue == 0);

			if (bUpdatePrimitiveData)
			{
				InstanceWPODisableDistance = NewValue;
				GetScene().RequestUniformBufferUpdate(*GetPrimitiveSceneInfo());
				GetScene().RequestGPUSceneUpdate(*GetPrimitiveSceneInfo(), EPrimitiveDirtyState::ChangedOther);
				if (bUpdateDrawCmds)
				{
					GetRendererModule().RequestStaticMeshUpdate(GetPrimitiveSceneInfo());
				}
			}
		});
}

void FSceneProxy::SetInstanceCullDistance_RenderThread(float InStartCullDistance, float InEndCullDistance)
{
	EndCullDistance = InEndCullDistance;
}

FInstanceDataUpdateTaskInfo *FSceneProxy::GetInstanceDataUpdateTaskInfo() const
{
	return InstanceDataSceneProxy ? InstanceDataSceneProxy->GetUpdateTaskInfo() : nullptr;
}

#if RHI_RAYTRACING
bool FSceneProxy::HasRayTracingRepresentation() const
{
	return bHasRayTracingInstances;
}

int32 FSceneProxy::GetFirstValidRaytracingGeometryLODIndex() const
{
	if (GetRayTracingMode() != ERayTracingMode::Fallback)
	{
		// NaniteRayTracing always uses LOD0
		return 0;
	}

	int32 NumLODs = RenderData->LODResources.Num();
	int LODIndex = ClampedMinLOD;

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
		const FRayTracingGeometry& RayTracingGeometry = RenderData->LODResources[LODIndex].RayTracingGeometry;
		if (RayTracingGeometry.IsValid() && !RayTracingGeometry.HasPendingBuildRequest())
		{
			return LODIndex;
		}
	}

	return INDEX_NONE;
}

void FSceneProxy::SetupRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& OutMaterials) const
{
	OutMaterials.SetNum(MaterialSections.Num());

	for (int32 SectionIndex = 0; SectionIndex < OutMaterials.Num(); ++SectionIndex)
	{
		const FMaterialSection& MaterialSection = MaterialSections[SectionIndex];

		const bool bWireframe = false;
		const bool bUseReversedIndices = false;

		FMeshBatch& MeshBatch = OutMaterials[SectionIndex];
		FMeshBatchElement& MeshBatchElement = MeshBatch.Elements[0];

		MeshBatch.VertexFactory = GVertexFactoryResource.GetVertexFactory();
		MeshBatch.MaterialRenderProxy = MaterialSection.ShadingMaterialProxy;
		MeshBatch.bWireframe = bWireframe;
		MeshBatch.SegmentIndex = SectionIndex;
		MeshBatch.LODIndex = 0;
		MeshBatch.CastRayTracedShadow = CastsDynamicShadow(); // Relying on BuildInstanceMaskAndFlags(...) to check Material.CastsRayTracedShadows()

		MeshBatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}
}

void FSceneProxy::SetupFallbackRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& OutMaterials) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];

	const FFallbackLODInfo& FallbackLODInfo = FallbackLODs[LODIndex];

	OutMaterials.SetNum(FallbackLODInfo.Sections.Num());

	for (int32 SectionIndex = 0; SectionIndex < OutMaterials.Num(); ++SectionIndex)
	{
		const FFallbackLODInfo::FSectionInfo& SectionInfo = FallbackLODInfo.Sections[SectionIndex];

		FMeshBatch& MeshBatch = OutMaterials[SectionIndex];
		FMeshBatchElement& MeshBatchElement = MeshBatch.Elements[0];

		const bool bWireframe = false;
		const bool bUseReversedIndices = false;

		SetMeshElementGeometrySource(LODIndex, SectionIndex, bWireframe, bUseReversedIndices, &VFs.VertexFactory, MeshBatch);

		MeshBatch.VertexFactory = &VFs.VertexFactory;
		MeshBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();

		const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

		MeshBatchElement.MinVertexIndex = Section.MinVertexIndex;
		MeshBatchElement.MaxVertexIndex = Section.MaxVertexIndex;

		MeshBatch.MaterialRenderProxy = SectionInfo.MaterialProxy;
		MeshBatch.bWireframe = bWireframe;
		MeshBatch.SegmentIndex = SectionIndex;
		MeshBatch.LODIndex = 0; // CacheRayTracingPrimitive(...) currently assumes that primitives with CacheInstances flag only cache mesh commands for one LOD
		MeshBatch.CastRayTracedShadow = CastsDynamicShadow(); // Relying on BuildInstanceMaskAndFlags(...) to check Material.CastsRayTracedShadows()
		MeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);

		MeshBatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}
}

void FSceneProxy::CreateDynamicRayTracingGeometries(FRHICommandListBase& RHICmdList)
{
	check(bNeedsDynamicRayTracingGeometries);
	check(DynamicRayTracingGeometries.IsEmpty());

	DynamicRayTracingGeometries.AddDefaulted(RenderData->LODResources.Num());

	for (int32 LODIndex = ClampedMinLOD; LODIndex < RenderData->LODResources.Num(); LODIndex++)
	{
		FRayTracingGeometryInitializer Initializer = RenderData->LODResources[LODIndex].RayTracingGeometry.Initializer;
		for (FRayTracingGeometrySegment& Segment : Initializer.Segments)
		{
			Segment.VertexBuffer = nullptr;
		}
		Initializer.bAllowUpdate = true;
		Initializer.bFastBuild = true;
		Initializer.Type = ERayTracingGeometryInitializerType::Rendering;

		DynamicRayTracingGeometries[LODIndex].SetInitializer(MoveTemp(Initializer));
		DynamicRayTracingGeometries[LODIndex].InitResource(RHICmdList);
	}
}

void FSceneProxy::ReleaseDynamicRayTracingGeometries()
{
	for (auto& Geometry : DynamicRayTracingGeometries)
	{
		Geometry.ReleaseResource();
	}

	DynamicRayTracingGeometries.Empty();
}

void FSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	check(!IsRayTracingStaticRelevant());

	if (CVarRayTracingNaniteProxyMeshes.GetValueOnRenderThread() == 0 || !bHasRayTracingInstances)
	{
		return;
	}

	if (GetRayTracingMode() != ERayTracingMode::Fallback)
	{
		// We don't currently support non-fallback dynamic instances
		return;
	}

	// try and find the first valid RT geometry index
	int32 ValidLODIndex = GetFirstValidRaytracingGeometryLODIndex();
	if (ValidLODIndex == INDEX_NONE)
	{
		return;
	}

	if (!ensure(DynamicRayTracingGeometries.IsValidIndex(ValidLODIndex)))
	{
		return;
	}

	const FStaticMeshLODResources& LODData = RenderData->LODResources[ValidLODIndex];
	FRayTracingGeometry* DynamicGeometry = &DynamicRayTracingGeometries[ValidLODIndex];

	// Setup a new instance
	FRayTracingInstance& RayTracingInstance = OutRayTracingInstances.Emplace_GetRef();
	RayTracingInstance.Geometry = DynamicGeometry;

	const FInstanceSceneDataBuffers* InstanceSceneDataBuffers = GetInstanceSceneDataBuffers();
	const int32 InstanceCount = InstanceSceneDataBuffers ? InstanceSceneDataBuffers->GetNumInstances() : 1;
	
	// NOTE: For now, only single-instance dynamic ray tracing is supported
	checkf(
		InstanceCount == 1,
		TEXT("GetDynamicRayTracingInstances called for a Nanite scene proxy with multiple instances. ")
		TEXT("This isn't currently supported.")
	);
	RayTracingInstance.InstanceTransformsView = MakeArrayView(&GetLocalToWorld(), 1);
	RayTracingInstance.NumTransforms = 1;

	const int32 NumRayTracingMaterialEntries = RenderData->LODResources[ValidLODIndex].Sections.Num();

	// Setup the cached materials again when the LOD changes
	if (NumRayTracingMaterialEntries != CachedRayTracingMaterials.Num() || ValidLODIndex != CachedRayTracingMaterialsLODIndex)
	{
		CachedRayTracingMaterials.Reset();

		SetupFallbackRayTracingMaterials(ValidLODIndex, CachedRayTracingMaterials);
		CachedRayTracingMaterialsLODIndex = ValidLODIndex;
	}
	else
	{
		// Skip computing the mask and flags in the renderer since material didn't change
		RayTracingInstance.bInstanceMaskAndFlagsDirty = false;
	}

	RayTracingInstance.MaterialsView = CachedRayTracingMaterials;

	// Use the shared vertex buffer - needs to be updated every frame
	FRWBuffer* VertexBuffer = nullptr;

	Context.DynamicRayTracingGeometriesToUpdate.Add(
		FRayTracingDynamicGeometryUpdateParams
		{
			CachedRayTracingMaterials,
			false,
			(uint32)LODData.GetNumVertices(),
			(uint32)LODData.GetNumVertices() * (uint32)sizeof(FVector3f),
			DynamicGeometry->Initializer.TotalPrimitiveCount,
			DynamicGeometry,
			VertexBuffer,
			true
		}
	);
}

ERayTracingPrimitiveFlags FSceneProxy::GetCachedRayTracingInstance(FRayTracingInstance& RayTracingInstance)
{
	if (!(IsVisibleInRayTracing() && ShouldRenderInMainPass() && (IsDrawnInGame() || AffectsIndirectLightingWhileHidden()|| CastsHiddenShadow())) && !IsRayTracingFarField())
	{
		return ERayTracingPrimitiveFlags::Exclude;
	}

	if (CVarRayTracingNaniteProxyMeshes.GetValueOnRenderThread() == 0 || !bHasRayTracingInstances)
	{
		return ERayTracingPrimitiveFlags::Exclude;
	}

	static const auto RayTracingStaticMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.StaticMeshes"));

	if (RayTracingStaticMeshesCVar && RayTracingStaticMeshesCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Exclude;
	}

	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));

	if (bIsHierarchicalInstancedStaticMesh && RayTracingHISMCVar && RayTracingHISMCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Exclude;
	}

	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	if (bIsLandscapeGrass && RayTracingLandscapeGrassCVar && RayTracingLandscapeGrassCVar->GetValueOnRenderThread() <= 0)
	{
		return ERayTracingPrimitiveFlags::Exclude;
	}

	const bool bUsingNaniteRayTracing = GetRayTracingMode() != ERayTracingMode::Fallback;

	// try and find the first valid RT geometry index
	int32 ValidLODIndex = GetFirstValidRaytracingGeometryLODIndex();
	if (ValidLODIndex == INDEX_NONE)
	{
		// Use Skip flag here since Excluded primitives don't get cached ray tracing state updated even if it's marked dirty.
		// ERayTracingPrimitiveFlags::Exclude should only be used for conditions that will cause proxy to be recreated when they change.
		ERayTracingPrimitiveFlags ResultFlags = ERayTracingPrimitiveFlags::Skip;

		if (CoarseMeshStreamingHandle != INDEX_NONE)
		{
			// If there is a streaming handle (but no valid LOD available), then give the streaming flag to make sure it's not excluded
			// It's still needs to be processed during TLAS build because this will drive the streaming of these resources.
			ResultFlags |= ERayTracingPrimitiveFlags::Streaming;
		}

		return ResultFlags;
	}

	if (bUsingNaniteRayTracing)
	{
		RayTracingInstance.Geometry = nullptr;
		RayTracingInstance.bApplyLocalBoundsTransform = false;
	}
	else
	{
		RayTracingInstance.Geometry = &RenderData->LODResources[ValidLODIndex].RayTracingGeometry;
		RayTracingInstance.bApplyLocalBoundsTransform = false;
	}

	//checkf(SupportsInstanceDataBuffer() && InstanceSceneData.Num() <= GetPrimitiveSceneInfo()->GetNumInstanceSceneDataEntries(),
	//	TEXT("Primitives using ERayTracingPrimitiveFlags::CacheInstances require instance transforms available in GPUScene"));

	RayTracingInstance.NumTransforms = GetPrimitiveSceneInfo()->GetNumInstanceSceneDataEntries();
	// When ERayTracingPrimitiveFlags::CacheInstances is used, instance transforms are copied from GPUScene while building ray tracing instance buffer.

	if (bUsingNaniteRayTracing)
	{
		SetupRayTracingMaterials(ValidLODIndex, RayTracingInstance.Materials);
	}
	else
	{
		SetupFallbackRayTracingMaterials(ValidLODIndex, RayTracingInstance.Materials);
	}

	const bool bIsRayTracingFarField = IsRayTracingFarField();

	RayTracingInstance.InstanceLayer = bIsRayTracingFarField ? ERayTracingInstanceLayer::FarField : ERayTracingInstanceLayer::NearField;

	// setup the flags
	ERayTracingPrimitiveFlags ResultFlags = ERayTracingPrimitiveFlags::CacheInstances;

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

RayTracing::GeometryGroupHandle FSceneProxy::GetRayTracingGeometryGroupHandle() const
{
	check(IsInRenderingThread());
	return RayTracingGeometryGroupHandle;
}

#endif // RHI_RAYTRACING

#if RHI_RAYTRACING || NANITE_ENABLE_DEBUG_RENDERING

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
	// Use != to ensure consistent face directions between negatively and positively scaled primitives
	// NOTE: This is only used by ray tracing and debug draw mesh elements
	// (Nanite determines cull mode on the GPU. See ReverseWindingOrder() in NaniteRasterizer.usf)
	const bool bReverseNeeded = IsCullingReversedByComponent() != IsLocalToWorldDeterminantNegative();
	return bReverseNeeded && !bUseReversedIndices;
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

	OutInfo.NumResidentClusters = Resources->NumResidentClusters;

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

struct FAuditMaterialSlotInfo
{
	UMaterialInterface* Material;
	FName SlotName;
	FMeshUVChannelInfo UVChannelData;
};

template<class T>
TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> GetMaterialSlotInfos(const T& Object)
{
	TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> Infos;

	if (UStaticMesh* StaticMesh = Object.GetStaticMesh())
	{
		TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

		uint32 Index = 0;
		for (FStaticMaterial& Material : StaticMaterials)
		{
			Infos.Add({Object.GetNaniteAuditMaterial(Index), Material.MaterialSlotName, Material.UVChannelData});
			Index++;
		}
	}

	return Infos;
}

template<class T> 
FMaterialAudit& AuditMaterialsImp(const T* InProxyDesc, FMaterialAudit& Audit, bool bSetMaterialUsage)
{
	static const auto NaniteForceEnableMeshesCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Nanite.ForceEnableMeshes"));
	static const bool bNaniteForceEnableMeshes = NaniteForceEnableMeshesCvar && NaniteForceEnableMeshesCvar->GetValueOnAnyThread() != 0;

	Audit.bHasAnyError = false;
	Audit.Entries.Reset();	

	if (InProxyDesc != nullptr)
	{
		TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> Slots = Nanite::GetMaterialSlotInfos(*InProxyDesc);		

		uint32 Index = 0;
		for (const FAuditMaterialSlotInfo& SlotInfo : Slots)
		{
			FMaterialAuditEntry& Entry = Audit.Entries.AddDefaulted_GetRef();
			Entry.MaterialSlotName = SlotInfo.SlotName;
			Entry.MaterialIndex = Index;
			Index++;
			Entry.Material = SlotInfo.Material;
			Entry.bHasNullMaterial = Entry.Material == nullptr;
			Entry.LocalUVDensities = FVector4f(
				SlotInfo.UVChannelData.LocalUVDensities[0],
				SlotInfo.UVChannelData.LocalUVDensities[1],
				SlotInfo.UVChannelData.LocalUVDensities[2],
				SlotInfo.UVChannelData.LocalUVDensities[3]
			);

			if (Entry.bHasNullMaterial)
			{
				// Never allow null materials, assign default instead
				Entry.Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			const UMaterial* Material = Entry.Material->GetMaterial_Concurrent();
			check(Material != nullptr); // Should always be valid here

			const EBlendMode BlendMode = Entry.Material->GetBlendMode();

			bool bUsingCookedEditorData = false;
#if WITH_EDITORONLY_DATA
			bUsingCookedEditorData = Material->GetOutermost()->bIsCookedForEditor;
#endif
			bool bUsageSetSuccessfully = false;

			const FMaterialCachedExpressionData& CachedMaterialData = Material->GetCachedExpressionData();
			Entry.bHasVertexInterpolator		= CachedMaterialData.bHasVertexInterpolator;
			Entry.bHasPerInstanceRandomID		= CachedMaterialData.bHasPerInstanceRandom;
			Entry.bHasPerInstanceCustomData		= CachedMaterialData.bHasPerInstanceCustomData;
			Entry.bHasPixelDepthOffset			= Material->HasPixelDepthOffsetConnected();
			Entry.bHasWorldPositionOffset		= Material->HasVertexPositionOffsetConnected();
			Entry.bHasTessellationEnabled		= Material->IsTessellationEnabled();
			Entry.bHasUnsupportedBlendMode		= !IsSupportedBlendMode(BlendMode);
			Entry.bHasUnsupportedShadingModel	= !IsSupportedShadingModel(Material->GetShadingModels());
			Entry.bHasInvalidUsage				= (bUsingCookedEditorData || !bSetMaterialUsage) ? Material->NeedsSetMaterialUsage_Concurrent(bUsageSetSuccessfully, MATUSAGE_Nanite) : !Material->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);

			if (BlendMode == BLEND_Masked)
			{
				Audit.bHasMasked = true;
			}

			if (Material->bIsSky)
			{
				// Sky material is a special case we want to skip
				Audit.bHasSky = true;
			}

			Entry.bHasAnyError =
				Entry.bHasUnsupportedBlendMode |
				Entry.bHasUnsupportedShadingModel |
				Entry.bHasInvalidUsage;

			if (!bUsingCookedEditorData && Entry.bHasAnyError && !Audit.bHasAnyError)
			{
				// Only populate on error for performance/memory reasons
				Audit.AssetName = InProxyDesc->GetStaticMesh()->GetName();
				Audit.FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			Audit.bHasAnyError |= Entry.bHasAnyError;

#if !(UE_BUILD_SHIPPING) || WITH_EDITOR
			if (!bUsingCookedEditorData && !bNaniteForceEnableMeshes)
			{
				if (Entry.bHasUnsupportedBlendMode)
				{
					const FString BlendModeName = GetBlendModeString(Entry.Material->GetBlendMode());
					UE_LOG
					(
						LogStaticMesh, Warning,
						TEXT("Invalid material [%s] used on Nanite static mesh [%s]. Only opaque or masked blend modes are currently supported, [%s] blend mode was specified."),
						*Entry.Material->GetName(),
						*Audit.AssetName,
						*BlendModeName
					);
				}
				if (Entry.bHasUnsupportedShadingModel)
				{
					const FString ShadingModelString = GetShadingModelFieldString(Entry.Material->GetShadingModels());
					UE_LOG
					(
						LogStaticMesh, Warning,
						TEXT("Invalid material [%s] used on Nanite static mesh [%s]. The SingleLayerWater shading model is currently not supported, [%s] shading model was specified."),
						*Entry.Material->GetName(),
						*Audit.AssetName,
						*ShadingModelString
					);
				}
			}
#endif
		}
	}

	return Audit;
}


void AuditMaterials(const UStaticMeshComponent* Component, FMaterialAudit& Audit, bool bSetMaterialUsage)
{
	AuditMaterialsImp(Component, Audit, bSetMaterialUsage);
}

void AuditMaterials(const FStaticMeshSceneProxyDesc* ProxyDesc, FMaterialAudit& Audit, bool bSetMaterialUsage)
{
	AuditMaterialsImp(ProxyDesc, Audit, bSetMaterialUsage);
}

bool IsSupportedBlendMode(EBlendMode BlendMode)
{
	return IsOpaqueOrMaskedBlendMode(BlendMode);
}
bool IsSupportedBlendMode(const FMaterialShaderParameters& In)	{ return IsSupportedBlendMode(In.BlendMode); }
bool IsSupportedBlendMode(const FMaterial& In)					{ return IsSupportedBlendMode(In.GetBlendMode()); }
bool IsSupportedBlendMode(const UMaterialInterface& In)			{ return IsSupportedBlendMode(In.GetBlendMode()); }

bool IsSupportedMaterialDomain(EMaterialDomain Domain)
{
	return Domain == EMaterialDomain::MD_Surface;
}

bool IsSupportedShadingModel(FMaterialShadingModelField ShadingModelField)
{
	return !ShadingModelField.HasShadingModel(MSM_SingleLayerWater);
}

bool IsMaskingAllowed(UWorld* World, bool bForceNaniteForMasked)
{
	bool bAllowedByWorld = true;

	if (World)
	{
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			bAllowedByWorld = WorldSettings->NaniteSettings.bAllowMaskedMaterials;
		}
	}
	
	return (GNaniteAllowMaskedMaterials != 0) && (bAllowedByWorld || bForceNaniteForMasked);
}

void FVertexFactoryResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);

		if (NaniteLegacyMaterialsSupported())
		{
			VertexFactory = new FVertexFactory(ERHIFeatureLevel::SM5);
			VertexFactory->InitResource(RHICmdList);
		}

		if (NaniteComputeMaterialsSupported())
		{
			VertexFactory2 = new FNaniteVertexFactory(ERHIFeatureLevel::SM5);
			VertexFactory2->InitResource(RHICmdList);
		}
	}
}

void FVertexFactoryResource::ReleaseRHI()
{
	if (DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		LLM_SCOPE_BYTAG(Nanite);

		if (NaniteLegacyMaterialsSupported())
		{
			delete VertexFactory;
			VertexFactory = nullptr;
		}

		if (NaniteComputeMaterialsSupported())
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

void FNaniteVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	LLM_SCOPE_BYTAG(Nanite);
}

bool FNaniteVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	bool bShouldCompile =
		NaniteComputeMaterialsSupported() &&
		Parameters.ShaderType->GetFrequency() == SF_Compute &&
		(Parameters.MaterialParameters.bIsUsedWithNanite || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		Nanite::IsSupportedMaterialDomain(Parameters.MaterialParameters.MaterialDomain) &&
		Nanite::IsSupportedBlendMode(Parameters.MaterialParameters) &&
		!IsVulkanPlatform(Parameters.Platform) &&
		!IsMetalPlatform(Parameters.Platform) && // TODO: Support CS derivatives
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
	OutEnvironment.SetDefine(TEXT("ALWAYS_EVALUATE_WORLD_POSITION_OFFSET"),
		Parameters.MaterialParameters.bAlwaysEvaluateWorldPositionOffset ? 1 : 0);

	if (NaniteSplineMeshesSupported())
	{
		if (Parameters.MaterialParameters.bIsUsedWithSplineMeshes || Parameters.MaterialParameters.bIsDefaultMaterial)
		{
			// NOTE: This effectively means the logic to deform vertices will be added to the barycentrics calculation in the
			// Nanite shading CS, but will be branched over on instances that do not supply spline mesh parameters. If that
			// frequently causes occupancy issues, we may want to consider ways to split the spline meshes into their own
			// shading bin and permute the CS.
			OutEnvironment.SetDefine(TEXT("USE_SPLINEDEFORM"), 1);
			OutEnvironment.SetDefine(TEXT("USE_SPLINE_MESH_SCENE_RESOURCES"), UseSplineMeshSceneResources(Parameters.Platform));
		}
	}

	OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
	OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
	OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	OutEnvironment.CompilerFlags.Add(CFLAG_RootConstants);
	OutEnvironment.CompilerFlags.Add(CFLAG_ShaderBundle);
	OutEnvironment.CompilerFlags.Add(CFLAG_CheckForDerivativeOps);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNaniteVertexFactory, "/Engine/Private/Nanite/NaniteVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsNaniteRendering
	| EVertexFactoryFlags::SupportsComputeShading
	| EVertexFactoryFlags::SupportsManualVertexFetch
	| EVertexFactoryFlags::SupportsLumenMeshCards
	| EVertexFactoryFlags::SupportsLandscape
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

void ClearNaniteResources(Nanite::FResources& InResources)
{
	InResources = {};
}

void ClearNaniteResources(TPimplPtr<Nanite::FResources>& InResources)
{
	InitNaniteResources(InResources, false /* recreate */);
	ClearNaniteResources(*InResources);
}

void InitNaniteResources(TPimplPtr<Nanite::FResources>& InResources, bool bRecreate)
{
	if (!InResources.IsValid() || bRecreate)
	{
		InResources = MakePimpl<Nanite::FResources>();
	}
}

uint64 GetNaniteResourcesSize(const TPimplPtr<Nanite::FResources>& InResources)
{
	if (InResources.IsValid())
	{
		GetNaniteResourcesSize(*InResources);
	}

	return 0;
}

uint64 GetNaniteResourcesSize(const Nanite::FResources& InResources)
{
	uint64 ResourcesSize = 0;
	ResourcesSize += InResources.RootData.GetAllocatedSize();
	ResourcesSize += InResources.ImposterAtlas.GetAllocatedSize();
	ResourcesSize += InResources.HierarchyNodes.GetAllocatedSize();
	ResourcesSize += InResources.HierarchyRootOffsets.GetAllocatedSize();
	ResourcesSize += InResources.PageStreamingStates.GetAllocatedSize();
	ResourcesSize += InResources.PageDependencies.GetAllocatedSize();
	return ResourcesSize;
}

void GetNaniteResourcesSizeEx(const TPimplPtr<Nanite::FResources>& InResources, FResourceSizeEx& CumulativeResourceSize)
{
	if (InResources.IsValid())
	{
		GetNaniteResourcesSizeEx(*InResources.Get(), CumulativeResourceSize);
	}
}

void GetNaniteResourcesSizeEx(const Nanite::FResources& InResources, FResourceSizeEx& CumulativeResourceSize)
{
	InResources.GetResourceSizeEx(CumulativeResourceSize);
}
