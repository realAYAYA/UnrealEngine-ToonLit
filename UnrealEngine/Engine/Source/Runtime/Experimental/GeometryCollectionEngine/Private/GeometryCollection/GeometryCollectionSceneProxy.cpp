// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSceneProxy.h"

#include "Async/ParallelFor.h"
#include "Engine/Engine.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "MaterialDomain.h"
#include "MaterialShaderType.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "CommonRenderResources.h"
#include "Rendering/NaniteResources.h"
#include "PrimitiveSceneInfo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionHitProxy.h"
#include "RHIDefinitions.h"
#include "ComponentReregisterContext.h"
#include "ComponentRecreateRenderStateContext.h"
#include "RenderGraphBuilder.h"

#if RHI_RAYTRACING
#include "RayTracingInstance.h"
#endif

#if INTEL_ISPC
#if USING_CODE_ANALYSIS
    MSVC_PRAGMA( warning( push ) )
    MSVC_PRAGMA( warning( disable : ALL_CODE_ANALYSIS_WARNINGS ) )
#endif    // USING_CODE_ANALYSIS

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#endif

#include "GeometryCollectionSceneProxy.ispc.generated.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if USING_CODE_ANALYSIS
    MSVC_PRAGMA( warning( pop ) )
#endif    // USING_CODE_ANALYSIS

static_assert(sizeof(ispc::FMatrix44f) == sizeof(FMatrix44f), "sizeof(ispc::FMatrix44f) != sizeof(FMatrix44f)");
static_assert(sizeof(ispc::FVector3f) == sizeof(FVector3f), "sizeof(ispc::FVector3f) != sizeof(FVector3f)");
#endif


static int32 GParallelGeometryCollectionBatchSize = 1024;
static TAutoConsoleVariable<int32> CVarParallelGeometryCollectionBatchSize(
	TEXT("r.ParallelGeometryCollectionBatchSize"),
	GParallelGeometryCollectionBatchSize,
	TEXT("The number of vertices per thread dispatch in a single collection. \n"),
	ECVF_Default
);

static int32 GGeometryCollectionTripleBufferUploads = 1;
FAutoConsoleVariableRef CVarGeometryCollectionTripleBufferUploads(
	TEXT("r.GeometryCollectionTripleBufferUploads"),
	GGeometryCollectionTripleBufferUploads,
	TEXT("Whether to triple buffer geometry collection uploads, which allows Lock_NoOverwrite uploads which are much faster on the GPU with large amounts of data."),
	ECVF_Default
);

static int32 GRayTracingGeometryCollectionProxyMeshes = 0;
FAutoConsoleVariableRef CVarRayTracingGeometryCollectionProxyMeshes(
	TEXT("r.RayTracing.Geometry.GeometryCollection"),
	GRayTracingGeometryCollectionProxyMeshes,
	TEXT("Include geometry collection proxy meshes in ray tracing effects (default = 0 (Geometry collection meshes disabled in ray tracing))"),
	ECVF_RenderThreadSafe
);


#if !defined(CHAOS_GEOMETRY_COLLECTION_SET_DYNAMIC_DATA_ISPC_ENABLED_DEFAULT)
#define CHAOS_GEOMETRY_COLLECTION_SET_DYNAMIC_DATA_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bGeometryCollection_SetDynamicData_ISPC_Enabled = INTEL_ISPC && CHAOS_GEOMETRY_COLLECTION_SET_DYNAMIC_DATA_ISPC_ENABLED_DEFAULT;
#else
static bool bGeometryCollection_SetDynamicData_ISPC_Enabled = CHAOS_GEOMETRY_COLLECTION_SET_DYNAMIC_DATA_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarGeometryCollectionSetDynamicDataISPCEnabled(TEXT("r.GeometryCollectionSetDynamicData.ISPC"), bGeometryCollection_SetDynamicData_ISPC_Enabled, TEXT("Whether to use ISPC optimizations to set dynamic data in geometry collections"));
#endif

DEFINE_LOG_CATEGORY_STATIC(FGeometryCollectionSceneProxyLogging, Log, All);

FGeometryCollectionDynamicDataPool GDynamicDataPool;


class FGeometryCollectionMeshCollectorResources : public FOneFrameResource
{
public:
	FGeometryCollectionVertexFactory VertexFactory;
	
	FGeometryCollectionMeshCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel,true)
	{
	}

	virtual ~FGeometryCollectionMeshCollectorResources()
	{
		VertexFactory.ReleaseResource();
	}

	virtual FGeometryCollectionVertexFactory& GetVertexFactory() { return VertexFactory; }
};


FGeometryCollectionSceneProxy::FGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, MeshResource(Component->GetRestCollection()->RenderData->MeshResource)
	, MeshDescription(Component->GetRestCollection()->RenderData->MeshDescription)
	, VertexFactory(GetScene().GetFeatureLevel())
	, bSupportsManualVertexFetch(VertexFactory.SupportsManualVertexFetch(GetScene().GetFeatureLevel()))
	, bSupportsTripleBufferVertexUpload(GRHISupportsMapWriteNoOverwrite)
#if WITH_EDITOR
	, bShowBoneColors(Component->GetShowBoneColors())
	, bSuppressSelectionMaterial(Component->GetSuppressSelectionMaterial())
	, VertexFactoryDebugColor(GetScene().GetFeatureLevel())
#endif
{
	EnableGPUSceneSupportFlags();

	Materials.Empty();
	const int32 NumMaterials = Component->GetNumMaterials();
	for (int MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		Materials.Push(Component->GetMaterial(MaterialIndex));

		if (Materials[MaterialIndex] == nullptr || !Materials[MaterialIndex]->CheckMaterialUsage_Concurrent(MATUSAGE_GeometryCollections))
		{
			Materials[MaterialIndex] = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	Component->GetRestTransforms(RestTransforms);
	NumTransforms = RestTransforms.Num();

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	// Render by SubSection if we are in the rigid body picker.
	bUsesSubSections = Component->GetIsTransformSelectionMode() && MeshDescription.SubSections.Num();
	// Enable bone hit selection proxies if we are in the rigid body picker or in the fracture modes.
	bEnableBoneSelection = Component->GetEnableBoneSelection();

	if (bEnableBoneSelection || bUsesSubSections)
	{
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			HGeometryCollection* HitProxy = new HGeometryCollection(Component, TransformIndex);
			HitProxies.Add(HitProxy);
		}
	}
#endif

#if WITH_EDITOR
	if (bShowBoneColors || bEnableBoneSelection)
	{
		Component->GetBoneColors(BoneColors);
		ColorVertexBuffer.InitFromColorArray(BoneColors);

		if (Component->GetRestCollection())
		{
			BoneSelectedMaterial = Component->GetRestCollection()->GetBoneSelectedMaterial();
		}
		if (!BoneSelectedMaterial)
		{
			int32 LegacyBoneMaterialID = Component->GetBoneSelectedMaterialID();
			if (Materials.IsValidIndex(LegacyBoneMaterialID))
			{
				BoneSelectedMaterial = Materials[LegacyBoneMaterialID];
			}
		}
		if (BoneSelectedMaterial && !BoneSelectedMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_GeometryCollections))
		{
			// If we have an invalid BoneSelectedMaterial, switch it back to null to skip its usage in GetDynamicMeshElements below
			BoneSelectedMaterial = nullptr;
		}

		// Make sure the vertex color material has the usage flag for rendering geometry collections
		if (GEngine->VertexColorMaterial)
		{
			GEngine->VertexColorMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_GeometryCollections);
		}
	}

	// Get hidden geometry and zero the associated transforms.
	Component->GetHiddenTransforms(HiddenTransforms);
	if (HiddenTransforms.Num())
	{
		check(HiddenTransforms.Num() == RestTransforms.Num());
		for (int32 TransformIndex = 0; TransformIndex < RestTransforms.Num(); ++TransformIndex)
		{
			if (HiddenTransforms[TransformIndex])
			{
				RestTransforms[TransformIndex] = FMatrix44f(EForceInit::ForceInitToZero);
			}
		}
	}
