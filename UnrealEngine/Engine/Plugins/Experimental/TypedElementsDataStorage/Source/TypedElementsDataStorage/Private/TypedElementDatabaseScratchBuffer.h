// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include "Experimental/ConcurrentLinearAllocator.h"

// A thread-safe memory allocator that uses linear allocation. This provides a fast and lightweight way to allocate temporary memory 
// for intermediate values that will live at best until the end of the frame.
class FTypedElementDatabaseScratchBuffer
{
private:
	struct FScratchBufferAllocationTag
	{
		static constexpr uint32 BlockSize = 64 * 1024;		// Blocksize used to allocate from
		static constexpr bool AllowOversizedBlocks = true;  // The allocator supports oversized Blocks and will store them in a separate Block with counter 1
		static constexpr bool RequiresAccurateSize = true;  // GetAllocationSize returning the accurate size of the allocation otherwise it could be relaxed to return the size to the end of the Block
		static constexpr bool InlineBlockAllocation = false;  // Inline or Noinline the BlockAllocation which can have an impact on Performance
		static constexpr const char* TagName = "TEDS_ScratchBuffer";

		using Allocator = TBlockAllocationLockFreeCache<BlockSize, FOsAllocator>;
	};
	using MemoryAllocator = TConcurrentLinearBulkObjectAllocator<FScratchBufferAllocationTag>;

public:
	FTypedElementDatabaseScratchBuffer();

	void* Allocate(size_t Size, size_t Alignment);
	template<typename T, typename... ArgTypes>
	T* Emplace(ArgTypes... Args);
	template<typename T, typename... ArgTypes>
	T* EmplaceArray(int32 Count, const ArgTypes&... Args);

	// Activates a new allocator and deletes all commands and objects in the least recently touched scratch buffer.
	void BatchDelete();
constexpr static int32 MaxAllocationSize();

private:
	// Using a triple buffered approach because the direct API in TEDS (those calls that can be made directly to the API and don't go
	// through a context) are not required to be atomic. As such it's possible that data for a command is stored in allocator A while
	// the command is in allocator B if those calls are issued while TEDS is closing it's processing cycle. With double buffering this
	// would result in allocator A being flushed thus clearing out the data for the command. Using a triple buffered approach will
	// cause the clearing to be delayed by a frame, avoid this problem. This however does assume that all data and command issuing happens
	// within a single tick, though for the direct API this should always be true.

	MemoryAllocator Allocators[3];
	std::atomic<MemoryAllocator*> CurrentAllocator;
	MemoryAllocator* PreviousAllocator;
	MemoryAllocator* LeastRecentAllocator;
};

template<typename T, typename... ArgTypes>
T* FTypedElementDatabaseScratchBuffer::Emplace(ArgTypes... Args)
{
	return CurrentAllocator.load()->Create<T>(Forward<ArgTypes>(Args)...);
}

template<typename T, typename... ArgTypes>
T* FTypedElementDatabaseScratchBuffer::EmplaceArray(int32 Count, const ArgTypes&... Args)
{
	return CurrentAllocator.load()->CreateArray<T>(Count, Args...);
}

constexpr int32 FTypedElementDatabaseScratchBuffer::MaxAllocationSize()
{
	return FScratchBufferAllocationTag::BlockSize;
}
