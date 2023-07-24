// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InstanceCulling/InstanceCullingContext.h"
#include "InstanceCulling/InstanceCullingLoadBalancer.h"

class FInstanceCullingMergedContext
{
public:
	FInstanceCullingMergedContext(ERHIFeatureLevel::Type InFeatureLevel)
		: FeatureLevel(InFeatureLevel)
	{}

	struct FBatchItem
	{
		const FInstanceCullingContext* Context = nullptr;
		FInstanceCullingDrawParams* Result = nullptr;
		int32 DynamicInstanceIdOffset = 0;
		int32 DynamicInstanceIdNum = 0;
	};

	// Info about a batch of culling work produced by a context, when part of a batched job
	// Store once per context, provides start offsets to commands/etc for the context.
	struct FContextBatchInfo
	{
		uint32 IndirectArgsOffset;
		uint32 InstanceDataWriteOffset;
		uint32 PayloadDataOffset;
		uint32 CompactionDataOffset;
		uint32 ViewIdsOffset;
		uint32 NumViewIds;
		uint32 DynamicInstanceIdOffset;
		uint32 DynamicInstanceIdMax;
		uint32 ItemDataOffset[uint32(EBatchProcessingMode::Num)];
	};

	/** Batches of GPU instance culling input data. */
	TArray<FBatchItem, SceneRenderingAllocator> Batches;


	/** 
	 * Merged data, derived in MergeBatches(), follows.
	 */
	TArray<int32, SceneRenderingAllocator> ViewIds;
	//TArray<FMeshDrawCommandInfo, SceneRenderingAllocator> MeshDrawCommandInfos;
	TArray<FRHIDrawIndexedIndirectParameters, SceneRenderingAllocator> IndirectArgs;
	TArray<uint32, SceneRenderingAllocator> DrawCommandDescs;
	TArray<FInstanceCullingContext::FPayloadData, SceneRenderingAllocator> PayloadData;
	TArray<uint32, SceneRenderingAllocator> InstanceIdOffsets;
	TArray<FInstanceCullingContext::FCompactionData, SceneRenderingAllocator> DrawCommandCompactionData;
	TArray<uint32, SceneRenderingAllocator> CompactionBlockDataIndices;	

	TStaticArray<TInstanceCullingLoadBalancer<SceneRenderingAllocator>, static_cast<uint32>(EBatchProcessingMode::Num)> LoadBalancers;
	TStaticArray<TArray<uint32, SceneRenderingAllocator>, static_cast<uint32>(EBatchProcessingMode::Num)> BatchInds;
	TArray<FContextBatchInfo, SceneRenderingAllocator> BatchInfos;

	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	// Counters to sum up all sizes to facilitate pre-sizing
	uint32 InstanceIdBufferSize = 0U;
	TStaticArray<int32, uint32(EBatchProcessingMode::Num)> TotalBatches = TStaticArray<int32, uint32(EBatchProcessingMode::Num)>(InPlace, 0);
	TStaticArray<int32, uint32(EBatchProcessingMode::Num)> TotalItems = TStaticArray<int32, uint32(EBatchProcessingMode::Num)>(InPlace, 0);
	int32 TotalIndirectArgs = 0;
	int32 TotalPayloads = 0;
	int32 TotalViewIds = 0;
	int32 TotalInstances = 0;
	int32 TotalCompactionDrawCommands = 0;
	int32 TotalCompactionBlocks = 0;
	int32 TotalCompactionInstances = 0;

	// Single Previous frame HZB which is shared among all batched contexts, thus only one is allowed (but the same can be used in multiple passes). (Needs atlas or bindless to expand).
	FRDGTextureRef PrevHZB = nullptr;
	int32 NumCullingViews = 0;

	// Merge the queued batches and populate the derived data.
	void MergeBatches();


	void AddBatch(FRDGBuilder& GraphBuilder, const FInstanceCullingContext* Context, int32 DynamicInstanceIdOffset, int32 DynamicInstanceIdNum, FInstanceCullingDrawParams* InstanceCullingDrawParams = nullptr);
};