// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceParamStorage.h"
#include "HAL/UnrealMemory.h"

namespace UE::AnimNext::Interface
{


FParamStorage::FParamStorage(int32 InMaxParams, int32 InAllocatedMemorySize, int32 InMaxBlocks, bool InAllowGrowing)
	: AllowGrowing(InAllowGrowing)
{
	check(InMaxParams > 0 && InMaxBlocks > 0 && InAllocatedMemorySize > 0);

	Parameters.Reserve(InMaxParams);
	Parameters.AddDefaulted(InMaxParams);

	RefCounts.Reserve(InMaxParams);
	RefCounts.AddZeroed(InMaxParams);

	ParamMemoryHandles.Reserve(InMaxParams);
	for (int32 i = 0; i < InMaxParams; i++)
	{
		ParamMemoryHandles.Add(InvalidBlockHandle);
	}

	// --- ---

	RawMemory = (uint8*)FMemory::Malloc(InAllocatedMemorySize);
	RawMemorySize = InAllocatedMemorySize;

	BlockOffets.Reserve(InMaxBlocks);
	BlockOffets.AddZeroed(InMaxBlocks);

	BlockSizes.Reserve(InMaxBlocks);
	BlockSizes.AddZeroed(InMaxBlocks);

	BlockFreeFlag.Reserve(InMaxBlocks);
	BlockFreeFlag.Init(true, InMaxBlocks);

	FreeBlockIndex = 0;
}

FParamStorage::~FParamStorage()
{
	// There should be no blocks of memory or HParams in memory at this point
	check(FreeBlockIndex == 0);
	check(ParamFreeIndex == 0);

	FMemory::Free(RawMemory);
}

FParamStorage::TBlockDataPair FParamStorage::RequestBlock(int32 RequestedBlockSize)
{
	FParamStorageHandle BlockHandle = InvalidBlockHandle;
	uint8* BlockMemory = nullptr;

	check((CurrentBlocksSize + RequestedBlockSize < RawMemorySize) || AllowGrowing);
	check((FreeBlockIndex >= 0 && FreeBlockIndex < (BlockSizes.Num() - 1)) || AllowGrowing);

	if (CheckBufferSize(RequestedBlockSize) && CheckNumBlocks(FreeBlockIndex))
	{
		const int32 BlockIndex = FreeBlockIndex;

		BlockOffets[BlockIndex] = (BlockIndex > 0) ? BlockOffets[BlockIndex - 1] + BlockSizes[BlockIndex - 1] : 0;
		BlockSizes[BlockIndex] = RequestedBlockSize;
		CurrentBlocksSize += RequestedBlockSize;
		BlockFreeFlag[BlockIndex] = false;
		FreeBlockIndex++;

		BlockHandle = BlockIndex;
		BlockMemory = RawMemory + BlockOffets[BlockIndex];
	}

	return TPair<FParamStorageHandle, uint8*>(BlockHandle, BlockMemory);
}

void FParamStorage::ReleaseBlock(FParamStorageHandle BlockHandle)
{
	int32 BlockIndex = BlockHandle;

	check(BlockIndex >= 0 && BlockIndex < FreeBlockIndex);

	// just remove the offset here
	BlockFreeFlag[BlockIndex] = true;
	
	// if it is the last block, release the memory and deal with the prev blocks flagged as free, if any
	if (BlockIndex == FreeBlockIndex - 1)
	{
		// Recover free slots
		while (BlockIndex >= 0 && BlockFreeFlag[BlockIndex] == true)
		{
			BlockOffets[BlockIndex] = 0;
			CurrentBlocksSize -= BlockSizes[BlockIndex];
			BlockSizes[BlockIndex] = 0;
			BlockIndex--;
			FreeBlockIndex--;
		}
	}
}

uint8* FParamStorage::GetBlockMemory(FParamStorageHandle BlockHandle)
{
	uint8* BlockMemory = nullptr;

	const int32 BlockIndex = BlockHandle;

	if (ensure(BlockIndex >= 0 && BlockIndex < FreeBlockIndex))
	{
		bool IsSet = BlockFreeFlag[BlockIndex];
		ensure(IsSet == false);
		BlockMemory = (IsSet == false) ? RawMemory + BlockOffets[BlockIndex] : nullptr;
	}

	return BlockMemory;
}

bool FParamStorage::CheckBufferSize(int32 RequestedBlockSize)
{
	bool BufferSizeCorrect = true;

	if (CurrentBlocksSize + RequestedBlockSize > RawMemorySize)
	{
		if (AllowGrowing)
		{
			// TODO : Grow
			check(false);
		}
		else
		{
			BufferSizeCorrect = false;
		}
	}

	return BufferSizeCorrect;
}

bool FParamStorage::CheckNumBlocks(int32 BlockIndex)
{
	bool ValidIndex = true;

	if (BlockIndex >= BlockSizes.Num())
	{
		if (AllowGrowing)
		{
			// TODO : Grow
		}
		else
		{
			ValidIndex = false;
		}
		// TODO : Grow
	}

	return ValidIndex;
}

} // end namespace UE::AnimNext::Interface
