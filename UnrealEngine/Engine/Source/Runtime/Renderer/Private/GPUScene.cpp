// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUScene.cpp
=============================================================================*/

#include "GPUScene.h"
#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "Rendering/NaniteResources.h"
#include "Async/ParallelFor.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "NaniteSceneProxy.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"
#include "InstanceUniformShaderParameters.h"
#include "ShaderPrint.h"
#include "RenderCore.h"
#include "LightSceneData.h"
#include "LightSceneProxy.h"
#include "SystemTextures.h"
#include "SceneDefinitions.h"
#include "PrimitiveSceneShaderData.h"
#include "RendererOnScreenNotification.h"
#include "InstanceCulling/InstanceCullingOcclusionQuery.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "InstanceDataSceneProxy.h"
#include "SceneRendererInterface.h"

// Useful for debugging
#define FORCEINLINE_GPUSCENE FORCEINLINE
//#define FORCEINLINE_GPUSCENE

// Defaults to being disabled, enable using the command line argument: -CsvCategory GPUScene
CSV_DEFINE_CATEGORY(GPUScene, false);

DEFINE_GPU_STAT(GPUSceneUpdate);

#define LOG_INSTANCE_ALLOCATIONS 0

static void ConstructDefault(FGPUSceneResourceParameters& GPUScene, FRDGBuilder& GraphBuilder)
{
	FRDGBufferRef DummyBufferVec4 = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f));
	GPUScene.GPUSceneInstanceSceneData = GraphBuilder.CreateSRV(DummyBufferVec4);
	GPUScene.GPUSceneInstancePayloadData = GraphBuilder.CreateSRV(DummyBufferVec4);
	GPUScene.GPUScenePrimitiveSceneData = GraphBuilder.CreateSRV(DummyBufferVec4);
	GPUScene.GPUSceneLightmapData = GraphBuilder.CreateSRV(DummyBufferVec4);
	FRDGBufferRef DummyBufferLight = GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FLightSceneData));
	GPUScene.GPUSceneLightData = GraphBuilder.CreateSRV(DummyBufferLight);
	GPUScene.InstanceDataSOAStride = 0;
	GPUScene.GPUSceneFrameNumber = 0;
	GPUScene.NumInstances = 0;
	GPUScene.NumScenePrimitives = 0;
}
IMPLEMENT_SCENE_UB_STRUCT(FGPUSceneResourceParameters, GPUScene, ConstructDefault);

