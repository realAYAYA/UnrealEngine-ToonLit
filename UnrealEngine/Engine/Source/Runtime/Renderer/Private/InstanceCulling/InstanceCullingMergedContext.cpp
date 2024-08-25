// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceCulling/InstanceCullingMergedContext.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "RenderGraphBuilder.h"

void FInstanceCullingMergedContext::MergeBatches()
{
	for (FBatchItem& AsyncBatchItem : AsyncBatches)
	{
		AsyncBatchItem.Context->WaitForSetupTask();
		check(AsyncBatchItem.Context->DynamicInstanceIdOffset >= 0);
		check(AsyncBatchItem.Context->DynamicInstanceIdNum >= 0);

		AddBatchItem(AsyncBatchItem);
	}
	AsyncBatches.Reset();

	for (uint32 Mode = 0U; Mode < uint32(EBatchProcessingMode::Num); ++Mode)
	{
		LoadBalancers[Mode].ReserveStorage(TotalBatches[Mode], TotalItems[Mode]);
	}
	// Pre-size all arrays
	IndirectArgs.Empty(TotalIndirectArgs);
	DrawCommandDescs.Empty(TotalIndirectArgs);
	InstanceIdOffsets.Empty(TotalIndirectArgs);
	PayloadData.Empty(TotalPayloads);
	ViewIds.Empty(TotalViewIds);
	DrawCommandCompactionData.Empty(TotalCompactionDrawCommands);
	CompactionBlockDataIndices.Empty(TotalCompactionBlocks);

	BatchInfos.Reserve(Batches.Num());
	uint32 InstanceIdBufferOffset = 0U; // in buffer elements
	uint32 InstanceDataByteOffset = 0U;
	uint32 TempCompactionInstanceOffset = 0U;
	
	// Index that maps from each command to the corresponding batch - maybe not the utmost efficiency
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		const FBatchItem& BatchItem = Batches[BatchIndex];
		const FInstanceCullingContext& InstanceCullingContext = *BatchItem.Context;

		// Empty contexts should never be added to this list!
		check(InstanceCullingContext.HasCullingCommands());

		int32 BatchInfoIndex = BatchInfos.Num();
		FContextBatchInfo& BatchInfo = BatchInfos.AddDefaulted_GetRef();

		BatchInfo.IndirectArgsOffset = IndirectArgs.Num();
		//BatchInfo.NumIndirectArgs = InstanceCullingContext.IndirectArgs.Num();
		check(InstanceCullingContext.DrawCommandDescs.Num() == InstanceCullingContext.IndirectArgs.Num());
		IndirectArgs.Append(InstanceCullingContext.IndirectArgs);
		DrawCommandDescs.Append(InstanceCullingContext.DrawCommandDescs);

		BatchInfo.PayloadDataOffset = PayloadData.Num();
		PayloadData.Append(InstanceCullingContext.PayloadData);

		check(InstanceCullingContext.InstanceIdOffsets.Num() == InstanceCullingContext.IndirectArgs.Num());
		InstanceIdOffsets.AddDefaulted(InstanceCullingContext.InstanceIdOffsets.Num());
		// TODO: perform offset on GPU
		// InstanceIdOffsets.Append(InstanceCullingContext.InstanceIdOffsets);
		for (int32 Index = 0; Index < InstanceCullingContext.InstanceIdOffsets.Num(); ++Index)
		{
			InstanceIdOffsets[BatchInfo.IndirectArgsOffset + Index] = InstanceCullingContext.InstanceIdOffsets[Index] + InstanceIdBufferOffset;
		}

		BatchInfo.ViewIdsOffset = ViewIds.Num();
		BatchInfo.NumViewIds = InstanceCullingContext.ViewIds.Num();
		ViewIds.Append(InstanceCullingContext.ViewIds);

		check(InstanceCullingContext.DynamicInstanceIdOffset >= 0);
		check(InstanceCullingContext.DynamicInstanceIdNum >= 0);

		BatchInfo.DynamicInstanceIdOffset = InstanceCullingContext.DynamicInstanceIdOffset;
		BatchInfo.DynamicInstanceIdMax = InstanceCullingContext.DynamicInstanceIdOffset + InstanceCullingContext.DynamicInstanceIdNum;

		for (uint32 Mode = 0U; Mode < uint32(EBatchProcessingMode::Num); ++Mode)
		{
			int32 StartIndex = BatchInds[Mode].Num();
			TInstanceCullingLoadBalancer<SceneRenderingAllocator>* MergedLoadBalancer = &LoadBalancers[Mode];

			BatchInfo.ItemDataOffset[Mode] = MergedLoadBalancer->GetItems().Num();
			FInstanceProcessingGPULoadBalancer* LoadBalancer = InstanceCullingContext.LoadBalancers[Mode];
			LoadBalancer->FinalizeBatches();

			// UnCulled bucket is used for a single instance mode
			check(EBatchProcessingMode(Mode) != EBatchProcessingMode::UnCulled || LoadBalancer->HasSingleInstanceItemsOnly());

			BatchInds[Mode].AddDefaulted(LoadBalancer->GetBatches().Num());

			MergedLoadBalancer->AppendData(*LoadBalancer);
			for (int32 Index = StartIndex; Index < BatchInds[Mode].Num(); ++Index)
			{
				BatchInds[Mode][Index] = BatchInfoIndex;
			}
		}
		const uint32 BatchTotalInstances = InstanceCullingContext.TotalInstances * InstanceCullingContext.ViewIds.Num();
		const uint32 BatchTotalDraws = InstanceCullingContext.InstanceIdOffsets.Num();

		FInstanceCullingDrawParams& Result = *BatchItem.Result;
		Result.InstanceDataByteOffset = InstanceDataByteOffset;
		Result.IndirectArgsByteOffset = BatchInfo.IndirectArgsOffset * FInstanceCullingContext::IndirectArgsNumWords * sizeof(uint32);

		BatchInfo.InstanceDataWriteOffset = InstanceIdBufferOffset;

		// Append the compaction data, but fix up the offsets for the batch
		BatchInfo.CompactionDataOffset = DrawCommandCompactionData.Num();
		const uint32 CompactionBlockOffset = CompactionBlockDataIndices.Num();
		for (auto CompactionData : InstanceCullingContext.DrawCommandCompactionData)
		{			
			CompactionData.BlockOffset += CompactionBlockOffset;
			CompactionData.IndirectArgsIndex += BatchInfo.IndirectArgsOffset;
			CompactionData.SrcInstanceIdOffset += TempCompactionInstanceOffset;
			CompactionData.DestInstanceIdOffset += InstanceIdBufferOffset;
			DrawCommandCompactionData.Add(CompactionData);
		}
		for (uint32 CompactionDataIndex : InstanceCullingContext.CompactionBlockDataIndices)
		{
			CompactionBlockDataIndices.Add(CompactionDataIndex + BatchInfo.CompactionDataOffset);
		}
		TempCompactionInstanceOffset += InstanceCullingContext.NumCompactionInstances;

		// Advance offset into instance ID and per-instance buffer
		InstanceIdBufferOffset += InstanceCullingContext.GetInstanceIdNumElements();
		InstanceDataByteOffset += InstanceCullingContext.StepInstanceDataOffsetBytes(BatchTotalDraws);
	}
}

