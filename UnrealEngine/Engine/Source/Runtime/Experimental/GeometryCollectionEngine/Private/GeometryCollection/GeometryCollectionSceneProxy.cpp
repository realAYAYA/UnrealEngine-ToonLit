// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSceneProxy.h"

#include "Async/ParallelFor.h"
#include "Engine/Engine.h"
#include "MaterialShaderType.h"
#include "Materials/Material.h"
#include "CommonRenderResources.h"
#include "Rendering/NaniteResources.h"
#include "PrimitiveSceneInfo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "RHIDefinitions.h"
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
#include "GeometryCollection/GeometryCollectionHitProxy.h"
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

#include "ComponentReregisterContext.h"
#include "ComponentRecreateRenderStateContext.h"

#if RHI_RAYTRACING
#include "RayTracingInstance.h"
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

static int32 GGeometryCollectionOptimizedTransforms = 1;
FAutoConsoleVariableRef CVarGeometryCollectionOptimizedTransforms(
	TEXT("r.GeometryCollectionOptimizedTransforms"),
	GGeometryCollectionOptimizedTransforms,
	TEXT("Whether to optimize transform update by skipping automatic updates in GPUScene."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
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
	
	FGeometryCollectionMeshCollectorResources(ERHIFeatureLevel::Type InFeatureLevel):VertexFactory(InFeatureLevel,true)
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
	, NumVertices(0)
	, NumIndices(0)
	, VertexFactory(GetScene().GetFeatureLevel())
	, bSupportsManualVertexFetch(VertexFactory.SupportsManualVertexFetch(GetScene().GetFeatureLevel()))
	, bSupportsTripleBufferVertexUpload(GRHISupportsMapWriteNoOverwrite)
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	, SubSections()
	, SubSectionHitProxies()
	, SubSectionHitProxyIndexMap()
	, bUsesSubSections(false)
#endif
	, DynamicData(nullptr)
	, ConstantData(nullptr)
	, bShowBoneColors(Component->GetShowBoneColors())
	, bEnableBoneSelection(Component->GetEnableBoneSelection())
	, bSuppressSelectionMaterial(Component->GetSuppressSelectionMaterial())
	, BoneSelectionMaterialID(Component->GetBoneSelectedMaterialID())
	, bUseFullPrecisionUVs(Component->GetRestCollection()->bUseFullPrecisionUVs)
	, TransformVertexBuffersContainsOriginalMesh(false)
{
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

	// Make sure the vertex color material has the usage flag for rendering geometry collections
	if (GEngine->VertexColorMaterial)
	{
		GEngine->VertexColorMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_GeometryCollections);
	}

	// #todo(dmp): We create the sections before we set the constant data because we need to make sure these
	// are set before the hit proxies are created via CreateHitProxies.  Ideally, all data is passed in
	// here when we create proxies, and they are thrown away if underlying geometry changes.
	TManagedArray<FGeometryCollectionSection>& InputSections = Component->GetRestCollection()->GetGeometryCollection()->Sections;

	const int32 NumSections = InputSections.Num();
	Sections.Reset(NumSections);

	for (int SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		FGeometryCollectionSection& Section = InputSections[SectionIndex];
		
		if (Section.NumTriangles > 0)
		{
			Sections.Add(Section);
		}
	}

	EnableGPUSceneSupportFlags();

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	// Init HitProxy array with the maximum number of subsections
	SubSectionHitProxies.SetNumZeroed(Sections.Num() * Component->GetTransformArray().Num());
#endif

	// #todo(dmp): This flag means that when motion blur is turned on, it will always render geometry collections into the
	// velocity buffer.  Note that the way around this is to loop through the global matrices and test whether they have
	// changed from the prev to curr frame, but this is expensive.  We should revisit this if the draw calls for velocity
	// rendering become a problem. One solution could be to use internal solver sleeping state to drive motion blur.
	bAlwaysHasVelocity = true;

	// Build pre-skinned bounds from the rest collection, never needs to change as this is the bounds before
	// any movement, or skinning ever happens to the component so it is logically immutable.
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>            
										Collection     = Component->RestCollection->GetGeometryCollection();
	const TManagedArray<FBox>&			BoundingBoxes  = Collection->BoundingBox;
	const TManagedArray<FTransform>&	Transform      = Collection->Transform;
	const TManagedArray<int32>&			Parent         = Collection->Parent;
	const TManagedArray<int32>&			TransformIndex = Collection->TransformIndex;

	const int32 NumBoxes = BoundingBoxes.Num();
	PreSkinnedBounds = Component->Bounds;

	if(NumBoxes > 0)
	{
		TArray<FMatrix> TmpGlobalMatrices;
		GeometryCollectionAlgo::GlobalMatrices(Transform, Parent, TmpGlobalMatrices);

		FBox PreSkinnedBoundsTemp(ForceInit);
		bool bBoundsInit = false;
		for(int32 BoxIdx = 0; BoxIdx < NumBoxes; ++BoxIdx)
		{
			const int32 TIndex = TransformIndex[BoxIdx];
			if(Collection->IsGeometry(TIndex))
			{
				if(!bBoundsInit)
				{
					PreSkinnedBoundsTemp = BoundingBoxes[BoxIdx].TransformBy(TmpGlobalMatrices[TIndex]);
					bBoundsInit = true;
				}
				else
				{
					PreSkinnedBoundsTemp += BoundingBoxes[BoxIdx].TransformBy(TmpGlobalMatrices[TIndex]);
				}
			}
		}

		PreSkinnedBounds = FBoxSphereBounds(PreSkinnedBoundsTemp);
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{

		ENQUEUE_RENDER_COMMAND(InitGeometryCollectionRayTracingGeometry)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				FRayTracingGeometryInitializer Initializer;
				Initializer.DebugName = FName(TEXT("GeometryCollection"));
				Initializer.GeometryType = RTGT_Triangles;
				Initializer.bFastBuild = true;
				Initializer.bAllowUpdate = false;
				Initializer.TotalPrimitiveCount = 0;

				RayTracingGeometry.SetInitializer(Initializer);
				RayTracingGeometry.InitResource();
			});
	}
#endif
}

FGeometryCollectionSceneProxy::~FGeometryCollectionSceneProxy()
{
}

void FGeometryCollectionSceneProxy::DestroyRenderThreadResources()
{
	ReleaseResources();

	if (DynamicData != nullptr)
	{
		GDynamicDataPool.Release(DynamicData);
		DynamicData = nullptr;
	}

	if (ConstantData != nullptr)
	{
		delete ConstantData;
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		RayTracingGeometry.ReleaseResource();
		RayTracingDynamicVertexBuffer.Release();
	}
#endif
}

