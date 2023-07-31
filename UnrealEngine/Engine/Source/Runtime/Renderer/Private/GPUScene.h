// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "RendererInterface.h"
#include "PrimitiveUniformShaderParameters.h"
#include "PrimitiveSceneInfo.h"
#include "GrowOnlySpanAllocator.h"
#include "InstanceCulling/InstanceCullingLoadBalancer.h"
#include "MeshBatch.h"

class FRHICommandList;
class FScene;
class FViewInfo;

UE_DEPRECATED(5.0, "Use GPUScene::AddPrimitiveToUpdate instead.4")
extern RENDERER_API void AddPrimitiveToUpdateGPU(FScene& Scene, int32 PrimitiveId);


class FGPUScene;
class FGPUSceneDynamicContext;

BEGIN_SHADER_PARAMETER_STRUCT(FGPUSceneResourceParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneLightmapData)
	SHADER_PARAMETER(uint32, InstanceDataSOAStride)
	SHADER_PARAMETER(uint32, GPUSceneFrameNumber)
	SHADER_PARAMETER(int32, NumInstances)
	SHADER_PARAMETER(int32, NumScenePrimitives)
END_SHADER_PARAMETER_STRUCT()

/**
 * Used to manage dynamic primitives for a given view, during InitViews the data is collected and then can be committed to the GPU-Scene. 
 * Once committed the range of indices are valid and can be used to calculate the PrimitiveIds.
 */
class FGPUScenePrimitiveCollector
{
public:
	FGPUScenePrimitiveCollector(FGPUSceneDynamicContext* InGPUSceneDynamicContext = nullptr) :
		GPUSceneDynamicContext(InGPUSceneDynamicContext)
	{};

	/**
	 * Add data for a primitive with a number of instances.
	 * May be called outside (before) a FGPUScene::Begin/EndRender block.
	 * Note: needs to be virtual to prevent a linker error
	 */
	virtual void Add(
		const FMeshBatchDynamicPrimitiveData* MeshBatchData,
		const FPrimitiveUniformShaderParameters& PrimitiveShaderParams,
		uint32 NumInstances,
		uint32& OutPrimitiveIndex,
		uint32& OutInstanceSceneDataOffset);

	/**
	 * Allocates the range in GPUScene and queues the data for upload. 
	 * After this is called no more calls to Add are allowed.
	 * Only allowed inside a FGPUScene::Begin/EndRender block.
	 */
	RENDERER_API void Commit();

	/**
	 * Get the range of Primitive IDs in GPU-Scene for this batch of dynamic primitives, only valid to call after commit.
	 */
	FORCEINLINE const TRange<int32>& GetPrimitiveIdRange() const
	{
		check(bCommitted || UploadData == nullptr);
		return PrimitiveIdRange;
	}

	FORCEINLINE int32 GetInstanceSceneDataOffset() const
	{
		check(bCommitted || UploadData == nullptr);

		return UploadData != nullptr ? UploadData->InstanceSceneDataOffset : 0;
	}

	FORCEINLINE int32 GetInstancePayloadDataOffset() const
	{
		check(bCommitted || UploadData == nullptr);

		return UploadData != nullptr ? UploadData->InstancePayloadDataOffset : 0;
	}

	int32 Num() const {	return UploadData != nullptr ? UploadData->PrimitiveData.Num() : 0; }
	int32 NumInstances() const { return UploadData != nullptr ? UploadData->TotalInstanceCount : 0; }
	int32 NumPayloadDataSlots() const { return UploadData != nullptr ? UploadData->InstancePayloadDataFloat4Count : 0; }

#if DO_CHECK
	/**
	 * Determines if the specified primitive has been sufficiently processed and its data can be read
	 */
	bool IsPrimitiveProcessed(uint32 PrimitiveIndex, const FGPUScene& GPUScene) const;
#endif // DO_CHECK

private:

	friend class FGPUScene;
	friend class FGPUSceneDynamicContext;
	friend struct FGPUSceneCompactInstanceData;
	friend struct FUploadDataSourceAdapterDynamicPrimitives;

