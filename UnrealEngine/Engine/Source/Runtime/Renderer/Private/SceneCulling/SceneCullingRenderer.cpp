// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneCullingRenderer.h"
#include "SceneCulling.h"
#include "RenderGraphUtils.h"

FInstanceHierarchyParameters& FSceneCullingRenderer::GetShaderParameters(FRDGBuilder& GraphBuilder) 
{ 
	// Sync any update that is in progress.
	SceneCulling.EndUpdate(GraphBuilder, true);

	// This should not need to be done more than once per frame
	if (CellHeadersRDG == nullptr)
	{
		CellBlockDataRDG = SceneCulling.CellBlockDataBuffer.Register(GraphBuilder);
		CellHeadersRDG = SceneCulling.CellHeadersBuffer.Register(GraphBuilder);
		ItemChunksRDG = SceneCulling.ItemChunksBuffer.Register(GraphBuilder);
		ItemsRDG = SceneCulling.ItemsBuffer.Register(GraphBuilder);

#if 0
		// Fully upload the buffers for debugging. 
		CellBlockDataRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.BlockData"), SceneCulling.CellBlockData);
		CellHeadersRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.CellHeaders"), SceneCulling.CellHeaders);
		ItemChunksRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.ItemChunks"), SceneCulling.PackedCellChunkData);
		ItemsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.Items"), SceneCulling.PackedCellData);
#endif

		ShaderParameters.NumCellsPerBlockLog2 = FSpatialHash::NumCellsPerBlockLog2;
		ShaderParameters.CellBlockDimLog2 = FSpatialHash::CellBlockDimLog2;
		ShaderParameters.LocalCellCoordMask = (1U << FSpatialHash::CellBlockDimLog2) - 1U;
		ShaderParameters.FirstLevel = SceneCulling.SpatialHash.GetFirstLevel();
		ShaderParameters.InstanceHierarchyCellBlockData = GraphBuilder.CreateSRV(CellBlockDataRDG);
		ShaderParameters.InstanceHierarchyCellHeaders = GraphBuilder.CreateSRV(CellHeadersRDG);
		ShaderParameters.InstanceHierarchyItems = GraphBuilder.CreateSRV(ItemsRDG);
		ShaderParameters.InstanceHierarchyItemChunks = GraphBuilder.CreateSRV(ItemChunksRDG);
	}

	return ShaderParameters; 
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CullInstances(FRDGBuilder& GraphBuilder, const TConstArrayView<FConvexVolume>& ViewCullVolumes)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);

		for (int32 Index = 0; Index < ViewCullVolumes.Num(); ++Index)
		{
			FCullingVolume CullingVolume;
			CullingVolume.ConvexVolume = ViewCullVolumes[Index];
			Query->Add(Index, 1, 1, CullingVolume);
		}

		Query->Dispatch(GraphBuilder);

		return Query;
	}
	return nullptr;
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CreateInstanceQuery(FRDGBuilder& GraphBuilder)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);
		return Query;
	}
	return nullptr;
}

int32 FSceneInstanceCullingQuery::Add(uint32 FirstPrimaryView, uint32 NumPrimaryViews, uint32 MaxNumViews, const FCullingVolume& CullingVolume)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	if (SceneCullingRenderer.IsEnabled())
	{
		FCullingJob Job;
		Job.MaxNumViews = MaxNumViews;
		Job.ViewDrawGroup.FirstView = FirstPrimaryView;
		Job.ViewDrawGroup.NumViews = NumPrimaryViews;
		Job.CullingVolume = CullingVolume;
		CullingJobs.Add(Job);

		// return index of the job / view group
		return CullingJobs.Num() - 1;
	}
	return INDEX_NONE;
}

void FSceneInstanceCullingQuery::Dispatch(FRDGBuilder& GraphBuilder, bool bInAllowAsync)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	const bool bAllowAsync = SceneCullingRenderer.SceneCulling.bUseAsyncUpdate && bInAllowAsync;

	if (!CullingJobs.IsEmpty())
	{
		// Must wait if this query is not running async or we might race against the update task.
		UE::Tasks::FTask UpdateTaskHandle = SceneCullingRenderer.SceneCulling.GetUpdateTaskHandle();
		if (!bAllowAsync && UpdateTaskHandle.IsValid())
		{
			UpdateTaskHandle.Wait();
		}

		CullingResult = GraphBuilder.AllocObject<FSceneInstanceCullResult>();

		AsyncTaskHandle = GraphBuilder.AddSetupTask([this]()
		{
			ComputeResult();
		},
		nullptr, TArray<UE::Tasks::FTask>{ UpdateTaskHandle }, UE::Tasks::ETaskPriority::Normal, bAllowAsync);
	}
}

FSceneInstanceCullResult* FSceneInstanceCullingQuery::GetResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_GetResult, FColor::Emerald);

	if (AsyncTaskHandle.IsValid())
	{
		AsyncTaskHandle.Wait();
	}

	if (CullingResult == nullptr && !CullingJobs.IsEmpty())
	{
		ComputeResult();
	}

	return CullingResult;
}

void FSceneInstanceCullingQuery::ComputeResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_ComputeResult, FColor::Emerald);
	CullingResult->MaxOccludedCellDraws = 0;
	// loop and append all results
	for (const FCullingJob& CullingJob : CullingJobs)
	{
		int32 PrevNumCellDraws = CullingResult->CellDraws.Num();
		SceneCullingRenderer.SceneCulling.Test(CullingJob.CullingVolume, CullingResult->CellDraws, uint32(CullingResult->ViewDrawGroups.Num()), CullingJob.MaxNumViews, CullingResult->NumInstanceGroups);
		CullingResult->MaxOccludedCellDraws += (CullingResult->CellDraws.Num() - PrevNumCellDraws) * CullingJob.MaxNumViews;
		CullingResult->ViewDrawGroups.Add(CullingJob.ViewDrawGroup);
	}
	CullingResult->UncullableItemChunksOffset = SceneCullingRenderer.SceneCulling.UncullableItemChunksOffset;
	CullingResult->UncullableNumItemChunks = SceneCullingRenderer.SceneCulling.UncullableNumItemChunks;
	CullingResult->SceneCullingRenderer = &SceneCullingRenderer;
}

FSceneInstanceCullingQuery::FSceneInstanceCullingQuery(FSceneCullingRenderer& InSceneCullingRenderer)
	: SceneCullingRenderer(InSceneCullingRenderer)
{
}