#endif

	// #todo(dmp): This flag means that when motion blur is turned on, it will always render geometry collections into the
	// velocity buffer.  Note that the way around this is to loop through the global matrices and test whether they have
	// changed from the prev to curr frame, but this is expensive.  We should revisit this if the draw calls for velocity
	// rendering become a problem. One solution could be to use internal solver sleeping state to drive motion blur.
	bAlwaysHasVelocity = true;
}

FGeometryCollectionSceneProxy::~FGeometryCollectionSceneProxy()
{
	if (DynamicData != nullptr)
	{
		GDynamicDataPool.Release(DynamicData);
		DynamicData = nullptr;
	}
}

SIZE_T FGeometryCollectionSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

static void UpdateLooseParameter(
	FGeometryCollectionVertexFactory& VertexFactory,
	FRHIShaderResourceView* BoneTransformSRV,
	FRHIShaderResourceView* BonePrevTransformSRV,
	FRHIShaderResourceView* BoneMapSRV)
{
	FGCBoneLooseParameters LooseParameters;

	LooseParameters.VertexFetch_BoneTransformBuffer = BoneTransformSRV;
	LooseParameters.VertexFetch_BonePrevTransformBuffer = BonePrevTransformSRV;
	LooseParameters.VertexFetch_BoneMapBuffer = BoneMapSRV;

	EUniformBufferUsage UniformBufferUsage = VertexFactory.EnableLooseParameter ? UniformBuffer_SingleFrame : UniformBuffer_MultiFrame;

	VertexFactory.LooseParameterUniformBuffer = FGCBoneLooseParametersRef::CreateUniformBufferImmediate(LooseParameters, UniformBufferUsage);
}

void FGeometryCollectionSceneProxy::SetupVertexFactory(FRHICommandListBase& RHICmdList, FGeometryCollectionVertexFactory& GeometryCollectionVertexFactory, FColorVertexBuffer* ColorOverride) const
{
	FGeometryCollectionVertexFactory::FDataType Data;
	
	FPositionVertexBuffer const& PositionVB = bSupportsManualVertexFetch ? MeshResource.PositionVertexBuffer : SkinnedPositionVertexBuffer;
	PositionVB.BindPositionVertexBuffer(&GeometryCollectionVertexFactory, Data);

	MeshResource.StaticMeshVertexBuffer.BindTangentVertexBuffer(&GeometryCollectionVertexFactory, Data);
	MeshResource.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&GeometryCollectionVertexFactory, Data);
	MeshResource.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&GeometryCollectionVertexFactory, Data, 0);

	FColorVertexBuffer const& ColorVB = ColorOverride ? *ColorOverride : MeshResource.ColorVertexBuffer;
	ColorVB.BindColorVertexBuffer(&GeometryCollectionVertexFactory, Data);

	if (bSupportsManualVertexFetch)
	{
		Data.BoneMapSRV = MeshResource.BoneMapVertexBuffer.GetSRV();
		Data.BoneTransformSRV = TransformBuffers[CurrentTransformBufferIndex].VertexBufferSRV;
		Data.BonePrevTransformSRV = PrevTransformBuffers[CurrentTransformBufferIndex].VertexBufferSRV;
	}
	else
	{
		// Make sure these are not null to pass UB validation
		Data.BoneMapSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.BoneTransformSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.BonePrevTransformSRV = GNullColorVertexBuffer.VertexBufferSRV;
	}

	GeometryCollectionVertexFactory.SetData(RHICmdList, Data);

	if (!GeometryCollectionVertexFactory.IsInitialized())
	{
		GeometryCollectionVertexFactory.InitResource(RHICmdList);
	}
	else
	{
		GeometryCollectionVertexFactory.UpdateRHI(RHICmdList);
	}
}

void FGeometryCollectionSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	if (bSupportsManualVertexFetch)
	{
		// Initialize transform buffers and upload rest transforms.
		TransformBuffers.AddDefaulted(1);
		PrevTransformBuffers.AddDefaulted(1);

		TransformBuffers[0].NumTransforms = NumTransforms;
		PrevTransformBuffers[0].NumTransforms = NumTransforms;
		TransformBuffers[0].InitResource(RHICmdList);
		PrevTransformBuffers[0].InitResource(RHICmdList);

		const bool bLocalGeometryCollectionTripleBufferUploads = (GGeometryCollectionTripleBufferUploads != 0) && bSupportsTripleBufferVertexUpload;
		const EResourceLockMode LockMode = bLocalGeometryCollectionTripleBufferUploads ? RLM_WriteOnly_NoOverwrite : RLM_WriteOnly;

		FGeometryCollectionTransformBuffer& TransformBuffer = GetCurrentTransformBuffer();
		TransformBuffer.UpdateDynamicData(RHICmdList, RestTransforms, LockMode);
		FGeometryCollectionTransformBuffer& PrevTransformBuffer = GetCurrentPrevTransformBuffer();
		PrevTransformBuffer.UpdateDynamicData(RHICmdList, RestTransforms, LockMode);
	}
	else
	{
		// Initialize CPU skinning buffer with rest transforms.
		SkinnedPositionVertexBuffer.Init(MeshResource.PositionVertexBuffer.GetNumVertices(), false);
		SkinnedPositionVertexBuffer.InitResource(RHICmdList);
		UpdateSkinnedPositions(RHICmdList, RestTransforms);
	}

	SetupVertexFactory(RHICmdList, VertexFactory);

#if WITH_EDITOR
	if (bShowBoneColors || bEnableBoneSelection)
	{
		// Initialize debug color buffer and associated vertex factory.
		ColorVertexBuffer.InitResource(RHICmdList);
		SetupVertexFactory(RHICmdList, VertexFactoryDebugColor, &ColorVertexBuffer);
	}