static TAutoConsoleVariable<int32> CVarGPUSceneUploadEveryFrame(
	TEXT("r.GPUScene.UploadEveryFrame"),
	0,
	TEXT("Whether to upload the entire scene's primitive data every frame.  Useful for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarGPUSceneMaxPooledUploadBufferSize(
	TEXT("r.GPUScene.MaxPooledUploadBufferSize"),
	256000,
	TEXT("Maximum size of GPU Scene upload buffer size to pool."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarGPUSceneParallelUpdate(
	TEXT("r.GPUScene.ParallelUpdate"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
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

static TAutoConsoleVariable<int32> CVarGPUSceneUseGrowOnlyAllocationPolicy(
	TEXT("r.GPUScene.UseGrowOnlyAllocationPolicy"),
	0,
	TEXT("Deprecated 5.3. If set to 1 the allocators used for GPU-scene instances and similar will use a grow-only allocation policy to mimic the behavior in 5.2 and earlier.\n")
	TEXT("  Disabled by default, which means that the buffers can shrink as well as grow."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
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

const FPrimitiveUniformShaderParameters* FGPUScenePrimitiveCollector::GetPrimitiveShaderParameters(int32 DrawPrimitiveId) const
{
	if (UploadData != nullptr && (DrawPrimitiveId & GPrimIDDynamicFlag) != 0)
	{
		int32 DynamicPrimitiveIndex = DrawPrimitiveId & (~GPrimIDDynamicFlag);
		if (UploadData->PrimitiveData.IsValidIndex(DynamicPrimitiveIndex))
		{
			return UploadData->PrimitiveData[DynamicPrimitiveIndex].ShaderParams;
		}
	}
	return nullptr;
}

#if DO_CHECK

void FGPUScenePrimitiveCollector::CheckPrimitiveProcessed(uint32 PrimitiveIndex, const FGPUScene& GPUScene) const
{
	checkf(UploadData != nullptr && bCommitted, TEXT("Dynamic Primitive index %u has not been fully processed. The collector hasn't collected anything or hasn't been uploaded."), PrimitiveIndex);

	if (UploadData != nullptr)
	{
		checkf(PrimitiveIndex < uint32(UploadData->PrimitiveData.Num()), TEXT("Dynamic Primitive index %u has not been fully processed. The specified index is out of range [0,%d)."), PrimitiveIndex, UploadData->PrimitiveData.Num());

		// Early out to avoid the HasPendingGPUWrite check for cases where it cannot happen anyway.
		const FMeshBatchDynamicPrimitiveData& SourceData = UploadData->PrimitiveData[PrimitiveIndex].SourceData;
		if (!SourceData.DataWriterGPU.IsBound() || SourceData.DataWriterGPUPass == EGPUSceneGPUWritePass::None)
		{
			return;
		}
	}

	// If the GPU scene still has a pending deferred write for the primitive, then it has not been fully processed yet
	const uint32 PrimitiveId = GetPrimitiveIdRange().GetLowerBoundValue() + PrimitiveIndex;
	checkf(!GPUScene.HasPendingGPUWrite(PrimitiveId), TEXT("Dynamic Primitive index %u has not been fully processed. The GPU scene still has a pending deferred write for the primitive, it has not been fully processed yet."), PrimitiveIndex);
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

/**
 * Info needed by the uploader to prepare to upload a primitive.
 */
struct FPrimitiveUploadInfoHeader
{
	uint32 PrimitiveID = INVALID_PRIMITIVE_ID;

	/** Optional */
	int32 NumInstanceUploads = 0;
	int32 NumInstancePayloadDataUploads = 0;
	int32 LightmapUploadCount = 0;

	/** NaniteSceneProxy must be set if the proxy is a Nanite proxy */
	const Nanite::FSceneProxyBase* NaniteSceneProxy = nullptr;
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
};

/**
 * Info required by the uploader to update the instances that belong to a primitive.
 */
struct FInstanceUploadInfo
{
	const FInstanceSceneDataBuffers *InstanceSceneDataBuffers = nullptr;
	TConstArrayView<FInstanceSceneData> PrimitiveInstances;
	int32 InstanceSceneDataOffset = INDEX_NONE;
	int32 InstancePayloadDataOffset = INDEX_NONE;
	int32 InstancePayloadDataStride = 0;
	int32 InstancePayloadExtensionCount = 0;
	int32 InstanceCustomDataCount = 0;

	// Optional per-instance data views
	TConstArrayView<FInstanceDynamicData> InstanceDynamicData;
	TConstArrayView<FVector4f> InstanceLightShadowUVBias;
	TConstArrayView<float> InstanceCustomData;
	TConstArrayView<float> InstanceRandomID;
	TConstArrayView<uint32> InstanceHierarchyOffset;
	TConstArrayView<FRenderBounds> InstanceLocalBounds;
	TConstArrayView<FVector4f> InstancePayloadExtension;
#if WITH_EDITOR
	TConstArrayView<uint32> InstanceEditorData;
#endif

	uint32 InstanceFlags = 0x0;

	FRenderTransform PrimitiveToWorld;
	FRenderTransform PrevPrimitiveToWorld;
	int32 PrimitiveID = INDEX_NONE;
	uint32 LastUpdateSceneFrameNumber = ~uint32(0);
	int32 NumInstances = 0;
	bool bIsPrimitiveForceHidden = false;
#if DO_CHECK
	const FPrimitiveSceneInfo *PrimitiveSceneInfo = nullptr;
#endif
};

#if DO_CHECK
void ValidateInstanceUploadInfo(const FInstanceUploadInfo& UploadInfo, FRDGBuffer* InstancePayloadDataBuffer)
{
	const bool bHasRandomID			= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_RANDOM) != 0u;
	const bool bHasCustomData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_CUSTOM_DATA) != 0u;
	const bool bHasDynamicData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_DYNAMIC_DATA) != 0u;
	const bool bHasLightShadowUVBias = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LIGHTSHADOW_UV_BIAS) != 0u;
	const bool bHasHierarchyOffset	= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_HIERARCHY_OFFSET) != 0u;
	const bool bHasLocalBounds		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS) != 0u;
	const bool bHasPayloadExtension	= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_PAYLOAD_EXTENSION) != 0u;
#if WITH_EDITOR
	const bool bHasEditorData		= (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_EDITOR_DATA) != 0u;
#endif

	check(!bHasRandomID || UploadInfo.InstanceRandomID.Num() == UploadInfo.NumInstances);
	check(UploadInfo.InstanceLightShadowUVBias.Num()	== (bHasLightShadowUVBias ? UploadInfo.NumInstances : 0));
	check(UploadInfo.InstanceHierarchyOffset.Num()		== (bHasHierarchyOffset	? UploadInfo.NumInstances : 0));
#if WITH_EDITOR
	check(UploadInfo.InstanceEditorData.Num() == (bHasEditorData ? UploadInfo.NumInstances : 0));
#endif

	if (bHasCustomData)
	{
		//check(UploadInfo.InstanceCustomDataCount > 0);
		check(UploadInfo.InstanceCustomDataCount * UploadInfo.NumInstances == UploadInfo.InstanceCustomData.Num());
	}
	else
	{
		check(UploadInfo.InstanceCustomDataCount == 0);
	}

	if (bHasPayloadExtension)
	{
		check(UploadInfo.InstancePayloadExtensionCount > 0);
		check(UploadInfo.InstancePayloadExtensionCount <= PRIMITIVE_SCENE_DATA_MAX_PAYLOAD_EXTENSION_SIZE);
		check(UploadInfo.InstancePayloadExtensionCount * UploadInfo.NumInstances == UploadInfo.InstancePayloadExtension.Num());
	}
	else
	{
		check(UploadInfo.InstancePayloadExtensionCount == 0 && UploadInfo.InstancePayloadExtension.Num() == 0);
	}

	// RandomID is not stored in the payload but in the instance scene data.
	const bool bHasAnyPayloadData = bHasHierarchyOffset || bHasLocalBounds || bHasDynamicData || bHasLightShadowUVBias || bHasCustomData|| bHasPayloadExtension;

	if (bHasAnyPayloadData)
	{
		check(InstancePayloadDataBuffer);
		check(UploadInfo.InstancePayloadDataOffset != INDEX_NONE);

		const int32 PayloadBufferSize = InstancePayloadDataBuffer->GetSize() / InstancePayloadDataBuffer->GetStride();
		check(UploadInfo.InstancePayloadDataOffset < PayloadBufferSize);
	}

	if (UploadInfo.InstanceSceneDataBuffers != nullptr)
	{
		if (UploadInfo.InstanceSceneDataBuffers->GetNumInstances() > 0 && !ensure(UploadInfo.InstanceSceneDataBuffers->GetPrimitiveToRelativeWorld().Equals(UploadInfo.PrimitiveToWorld)))
		{
			UE_LOG(LogTemp, Warning, TEXT("Mismatched Primitive transform! primitive ID %d"), UploadInfo.PrimitiveID);
		}
		check(UploadInfo.InstanceSceneDataBuffers->GetReadView().InstanceToPrimitiveRelative.Num() == UploadInfo.NumInstances);

		// TODO: we may alias with the InstanceToPrimitiveRelative if no per-instance transforms have been requested...
		check(UploadInfo.InstanceSceneDataBuffers->GetReadView().PrevInstanceToPrimitiveRelative.Num() == (bHasDynamicData ? UploadInfo.NumInstances : 0));
	}
	else
	{
		check(UploadInfo.InstanceDynamicData.Num()	== (bHasDynamicData	? UploadInfo.NumInstances : 0));
		check(UploadInfo.NumInstances == UploadInfo.PrimitiveInstances.Num()  || UploadInfo.NumInstances == 1 && UploadInfo.PrimitiveInstances.IsEmpty());
	}

	if (UploadInfo.PrimitiveSceneInfo)
	{
		check(UploadInfo.PrimitiveSceneInfo->GetNumInstanceSceneDataEntries() == UploadInfo.NumInstances);
	}

}
#else
FORCEINLINE void ValidateInstanceUploadInfo(const FInstanceUploadInfo& , FRDGBuffer* ) {}
#endif

/**
 * Info required by the uploader to update the lightmap data for a primitive.
 */
struct FLightMapUploadInfo
{
	FPrimitiveSceneProxy::FLCIArray LCIs;
	int32 LightmapDataOffset = 0;
};

/**
 * Implements a thin data abstraction such that the UploadGeneral function can upload primitive data from
 * both scene primitives and dynamic primitives (which are not stored in the same way). 
 * Note: handling of Nanite material table upload data is not abstracted (since at present it can only come via the scene primitives).
 */
struct FUploadDataSourceAdapterScenePrimitives
{
	static constexpr bool bUpdateNaniteMaterialTables = true;

	FUploadDataSourceAdapterScenePrimitives(FScene& InScene, uint32 InSceneFrameNumber, TArray<FPersistentPrimitiveIndex> InPrimitivesToUpdate, TArray<EPrimitiveDirtyState> InPrimitiveDirtyState)
		: Scene(InScene)
		, SceneFrameNumber(InSceneFrameNumber)
		, PrimitivesToUpdate(MoveTemp(InPrimitivesToUpdate))
		, PrimitiveDirtyState(MoveTemp(InPrimitiveDirtyState))
	{}

	/**
	 * Return the number of primitives to upload N, GetPrimitiveInfoHeader/GetPrimitiveShaderData will be called with ItemIndex in [0,N).
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
	FORCEINLINE_GPUSCENE void GetPrimitiveInfoHeader(int32 ItemIndex, FPrimitiveUploadInfoHeader& PrimitiveUploadInfo) const
	{
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitivesToUpdate[ItemIndex];
		const int32 PrimitiveID = Scene.GetPrimitiveIndex(PersistentPrimitiveIndex);
		check(Scene.PrimitiveSceneProxies.IsValidIndex(PrimitiveID));

		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

		PrimitiveUploadInfo.PrimitiveID = uint32(PersistentPrimitiveIndex.Index);
		PrimitiveUploadInfo.LightmapUploadCount = PrimitiveSceneInfo->GetNumLightmapDataEntries();
		PrimitiveUploadInfo.NaniteSceneProxy = PrimitiveSceneProxy->IsNaniteMesh() ? static_cast<const Nanite::FSceneProxyBase*>(PrimitiveSceneProxy) : nullptr;
		PrimitiveUploadInfo.PrimitiveSceneInfo = PrimitiveSceneInfo;

		PrimitiveUploadInfo.NumInstanceUploads = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
		PrimitiveUploadInfo.NumInstancePayloadDataUploads = PrimitiveSceneInfo->GetInstancePayloadDataStride() * PrimitiveUploadInfo.NumInstanceUploads;
	}

	FORCEINLINE uint32 PackFlags(FInstanceDataFlags Flags) const
	{
		uint32 PackedFlags = 0x0;
		PackedFlags |= Flags.bHasPerInstanceRandom          ? INSTANCE_SCENE_DATA_FLAG_HAS_RANDOM              : 0u;
		PackedFlags |= Flags.bHasPerInstanceCustomData      ? INSTANCE_SCENE_DATA_FLAG_HAS_CUSTOM_DATA         : 0u;
		PackedFlags |= Flags.bHasPerInstanceDynamicData     ? INSTANCE_SCENE_DATA_FLAG_HAS_DYNAMIC_DATA        : 0u;
		PackedFlags |= Flags.bHasPerInstanceLMSMUVBias      ? INSTANCE_SCENE_DATA_FLAG_HAS_LIGHTSHADOW_UV_BIAS : 0u;
		PackedFlags |= Flags.bHasPerInstanceHierarchyOffset ? INSTANCE_SCENE_DATA_FLAG_HAS_HIERARCHY_OFFSET    : 0u;
		PackedFlags |= Flags.bHasPerInstanceLocalBounds     ? INSTANCE_SCENE_DATA_FLAG_HAS_LOCAL_BOUNDS        : 0u;
		PackedFlags |= Flags.bHasPerInstancePayloadExtension? INSTANCE_SCENE_DATA_FLAG_HAS_PAYLOAD_EXTENSION   : 0u;
	#if WITH_EDITOR
		PackedFlags |= Flags.bHasPerInstanceEditorData  ? INSTANCE_SCENE_DATA_FLAG_HAS_EDITOR_DATA         : 0u;
	#endif

		return PackedFlags;
	}

	/**
	 * Populate the primitive info for a given item index.
	 * 
	 */
	FORCEINLINE_GPUSCENE void GetPrimitiveShaderData(int32 ItemIndex, FVector4f* RESTRICT OutData) const
	{
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitivesToUpdate[ItemIndex];
		const int32 PrimitiveID = Scene.GetPrimitiveIndex(PersistentPrimitiveIndex);
		check(Scene.PrimitiveSceneProxies.IsValidIndex(PrimitiveID));

		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		FPrimitiveSceneShaderData::BuildDataFromProxy(PrimitiveSceneProxy, OutData);
	}

	FORCEINLINE_GPUSCENE void GetInstanceInfo(int32 ItemIndex, FInstanceUploadInfo& InstanceUploadInfo) const
	{
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitivesToUpdate[ItemIndex];
		const int32 PrimitiveID = Scene.GetPrimitiveIndex(PersistentPrimitiveIndex);

		check(Scene.PrimitiveSceneProxies.IsValidIndex(PrimitiveID));
		check(PrimitiveDirtyState[PersistentPrimitiveIndex.Index] != EPrimitiveDirtyState::Removed);

		FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[PrimitiveID];
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
#if DO_CHECK
		InstanceUploadInfo.PrimitiveSceneInfo = PrimitiveSceneInfo;
#endif

		InstanceUploadInfo.InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
		check(InstanceUploadInfo.InstanceSceneDataOffset >= 0);
		InstanceUploadInfo.InstancePayloadDataOffset = PrimitiveSceneInfo->GetInstancePayloadDataOffset();
		InstanceUploadInfo.InstancePayloadDataStride = PrimitiveSceneInfo->GetInstancePayloadDataStride();
		InstanceUploadInfo.LastUpdateSceneFrameNumber = SceneFrameNumber;
		InstanceUploadInfo.PrimitiveID = PersistentPrimitiveIndex.Index;
		// HACK: ignoring IsForceHidden for non-Nanite due to issues that cropped up with water rendering.
		// TODO: Remove the IsNaniteMesh() check
		InstanceUploadInfo.bIsPrimitiveForceHidden = PrimitiveSceneProxy->IsNaniteMesh() && PrimitiveSceneProxy->IsForceHidden();

		InstanceUploadInfo.InstanceSceneDataBuffers = PrimitiveSceneInfo->GetInstanceSceneDataBuffers();
		if (InstanceUploadInfo.InstanceSceneDataBuffers)
		{
			const FInstanceSceneDataBuffers::FReadView InstanceSceneDataBuffers = InstanceUploadInfo.InstanceSceneDataBuffers->GetReadView();
			InstanceUploadInfo.NumInstances = InstanceUploadInfo.InstanceSceneDataBuffers->GetNumInstances();
			InstanceUploadInfo.InstanceFlags = PackFlags(InstanceSceneDataBuffers.Flags);
			InstanceUploadInfo.InstanceLightShadowUVBias = InstanceSceneDataBuffers.InstanceLightShadowUVBias;
			InstanceUploadInfo.InstanceCustomData = InstanceSceneDataBuffers.InstanceCustomData;
			InstanceUploadInfo.InstanceRandomID = InstanceSceneDataBuffers.InstanceRandomIDs;
			InstanceUploadInfo.InstanceHierarchyOffset = InstanceSceneDataBuffers.InstanceHierarchyOffset;
			InstanceUploadInfo.InstancePayloadExtension = InstanceSceneDataBuffers.InstancePayloadExtension;
			InstanceUploadInfo.InstanceLocalBounds = InstanceSceneDataBuffers.InstanceLocalBounds;
#if WITH_EDITOR
			InstanceUploadInfo.InstanceEditorData = InstanceSceneDataBuffers.InstanceEditorData;
#endif
#if DO_CHECK
			// This is already precomputed in the InstanceSceneDataBuffers and we don't need to do it again here, except for validation purposes
			// TODO: this validation should (also?) move elsewhere and validate that the transform on RT matches that on GT
			const FMatrix LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();
			const FDFVector3 AbsoluteOrigin(LocalToWorld.GetOrigin());
			InstanceUploadInfo.PrimitiveToWorld = FDFMatrix::MakeToRelativeWorldMatrix(AbsoluteOrigin.High, LocalToWorld).M;
#endif

		}
		else
		{
			// Old path, only taken for uninstanced primitives.
			const FMatrix LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();
			const FDFVector3 AbsoluteOrigin(LocalToWorld.GetOrigin());
			InstanceUploadInfo.PrimitiveToWorld = FDFMatrix::MakeToRelativeWorldMatrix(AbsoluteOrigin.High, LocalToWorld).M;

			InstanceUploadInfo.InstanceFlags = 0u;
			check(InstanceUploadInfo.InstancePayloadDataOffset == INDEX_NONE && InstanceUploadInfo.InstancePayloadDataStride == 0);

			// empty array signals that we should use the PrimitiveToWorld as the instance transform (and that it is therefore pre-transformed).
			InstanceUploadInfo.PrimitiveInstances = TConstArrayView<FInstanceSceneData>();
			InstanceUploadInfo.InstanceDynamicData = TConstArrayView<FInstanceDynamicData>();

#if 0
			// NOTE: We only need this if not using the InstanceSceneDataBuffers and if the old path were to support dynamic data
			{
				bool bHasPrecomputedVolumetricLightmap{};
				bool bOutputVelocity{};
				int32 SingleCaptureIndex{};

				FMatrix PreviousLocalToWorld;
				Scene.GetPrimitiveUniformShaderParameters_RenderThread(PrimitiveSceneInfo, bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
				InstanceUploadInfo.PrevPrimitiveToWorld = FDFMatrix::MakeClampedToRelativeWorldMatrix(AbsoluteOrigin.High, PreviousLocalToWorld).M;
			}
#endif
			InstanceUploadInfo.InstanceLightShadowUVBias = TConstArrayView<FVector4f>();
			InstanceUploadInfo.InstanceCustomData = TConstArrayView<float>();
			InstanceUploadInfo.InstanceRandomID = TConstArrayView<float>();
			InstanceUploadInfo.InstanceHierarchyOffset = TConstArrayView<uint32>();
			InstanceUploadInfo.InstancePayloadExtension = TConstArrayView<FVector4f>();
			InstanceUploadInfo.NumInstances = 1;

#if WITH_EDITOR
			InstanceUploadInfo.InstanceEditorData = TConstArrayView<uint32>();
#endif
		}

		InstanceUploadInfo.InstancePayloadExtensionCount = 0;
		if (InstanceUploadInfo.InstancePayloadExtension.Num() > 0)
		{
			InstanceUploadInfo.InstancePayloadExtensionCount = InstanceUploadInfo.InstancePayloadExtension.Num() / InstanceUploadInfo.NumInstances;
		}

		InstanceUploadInfo.InstanceCustomDataCount = 0;
		if (InstanceUploadInfo.InstanceCustomData.Num() > 0)
		{
			InstanceUploadInfo.InstanceCustomDataCount = InstanceUploadInfo.InstanceCustomData.Num() / InstanceUploadInfo.NumInstances;
		}

		// Only trigger upload if this primitive has instances
		check(InstanceUploadInfo.NumInstances > 0);
	}

	FORCEINLINE_GPUSCENE bool GetLightMapInfo(int32 ItemIndex, FLightMapUploadInfo &UploadInfo) const
	{
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitivesToUpdate[ItemIndex];
		const int32 PrimitiveID = Scene.GetPrimitiveIndex(PersistentPrimitiveIndex);
		if (Scene.PrimitiveSceneProxies.IsValidIndex(PrimitiveID))
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
	TArray<FPersistentPrimitiveIndex> PrimitivesToUpdate;
	TArray<EPrimitiveDirtyState> PrimitiveDirtyState;
};

void FGPUScene::SetEnabled(ERHIFeatureLevel::Type InFeatureLevel)
{
	FeatureLevel = InFeatureLevel;
	bIsEnabled = UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel);
}

FGPUScene::FGPUScene(FScene &InScene)
	: bUpdateAllPrimitives(false)
	, InstanceSceneDataSOAStride(0)
	, InstancePayloadDataAllocator(CVarGPUSceneUseGrowOnlyAllocationPolicy.GetValueOnAnyThread() != 0)
	, LightmapDataAllocator(CVarGPUSceneUseGrowOnlyAllocationPolicy.GetValueOnAnyThread() != 0)
	, Scene(InScene)
	, InstanceSceneDataAllocator(CVarGPUSceneUseGrowOnlyAllocationPolicy.GetValueOnAnyThread() != 0)
{
#if !UE_BUILD_SHIPPING
	ScreenMessageDelegate = FRendererOnScreenNotification::Get().AddLambda([this](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
	{
		if (InstanceSceneDataSOAStride < MaxInstancesDuringPrevUpdate)
		{
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(FString::Printf(TEXT("GPU-Scene Instance data overflow detected, reduce instance count to avoid rendering artifacts"))));
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(FString::Printf(TEXT("  Max allocated ID %d, instance buffer size: %dM"), MaxInstancesDuringPrevUpdate, InstanceSceneDataSOAStride >> 20)));
			if (!bLoggedInstanceOverflow )
			{
				UE_LOG(LogRenderer, Warning, TEXT("GPU-Scene Instance data overflow detected, reduce instance count to avoid rendering artifacts.\n")
					TEXT(" Max allocated ID %d (%0.3fM), instance buffer size: %dM"), MaxInstancesDuringPrevUpdate, double(MaxInstancesDuringPrevUpdate) / (1024.0 * 1024.0),  InstanceSceneDataSOAStride >> 20);
				bLoggedInstanceOverflow = true;
			}
		}
		else
		{
			bLoggedInstanceOverflow = false;
		}
	});
#endif
}


FGPUScene::~FGPUScene()
{
#if !UE_BUILD_SHIPPING
	FRendererOnScreenNotification::Get().Remove(ScreenMessageDelegate);
#endif
}

void FGPUScene::BeginRender(FRDGBuilder& GraphBuilder, FGPUSceneDynamicContext &GPUSceneDynamicContext)
{
	ensure(!bInBeginEndBlock);
	ensure(CurrentDynamicContext == nullptr);

	CurrentDynamicContext = &GPUSceneDynamicContext;
	bInBeginEndBlock = true;
	check(!CachedRegisteredBuffers.IsValid());
	if (bIsEnabled)
	{
		check(NumScenePrimitives == Scene.Primitives.Num());
		// Should always be reset to this as the neutral state.
		check(DynamicPrimitivesOffset == Scene.GetMaxPersistentPrimitiveIndex());
		// Do it anyway for old times sake
		DynamicPrimitivesOffset = Scene.GetMaxPersistentPrimitiveIndex();

		CachedRegisteredBuffers = RegisterBuffers(GraphBuilder);
	}
}

void FGPUScene::EndRender()
{
	ensure(bInBeginEndBlock);
	ensure(CurrentDynamicContext != nullptr);
	check(!bIsEnabled || DynamicPrimitivesOffset >= NumScenePrimitives);

	// Pop all dynamic primitives off the stack
	DynamicPrimitivesOffset = Scene.GetMaxPersistentPrimitiveIndex();
	bInBeginEndBlock = false;
	CurrentDynamicContext = nullptr;
	CachedRegisteredBuffers = {};
}

void FGPUScene::UpdateGPULights(FRDGBuilder& GraphBuilder, const UE::Tasks::FTask& PrerequisiteTask)
{
	FRDGUploadData<FLightSceneData> LightData(GraphBuilder, FMath::Max(1, Scene.Lights.GetMaxIndex()));

	GraphBuilder.AddSetupTask([this, LightData]
	{
		SCOPED_NAMED_EVENT(UpdateGPUScene_Lights, FColor::Green);
		const bool bAllowStaticLighting = IsStaticLightingAllowed();

		for (int32 Index = 0; Index < Scene.Lights.GetMaxIndex(); ++Index)
		{
			if (Scene.Lights.IsAllocated(Index))
			{
				InitLightData(Scene.Lights[Index], bAllowStaticLighting, LightData[Index]);
			}
			else
			{
				LightData[Index].WorldPosition = FDFVector3{};
				LightData[Index].Color = FVector3f::ZeroVector;
				LightData[Index].InvRadius = 0.0f;
			}
		}

	}, PrerequisiteTask);

	const uint32 LightDataBufferNumElements = FMath::RoundUpToPowerOfTwo(FMath::Max(Scene.Lights.GetMaxIndex(), InitialBufferSize));
	const FRDGBufferDesc LightDataBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FLightSceneData), LightDataBufferNumElements);

	FRDGBuffer* LightDataBufferRDG;
	if (LightDataBuffer == nullptr)
	{
		LightDataBufferRDG = GraphBuilder.CreateBuffer(LightDataBufferDesc, TEXT("GPUScene.LightData"));
		LightDataBuffer = GraphBuilder.ConvertToExternalBuffer(LightDataBufferRDG);
	}
	else
	{
		const uint32 BufferSizeNew = LightDataBufferDesc.GetSize();
		const uint32 BufferSizeOld = LightDataBuffer->GetCommittedSize();

		if (BufferSizeNew == BufferSizeOld)
		{
			LightDataBufferRDG = GraphBuilder.RegisterExternalBuffer(LightDataBuffer);
		}
		else
		{
			LightDataBufferRDG = GraphBuilder.CreateBuffer(LightDataBufferDesc, TEXT("GPUScene.LightData"));
			LightDataBuffer = GraphBuilder.ConvertToExternalBuffer(LightDataBufferRDG);

			// Currently whole contents of LightDataBuffer are uploaded so don't need to copy previous contents when resizing
		}

	}

	GraphBuilder.QueueBufferUpload<FLightSceneData>(LightDataBufferRDG, LightData, ERDGInitialDataFlags::NoCopy);
}