	struct FPrimitiveData
	{
		FMeshBatchDynamicPrimitiveData SourceData;
		const FPrimitiveUniformShaderParameters* ShaderParams = nullptr;
		uint32 NumInstances = 0;
		uint32 LocalInstanceSceneDataOffset = INDEX_NONE;
		uint32 LocalPayloadDataOffset = INDEX_NONE;
	};

	struct FUploadData
	{
		TArray<FPrimitiveData, TInlineAllocator<8>> PrimitiveData;
		TArray<uint32> GPUWritePrimitives;

		uint32 InstanceSceneDataOffset = INDEX_NONE;
		uint32 TotalInstanceCount = 0;
		uint32 InstancePayloadDataOffset = INDEX_NONE;
		uint32 InstancePayloadDataFloat4Count = 0;
		bool bIsUploaded = false;
	};

	FUploadData* AllocateUploadData();

	/**
	 * Range in GPU scene allocated to the dynamic primitives.
	 */
	TRange<int32> PrimitiveIdRange = TRange<int32>::Empty();
	FUploadData* UploadData = nullptr; // Owned by FGPUSceneDynamicContext
	bool bCommitted = false;
	FGPUSceneDynamicContext* GPUSceneDynamicContext = nullptr;
};

// TODO: move to own header
class FInstanceProcessingGPULoadBalancer;

/**
 * Contains and controls the lifetime of any dynamic primitive data collected for the scene rendering.
 * Typically shares life-time with the SceneRenderer. 
 */
class FGPUSceneDynamicContext
{
public:
	FGPUSceneDynamicContext(FGPUScene& InGPUScene) : GPUScene(InGPUScene) {}
	~FGPUSceneDynamicContext();

	void Release();

private:
	friend class FGPUScene;
	friend class FGPUScenePrimitiveCollector;

	FGPUScenePrimitiveCollector::FUploadData* AllocateDynamicPrimitiveData();
	TArray<FGPUScenePrimitiveCollector::FUploadData*, TInlineAllocator<128, SceneRenderingAllocator> > DymamicPrimitiveUploadData;
	FGPUScene& GPUScene;
};

// Buffers used by GPU-Scene, since they can be resized during updates AND the render passes must retain the 
// right copy (this is chiefly because the init of shadow views after pre-pass means we need to be able to set 
// up GPU-Scene before pre-pass, but then may discover new primitives etc. As there is no way to know how many
// dynamic primitives will turn up after Pre-pass, we can't guarantee a resize won't happen).
struct FGPUSceneBufferState
{
	bool IsValid() const { return PrimitiveBuffer != nullptr; }

	FRDGBuffer* PrimitiveBuffer = nullptr;
	FRDGBuffer* InstanceSceneDataBuffer = nullptr;
	uint32 InstanceSceneDataSOAStride = 1; // Distance between arrays in float4s
	FRDGBuffer* InstancePayloadDataBuffer = nullptr;
	FRDGBuffer* InstanceBVHBuffer = nullptr;
	FRDGBuffer* LightmapDataBuffer = nullptr;
	uint32 LightMapDataBufferSize = 0;
};

class FGPUScene
{
public:
	FGPUScene()
		: bUpdateAllPrimitives(false)
		, InstanceSceneDataSOAStride(0)
	{
	}
	~FGPUScene();