#endif

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	if (MeshDescription.NumVertices && HitProxies.Num())
	{
		// Create buffer containing per vertex hit proxy IDs.
		HitProxyIdBuffer.Init(MeshDescription.NumVertices);
		HitProxyIdBuffer.InitResource(RHICmdList);

		uint16 const* BoneMapData = &MeshResource.BoneMapVertexBuffer.BoneIndex(0);
		ParallelFor(MeshDescription.NumVertices, [&](int32 i)
		{
			// Note that some fracture undo/redo operations can: recreate scene proxy, then update render data, then recreate proxy again.
			// In that case we can come here the first time with too few hit proxy objects for the bone map which hasn't updated.
			// But we then enter here a second time with the render data correct.
			int16 ProxyIndex = BoneMapData[i];
			ProxyIndex = HitProxies.IsValidIndex(ProxyIndex) ? ProxyIndex : 0;
			HitProxyIdBuffer.VertexColor(i) = HitProxies[ProxyIndex]->Id.GetColor();
		});

		void* VertexBufferData = RHICmdList.LockBuffer(HitProxyIdBuffer.VertexBufferRHI, 0, HitProxyIdBuffer.GetNumVertices() * HitProxyIdBuffer.GetStride(), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, HitProxyIdBuffer.GetVertexData(), HitProxyIdBuffer.GetNumVertices() * HitProxyIdBuffer.GetStride());
		RHICmdList.UnlockBuffer(HitProxyIdBuffer.VertexBufferRHI);
	}
#endif

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		FRayTracingGeometryInitializer Initializer;
		Initializer.DebugName = FName(TEXT("GeometryCollection"));
		Initializer.GeometryType = RTGT_Triangles;
		Initializer.bFastBuild = true;
		Initializer.bAllowUpdate = false;
		Initializer.TotalPrimitiveCount = 0;

		RayTracingGeometry.SetInitializer(Initializer);
		RayTracingGeometry.InitResource(RHICmdList);

		bGeometryResourceUpdated = true;
	}
#endif

	bRenderResourcesCreated = true;
	SetDynamicData_RenderThread(RHICmdList, DynamicData);
}

void FGeometryCollectionSceneProxy::DestroyRenderThreadResources()
{
	if (bSupportsManualVertexFetch)
	{
		for (int32 i = 0; i < TransformBuffers.Num(); i++)
		{
			TransformBuffers[i].ReleaseResource();
			PrevTransformBuffers[i].ReleaseResource();
		}
		TransformBuffers.Reset();
	}
	else
	{
		SkinnedPositionVertexBuffer.ReleaseResource();
	}

	VertexFactory.ReleaseResource();

#if WITH_EDITOR
	VertexFactoryDebugColor.ReleaseResource();
	ColorVertexBuffer.ReleaseResource();
#endif

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	HitProxyIdBuffer.ReleaseResource();
#endif

#if RHI_RAYTRACING
	if (IsRayTracingAllowed())
	{
		RayTracingGeometry.ReleaseResource();
		RayTracingDynamicVertexBuffer.Release();
	}
#endif
}

void FGeometryCollectionSceneProxy::SetDynamicData_RenderThread(FRHICommandListBase& RHICmdList, FGeometryCollectionDynamicData* NewDynamicData)
{
	if (NewDynamicData != DynamicData)
	{
		if (DynamicData)
		{
			GDynamicDataPool.Release(DynamicData);
			DynamicData = nullptr;
		}
		DynamicData = NewDynamicData;
	}

	if (MeshDescription.NumVertices == 0 || !DynamicData || !bRenderResourcesCreated)
	{
		return;
	}
	// Early out if if we are applying (non-dynamic) rest transforms over multiple frames.
	if (!DynamicData->IsDynamic && TransformVertexBuffersContainsRestTransforms)
	{
		return;
	}
	TransformVertexBuffersContainsRestTransforms = !DynamicData->IsDynamic;
		
	if (bSupportsManualVertexFetch)
	{
		const bool bLocalGeometryCollectionTripleBufferUploads = (GGeometryCollectionTripleBufferUploads != 0) && bSupportsTripleBufferVertexUpload;

		if (bLocalGeometryCollectionTripleBufferUploads && TransformBuffers.Num() == 1)
		{
			TransformBuffers.AddDefaulted(2);
			PrevTransformBuffers.AddDefaulted(2);

			for (int32 i = 1; i < 3; i++)
			{
				TransformBuffers[i].NumTransforms = NumTransforms;
				PrevTransformBuffers[i].NumTransforms = NumTransforms;
				TransformBuffers[i].InitResource(RHICmdList);
				PrevTransformBuffers[i].InitResource(RHICmdList);
			}
		}

		// Copy the transform data over to the vertex buffer	
		{
			const EResourceLockMode LockMode = bLocalGeometryCollectionTripleBufferUploads ? RLM_WriteOnly_NoOverwrite : RLM_WriteOnly;

			CycleTransformBuffers(bLocalGeometryCollectionTripleBufferUploads);

			FGeometryCollectionTransformBuffer& TransformBuffer = GetCurrentTransformBuffer();
			FGeometryCollectionTransformBuffer& PrevTransformBuffer = GetCurrentPrevTransformBuffer();

			VertexFactory.SetBoneTransformSRV(TransformBuffer.VertexBufferSRV);
			VertexFactory.SetBonePrevTransformSRV(PrevTransformBuffer.VertexBufferSRV);

#if WITH_EDITOR
			// Implement hiding geometry in editor by zeroing the transform.
			// Could move this to InitDynamicData?
			if (HiddenTransforms.Num())
			{
				for (int32 TransformIndex = 0; TransformIndex < DynamicData->Transforms.Num(); ++TransformIndex)
				{
					if (HiddenTransforms[TransformIndex])
					{
						DynamicData->Transforms[TransformIndex] = FMatrix44f(EForceInit::ForceInitToZero);
						DynamicData->PrevTransforms[TransformIndex] = FMatrix44f(EForceInit::ForceInitToZero);
					}
				}
			}
#endif
				
			if (DynamicData->IsDynamic)
			{
				TransformBuffer.UpdateDynamicData(RHICmdList, DynamicData->Transforms, LockMode);
				PrevTransformBuffer.UpdateDynamicData(RHICmdList, DynamicData->PrevTransforms, LockMode);
			}
			else
			{
				// If we are rendering the base mesh geometry then use RestTransforms for both current and previous transforms.
				TransformBuffer.UpdateDynamicData(RHICmdList, RestTransforms, LockMode);
				PrevTransformBuffer.UpdateDynamicData(RHICmdList, RestTransforms, LockMode);
			}

			UpdateLooseParameter(VertexFactory, TransformBuffer.VertexBufferSRV, PrevTransformBuffer.VertexBufferSRV, MeshResource.BoneMapVertexBuffer.GetSRV());

#if WITH_EDITOR
			if (bShowBoneColors || bEnableBoneSelection)
			{
				VertexFactoryDebugColor.SetBoneTransformSRV(TransformBuffer.VertexBufferSRV);
				VertexFactoryDebugColor.SetBonePrevTransformSRV(PrevTransformBuffer.VertexBufferSRV);
				UpdateLooseParameter(VertexFactoryDebugColor, TransformBuffer.VertexBufferSRV, PrevTransformBuffer.VertexBufferSRV, MeshResource.BoneMapVertexBuffer.GetSRV());
			}
#endif
		}
	}
	else
	{
		UpdateSkinnedPositions(RHICmdList, DynamicData->IsDynamic ? DynamicData->Transforms : RestTransforms);
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		bGeometryResourceUpdated = true;
	}
#endif
}