void UpdateLooseParameter(FGeometryCollectionVertexFactory& VertexFactory,
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

void FGeometryCollectionSceneProxy::SetupVertexFactory(FGeometryCollectionVertexFactory& GeometryCollectionVertexFactory)const
{
	FGeometryCollectionVertexFactory::FDataType Data;
	
	VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&GeometryCollectionVertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&GeometryCollectionVertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&GeometryCollectionVertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&GeometryCollectionVertexFactory, Data, 0);
	VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&GeometryCollectionVertexFactory, Data);

	Data.BoneMapSRV = BoneMapBuffer.VertexBufferSRV;
	Data.BoneTransformSRV = TransformBuffers[CurrentTransformBufferIndex].VertexBufferSRV;
	Data.BonePrevTransformSRV = PrevTransformBuffers[CurrentTransformBufferIndex].VertexBufferSRV;

	GeometryCollectionVertexFactory.SetData(Data);

	if (!GeometryCollectionVertexFactory.IsInitialized())
	{
		GeometryCollectionVertexFactory.InitResource();
	}
	else
	{
		GeometryCollectionVertexFactory.UpdateRHI();
	}
}

void FGeometryCollectionSceneProxy::InitResources()
{
	check(ConstantData);
	check(IsInRenderingThread());
	
	NumVertices = ConstantData->Vertices.Num();
	NumIndices = ConstantData->Indices.Num()*3;	

	// taken from this, and expanded here to accommodate modifications for
	// GeometryCollection vertex factory data (transform and bonemap)
	// VertexBuffers.InitWithDummyData(&VertexFactory, GetRequiredVertexCount());

	// get vertex factory data
	FGeometryCollectionVertexFactory::FDataType Data;

	VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
	
	// Init buffers
	VertexBuffers.PositionVertexBuffer.Init(NumVertices);
	VertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, GeometryCollectionUV::MAX_NUM_UV_CHANNELS);
	VertexBuffers.ColorVertexBuffer.Init(NumVertices);

	// Init resources
	VertexBuffers.PositionVertexBuffer.InitResource();
	VertexBuffers.StaticMeshVertexBuffer.InitResource();
	VertexBuffers.ColorVertexBuffer.InitResource();

	// Bind buffers
	VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&VertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, Data);
	VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&VertexFactory, Data);	
	VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&VertexFactory, Data, 0);
	VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&VertexFactory, Data);

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	// Note: Could skip this when bEnableBoneSelection is false if the hitproxy shader was made to not require per-vertex hit proxy IDs in that case
	{
		HitProxyIdBuffer.Init(NumVertices);
		HitProxyIdBuffer.InitResource();
	}
#endif

	IndexBuffer.NumIndices = GetRequiredIndexCount();
	IndexBuffer.InitResource();
		
	OriginalMeshIndexBuffer.NumIndices = GetRequiredIndexCount();
	OriginalMeshIndexBuffer.InitResource();	
	
	// If using manual vertex fetch, then we will setup the GPU point transform implementation
	if (bSupportsManualVertexFetch)
	{
		BoneMapBuffer.NumVertices = NumVertices;

		TransformBuffers.AddDefaulted(1);
		PrevTransformBuffers.AddDefaulted(1);

		TransformBuffers[0].NumTransforms = ConstantData->NumTransforms;
		PrevTransformBuffers[0].NumTransforms = ConstantData->NumTransforms;
		TransformBuffers[0].InitResource();
		PrevTransformBuffers[0].InitResource();

		BoneMapBuffer.InitResource();

		Data.BoneMapSRV = BoneMapBuffer.VertexBufferSRV;
		Data.BoneTransformSRV = TransformBuffers[0].VertexBufferSRV;
		Data.BonePrevTransformSRV = PrevTransformBuffers[0].VertexBufferSRV;
	}
	else
	{
		// make sure these are not null to pass UB validation
		Data.BoneMapSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.BoneTransformSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.BonePrevTransformSRV = GNullColorVertexBuffer.VertexBufferSRV;
	}

	// 
	// from InitOrUpdateResource(VertexFactory);
	//

	// also make sure to do the binding to the vertex factory
	VertexFactory.SetData(Data);

	if (!VertexFactory.IsInitialized())
	{
		VertexFactory.InitResource();
	}
	else
	{
		VertexFactory.UpdateRHI();
	}
}

void FGeometryCollectionSceneProxy::ReleaseResources()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();

	OriginalMeshIndexBuffer.ReleaseResource();

	if (bSupportsManualVertexFetch)
	{
		BoneMapBuffer.ReleaseResource();

		for (int32 i = 0; i < TransformBuffers.Num(); i++)
		{
			TransformBuffers[i].ReleaseResource();
			PrevTransformBuffers[i].ReleaseResource();
		}
		TransformBuffers.Reset();
	}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	HitProxyIdBuffer.ReleaseResource();
#endif

	VertexFactory.ReleaseResource();
}

void FGeometryCollectionSceneProxy::BuildGeometry( const FGeometryCollectionConstantData* ConstantDataIn, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, TArray<int32> &OutOriginalMeshIndices)
{
	OutVertices.SetNumUninitialized(ConstantDataIn->Vertices.Num());
	ParallelFor(ConstantData->Vertices.Num(), [&](int32 PointIdx)
	{
		OutVertices[PointIdx] =
			FDynamicMeshVertex(
				ConstantDataIn->Vertices[PointIdx],
				ConstantDataIn->UVs[PointIdx][0],
				(bShowBoneColors||bEnableBoneSelection) 
					? bShowBoneColors 
						? ConstantDataIn->BoneColors[PointIdx].ToFColor(true)
						: (ConstantDataIn->BoneColors[PointIdx] * ConstantDataIn->Colors[PointIdx]).ToFColor(true)
					: ConstantDataIn->Colors[PointIdx].ToFColor(true)
			);
		OutVertices[PointIdx].SetTangents(ConstantDataIn->TangentU[PointIdx], ConstantDataIn->TangentV[PointIdx], ConstantDataIn->Normals[PointIdx]);

		if (ConstantDataIn->UVs[PointIdx].Num() > 1)
		{
			for (int32 UVLayerIdx = 1; UVLayerIdx < ConstantDataIn->UVs[PointIdx].Num(); ++UVLayerIdx)
			{
				OutVertices[PointIdx].TextureCoordinate[UVLayerIdx] = ConstantDataIn->UVs[PointIdx][UVLayerIdx];
			}
		}
	});

	check(ConstantDataIn->Indices.Num() * 3 == NumIndices);

	OutIndices.SetNumUninitialized(NumIndices);
	ParallelFor(ConstantDataIn->Indices.Num(), [&](int32 IndexIdx)
	{
		OutIndices[IndexIdx * 3 ]    = ConstantDataIn->Indices[IndexIdx].X;
		OutIndices[IndexIdx * 3 + 1] = ConstantDataIn->Indices[IndexIdx].Y;
		OutIndices[IndexIdx * 3 + 2] = ConstantDataIn->Indices[IndexIdx].Z;
	});
	
	OutOriginalMeshIndices.SetNumUninitialized(ConstantDataIn->OriginalMeshIndices.Num() * 3);
	ParallelFor(ConstantDataIn->OriginalMeshIndices.Num(), [&](int32 IndexIdx)
	{
		OutOriginalMeshIndices[IndexIdx * 3] = ConstantDataIn->OriginalMeshIndices[IndexIdx].X;
		OutOriginalMeshIndices[IndexIdx * 3 + 1] = ConstantDataIn->OriginalMeshIndices[IndexIdx].Y;
		OutOriginalMeshIndices[IndexIdx * 3 + 2] = ConstantDataIn->OriginalMeshIndices[IndexIdx].Z;
	});
}