void FGPUScene::InitLightData(const FLightSceneInfoCompact& LightInfoCompact, bool bAllowStaticLighting, FLightSceneData& DataOut)
{
	const FLightSceneInfo& LightInfo = *LightInfoCompact.LightSceneInfo;
	const FLightSceneProxy& LightProxy = *LightInfo.Proxy;

	FLightRenderParameters LightParams;
	LightProxy.GetLightShaderParameters(LightParams);

	if (LightProxy.IsInverseSquared())
	{
		LightParams.FalloffExponent = 0;
	}

	// FLightRenderParameters fields
	DataOut.WorldPosition = FDFVector3{ LightParams.WorldPosition };
	DataOut.InvRadius = LightParams.InvRadius;
	DataOut.Color = LightParams.Color;
	DataOut.FalloffExponent = LightParams.FalloffExponent;
	DataOut.Direction = LightParams.Direction;
	DataOut.SpecularScale = LightParams.SpecularScale;
	DataOut.Tangent = LightParams.Tangent;
	DataOut.SourceRadius = LightParams.SourceRadius;
	DataOut.SpotAngles = LightParams.SpotAngles;
	DataOut.SoftSourceRadius = LightParams.SoftSourceRadius;
	DataOut.SourceLength = LightParams.SourceLength;
	DataOut.RectLightBarnCosAngle = LightParams.RectLightBarnCosAngle;
	DataOut.RectLightBarnLength = LightParams.RectLightBarnLength;
	DataOut.RectLightAtlasUVOffset = LightParams.RectLightAtlasUVOffset;
	DataOut.RectLightAtlasUVScale = LightParams.RectLightAtlasUVScale;
	DataOut.RectLightAtlasMaxLevel = LightParams.RectLightAtlasMaxLevel;
	DataOut.InverseExposureBlend = LightParams.InverseExposureBlend;
	DataOut.IESAtlasIndex = LightParams.IESAtlasIndex;
	DataOut.LightTypeAndShadowMapChannelMaskPacked = LightInfo.PackLightTypeAndShadowMapChannelMask(bAllowStaticLighting);
}

