// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"

namespace UE::Net::Private
{
// Queued data chunk
struct FQueuedDataChunk
{
	FQueuedDataChunk()
	: StorageOffset(0U)
	, NumBits(0U)
	, bHasBatchOwnerData(0U)
	, bIsEndReplicationChunk(0U)
	{
	}

	uint32 StorageOffset;
	uint32 NumBits : 30;
	uint32 bHasBatchOwnerData : 1;
	uint32 bIsEndReplicationChunk : 1;
};

// Struct to contain storage and required data for queued batches pending must be mapped references
struct FPendingBatchData
{
	// We use a single array to store the actual data, it will grow if required.
	TArray<uint32, TInlineAllocator<32>> DataChunkStorage;		
	TArray<FQueuedDataChunk, TInlineAllocator<4>> QueuedDataChunks;

	// Must be mapped references pending resolve
	TArray<FNetRefHandle, TInlineAllocator<4>> PendingMustBeMappedReferences;

	// Resolved references for which we have are holding on to references to avoid GC
	TArray<FNetRefHandle, TInlineAllocator<4>> ResolvedReferences;

	// Batch owner with queued data chunks
	FNetRefHandle Handle;
};

struct FPendingBatches
{
	FPendingBatchData* Find(FNetRefHandle NetRefHandle) { return PendingBatches.FindByPredicate([&NetRefHandle](const FPendingBatchData& Entry) { return Entry.Handle == NetRefHandle; });}
	const FPendingBatchData* Find(FNetRefHandle NetRefHandle) const { return PendingBatches.FindByPredicate([&NetRefHandle](const FPendingBatchData& Entry) { return Entry.Handle == NetRefHandle; });}
	bool GetHasPendingBatches() const { return !PendingBatches.IsEmpty(); }

	TArray<FPendingBatchData> PendingBatches;
};

}