void FGeometryCollectionSceneProxy::SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData, bool ForceInit)
{
	check(IsInRenderingThread());
	check(NewConstantData);

	if (ConstantData)
	{
		delete ConstantData;
		ConstantData = nullptr;
	}
	ConstantData = NewConstantData;

	if (ConstantData->Vertices.Num() != VertexBuffers.PositionVertexBuffer.GetNumVertices() || ForceInit)
	{
		ReleaseResources();
		InitResources();
	}

	TArray<int32> Indices;
	TArray<int32> OriginalMeshIndices;
	TArray<FDynamicMeshVertex> Vertices;
	BuildGeometry(ConstantData, Vertices, Indices, OriginalMeshIndices);
	check(Vertices.Num() == GetRequiredVertexCount());
	check(Indices.Num() == GetRequiredIndexCount());

	if (GetRequiredVertexCount())
	{
		ParallelFor(Vertices.Num(), [&](int32 i)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector3f(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector3f());
			for (int UVChannelIndex = 0; UVChannelIndex < GeometryCollectionUV::MAX_NUM_UV_CHANNELS; UVChannelIndex++)
			{
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, UVChannelIndex, Vertex.TextureCoordinate[UVChannelIndex]);
			}
			VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
			if (bEnableBoneSelection && PerBoneHitProxies.Num())
			{
				// One proxy per bone
				const int32 ProxyIndex = ConstantData->BoneMap[i];
				HitProxyIdBuffer.VertexColor(i) = PerBoneHitProxies[ProxyIndex]->Id.GetColor();
			}
			else
			{
				HitProxyIdBuffer.VertexColor(i) = WholeObjectHitProxyColor;
			}
#endif
		});

		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.ColorVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
		}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		// Note: Could skip this when bEnableBoneSelection is false if the hitproxy shader was made to not require per-vertex hit proxy IDs in that case
		{
			auto& VertexBuffer = HitProxyIdBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
		}
#endif

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			RHIUnlockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
			RHIUnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
		}

		{
			void* IndexBufferData = RHILockBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
			RHIUnlockBuffer(IndexBuffer.IndexBufferRHI);
		}

		{							
			void* OriginalMeshIndexBufferData = RHILockBuffer(OriginalMeshIndexBuffer.IndexBufferRHI, 0, OriginalMeshIndices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(OriginalMeshIndexBufferData, &OriginalMeshIndices[0], OriginalMeshIndices.Num() * sizeof(int32));
			RHIUnlockBuffer(OriginalMeshIndexBuffer.IndexBufferRHI);
		}

		// If we are using the GeometryCollection vertex factory, populate the vertex buffer for bone map
		if (bSupportsManualVertexFetch)
		{
			void* BoneMapBufferData = RHILockBuffer(BoneMapBuffer.VertexBufferRHI, 0, Vertices.Num() * sizeof(int32), RLM_WriteOnly);								
			FMemory::Memcpy(BoneMapBufferData, &ConstantData->BoneMap[0], ConstantData->BoneMap.Num() * sizeof(int32));
			RHIUnlockBuffer(BoneMapBuffer.VertexBufferRHI);

			// In order to use loose parameter to support raytracing, we need to initialize the transform/pretransform buffer
			// before it's set up in the dynamic path. Otherwise, the transformation matrix will be all zero instead of identity. 
			// Then, nothing will be drawn
			const bool bLocalGeometryCollectionTripleBufferUploads = (GGeometryCollectionTripleBufferUploads != 0) && bSupportsTripleBufferVertexUpload;
			const EResourceLockMode LockMode = bLocalGeometryCollectionTripleBufferUploads ? RLM_WriteOnly_NoOverwrite : RLM_WriteOnly;

			FGeometryCollectionTransformBuffer& TransformBuffer = GetCurrentTransformBuffer();
			FGeometryCollectionTransformBuffer& PrevTransformBuffer = GetCurrentPrevTransformBuffer();

			
			// if we are rendering the base mesh geometry, then use rest transforms rather than the simulated one for both current and previous transforms
			TransformBuffer.UpdateDynamicData(ConstantData->RestTransforms, LockMode);
			PrevTransformBuffer.UpdateDynamicData(ConstantData->RestTransforms, LockMode);

		}

		// Update mesh sections
		Sections.Reset(ConstantData->Sections.Num());
		// #todo(dmp): We should restructure the component/SceneProxy usage to avoid this messy stuff.  We need to know the sections
		// when we create the sceneproxy for the hit proxy to work, but then we are updating the sections here with potentially differing
		// vertex counts due to hiding geometry.  Ideally, the SceneProxy is treated as const and recreated whenever the geometry
		// changes rather than this.  SetConstantData_RenderThread should be done in the constructor for the sceneproxy, most likely
		for (FGeometryCollectionSection Section : ConstantData->Sections)
		{
			if (Section.NumTriangles > 0)
			{
				FGeometryCollectionSection& NewSection = Sections.AddDefaulted_GetRef();
				NewSection.MaterialID = Section.MaterialID;
				NewSection.FirstIndex = Section.FirstIndex;
				NewSection.NumTriangles = Section.NumTriangles;
				NewSection.MinVertexIndex = Section.MinVertexIndex;
				NewSection.MaxVertexIndex = Section.MaxVertexIndex;				
			}
		}

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		// Recreate or release subsections as needed
		if (bUsesSubSections)
		{
			InitializeSubSections_RenderThread();
		}
		else
		{
			ReleaseSubSections_RenderThread();
		}
	}
	else
	{
		ReleaseSubSections_RenderThread();
#endif
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		bGeometryResourceUpdated = true;
	}
#endif

}