void FGPUScene::UpdateInternal(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUB, FRDGExternalAccessQueue& ExternalAccessQueue, const UE::Tasks::FTask& UpdateTaskPrerequisites)
{
	LLM_SCOPE_BYTAG(GPUScene);

	check(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	check(NumScenePrimitives == Scene.Primitives.Num());
	check(DynamicPrimitivesOffset >= Scene.Primitives.Num());

	CSV_CUSTOM_STAT(GPUScene, InstanceAllocMaxSize, InstanceSceneDataAllocator.GetMaxSize(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(GPUScene, InstanceAllocUsedSize, InstanceSceneDataAllocator.GetSparselyAllocatedSize(), ECsvCustomStatOp::Set);

	CSV_CUSTOM_STAT(GPUScene, InstancePayloadAllocMaxSize, InstancePayloadDataAllocator.GetMaxSize(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(GPUScene, InstancePayloadAllocUsedSize, InstancePayloadDataAllocator.GetSparselyAllocatedSize(), ECsvCustomStatOp::Set);

	CSV_CUSTOM_STAT(GPUScene, LightmapDataAllocMaxSize, LightmapDataAllocator.GetMaxSize(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(GPUScene, LightmapDataAllocUsedSize, LightmapDataAllocator.GetSparselyAllocatedSize(), ECsvCustomStatOp::Set);

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.Update");

	// Do this stat separately since it should always be on.
	CSV_CUSTOM_STAT_GLOBAL(GPUSceneInstanceCount, float(InstanceSceneDataAllocator.GetMaxSize()), ECsvCustomStatOp::Set);

#if !UE_BUILD_SHIPPING
	MaxInstancesDuringPrevUpdate = uint32(InstanceSceneDataAllocator.GetMaxSize());
#endif // UE_BUILD_SHIPPING

	LastDeferredGPUWritePass = EGPUSceneGPUWritePass::None;

	if ((CVarGPUSceneUploadEveryFrame.GetValueOnRenderThread() != 0) || bUpdateAllPrimitives)
	{
		PrimitivesToUpdate.Reset();
		ResizeDirtyState(Scene.GetMaxPersistentPrimitiveIndex());
		for (FPrimitiveSceneInfo *PrimitiveSceneInfo : Scene.Primitives)
		{
			PrimitiveDirtyState[PrimitiveSceneInfo->GetPersistentIndex().Index] |= EPrimitiveDirtyState::ChangedAll;
			PrimitivesToUpdate.Add(PrimitiveSceneInfo->GetPersistentIndex());
		}

		// Clear the full instance data range
		InstanceRangesToClear.Empty();
		InstanceRangesToClear.Add(FInstanceRange{ 0U, uint32(GetInstanceIdUpperBoundGPU()) });

		bUpdateAllPrimitives = false;
	}

	// Store in GPU-scene to enable validation that update has been carried out.
	SceneFrameNumber = Scene.GetFrameNumber();

	// Strip all out-of-range ID's (left over because of deletes) so we don't need to check later
	for (int32 Index = 0; Index < PrimitivesToUpdate.Num();)
	{
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitivesToUpdate[Index];
		if (!PrimitiveDirtyState.IsValidIndex(PersistentPrimitiveIndex.Index))
		{
			PrimitivesToUpdate.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		else
		{
			// If it was removed (and not readded)
			if (EnumHasAnyFlags(PrimitiveDirtyState[PersistentPrimitiveIndex.Index], EPrimitiveDirtyState::Removed) 
				&& !EnumHasAnyFlags(PrimitiveDirtyState[PersistentPrimitiveIndex.Index], EPrimitiveDirtyState::Added))
			{
				PrimitivesToUpdate.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
			else
			{
				// Should only have valid, current, primitives left
				check(Scene.GetPrimitiveIndex(PersistentPrimitiveIndex) != INDEX_NONE);
				++Index;
			}
		}
	}

	FUploadDataSourceAdapterScenePrimitives& Adapter = *GraphBuilder.AllocObject<FUploadDataSourceAdapterScenePrimitives>(Scene, SceneFrameNumber, MoveTemp(PrimitivesToUpdate), MoveTemp(PrimitiveDirtyState));
	
	FRegisteredBuffers BufferState = UpdateBufferAllocations(GraphBuilder, SceneUB, Adapter);

	// Run a pass that clears (Sets ID to invalid) any instances that need it
	AddClearInstancesPass(GraphBuilder, Scene.InstanceCullingOcclusionQueryRenderer);

	// The adapter copies the IDs of primitives to update such that any that are (incorrectly) marked for update after are not lost.
	PrimitivesToUpdate.Reset();
	PrimitiveDirtyState.Init(EPrimitiveDirtyState::None, PrimitiveDirtyState.Num());

	{
		SCOPED_NAMED_EVENT(UpdateGPUScene, FColor::Green);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateGPUScene);
		SCOPE_CYCLE_COUNTER(STAT_UpdateGPUSceneTime);

		UploadGeneral<FUploadDataSourceAdapterScenePrimitives>(GraphBuilder, BufferState, &ExternalAccessQueue, Adapter, UpdateTaskPrerequisites);
	}
}

template<typename FUploadDataSourceAdapter>
FGPUScene::FRegisteredBuffers FGPUScene::UpdateBufferAllocations(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUB, const FUploadDataSourceAdapter& UploadDataSourceAdapter)
{
	LLM_SCOPE_BYTAG(GPUScene);

	check(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	check(NumScenePrimitives == Scene.Primitives.Num());

	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	FRegisteredBuffers BufferState;

	const uint32 SizeReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(DynamicPrimitivesOffset, InitialBufferSize));
	BufferState.PrimitiveBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, PrimitiveBuffer, SizeReserve * sizeof(FPrimitiveSceneShaderData::Data), TEXT("GPUScene.PrimitiveData"));

	// Clamp buffer to be smaller than the MAX_INSTANCE_ID.
	const uint32 InstanceSceneDataSizeReserve = FMath::Min(MAX_INSTANCE_ID, FMath::RoundUpToPowerOfTwo(FMath::Max(InstanceSceneDataAllocator.GetMaxSize(), InitialBufferSize)));

	FResizeResourceSOAParams ResizeParams;
	ResizeParams.NumBytes = InstanceSceneDataSizeReserve * FInstanceSceneShaderData::GetEffectiveNumBytes();
	ResizeParams.NumArrays = FInstanceSceneShaderData::GetDataStrideInFloat4s();

	BufferState.InstanceSceneDataBuffer = ResizeStructuredBufferSOAIfNeeded(GraphBuilder, InstanceSceneDataBuffer, ResizeParams, TEXT("GPUScene.InstanceSceneData"));
	InstanceSceneDataSOAStride = InstanceSceneDataSizeReserve;

	const uint32 PayloadFloat4Count = FMath::Max(InstancePayloadDataAllocator.GetMaxSize(), InitialBufferSize);
	const uint32 InstancePayloadDataSizeReserve = FMath::RoundUpToPowerOfTwo(PayloadFloat4Count * sizeof(FVector4f));
	BufferState.InstancePayloadDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, InstancePayloadDataBuffer, InstancePayloadDataSizeReserve, TEXT("GPUScene.InstancePayloadData"));

	const bool bNaniteEnabled = DoesPlatformSupportNanite(GMaxRHIShaderPlatform);
	if (UploadDataSourceAdapter.bUpdateNaniteMaterialTables && bNaniteEnabled)
	{
		// Nanite draw commands build raster material tables.
		Scene.WaitForCacheNaniteMaterialBinsTask();

		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			Scene.NaniteMaterials[NaniteMeshPassIndex].UpdateBufferState(GraphBuilder, Scene.GetMaxPersistentPrimitiveIndex());
		}
	}
	
	const uint32 LightMapDataBufferSize = FMath::RoundUpToPowerOfTwo(FMath::Max(LightmapDataAllocator.GetMaxSize(), InitialBufferSize));
	BufferState.LightmapDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LightmapDataBuffer, LightMapDataBufferSize * sizeof(FLightmapSceneShaderData::Data), TEXT("GPUScene.LightmapData"));

	BufferState.LightDataBuffer = GraphBuilder.RegisterExternalBuffer(LightDataBuffer);

	FGPUSceneResourceParameters ShaderParameters;
	ShaderParameters.GPUSceneInstanceSceneData = GraphBuilder.CreateSRV(BufferState.InstanceSceneDataBuffer);
	ShaderParameters.GPUSceneInstancePayloadData = GraphBuilder.CreateSRV(BufferState.InstancePayloadDataBuffer);
	ShaderParameters.GPUScenePrimitiveSceneData = GraphBuilder.CreateSRV(BufferState.PrimitiveBuffer);
	ShaderParameters.GPUSceneLightmapData = GraphBuilder.CreateSRV(BufferState.LightmapDataBuffer);
	ShaderParameters.GPUSceneLightData = GraphBuilder.CreateSRV(BufferState.LightDataBuffer);
	ShaderParameters.InstanceDataSOAStride = InstanceSceneDataSOAStride;
	ShaderParameters.NumScenePrimitives = NumScenePrimitives;
	ShaderParameters.NumInstances = InstanceSceneDataAllocator.GetMaxSize();
	ShaderParameters.GPUSceneFrameNumber = GetSceneFrameNumber();

	SceneUB.Set(SceneUB::GPUScene, ShaderParameters);

	if (bInBeginEndBlock)
	{
		CachedRegisteredBuffers = BufferState;
	}
	else
	{
		check(!CachedRegisteredBuffers.IsValid());
	}

	return BufferState;
}


FGPUScene::FRegisteredBuffers FGPUScene::RegisterBuffers(FRDGBuilder& GraphBuilder) const
{
	FRegisteredBuffers Result;

	Result.PrimitiveBuffer = GraphBuilder.RegisterExternalBuffer(PrimitiveBuffer);
	Result.InstanceSceneDataBuffer = GraphBuilder.RegisterExternalBuffer(InstanceSceneDataBuffer);
	Result.InstancePayloadDataBuffer = GraphBuilder.RegisterExternalBuffer(InstancePayloadDataBuffer);
	Result.LightmapDataBuffer = GraphBuilder.RegisterExternalBuffer(LightmapDataBuffer);
	Result.LightDataBuffer = GraphBuilder.RegisterExternalBuffer(LightDataBuffer);

	return Result;
}

/**
 * Used to queue up load-balanced chunks of instance upload work such that it can be spread over a large number of cores.
 */
struct FInstanceUploadBatch
{
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
	int32 MaxItems = 64;
	int32 MaxCost = MaxItems * 2; // Selected to allow filling the array when 1:1 primitive / instances

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

	FInstanceBatcher(bool bExecuteInParallel, int32 NumPrimitiveDataUploads)
	{
		if (bExecuteInParallel)
		{
			MaxItems = 64;
			MaxCost = MaxItems * 2;
		}
		else
		{
			// funnel  all items into a single batch with one item per primitive.
			MaxItems = MAX_int32;
			MaxCost = MAX_int32;
			UpdateBatchItems.Reserve(NumPrimitiveDataUploads);
		}
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
			int32 MaxInstancesThisBatch = MaxCost - CurrentBatchCost - 1;

			if (MaxInstancesThisBatch > 0)
			{
				const int NumInstancesThisItem = FMath::Min(MaxInstancesThisBatch, NumInstances - InstancesAdded);
				UpdateBatchItems.Add(FInstanceUploadBatch::FItem{ ItemIndex, InstancesAdded, NumInstancesThisItem });
				CurrentBatch->NumItems += 1;
				InstancesAdded += NumInstancesThisItem;
				CurrentBatchCost += NumInstancesThisItem + 1;
			}

			// Flush batch if it is not possible to add any more items (for one of the reasons)
			if (MaxInstancesThisBatch <= 0 || CurrentBatchCost > MaxCost - 1 || CurrentBatch->NumItems >= MaxItems)
			{
				CurrentBatchCost = 0;
				CurrentBatch = &UpdateBatches.AddDefaulted_GetRef();
				CurrentBatch->FirstItem = UpdateBatchItems.Num();
			}
		}
	}
};

template<typename FUploadDataSourceAdapter>
void FGPUScene::UploadGeneral(FRDGBuilder& GraphBuilder, const FRegisteredBuffers &BufferState, FRDGExternalAccessQueue* ExternalAccessQueue, const FUploadDataSourceAdapter& UploadDataSourceAdapter, const UE::Tasks::FTask& PrerequisiteTask)
{
	LLM_SCOPE_BYTAG(GPUScene);

	ensure(bIsEnabled == UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()));
	ensure(NumScenePrimitives == Scene.Primitives.Num());

	const int32 NumPrimitiveDataUploads = UploadDataSourceAdapter.NumPrimitivesToUpload();

	if (!NumPrimitiveDataUploads)
	{
		return;
	}

	const bool bExecuteInParallel = CVarGPUSceneParallelUpdate.GetValueOnRenderThread() != 0 && FApp::ShouldUseThreadingForPerformance();
	const bool bNaniteEnabled = DoesPlatformSupportNanite(GMaxRHIShaderPlatform);

	SCOPED_NAMED_EVENT(UpdateGPUScene, FColor::Green);

	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
	RDG_EVENT_SCOPE(GraphBuilder, "UpdateGPUScene NumPrimitiveDataUploads %u", NumPrimitiveDataUploads);

	struct FTaskContext
	{
		TArray<FPrimitiveUploadInfoHeader, FSceneRenderingArrayAllocator> PrimitiveUploadInfos;

		FRDGScatterUploader* PrimitiveUploader = nullptr;
		FRDGScatterUploader* InstancePayloadUploader = nullptr;
		FRDGScatterUploader* InstanceSceneUploader = nullptr;
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
	TaskContext.InstanceSceneDataSOAStride = InstanceSceneDataSOAStride;
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

	if (TaskContext.NumLightmapDataUploads > 0)
	{
		TaskContext.LightmapUploader = LightmapUploadBuffer.Begin(GraphBuilder, BufferState.LightmapDataBuffer, TaskContext.NumLightmapDataUploads, sizeof(FLightmapSceneShaderData::Data), TEXT("LightmapUploadBuffer"));
	}

	if (UploadDataSourceAdapter.bUpdateNaniteMaterialTables && bNaniteEnabled)
	{
		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			TaskContext.NaniteMaterialUploaders[NaniteMeshPassIndex] = Scene.NaniteMaterials[NaniteMeshPassIndex].Begin(GraphBuilder, Scene.GetMaxPersistentPrimitiveIndex(), NumPrimitiveDataUploads);
		}

		TaskContext.bUseNaniteMaterialUploaders = true;
	}

	GraphBuilder.AddCommandListSetupTask([&TaskContext, &UploadDataSourceAdapter, bNaniteEnabled, bExecuteInParallel, FeatureLevel = FeatureLevel](FRHICommandListBase& RHICmdList)
	{
		SCOPED_NAMED_EVENT(UpdateGPUScene_Primitives, FColor::Green);

		LockIfValid(RHICmdList, TaskContext.PrimitiveUploader);
		LockIfValid(RHICmdList, TaskContext.InstancePayloadUploader);
		LockIfValid(RHICmdList, TaskContext.InstanceSceneUploader);
		LockIfValid(RHICmdList, TaskContext.LightmapUploader);

		for (FNaniteMaterialCommands::FUploader* Uploader : TaskContext.NaniteMaterialUploaders)
		{
			LockIfValid(RHICmdList, Uploader);
		}

		FInstanceBatcher InstanceUpdates(bExecuteInParallel, TaskContext.NumPrimitiveDataUploads);

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
			}

			TaskContext.PrimitiveUploader->Add(UploadDataSourceAdapter.GetItemPrimitiveIds());

			ParallelForTemplate(TEXT("GPUScene Upload Primitives Task"), TaskContext.NumPrimitiveDataUploads, 1, [&TaskContext, &UploadDataSourceAdapter](int32 ItemIndex)
			{
				FOptionalTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				FVector4f* DstData = static_cast<FVector4f*>(TaskContext.PrimitiveUploader->GetRef(ItemIndex));
				UploadDataSourceAdapter.GetPrimitiveShaderData(ItemIndex, DstData);
			}, bExecuteInParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
		}

		if (TaskContext.NumInstanceSceneDataUploads > 0 && !InstanceUpdates.UpdateBatches.IsEmpty())
		{
			SCOPED_NAMED_EVENT(Instances, FColor::Green);

			ParallelForTemplate(TEXT("GPUScene Upload Instances Task"), InstanceUpdates.UpdateBatches.Num(), 1, [&TaskContext, &InstanceUpdates, &UploadDataSourceAdapter](int32 BatchIndex)
			{
				const FInstanceUploadBatch Batch = InstanceUpdates.UpdateBatches[BatchIndex];
				const uint32 InstanceDataStrideInFloat4s = FInstanceSceneShaderData::GetDataStrideInFloat4s();
				const bool bSupportsCompressedTransforms = FInstanceSceneShaderData::SupportsCompressedTransforms();

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

						// Directly embedded in instance scene data
						const float RandomID = (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_RANDOM) ? UploadInfo.InstanceRandomID[InstanceIndex] : 0.0f;

						FInstanceSceneShaderData InstanceSceneData;
						if (UploadInfo.InstanceSceneDataBuffers != nullptr)
						{
							InstanceSceneData.BuildInternal(
								UploadInfo.PrimitiveID,
								InstanceIndex,
								UploadInfo.InstanceFlags,
								UploadInfo.LastUpdateSceneFrameNumber,
								UploadInfo.InstanceCustomDataCount,
								RandomID,
								UploadInfo.InstanceSceneDataBuffers->GetInstanceToPrimitiveRelative(InstanceIndex),
								!UploadInfo.bIsPrimitiveForceHidden && UploadInfo.InstanceSceneDataBuffers->GetInstanceVisible(InstanceIndex),
								bSupportsCompressedTransforms
							);
						}
						else if (UploadInfo.PrimitiveInstances.IsEmpty())
						{
							// This path should only be taken for uninstanced primitives
							check(UploadInfo.NumInstances == 1 && InstanceIndex == 0);
							InstanceSceneData.BuildInternal(
								UploadInfo.PrimitiveID,
								InstanceIndex,
								UploadInfo.InstanceFlags,
								UploadInfo.LastUpdateSceneFrameNumber,
								UploadInfo.InstanceCustomDataCount,
								RandomID,
								UploadInfo.PrimitiveToWorld,
								!UploadInfo.bIsPrimitiveForceHidden,
								bSupportsCompressedTransforms
							);
						}
						else
						{
							const FInstanceSceneData& SceneData = UploadInfo.PrimitiveInstances[InstanceIndex];
							InstanceSceneData.Build(
								UploadInfo.PrimitiveID,
								InstanceIndex,
								UploadInfo.InstanceFlags,
								UploadInfo.LastUpdateSceneFrameNumber,
								UploadInfo.InstanceCustomDataCount,
								RandomID,
								SceneData.LocalToPrimitive,
								UploadInfo.PrimitiveToWorld,
								!UploadInfo.bIsPrimitiveForceHidden
							);
						}
						// RefIndex* BufferState.InstanceSceneDataSOAStride + UploadInfo.InstanceSceneDataOffset + InstanceIndex
						const uint32 UploadInstanceItemOffset = (PrimitiveItemInfo.InstanceSceneDataUploadOffset + InstanceIndex) * InstanceDataStrideInFloat4s;

						for (uint32 RefIndex = 0; RefIndex < InstanceDataStrideInFloat4s; ++RefIndex)
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
									check(UploadInfo.InstanceLocalBounds.Num() == UploadInfo.NumInstances);
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
								const FRenderTransform PrevLocalToWorld = UploadInfo.InstanceSceneDataBuffers != nullptr ?
									UploadInfo.InstanceSceneDataBuffers->GetPrevInstanceToPrimitiveRelative(InstanceIndex) :
									UploadInfo.InstanceDynamicData[InstanceIndex].ComputePrevLocalToWorld(UploadInfo.PrevPrimitiveToWorld);

								if (bSupportsCompressedTransforms)
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
								check(UploadInfo.InstanceLightShadowUVBias.Num() == UploadInfo.NumInstances);
								InstancePayloadData[PayloadPosition] = UploadInfo.InstanceLightShadowUVBias[InstanceIndex];
								PayloadPosition += 1;
							}

							if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_PAYLOAD_EXTENSION)
							{
								check(UploadInfo.InstancePayloadExtension.Num() / UploadInfo.InstancePayloadExtensionCount == UploadInfo.NumInstances);
								const int32 SrcOffset = InstanceIndex * UploadInfo.InstancePayloadExtensionCount;
								for (int32 Idx = 0; Idx < UploadInfo.InstancePayloadExtensionCount; ++Idx)
								{
									InstancePayloadData[PayloadPosition++] = UploadInfo.InstancePayloadExtension[SrcOffset + Idx];
								}
							}

							if (UploadInfo.InstanceFlags & INSTANCE_SCENE_DATA_FLAG_HAS_CUSTOM_DATA && UploadInfo.InstanceCustomDataCount > 0)
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

			}, bExecuteInParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
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
		UnlockIfValid(RHICmdList, TaskContext.LightmapUploader);

		for (FNaniteMaterialCommands::FUploader* Uploader : TaskContext.NaniteMaterialUploaders)
		{
			UnlockIfValid(RHICmdList, Uploader);
		}

	}, PrerequisiteTask);

	PrimitiveUploadBuffer.End(GraphBuilder, TaskContext.PrimitiveUploader);

	if (TaskContext.InstancePayloadUploader)
	{
		InstancePayloadUploadBuffer.End(GraphBuilder, TaskContext.InstancePayloadUploader);
	}

	if (TaskContext.InstanceSceneUploader)
	{
		InstanceSceneUploadBuffer.End(GraphBuilder, TaskContext.InstanceSceneUploader);
	}

	if (TaskContext.LightmapUploader)
	{
		LightmapUploadBuffer.End(GraphBuilder, TaskContext.LightmapUploader);
	}

	if (TaskContext.bUseNaniteMaterialUploaders)
	{
		check(ExternalAccessQueue);

		for (int32 NaniteMeshPassIndex = 0; NaniteMeshPassIndex < ENaniteMeshPass::Num; ++NaniteMeshPassIndex)
		{
			Scene.NaniteMaterials[NaniteMeshPassIndex].Finish(GraphBuilder, *ExternalAccessQueue, TaskContext.NaniteMaterialUploaders[NaniteMeshPassIndex]);
		}
	}
	const uint32 MaxPooledSize = uint32(CVarGPUSceneMaxPooledUploadBufferSize.GetValueOnRenderThread());
	if (PrimitiveUploadBuffer.GetNumBytes() > MaxPooledSize)
	{
		PrimitiveUploadBuffer.Release();
	}

	if (InstanceSceneUploadBuffer.GetNumBytes() > MaxPooledSize)
	{
		InstanceSceneUploadBuffer.Release();
	}

	if (InstancePayloadUploadBuffer.GetNumBytes() > MaxPooledSize)
	{
		InstancePayloadUploadBuffer.Release();
	}

	if (LightmapUploadBuffer.GetNumBytes() > MaxPooledSize)
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


	FORCEINLINE_GPUSCENE TArrayView<const uint32> GetItemPrimitiveIds() const
	{
		return TArrayView<const uint32>(PrimitivesIds.GetData(), PrimitivesIds.Num());
	}


	FORCEINLINE_GPUSCENE void GetPrimitiveInfoHeader(int32 ItemIndex, FPrimitiveUploadInfoHeader& PrimitiveUploadInfo) const
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

	FORCEINLINE_GPUSCENE void GetPrimitiveShaderData(int32 ItemIndex, FVector4f* RESTRICT OutData) const
	{
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

		FPrimitiveSceneShaderData::Setup(Tmp, OutData);
	}

	FORCEINLINE_GPUSCENE bool GetInstanceInfo(int32 ItemIndex, FInstanceUploadInfo& InstanceUploadInfo) const
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
			InstanceUploadInfo.NumInstances = PrimData.SourceData.InstanceSceneData.Num();
			InstanceUploadInfo.bIsPrimitiveForceHidden = false;
			// Use primitive to world as instance transform:
			if (InstanceUploadInfo.PrimitiveInstances.Num() == 0)
			{
				InstanceUploadInfo.NumInstances = 1;
			}

			return true;
		}

		return false;
	}

	FORCEINLINE_GPUSCENE bool GetLightMapInfo(int32 ItemIndex, FLightMapUploadInfo& UploadInfo) const
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