void FGeometryCollectionSceneProxy::UpdateSkinnedPositions(FRHICommandListBase& RHICmdList, TArray<FMatrix44f> const& Transforms)
{
	const int32 VertexStride = SkinnedPositionVertexBuffer.GetStride();
	const int32 VertexCount = SkinnedPositionVertexBuffer.GetNumVertices();
	check (VertexCount == MeshDescription.NumVertices)

	void* VertexBufferData = RHICmdList.LockBuffer(SkinnedPositionVertexBuffer.VertexBufferRHI, 0, VertexCount * VertexStride, RLM_WriteOnly);
	check(VertexBufferData != nullptr);

	FPositionVertexBuffer const& SourcePositionVertexBuffer = MeshResource.PositionVertexBuffer;
	FBoneMapVertexBuffer const& SourceBoneMapVertexBuffer = MeshResource.BoneMapVertexBuffer;

	bool bParallelGeometryCollection = true;
	int32 ParallelGeometryCollectionBatchSize = CVarParallelGeometryCollectionBatchSize.GetValueOnRenderThread();

	int32 NumBatches = (VertexCount / ParallelGeometryCollectionBatchSize);

	if (VertexCount != ParallelGeometryCollectionBatchSize)
	{
		NumBatches++;
	}

	// Batch too small, don't bother with parallel
	if (ParallelGeometryCollectionBatchSize > VertexCount)
	{
		bParallelGeometryCollection = false;
		ParallelGeometryCollectionBatchSize = VertexCount;
	}

	auto GeometryCollectionBatch([&](int32 BatchNum)
	{
		uint32 IndexOffset = ParallelGeometryCollectionBatchSize * BatchNum;
		uint32 ThisBatchSize = ParallelGeometryCollectionBatchSize;

		// Check for final batch
		if (IndexOffset + ParallelGeometryCollectionBatchSize > MeshDescription.NumVertices)
		{
			ThisBatchSize = VertexCount - IndexOffset;
		}

		if (ThisBatchSize > 0)
		{
			const FMatrix44f* RESTRICT BoneTransformsPtr = Transforms.GetData();

			if (bGeometryCollection_SetDynamicData_ISPC_Enabled)
			{
#if INTEL_ISPC
				uint8* VertexBufferOffset = (uint8*)VertexBufferData + (IndexOffset * VertexStride);
				ispc::SetDynamicData_RenderThread(
					(ispc::FVector3f*)VertexBufferOffset,
					ThisBatchSize,
					VertexStride,
					&SourceBoneMapVertexBuffer.BoneIndex(IndexOffset),
					(ispc::FMatrix44f*)BoneTransformsPtr,
					(ispc::FVector3f*)&SourcePositionVertexBuffer.VertexPosition(IndexOffset));
#endif
			}
			else
			{
				for (uint32 i = IndexOffset; i < IndexOffset + ThisBatchSize; i++)
				{
					FVector3f Transformed = BoneTransformsPtr[SourceBoneMapVertexBuffer.BoneIndex(i)].TransformPosition(SourcePositionVertexBuffer.VertexPosition(i));
					FMemory::Memcpy((uint8*)VertexBufferData + (i * VertexStride), &Transformed, sizeof(FVector3f));
				}
			}
		}
	});

	ParallelFor(NumBatches, GeometryCollectionBatch, !bParallelGeometryCollection);

	RHICmdList.UnlockBuffer(SkinnedPositionVertexBuffer.VertexBufferRHI);
}

FMaterialRenderProxy* FGeometryCollectionSceneProxy::GetMaterial(FMeshElementCollector& Collector, int32 MaterialIndex) const
{
	FMaterialRenderProxy* MaterialProxy = nullptr;

#if WITH_EDITOR
	if (bShowBoneColors && GEngine->VertexColorMaterial)
	{
		// Material for colored bones
		UMaterial* VertexColorVisualizationMaterial = GEngine->VertexColorMaterial;
		FMaterialRenderProxy* VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
			VertexColorVisualizationMaterial->GetRenderProxy(),
			GetSelectionColor(FLinearColor::White, false, false)
		);
		Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
		MaterialProxy = VertexColorVisualizationMaterialInstance;
	}
	else 
#endif
	if(Materials.IsValidIndex(MaterialIndex))
	{
		MaterialProxy = Materials[MaterialIndex]->GetRenderProxy();
	}

	if (MaterialProxy == nullptr)
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	}

	return MaterialProxy;
}

FVertexFactory const* FGeometryCollectionSceneProxy::GetVertexFactory() const
{
#if WITH_EDITOR
	return bShowBoneColors ? &VertexFactoryDebugColor : &VertexFactory;
#else
	return &VertexFactory;
#endif
}

void FGeometryCollectionSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GeometryCollectionSceneProxy_GetDynamicMeshElements);
	if (MeshDescription.NumVertices == 0)
	{
		return;
	}
		
	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;
	const bool bWireframe = AllowDebugViewmodes() && EngineShowFlags.Wireframe;
	const bool bProxyIsSelected = IsSelected();

	auto SetDebugMaterial = [this, &Collector, &EngineShowFlags, bProxyIsSelected](FMeshBatch& Mesh) -> void
	{
#if UE_ENABLE_DEBUG_DRAWING

		// flag to indicate whether we've set a debug material yet
		// Note: Will be used if we add more debug material options
		// (compare to variable of same name in StaticMeshRender.cpp)
		bool bDebugMaterialRenderProxySet = false;

		if (!bDebugMaterialRenderProxySet && bProxyIsSelected && EngineShowFlags.VertexColors && AllowDebugViewmodes())
		{
			// Override the mesh's material with our material that draws the vertex colors
			UMaterial* VertexColorVisualizationMaterial = NULL;
			switch (GVertexColorViewMode)
			{
			case EVertexColorViewMode::Color:
				VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
				break;

			case EVertexColorViewMode::Alpha:
				VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
				break;

			case EVertexColorViewMode::Red:
				VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
				break;

			case EVertexColorViewMode::Green:
				VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
				break;

			case EVertexColorViewMode::Blue:
				VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
				break;
			}
			check(VertexColorVisualizationMaterial != NULL);

			// Note: static mesh renderer does something more complicated involving per-section selection,
			// but whole component selection seems ok for now
			bool bSectionIsSelected = bProxyIsSelected;

			auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
				VertexColorVisualizationMaterial->GetRenderProxy(),
				GetSelectionColor(FLinearColor::White, bSectionIsSelected, IsHovered())
			);

			Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
			Mesh.MaterialRenderProxy = VertexColorVisualizationMaterialInstance;

			bDebugMaterialRenderProxySet = true;
		}
#endif
	};

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if ((VisibilityMap & (1 << ViewIndex)) == 0) 
		{
			continue; 
		}

		// If not dynamic then use the section array with interior fracture surfaces removed.
		bool bRemoveInternalFaces = DynamicData != nullptr && !DynamicData->IsDynamic && MeshDescription.SectionsNoInternal.Num();

