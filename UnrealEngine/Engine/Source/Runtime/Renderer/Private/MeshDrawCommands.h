// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshDrawCommands.h: Mesh draw commands.
=============================================================================*/

#pragma once

#include "MeshPassProcessor.h"
#include "TranslucencyPass.h"
#include "InstanceCulling/InstanceCullingContext.h"
#include "InstanceCulling/InstanceCullingManager.h"

struct FMeshBatchAndRelevance;
class FStaticMeshBatch;
class FParallelCommandListSet;
class FInstanceCullingManager;

/**
 * Global vertex buffer pool used for GPUScene primitive id arrays.
 */

struct FPrimitiveIdVertexBufferPoolEntry
{
	int32 BufferSize = 0;
	uint32 LastDiscardId = 0;
	FBufferRHIRef BufferRHI;
};

class FPrimitiveIdVertexBufferPool : public FRenderResource
{
public:
	FPrimitiveIdVertexBufferPool();
	~FPrimitiveIdVertexBufferPool();

	FPrimitiveIdVertexBufferPoolEntry Allocate(FRHICommandList& RHICmdList, int32 BufferSize);
	void ReturnToFreeList(FPrimitiveIdVertexBufferPoolEntry Entry);
	RENDERER_API void DiscardAll();

	virtual void ReleaseDynamicRHI() override;

private:
	uint32 DiscardId;
	TArray<FPrimitiveIdVertexBufferPoolEntry> Entries;
	FCriticalSection AllocationCS;
};

extern RENDERER_API TGlobalResource<FPrimitiveIdVertexBufferPool> GPrimitiveIdVertexBufferPool;

/**	
 * Parallel mesh draw command pass setup task context.
 */
class FMeshDrawCommandPassSetupTaskContext
{
public:
	FMeshDrawCommandPassSetupTaskContext()
		: View(nullptr)
		, Scene(nullptr)
		, ShadingPath(EShadingPath::Num)
		, PassType(EMeshPass::Num)
		, bUseGPUScene(false)
		, bDynamicInstancing(false)
		, bReverseCulling(false)
		, bRenderSceneTwoSided(false)
		, BasePassDepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
		, MeshPassProcessor(nullptr)
		, MobileBasePassCSMMeshPassProcessor(nullptr)
		, DynamicMeshElements(nullptr)
		, InstanceFactor(1)
		, NumDynamicMeshElements(0)
		, NumDynamicMeshCommandBuildRequestElements(0)
		, NeedsShaderInitialisation(false)
		, PrimitiveIdBufferData(nullptr)
		, PrimitiveIdBufferDataSize(0)
		, PrimitiveBounds(nullptr)
		, VisibleMeshDrawCommandsNum(0)
		, NewPassVisibleMeshDrawCommandsNum(0)
		, MaxInstances(1)
	{
	}

	const FViewInfo* View;
	const FScene* Scene;
	EShadingPath ShadingPath;
	EShaderPlatform ShaderPlatform;
	EMeshPass::Type PassType;
	bool bUseGPUScene;
	bool bDynamicInstancing;
	bool bReverseCulling;
	bool bRenderSceneTwoSided;
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess;
	FExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess;

	// Mesh pass processor.
	FMeshPassProcessor* MeshPassProcessor;
	FMeshPassProcessor* MobileBasePassCSMMeshPassProcessor;
	const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>* DynamicMeshElements;
	const TArray<FMeshPassMask, SceneRenderingAllocator>* DynamicMeshElementsPassRelevance;

	// Commands.
	int32 InstanceFactor;
	int32 NumDynamicMeshElements;
	int32 NumDynamicMeshCommandBuildRequestElements;
	FMeshCommandOneFrameArray MeshDrawCommands;
	FMeshCommandOneFrameArray MobileBasePassCSMMeshDrawCommands;
	TArray<const FStaticMeshBatch*, SceneRenderingAllocator> DynamicMeshCommandBuildRequests;
	TArray<const FStaticMeshBatch*, SceneRenderingAllocator> MobileBasePassCSMDynamicMeshCommandBuildRequests;
	FDynamicMeshDrawCommandStorage MeshDrawCommandStorage;
	FGraphicsMinimalPipelineStateSet MinimalPipelineStatePassSet;
	bool NeedsShaderInitialisation;

	// Resources preallocated on rendering thread.
	void* PrimitiveIdBufferData;
	int32 PrimitiveIdBufferDataSize;
	FMeshCommandOneFrameArray TempVisibleMeshDrawCommands;

	// For UpdateTranslucentMeshSortKeys.
	ETranslucencyPass::Type TranslucencyPass;
	ETranslucentSortPolicy::Type TranslucentSortPolicy;
	FVector TranslucentSortAxis;
	FVector ViewOrigin;
	FMatrix ViewMatrix;
	const TArray<struct FPrimitiveBounds>* PrimitiveBounds;