void FGPUScene::UploadDynamicPrimitiveShaderDataForViewInternal(FRDGBuilder& GraphBuilder, FViewInfo& View, UE::Renderer::Private::IShadowInvalidatingInstances *ShadowInvalidatingInstances)
{
	LLM_SCOPE_BYTAG(GPUScene);

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.UploadDynamicPrimitiveShaderDataForView");
	SCOPED_NAMED_EVENT(FGPUScene_UploadDynamicPrimitiveShaderDataForView, FColor::Green);

	ensure(bInBeginEndBlock);
	ensure(DynamicPrimitivesOffset >= Scene.GetMaxPersistentPrimitiveIndex());

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

		if (ShadowInvalidatingInstances)
		{
			// Enqueue cache invalidations for all dynamic primitives' instances, as they will be removed this frame and are not associated
			// with any particular FPrimitiveSceneInfo. Will occur on the next call to UpdateAllPrimitiveSceneInfos
			for (const FGPUScenePrimitiveCollector::FPrimitiveData& PrimitiveData : Collector.UploadData->PrimitiveData)
			{
				check(PrimitiveData.LocalInstanceSceneDataOffset != INDEX_NONE);
				ShadowInvalidatingInstances->AddInstanceRange(FPersistentPrimitiveIndex(),
					PrimitiveData.LocalInstanceSceneDataOffset + uint32(InstanceIdStart), PrimitiveData.NumInstances);
			}
		}

		FUploadDataSourceAdapterDynamicPrimitives& UploadAdapter = *GraphBuilder.AllocObject<FUploadDataSourceAdapterDynamicPrimitives>(
			Collector.UploadData->PrimitiveData,
			UploadIdStart,
			InstanceIdStart,
			Collector.UploadData->InstancePayloadDataOffset,
			SceneFrameNumber);

		FRegisteredBuffers BufferState = UpdateBufferAllocations(GraphBuilder, View.GetSceneUniforms(), UploadAdapter);

		// Run a pass that clears (Sets ID to invalid) any instances that need it.
		AddClearInstancesPass(GraphBuilder, Scene.InstanceCullingOcclusionQueryRenderer);

		UploadGeneral<FUploadDataSourceAdapterDynamicPrimitives>(GraphBuilder, BufferState, nullptr, UploadAdapter, UE::Tasks::FTask{});
	}

	FSceneUniformBuffer& SceneUniforms = View.GetSceneUniforms();
	FillSceneUniformBuffer(GraphBuilder, SceneUniforms);

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
				Params.PersistentPrimitiveId = PrimitiveIdStart + PrimitiveIndex;
				Params.InstanceSceneDataOffset = InstanceIdStart + PrimData.LocalInstanceSceneDataOffset;

				PrimData.SourceData.DataWriterGPU.Execute(GraphBuilder, Params);
			}
		}
	}
}