#if WITH_EDITOR
		// If hiding geometry in editor then we don't remove hidden faces.
		bRemoveInternalFaces &= HiddenTransforms.Num() == 0;
#endif

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		// If using subsections then use the subsection array. 
		TArray<FGeometryCollectionMeshElement> const& SectionArray = bUsesSubSections 
			? MeshDescription.SubSections 
			: bRemoveInternalFaces ? MeshDescription.SectionsNoInternal : MeshDescription.Sections;
#else
		TArray<FGeometryCollectionMeshElement> const& SectionArray = bRemoveInternalFaces ? MeshDescription.SectionsNoInternal : MeshDescription.Sections;
#endif

		// Grab the material proxies we'll be using for each section.
		TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;
		for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
		{
			const FGeometryCollectionMeshElement& Section = SectionArray[SectionIndex];
			FMaterialRenderProxy* MaterialProxy = GetMaterial(Collector, Section.MaterialIndex);
			MaterialProxies.Add(MaterialProxy);
		}

		// Draw the meshes.
		for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
		{
			const FGeometryCollectionMeshElement& Section = SectionArray[SectionIndex];

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = bWireframe;
			Mesh.VertexFactory = GetVertexFactory();
			Mesh.MaterialRenderProxy = MaterialProxies[SectionIndex];
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = true;
			SetDebugMaterial(Mesh);

			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &MeshResource.IndexBuffer;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.FirstIndex = Section.TriangleStart * 3;
			BatchElement.NumPrimitives = Section.TriangleCount;
			BatchElement.MinVertexIndex = Section.VertexStart;
			BatchElement.MaxVertexIndex = Section.VertexEnd;

			Collector.AddMesh(ViewIndex, Mesh);
		}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		// Highlight selected bone using specialized material.
		// #note: This renders the geometry again but with the bone selection material.  Ideally we'd have one render pass and one material.
		if (bEnableBoneSelection && !bSuppressSelectionMaterial && BoneSelectedMaterial)
		{
			FMaterialRenderProxy* MaterialRenderProxy = BoneSelectedMaterial->GetRenderProxy();

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = bWireframe;
			Mesh.VertexFactory = &VertexFactoryDebugColor;
			Mesh.MaterialRenderProxy = MaterialRenderProxy;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;

			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &MeshResource.IndexBuffer;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = MeshDescription.NumTriangles;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = MeshDescription.NumVertices;

			Collector.AddMesh(ViewIndex, Mesh);
		}
#endif // GEOMETRYCOLLECTION_EDITOR_SELECTION

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
	}
}

#if RHI_RAYTRACING
void FGeometryCollectionSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances)
{
	if (GRayTracingGeometryCollectionProxyMeshes == 0 || MeshDescription.NumVertices == 0)
	{
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_GeometryCollectionSceneProxy_GetDynamicRayTracingInstances);

	const uint32 LODIndex = 0;
	const bool bWireframe = false;
		
	//Loose parameter needs to be updated every frame
	FGeometryCollectionMeshCollectorResources* CollectorResources;
	CollectorResources = &Context.RayTracingMeshResourceCollector.
		AllocateOneFrameResource<FGeometryCollectionMeshCollectorResources>(GetScene().GetFeatureLevel());
	FGeometryCollectionVertexFactory& GeometryCollectionVertexFactory = CollectorResources->GetVertexFactory();
		
	// Render dynamic objects
	if (!GeometryCollectionVertexFactory.GetType()->SupportsRayTracingDynamicGeometry())
	{
		return;
	}

	SetupVertexFactory(Context.GraphBuilder.RHICmdList, GeometryCollectionVertexFactory);

	// If not dynamic then use the section array with interior fracture surfaces removed.
	const bool bRemoveInternalFaces = DynamicData != nullptr && !DynamicData->IsDynamic && MeshDescription.SectionsNoInternal.Num();
	TArray<FGeometryCollectionMeshElement> const& SectionArray = bRemoveInternalFaces ? MeshDescription.SectionsNoInternal : MeshDescription.Sections;

	UpdatingRayTracingGeometry_RenderingThread(SectionArray);

	if (SectionArray.Num() && RayTracingGeometry.RayTracingGeometryRHI.IsValid())
	{
		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &RayTracingGeometry;
		RayTracingInstance.InstanceTransforms.Emplace(GetLocalToWorld());

		// Grab the material proxies we'll be using for each section
		TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;

		for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
		{
			const FGeometryCollectionMeshElement& Section = SectionArray[SectionIndex];
				
			//TODO: Add BoneColor support in Path/Ray tracing?
			FMaterialRenderProxy* MaterialProxy= Materials[Section.MaterialIndex]->GetRenderProxy();

			if (MaterialProxy == nullptr)
			{
				MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			}

			MaterialProxies.Add(MaterialProxy);
		}

		uint32 MaxVertexIndex = 0;
		for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
		{
			const FGeometryCollectionMeshElement& Section = SectionArray[SectionIndex];

			// Draw the mesh
			FMeshBatch& Mesh = RayTracingInstance.Materials.AddDefaulted_GetRef();
			Mesh.bWireframe = bWireframe;
			Mesh.SegmentIndex = SectionIndex;
			Mesh.VertexFactory = &GeometryCollectionVertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxies[SectionIndex];
			Mesh.LODIndex = LODIndex;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.bDisableBackfaceCulling = true;
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = true;

			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &MeshResource.IndexBuffer;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.FirstIndex = Section.TriangleStart * 3;
			BatchElement.NumPrimitives = Section.TriangleCount;
			BatchElement.MinVertexIndex = Section.VertexStart;
			BatchElement.MaxVertexIndex = Section.VertexEnd; 
			BatchElement.NumInstances = 1;

			MaxVertexIndex = std::max(Section.VertexEnd, MaxVertexIndex);

			//#TODO: bone color, bone selection and render bound?
		}

		FRWBuffer* VertexBuffer = RayTracingDynamicVertexBuffer.NumBytes > 0 ? &RayTracingDynamicVertexBuffer : nullptr;

		const uint32 VertexCount = MaxVertexIndex + 1;
		Context.DynamicRayTracingGeometriesToUpdate.Add(
			FRayTracingDynamicGeometryUpdateParams
			{
				RayTracingInstance.Materials,
				false,
				VertexCount,
				VertexCount * (uint32)sizeof(FVector3f),
				RayTracingGeometry.Initializer.TotalPrimitiveCount,
				&RayTracingGeometry,
				VertexBuffer,
				true
			}
		);

		OutRayTracingInstances.Emplace(RayTracingInstance);
	}
}