void FGeometryCollectionSceneProxy::SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData)
{
	check(IsInRenderingThread());
	if (GetRequiredVertexCount())
	{
		if (DynamicData)
		{
			GDynamicDataPool.Release(DynamicData);
			DynamicData = nullptr;
		}
		DynamicData = NewDynamicData;
		
		check(VertexBuffers.PositionVertexBuffer.GetNumVertices() == (uint32)ConstantData->Vertices.Num());

		if (bSupportsManualVertexFetch)
		{
			const bool bLocalGeometryCollectionTripleBufferUploads = (GGeometryCollectionTripleBufferUploads != 0) && bSupportsTripleBufferVertexUpload;

			if (bLocalGeometryCollectionTripleBufferUploads && TransformBuffers.Num() == 1)
			{
				TransformBuffers.AddDefaulted(2);
				PrevTransformBuffers.AddDefaulted(2);

				for (int32 i = 1; i < 3; i++)
				{
					TransformBuffers[i].NumTransforms = ConstantData->NumTransforms;
					PrevTransformBuffers[i].NumTransforms = ConstantData->NumTransforms;
					TransformBuffers[i].InitResource();
					PrevTransformBuffers[i].InitResource();
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

				if (DynamicData->IsDynamic)
				{
					TransformBuffer.UpdateDynamicData(DynamicData->Transforms, LockMode);
					PrevTransformBuffer.UpdateDynamicData(DynamicData->PrevTransforms, LockMode);

					TransformVertexBuffersContainsOriginalMesh = false;
				}
				else if (!TransformVertexBuffersContainsOriginalMesh)
				{
					// if we are rendering the base mesh geometry, then use rest transforms rather than the simulated one for both current and previous transforms
					TransformBuffer.UpdateDynamicData(ConstantData->RestTransforms, LockMode);
					PrevTransformBuffer.UpdateDynamicData(ConstantData->RestTransforms, LockMode);

					TransformVertexBuffersContainsOriginalMesh = true;
				}

				UpdateLooseParameter(VertexFactory, TransformBuffer.VertexBufferSRV, PrevTransformBuffer.VertexBufferSRV, BoneMapBuffer.VertexBufferSRV);
			}
		}
		else
		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);

			bool bParallelGeometryCollection = true;
			int32 TotalVertices = ConstantData->Vertices.Num();
			int32 ParallelGeometryCollectionBatchSize = CVarParallelGeometryCollectionBatchSize.GetValueOnRenderThread();

			int32 NumBatches = (TotalVertices / ParallelGeometryCollectionBatchSize);

			if (TotalVertices != ParallelGeometryCollectionBatchSize)
			{
				NumBatches++;
			}

			// Batch too small, don't bother with parallel
			if (ParallelGeometryCollectionBatchSize > TotalVertices)
			{
				bParallelGeometryCollection = false;
				ParallelGeometryCollectionBatchSize = TotalVertices;
			}

			auto GeometryCollectionBatch([&](int32 BatchNum)
			{
				int32 IndexOffset = ParallelGeometryCollectionBatchSize * BatchNum;
				int32 ThisBatchSize = ParallelGeometryCollectionBatchSize;

				// Check for final batch
				if (IndexOffset + ParallelGeometryCollectionBatchSize > NumVertices)
				{
					ThisBatchSize = TotalVertices - IndexOffset;
				}

				if (ThisBatchSize > 0)
				{
					const FMatrix44f* RESTRICT BoneTransformsPtr = DynamicData->IsDynamic ? DynamicData->Transforms.GetData() : ConstantData->RestTransforms.GetData();

					if (bGeometryCollection_SetDynamicData_ISPC_Enabled)
					{
#if INTEL_ISPC
						uint8* VertexBufferOffset = (uint8*)VertexBufferData + (IndexOffset * VertexBuffer.GetStride());
						ispc::SetDynamicData_RenderThread(
							(ispc::FVector3f*)VertexBufferOffset,
							ThisBatchSize,
							VertexBuffer.GetStride(),
							&ConstantData->BoneMap[IndexOffset],
							(ispc::FMatrix44f*)BoneTransformsPtr,
							(ispc::FVector3f*)&ConstantData->Vertices[IndexOffset]);
#endif
					}
					else
					{
						for (int32 i = IndexOffset; i < IndexOffset + ThisBatchSize; i++)
						{
							FVector3f Transformed = BoneTransformsPtr[ConstantData->BoneMap[i]].TransformPosition(ConstantData->Vertices[i]);
							FMemory::Memcpy((uint8*)VertexBufferData + (i * VertexBuffer.GetStride()), &Transformed, sizeof(FVector3f));
						}
					}
				}
			});

			ParallelFor(NumBatches, GeometryCollectionBatch, !bParallelGeometryCollection);

			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
		}

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			bGeometryResourceUpdated = true;
		}
#endif
	}
}

FMaterialRenderProxy* FGeometryCollectionSceneProxy::GetMaterial(FMeshElementCollector& Collector, int32 MaterialIndex) const
{
	// material for wireframe
	/*
	never used
	auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : nullptr,
		FLinearColor(0, 0.5f, 1.f)
	);
	Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	*/

	// material for colored bones

	FMaterialRenderProxy* MaterialProxy = nullptr;

	if (bShowBoneColors && GEngine->VertexColorMaterial)
	{
		UMaterial* VertexColorVisualizationMaterial = GEngine->VertexColorMaterial;
		auto VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
			VertexColorVisualizationMaterial->GetRenderProxy(),
			GetSelectionColor(FLinearColor::White, false, false)
		);
		Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
		MaterialProxy = VertexColorVisualizationMaterialInstance;
	}
	else
	{
		MaterialProxy = Materials[MaterialIndex]->GetRenderProxy();
	}

	if (MaterialProxy == nullptr)
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	}

	return MaterialProxy;
}

void FGeometryCollectionSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GeometryCollectionSceneProxy_GetDynamicMeshElements);
	if (GetRequiredVertexCount())
	{
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		const bool bProxyIsSelected = IsSelected();

		const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;

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
			if ((VisibilityMap & (1 << ViewIndex)) == 0) { continue; }

			// Render Batches
						
			// render original mesh if it isn't dynamic and there is an unfractured mesh
			// #todo(dmp): refactor this to share more code later
			const bool bIsDynamic = DynamicData && DynamicData->IsDynamic;
			if (!bIsDynamic)
			{
			#if GEOMETRYCOLLECTION_EDITOR_SELECTION
				const TArray<FGeometryCollectionSection>& SectionArray = bUsesSubSections && SubSections.Num() ? SubSections: ConstantData->OriginalMeshSections;
				UE_LOG(FGeometryCollectionSceneProxyLogging, VeryVerbose, TEXT("GetDynamicMeshElements, bUseSubSections=%d, NumSections=%d for %p."), bUsesSubSections, SectionArray.Num(), this);
			#else
				const TArray<FGeometryCollectionSection>& SectionArray = ConstantData->OriginalMeshSections;
			#endif

				// Grab the material proxies we'll be using for each section
				TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;

				for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); SectionIndex++)
				{
					const FGeometryCollectionSection& Section = SectionArray[SectionIndex];
					FMaterialRenderProxy* MaterialProxy = GetMaterial(Collector, Section.MaterialID);
					MaterialProxies.Add(MaterialProxy);
				}

				for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); SectionIndex++)
				{
					const FGeometryCollectionSection& Section = SectionArray[SectionIndex];

					// Draw the mesh.
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &OriginalMeshIndexBuffer;
					Mesh.bWireframe = bWireframe;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MaterialProxies[SectionIndex];

					/*
					bool bHasPrecomputedVolumetricLightmap;
					FMatrix PreviousLocalToWorld;
					int32 SingleCaptureIndex;
					bool bOutputVelocity;
					GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
					bOutputVelocity |= AlwaysHasVelocity();

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
					*/

					BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
					BatchElement.FirstIndex = Section.FirstIndex;
					BatchElement.NumPrimitives = Section.NumTriangles;
					BatchElement.MinVertexIndex = Section.MinVertexIndex;
					BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = true;
				#if WITH_EDITOR
					if (GIsEditor)
					{
						Mesh.BatchHitProxyId = Section.HitProxy ? Section.HitProxy->Id : FHitProxyId();
					}
				#endif

					SetDebugMaterial(Mesh);

					Collector.AddMesh(ViewIndex, Mesh);
				}
			}
			else
			{
			#if GEOMETRYCOLLECTION_EDITOR_SELECTION
				const TArray<FGeometryCollectionSection>& SectionArray = bUsesSubSections && SubSections.Num() ? SubSections: Sections;
				UE_LOG(FGeometryCollectionSceneProxyLogging, VeryVerbose, TEXT("GetDynamicMeshElements, bUseSubSections=%d, NumSections=%d for %p."), bUsesSubSections, SectionArray.Num(), this);
			#else
				const TArray<FGeometryCollectionSection>& SectionArray = Sections;
			#endif

				// Grab the material proxies we'll be using for each section
				TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;

				for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
				{
					const FGeometryCollectionSection& Section = SectionArray[SectionIndex];

					FMaterialRenderProxy* MaterialProxy = GetMaterial(Collector, Section.MaterialID);
					MaterialProxies.Add(MaterialProxy);
				}

				for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
				{
					const FGeometryCollectionSection& Section = SectionArray[SectionIndex];

					// Draw the mesh.
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = bWireframe;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MaterialProxies[SectionIndex];
					BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
					BatchElement.FirstIndex = Section.FirstIndex;
					BatchElement.NumPrimitives = Section.NumTriangles;
					BatchElement.MinVertexIndex = Section.MinVertexIndex;
					BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = true;
				#if WITH_EDITOR
					if (GIsEditor)
					{
						Mesh.BatchHitProxyId = Section.HitProxy ? Section.HitProxy->Id : FHitProxyId();
					}
				#endif

					SetDebugMaterial(Mesh);

					Collector.AddMesh(ViewIndex, Mesh);
				}
			}

			// Highlight selected bone using specialized material - when rendering bones as colors we don't need to run this code as the
			// bone selection is already contained in the rendered colors
			// #note: This renders the geometry again but with the bone selection material.  Ideally we'd have one render pass and one
			// material.
			if ((bShowBoneColors || bEnableBoneSelection) && !bSuppressSelectionMaterial && Materials.IsValidIndex(BoneSelectionMaterialID))
			{
				FMaterialRenderProxy* MaterialRenderProxy = Materials[BoneSelectionMaterialID]->GetRenderProxy();

				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialRenderProxy;
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = GetRequiredIndexCount() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = GetRequiredVertexCount();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		#endif
		}
	}
}