bool FGPUScene::FillSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUB) const
{
	if (!bIsEnabled)
	{
		return false;
	}

	if (PrimitiveBuffer != nullptr)
	{
		return SceneUB.Set(SceneUB::GPUScene, GetShaderParameters(GraphBuilder));
	}
	else
	{
		// leave the dummy data in place - the gpu scene is not yet populated
		return false;
	}
}

FGPUSceneResourceParameters FGPUScene::GetShaderParameters(FRDGBuilder& GraphBuilder) const
{
	FRegisteredBuffers BufferState = CachedRegisteredBuffers;
	// If we're not in a begin/end block we need to register the buffers here and now.
	if (!BufferState.IsValid())
	{
		BufferState = RegisterBuffers(GraphBuilder);
	}

	FGPUSceneResourceParameters TmpParameters;
	// Not in an active rendering context, must register the buffers and fill in the data structure.
	TmpParameters.GPUSceneInstanceSceneData = GraphBuilder.CreateSRV(BufferState.InstanceSceneDataBuffer);
	TmpParameters.GPUSceneInstancePayloadData = GraphBuilder.CreateSRV(BufferState.InstancePayloadDataBuffer);
	TmpParameters.GPUScenePrimitiveSceneData = GraphBuilder.CreateSRV(BufferState.PrimitiveBuffer);
	TmpParameters.GPUSceneLightmapData = GraphBuilder.CreateSRV(BufferState.LightmapDataBuffer);
	TmpParameters.GPUSceneLightData = GraphBuilder.CreateSRV(BufferState.LightDataBuffer);
	TmpParameters.InstanceDataSOAStride = InstanceSceneDataSOAStride;
	TmpParameters.NumScenePrimitives = NumScenePrimitives;
	TmpParameters.NumInstances = InstanceSceneDataAllocator.GetMaxSize();
	TmpParameters.GPUSceneFrameNumber = GetSceneFrameNumber();

	return TmpParameters;
}

