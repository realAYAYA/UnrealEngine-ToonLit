// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUScene.cpp
=============================================================================*/

#include "GPUScene.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "UnifiedBuffer.h"
#include "SpriteIndexBuffer.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "Rendering/NaniteResources.h"
#include "Async/ParallelFor.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "NaniteSceneProxy.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"
#include "InstanceUniformShaderParameters.h"
#include "ShaderPrint.h"

#define LOG_INSTANCE_ALLOCATIONS 0

int32 GGPUSceneUploadEveryFrame = 0;
FAutoConsoleVariableRef CVarGPUSceneUploadEveryFrame(
	TEXT("r.GPUScene.UploadEveryFrame"),
	GGPUSceneUploadEveryFrame,
	TEXT("Whether to upload the entire scene's primitive data every frame.  Useful for debugging."),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneValidatePrimitiveBuffer = 0;
FAutoConsoleVariableRef CVarGPUSceneValidatePrimitiveBuffer(
	TEXT("r.GPUScene.ValidatePrimitiveBuffer"),
	GGPUSceneValidatePrimitiveBuffer,
	TEXT("Whether to readback the GPU primitive data and assert if it doesn't match the RT primitive data.  Useful for debugging."),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneValidateInstanceBuffer = 0;
FAutoConsoleVariableRef CVarGPUSceneValidateInstanceBuffer(
	TEXT("r.GPUScene.ValidateInstanceBuffer"),
	GGPUSceneValidateInstanceBuffer,
	TEXT("Whether to readback the GPU instance data and assert if it doesn't match the RT primitive data.  Useful for debugging."),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneMaxPooledUploadBufferSize = 256000;
FAutoConsoleVariableRef CVarGGPUSceneMaxPooledUploadBufferSize(
	TEXT("r.GPUScene.MaxPooledUploadBufferSize"),
	GGPUSceneMaxPooledUploadBufferSize,
	TEXT("Maximum size of GPU Scene upload buffer size to pool."),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneParallelUpdate = 0;
FAutoConsoleVariableRef CVarGPUSceneParallelUpdate(
	TEXT("r.GPUScene.ParallelUpdate"),
	GGPUSceneParallelUpdate,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneInstanceBVH = 0;
FAutoConsoleVariableRef CVarGPUSceneInstanceBVH(
	TEXT("r.GPUScene.InstanceBVH"),
	GGPUSceneInstanceBVH,
	TEXT("Add instances to BVH. (WIP)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUSceneDebugMode(
	TEXT("r.GPUScene.DebugMode"),
	0,
	TEXT("Debug Rendering Mode:\n")
	TEXT("0 - (show nothing, decault)\n")
	TEXT(" 1 - Draw All\n")
	TEXT(" 2 - Draw Selected (in the editor)\n")
	TEXT(" 3 - Draw Updated (updated this frame)\n")
	TEXT("You can use r.GPUScene.DebugDrawRange to limit the range\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarGPUSceneDebugDrawRange(
	TEXT("r.GPUScene.DebugDrawRange"),
	-1.0f,
	TEXT("Maximum distance the to draw instance bounds, the default is -1.0 <=> infinite range."),
	ECVF_RenderThreadSafe
);

static int32 GGPUSceneAllowDeferredAllocatorMerges = 1;
FAutoConsoleVariableRef CVarGPUSceneAllowDeferredAllocatorMerges(
	TEXT("r.GPUScene.AllowDeferredAllocatorMerges"),
	GGPUSceneAllowDeferredAllocatorMerges,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GGPUSceneInstanceUploadViaCreate = 1;
FAutoConsoleVariableRef CVarGPUSceneInstanceUploadViaCreate(
	TEXT("r.GPUScene.InstanceUploadViaCreate"),
	GGPUSceneInstanceUploadViaCreate,
	TEXT("When uploading GPUScene InstanceData, upload via resource creation when the RHI supports it efficiently."),
	ECVF_RenderThreadSafe
);


LLM_DECLARE_TAG_API(GPUScene, RENDERER_API);
DECLARE_LLM_MEMORY_STAT(TEXT("GPUScene"), STAT_GPUSceneLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GPUScene"), STAT_GPUSceneSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(GPUScene, NAME_None, NAME_None, GET_STATFNAME(STAT_GPUSceneLLM), GET_STATFNAME(STAT_GPUSceneSummaryLLM));

static int32 GetMaxPrimitivesUpdate(uint32 NumUploads, uint32 InStrideInFloat4s)
{
	return FMath::Min((uint32)(GetMaxBufferDimension() / InStrideInFloat4s), NumUploads);
}

struct FParallelUpdateRange
{
	int32 ItemStart;
	int32 ItemCount;
};

struct FParallelUpdateRanges
{
	FParallelUpdateRange Range[4];
};

// TODO: Improve and move to shared utility location.
static int32 PartitionUpdateRanges(FParallelUpdateRanges& Ranges, int32 ItemCount, bool bAllowParallel)
{
	if (ItemCount < 256 || !bAllowParallel)
	{
		Ranges.Range[0].ItemStart = 0;
		Ranges.Range[0].ItemCount = ItemCount;
		return 1;
	}

	const int32 RangeCount = Align(ItemCount, 4) >> 2;

	Ranges.Range[0].ItemCount = RangeCount;
	Ranges.Range[1].ItemCount = RangeCount;
	Ranges.Range[2].ItemCount = RangeCount;

	Ranges.Range[0].ItemStart = 0;
	Ranges.Range[1].ItemStart = RangeCount;
	Ranges.Range[2].ItemStart = RangeCount * 2;
	Ranges.Range[3].ItemStart = RangeCount * 3;
	Ranges.Range[3].ItemCount = ItemCount - Ranges.Range[3].ItemStart;

	return Ranges.Range[3].ItemCount > 0 ? 4 : 3;
}

void FGPUScenePrimitiveCollector::Add(
	const FMeshBatchDynamicPrimitiveData* MeshBatchData,
	const FPrimitiveUniformShaderParameters& PrimitiveShaderParams,
	uint32 NumInstances,
	uint32& OutPrimitiveIndex,
	uint32& OutInstanceSceneDataOffset)
{
	check(GPUSceneDynamicContext != nullptr);
	check(!bCommitted);
	

	// Lazy allocation of the upload data to not waste space and processing if none was needed.
	if (UploadData == nullptr)
	{
		UploadData = AllocateUploadData();
	}

	const int32 PrimitiveIndex = UploadData->PrimitiveData.Num();
	FPrimitiveData& PrimitiveData = UploadData->PrimitiveData.AddDefaulted_GetRef();		

	if (MeshBatchData != nullptr)
	{
		// make sure the source data is appropriately structured
		MeshBatchData->Validate(NumInstances);
		PrimitiveData.SourceData = *MeshBatchData;
	}
	
	const int32 PayloadFloat4Stride = PrimitiveData.SourceData.GetPayloadFloat4Stride();
	
	PrimitiveData.ShaderParams = &PrimitiveShaderParams;
	PrimitiveData.NumInstances = NumInstances;
	PrimitiveData.LocalInstanceSceneDataOffset = UploadData->TotalInstanceCount;
	PrimitiveData.LocalPayloadDataOffset = PayloadFloat4Stride > 0 ? UploadData->InstancePayloadDataFloat4Count : INDEX_NONE;

	UploadData->TotalInstanceCount += NumInstances;	
	UploadData->InstancePayloadDataFloat4Count += PayloadFloat4Stride * NumInstances;

	if (PrimitiveData.SourceData.DataWriterGPU.IsBound())
	{
		// Enqueue this primitive data to be executed (either upon upload or deferred to a later GPU write pass)
		UploadData->GPUWritePrimitives.Add(PrimitiveIndex);
	}

	// Set the output data offsets
	OutPrimitiveIndex = PrimitiveIndex;
	OutInstanceSceneDataOffset = PrimitiveData.LocalInstanceSceneDataOffset;
}

#if DO_CHECK

bool FGPUScenePrimitiveCollector::IsPrimitiveProcessed(uint32 PrimitiveIndex, const FGPUScene& GPUScene) const
{
	if (UploadData == nullptr || !bCommitted)
	{
		// The collector hasn't collected anything or hasn't been uploaded
		return false;
	}

	if (PrimitiveIndex >= uint32(UploadData->PrimitiveData.Num()))
	{
		// The specified index is out of range
		return false;
	}

	const FMeshBatchDynamicPrimitiveData& SourceData = UploadData->PrimitiveData[PrimitiveIndex].SourceData;
	if (!SourceData.DataWriterGPU.IsBound() || SourceData.DataWriterGPUPass == EGPUSceneGPUWritePass::None)
	{
		// The primitive doesn't have a pending GPU write and has been uploaded or written to by the GPU already
		return true;
	}

	// If the GPU scene still has a pending deferred write for the primitive, then it has not been fully processed yet
	const uint32 PrimitiveId = GetPrimitiveIdRange().GetLowerBoundValue() + PrimitiveIndex;
	return !GPUScene.HasPendingGPUWrite(PrimitiveId);
}

#endif // DO_CHECK

void FGPUScenePrimitiveCollector::Commit()
{
	ensure(!bCommitted);
	if (UploadData)
	{
		PrimitiveIdRange = GPUSceneDynamicContext->GPUScene.CommitPrimitiveCollector(*this);
	}
	bCommitted = true;
}

FGPUScenePrimitiveCollector::FUploadData* FGPUScenePrimitiveCollector::AllocateUploadData()
{
	return GPUSceneDynamicContext->AllocateDynamicPrimitiveData();
}

struct FBVHNode
{
	uint32		ChildIndexes[4];
	FVector4	ChildMin[3];
	FVector4	ChildMax[3];
};

/**
 * Info needed by the uploader to prepare to upload a primitive.
 */
struct FPrimitiveUploadInfoHeader
{
	int32 PrimitiveID = INDEX_NONE;

	/** Optional */
	int32 NumInstanceUploads = 0;
	int32 NumInstancePayloadDataUploads = 0;
	int32 LightmapUploadCount = 0;

	/** NaniteSceneProxy must be set if the proxy is a Nanite proxy */
	const Nanite::FSceneProxyBase* NaniteSceneProxy = nullptr;
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
};
/**
 * Info needed by the uploader to update a primitive.
 */
struct FPrimitiveUploadInfo : public FPrimitiveUploadInfoHeader
{
	FPrimitiveSceneShaderData PrimitiveSceneData;
};
/**
 * Info required by the uploader to update the instances that belong to a primitive.
 */
struct FInstanceUploadInfo
{
	TConstArrayView<FPrimitiveInstance> PrimitiveInstances;
	int32 InstanceSceneDataOffset = INDEX_NONE;
	int32 InstancePayloadDataOffset = INDEX_NONE;
	int32 InstancePayloadDataStride = 0;
	int32 InstanceCustomDataCount = 0;

	// Optional per-instance data views
	TConstArrayView<FPrimitiveInstanceDynamicData> InstanceDynamicData;
	TConstArrayView<FVector4f> InstanceLightShadowUVBias;
	TConstArrayView<float> InstanceCustomData;
	TConstArrayView<float> InstanceRandomID;
	TConstArrayView<uint32> InstanceHierarchyOffset;
	TConstArrayView<FRenderBounds> InstanceLocalBounds;
#if WITH_EDITOR
	TConstArrayView<uint32> InstanceEditorData;
#endif

	// Used for primitives that need to create a dummy instance (they do not have instance data in the proxy)
	FPrimitiveInstance DummyInstance;
	FRenderBounds DummyLocalBounds;

	uint32 InstanceFlags = 0x0;

	FRenderTransform PrimitiveToWorld;
	FRenderTransform PrevPrimitiveToWorld;
	int32 PrimitiveID = INDEX_NONE;
	uint32 LastUpdateSceneFrameNumber = ~uint32(0);
};

void ValidateInstanceUploadInfo(const FInstanceUploadInfo& UploadInfo, FRDGBuffer* InstancePayloadDataBuffer)
{
#if DO_CHECK
	const bool bHasRandomID			= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_RANDOM) != 0u;
	const bool bHasCustomData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_CUSTOM_DATA) != 0u;
	const bool bHasDynamicData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_DYNAMIC_DATA) != 0u;
	const bool bHasLightShadowUVBias = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LIGHTSHADOW_UV_BIAS) != 0u;
	const bool bHasHierarchyOffset	= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_HIERARCHY_OFFSET) != 0u;
	const bool bHasLocalBounds		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS) != 0u;
#if WITH_EDITOR
	const bool bHasEditorData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_EDITOR_DATA) != 0u;
#endif

	const int32 InstanceCount = UploadInfo.PrimitiveInstances.Num();
	check(UploadInfo.InstanceRandomID.Num()				== (bHasRandomID		? InstanceCount : 0));
	check(UploadInfo.InstanceDynamicData.Num()			== (bHasDynamicData		? InstanceCount : 0));
	check(UploadInfo.InstanceLightShadowUVBias.Num()	== (bHasLightShadowUVBias ? InstanceCount : 0));
	check(UploadInfo.InstanceHierarchyOffset.Num()		== (bHasHierarchyOffset	? InstanceCount : 0));
#if WITH_EDITOR
	check(UploadInfo.InstanceEditorData.Num() == (bHasEditorData ? InstanceCount : 0));
#endif

	if (bHasCustomData)
	{
		check(UploadInfo.InstanceCustomDataCount > 0);
		check(UploadInfo.InstanceCustomDataCount * InstanceCount == UploadInfo.InstanceCustomData.Num());
	}
	else
	{
		check(UploadInfo.InstanceCustomData.Num() == 0 && UploadInfo.InstanceCustomDataCount == 0);
	}

	// RandomID is not stored in the payload but in the instance scene data.
	const bool bHasAnyPayloadData = bHasHierarchyOffset || bHasLocalBounds || bHasDynamicData || bHasLightShadowUVBias || bHasCustomData/*|| bHasRandomID*/;

	if (bHasAnyPayloadData)
	{
		check(InstancePayloadDataBuffer);
		check(UploadInfo.InstancePayloadDataOffset != INDEX_NONE);

		const int32 PayloadBufferSize = InstancePayloadDataBuffer->GetSize() / InstancePayloadDataBuffer->GetStride();
		check(UploadInfo.InstancePayloadDataOffset < PayloadBufferSize);
	}
#endif
}

/**
 * Info required by the uploader to update the lightmap data for a primitive.
 */
struct FLightMapUploadInfo
{
	FPrimitiveSceneProxy::FLCIArray LCIs;
	int32 LightmapDataOffset = 0;
};

// TODO: Temporary hack : For FPrimitiveSceneProxy::IsForceHidden() to work with Nanite proxies, return an invalid primitive ID if IsForceHidden() returns true.
static FORCEINLINE int32 GetPrimitiveID(const FScene& InScene, const int32 InPrimitiveID)
{
	const FPrimitiveSceneProxy* PrimitiveSceneProxy = InScene.PrimitiveSceneProxies[InPrimitiveID];
	return (PrimitiveSceneProxy->IsNaniteMesh() && PrimitiveSceneProxy->IsForceHidden()) ? INVALID_PRIMITIVE_ID : InPrimitiveID;
}

/**
 * Implements a thin data abstraction such that the UploadGeneral function can upload primitive data from
 * both scene primitives and dynamic primitives (which are not stored in the same way). 
 * Note: handling of Nanite material table upload data is not abstracted (since at present it can only come via the scene primitives).
 */
struct FUploadDataSourceAdapterScenePrimitives
{
	static constexpr bool bUpdateNaniteMaterialTables = true;

	FUploadDataSourceAdapterScenePrimitives(FScene& InScene, uint32 InSceneFrameNumber, TArray<int32> InPrimitivesToUpdate, TArray<EPrimitiveDirtyState> InPrimitiveDirtyState)
		: Scene(InScene)
		, SceneFrameNumber(InSceneFrameNumber)
		, PrimitivesToUpdate(MoveTemp(InPrimitivesToUpdate))
		, PrimitiveDirtyState(MoveTemp(InPrimitiveDirtyState))
	{}

	/**
	 * Return the number of primitives to upload N, GetPrimitiveInfo will be called with ItemIndex in [0,N).
	 */
	FORCEINLINE int32 NumPrimitivesToUpload() const 
	{ 
		return PrimitivesToUpdate.Num(); 
	}

	FORCEINLINE TArrayView<const uint32> GetItemPrimitiveIds() const
	{
		return TArrayView<const uint32>(reinterpret_cast<const uint32 *>(PrimitivesToUpdate.GetData()), PrimitivesToUpdate.Num());
	}

	/**
	 * Populate the primitive info for a given item index.
	 * 
	 */
	FORCEINLINE void GetPrimitiveInfoHeader(int32 ItemIndex, FPrimitiveUploadInfoHeader& PrimitiveUploadInfo) const
	{
		int32 PrimitiveID = PrimitivesToUpdate[ItemIndex];
		check(PrimitiveID < Scene.PrimitiveSceneProxies.Num());

		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

		PrimitiveUploadInfo.PrimitiveID = PrimitiveID;;
		PrimitiveUploadInfo.LightmapUploadCount = PrimitiveSceneInfo->GetNumLightmapDataEntries();
		PrimitiveUploadInfo.NaniteSceneProxy = PrimitiveSceneProxy->IsNaniteMesh() ? static_cast<const Nanite::FSceneProxyBase*>(PrimitiveSceneProxy) : nullptr;
		PrimitiveUploadInfo.PrimitiveSceneInfo = PrimitiveSceneInfo;

		// Prevent these from allocating instance update work
		if (PrimitiveDirtyState[PrimitiveID] == EPrimitiveDirtyState::ChangedId)
		{
			PrimitiveUploadInfo.NumInstanceUploads = 0;
			PrimitiveUploadInfo.NumInstancePayloadDataUploads = 0;
		}
		else 
		{
			PrimitiveUploadInfo.NumInstanceUploads = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
			PrimitiveUploadInfo.NumInstancePayloadDataUploads = PrimitiveSceneInfo->GetInstancePayloadDataStride() * PrimitiveUploadInfo.NumInstanceUploads;
		}
	}

	/**
	 * Populate the primitive info for a given item index.
	 * 
	 */
	FORCEINLINE void GetPrimitiveInfo(int32 ItemIndex, FPrimitiveUploadInfo& PrimitiveUploadInfo) const
	{
		int32 PrimitiveID = PrimitivesToUpdate[ItemIndex];
		check(PrimitiveID < Scene.PrimitiveSceneProxies.Num());

		GetPrimitiveInfoHeader(ItemIndex, PrimitiveUploadInfo);

		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

		PrimitiveUploadInfo.PrimitiveSceneData = FPrimitiveSceneShaderData(PrimitiveSceneProxy);
	}

	FORCEINLINE void GetInstanceInfo(int32 ItemIndex, FInstanceUploadInfo& InstanceUploadInfo) const
	{
		const int32 PrimitiveID = PrimitivesToUpdate[ItemIndex];

		check(PrimitiveID < Scene.PrimitiveSceneProxies.Num());
		check(PrimitiveDirtyState[PrimitiveID] != EPrimitiveDirtyState::ChangedId);

		FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
		
		const FMatrix LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();
		const FLargeWorldRenderPosition AbsoluteOrigin(LocalToWorld.GetOrigin());

		InstanceUploadInfo.InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
		check(InstanceUploadInfo.InstanceSceneDataOffset >= 0);
		InstanceUploadInfo.InstancePayloadDataOffset = PrimitiveSceneInfo->GetInstancePayloadDataOffset();
		InstanceUploadInfo.InstancePayloadDataStride = PrimitiveSceneInfo->GetInstancePayloadDataStride();

		InstanceUploadInfo.LastUpdateSceneFrameNumber = SceneFrameNumber;
		InstanceUploadInfo.PrimitiveID = GetPrimitiveID(Scene, PrimitiveID);
		InstanceUploadInfo.PrimitiveToWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(AbsoluteOrigin.GetTileOffset(), LocalToWorld);

		{
			bool bHasPrecomputedVolumetricLightmap{};
			bool bOutputVelocity{};
			int32 SingleCaptureIndex{};

			FMatrix PreviousLocalToWorld;
			Scene.GetPrimitiveUniformShaderParameters_RenderThread(PrimitiveSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
			InstanceUploadInfo.PrevPrimitiveToWorld = FLargeWorldRenderScalar::MakeClampedToRelativeWorldMatrix(AbsoluteOrigin.GetTileOffset(), PreviousLocalToWorld);;
		}

		InstanceUploadInfo.InstanceFlags = PrimitiveSceneProxy->GetInstanceSceneDataFlags();
		InstanceUploadInfo.InstanceLocalBounds = PrimitiveSceneProxy->GetInstanceLocalBounds();
		if (InstanceUploadInfo.InstanceLocalBounds.Num() == 0)
		{
			InstanceUploadInfo.DummyLocalBounds = PrimitiveSceneProxy->GetLocalBounds();
			InstanceUploadInfo.InstanceLocalBounds = TConstArrayView<FRenderBounds>(&InstanceUploadInfo.DummyLocalBounds, 1);
		}

		if (PrimitiveSceneProxy->SupportsInstanceDataBuffer())
		{
			InstanceUploadInfo.PrimitiveInstances = PrimitiveSceneProxy->GetInstanceSceneData();
			InstanceUploadInfo.InstanceDynamicData = PrimitiveSceneProxy->GetInstanceDynamicData();
			InstanceUploadInfo.InstanceLightShadowUVBias = PrimitiveSceneProxy->GetInstanceLightShadowUVBias();
			InstanceUploadInfo.InstanceCustomData = PrimitiveSceneProxy->GetInstanceCustomData();
			InstanceUploadInfo.InstanceRandomID = PrimitiveSceneProxy->GetInstanceRandomID();
			InstanceUploadInfo.InstanceHierarchyOffset = PrimitiveSceneProxy->GetInstanceHierarchyOffset();

#if WITH_EDITOR
			InstanceUploadInfo.InstanceEditorData = PrimitiveSceneProxy->GetInstanceEditorData();
#endif
		}
		else
		{
			checkf((InstanceUploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_PAYLOAD_MASK) == 0x0, TEXT("Proxy must support instance data buffer to use payload data"));
			check(InstanceUploadInfo.InstancePayloadDataOffset == INDEX_NONE && InstanceUploadInfo.InstancePayloadDataStride == 0);

			// We always create an instance to ensure that we can always use the same code paths in the shader
			// In the future we should remove redundant data from the primitive, and then the instances should be
			// provided by the proxy. However, this is a lot of work before we can just enable it in the base proxy class.
			InstanceUploadInfo.DummyInstance.LocalToPrimitive.SetIdentity();

			InstanceUploadInfo.PrimitiveInstances = TConstArrayView<FPrimitiveInstance>(&InstanceUploadInfo.DummyInstance, 1);
			InstanceUploadInfo.InstanceDynamicData = TConstArrayView<FPrimitiveInstanceDynamicData>();
			InstanceUploadInfo.InstanceLightShadowUVBias = TConstArrayView<FVector4f>();
			InstanceUploadInfo.InstanceCustomData = TConstArrayView<float>();
			InstanceUploadInfo.InstanceRandomID = TConstArrayView<float>();
			InstanceUploadInfo.InstanceHierarchyOffset = TConstArrayView<uint32>();
#if WITH_EDITOR
			InstanceUploadInfo.InstanceEditorData = TConstArrayView<uint32>();
#endif
		}

		InstanceUploadInfo.InstanceCustomDataCount = 0;
		if (InstanceUploadInfo.InstanceCustomData.Num() > 0)
		{
			InstanceUploadInfo.InstanceCustomDataCount = InstanceUploadInfo.InstanceCustomData.Num() / InstanceUploadInfo.PrimitiveInstances.Num();
		}

		// Only trigger upload if this primitive has instances
		check(InstanceUploadInfo.PrimitiveInstances.Num() > 0);
	}

	FORCEINLINE bool GetLightMapInfo(int32 ItemIndex, FLightMapUploadInfo &UploadInfo) const
	{
		const int32 PrimitiveID = PrimitivesToUpdate[ItemIndex];
		if (PrimitiveID < Scene.PrimitiveSceneProxies.Num())
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];

			PrimitiveSceneProxy->GetLCIs(UploadInfo.LCIs);
			check(UploadInfo.LCIs.Num() == PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetNumLightmapDataEntries());
			UploadInfo.LightmapDataOffset = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset();
			return true;
		}

		return false;
	}

	FScene& Scene;
	const uint32 SceneFrameNumber;
	TArray<int32> PrimitivesToUpdate;
	TArray<EPrimitiveDirtyState> PrimitiveDirtyState;
};

void FGPUScene::SetEnabled(ERHIFeatureLevel::Type InFeatureLevel)
{
	FeatureLevel = InFeatureLevel;
	bIsEnabled = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel);
}