#if RHI_RAYTRACING
void FGeometryCollectionSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<struct FRayTracingInstance>& OutRayTracingInstances)
{
	if (GRayTracingGeometryCollectionProxyMeshes == 0)
	{
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_GeometryCollectionSceneProxy_GetDynamicRayTracingInstances);

	if (GetRequiredVertexCount())
	{
		const uint32 LODIndex = 0;
		const bool bWireframe = false; //AllowDebugViewmodes();//&& ViewFamily.EngineShowFlags.Wireframe;
		
		// render original mesh if it isn't dynamic and there is an unfractured mesh
		// #todo(dmp): refactor this to share more code later
		const bool bIsDynamic = DynamicData && DynamicData->IsDynamic;

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

		SetupVertexFactory(GeometryCollectionVertexFactory);

		FGeometryCollectionIndexBuffer* ActiveIndexBuffer = bIsDynamic ? &IndexBuffer : &OriginalMeshIndexBuffer;
		UpdatingRayTracingGeometry_RenderingThread(ActiveIndexBuffer);

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		const TArray<FGeometryCollectionSection>& SectionArray = bUsesSubSections && SubSections.Num() ? SubSections : Sections;
		UE_LOG(FGeometryCollectionSceneProxyLogging, VeryVerbose, TEXT("GetDynamicMeshElements, bUseSubSections=%d, NumSections=%d for %p."), bUsesSubSections, SectionArray.Num(), this);
#else
		const TArray<FGeometryCollectionSection>& SectionArray = Sections;
#endif
		const int32 InstanceCount = SectionArray.Num();

		if (InstanceCount > 0 && RayTracingGeometry.RayTracingGeometryRHI.IsValid())
		{

			FRayTracingInstance RayTracingInstance;
			RayTracingInstance.Geometry = &RayTracingGeometry;
			RayTracingInstance.InstanceTransforms.Emplace(GetLocalToWorld());

			// Grab the material proxies we'll be using for each section
			TArray<FMaterialRenderProxy*, TInlineAllocator<32>> MaterialProxies;

			for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
			{
				const FGeometryCollectionSection& Section = SectionArray[SectionIndex];
				
				//TODO: Add BoneColor support in Path/Ray tracing?
				FMaterialRenderProxy* MaterialProxy= Materials[Section.MaterialID]->GetRenderProxy();

				if (MaterialProxy == nullptr)
				{
					MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
				}

				MaterialProxies.Add(MaterialProxy);
			}

			int32 MaxVertexIndex = 0;
			for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
			{
				const FGeometryCollectionSection& Section = SectionArray[SectionIndex];

				// Draw the mesh

				FMeshBatch& Mesh = RayTracingInstance.Materials.AddDefaulted_GetRef();
				Mesh.bWireframe = bWireframe;//bWireframe needs viewfamily access ?
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
				BatchElement.IndexBuffer = ActiveIndexBuffer;
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
				BatchElement.FirstIndex = Section.FirstIndex;
				BatchElement.NumPrimitives = Section.NumTriangles;
				BatchElement.MinVertexIndex = Section.MinVertexIndex;
				BatchElement.MaxVertexIndex = Section.MaxVertexIndex; 
				BatchElement.NumInstances = 1;

				MaxVertexIndex = std::max(Section.MaxVertexIndex, MaxVertexIndex);
#if WITH_EDITOR
				if (GIsEditor)
				{
					Mesh.BatchHitProxyId = Section.HitProxy ? Section.HitProxy->Id : FHitProxyId();
				}
#endif
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

			RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel());

			OutRayTracingInstances.Emplace(RayTracingInstance);
		}

	}

}

void FGeometryCollectionSceneProxy::UpdatingRayTracingGeometry_RenderingThread(FGeometryCollectionIndexBuffer* InIndexBuffer)
{
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	const TArray<FGeometryCollectionSection>& SectionArray = bUsesSubSections && SubSections.Num() ? SubSections : Sections;
	UE_LOG(FGeometryCollectionSceneProxyLogging, VeryVerbose, TEXT("GetDynamicMeshElements, bUseSubSections=%d, NumSections=%d for %p."), bUsesSubSections, SectionArray.Num(), this);
#else
	const TArray<FGeometryCollectionSection>& SectionArray = Sections;
#endif
	const int32 InstanceCount = SectionArray.Num();//InstanceSceneData.Num();

	if (InIndexBuffer && bGeometryResourceUpdated)
	{
		RayTracingGeometry.Initializer.Segments.Empty();
		RayTracingGeometry.Initializer.TotalPrimitiveCount = 0;

		for (int SectionIndex = 0; SectionIndex < InstanceCount; ++SectionIndex)
		{
			const FGeometryCollectionSection& Section = SectionArray[SectionIndex];
			FRayTracingGeometrySegment Segment;
			Segment.FirstPrimitive = Section.FirstIndex / 3;
			Segment.VertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = Section.NumTriangles;
			Segment.MaxVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
			RayTracingGeometry.Initializer.Segments.Add(Segment);
			RayTracingGeometry.Initializer.TotalPrimitiveCount += Section.NumTriangles;
		}
				
		if (RayTracingGeometry.Initializer.TotalPrimitiveCount > 0)
		{
			RayTracingGeometry.Initializer.IndexBuffer = InIndexBuffer->IndexBufferRHI;
			// Create the ray tracing geometry but delay the acceleration structure build.
			RayTracingGeometry.CreateRayTracingGeometry(ERTAccelerationStructureBuildPriority::Skip);
		}

		bGeometryResourceUpdated = false;
	}
}

