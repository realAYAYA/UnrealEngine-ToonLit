// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformMemory.h"

struct FCachedOSPageAllocator
{
protected:
	struct FFreePageBlock
	{
		void*  Ptr;
		SIZE_T ByteSize;

		FFreePageBlock() 
		{
			Ptr      = nullptr;
			ByteSize = 0;
		}
	};

	void* AllocateImpl(SIZE_T Size, uint32 CachedByteLimit, FFreePageBlock* First, FFreePageBlock* Last, uint32& FreedPageBlocksNum, SIZE_T& CachedTotal, FCriticalSection* Mutex);
	void FreeImpl(void* Ptr, SIZE_T Size, uint32 NumCacheBlocks, uint32 CachedByteLimit, FFreePageBlock* First, uint32& FreedPageBlocksNum, SIZE_T& CachedTotal, FCriticalSection* Mutex, bool ThreadIsTimeCritical);
	void FreeAllImpl(FFreePageBlock* First, uint32& FreedPageBlocksNum, SIZE_T& CachedTotal, FCriticalSection* Mutex);

	static bool IsOSAllocation(SIZE_T Size, uint32 CachedByteLimit)
	{
		return (FPlatformMemory::BinnedPlatformHasMemoryPoolForThisSize(Size) || Size > CachedByteLimit / 4);
	}
};

template <uint32 NumCacheBlocks, uint32 CachedByteLimit>
struct TCachedOSPageAllocator : private FCachedOSPageAllocator
{
	TCachedOSPageAllocator()
		: CachedTotal(0)
		, FreedPageBlocksNum(0)
	{
	}

	FORCEINLINE void* Allocate(SIZE_T Size, uint32 AllocationHint = 0, FCriticalSection* Mutex = nullptr)
	{
		return AllocateImpl(Size, CachedByteLimit, FreedPageBlocks, FreedPageBlocks + FreedPageBlocksNum, FreedPageBlocksNum, CachedTotal, Mutex);
	}

	void Free(void* Ptr, SIZE_T Size, FCriticalSection* Mutex = nullptr, bool ThreadIsTimeCritical = false)
	{
		return FreeImpl(Ptr, Size, ThreadIsTimeCritical ? NumCacheBlocks*2 : NumCacheBlocks, CachedByteLimit, FreedPageBlocks, FreedPageBlocksNum, CachedTotal, Mutex, ThreadIsTimeCritical);
	}
	void FreeAll(FCriticalSection* Mutex = nullptr)
	{
		return FreeAllImpl(FreedPageBlocks, FreedPageBlocksNum, CachedTotal, Mutex);
	}
	// Refresh cached os allocator if needed. Does nothing for this implementation
	void Refresh()
	{

	}
	void UpdateStats()
	{

	}
	uint64 GetCachedFreeTotal()
	{
		return CachedTotal;
	}

	bool IsOSAllocation(SIZE_T Size)
	{
		return FCachedOSPageAllocator::IsOSAllocation(Size, CachedByteLimit);
	}

private:
	FFreePageBlock FreedPageBlocks[NumCacheBlocks*2];
	SIZE_T         CachedTotal;
	uint32         FreedPageBlocksNum;
};