FGPUScene::~FGPUScene()
{
}

void FGPUScene::BeginRender(const FScene* Scene, FGPUSceneDynamicContext &GPUSceneDynamicContext)
{
	ensure(!bInBeginEndBlock);
	ensure(CurrentDynamicContext == nullptr);
	if (Scene != nullptr)
	{
		ensure(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene->GetFeatureLevel()));
		NumScenePrimitives = Scene->Primitives.Num();
	}
	else
	{
		NumScenePrimitives = 0;
	}
	CurrentDynamicContext = &GPUSceneDynamicContext;
	DynamicPrimitivesOffset = NumScenePrimitives;
	bInBeginEndBlock = true;
	bInExternalAccessMode = false;
}

void FGPUScene::EndRender()
{
	ensure(bInBeginEndBlock);
	ensure(CurrentDynamicContext != nullptr);
	DynamicPrimitivesOffset = -1;
	bInBeginEndBlock = false;
	CurrentDynamicContext = nullptr;
	BufferState = {};
}


void FGPUScene::UpdateInternal(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue)
{
	LLM_SCOPE_BYTAG(GPUScene);

	ensure(bInBeginEndBlock);
	ensure(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	ensure(NumScenePrimitives == Scene.Primitives.Num());
	ensure(DynamicPrimitivesOffset >= Scene.Primitives.Num());

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.Update");

	LastDeferredGPUWritePass = EGPUSceneGPUWritePass::None;

	if (GGPUSceneUploadEveryFrame || bUpdateAllPrimitives)
	{
		PrimitivesToUpdate.Reset();

		for (int32 Index = 0; Index < Scene.Primitives.Num(); ++Index)
		{
			PrimitiveDirtyState[Index] |= EPrimitiveDirtyState::ChangedAll;
			PrimitivesToUpdate.Add(Index);
		}

		// Clear the full instance data range
		InstanceRangesToClear.Empty();
		InstanceRangesToClear.Add(FInstanceRange{ 0U, uint32(GetNumInstances()) });

		bUpdateAllPrimitives = false;
	}

	// Store in GPU-scene to enable validation that update has been carried out.
	SceneFrameNumber = Scene.GetFrameNumber();

	// Strip all out-of-range ID's (left over because of deletes) so we don't need to check later
	for (int32 Index = 0; Index < PrimitivesToUpdate.Num();)
	{
		if (PrimitivesToUpdate[Index] >= Scene.PrimitiveSceneProxies.Num())
		{
			PrimitivesToUpdate.RemoveAtSwap(Index, 1, false);
		}
		else
		{
			++Index;
		}
	}

	check(!BufferState.IsValid());

	FUploadDataSourceAdapterScenePrimitives& Adapter = *GraphBuilder.AllocObject<FUploadDataSourceAdapterScenePrimitives>(Scene, SceneFrameNumber, MoveTemp(PrimitivesToUpdate), MoveTemp(PrimitiveDirtyState));
	UpdateBufferState(GraphBuilder, Scene, Adapter);

	// Run a pass that clears (Sets ID to invalid) any instances that need it
	AddClearInstancesPass(GraphBuilder);

	// Pull out instances needing only primitive ID update, they still have to go to the general update such that the primitive gets updated (as it moved)
	{
		FInstanceGPULoadBalancer IdOnlyUpdateData;
		for (int32 Index = 0; Index < Adapter.PrimitivesToUpdate.Num(); ++Index)
		{
			int32 PrimitiveId = Adapter.PrimitivesToUpdate[Index];

			check(PrimitiveId < Scene.PrimitiveSceneProxies.Num());
			if (Adapter.PrimitiveDirtyState[PrimitiveId] == EPrimitiveDirtyState::ChangedId)
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene.Primitives[PrimitiveId];
				check(PrimitiveSceneInfo->GetInstanceSceneDataOffset() >= 0 || PrimitiveSceneInfo->GetNumInstanceSceneDataEntries() == 0);
				IdOnlyUpdateData.Add(PrimitiveSceneInfo->GetInstanceSceneDataOffset(), PrimitiveSceneInfo->GetNumInstanceSceneDataEntries(), GetPrimitiveID(Scene, PrimitiveId));
			}
		}
		AddUpdatePrimitiveIdsPass(GraphBuilder, IdOnlyUpdateData);
	}

	// The adapter copies the IDs of primitives to update such that any that are (incorrectly) marked for update after are not lost.
	PrimitivesToUpdate.Reset();
	PrimitiveDirtyState.Init(EPrimitiveDirtyState::None, PrimitiveDirtyState.Num());

	{
		SCOPED_NAMED_EVENT(STAT_UpdateGPUScene, FColor::Green);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateGPUScene);
		SCOPE_CYCLE_COUNTER(STAT_UpdateGPUSceneTime);

		UploadGeneral<FUploadDataSourceAdapterScenePrimitives>(GraphBuilder, Scene, ExternalAccessQueue, Adapter);
	}

	UseExternalAccessMode(ExternalAccessQueue, ERHIAccess::SRVMask, ERHIPipeline::All);
}