#endif

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

#if WITH_EDITOR
HHitProxy* FGeometryCollectionSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	// In order to be able to click on static meshes when they're batched up, we need to have catch all default
	// hit proxy to return.
	HHitProxy* DefaultHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
	WholeObjectHitProxyColor = DefaultHitProxy->Id.GetColor();

	// @todo FractureTools - Reconcile with subsection hit proxies.  Subsection is a draw call per hit proxy but is not suitable per-vertex as written
	if (bEnableBoneSelection)
	{
		UGeometryCollectionComponent* GeometryCollectionComp = CastChecked<UGeometryCollectionComponent>(Component);
		int32 NumTransforms = GeometryCollectionComp->GetTransformArray().Num();
		PerBoneHitProxies.Empty();
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			HGeometryCollectionBone* HitProxy = new HGeometryCollectionBone(GeometryCollectionComp, TransformIndex);
			PerBoneHitProxies.Add(HitProxy);
		}

		OutHitProxies.Append(PerBoneHitProxies);
	}
	else if (Component->GetOwner())
	{
#if GEOMETRYCOLLECTION_EDITOR_SELECTION
		// Note the below-created subsection hitproxies are never drawn, because the per-vertex hitproxy 
		// rendering path is always on for geometry collections (i.e., USE_PER_VERTEX_HITPROXY_ID is 1)
		const int32 NumTransforms = (Sections.Num() > 0) ? SubSectionHitProxies.Num() / Sections.Num(): 0;
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			// Create HitProxy for regular material based sections, and update existing section
			FGeometryCollectionSection& Section = Sections[SectionIndex];

			const int32 MaterialID = Section.MaterialID;
			HActor* const HitProxy = new HActor(Component->GetOwner(), Component, SectionIndex, MaterialID);

			OutHitProxies.Add(HitProxy);
			Section.HitProxy = HitProxy;

			// Create HitProxy per transform index using the same material Id than the current sections
			// All combinations of material id/transform index are populated,
			// since it can't be assumed that any of them won't be needed.
			const int32 SectionOffset = SectionIndex * NumTransforms;

			for (int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
			{
				static const int32 SubSectionIndex = INDEX_NONE;  // The index will get updated later for existing subsections
				
				HGeometryCollection* const SubSectionHitProxy = new HGeometryCollection(Component->GetOwner(), Component, SubSectionIndex, MaterialID, TransformIndex);

				OutHitProxies.Add(SubSectionHitProxy);
				SubSectionHitProxies[SectionOffset + TransformIndex] = SubSectionHitProxy;
			}
		}
	#else
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			// Create HitProxy for regular material based sections, and update existing section
			FGeometryCollectionSection& Section = Sections[SectionIndex];

			const int32 MaterialID = Section.MaterialID;
			HActor* const HitProxy = new HActor(Component->GetOwner(), Component, SectionIndex, MaterialID);

			OutHitProxies.Add(HitProxy);
			Section.HitProxy = HitProxy;
		}
	#endif
	}

	return DefaultHitProxy;
}
#endif // WITH_EDITOR

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
void FGeometryCollectionSceneProxy::UseSubSections(bool bInUsesSubSections, bool bForceInit)
{
	if (!bForceInit)
	{
		bUsesSubSections = bInUsesSubSections;
	}
	else if (bInUsesSubSections)
	{
		FGeometryCollectionSceneProxy* GeometryCollectionSceneProxy = this;
		ENQUEUE_RENDER_COMMAND(InitializeSubSections)(
			[GeometryCollectionSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				if (GeometryCollectionSceneProxy)
				{
					GeometryCollectionSceneProxy->InitializeSubSections_RenderThread();
					GeometryCollectionSceneProxy->bUsesSubSections = true;
					UE_LOG(FGeometryCollectionSceneProxyLogging, Verbose, TEXT("UseSubSections, %d SubSections initialized for %p."), GeometryCollectionSceneProxy->SubSections.Num(), GeometryCollectionSceneProxy);
				}
			});
	}
	else
	{
		FGeometryCollectionSceneProxy* GeometryCollectionSceneProxy = this;
		ENQUEUE_RENDER_COMMAND(ReleaseSubSections)(
			[GeometryCollectionSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				if (GeometryCollectionSceneProxy)
				{
					GeometryCollectionSceneProxy->ReleaseSubSections_RenderThread();
					GeometryCollectionSceneProxy->bUsesSubSections = false;
					UE_LOG(FGeometryCollectionSceneProxyLogging, Verbose, TEXT("UseSubSections, SubSections released for %p."), GeometryCollectionSceneProxy);
				}
			});
	}
}