void FGeometryCollectionSceneProxy::UpdatingRayTracingGeometry_RenderingThread(TArray<FGeometryCollectionMeshElement> const& InSectionArray)
{
	FRHICommandList& RHICmdList = FRHICommandListImmediate::Get();

	if (bGeometryResourceUpdated)
	{
		RayTracingGeometry.Initializer.Segments.Empty();
		RayTracingGeometry.Initializer.TotalPrimitiveCount = 0;

		for (int SectionIndex = 0; SectionIndex < InSectionArray.Num(); ++SectionIndex)
		{
			const FGeometryCollectionMeshElement& Section = InSectionArray[SectionIndex];
			FRayTracingGeometrySegment Segment;
			Segment.FirstPrimitive = Section.TriangleStart;
			Segment.VertexBuffer = MeshResource.PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = Section.TriangleCount;
			Segment.MaxVertices = Section.VertexEnd;
			RayTracingGeometry.Initializer.Segments.Add(Segment);
			RayTracingGeometry.Initializer.TotalPrimitiveCount += Section.TriangleCount;
		}
				
		if (RayTracingGeometry.Initializer.TotalPrimitiveCount > 0)
		{
			RayTracingGeometry.Initializer.IndexBuffer = MeshResource.IndexBuffer.IndexBufferRHI;
			// Create the ray tracing geometry but delay the acceleration structure build.
			RayTracingGeometry.CreateRayTracingGeometry(RHICmdList, ERTAccelerationStructureBuildPriority::Skip);
		}

		bGeometryResourceUpdated = false;
	}
}
#endif // RHI_RAYTRACING

FPrimitiveViewRelevance FGeometryCollectionSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);

	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;

	return Result;
}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
HHitProxy* FGeometryCollectionSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	HHitProxy* DefaultHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
	OutHitProxies.Append(HitProxies);
	return DefaultHitProxy;
}
#endif

void FGeometryCollectionSceneProxy::GetPreSkinnedLocalBounds(FBoxSphereBounds& OutBounds) const
{
	OutBounds = MeshDescription.PreSkinnedBounds; 
}

uint32 FGeometryCollectionSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize()
		+ Materials.GetAllocatedSize()
		+ MeshDescription.Sections.GetAllocatedSize()
		+ MeshDescription.SubSections.GetAllocatedSize()
		+ RestTransforms.GetAllocatedSize()
		+ (SkinnedPositionVertexBuffer.GetAllowCPUAccess() ? SkinnedPositionVertexBuffer.GetStride() * SkinnedPositionVertexBuffer.GetNumVertices() : 0)
#if WITH_EDITOR
		+ BoneColors.GetAllocatedSize()
		+ (ColorVertexBuffer.GetAllowCPUAccess() ? ColorVertexBuffer.GetStride() * ColorVertexBuffer.GetNumVertices() : 0)
		+ HiddenTransforms.GetAllocatedSize()
#endif
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		+ HitProxies.GetAllocatedSize()
		+ (HitProxyIdBuffer.GetAllowCPUAccess() ? HitProxyIdBuffer.GetStride() * HitProxyIdBuffer.GetNumVertices() : 0)
#endif
#if RHI_RAYTRACING
		+ RayTracingGeometry.RawData.GetAllocatedSize()
#endif
		;
}


FNaniteGeometryCollectionSceneProxy::FNaniteGeometryCollectionSceneProxy(UGeometryCollectionComponent* Component)
: Nanite::FSceneProxyBase(Component)
, GeometryCollection(Component->GetRestCollection())
, bCurrentlyInMotion(false)
, bRequiresGPUSceneUpdate(false)
{
	LLM_SCOPE_BYTAG(Nanite);

	// Nanite requires GPUScene
	checkSlow(UseGPUScene(GMaxRHIShaderPlatform, GetScene().GetFeatureLevel()));
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));
	checkSlow(GeometryCollection->HasNaniteData());

	MaterialRelevance = Component->GetMaterialRelevance(Component->GetScene()->GetFeatureLevel());

	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffersImpl.BeginWriteAccess(AccessTag);
	ProxyData.Flags.bHasPerInstanceHierarchyOffset = true;
	ProxyData.Flags.bHasPerInstanceLocalBounds = true;
	ProxyData.Flags.bHasPerInstanceDynamicData = true;
	InstanceSceneDataBuffersImpl.EndWriteAccess(AccessTag);

	// Note: ideally this would be picked up from the Flags.bHasPerInstanceDynamicData above, but that path is not great at the moment.
	bAlwaysHasVelocity = true;

	// Nanite supports the GPUScene instance data buffer.
	SetupInstanceSceneDataBuffers(&InstanceSceneDataBuffersImpl);

	bSupportsDistanceFieldRepresentation = false;

	// Dynamic draw path without Nanite isn't supported by Lumen
	bVisibleInLumenScene = false;

	// Use fast path that does not update static draw lists.
	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	// Nanite always uses GPUScene, so we can skip expensive primitive uniform buffer updates.
	bVFRequiresPrimitiveUniformBuffer = false;

	// Indicates if 1 or more materials contain settings not supported by Nanite.
	bHasMaterialErrors = false;

	// Check if the assigned material can be rendered in Nanite. If not, default.
	// TODO: Handle cases like geometry collections adding a "selected geometry" material with translucency.
	const bool IsRenderable = true;// Nanite::FSceneProxy::IsNaniteRenderable(MaterialRelevance);

	if (!IsRenderable)
	{
		bHasMaterialErrors = true;
	}

	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = GeometryCollection->GetGeometryCollection();
	const TManagedArray<int32>& TransformToGeometryIndices = Collection->TransformToGeometryIndex;
	const TManagedArray<int32>& SimulationType = Collection->SimulationType;
	const TManagedArray<FGeometryCollectionSection>& SectionsArray = Collection->Sections;

	MaterialSections.SetNumZeroed(SectionsArray.Num());

	for (int32 SectionIndex = 0; SectionIndex < SectionsArray.Num(); ++SectionIndex)
	{
		const FGeometryCollectionSection& MeshSection = SectionsArray[SectionIndex];
		const bool bValidMeshSection = MeshSection.MaterialID != INDEX_NONE;

		// Keep track of highest observed material index.
		MaterialMaxIndex = FMath::Max(MeshSection.MaterialID, MaterialMaxIndex);

		UMaterialInterface* MaterialInterface = bValidMeshSection ? Component->GetMaterial(MeshSection.MaterialID) : nullptr;

		// TODO: PROG_RASTER (Implement programmable raster support)
		const bool bInvalidMaterial = !MaterialInterface || !IsOpaqueOrMaskedBlendMode(*MaterialInterface) || MaterialInterface->GetShadingModels().HasShadingModel(MSM_SingleLayerWater);
		if (bInvalidMaterial)
		{
			bHasMaterialErrors = true;
			if (MaterialInterface)
			{
				UE_LOG
				(
					LogStaticMesh, Warning,
					TEXT("Invalid material [%s] used on Nanite geometry collection [%s] - forcing default material instead. Only opaque blend mode and a shading model that is not SingleLayerWater is currently supported, [%s] blend mode and [%s] shading model was specified."),
					*MaterialInterface->GetName(),
					*GeometryCollection->GetName(),
					*GetBlendModeString(MaterialInterface->GetBlendMode()),
					*GetShadingModelFieldString(MaterialInterface->GetShadingModels())
				);
			}
		}

		const bool bForceDefaultMaterial = /*!!FORCE_NANITE_DEFAULT_MATERIAL ||*/ bHasMaterialErrors;
		if (bForceDefaultMaterial)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// Should never be null here
		check(MaterialInterface != nullptr);

		// Should always be opaque blend mode here.
		check(IsOpaqueOrMaskedBlendMode(*MaterialInterface));

		MaterialSections[SectionIndex].ShadingMaterialProxy = MaterialInterface->GetRenderProxy();
		MaterialSections[SectionIndex].RasterMaterialProxy  = MaterialInterface->GetRenderProxy(); // TODO: PROG_RASTER (Implement programmable raster support)
		MaterialSections[SectionIndex].MaterialIndex = MeshSection.MaterialID;
	}

	const bool bHasGeometryBoundingBoxes = 
		Collection->HasAttribute("BoundingBox", FGeometryCollection::GeometryGroup) &&
		Collection->NumElements(FGeometryCollection::GeometryGroup);
	
	const bool bHasTransformBoundingBoxes = 
		Collection->NumElements(FGeometryCollection::TransformGroup) && 
		Collection->HasAttribute("BoundingBox", FGeometryCollection::TransformGroup) &&
		Collection->HasAttribute("TransformToGeometryIndex", FGeometryCollection::TransformGroup);

	int32 NumGeometry = 0;
	if (bHasGeometryBoundingBoxes)
	{
		NumGeometry = Collection->NumElements(FGeometryCollection::GeometryGroup);
		GeometryNaniteData.SetNumUninitialized(NumGeometry);

		const TManagedArray<FBox>& BoundingBoxes = Collection->GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);
		for (int32 GeometryIndex = 0; GeometryIndex < NumGeometry; ++GeometryIndex)
		{
			FGeometryNaniteData& Instance = GeometryNaniteData[GeometryIndex];
			Instance.HierarchyOffset = GeometryCollection->GetNaniteHierarchyOffset(GeometryIndex);
			Instance.LocalBounds = BoundingBoxes[GeometryIndex];
		}
	}
	else if (bHasTransformBoundingBoxes)
	{
		NumGeometry = GeometryCollection->RenderData->NaniteResourcesPtr->HierarchyRootOffsets.Num();
		GeometryNaniteData.SetNumUninitialized(NumGeometry);
		
		const TManagedArray<FBox>& BoundingBoxes = Collection->GetAttribute<FBox>("BoundingBox", FGeometryCollection::TransformGroup);
		const TManagedArray<int32>& TransformToGeometry = Collection->GetAttribute<int32>("TransformToGeometryIndex", FGeometryCollection::TransformGroup);
		const int32 NumTransforms = TransformToGeometry.Num();
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			const int32 GeometryIndex = TransformToGeometry[TransformIndex];
			if (GeometryIndex > INDEX_NONE)
			{
				FGeometryNaniteData& Instance = GeometryNaniteData[GeometryIndex];
				Instance.HierarchyOffset = GeometryCollection->GetNaniteHierarchyOffset(GeometryIndex);
				Instance.LocalBounds = BoundingBoxes[TransformIndex];
			}
		}
	}

	// Initialize to rest transforms.
	TArray<FMatrix44f> RestTransforms;
	Component->GetRestTransforms(RestTransforms);

	FGeometryCollectionDynamicData* DynamicData = GDynamicDataPool.Allocate();
	DynamicData->IsDynamic = true;
	DynamicData->Transforms = RestTransforms;
	DynamicData->PrevTransforms = RestTransforms;
	SetDynamicData_RenderThread(DynamicData, Component->GetRenderMatrix());
}

void FNaniteGeometryCollectionSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	// Should have valid Nanite data at this point.
	NaniteResourceID = GeometryCollection->GetNaniteResourceID();
	NaniteHierarchyOffset = GeometryCollection->GetNaniteHierarchyOffset();
	check(NaniteResourceID != INDEX_NONE && NaniteHierarchyOffset != INDEX_NONE);
}

SIZE_T FNaniteGeometryCollectionSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FPrimitiveViewRelevance FNaniteGeometryCollectionSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	LLM_SCOPE_BYTAG(Nanite);

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.NaniteMeshes;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bRenderCustomDepth = Nanite::GetSupportsCustomDepthRendering() && ShouldRenderCustomDepth();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

	// Always render the Nanite mesh data with static relevance.
	Result.bStaticRelevance = true;

	// Should always be covered by constructor of Nanite scene proxy.
	Result.bRenderInMainPass = true;

#if WITH_EDITOR
	// Only check these in the editor
	Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
	Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered());
#endif

	bool bSetDynamicRelevance = false;

	Result.bOpaque = true;

	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = Result.bOpaque && Result.bRenderInMainPass && DrawsVelocity();

	return Result;
}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
HHitProxy* FNaniteGeometryCollectionSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	LLM_SCOPE_BYTAG(Nanite);

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

	return Super::CreateHitProxies(Component, OutHitProxies);
}
#endif

void FNaniteGeometryCollectionSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	const FLightCacheInterface* LCI = nullptr;
	DrawStaticElementsInternal(PDI, LCI);
}

uint32 FNaniteGeometryCollectionSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

void FNaniteGeometryCollectionSceneProxy::OnTransformChanged(FRHICommandListBase& RHICmdList)
{
}

void FNaniteGeometryCollectionSceneProxy::GetNaniteResourceInfo(uint32& ResourceID, uint32& HierarchyOffset, uint32& ImposterIndex) const
{
	ResourceID = NaniteResourceID;
	HierarchyOffset = NaniteHierarchyOffset;
	ImposterIndex = INDEX_NONE;	// Imposters are not supported (yet?)
}

void FNaniteGeometryCollectionSceneProxy::GetNaniteMaterialMask(FUint32Vector2& OutMaterialMask) const
{
	// TODO: Implement support
	OutMaterialMask = FUint32Vector2(~uint32(0), ~uint32(0));
}

Nanite::FResourceMeshInfo FNaniteGeometryCollectionSceneProxy::GetResourceMeshInfo() const
{
	Nanite::FResources& NaniteResources = *GeometryCollection->RenderData->NaniteResourcesPtr.Get();

	Nanite::FResourceMeshInfo OutInfo;

	OutInfo.NumClusters = NaniteResources.NumClusters;
	OutInfo.NumNodes = NaniteResources.NumHierarchyNodes;
	OutInfo.NumVertices = NaniteResources.NumInputVertices;
	OutInfo.NumTriangles = NaniteResources.NumInputTriangles;
	OutInfo.NumMaterials = MaterialMaxIndex + 1;
	OutInfo.DebugName = GeometryCollection->GetFName();

	OutInfo.NumResidentClusters = NaniteResources.NumResidentClusters;

	// TODO: SegmentMapping
	OutInfo.NumSegments = 0;

	return MoveTemp(OutInfo);
}