template<typename FUploadDataSourceAdapter>
void FGPUScene::UpdateBufferState(FRDGBuilder& GraphBuilder, FScene& Scene, const FUploadDataSourceAdapter& UploadDataSourceAdapter)
{
	LLM_SCOPE_BYTAG(GPUScene);

	ensure(bInBeginEndBlock);
	ensure(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	ensure(NumScenePrimitives == Scene.Primitives.Num());

	// Multi-GPU support : Updating on all GPUs is inefficient for AFR. Work is wasted
	// for any primitives that update on consecutive frames.
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	constexpr int32 InitialBufferSize = 256;

	const uint32 SizeReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(DynamicPrimitivesOffset, InitialBufferSize));
	BufferState.PrimitiveBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, PrimitiveBuffer, SizeReserve * sizeof(FPrimitiveSceneShaderData::Data), TEXT("GPUScene.PrimitiveData"));

	const uint32 InstanceSceneDataSizeReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(InstanceSceneDataAllocator.GetMaxSize(), InitialBufferSize));
	FResizeResourceSOAParams ResizeParams;
	ResizeParams.NumBytes = InstanceSceneDataSizeReserve * FInstanceSceneShaderData::GetEffectiveNumBytes();
	ResizeParams.NumArrays = FInstanceSceneShaderData::GetDataStrideInFloat4s();

	BufferState.InstanceSceneDataBuffer = ResizeStructuredBufferSOAIfNeeded(GraphBuilder, InstanceSceneDataBuffer, ResizeParams, TEXT("GPUScene.InstanceSceneData"));
	InstanceSceneDataSOAStride = InstanceSceneDataSizeReserve;
	BufferState.InstanceSceneDataSOAStride = InstanceSceneDataSizeReserve;

	const uint32 PayloadFloat4Count = FMath::Max(InstancePayloadDataAllocator.GetMaxSize(), InitialBufferSize);
	const uint32 InstancePayloadDataSizeReserve = FMath::RoundUpToPowerOfTwo(PayloadFloat4Count * sizeof(FVector4f));
	BufferState.InstancePayloadDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, InstancePayloadDataBuffer, InstancePayloadDataSizeReserve, TEXT("GPUScene.InstancePayloadData"));

	const uint32 NumNodes = FMath::RoundUpToPowerOfTwo(FMath::Max(Scene.InstanceBVH.GetNumNodes(), InitialBufferSize));
	BufferState.InstanceBVHBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, InstanceBVHBuffer, NumNodes * sizeof(FBVHNode), TEXT("InstanceBVH"));

	const bool bNaniteEnabled = DoesPlatformSupportNanite(GMaxRHIShaderPlatform);
	if (UploadDataSourceAdapter.bUpdateNaniteMaterialTables && bNaniteEnabled)
	{
		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			Scene.NaniteMaterials[NaniteMeshPassIndex].UpdateBufferState(GraphBuilder, Scene.Primitives.Num());
		}
	}
	
	const uint32 LightMapDataBufferSize = FMath::RoundUpToPowerOfTwo(FMath::Max(LightmapDataAllocator.GetMaxSize(), InitialBufferSize));
	BufferState.LightmapDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LightmapDataBuffer, LightMapDataBufferSize * sizeof(FLightmapSceneShaderData::Data), TEXT("GPUScene.LightmapData"));
	BufferState.LightMapDataBufferSize = LightMapDataBufferSize;

	ShaderParameters.GPUSceneInstanceSceneData = GraphBuilder.CreateSRV(BufferState.InstanceSceneDataBuffer);
	ShaderParameters.GPUSceneInstancePayloadData = GraphBuilder.CreateSRV(BufferState.InstancePayloadDataBuffer);
	ShaderParameters.GPUScenePrimitiveSceneData = GraphBuilder.CreateSRV(BufferState.PrimitiveBuffer);
	ShaderParameters.GPUSceneLightmapData = GraphBuilder.CreateSRV(BufferState.LightmapDataBuffer);
	ShaderParameters.InstanceDataSOAStride = InstanceSceneDataSOAStride;
	ShaderParameters.NumScenePrimitives = NumScenePrimitives;
	ShaderParameters.NumInstances = InstanceSceneDataAllocator.GetMaxSize();
	ShaderParameters.GPUSceneFrameNumber = GetSceneFrameNumber();
}

/**
 * Used to queue up load-balanced chunks of instance upload work such that it can be spread over a large number of cores.
 */
struct FInstanceUploadBatch
{
	static constexpr int32 MaxItems = 64;
	static constexpr int32 MaxCost = MaxItems * 2; // Selected to allow filling the array when 1:1 primitive / instances

	struct FItem
	{
		int32 ItemIndex;
		int32 FirstInstance;
		int32 NumInstances;
	};

	int32 FirstItem = 0;
	int32 NumItems = 0;
};

struct FInstanceBatcher
{
	struct FPrimitiveItemInfo
	{
		int32 InstanceSceneDataUploadOffset;
		int32 InstancePayloadDataUploadOffset;
	};

	FInstanceUploadBatch* CurrentBatch = nullptr;
	TArray<FInstanceUploadBatch, TInlineAllocator<1, SceneRenderingAllocator> > UpdateBatches;
	TArray<FInstanceUploadBatch::FItem, TInlineAllocator<256, SceneRenderingAllocator> > UpdateBatchItems;
	TArray<FPrimitiveItemInfo, SceneRenderingAllocator> PerPrimitiveItemInfo;

	int32 CurrentBatchCost = 0;

	int32 InstanceSceneDataUploadOffset = 0;
	int32 InstancePayloadDataUploadOffset = 0; // Count of float4s

	FInstanceBatcher()
	{
		CurrentBatch = &UpdateBatches.AddDefaulted_GetRef();
	}

	void QueueInstances(const FPrimitiveUploadInfoHeader &UploadInfo, int32 ItemIndex, const FPrimitiveItemInfo & PrimitiveItemInfo)
	{
		PerPrimitiveItemInfo[ItemIndex] = PrimitiveItemInfo;
		const int32 NumInstances = UploadInfo.NumInstanceUploads;
		int32 InstancesAdded = 0;
		while (InstancesAdded < NumInstances)
		{
			// Populate the last batch until full. Max items/batch = 64, for balance use cost estimate of 1:1 for primitive:instance
			// Fill to minimum cost (1

			// Can add one less to account for primitive cost
			int32 MaxInstancesThisBatch = FInstanceUploadBatch::MaxCost - CurrentBatchCost - 1;

			if (MaxInstancesThisBatch > 0)
			{
				const int NumInstancesThisItem = FMath::Min(MaxInstancesThisBatch, NumInstances - InstancesAdded);
				UpdateBatchItems.Add(FInstanceUploadBatch::FItem{ ItemIndex, InstancesAdded, NumInstancesThisItem });
				CurrentBatch->NumItems += 1;
				InstancesAdded += NumInstancesThisItem;
				CurrentBatchCost += NumInstancesThisItem + 1;
			}

			// Flush batch if it is not possible to add any more items (for one of the reasons)
			if (MaxInstancesThisBatch <= 0 || CurrentBatchCost > FInstanceUploadBatch::MaxCost - 1 || CurrentBatch->NumItems >= FInstanceUploadBatch::MaxItems)
			{
				CurrentBatchCost = 0;
				CurrentBatch = &UpdateBatches.AddDefaulted_GetRef();
				CurrentBatch->FirstItem = UpdateBatchItems.Num();
			}
		}
	}
};

void FGPUScene::UseInternalAccessMode(FRDGBuilder& GraphBuilder)
{
	if (!bInExternalAccessMode)
	{
		return;
	}

	GraphBuilder.UseInternalAccessMode({
		BufferState.InstanceSceneDataBuffer,
		BufferState.InstancePayloadDataBuffer,
		BufferState.PrimitiveBuffer,
		BufferState.LightmapDataBuffer
	});

	if (BufferState.InstanceBVHBuffer)
	{
		GraphBuilder.UseInternalAccessMode(BufferState.InstanceBVHBuffer);
	}

	bInExternalAccessMode = false;
}