void FGPUScene::AddPrimitiveToUpdate(FPersistentPrimitiveIndex PersistentPrimitiveIndex, EPrimitiveDirtyState DirtyState)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled && PersistentPrimitiveIndex.IsValid())
	{
		ResizeDirtyState(PersistentPrimitiveIndex.Index + 1);

		// Make sure we aren't updating same primitive multiple times.
		if (PrimitiveDirtyState[PersistentPrimitiveIndex.Index] == EPrimitiveDirtyState::None)
		{
			PrimitivesToUpdate.Add(PersistentPrimitiveIndex);
		}

		EPrimitiveDirtyState NewState = PrimitiveDirtyState[PersistentPrimitiveIndex.Index] | DirtyState;

		// When a primitive is removed, we clear pending "added" state since it is no longer being added
		// Thus if the state is both added & removed, we know this is a current primitive, where the slot has been reused.
		if (EnumHasAnyFlags(DirtyState, EPrimitiveDirtyState::Removed))
		{
			EnumRemoveFlags(NewState, EPrimitiveDirtyState::Added);
		}
		PrimitiveDirtyState[PersistentPrimitiveIndex.Index] = NewState;
	}
}


void FGPUScene::Update(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUB, FRDGExternalAccessQueue& ExternalAccessQueue, const UE::Tasks::FTask& UpdateTaskPrerequisites)
{
	if (bIsEnabled)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		ensure(!bInBeginEndBlock);

		// record primitive high-watermark (dynamic ones are allocated after that point)
		NumScenePrimitives = Scene.Primitives.Num();

		// Default state when updated (no "dynamic primitives" pushed)
		DynamicPrimitivesOffset = Scene.GetMaxPersistentPrimitiveIndex();

		UpdateInternal(GraphBuilder, SceneUB, ExternalAccessQueue, UpdateTaskPrerequisites);
	}
}

void FGPUScene::UploadDynamicPrimitiveShaderDataForView(FRDGBuilder& GraphBuilder, FViewInfo& View, UE::Renderer::Private::IShadowInvalidatingInstances *ShadowInvalidatingInstances)
{
	if (bIsEnabled)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		UploadDynamicPrimitiveShaderDataForViewInternal(GraphBuilder, View, ShadowInvalidatingInstances);
	}
}

// Grow the last entry in the Output using the given Range if trivial merge is possible (typical case)
inline void AddOrMergeInstanceRange(TArray<FGPUSceneInstanceRange>& Output, FGPUSceneInstanceRange Range)
{
	if (!Output.IsEmpty())
	{
		FGPUSceneInstanceRange& Last = Output.Last();
		if (Range.InstanceSceneDataOffset == Last.InstanceSceneDataOffset + Last.NumInstanceSceneDataEntries)
		{
			Last.NumInstanceSceneDataEntries += Range.NumInstanceSceneDataEntries;
			return;
		}
	}
	Output.Add(Range);
}

