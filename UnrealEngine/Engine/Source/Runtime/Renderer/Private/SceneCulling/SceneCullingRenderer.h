// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneCulling.h"

class FSceneCulling;
class FSceneInstanceCullResult;
class FSceneInstanceCullingQuery;

BEGIN_SHADER_PARAMETER_STRUCT( FInstanceHierarchyParameters, )
	SHADER_PARAMETER(uint32, NumCellsPerBlockLog2)
	SHADER_PARAMETER(uint32, CellBlockDimLog2)
	SHADER_PARAMETER(uint32, LocalCellCoordMask) // (1 << NumCellsPerBlockLog2) - 1
	SHADER_PARAMETER(int32, FirstLevel)
	SHADER_PARAMETER(uint32, bUseExplicitCellBounds)

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FCellBlockData >, InstanceHierarchyCellBlockData)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FPackedCellHeader >, InstanceHierarchyCellHeaders)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >, InstanceHierarchyItemChunks)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >, InstanceIds)
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< float4 >, ExplicitCellBounds)
END_SHADER_PARAMETER_STRUCT()

/**
 * Renderer-lifetime functionality, provides scope for anything that should share life-time with a Scene Renderer, rather than Scene.
 */
class FSceneCullingRenderer
{
public:
	friend class FSceneInstanceCullingQuery;

	FSceneCullingRenderer(FSceneCulling& InSceneCulling, ISceneRenderer &InSceneRenderer) : SceneCulling(InSceneCulling), SceneRenderer(InSceneRenderer) {}

	inline bool IsEnabled() const { return SceneCulling.IsEnabled(); }

	/**
	 * Getting the shader parameters forces a sync wrt the hierarchy update, since we need to resize the GPU buffers at this point.
	 */
	FInstanceHierarchyParameters& GetShaderParameters(FRDGBuilder& GraphBuilder);
	
	/**
	 * Create and dispatch a culling query for a set of views that has a 1:1 mapping from culling volume to view index
	 * May run async.
	 */
	FSceneInstanceCullingQuery* CullInstances(FRDGBuilder& GraphBuilder, const TConstArrayView<FConvexVolume>& ViewCullVolumes);
	FSceneInstanceCullingQuery* CullInstances(FRDGBuilder& GraphBuilder, const FConvexVolume& ViewCullVolume) { return CullInstances(GraphBuilder, TConstArrayView<FConvexVolume>(&ViewCullVolume, 1)); }

	/**
	 * Create a query that is not immediately dispatched, such that jobs can be added first.
	 */
	FSceneInstanceCullingQuery* CreateInstanceQuery(FRDGBuilder& GraphBuilder);

	/**
	 */
	void DebugRender(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views);

private:
	FSceneCulling& SceneCulling;
	ISceneRenderer &SceneRenderer;
	using FSpatialHash = FSceneCulling::FSpatialHash;

	FInstanceHierarchyParameters ShaderParameters;
	FRDGBuffer* CellHeadersRDG = nullptr;
	FRDGBuffer* ItemChunksRDG = nullptr;
	FRDGBuffer* InstanceIdsRDG = nullptr;
	FRDGBuffer* CellBlockDataRDG = nullptr;
	FRDGBuffer* ExplicitCellBoundsRDG = nullptr;
};

/**
 */
class FSceneInstanceCullingQuery
{
public:
	FSceneInstanceCullingQuery(FSceneCullingRenderer& InSceneCullingRenderer);
	/**
	 * Add a view-group to the query. Culling results are indexed by the returned index.
	 * MaxNumViews should be the maximum number of views that may be referenced. This includes mip views when relevant.
	 */
	int32 Add(uint32 FirstPrimaryView, uint32 NumPrimaryViews, uint32 MaxNumViews, const FCullingVolume& CullingVolume);

	/**
	 * Run culling job.
	 */
	void Dispatch(FRDGBuilder& GraphBuilder, bool bAllowAsync = true);

	// Get GPU stuffs, TBD
	FSceneInstanceCullResult* GetResult();

	/**
	 * returns true if the task is running async
	 * NOTE: may return false for a task that has completed, even if it was spawned as an asyc task.
	 */
	bool IsAsync() const { return AsyncTaskHandle.IsValid() && !AsyncTaskHandle.IsCompleted(); }

	/**
	 * Get the task handle to be able to queue subsequent work, for example.
	 */
	UE::Tasks::FTask GetAsyncTaskHandle() const { return AsyncTaskHandle; }

	FSceneCullingRenderer& GetSceneCullingRenderer() { return SceneCullingRenderer; }
private:
	void ComputeResult();

	FSceneCullingRenderer& SceneCullingRenderer;

	struct FCullingJob
	{
		FCullingVolume CullingVolume;
		FViewDrawGroup ViewDrawGroup;
		uint32 MaxNumViews = 1u;
	};

	TArray<FCullingJob, SceneRenderingAllocator> CullingJobs;

	FSceneInstanceCullResult *CullingResult = nullptr;

	UE::Tasks::FTask AsyncTaskHandle;
};


class FSceneInstanceCullResult
{
public:
	using FCellDraws = TArray<FCellDraw, SceneRenderingAllocator>;
	using FViewDrawGroups = TArray<FViewDrawGroup, SceneRenderingAllocator>;
	// The list of cell/view-group pairs to feed to rendering
	FCellDraws CellDraws;
	FViewDrawGroups ViewDrawGroups;
	uint32 NumInstanceGroups = 0;
	int32 MaxOccludedCellDraws = 0;
	FSceneCullingRenderer* SceneCullingRenderer = nullptr;
	uint32 UncullableItemChunksOffset = 0;
	uint32 UncullableNumItemChunks = 0;
};