void FGPUScene::UseExternalAccessMode(FRDGExternalAccessQueue& ExternalAccessQueue, ERHIAccess Access, ERHIPipeline Pipelines)
{
	if (bInExternalAccessMode)
	{
		return;
	}

	ExternalAccessQueue.AddUnique(BufferState.InstanceSceneDataBuffer, Access, Pipelines);
	ExternalAccessQueue.AddUnique(BufferState.InstancePayloadDataBuffer, Access, Pipelines);
	ExternalAccessQueue.AddUnique(BufferState.PrimitiveBuffer, Access, Pipelines);
	ExternalAccessQueue.AddUnique(BufferState.LightmapDataBuffer, Access, Pipelines);

	if (BufferState.InstanceBVHBuffer)
	{
		ExternalAccessQueue.AddUnique(BufferState.InstanceBVHBuffer, Access, Pipelines);
	}

	bInExternalAccessMode = true;
}

template<typename FUploadDataSourceAdapter>
void FGPUScene::UploadGeneral(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue, const FUploadDataSourceAdapter& UploadDataSourceAdapter)
{
	LLM_SCOPE_BYTAG(GPUScene);

	ensure(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	ensure(NumScenePrimitives == Scene.Primitives.Num());

	const int32 NumPrimitiveDataUploads = UploadDataSourceAdapter.NumPrimitivesToUpload();

	if (!NumPrimitiveDataUploads)
	{
		return;
	}

	const bool bExecuteInParallel = GGPUSceneParallelUpdate != 0 && FApp::ShouldUseThreadingForPerformance();
	const bool bNaniteEnabled = DoesPlatformSupportNanite(GMaxRHIShaderPlatform);

	SCOPED_NAMED_EVENT(UpdateGPUScene, FColor::Green);

	// Multi-GPU support : Updating on all GPUs is inefficient for AFR. Work is wasted
	// for any primitives that update on consecutive frames.
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
	RDG_EVENT_SCOPE(GraphBuilder, "UpdateGPUScene NumPrimitiveDataUploads %u", NumPrimitiveDataUploads);

	struct FTaskContext
	{
		TArray<FPrimitiveUploadInfoHeader, FSceneRenderingArrayAllocator> PrimitiveUploadInfos;

		FRDGScatterUploader* PrimitiveUploader = nullptr;
		FRDGScatterUploader* InstancePayloadUploader = nullptr;
		FRDGScatterUploader* InstanceSceneUploader = nullptr;
		FRDGScatterUploader* InstanceBVHUploader = nullptr;
		FRDGScatterUploader* LightmapUploader = nullptr;

		TStaticArray<FNaniteMaterialCommands::FUploader*, ENaniteMeshPass::Num> NaniteMaterialUploaders{ InPlace, nullptr };

		int32 NumPrimitiveDataUploads = 0;
		int32 NumLightmapDataUploads = 0;
		int32 NumInstanceSceneDataUploads = 0;
		int32 NumInstancePayloadDataUploads = 0; // Count of float4s

		uint32 InstanceSceneDataSOAStride = 1;

		bool bUseNaniteMaterialUploaders = false;
	};

	FTaskContext& TaskContext = *GraphBuilder.AllocObject<FTaskContext>();

	TaskContext.NumPrimitiveDataUploads = NumPrimitiveDataUploads;
	TaskContext.InstanceSceneDataSOAStride = BufferState.InstanceSceneDataSOAStride;
	TaskContext.PrimitiveUploadInfos.SetNumUninitialized(NumPrimitiveDataUploads);

	for (int32 ItemIndex = 0; ItemIndex < NumPrimitiveDataUploads; ++ItemIndex)
	{
		FPrimitiveUploadInfoHeader& UploadInfo = TaskContext.PrimitiveUploadInfos[ItemIndex];
		UploadDataSourceAdapter.GetPrimitiveInfoHeader(ItemIndex, UploadInfo);

		TaskContext.NumLightmapDataUploads += UploadInfo.LightmapUploadCount; // Not thread safe
		TaskContext.NumInstanceSceneDataUploads += UploadInfo.NumInstanceUploads; // Not thread safe
		TaskContext.NumInstancePayloadDataUploads += UploadInfo.NumInstancePayloadDataUploads; // Not thread safe
	}

	TaskContext.PrimitiveUploader = PrimitiveUploadBuffer.Begin(GraphBuilder, BufferState.PrimitiveBuffer, UploadDataSourceAdapter.GetItemPrimitiveIds().Num(), sizeof(FPrimitiveSceneShaderData::Data), TEXT("PrimitiveUploadBuffer"));

	if (TaskContext.NumInstancePayloadDataUploads > 0)
	{
		TaskContext.InstancePayloadUploader = InstancePayloadUploadBuffer.BeginPreSized(GraphBuilder, BufferState.InstancePayloadDataBuffer, TaskContext.NumInstancePayloadDataUploads, sizeof(FVector4f), TEXT("InstancePayloadUploadBuffer"));
	}

	if (TaskContext.NumInstanceSceneDataUploads > 0)
	{
		TaskContext.InstanceSceneUploader = InstanceSceneUploadBuffer.BeginPreSized(GraphBuilder, BufferState.InstanceSceneDataBuffer, TaskContext.NumInstanceSceneDataUploads * FInstanceSceneShaderData::GetDataStrideInFloat4s(), sizeof(FVector4f), TEXT("InstanceSceneUploadBuffer"));
	}

	if (Scene.InstanceBVH.GetNumDirty() > 0)
	{
		TaskContext.InstanceBVHUploader = InstanceBVHUploadBuffer.Begin(GraphBuilder, BufferState.InstanceBVHBuffer, Scene.InstanceBVH.GetNumDirty(), sizeof(FBVHNode), TEXT("InstanceSceneUploadBuffer"));
	}

	if (TaskContext.NumLightmapDataUploads > 0)
	{
		TaskContext.LightmapUploader = LightmapUploadBuffer.Begin(GraphBuilder, BufferState.LightmapDataBuffer, TaskContext.NumLightmapDataUploads, sizeof(FLightmapSceneShaderData::Data), TEXT("LightmapUploadBuffer"));
	}

	if (UploadDataSourceAdapter.bUpdateNaniteMaterialTables && bNaniteEnabled)
	{
		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			TaskContext.NaniteMaterialUploaders[NaniteMeshPassIndex] = Scene.NaniteMaterials[NaniteMeshPassIndex].Begin(GraphBuilder, Scene.Primitives.Num(), NumPrimitiveDataUploads);
		}

		TaskContext.bUseNaniteMaterialUploaders = true;
	}

	GraphBuilder.AddCommandListSetupTask([&TaskContext, &UploadDataSourceAdapter, &Scene, bNaniteEnabled, bExecuteInParallel, FeatureLevel = FeatureLevel](FRHICommandListBase& RHICmdList)
	{
		SCOPED_NAMED_EVENT(UpdateGPUScene, FColor::Green);

		LockIfValid(RHICmdList, TaskContext.PrimitiveUploader);
		LockIfValid(RHICmdList, TaskContext.InstancePayloadUploader);
		LockIfValid(RHICmdList, TaskContext.InstanceSceneUploader);
		LockIfValid(RHICmdList, TaskContext.InstanceBVHUploader);
		LockIfValid(RHICmdList, TaskContext.LightmapUploader);

		for (FNaniteMaterialCommands::FUploader* Uploader : TaskContext.NaniteMaterialUploaders)
		{
			LockIfValid(RHICmdList, Uploader);
		}

		FInstanceBatcher InstanceUpdates;

		{
			SCOPED_NAMED_EVENT(Primitives, FColor::Green);

			InstanceUpdates.PerPrimitiveItemInfo.SetNumUninitialized(TaskContext.NumPrimitiveDataUploads);

			// Recalculated to determine offsets.
			int32 NumInstanceSceneDataUploads = 0;
			int32 NumInstancePayloadDataUploads = 0;

			for (int32 ItemIndex = 0; ItemIndex < TaskContext.NumPrimitiveDataUploads; ++ItemIndex)
			{
				const FPrimitiveUploadInfoHeader& UploadInfo = TaskContext.PrimitiveUploadInfos[ItemIndex];

				FInstanceBatcher::FPrimitiveItemInfo PrimitiveItemInfo;
				PrimitiveItemInfo.InstanceSceneDataUploadOffset = NumInstanceSceneDataUploads;
				PrimitiveItemInfo.InstancePayloadDataUploadOffset = NumInstancePayloadDataUploads;
				InstanceUpdates.QueueInstances(UploadInfo, ItemIndex, PrimitiveItemInfo);

				NumInstanceSceneDataUploads += UploadInfo.NumInstanceUploads;
				NumInstancePayloadDataUploads += UploadInfo.NumInstancePayloadDataUploads;

				if (TaskContext.bUseNaniteMaterialUploaders && UploadInfo.NaniteSceneProxy != nullptr)
				{
					check(UploadDataSourceAdapter.bUpdateNaniteMaterialTables);
					check(UploadInfo.PrimitiveSceneInfo != nullptr);
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = UploadInfo.PrimitiveSceneInfo;
					const Nanite::FSceneProxyBase* NaniteSceneProxy = UploadInfo.NaniteSceneProxy;
					const TArray<Nanite::FSceneProxyBase::FMaterialSection>& PassMaterials = NaniteSceneProxy->GetMaterialSections();

					// Update raster bin, material depth and hit proxy ID remapping tables.
					for (int32 NaniteMeshPass = 0; NaniteMeshPass < ENaniteMeshPass::Num; ++NaniteMeshPass)
					{
						FNaniteMaterialCommands::FUploader* NaniteMaterialUploader = TaskContext.NaniteMaterialUploaders[NaniteMeshPass];
						const TArray<FNaniteMaterialSlot>& PassMaterialSlots = PrimitiveSceneInfo->NaniteMaterialSlots[NaniteMeshPass];

						if (PassMaterials.Num() == PassMaterialSlots.Num())
						{
							const uint32 MaterialSlotCount = uint32(PassMaterialSlots.Num());
							const uint32 TableEntryCount = uint32(NaniteSceneProxy->GetMaterialMaxIndex() + 1);

							// TODO: Make this more robust, and catch issues earlier on
							const uint32 UploadEntryCount = FMath::Max(MaterialSlotCount, TableEntryCount);

							void* MaterialSlotRange = NaniteMaterialUploader->GetMaterialSlotPtr(UploadInfo.PrimitiveID, UploadEntryCount);
							auto MaterialSlots = static_cast<FNaniteMaterialSlot::FPacked*>(MaterialSlotRange);
							for (uint32 Entry = 0; Entry < MaterialSlotCount; ++Entry)
							{
								MaterialSlots[PassMaterials[Entry].MaterialIndex] = PassMaterialSlots[Entry].Pack();
							}

#if WITH_EDITOR
							if (NaniteMeshPass == ENaniteMeshPass::BasePass && NaniteSceneProxy->GetHitProxyMode() == Nanite::FSceneProxyBase::EHitProxyMode::MaterialSection)
							{
								const TArray<uint32>& PassHitProxyIds = PrimitiveSceneInfo->NaniteHitProxyIds;
								uint32* HitProxyTable = static_cast<uint32*>(NaniteMaterialUploader->GetHitProxyTablePtr(UploadInfo.PrimitiveID, MaterialSlotCount));
								for (int32 Entry = 0; Entry < PassHitProxyIds.Num(); ++Entry)
								{
									HitProxyTable[PassMaterials[Entry].MaterialIndex] = PassHitProxyIds[Entry];
								}
							}
#endif
						}
					}
				}
			}

			TaskContext.PrimitiveUploader->Add(UploadDataSourceAdapter.GetItemPrimitiveIds());

			ParallelFor(TaskContext.NumPrimitiveDataUploads, [&TaskContext, &UploadDataSourceAdapter](int32 ItemIndex)
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				FPrimitiveUploadInfo UploadInfo;
				UploadDataSourceAdapter.GetPrimitiveInfo(ItemIndex, UploadInfo);

				FVector4f* DstData = static_cast<FVector4f*>(TaskContext.PrimitiveUploader->GetRef(ItemIndex));
				for (uint32 VectorIndex = 0; VectorIndex < FPrimitiveSceneShaderData::DataStrideInFloat4s; ++VectorIndex)
				{
					DstData[VectorIndex] = UploadInfo.PrimitiveSceneData.Data[VectorIndex];
				}

			}, !bExecuteInParallel);
		}

		if (TaskContext.NumInstanceSceneDataUploads > 0 && !InstanceUpdates.UpdateBatches.IsEmpty())
		{
			SCOPED_NAMED_EVENT(Instances, FColor::Green);

			ParallelFor(InstanceUpdates.UpdateBatches.Num(), [&TaskContext, &InstanceUpdates, &UploadDataSourceAdapter](int32 BatchIndex)
			{
				const FInstanceUploadBatch Batch = InstanceUpdates.UpdateBatches[BatchIndex];

				for (int32 BatchItemIndex = 0; BatchItemIndex < Batch.NumItems; ++BatchItemIndex)
				{
					const FInstanceUploadBatch::FItem Item = InstanceUpdates.UpdateBatchItems[Batch.FirstItem + BatchItemIndex];

					const int32 ItemIndex = Item.ItemIndex;
					FInstanceUploadInfo UploadInfo;
					UploadDataSourceAdapter.GetInstanceInfo(ItemIndex, UploadInfo);
					ValidateInstanceUploadInfo(UploadInfo, TaskContext.InstancePayloadUploader ? GetAsBuffer(TaskContext.InstancePayloadUploader->GetDstResource()) : nullptr);
					FInstanceBatcher::FPrimitiveItemInfo PrimitiveItemInfo = InstanceUpdates.PerPrimitiveItemInfo[ItemIndex];

					check(TaskContext.NumInstancePayloadDataUploads > 0 || UploadInfo.InstancePayloadDataStride == 0); // Sanity check

					for (int32 BatchInstanceIndex = 0; BatchInstanceIndex < Item.NumInstances; ++BatchInstanceIndex)
					{
						int32 InstanceIndex = Item.FirstInstance + BatchInstanceIndex;
						const FPrimitiveInstance& SceneData = UploadInfo.PrimitiveInstances[InstanceIndex];

						// Directly embedded in instance scene data
						const float RandomID = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_RANDOM) ? UploadInfo.InstanceRandomID[InstanceIndex] : 0.0f;

						FInstanceSceneShaderData InstanceSceneData;
						InstanceSceneData.Build(
							UploadInfo.PrimitiveID,
							InstanceIndex,
							UploadInfo.InstanceFlags,
							UploadInfo.LastUpdateSceneFrameNumber,
							UploadInfo.InstanceCustomDataCount,
							RandomID,
							SceneData.LocalToPrimitive,
							UploadInfo.PrimitiveToWorld,
							UploadInfo.PrevPrimitiveToWorld
						);

						// RefIndex* BufferState.InstanceSceneDataSOAStride + UploadInfo.InstanceSceneDataOffset + InstanceIndex
						const uint32 UploadInstanceItemOffset = (PrimitiveItemInfo.InstanceSceneDataUploadOffset + InstanceIndex) * FInstanceSceneShaderData::GetDataStrideInFloat4s();

						for (uint32 RefIndex = 0; RefIndex < FInstanceSceneShaderData::GetDataStrideInFloat4s(); ++RefIndex)
						{
							FVector4f* DstVector = static_cast<FVector4f*>(TaskContext.InstanceSceneUploader->Set_GetRef(UploadInstanceItemOffset + RefIndex, RefIndex * TaskContext.InstanceSceneDataSOAStride + UploadInfo.InstanceSceneDataOffset + InstanceIndex));
							*DstVector = InstanceSceneData.Data[RefIndex];
						}

						if (UploadInfo.InstancePayloadDataStride > 0)
						{
							const uint32 UploadPayloadItemOffset = PrimitiveItemInfo.InstancePayloadDataUploadOffset + InstanceIndex * UploadInfo.InstancePayloadDataStride;

							int32 PayloadDataStart = UploadInfo.InstancePayloadDataOffset + (InstanceIndex * UploadInfo.InstancePayloadDataStride);
							FVector4f* DstPayloadData = static_cast<FVector4f*>(TaskContext.InstancePayloadUploader->Set_GetRef(UploadPayloadItemOffset, PayloadDataStart, UploadInfo.InstancePayloadDataStride));
							TArrayView<FVector4f> InstancePayloadData = TArrayView<FVector4f>(DstPayloadData, UploadInfo.InstancePayloadDataStride);

							int32 PayloadPosition = 0;

							if (UploadInfo.InstanceFlags & (INSTANCE_SCENE_DATA_FLAG_HAS_HIERARCHY_OFFSET | INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS | INSTANCE_SCENE_DATA_FLAG_HAS_EDITOR_DATA))
							{
								const uint32 InstanceHierarchyOffset = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_HIERARCHY_OFFSET) ? UploadInfo.InstanceHierarchyOffset[InstanceIndex] : 0;
								InstancePayloadData[PayloadPosition].X = *(const float*)&InstanceHierarchyOffset;

#if WITH_EDITOR
								const uint32 InstanceEditorData = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_EDITOR_DATA) ? UploadInfo.InstanceEditorData[InstanceIndex] : 0;
								InstancePayloadData[PayloadPosition].Y = *(const float*)&InstanceEditorData;
#endif
								if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS)
								{
									check(UploadInfo.InstanceLocalBounds.Num() == UploadInfo.PrimitiveInstances.Num());
									const FRenderBounds& InstanceLocalBounds = UploadInfo.InstanceLocalBounds[InstanceIndex];
									const FVector3f BoundsOrigin = InstanceLocalBounds.GetCenter();
									const FVector3f BoundsExtent = InstanceLocalBounds.GetExtent();

									InstancePayloadData[PayloadPosition + 0].Z = *(const float*)&BoundsOrigin.X;
									InstancePayloadData[PayloadPosition + 0].W = *(const float*)&BoundsOrigin.Y;

									InstancePayloadData[PayloadPosition + 1].X = *(const float*)&BoundsOrigin.Z;
									InstancePayloadData[PayloadPosition + 1].Y = *(const float*)&BoundsExtent.X;
									InstancePayloadData[PayloadPosition + 1].Z = *(const float*)&BoundsExtent.Y;
									InstancePayloadData[PayloadPosition + 1].W = *(const float*)&BoundsExtent.Z;
								}

								PayloadPosition += (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS) ? 2 : 1;
							}

							if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_DYNAMIC_DATA)
							{
								check(UploadInfo.InstanceDynamicData.Num() == UploadInfo.PrimitiveInstances.Num());
								const FRenderTransform PrevLocalToWorld = UploadInfo.InstanceDynamicData[InstanceIndex].ComputePrevLocalToWorld(UploadInfo.PrevPrimitiveToWorld);
								if (FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform))
								{
									check(PayloadPosition + 1 < InstancePayloadData.Num()); // Sanity check
									FCompressedTransform CompressedPrevLocalToWorld(PrevLocalToWorld);
									InstancePayloadData[PayloadPosition + 0] = *(const FVector4f*)&CompressedPrevLocalToWorld.Rotation[0];
									InstancePayloadData[PayloadPosition + 1] = *(const FVector3f*)&CompressedPrevLocalToWorld.Translation;
									PayloadPosition += 2;
								}
								else
								{
									// Note: writes 3x float4s
									check(PayloadPosition + 2 < InstancePayloadData.Num()); // Sanity check
									PrevLocalToWorld.To3x4MatrixTranspose((float*)&InstancePayloadData[PayloadPosition]);
									PayloadPosition += 3;
								}
							}

							if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LIGHTSHADOW_UV_BIAS)
							{
								check(UploadInfo.InstanceLightShadowUVBias.Num() == UploadInfo.PrimitiveInstances.Num());
								InstancePayloadData[PayloadPosition] = UploadInfo.InstanceLightShadowUVBias[InstanceIndex];
								PayloadPosition += 1;
							}

							if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_CUSTOM_DATA)
							{
								check(PayloadPosition + (UploadInfo.InstanceCustomDataCount >> 2u) <= InstancePayloadData.Num());
								const float* CustomDataGetPtr = &UploadInfo.InstanceCustomData[(InstanceIndex * UploadInfo.InstanceCustomDataCount)];
								float* CustomDataPutPtr = (float*)&InstancePayloadData[PayloadPosition];
								for (uint32 FloatIndex = 0; FloatIndex < uint32(UploadInfo.InstanceCustomDataCount); ++FloatIndex)
								{
									CustomDataPutPtr[FloatIndex] = CustomDataGetPtr[FloatIndex];
								}
							}
						}
					}
				}

			}, !bExecuteInParallel);
		}

		if (TaskContext.InstanceBVHUploader)
		{
			SCOPED_NAMED_EVENT(InstanceBVH, FColor::Green);

			Scene.InstanceBVH.ForAllDirty(
				[&](uint32 NodeIndex, const auto& Node)
			{
				FBVHNode GPUNode;
				for (int i = 0; i < 4; i++)
				{
					GPUNode.ChildIndexes[i] = Node.ChildIndexes[i];

					GPUNode.ChildMin[0][i] = Node.ChildBounds[i].Min.X;
					GPUNode.ChildMin[1][i] = Node.ChildBounds[i].Min.Y;
					GPUNode.ChildMin[2][i] = Node.ChildBounds[i].Min.Z;

					GPUNode.ChildMax[0][i] = Node.ChildBounds[i].Max.X;
					GPUNode.ChildMax[1][i] = Node.ChildBounds[i].Max.Y;
					GPUNode.ChildMax[2][i] = Node.ChildBounds[i].Max.Z;
				}

				TaskContext.InstanceBVHUploader->Add(NodeIndex, &GPUNode);
			});
		}

		if (TaskContext.LightmapUploader)
		{
			for (int32 ItemIndex = 0; ItemIndex < TaskContext.NumPrimitiveDataUploads; ++ItemIndex)
			{
				FLightMapUploadInfo UploadInfo;
				if (UploadDataSourceAdapter.GetLightMapInfo(ItemIndex, UploadInfo))
				{
					for (int32 LCIIndex = 0; LCIIndex < UploadInfo.LCIs.Num(); LCIIndex++)
					{
						FLightmapSceneShaderData LightmapSceneData(UploadInfo.LCIs[LCIIndex], FeatureLevel);
						TaskContext.LightmapUploader->Add(UploadInfo.LightmapDataOffset + LCIIndex, &LightmapSceneData.Data[0]);
					}
				}
			}
		}

		UnlockIfValid(RHICmdList, TaskContext.PrimitiveUploader);
		UnlockIfValid(RHICmdList, TaskContext.InstancePayloadUploader);
		UnlockIfValid(RHICmdList, TaskContext.InstanceSceneUploader);
		UnlockIfValid(RHICmdList, TaskContext.InstanceBVHUploader);
		UnlockIfValid(RHICmdList, TaskContext.LightmapUploader);

		for (FNaniteMaterialCommands::FUploader* Uploader : TaskContext.NaniteMaterialUploaders)
		{
			UnlockIfValid(RHICmdList, Uploader);
		}
	});

	PrimitiveUploadBuffer.End(GraphBuilder, TaskContext.PrimitiveUploader);

	if (TaskContext.InstancePayloadUploader)
	{
		InstancePayloadUploadBuffer.End(GraphBuilder, TaskContext.InstancePayloadUploader);
	}

	if (TaskContext.InstanceSceneUploader)
	{
		InstanceSceneUploadBuffer.End(GraphBuilder, TaskContext.InstanceSceneUploader);
	}

	if (TaskContext.InstanceBVHUploader)
	{
		InstanceBVHUploadBuffer.End(GraphBuilder, TaskContext.InstanceBVHUploader);
	}

	if (TaskContext.LightmapUploader)
	{
		LightmapUploadBuffer.End(GraphBuilder, TaskContext.LightmapUploader);
	}

	if (TaskContext.bUseNaniteMaterialUploaders)
	{
		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			Scene.NaniteMaterials[NaniteMeshPassIndex].Finish(GraphBuilder, ExternalAccessQueue, TaskContext.NaniteMaterialUploaders[NaniteMeshPassIndex]);
		}
	}

	if (PrimitiveUploadBuffer.GetNumBytes() > (uint32)GGPUSceneMaxPooledUploadBufferSize)
	{
		PrimitiveUploadBuffer.Release();
	}

	if (InstanceSceneUploadBuffer.GetNumBytes() > (uint32)GGPUSceneMaxPooledUploadBufferSize)
	{
		InstanceSceneUploadBuffer.Release();
	}

	if (InstancePayloadUploadBuffer.GetNumBytes() > (uint32)GGPUSceneMaxPooledUploadBufferSize)
	{
		InstancePayloadUploadBuffer.Release();
	}

	if (InstanceBVHUploadBuffer.GetNumBytes() > (uint32)GGPUSceneMaxPooledUploadBufferSize)
	{
		InstanceBVHUploadBuffer.Release();
	}

	if (LightmapUploadBuffer.GetNumBytes() > (uint32)GGPUSceneMaxPooledUploadBufferSize)
	{
		LightmapUploadBuffer.Release();
	}
}