int32 FGPUScene::AllocateInstanceSceneDataSlots(int32 NumInstanceSceneDataEntries)
{
	LLM_SCOPE_BYTAG(GPUScene);

	if (bIsEnabled)
	{
		if (NumInstanceSceneDataEntries > 0)
		{
			int32 InstanceSceneDataOffset = InstanceSceneDataAllocator.Allocate(NumInstanceSceneDataEntries);
			AddOrMergeInstanceRange(InstanceRangesToClear, FInstanceRange{ uint32(InstanceSceneDataOffset), uint32(NumInstanceSceneDataEntries) });
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
		AddOrMergeInstanceRange(InstanceRangesToClear, FInstanceRange{ uint32(InstanceSceneDataOffset), uint32(NumInstanceSceneDataEntries) });
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

uint32 FGPUScene::GetInstanceIdUpperBoundGPU() const 
{ 
	return FMath::Min( uint32(InstanceSceneDataAllocator.GetMaxSize()), MAX_INSTANCE_ID); 
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
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
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
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);

		// Skip optimization for avoiding long compilation time due to large UAV writes
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGPUSceneDebugRenderCS, "/Engine/Private/GPUSceneDebugRender.usf", "GPUSceneDebugRenderCS", SF_Compute);

void FGPUScene::DebugRender(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer, FViewInfo& View)
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
			SelectedPrimitiveFlags.Init(0U, FMath::DivideAndRoundUp(Scene.GetMaxPersistentPrimitiveIndex(), BitsPerWord));
			for (int32 PackedIndex = 0; PackedIndex < Scene.PrimitiveSceneProxies.Num(); ++PackedIndex)
			{
				if (Scene.PrimitiveSceneProxies[PackedIndex]->IsSelected())
				{
					FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene.Primitives[PackedIndex];
					FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitiveSceneInfo->GetPersistentIndex();

					SelectedPrimitiveFlags[PersistentPrimitiveIndex.Index / BitsPerWord] |= 1U << uint32(PersistentPrimitiveIndex.Index % BitsPerWord);

					// Collect Names
					if (SelectedNameInfos.Num() < MaxPrimitiveNameCount)
					{
						const FString OwnerName = PrimitiveSceneInfo->GetFullnameForDebuggingOnly();
						const uint32 NameOffset = SelectedNames.Num();
						const uint32 NameLength = OwnerName.Len();
						for (TCHAR C : OwnerName)
						{
							SelectedNames.Add(uint8(C));
						}

						FPrimitiveSceneDebugNameInfo& NameInfo = SelectedNameInfos.AddDefaulted_GetRef();
						NameInfo.PrimitiveID= PersistentPrimitiveIndex.Index;
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
			PassParameters->Scene = SceneUniformBuffer.GetBuffer(GraphBuilder);
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

void FGPUScene::ConsolidateInstanceDataAllocations()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ConsolidateInstanceDataAllocations);

	InstanceSceneDataAllocator.Consolidate();
	InstancePayloadDataAllocator.Consolidate();
	LightmapDataAllocator.Consolidate();
		SCOPED_NAMED_EVENT(FGPUScene_EndDeferAllocatorMerges, FColor::Green);
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

	RDG_EVENT_SCOPE(GraphBuilder, "GPUScene.DeferredGPUWrites - Pass %u", uint32(GPUWritePass));

	FGPUSceneWriteDelegateParams Params;
	Params.GPUWritePass = GPUWritePass;
	GetWriteParameters(GraphBuilder, Params.GPUWriteParams);

	for (const FDeferredGPUWrite& DeferredWrite : DeferredGPUWritePassDelegates[PassIndex])
	{
		FViewInfo* View = Views.FindByPredicate([&DeferredWrite](const FViewInfo& V){ return V.GPUSceneViewId == DeferredWrite.ViewId; });
		checkf(View != nullptr, TEXT("Deferred GPU Write found with no matching view in the view family"));
		
		Params.View = View;
		Params.PersistentPrimitiveId = DeferredWrite.PrimitiveId;
		Params.InstanceSceneDataOffset = DeferredWrite.InstanceSceneDataOffset;

		DeferredWrite.DataWriterGPU.Execute(GraphBuilder, Params);
	}

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

void FGPUScene::OnPreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ScenePreUpdateData)
{
	for (FPersistentPrimitiveIndex PersistentPrimitiveIndex  : ScenePreUpdateData.RemovedPrimitiveIds)
	{
		AddPrimitiveToUpdate(PersistentPrimitiveIndex, EPrimitiveDirtyState::Removed);
	}
	for (FPersistentPrimitiveIndex PersistentPrimitiveIndex  : ScenePreUpdateData.UpdatedPrimitiveIds)
	{
		AddPrimitiveToUpdate(PersistentPrimitiveIndex, EPrimitiveDirtyState::ChangedTransform);
	}
}

void FGPUScene::OnPostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ScenePostUpdateData)
{
	for (FPersistentPrimitiveIndex PersistentPrimitiveIndex  : ScenePostUpdateData.AddedPrimitiveIds)
	{
		AddPrimitiveToUpdate(PersistentPrimitiveIndex, EPrimitiveDirtyState::AddedMask);
	}
}

void FGPUScene::OnPostLightSceneInfoUpdate(FRDGBuilder& GraphBuilder, const FLightSceneChangeSet& LightsPostUpdateData)
{
	// TODO: implement proper dirty tracking such that we can actually do incremental updates without loosing information
	// const bool bAnythingChanged = !(LightsPostUpdateData.AddedLightIds.IsEmpty() && LightsPostUpdateData.RemovedLightIds.IsEmpty() && LightsPostUpdateData.TransformUpdatedLightIds.IsEmpty() && LightsPostUpdateData.ColorUpdatedLightIds.IsEmpty());
	
	// SceneFrameNumber is updated in UpdateInternal so if it is the same, we have already uploaded the lights this "frame" - this is not 100% robust since 
	// if UpdateAllPrimitiveSceneInfos is called multiple times without updating the FScene frame number it will skip light uploads. The real solution is 
	// to implement functional change tracking for lights so we can actually know when there are relevant changes.
	if (SceneFrameNumber != Scene.GetFrameNumberRenderThread() || !LightDataBuffer.IsValid())
	{
		UpdateGPULights(GraphBuilder, UE::Tasks::FTask{});
	}
}

SIZE_T FGPUScene::GetAllocatedSize() const
{
	return PrimitivesToUpdate.GetAllocatedSize()
		+ InstanceRangesToClear.GetAllocatedSize()
		+ PrimitiveDirtyState.GetAllocatedSize()
		+ DeferredGPUWritePassDelegates[uint32(EGPUSceneGPUWritePass::PostOpaqueRendering)].GetAllocatedSize();
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
	UE::TScopeLock ScopeLock(DymamicPrimitiveUploadDataMutex);
	DymamicPrimitiveUploadData.Add(UploadData);
	return UploadData;
}

/**
 * Fills in the FGPUSceneWriterParameters to use for read/write access to the GPU Scene.
 */
void FGPUScene::GetWriteParameters(FRDGBuilder& GraphBuilder, FGPUSceneWriterParameters& GPUSceneWriterParametersOut)
{
	FRegisteredBuffers BufferState = CachedRegisteredBuffers;
	// If we're not in a begin/end block we need to register the buffers here and now.
	if (!BufferState.IsValid())
	{
		BufferState = RegisterBuffers(GraphBuilder);
	}
	GPUSceneWriterParametersOut.GPUSceneFrameNumber = SceneFrameNumber;
	GPUSceneWriterParametersOut.GPUSceneInstanceSceneDataSOAStride = InstanceSceneDataSOAStride;
	GPUSceneWriterParametersOut.GPUSceneNumAllocatedInstances = InstanceSceneDataAllocator.GetMaxSize();
	GPUSceneWriterParametersOut.GPUSceneNumAllocatedPrimitives = DynamicPrimitivesOffset;
	GPUSceneWriterParametersOut.GPUSceneInstanceSceneDataRW = GraphBuilder.CreateUAV(BufferState.InstanceSceneDataBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	GPUSceneWriterParametersOut.GPUSceneInstancePayloadDataRW = GraphBuilder.CreateUAV(BufferState.InstancePayloadDataBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	GPUSceneWriterParametersOut.GPUScenePrimitiveSceneDataRW = GraphBuilder.CreateUAV(BufferState.PrimitiveBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
}

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

FBatchedPrimitiveShaderData::FBatchedPrimitiveShaderData(const FPrimitiveSceneProxy* Proxy)
	: Data(InPlace, NoInit)
{
	FPrimitiveUniformShaderParametersBuilder Builder = FPrimitiveUniformShaderParametersBuilder{};
	Proxy->BuildUniformShaderParameters(Builder);
	Setup(Builder.Build());
}

void FBatchedPrimitiveShaderData::Emplace(FBatchedPrimitiveShaderData* Dest, const FPrimitiveUniformShaderParameters& ShaderParams)
{
	new (Dest) FBatchedPrimitiveShaderData(ShaderParams);
}

void FBatchedPrimitiveShaderData::Emplace(FBatchedPrimitiveShaderData* Dest, const FPrimitiveSceneProxy* Proxy)
{
	new (Dest) FBatchedPrimitiveShaderData(Proxy);
}

void FBatchedPrimitiveShaderData::Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
{
	// Note: layout must match LoadPrimitiveDataUBO in SceneDataMobileLoader.ush
	int32 i = 0;

	const bool bAllowStaticLighting = IsStaticLightingAllowed();
	if (bAllowStaticLighting)
	{
		FVector4f LightMapUVScaleBias = FVector4f(1, 1, 0, 0);
		FVector4f ShadowMapUVScaleBias = FVector4f(1, 1, 0, 0);
		uint32 LightMapDataIdx = 0;

		// FIXME: valid lightmap data
		Data[i+0] = LightMapUVScaleBias;
		Data[i+1] = ShadowMapUVScaleBias;
		Data[i+2] = FVector4f(FMath::AsFloat(LightMapDataIdx), 0.f, 0.f, 0.f);
		i+=3;
	}

	// PositionHigh, Flags
	{
		Data[i].X = PrimitiveUniformShaderParameters.PositionHigh.X;
		Data[i].Y = PrimitiveUniformShaderParameters.PositionHigh.Y;
		Data[i].Z = PrimitiveUniformShaderParameters.PositionHigh.Z;
		Data[i].W = FMath::AsFloat(PrimitiveUniformShaderParameters.Flags);
		i+=1;
	}
	
	// LocalToWorld
	{
		FMatrix44f LocalToRelativeWorldTranspose = PrimitiveUniformShaderParameters.LocalToRelativeWorld.GetTransposed();
		Data[i+0] = *(const FVector4f*)&LocalToRelativeWorldTranspose.M[0][0];
		Data[i+1] = *(const FVector4f*)&LocalToRelativeWorldTranspose.M[1][0];
		Data[i+2] = *(const FVector4f*)&LocalToRelativeWorldTranspose.M[2][0];
		i+=3;
	}

	// InvNonUniformScale, TODO .w
	{
		Data[i+0] = FVector4f(PrimitiveUniformShaderParameters.InvNonUniformScale, 0.0f);
		i+=1;
	}

	// ObjectWorldPosition, Radius
	{
		Data[i+0] = PrimitiveUniformShaderParameters.ObjectWorldPositionHighAndRadius;
		Data[i+1] = FVector4f(PrimitiveUniformShaderParameters.ObjectWorldPositionLow, 0.0f);
		i+=2;
	}

	// ActorWorldPosition, TODO .w
	{
		Data[i+0] = FVector4f(PrimitiveUniformShaderParameters.ActorWorldPositionHigh, 0.0f);
		Data[i+1] = FVector4f(PrimitiveUniformShaderParameters.ActorWorldPositionLow, 0.0f);
		i+=2;
	}

	// ObjectOrientation, ObjectBoundsX
	{
		Data[i+0] = FVector4f(PrimitiveUniformShaderParameters.ObjectOrientation, PrimitiveUniformShaderParameters.ObjectBoundsX);
		i+=1;
	}

	// LocalObjectBoundsMin, ObjectBoundsY
	{
		Data[i+0] = FVector4f(PrimitiveUniformShaderParameters.LocalObjectBoundsMin, PrimitiveUniformShaderParameters.ObjectBoundsY);
		i+=1;
	}

	// LocalObjectBoundsMax, ObjectBoundsZ
	{
		Data[i+0] = FVector4f(PrimitiveUniformShaderParameters.LocalObjectBoundsMax, PrimitiveUniformShaderParameters.ObjectBoundsZ);
		i+=1;
	}

	// WorldToLocal
	{
		FMatrix44f RelativeWorldToLocalTranspose = PrimitiveUniformShaderParameters.RelativeWorldToLocal.GetTransposed();
		Data[i+0] = *(const FVector4f*)&RelativeWorldToLocalTranspose.M[0][0];
		Data[i+1] = *(const FVector4f*)&RelativeWorldToLocalTranspose.M[1][0];
		Data[i+2] = *(const FVector4f*)&RelativeWorldToLocalTranspose.M[2][0];
		i+=3;
	}

	// PreviousLocalToWorld
	{
		FMatrix44f PreviousLocalToRelativeWorldTranspose = PrimitiveUniformShaderParameters.PreviousLocalToRelativeWorld.GetTransposed();
		Data[i+0] = *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[0][0];
		Data[i+1] = *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[1][0];
		Data[i+2] = *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[2][0];
		i+=3;
	}

	// PreviousWorldToLocal
	{
		FMatrix44f PreviousRelativeWorldToLocalTranspose = PrimitiveUniformShaderParameters.PreviousRelativeWorldToLocal.GetTransposed();
		Data[i+0] = *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[0][0];
		Data[i+1] = *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[1][0];
		Data[i+2] = *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[2][0];
		i+=3;
	}

	// Set all the custom primitive data float4. This matches the loop in SceneData.ush
	int32 NumCustomData = FMath::Min<int32>(FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s, DataStrideInFloat4s - i);
	for (int32 DataIndex = 0; DataIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s; ++DataIndex)
	{
		Data[i + DataIndex] = PrimitiveUniformShaderParameters.CustomPrimitiveData[DataIndex];
	}
}

void FGPUScene::AddClearInstancesPass(FRDGBuilder& GraphBuilder, FInstanceCullingOcclusionQueryRenderer* OcclusionQueryRenderer)
{
	FInstanceGPULoadBalancer ClearIdData;
#if LOG_INSTANCE_ALLOCATIONS
	FString RangesStr;
#endif
	for (FInstanceRange Range : InstanceRangesToClear)
	{
		// Clamp the instance range to the used instances.
		int32 RangeEnd = FMath::Min(int32(Range.InstanceSceneDataOffset + Range.NumInstanceSceneDataEntries), int32(GetInstanceIdUpperBoundGPU()));
		// Clamp to zero to avoid overflow since the start of the range may also be outside the valid instances
		Range.NumInstanceSceneDataEntries = uint32(FMath::Max(0, RangeEnd - int32(Range.InstanceSceneDataOffset)));

		if (Range.NumInstanceSceneDataEntries > 0u)
		{			
			ClearIdData.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries, INVALID_PRIMITIVE_ID);
#if LOG_INSTANCE_ALLOCATIONS
			RangesStr.Appendf(TEXT("[%6d, %6d), "), Range.InstanceSceneDataOffset, Range.InstanceSceneDataOffset + Range.NumInstanceSceneDataEntries);
#endif
		}
	}

	if (OcclusionQueryRenderer)
	{
		// Invalidate last frame occlusion query slots
		OcclusionQueryRenderer->MarkInstancesVisible(GraphBuilder, InstanceRangesToClear);
	}

#if LOG_INSTANCE_ALLOCATIONS
	UE_LOG(LogTemp, Warning, TEXT("AddClearInstancesPass: \n%s"), *RangesStr);
#endif
	AddUpdatePrimitiveIdsPass(GraphBuilder, ClearIdData);
	InstanceRangesToClear.Empty();
}