	void SetEnabled(ERHIFeatureLevel::Type InFeatureLevel);
	bool IsEnabled() const { return bIsEnabled; }
	/**
	 * Call at start of rendering (but after scene primitives are updated) to let GPU-Scene record scene primitive count 
	 * and prepare for dynamic primitive allocations.
	 * Scene may be NULL which means there are zero scene primitives (but there may be dynamic ones added later).
	 */
	void BeginRender(const FScene* Scene, FGPUSceneDynamicContext &GPUSceneDynamicContext);
	inline bool IsRendering() const { return bInBeginEndBlock; }
	void EndRender();

	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }
	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[FeatureLevel]; }

	/**
	 * Allocates a range of space in the instance scene data buffer for the required number of instances, 
	 * returns the offset to the first instance or INDEX_NONE if either the allocation failed or NumInstanceSceneDataEntries was zero.
	 * Marks the instances as requiring update (actual update is handled later).
	 */
	int32 AllocateInstanceSceneDataSlots(int32 NumInstanceSceneDataEntries);
	
	/**
	 * Free the instance data slots for reuse.
	 */
	void FreeInstanceSceneDataSlots(int32 InstanceSceneDataOffset, int32 NumInstanceSceneDataEntries);

	int32 AllocateInstancePayloadDataSlots(int32 NumInstancePayloadFloat4Entries);
	void FreeInstancePayloadDataSlots(int32 InstancePayloadDataOffset, int32 NumInstancePayloadFloat4Entries);

	/**
	 * Upload primitives from View.DynamicPrimitiveCollector.
	 */
	void UploadDynamicPrimitiveShaderDataForView(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View, FRDGExternalAccessQueue& ExternalAccessQueue, bool bIsShadowView = false);

	/**
	 * Pull all pending updates from Scene and upload primitive & instance data.
	 */
	void Update(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue);

	/**
	 * Queue the given primitive for upload to GPU at next call to Update.
	 * May be called multiple times, dirty-flags are cumulative.
	 */
	void RENDERER_API AddPrimitiveToUpdate(int32 PrimitiveId, EPrimitiveDirtyState DirtyState = EPrimitiveDirtyState::ChangedAll);

	/**
	 * Let GPU-Scene know that two primitive IDs swapped location, such that dirty-state can be tracked.
	 * Marks both as having changed ID. 
	 */
	FORCEINLINE void RecordPrimitiveIdSwap(int32 PrimitiveIdA, int32 PrimitiveIdB)
	{
		if (IsEnabled())
		{
			// We should never call this on a non-existent primitive, so no need to resize
			checkSlow(PrimitiveIdA < PrimitiveDirtyState.Num());
			checkSlow(PrimitiveIdB < PrimitiveDirtyState.Num());

			if (PrimitiveDirtyState[PrimitiveIdA] == EPrimitiveDirtyState::None)
			{
				PrimitivesToUpdate.Add(PrimitiveIdA);
			}
			PrimitiveDirtyState[PrimitiveIdA] |= EPrimitiveDirtyState::ChangedId;
			if (PrimitiveDirtyState[PrimitiveIdB] == EPrimitiveDirtyState::None)
			{
				PrimitivesToUpdate.Add(PrimitiveIdB);
			}
			PrimitiveDirtyState[PrimitiveIdB] |= EPrimitiveDirtyState::ChangedId;

			Swap(PrimitiveDirtyState[PrimitiveIdA], PrimitiveDirtyState[PrimitiveIdB]);
		}
	}

	FORCEINLINE EPrimitiveDirtyState GetPrimitiveDirtyState(int32 PrimitiveId) const 
	{ 
		if (PrimitiveId >= PrimitiveDirtyState.Num())
		{
			return EPrimitiveDirtyState::None;
		}
		return PrimitiveDirtyState[PrimitiveId]; 
	}

	FORCEINLINE void ResizeDirtyState(int32 NewSizeIn)
	{
		if (IsEnabled() && NewSizeIn > PrimitiveDirtyState.Num())
		{
			const int32 NewSize = Align(NewSizeIn, 64);
			static_assert(static_cast<uint32>(EPrimitiveDirtyState::None) == 0U, "Using AddZeroed to ensure efficent add, requires None == 0");
			PrimitiveDirtyState.AddZeroed(NewSize - PrimitiveDirtyState.Num());
		}
	}

	/**
	 * Fills in the FGPUSceneWriterParameters to use for read/write access to the GPU Scene.
	 */
	void GetWriteParameters(FRDGBuilder& GraphBuilder, FGPUSceneWriterParameters& GPUSceneWriterParametersOut);

	uint32 GetSceneFrameNumber() const { return SceneFrameNumber; }

	int32 GetNumInstances() const { return InstanceSceneDataAllocator.GetMaxSize(); }
	int32 GetNumPrimitives() const { return DynamicPrimitivesOffset; }
	int32 GetNumLightmapDataItems() const { return LightmapDataAllocator.GetMaxSize(); }

	const FGrowOnlySpanAllocator& GetInstanceSceneDataAllocator() const { return InstanceSceneDataAllocator; }

	/**
	 * Return the GPU scene resource
	 */
	FGPUSceneResourceParameters GetShaderParameters() const { return ShaderParameters; }

	/**
	 * Draw GPU-Scene debug info, such as bounding boxes. Call once per view at some point in the frame after GPU scene has been updated fully.
	 * What is drawn is controlled by the CVar: r.GPUScene.DebugMode. Enabling this cvar causes ShaderDraw to be being active (if supported). 
	 */
	void DebugRender(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View);

	/**
	 * Between these calls to FGrowOnlySpanAllocator::Free just appends the allocation to the free list, rather than trying to merge with existing allocations.
	 * At EndDeferAllocatorMerges the free list is consolidated by sorting and merging all spans. This amortises the cost of the merge over many calls.
	 */
	void BeginDeferAllocatorMerges();
	void EndDeferAllocatorMerges();

	/**
	 * Executes GPUScene writes that were deferred until a later point in scene rendering
	 **/
	bool ExecuteDeferredGPUWritePass(FRDGBuilder& GraphBuilder, TArray<FViewInfo>& Views, EGPUSceneGPUWritePass Pass);

	/** Returns whether or not a GPU Write is pending for the specified primitive */
	bool HasPendingGPUWrite(uint32 PrimitiveId) const;

	bool bUpdateAllPrimitives;

	/** Indices of primitives that need to be updated in GPU Scene */
	TArray<int32>                  PrimitivesToUpdate;

	/** GPU mirror of Primitives */
	TRefCountPtr<FRDGPooledBuffer> PrimitiveBuffer;
	FRDGAsyncScatterUploadBuffer   PrimitiveUploadBuffer;

	/** GPU primitive instance list */
	FGrowOnlySpanAllocator         InstanceSceneDataAllocator;
	TRefCountPtr<FRDGPooledBuffer> InstanceSceneDataBuffer;
	FRDGAsyncScatterUploadBuffer   InstanceSceneUploadBuffer;
	uint32                         InstanceSceneDataSOAStride;	// Distance between arrays in float4s

	FGrowOnlySpanAllocator         InstancePayloadDataAllocator;
	TRefCountPtr<FRDGPooledBuffer> InstancePayloadDataBuffer;
	FRDGAsyncScatterUploadBuffer   InstancePayloadUploadBuffer;

	TRefCountPtr<FRDGPooledBuffer> InstanceBVHBuffer;
	FRDGAsyncScatterUploadBuffer   InstanceBVHUploadBuffer;

	/** GPU light map data */
	FGrowOnlySpanAllocator         LightmapDataAllocator;
	TRefCountPtr<FRDGPooledBuffer> LightmapDataBuffer;
	FRDGAsyncScatterUploadBuffer        LightmapUploadBuffer;

	struct FInstanceRange
	{
		uint32 InstanceSceneDataOffset;
		uint32 NumInstanceSceneDataEntries;
	};

	TArray<FInstanceRange> DynamicPrimitiveInstancesToInvalidate;

	using FInstanceGPULoadBalancer = TInstanceCullingLoadBalancer<SceneRenderingAllocator>;