struct FUploadDataSourceAdapterDynamicPrimitives
{
	static constexpr bool bUpdateNaniteMaterialTables = false;

	FUploadDataSourceAdapterDynamicPrimitives(
		const TArray<FGPUScenePrimitiveCollector::FPrimitiveData, TInlineAllocator<8>>& InPrimitiveData,
		int32 InPrimitiveIDStartOffset,
		int32 InInstanceIDStartOffset,
		int32 InPayloadStartOffset,
		uint32 InSceneFrameNumber)
		: PrimitiveData(InPrimitiveData)
		, PrimitiveIDStartOffset(InPrimitiveIDStartOffset)
		, InstanceIDStartOffset(InInstanceIDStartOffset)
		, PayloadStartOffset(InPayloadStartOffset)
		, SceneFrameNumber(InSceneFrameNumber)
	{
		// Need to create this explicit for optimizing the common path
		PrimitivesIds.SetNumUninitialized(PrimitiveData.Num());
		for (int32 Index = 0; Index < PrimitivesIds.Num(); ++Index)
		{
			PrimitivesIds[Index] = uint32(PrimitiveIDStartOffset + Index);
		}
	}

	FORCEINLINE int32 NumPrimitivesToUpload() const
	{ 
		return PrimitiveData.Num();
	}