void FInstanceCullingMergedContext::AddBatch(FRDGBuilder& GraphBuilder, FInstanceCullingContext* Context, FInstanceCullingDrawParams* InstanceCullingDrawParams)
{
	checkfSlow(Batches.FindByPredicate([InstanceCullingDrawParams](const FBatchItem& Item) { return Item.Result == InstanceCullingDrawParams; }) == nullptr, TEXT("Output draw paramters registered twice."));

	const bool bOcclusionCullInstances = Context->PrevHZB.IsValid() && FInstanceCullingContext::IsOcclusionCullingEnabled();

	// Set HZB texture for the merged batches
	if (bOcclusionCullInstances)
	{
		// Verify that each batch contains the same HZB if not null as we only support one
		check(PrevHZB == nullptr || PrevHZB == GraphBuilder.RegisterExternalTexture(Context->PrevHZB));

		if (PrevHZB == nullptr)
		{
			// Note: performing the registration here because the final merge of contexts may happen during RDG execute (in case of the deferred culling) which seems ill-defined
			PrevHZB = GraphBuilder.RegisterExternalTexture(Context->PrevHZB);
		}
	}

	if (Context->SyncPrerequisitesFunc)
	{
		AsyncBatches.Add(FBatchItem{ Context, InstanceCullingDrawParams });

	}
	else
	{
		AddBatchItem(FBatchItem{ Context, InstanceCullingDrawParams });
	}

}

void FInstanceCullingMergedContext::AddBatchItem(const FBatchItem& BatchItem)
{
	const FInstanceCullingContext* Context = BatchItem.Context;
	if (Context->HasCullingCommands())
	{
		Batches.Add(BatchItem);

		// Accumulate the totals so the deferred processing can pre-size the arrays
		for (uint32 Mode = 0U; Mode < uint32(EBatchProcessingMode::Num); ++Mode)
		{
			Context->LoadBalancers[Mode]->FinalizeBatches();
			TotalBatches[Mode] += Context->LoadBalancers[Mode]->GetBatches().Max();
			TotalItems[Mode] += Context->LoadBalancers[Mode]->GetItems().Max();
		}
#if DO_CHECK
		for (int32 ViewId : Context->ViewIds)
		{
			checkf(NumCullingViews < 0 || ViewId < NumCullingViews, TEXT("Attempting to defer a culling context that references a view that has not been uploaded yet."));
		}
#endif 

		TotalIndirectArgs += Context->IndirectArgs.Num();
		TotalPayloads += Context->PayloadData.Num();
		TotalViewIds += Context->ViewIds.Num();
		InstanceIdBufferElements += Context->GetInstanceIdNumElements();
		TotalInstances += Context->TotalInstances;
		TotalCompactionDrawCommands += Context->DrawCommandCompactionData.Num();
		TotalCompactionBlocks += Context->CompactionBlockDataIndices.Num();
		TotalCompactionInstances += Context->NumCompactionInstances;
	}
#if DO_CHECK
	else
	{
		check(!bMustAddAllContexts);
	}
#endif 
}