void FGeometryCollectionSceneProxy::InitializeSubSections_RenderThread()
{
	// Exit now if there isn't any data
	if (!ConstantData)
	{
		SubSections.Empty();
		SubSectionHitProxyIndexMap.Empty();
		return;
	}

	// Retrieve the correct arrays depending on the dynamic state
	const bool bIsDynamic = DynamicData && DynamicData->IsDynamic;
	const TArray<FGeometryCollectionSection>& SectionArray = bIsDynamic ? Sections: ConstantData->OriginalMeshSections;
	const TArray<FIntVector>& IndexArray = bIsDynamic ? ConstantData->Indices: ConstantData->OriginalMeshIndices;
	const TArray<int32>& BoneMap = ConstantData->BoneMap;

	// Reserve sub sections array with a minimum of one transform per section
	SubSections.Empty(SectionArray.Num());
	SubSectionHitProxyIndexMap.Empty(SectionArray.Num());

	// Lambda that adds a new subsection and update the HitProxy section index
	auto AddSubSection = [this, IndexArray](int32 HitProxyIndex, const FGeometryCollectionSection& Section, int32 FirstFaceIndex, int32 EndFaceIndex)
	{
		// Find the matching HitProxy for this transform/section
		HGeometryCollection* const SubSectionHitProxy = SubSectionHitProxies[HitProxyIndex];

		// Add the subsection
		FGeometryCollectionSection SubSection;
		SubSection.MaterialID = Section.MaterialID;
		SubSection.FirstIndex = FirstFaceIndex * 3;
		SubSection.NumTriangles = EndFaceIndex - FirstFaceIndex;
		{
			// Find out new min/max vertex indices
			check(SubSection.NumTriangles > 0);
			SubSection.MinVertexIndex = TNumericLimits<int32>::Max();
			SubSection.MaxVertexIndex = TNumericLimits<int32>::Min();
			for (int32 FaceIndex = FirstFaceIndex; FaceIndex < EndFaceIndex; ++FaceIndex)
			{
				SubSection.MinVertexIndex = FMath::Min(SubSection.MinVertexIndex, IndexArray[FaceIndex].GetMin());
				SubSection.MaxVertexIndex = FMath::Max(SubSection.MaxVertexIndex, IndexArray[FaceIndex].GetMax());
			}
			check(SubSection.MinVertexIndex >= Section.MinVertexIndex && SubSection.MinVertexIndex <= Section.MaxVertexIndex)
			check(SubSection.MaxVertexIndex >= Section.MinVertexIndex && SubSection.MaxVertexIndex <= Section.MaxVertexIndex)
		}
		SubSection.HitProxy = SubSectionHitProxy;
		const int32 SubSectionIndex = SubSections.Add(SubSection);

		// Keep the HitProxy index in a map in case this section's HitProxy pointer ever needs to be updated (e.g. after CreateHitProxies is called)
		SubSectionHitProxyIndexMap.Add(SubSectionIndex, HitProxyIndex);

		// Update HitProxy with this subsection index
		if (SubSectionHitProxy)
		{
			SubSectionHitProxy->SectionIndex = SubSectionIndex;
		}
	};

	// Create subsections per transform
	const int32 NumTransforms = (SectionArray.Num() > 0) ? SubSectionHitProxies.Num() / SectionArray.Num(): 0;

	for (int32 SectionIndex = 0; SectionIndex < SectionArray.Num(); ++SectionIndex)
	{
		const int32 SectionOffset = SectionIndex * NumTransforms;

		const FGeometryCollectionSection& Section = SectionArray[SectionIndex];
		check(Section.NumTriangles > 0);  // Sections are not created with zero triangles

		const int32 FirstFaceIndex = Section.FirstIndex / 3;
		const int32 EndFaceIndex = FirstFaceIndex + Section.NumTriangles;

		int32 TransformIndex = BoneMap[IndexArray[FirstFaceIndex][0]];  // Assumes one transform per triangle
		int32 FaceIndex = FirstFaceIndex;

		for (int32 NextFaceIndex = FaceIndex + 1; NextFaceIndex < EndFaceIndex; ++NextFaceIndex)
		{
			const int32 NextTransformIndex = BoneMap[IndexArray[NextFaceIndex][0]];  // Assumes one transform per triangle
			if (TransformIndex != NextTransformIndex)
			{
				// Add the current subsection
				AddSubSection(SectionOffset + TransformIndex, Section, FaceIndex, NextFaceIndex);

				// Update variables for the next subsection
				TransformIndex = NextTransformIndex;
				FaceIndex = NextFaceIndex;
			}
		}

		// Add the last remaining subsection
		AddSubSection(SectionOffset + TransformIndex, Section, FaceIndex, EndFaceIndex);
	}
}

void FGeometryCollectionSceneProxy::ReleaseSubSections_RenderThread()
{
	SubSections.Reset();
	SubSectionHitProxyIndexMap.Reset();
}

#endif  // #if GEOMETRYCOLLECTION_EDITOR_SELECTION