	FORCEINLINE TArrayView<const uint32> GetItemPrimitiveIds() const
	{
		return TArrayView<const uint32>(PrimitivesIds.GetData(), PrimitivesIds.Num());
	}


	FORCEINLINE void GetPrimitiveInfoHeader(int32 ItemIndex, FPrimitiveUploadInfoHeader& PrimitiveUploadInfo) const
	{
		PrimitiveUploadInfo.LightmapUploadCount = 0;
		PrimitiveUploadInfo.NaniteSceneProxy = nullptr;
		PrimitiveUploadInfo.PrimitiveSceneInfo = nullptr;

		check(ItemIndex < PrimitiveData.Num());

		PrimitiveUploadInfo.PrimitiveID = PrimitiveIDStartOffset + ItemIndex;

		const FGPUScenePrimitiveCollector::FPrimitiveData& PrimData = PrimitiveData[ItemIndex];
		PrimitiveUploadInfo.NumInstanceUploads = PrimData.NumInstances;
		PrimitiveUploadInfo.NumInstancePayloadDataUploads = PrimData.SourceData.GetPayloadFloat4Stride() * PrimData.NumInstances;
		
		if (PrimData.SourceData.DataWriterGPU.IsBound())
		{
			// Only upload if we have data, otherwise expect the delegate to handle missing data
			PrimitiveUploadInfo.NumInstanceUploads = PrimData.SourceData.InstanceSceneData.Num();
			if (PrimData.SourceData.InstanceCustomData.Num() == 0)
			{
				PrimitiveUploadInfo.NumInstancePayloadDataUploads = 0;
			}
		}
	}

	FORCEINLINE void GetPrimitiveInfo(int32 ItemIndex, FPrimitiveUploadInfo& PrimitiveUploadInfo) const
	{
		GetPrimitiveInfoHeader(ItemIndex, PrimitiveUploadInfo);

		// Needed to ensure the link back to instance list is up to date
		const FGPUScenePrimitiveCollector::FPrimitiveData& PrimData = PrimitiveData[ItemIndex];		
		FPrimitiveUniformShaderParameters Tmp = *PrimData.ShaderParams;
		Tmp.InstanceSceneDataOffset = InstanceIDStartOffset + PrimData.LocalInstanceSceneDataOffset;
		Tmp.NumInstanceSceneDataEntries = PrimData.NumInstances;
		Tmp.Flags &= ~(PRIMITIVE_SCENE_DATA_FLAG_CACHE_SHADOW_AS_STATIC);	// Dyanmic primtives are never cached as static for shadows
		if (PrimitiveData[ItemIndex].LocalPayloadDataOffset != INDEX_NONE)
		{
			Tmp.InstancePayloadDataOffset = PayloadStartOffset + PrimData.LocalPayloadDataOffset;
			Tmp.InstancePayloadDataStride = PrimData.SourceData.GetPayloadFloat4Stride();
		}
		else
		{
			Tmp.InstancePayloadDataOffset = INDEX_NONE;
			Tmp.InstancePayloadDataStride = 0;
		}

		PrimitiveUploadInfo.PrimitiveSceneData = FPrimitiveSceneShaderData(Tmp);
	}

	FORCEINLINE bool GetInstanceInfo(int32 ItemIndex, FInstanceUploadInfo& InstanceUploadInfo) const
	{
		if (ItemIndex < PrimitiveData.Num())
		{
			const FGPUScenePrimitiveCollector::FPrimitiveData& PrimData = PrimitiveData[ItemIndex];
			const FPrimitiveUniformShaderParameters& ShaderParams = *PrimData.ShaderParams;

			InstanceUploadInfo.PrimitiveID					= PrimitiveIDStartOffset + ItemIndex;
			InstanceUploadInfo.PrimitiveToWorld				= ShaderParams.LocalToRelativeWorld;
			InstanceUploadInfo.PrevPrimitiveToWorld			= ShaderParams.PreviousLocalToRelativeWorld;
			InstanceUploadInfo.InstanceSceneDataOffset		= InstanceIDStartOffset + PrimData.LocalInstanceSceneDataOffset;
			InstanceUploadInfo.InstancePayloadDataOffset	= PrimData.LocalPayloadDataOffset == INDEX_NONE ? INDEX_NONE : PayloadStartOffset + PrimData.LocalPayloadDataOffset;
			InstanceUploadInfo.InstancePayloadDataStride	= PrimData.SourceData.GetPayloadFloat4Stride();
			InstanceUploadInfo.InstanceCustomDataCount		= PrimData.SourceData.NumInstanceCustomDataFloats;
			InstanceUploadInfo.InstanceFlags                = PrimData.SourceData.PayloadDataFlags;
			InstanceUploadInfo.PrimitiveInstances 			= PrimData.SourceData.InstanceSceneData;
			InstanceUploadInfo.InstanceDynamicData 			= PrimData.SourceData.InstanceDynamicData;
			InstanceUploadInfo.InstanceLocalBounds 			= PrimData.SourceData.InstanceLocalBounds;
			InstanceUploadInfo.InstanceCustomData 			= PrimData.SourceData.InstanceCustomData;
			InstanceUploadInfo.InstanceRandomID				= TConstArrayView<float>();
			InstanceUploadInfo.InstanceHierarchyOffset		= TConstArrayView<uint32>();
			InstanceUploadInfo.InstanceLightShadowUVBias	= TConstArrayView<FVector4f>();
#if WITH_EDITOR
			InstanceUploadInfo.InstanceEditorData			= TConstArrayView<uint32>();
#endif

			// upload dummies where applicable
			if (InstanceUploadInfo.PrimitiveInstances.Num() == 0)
			{
				InstanceUploadInfo.DummyInstance.LocalToPrimitive.SetIdentity();
				InstanceUploadInfo.PrimitiveInstances = TConstArrayView<FPrimitiveInstance>(&InstanceUploadInfo.DummyInstance, 1);
			}

			return true;
		}

		return false;
	}

	FORCEINLINE bool GetLightMapInfo(int32 ItemIndex, FLightMapUploadInfo& UploadInfo) const
	{
		return false;
	}

	const TArray<FGPUScenePrimitiveCollector::FPrimitiveData, TInlineAllocator<8>> &PrimitiveData;
	const int32 PrimitiveIDStartOffset;
	const int32 InstanceIDStartOffset;
	const int32 PayloadStartOffset;
	const uint32 SceneFrameNumber;
	TArray<uint32, SceneRenderingAllocator> PrimitivesIds;
};

void FGPUScene::UploadDynamicPrimitiveShaderDataForViewInternal(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View, FRDGExternalAccessQueue& ExternalAccessQueue, bool bIsShadowView)
{
	LLM_SCOPE_BYTAG(GPUScene);

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.UploadDynamicPrimitiveShaderDataForView");

	ensure(bInBeginEndBlock);
	ensure(DynamicPrimitivesOffset >= Scene.Primitives.Num());

	FGPUScenePrimitiveCollector& Collector = View.DynamicPrimitiveCollector;

	// Auto-commit if not done (should usually not be done, but sometimes the UploadDynamicPrimitiveShaderDataForViewInternal is called to ensure the 
	// CachedViewUniformShaderParameters is set on the view.
	if (!Collector.bCommitted)
	{
		Collector.Commit();
	}

	const int32 NumPrimitiveDataUploads = Collector.Num();
	ensure(Collector.GetPrimitiveIdRange().Size<int32>() == NumPrimitiveDataUploads);

	// Make sure we are not trying to upload data that lives in a different context.
	ensure(Collector.UploadData == nullptr || CurrentDynamicContext->DymamicPrimitiveUploadData.Find(Collector.UploadData) != INDEX_NONE);

	check(BufferState.IsValid());

	// Skip uploading empty & already uploaded data
	const bool bNeedsUpload = Collector.UploadData != nullptr && NumPrimitiveDataUploads > 0 && !Collector.UploadData->bIsUploaded;
	if (bNeedsUpload)
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, UploadDynamicPrimitiveShaderData);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UploadDynamicPrimitiveShaderData);

		Collector.UploadData->bIsUploaded = true;

		const int32 UploadIdStart = Collector.GetPrimitiveIdRange().GetLowerBoundValue();
		const int32 InstanceIdStart = Collector.UploadData->InstanceSceneDataOffset;
		ensure(UploadIdStart < DynamicPrimitivesOffset);
		ensure(InstanceIdStart != INDEX_NONE);

		if (bIsShadowView && Scene.GetVirtualShadowMapCache(View) != nullptr)
		{
			// Enqueue cache invalidations for all dynamic primitives' instances, as they will be removed this frame and are not associated
			// with any particular FPrimitiveSceneInfo. Will occur on the next call to UpdateAllPrimitiveSceneInfos
			for (const FGPUScenePrimitiveCollector::FPrimitiveData& PrimitiveData : Collector.UploadData->PrimitiveData)
			{
				ensure(PrimitiveData.LocalInstanceSceneDataOffset != INDEX_NONE);				
				DynamicPrimitiveInstancesToInvalidate.Add(
					FInstanceRange
					{
						PrimitiveData.LocalInstanceSceneDataOffset + InstanceIdStart,
						PrimitiveData.NumInstances
					}
				);
			}
		}

		FUploadDataSourceAdapterDynamicPrimitives& UploadAdapter = *GraphBuilder.AllocObject<FUploadDataSourceAdapterDynamicPrimitives>(
			Collector.UploadData->PrimitiveData,
			UploadIdStart,
			InstanceIdStart,
			Collector.UploadData->InstancePayloadDataOffset,
			SceneFrameNumber);

		UpdateBufferState(GraphBuilder, Scene, UploadAdapter);

		UseInternalAccessMode(GraphBuilder);

		// Run a pass that clears (Sets ID to invalid) any instances that need it.
		AddClearInstancesPass(GraphBuilder);

		UploadGeneral<FUploadDataSourceAdapterDynamicPrimitives>(GraphBuilder, Scene, ExternalAccessQueue, UploadAdapter);
	}

	// Update view uniform buffer
	View.CachedViewUniformShaderParameters->PrimitiveSceneData = PrimitiveBuffer->GetSRV();
	View.CachedViewUniformShaderParameters->LightmapSceneData = LightmapDataBuffer->GetSRV();
	View.CachedViewUniformShaderParameters->InstancePayloadData = InstancePayloadDataBuffer->GetSRV();
	View.CachedViewUniformShaderParameters->InstanceSceneData = InstanceSceneDataBuffer->GetSRV();
	View.CachedViewUniformShaderParameters->InstanceSceneDataSOAStride = InstanceSceneDataSOAStride;

	View.ViewUniformBuffer.UpdateUniformBufferImmediate(*View.CachedViewUniformShaderParameters);

	// Execute any instance data GPU writer callbacks. (Note: Done after the UB update, in case the user requires it)
	if (bNeedsUpload) 
	{
		const uint32 PrimitiveIdStart = Collector.GetPrimitiveIdRange().GetLowerBoundValue();
		const uint32 InstanceIdStart = Collector.UploadData->InstanceSceneDataOffset;

		// Determine if we have any GPU data writers this frame and simultaneously defer any writes that must happen later in the frame
		TArray<uint32, SceneRenderingAllocator> ImmediateWrites;
		ImmediateWrites.Reserve(Collector.UploadData->GPUWritePrimitives.Num());
		for (uint32 PrimitiveIndex : Collector.UploadData->GPUWritePrimitives)
		{
			const FGPUScenePrimitiveCollector::FPrimitiveData& PrimData = Collector.UploadData->PrimitiveData[PrimitiveIndex];
			const EGPUSceneGPUWritePass GPUWritePass = PrimData.SourceData.DataWriterGPUPass;
			
			// We're going to immediately execute any GPU writers whose write pass is immediate or has already happened this frame
			if (GPUWritePass == EGPUSceneGPUWritePass::None || GPUWritePass <= LastDeferredGPUWritePass)
			{
				ImmediateWrites.Add(PrimitiveIndex);
			}
			else
			{
				// Defer this write to a later GPU write pass
				FDeferredGPUWrite DeferredWrite;
				DeferredWrite.DataWriterGPU = PrimData.SourceData.DataWriterGPU;
				DeferredWrite.ViewId = View.GPUSceneViewId;
				DeferredWrite.PrimitiveId = PrimitiveIdStart + PrimitiveIndex;
				DeferredWrite.InstanceSceneDataOffset = InstanceIdStart + PrimData.LocalInstanceSceneDataOffset;

				uint32 PassIndex = uint32(PrimData.SourceData.DataWriterGPUPass);
				DeferredGPUWritePassDelegates[PassIndex].Add(DeferredWrite);
			}
		}

		if (ImmediateWrites.Num() > 0)
		{
			// Execute writes that should execute immediately
			RDG_EVENT_SCOPE(GraphBuilder, "GPU Writer Delegates");

			FGPUSceneWriteDelegateParams Params;
			Params.View = &View;
			Params.GPUWritePass = EGPUSceneGPUWritePass::None;
			GetWriteParameters(GraphBuilder, Params.GPUWriteParams);

			for (uint32 PrimitiveIndex : ImmediateWrites)
			{
				const FGPUScenePrimitiveCollector::FPrimitiveData& PrimData = Collector.UploadData->PrimitiveData[PrimitiveIndex];
				Params.PrimitiveId = PrimitiveIdStart + PrimitiveIndex;
				Params.InstanceSceneDataOffset = InstanceIdStart + PrimData.LocalInstanceSceneDataOffset;

				PrimData.SourceData.DataWriterGPU.Execute(GraphBuilder, Params);
			}
		}

		UseExternalAccessMode(ExternalAccessQueue, ERHIAccess::SRVMask, ERHIPipeline::All);
	}
}