private:
	FGPUSceneBufferState BufferState;
	FGPUSceneResourceParameters ShaderParameters;

	TArray<EPrimitiveDirtyState> PrimitiveDirtyState;

	TArray<FInstanceRange> InstanceRangesToClear;

	struct FDeferredGPUWrite
	{
		FGPUSceneWriteDelegate DataWriterGPU;
		int32 ViewId = INDEX_NONE;
		uint32 PrimitiveId = INDEX_NONE;
		uint32 InstanceSceneDataOffset = INDEX_NONE;
	};

	static constexpr uint32 NumDeferredGPUWritePasses = uint32(EGPUSceneGPUWritePass::Num);
	TArray<FDeferredGPUWrite> DeferredGPUWritePassDelegates[NumDeferredGPUWritePasses];
	EGPUSceneGPUWritePass LastDeferredGPUWritePass = EGPUSceneGPUWritePass::None;

	TRange<int32> CommitPrimitiveCollector(FGPUScenePrimitiveCollector& PrimitiveCollector);


	friend class FGPUScenePrimitiveCollector;

	/** 
	 * Stores a copy of the Scene.GetFrameNumber() when updated. Used to track which primitives/instances are updated.
	 * When using GPU-Scene for rendering it should ought to be the same as that stored in the Scene (otherwise they are not in sync).
	 */
	uint32 SceneFrameNumber = 0xFFFFFFFF;

	int32 DynamicPrimitivesOffset = 0;

	bool bIsEnabled = false;
	bool bInBeginEndBlock = false;
	bool bInExternalAccessMode = false;
	FGPUSceneDynamicContext* CurrentDynamicContext = nullptr;
	int32 NumScenePrimitives = 0;

	ERHIFeatureLevel::Type FeatureLevel;

	template<typename FUploadDataSourceAdapter>
	void UpdateBufferState(FRDGBuilder& GraphBuilder, FScene& Scene, const FUploadDataSourceAdapter& UploadDataSourceAdapter);

	/**
	 * Generalized upload that uses an adapter to abstract the data souce. Enables uploading scene primitives & dynamic primitives using a single path.
	 * @parameter Scene may be null, as it is only needed for the Nanite material table update (which is coupled to the Scene at the moment).
	 */
	template<typename FUploadDataSourceAdapter>
	void UploadGeneral(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue, const FUploadDataSourceAdapter& UploadDataSourceAdapter);

	void UploadDynamicPrimitiveShaderDataForViewInternal(FRDGBuilder& GraphBuilder, FScene& Scene, FViewInfo& View, FRDGExternalAccessQueue& ExternalAccessQueue, bool bIsShadowView);

	void UpdateInternal(FRDGBuilder& GraphBuilder, FScene& Scene, FRDGExternalAccessQueue& ExternalAccessQueue);

	void AddUpdatePrimitiveIdsPass(FRDGBuilder& GraphBuilder, FInstanceGPULoadBalancer& IdOnlyUpdateItems);

	void AddClearInstancesPass(FRDGBuilder& GraphBuilder);

	void UseInternalAccessMode(FRDGBuilder& GraphBuilder);
	void UseExternalAccessMode(FRDGExternalAccessQueue& ExternalAccessQueue, ERHIAccess Access, ERHIPipeline Pipelines);
};

class FGPUSceneScopeBeginEndHelper
{
public:
	FGPUSceneScopeBeginEndHelper(FGPUScene& InGPUScene, FGPUSceneDynamicContext &GPUSceneDynamicContext, const FScene* Scene) :
		GPUScene(InGPUScene)
	{
		GPUScene.BeginRender(Scene, GPUSceneDynamicContext);
	}

	~FGPUSceneScopeBeginEndHelper()
	{
		GPUScene.EndRender();
	}

private:
	FGPUSceneScopeBeginEndHelper() = delete;
	FGPUSceneScopeBeginEndHelper(const FGPUSceneScopeBeginEndHelper&) = delete;
	FGPUScene& GPUScene;
};

struct FGPUSceneCompactInstanceData
{
	FVector4f InstanceOriginAndId;
	FVector4f InstanceTransform1;
	FVector4f InstanceTransform2;
	FVector4f InstanceTransform3;
	FVector4f InstanceAuxData;

	void Init(const FGPUScenePrimitiveCollector* PrimitiveCollector, int32 PrimitiveId);
	void Init(const FScene* Scene, int32 PrimitiveId);
};