	// For logging instancing stats.
	int32 VisibleMeshDrawCommandsNum;
	int32 NewPassVisibleMeshDrawCommandsNum;
	int32 MaxInstances;

	FInstanceCullingContext InstanceCullingContext;
	FInstanceCullingResult InstanceCullingResult;
};

/**
 * Parallel mesh draw command processing and rendering. 
 * Encapsulates two parallel tasks - mesh command setup task and drawing task.
 */
class FParallelMeshDrawCommandPass
{
public:
	enum class EWaitThread
	{
		Render,
		Task,
		TaskAlreadyWaited,
	};

	FParallelMeshDrawCommandPass()
		: bHasInstanceCullingDrawParameters(false)
		, MaxNumDraws(0)
	{
	}

	~FParallelMeshDrawCommandPass();

	/**
	 * Dispatch visible mesh draw command process task, which prepares this pass for drawing.
	 * This includes generation of dynamic mesh draw commands, draw sorting and draw merging.
	 */
	void DispatchPassSetup(
		FScene* Scene,
		const FViewInfo& View, 
		FInstanceCullingContext &&InstanceCullingContext,
		EMeshPass::Type PassType, 
		FExclusiveDepthStencil::Type BasePassDepthStencilAccess,
		FMeshPassProcessor* MeshPassProcessor,
		const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements,
		const TArray<FMeshPassMask, SceneRenderingAllocator>* DynamicMeshElementsPassRelevance,
		int32 NumDynamicMeshElements,
		TArray<const FStaticMeshBatch*, SceneRenderingAllocator>& InOutDynamicMeshCommandBuildRequests,
		int32 NumDynamicMeshCommandBuildRequestElements,
		FMeshCommandOneFrameArray& InOutMeshDrawCommands,
		FMeshPassProcessor* MobileBasePassCSMMeshPassProcessor = nullptr, // Required only for the mobile base pass.
		FMeshCommandOneFrameArray* InOutMobileBasePassCSMMeshDrawCommands = nullptr // Required only for the mobile base pass.
	);

	/**
	 * Sync with setup task and run post-instance culling job to create the render commands and instance ID lists and optionally vertex instance data.
	 * Needs to happen after DispatchPassSetup and before DispatchDraw, but not before global instance culling has been done.
	 */
	void BuildRenderingCommands(
		FRDGBuilder& GraphBuilder,
		const FGPUScene& GPUScene,
		FInstanceCullingDrawParams& OutInstanceCullingDrawParams);

	/**
	 * Sync with setup task.
	 */
	void WaitForSetupTask();

	/**
	 * Dispatch visible mesh draw command draw task.
	 */
	void DispatchDraw(FParallelCommandListSet* ParallelCommandListSet, FRHICommandList& RHICmdList, const FInstanceCullingDrawParams* InstanceCullingDrawParams = nullptr) const;

	void WaitForTasksAndEmpty(EWaitThread WaitThread = EWaitThread::Render);
	void SetDumpInstancingStats(const FString& InPassName);
	bool HasAnyDraw() const { return MaxNumDraws > 0; }

	void InitCreateSnapshot()
	{
		new (&TaskContext.MinimalPipelineStatePassSet) FGraphicsMinimalPipelineStateSet();
	}

	void FreeCreateSnapshot()
	{
		TaskContext.MinimalPipelineStatePassSet.~FGraphicsMinimalPipelineStateSet();
	}

	static bool IsOnDemandShaderCreationEnabled();

	FInstanceCullingContext* GetInstanceCullingContext() { return &TaskContext.InstanceCullingContext; }
	const FGraphEventRef& GetTaskEvent() const { return TaskEventRef; }

private:
	FMeshDrawCommandPassSetupTaskContext TaskContext;
	FGraphEventRef TaskEventRef;
	FString PassNameForStats;

	bool bHasInstanceCullingDrawParameters;

	// Maximum number of draws for this pass. Used to prealocate resources on rendering thread. 
	// Has a guarantee that if there won't be any draws, then MaxNumDraws = 0;
	int32 MaxNumDraws;

	void DumpInstancingStats() const;
	void WaitForMeshPassSetupTask(EWaitThread WaitThread = EWaitThread::Render) const;
};

RENDERER_API extern void SortAndMergeDynamicPassMeshDrawCommands(
	const FSceneView& SceneView,
	FRHICommandList& RHICmdList,
	FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FDynamicMeshDrawCommandStorage& MeshDrawCommandStorage,
	FRHIBuffer*& OutPrimitiveIdVertexBuffer,
	uint32 InstanceFactor,
	const FGPUScenePrimitiveCollector* DynamicPrimitiveCollector);