void AddPrimitiveToUpdateGPU(FScene& Scene, int32 PrimitiveId)
{
	Scene.GPUScene.AddPrimitiveToUpdate(PrimitiveId, EPrimitiveDirtyState::ChangedAll);
}

void FGPUScene::AddPrimitiveToUpdate(int32 PrimitiveId, EPrimitiveDirtyState DirtyState)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		ResizeDirtyState(PrimitiveId + 1);

		// Make sure we aren't updating same primitive multiple times.
		if (PrimitiveDirtyState[PrimitiveId] == EPrimitiveDirtyState::None)
		{
			PrimitivesToUpdate.Add(PrimitiveId);
		}
		
		PrimitiveDirtyState[PrimitiveId] |= DirtyState;
	}
}


void FGPUScene::Update(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue)
{
	if (bIsEnabled)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		ensure(bInBeginEndBlock);
		
		UpdateInternal(GraphBuilder, Scene, ExternalAccessQueue);
	}
}

void FGPUScene::UploadDynamicPrimitiveShaderDataForView(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View, FRDGExternalAccessQueue& ExternalAccessQueue, bool bIsShadowView)
{
	if (bIsEnabled)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		UploadDynamicPrimitiveShaderDataForViewInternal(GraphBuilder, Scene, View, ExternalAccessQueue, bIsShadowView);
	}
}

int32 FGPUScene::AllocateInstanceSceneDataSlots(int32 NumInstanceSceneDataEntries)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		if (NumInstanceSceneDataEntries > 0)
		{
			int32 InstanceSceneDataOffset = InstanceSceneDataAllocator.Allocate(NumInstanceSceneDataEntries);
			InstanceRangesToClear.Add(FInstanceRange{ uint32(InstanceSceneDataOffset), uint32(NumInstanceSceneDataEntries) });
#if LOG_INSTANCE_ALLOCATIONS
			UE_LOG(LogTemp, Warning, TEXT("AllocateInstanceSceneDataSlots: [%6d,%6d)"), InstanceSceneDataOffset, InstanceSceneDataOffset + NumInstanceSceneDataEntries);
#endif

			return InstanceSceneDataOffset;
		}
	}
	return INDEX_NONE;
}


void FGPUScene::FreeInstanceSceneDataSlots(int32 InstanceSceneDataOffset, int32 NumInstanceSceneDataEntries)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		InstanceSceneDataAllocator.Free(InstanceSceneDataOffset, NumInstanceSceneDataEntries);
		InstanceRangesToClear.Add(FInstanceRange{ uint32(InstanceSceneDataOffset), uint32(NumInstanceSceneDataEntries) });
#if LOG_INSTANCE_ALLOCATIONS
		UE_LOG(LogTemp, Warning, TEXT("FreeInstanceSceneDataSlots: [%6d,%6d)"), InstanceSceneDataOffset, InstanceSceneDataOffset + NumInstanceSceneDataEntries);
#endif
	}
}

int32 FGPUScene::AllocateInstancePayloadDataSlots(int32 NumInstancePayloadFloat4Entries)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		if (NumInstancePayloadFloat4Entries > 0)
		{
			int32 InstancePayloadDataOffset = InstancePayloadDataAllocator.Allocate(NumInstancePayloadFloat4Entries);
			return InstancePayloadDataOffset;
		}
	}
	return INDEX_NONE;
}

void FGPUScene::FreeInstancePayloadDataSlots(int32 InstancePayloadDataOffset, int32 NumInstancePayloadFloat4Entries)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		InstancePayloadDataAllocator.Free(InstancePayloadDataOffset, NumInstancePayloadFloat4Entries);
	}
}

struct FPrimitiveSceneDebugNameInfo
{
	uint32 PrimitiveID;
	uint16 Offset;
	uint8  Length;
	uint8  Pad0;
};

class FGPUSceneDebugRenderCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGPUSceneDebugRenderCS);
	SHADER_USE_PARAMETER_STRUCT(FGPUSceneDebugRenderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneResourceParameters, GPUSceneResource)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER(int32, bDrawAll)
		SHADER_PARAMETER(int32, bDrawUpdatedOnly)
		SHADER_PARAMETER(int32, SelectedNameInfoCount)
		SHADER_PARAMETER(int32, SelectedNameCharacterCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SelectedPrimitiveFlags)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, SelectedPrimitiveNameInfos)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint8>, SelectedPrimitiveNames)
		SHADER_PARAMETER(FVector3f, PickingRayStart)
		SHADER_PARAMETER(FVector3f, PickingRayEnd)
		SHADER_PARAMETER(float, DrawRange)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, RWDrawCounter)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsPerGroup = 128U;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return UseGPUScene(Parameters.Platform) && ShaderPrint::IsSupported(Parameters.Platform);;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);

		// Skip optimization for avoiding long compilation time due to large UAV writes
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGPUSceneDebugRenderCS, "/Engine/Private/GPUSceneDebugRender.usf", "GPUSceneDebugRenderCS", SF_Compute);

void FGPUScene::DebugRender(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View)
{
	int32 DebugMode = CVarGPUSceneDebugMode.GetValueOnRenderThread();
	if (DebugMode > 0)
	{
		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true); 

		int32 NumInstances = InstanceSceneDataAllocator.GetMaxSize();
		if (ShaderPrint::IsEnabled(View.ShaderPrintData) && NumInstances > 0)
		{
			// This lags by one frame, so may miss some in one frame, also overallocates since we will cull a lot.
			ShaderPrint::RequestSpaceForLines(NumInstances * 12);

			FRDGBufferRef DrawCounterBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 1), TEXT("GPUScene.DebugCounter"));
			FRDGBufferUAVRef DrawCounterUAV = GraphBuilder.CreateUAV(DrawCounterBuffer, PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, DrawCounterUAV, 0u);

			const uint32 MaxPrimitiveNameCount = 128u;
			check(sizeof(FPrimitiveSceneDebugNameInfo) == 8);
			TArray<FPrimitiveSceneDebugNameInfo> SelectedNameInfos;
			TArray<uint8> SelectedNames;
			SelectedNames.Reserve(MaxPrimitiveNameCount * 30u);

			uint32 SelectedCount = 0;
			TArray<uint32> SelectedPrimitiveFlags;
			const int32 BitsPerWord = (sizeof(uint32) * 8U);
			SelectedPrimitiveFlags.Init(0U, FMath::DivideAndRoundUp(Scene.Primitives.Num(), BitsPerWord));
			for (int32 PrimitiveID = 0; PrimitiveID < Scene.PrimitiveSceneProxies.Num(); ++PrimitiveID)
			{
				if (Scene.PrimitiveSceneProxies[PrimitiveID]->IsSelected())
				{
					SelectedPrimitiveFlags[PrimitiveID / BitsPerWord] |= 1U << uint32(PrimitiveID % BitsPerWord);

					// Collect Names
					if (SelectedNameInfos.Num() < MaxPrimitiveNameCount)
					{
						const FString OwnerName = Scene.Primitives[PrimitiveID]->GetFullnameForDebuggingOnly();
						const uint32 NameOffset = SelectedNames.Num();
						const uint32 NameLength = OwnerName.Len();
						for (TCHAR C : OwnerName)
						{
							SelectedNames.Add(uint8(C));
						}

						FPrimitiveSceneDebugNameInfo& NameInfo = SelectedNameInfos.AddDefaulted_GetRef();
						NameInfo.PrimitiveID= PrimitiveID;
						NameInfo.Length		= NameLength;
						NameInfo.Offset		= NameOffset;
						++SelectedCount;
					}
				}
			}

			if (SelectedNameInfos.IsEmpty())
			{
				FPrimitiveSceneDebugNameInfo& NameInfo = SelectedNameInfos.AddDefaulted_GetRef();
				NameInfo.PrimitiveID= ~0;
				NameInfo.Length		= 4;
				NameInfo.Offset		= 0;
				SelectedNames.Add(uint8('N'));
				SelectedNames.Add(uint8('o'));
				SelectedNames.Add(uint8('n'));
				SelectedNames.Add(uint8('e'));
			}

			// Request more characters for printing if needed
			ShaderPrint::RequestSpaceForCharacters(SelectedNames.Num() + SelectedCount * 48u);

			FRDGBufferRef SelectedPrimitiveNames	 = CreateVertexBuffer(GraphBuilder, TEXT("GPUScene.Debug.SelectedPrimitiveNames"), FRDGBufferDesc::CreateBufferDesc(1, SelectedNames.Num()), SelectedNames.GetData(), SelectedNames.Num());
			FRDGBufferRef SelectedPrimitiveNameInfos = CreateStructuredBuffer(GraphBuilder, TEXT("GPUScene.Debug.SelectedPrimitiveNameInfos"), SelectedNameInfos);
			FRDGBufferRef SelectedPrimitiveFlagsRDG  = CreateStructuredBuffer(GraphBuilder, TEXT("GPUScene.Debug.SelectedPrimitiveFlags"), SelectedPrimitiveFlags);

			FGPUSceneDebugRenderCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGPUSceneDebugRenderCS::FParameters>();
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
			PassParameters->GPUSceneResource = ShaderParameters;
			PassParameters->bDrawUpdatedOnly = DebugMode == 3;
			PassParameters->bDrawAll = DebugMode != 2;
			PassParameters->SelectedNameInfoCount = SelectedCount;
			PassParameters->SelectedNameCharacterCount = SelectedCount > 0 ? SelectedNames.Num() : 0;
			PassParameters->SelectedPrimitiveFlags = GraphBuilder.CreateSRV(SelectedPrimitiveFlagsRDG);
			PassParameters->SelectedPrimitiveNameInfos = GraphBuilder.CreateSRV(SelectedPrimitiveNameInfos);
			PassParameters->SelectedPrimitiveNames = GraphBuilder.CreateSRV(SelectedPrimitiveNames, PF_R8_UINT);
			PassParameters->DrawRange = CVarGPUSceneDebugDrawRange.GetValueOnRenderThread();
			PassParameters->RWDrawCounter = DrawCounterUAV;

			FVector PickingRayStart(ForceInit);
			FVector PickingRayDir(ForceInit);
			View.DeprojectFVector2D(View.CursorPos, PickingRayStart, PickingRayDir);

			PassParameters->PickingRayStart = (FVector3f)PickingRayStart;
			PassParameters->PickingRayEnd = FVector3f(PickingRayStart + PickingRayDir * WORLD_MAX);

			auto ComputeShader = View.ShaderMap->GetShader<FGPUSceneDebugRenderCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GPUScene::DebugRender"),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(NumInstances, FGPUSceneDebugRenderCS::NumThreadsPerGroup)
			);
		}
	}
}