void FGeometryCollectionSceneProxy::GetPreSkinnedLocalBounds(FBoxSphereBounds& OutBounds) const
{
	OutBounds = PreSkinnedBounds;
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

	// Should have valid Nanite data at this point.
	check(GeometryCollection->HasNaniteData());
	NaniteResourceID = GeometryCollection->GetNaniteResourceID();
	NaniteHierarchyOffset = GeometryCollection->GetNaniteHierarchyOffset();

	MaterialRelevance = Component->GetMaterialRelevance(Component->GetScene()->GetFeatureLevel());

	// Nanite supports the GPUScene instance data buffer.
	bSupportsInstanceDataBuffer = true;

	// We always have correct instance transforms, skip GPUScene updates if allowed.
	bShouldUpdateGPUSceneTransforms = (GGeometryCollectionOptimizedTransforms == 0);

	bSupportsDistanceFieldRepresentation = false;
	bSupportsMeshCardRepresentation = false;

	// Use fast path that does not update static draw lists.
	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	// Nanite always uses GPUScene, so we can skip expensive primitive uniform buffer updates.
	bVFRequiresPrimitiveUniformBuffer = false;

	// Indicates if 1 or more materials contain settings not supported by Nanite.
	bHasMaterialErrors = false;

	bHasPerInstanceHierarchyOffset = true;
	bHasPerInstanceLocalBounds = true;
	bHasPerInstanceDynamicData = true;

	// Check if the assigned material can be rendered in Nanite. If not, default.
	// TODO: Handle cases like geometry collections adding a "selected geometry" material with translucency.
	const bool IsRenderable = true;// Nanite::FSceneProxy::IsNaniteRenderable(MaterialRelevance);

	if (!IsRenderable)
	{
		bHasMaterialErrors = true;
	}

	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = GeometryCollection->GetGeometryCollection();
	const TManagedArray<FGeometryCollectionSection>& SectionsArray = Component->GetSectionsArray();

	MaterialSections.SetNumZeroed(SectionsArray.Num());

	for (int32 SectionIndex = 0; SectionIndex < SectionsArray.Num(); ++SectionIndex)
	{
		const FGeometryCollectionSection& MeshSection = SectionsArray[SectionIndex];
		const bool bValidMeshSection = MeshSection.MaterialID != INDEX_NONE;

		// Keep track of highest observed material index.
		MaterialMaxIndex = FMath::Max(MeshSection.MaterialID, MaterialMaxIndex);

		UMaterialInterface* MaterialInterface = bValidMeshSection ? Component->GetMaterial(MeshSection.MaterialID) : nullptr;

		// TODO: PROG_RASTER (Implement programmable raster support)
		const bool bInvalidMaterial = !MaterialInterface || MaterialInterface->GetBlendMode() != BLEND_Opaque;
		if (bInvalidMaterial)
		{
			bHasMaterialErrors = true;
			if (MaterialInterface)
			{
				UE_LOG
				(
					LogStaticMesh, Warning,
					TEXT("Invalid material [%s] used on Nanite geometry collection [%s] - forcing default material instead. Only opaque blend mode is currently supported, [%s] blend mode was specified."),
					*MaterialInterface->GetName(),
					*GeometryCollection->GetName(),
					*GetBlendModeString(MaterialInterface->GetBlendMode())
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
		check(MaterialInterface->GetBlendMode() == BLEND_Opaque);

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
		Nanite::FResources& Resource = GeometryCollection->NaniteData->NaniteResource;
		NumGeometry = Resource.HierarchyRootOffsets.Num();
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

	// Need to specify initial instance list, even with just identity transforms, so that the
	// GPUScene instance data allocator reserves space for the instances early on. The instance
	// transforms will be corrected during the first frame before any rendering occurs.
	InstanceSceneData.SetNumUninitialized(NumGeometry);
	InstanceDynamicData.SetNumUninitialized(NumGeometry);
	InstanceLocalBounds.SetNumUninitialized(NumGeometry);
	InstanceHierarchyOffset.SetNumZeroed(NumGeometry);

	for (int32 GeometryIndex = 0; GeometryIndex < NumGeometry; ++GeometryIndex)
	{
		FPrimitiveInstance& SceneData = InstanceSceneData[GeometryIndex];
		SceneData.LocalToPrimitive.SetIdentity();

		FPrimitiveInstanceDynamicData& DynamicData = InstanceDynamicData[GeometryIndex];
		DynamicData.PrevLocalToPrimitive.SetIdentity();

		InstanceLocalBounds[GeometryIndex] = FRenderBounds();
	}
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

#if WITH_EDITOR
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

void FNaniteGeometryCollectionSceneProxy::OnTransformChanged()
{
}

void FNaniteGeometryCollectionSceneProxy::GetNaniteResourceInfo(uint32& ResourceID, uint32& HierarchyOffset, uint32& ImposterIndex) const
{
	ResourceID = NaniteResourceID;
	HierarchyOffset = NaniteHierarchyOffset;
	ImposterIndex = INDEX_NONE;	// Imposters are not supported (yet?)
}

Nanite::FResourceMeshInfo FNaniteGeometryCollectionSceneProxy::GetResourceMeshInfo() const
{
	Nanite::FResources& Resource = GeometryCollection->NaniteData->NaniteResource;

	Nanite::FResourceMeshInfo OutInfo;

	OutInfo.NumClusters = Resource.NumClusters;
	OutInfo.NumNodes = Resource.NumHierarchyNodes;
	OutInfo.NumVertices = Resource.NumInputVertices;
	OutInfo.NumTriangles = Resource.NumInputTriangles;
	OutInfo.NumMaterials = MaterialMaxIndex + 1;
	OutInfo.DebugName = GeometryCollection->GetFName();

	// TODO: SegmentMapping
	OutInfo.NumSegments = 0;

	return MoveTemp(OutInfo);
}

void FNaniteGeometryCollectionSceneProxy::SetConstantData_RenderThread(FGeometryCollectionConstantData* NewConstantData, bool ForceInit)
{
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = GeometryCollection->GetGeometryCollection();
	const TManagedArray<int32>& TransformToGeometryIndices = Collection->TransformToGeometryIndex;

	const int32 TransformCount = NewConstantData->RestTransforms.Num();
	check(TransformCount == TransformToGeometryIndices.Num());
	InstanceSceneData.Reset(TransformCount);
	InstanceDynamicData.Reset(TransformCount);
	InstanceLocalBounds.Reset(TransformCount);
	InstanceHierarchyOffset.Reset(TransformCount);

	for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
	{
		const int32 TransformToGeometryIndex = TransformToGeometryIndices[TransformIndex];
		if (!Collection->IsGeometry(TransformIndex))
		{
			continue;
		}

		const FGeometryNaniteData& NaniteData = GeometryNaniteData[TransformToGeometryIndex];

		FPrimitiveInstance& Instance	= InstanceSceneData.Emplace_GetRef();
		Instance.LocalToPrimitive		= NewConstantData->RestTransforms[TransformIndex];

		FPrimitiveInstanceDynamicData& DynamicData = InstanceDynamicData.Emplace_GetRef();
		DynamicData.PrevLocalToPrimitive = Instance.LocalToPrimitive;

		InstanceLocalBounds.Emplace(NaniteData.LocalBounds);
		InstanceHierarchyOffset.Emplace(NaniteData.HierarchyOffset);
	}

	delete NewConstantData;
}

void FNaniteGeometryCollectionSceneProxy::SetDynamicData_RenderThread(FGeometryCollectionDynamicData* NewDynamicData)
{
	// Are we currently simulating?
	if (NewDynamicData->IsDynamic)
	{
		const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = GeometryCollection->GetGeometryCollection();
		const TManagedArray<int32>& TransformToGeometryIndices	 = Collection->TransformToGeometryIndex;
		const TManagedArray<TSet<int32>>& TransformChildren		 = Collection->Children;
		const TManagedArray<int32>& SimulationType				 = Collection->SimulationType;

		const int32 TransformCount = NewDynamicData->Transforms.Num();
		check(TransformCount == TransformToGeometryIndices.Num());
		check(TransformCount == TransformChildren.Num());
		check(TransformCount == NewDynamicData->PrevTransforms.Num());
		InstanceSceneData.Reset(TransformCount);
		InstanceDynamicData.Reset(TransformCount);
		InstanceLocalBounds.Reset(TransformCount);
		InstanceHierarchyOffset.Reset(TransformCount);

		for (int32 TransformIndex = 0; TransformIndex < TransformCount; ++TransformIndex)
		{
			const int32 TransformToGeometryIndex = TransformToGeometryIndices[TransformIndex];
			if (SimulationType[TransformIndex] != FGeometryCollection::ESimulationTypes::FST_Rigid)
			{
				continue;
			}

			const FGeometryNaniteData& NaniteData = GeometryNaniteData[TransformToGeometryIndex];

			FPrimitiveInstance& Instance = InstanceSceneData.Emplace_GetRef();
			Instance.LocalToPrimitive = NewDynamicData->Transforms[TransformIndex];

			FPrimitiveInstanceDynamicData& DynamicData = InstanceDynamicData.Emplace_GetRef();

			if (bCurrentlyInMotion)
			{
				DynamicData.PrevLocalToPrimitive = NewDynamicData->PrevTransforms[TransformIndex];
			}
			else
			{
				DynamicData.PrevLocalToPrimitive = Instance.LocalToPrimitive;
			}

			InstanceLocalBounds.Emplace(NaniteData.LocalBounds);
			InstanceHierarchyOffset.Emplace(NaniteData.HierarchyOffset);
		}
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
	// Reset previous transforms to avoid locked motion vectors
	check(InstanceSceneData.Num() == InstanceDynamicData.Num()); // Sanity check, should always have matching associated arrays
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); ++InstanceIndex)
	{
		InstanceDynamicData[InstanceIndex].PrevLocalToPrimitive = InstanceSceneData[InstanceIndex].LocalToPrimitive;
	}
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
		NewEntry = FreeList.Pop(false /* no shrinking */);
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
		UsedList.RemoveAt(UsedIndex, 1, false /* no shrinking */);
		FreeList.Push(DynamicData);
	}
}

void FGeometryCollectionTransformBuffer::UpdateDynamicData(const TArray<FMatrix44f>& Transforms, EResourceLockMode LockMode)
{
	check(NumTransforms == Transforms.Num());

	void* VertexBufferData = RHILockBuffer(VertexBufferRHI, 0, Transforms.Num() * sizeof(FMatrix44f), LockMode);
	FMemory::Memcpy(VertexBufferData, Transforms.GetData(), Transforms.Num() * sizeof(FMatrix44f));
	RHIUnlockBuffer(VertexBufferRHI);
}