void FNaniteGeometryCollectionSceneProxy::SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData, const FMatrix &PrimitiveLocalToWorld)
{
	// Are we currently simulating?
	if (NewDynamicData->IsDynamic)
	{
		FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
		FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffersImpl.BeginWriteAccess(AccessTag);
		InstanceSceneDataBuffersImpl.SetPrimitiveLocalToWorld(PrimitiveLocalToWorld, AccessTag);

		const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = GeometryCollection->GetGeometryCollection();
		const TManagedArray<int32>& TransformToGeometryIndices	 = Collection->TransformToGeometryIndex;
		const TManagedArray<TSet<int32>>& TransformChildren		 = Collection->Children;
		const TManagedArray<int32>& SimulationType				 = Collection->SimulationType;

		const int32 TransformCount = NewDynamicData->Transforms.Num();
		check(TransformCount == TransformToGeometryIndices.Num());
		check(TransformCount == TransformChildren.Num());
		check(TransformCount == NewDynamicData->PrevTransforms.Num());


		ProxyData.InstanceToPrimitiveRelative.Reset(TransformCount);
		ProxyData.PrevInstanceToPrimitiveRelative.Reset(TransformCount);
		ProxyData.InstanceLocalBounds.Reset(TransformCount);
		ProxyData.InstanceHierarchyOffset.Reset(TransformCount);

		ProxyData.Flags.bHasPerInstanceDynamicData = true;
		ProxyData.Flags.bHasPerInstanceLocalBounds = true;
		ProxyData.Flags.bHasPerInstanceHierarchyOffset = true;

		for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
		{
			const int32 TransformToGeometryIndex = TransformToGeometryIndices[TransformIndex];
			if (SimulationType[TransformIndex] != FGeometryCollection::ESimulationTypes::FST_Rigid)
			{
				continue;
			}

			const FGeometryNaniteData& NaniteData = GeometryNaniteData[TransformToGeometryIndex];

			const FRenderTransform& InstanceToPrimitiveRelative = ProxyData.InstanceToPrimitiveRelative.Emplace_GetRef(InstanceSceneDataBuffersImpl.ComputeInstanceToPrimitiveRelative(NewDynamicData->Transforms[TransformIndex], AccessTag));

			FRenderTransform& PrevInstanceToPrimitiveRelative = ProxyData.PrevInstanceToPrimitiveRelative.Emplace_GetRef();

			if (bCurrentlyInMotion)
			{
				PrevInstanceToPrimitiveRelative = InstanceSceneDataBuffersImpl.ComputeInstanceToPrimitiveRelative(NewDynamicData->PrevTransforms[TransformIndex], AccessTag);
			}
			else
			{
				PrevInstanceToPrimitiveRelative = InstanceToPrimitiveRelative;
			}

			ProxyData.InstanceLocalBounds.Emplace(PadInstanceLocalBounds(NaniteData.LocalBounds));
			ProxyData.InstanceHierarchyOffset.Emplace(NaniteData.HierarchyOffset);
		}
		InstanceSceneDataBuffersImpl.EndWriteAccess(AccessTag);
	}
	else
	{
		// Rendering base geometry, use rest transforms rather than simulated transforms.
		// ...
	}

	GDynamicDataPool.Release(NewDynamicData);
}

void FNaniteGeometryCollectionSceneProxy::ResetPreviousTransforms_RenderThread()
{
	FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
	FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffersImpl.BeginWriteAccess(AccessTag);
	// Reset previous transforms to avoid locked motion vectors
	// TODO: we should be able to just turn off & delete the prev transforms instead.
	check(ProxyData.InstanceToPrimitiveRelative.Num() == ProxyData.PrevInstanceToPrimitiveRelative.Num()); // Sanity check, should always have matching associated arrays
	for (int32 InstanceIndex = 0; InstanceIndex < ProxyData.InstanceToPrimitiveRelative.Num(); ++InstanceIndex)
	{
		ProxyData.PrevInstanceToPrimitiveRelative[InstanceIndex] = ProxyData.InstanceToPrimitiveRelative[InstanceIndex];
	}
	InstanceSceneDataBuffersImpl.EndWriteAccess(AccessTag);
}

void FNaniteGeometryCollectionSceneProxy::FlushGPUSceneUpdate_GameThread()
{
	ENQUEUE_RENDER_COMMAND(NaniteProxyUpdateGPUScene)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FPrimitiveSceneInfo* NanitePrimitiveInfo = GetPrimitiveSceneInfo();
			if (NanitePrimitiveInfo && GetRequiresGPUSceneUpdate_RenderThread())
			{
				// Attempt to queue up a GPUScene update - maintain dirty flag if the request fails.
				const bool bRequiresUpdate = !NanitePrimitiveInfo->RequestGPUSceneUpdate();
				SetRequiresGPUSceneUpdate_RenderThread(bRequiresUpdate);
			}
		}
	);
}

void FNaniteGeometryCollectionSceneProxy::OnMotionBegin()
{
	bCurrentlyInMotion = true;
	bCanSkipRedundantTransformUpdates = false;
}

void FNaniteGeometryCollectionSceneProxy::OnMotionEnd()
{
	bCurrentlyInMotion = false;
	bCanSkipRedundantTransformUpdates = true;
	ResetPreviousTransforms_RenderThread();
}


FGeometryCollectionDynamicDataPool::FGeometryCollectionDynamicDataPool()
{
	FreeList.SetNum(32);
	for (int32 ListIndex = 0; ListIndex < FreeList.Num(); ++ListIndex)
	{
		FreeList[ListIndex] = new FGeometryCollectionDynamicData;
	}
}

FGeometryCollectionDynamicDataPool::~FGeometryCollectionDynamicDataPool()
{
	FScopeLock ScopeLock(&ListLock);

	for (FGeometryCollectionDynamicData* Entry : FreeList)
	{
		delete Entry;
	}

	for (FGeometryCollectionDynamicData* Entry : UsedList)
	{
		delete Entry;
	}

	FreeList.Empty();
	UsedList.Empty();
}

FGeometryCollectionDynamicData* FGeometryCollectionDynamicDataPool::Allocate()
{
	FScopeLock ScopeLock(&ListLock);

	FGeometryCollectionDynamicData* NewEntry = nullptr;
	if (FreeList.Num() > 0)
	{
		NewEntry = FreeList.Pop(EAllowShrinking::No);
	}

	if (NewEntry == nullptr)
	{
		NewEntry = new FGeometryCollectionDynamicData;
	}

	NewEntry->Reset();
	UsedList.Push(NewEntry);

	return NewEntry;
}

void FGeometryCollectionDynamicDataPool::Release(FGeometryCollectionDynamicData* DynamicData)
{
	FScopeLock ScopeLock(&ListLock);

	int32 UsedIndex = UsedList.Find(DynamicData);
	if (ensure(UsedIndex != INDEX_NONE))
	{
		UsedList.RemoveAt(UsedIndex, 1, EAllowShrinking::No);
		FreeList.Push(DynamicData);
	}
}

void FGeometryCollectionTransformBuffer::UpdateDynamicData(FRHICommandListBase& RHICmdList, const TArray<FMatrix44f>& Transforms, EResourceLockMode LockMode)
{
	check(NumTransforms == Transforms.Num());

	void* VertexBufferData = RHICmdList.LockBuffer(VertexBufferRHI, 0, Transforms.Num() * sizeof(FMatrix44f), LockMode);
	FMemory::Memcpy(VertexBufferData, Transforms.GetData(), Transforms.Num() * sizeof(FMatrix44f));
	RHICmdList.UnlockBuffer(VertexBufferRHI);
}