void FGPUScene::BeginDeferAllocatorMerges()
{
	if (GGPUSceneAllowDeferredAllocatorMerges != 0)
	{
		InstanceSceneDataAllocator.BeginDeferMerges();
		InstancePayloadDataAllocator.BeginDeferMerges();
		LightmapDataAllocator.BeginDeferMerges();
	}
}


void FGPUScene::EndDeferAllocatorMerges()
{
	if (GGPUSceneAllowDeferredAllocatorMerges != 0)
	{
		InstanceSceneDataAllocator.EndDeferMerges();
		InstancePayloadDataAllocator.EndDeferMerges();
		LightmapDataAllocator.EndDeferMerges();
	}
}


TRange<int32> FGPUScene::CommitPrimitiveCollector(FGPUScenePrimitiveCollector& PrimitiveCollector)
{
	ensure(bInBeginEndBlock);
	ensure(CurrentDynamicContext != nullptr);

	// Make sure we are not trying to commit data that lives in a different context.
	ensure(CurrentDynamicContext == nullptr || CurrentDynamicContext->DymamicPrimitiveUploadData.Find(PrimitiveCollector.UploadData) != INDEX_NONE);

	int32 StartOffset = DynamicPrimitivesOffset;
	DynamicPrimitivesOffset += PrimitiveCollector.Num();

	PrimitiveCollector.UploadData->InstanceSceneDataOffset = AllocateInstanceSceneDataSlots(PrimitiveCollector.NumInstances());
	PrimitiveCollector.UploadData->InstancePayloadDataOffset = AllocateInstancePayloadDataSlots(PrimitiveCollector.NumPayloadDataSlots());

	return TRange<int32>(StartOffset, DynamicPrimitivesOffset);
}


bool FGPUScene::ExecuteDeferredGPUWritePass(FRDGBuilder& GraphBuilder, TArray<FViewInfo>& Views, EGPUSceneGPUWritePass GPUWritePass)
{
	check(GPUWritePass != EGPUSceneGPUWritePass::None && GPUWritePass < EGPUSceneGPUWritePass::Num);
	check(LastDeferredGPUWritePass < GPUWritePass);

	if (!bIsEnabled)
	{
		return false;
	}

	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	// Mark this pass as having executed for the frame
	LastDeferredGPUWritePass = GPUWritePass;

	const uint32 PassIndex = uint32(GPUWritePass);
	if (DeferredGPUWritePassDelegates[PassIndex].Num() == 0)
	{
		// No deferred writes to make for this pass this frame
		return false;
	}

	check(BufferState.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.DeferredGPUWrites - Pass %u", uint32(GPUWritePass));

	UseInternalAccessMode(GraphBuilder);

	FGPUSceneWriteDelegateParams Params;
	Params.GPUWritePass = GPUWritePass;
	GetWriteParameters(GraphBuilder, Params.GPUWriteParams);

	for (const FDeferredGPUWrite& DeferredWrite : DeferredGPUWritePassDelegates[PassIndex])
	{
		FViewInfo* View = Views.FindByPredicate([&DeferredWrite](const FViewInfo& V){ return V.GPUSceneViewId == DeferredWrite.ViewId; });
		checkf(View != nullptr, TEXT("Deferred GPU Write found with no matching view in the view family"));
		
		Params.View = View;
		Params.PrimitiveId = DeferredWrite.PrimitiveId;
		Params.InstanceSceneDataOffset = DeferredWrite.InstanceSceneDataOffset;

		DeferredWrite.DataWriterGPU.Execute(GraphBuilder, Params);
	}

	FRDGExternalAccessQueue ExternalAccessQueue;
	UseExternalAccessMode(ExternalAccessQueue, ERHIAccess::SRVMask, ERHIPipeline::All);
	ExternalAccessQueue.Submit(GraphBuilder);

	DeferredGPUWritePassDelegates[PassIndex].Reset();
	return true;
}


bool FGPUScene::HasPendingGPUWrite(uint32 PrimitiveId) const
{
	for (uint32 PassIndex = uint32(LastDeferredGPUWritePass) + 1; PassIndex < uint32(EGPUSceneGPUWritePass::Num); ++PassIndex)
	{
		if (DeferredGPUWritePassDelegates[PassIndex].FindByPredicate(
			[PrimitiveId](const FDeferredGPUWrite& Write)
			{
				return Write.PrimitiveId == PrimitiveId;
			}))
		{
			return true;
		}
	}

	return false;
}


FGPUSceneDynamicContext::~FGPUSceneDynamicContext()
{
	Release();
}

void FGPUSceneDynamicContext::Release()
{
	for (auto UploadData : DymamicPrimitiveUploadData)
	{
		if (UploadData->InstanceSceneDataOffset != INDEX_NONE)
		{
			GPUScene.FreeInstanceSceneDataSlots(UploadData->InstanceSceneDataOffset, UploadData->TotalInstanceCount);
		}
		if (UploadData->InstancePayloadDataOffset != INDEX_NONE)
		{
			GPUScene.FreeInstancePayloadDataSlots(UploadData->InstancePayloadDataOffset, UploadData->InstancePayloadDataFloat4Count);
		}
		delete UploadData;
	}
	DymamicPrimitiveUploadData.Empty();
}

FGPUScenePrimitiveCollector::FUploadData* FGPUSceneDynamicContext::AllocateDynamicPrimitiveData()
{
	LLM_SCOPE_BYTAG(GPUScene);

	FGPUScenePrimitiveCollector::FUploadData* UploadData = new FGPUScenePrimitiveCollector::FUploadData;
	DymamicPrimitiveUploadData.Add(UploadData);
	return UploadData;
}

/**
 * Fills in the FGPUSceneWriterParameters to use for read/write access to the GPU Scene.
 */
void FGPUScene::GetWriteParameters(FRDGBuilder& GraphBuilder, FGPUSceneWriterParameters& GPUSceneWriterParametersOut)
{
	GPUSceneWriterParametersOut.GPUSceneFrameNumber = SceneFrameNumber;
	GPUSceneWriterParametersOut.GPUSceneInstanceSceneDataSOAStride = InstanceSceneDataSOAStride;
	GPUSceneWriterParametersOut.GPUSceneNumAllocatedInstances = InstanceSceneDataAllocator.GetMaxSize();
	GPUSceneWriterParametersOut.GPUSceneNumAllocatedPrimitives = DynamicPrimitivesOffset;
	GPUSceneWriterParametersOut.GPUSceneInstanceSceneDataRW = GraphBuilder.CreateUAV(BufferState.InstanceSceneDataBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	GPUSceneWriterParametersOut.GPUSceneInstancePayloadDataRW = GraphBuilder.CreateUAV(BufferState.InstancePayloadDataBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	GPUSceneWriterParametersOut.GPUScenePrimitiveSceneDataRW = GraphBuilder.CreateUAV(BufferState.PrimitiveBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
}

/**
 * Compute shader to project and invalidate the rectangles of given instances.
 */
class FGPUSceneSetInstancePrimitiveIdCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGPUSceneSetInstancePrimitiveIdCS);
	SHADER_USE_PARAMETER_STRUCT(FGPUSceneSetInstancePrimitiveIdCS, FGlobalShader)
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUScene::FInstanceGPULoadBalancer::FShaderParameters, BatcherParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneWriterParameters, GPUSceneWriterParameters)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr int32 NumThreadsPerGroup = FGPUScene::FInstanceGPULoadBalancer::ThreadGroupSize;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FGPUScene::FInstanceGPULoadBalancer::SetShaderDefines(OutEnvironment);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FGPUSceneSetInstancePrimitiveIdCS, "/Engine/Private/GPUScene/GPUSceneDataManagement.usf", "GPUSceneSetInstancePrimitiveIdCS", SF_Compute);


void FGPUScene::AddUpdatePrimitiveIdsPass(FRDGBuilder& GraphBuilder, FInstanceGPULoadBalancer& IdOnlyUpdateItems)
{
	if (!IdOnlyUpdateItems.IsEmpty())
	{
		FGPUSceneSetInstancePrimitiveIdCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGPUSceneSetInstancePrimitiveIdCS::FParameters>();

		IdOnlyUpdateItems.Upload(GraphBuilder).GetShaderParameters(GraphBuilder, PassParameters->BatcherParameters);

		GetWriteParameters(GraphBuilder, PassParameters->GPUSceneWriterParameters);

		auto ComputeShader = GetGlobalShaderMap(FeatureLevel)->GetShader<FGPUSceneSetInstancePrimitiveIdCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GPUScene::SetInstancePrimitiveIdCS"),
			ComputeShader,
			PassParameters,
			IdOnlyUpdateItems.GetWrappedCsGroupCount()
		);
	}
}

void FGPUSceneCompactInstanceData::Init(const FGPUScenePrimitiveCollector* PrimitiveCollector, int32 PrimitiveId)
{
	FMatrix44f LocalToRelativeWorld = FMatrix44f::Identity;
	int32 DynamicPrimitiveId = PrimitiveId;
	if (PrimitiveCollector && PrimitiveCollector->UploadData && !PrimitiveCollector->GetPrimitiveIdRange().IsEmpty())
	{
		DynamicPrimitiveId = PrimitiveCollector->GetPrimitiveIdRange().GetLowerBoundValue() + PrimitiveId;
		if (PrimitiveCollector->GetPrimitiveIdRange().Contains(DynamicPrimitiveId))
		{
			const FPrimitiveUniformShaderParameters& PrimitiveData = *PrimitiveCollector->UploadData->PrimitiveData[PrimitiveId].ShaderParams;
			LocalToRelativeWorld = PrimitiveData.LocalToRelativeWorld;
		}
	}
	InstanceOriginAndId		= LocalToRelativeWorld.GetOrigin();
	InstanceTransform1		= LocalToRelativeWorld.GetScaledAxis(EAxis::X);
	InstanceTransform2		= LocalToRelativeWorld.GetScaledAxis(EAxis::Y);
	InstanceTransform3		= LocalToRelativeWorld.GetScaledAxis(EAxis::Z);
	InstanceOriginAndId.W	= *(float*)&DynamicPrimitiveId;
	InstanceAuxData			= FVector4f(0);
}

void FGPUSceneCompactInstanceData::Init(const FScene* Scene, int32 PrimitiveId)
{
	FMatrix44f LocalToRelativeWorld = FMatrix44f::Identity;
	if (Scene && PrimitiveId >= 0 && PrimitiveId < Scene->PrimitiveTransforms.Num())
	{
		const FMatrix LocalToWorld = Scene->PrimitiveTransforms[PrimitiveId];
		const FLargeWorldRenderPosition AbsoluteOrigin(LocalToWorld.GetOrigin());
		LocalToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrix(AbsoluteOrigin.GetTileOffset(), LocalToWorld);
	}
	InstanceOriginAndId		= LocalToRelativeWorld.GetOrigin();
	InstanceTransform1		= LocalToRelativeWorld.GetScaledAxis(EAxis::X);
	InstanceTransform2		= LocalToRelativeWorld.GetScaledAxis(EAxis::Y);
	InstanceTransform3		= LocalToRelativeWorld.GetScaledAxis(EAxis::Z);
	InstanceOriginAndId.W	= *(float*)&PrimitiveId;
	InstanceAuxData			= FVector4f(0);
}

void FGPUScene::AddClearInstancesPass(FRDGBuilder& GraphBuilder)
{
	FInstanceGPULoadBalancer ClearIdData;
#if LOG_INSTANCE_ALLOCATIONS
	FString RangesStr;
#endif
	for (FInstanceRange Range : InstanceRangesToClear)
	{
		ClearIdData.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries, INVALID_PRIMITIVE_ID);
#if LOG_INSTANCE_ALLOCATIONS
		RangesStr.Appendf(TEXT("[%6d, %6d), "), Range.InstanceSceneDataOffset, Range.InstanceSceneDataOffset + Range.NumInstanceSceneDataEntries);
#endif
	}
#if LOG_INSTANCE_ALLOCATIONS
	UE_LOG(LogTemp, Warning, TEXT("AddClearInstancesPass: \n%s"), *RangesStr);
#endif
	AddUpdatePrimitiveIdsPass(GraphBuilder, ClearIdData);
	InstanceRangesToClear.Empty();
}
