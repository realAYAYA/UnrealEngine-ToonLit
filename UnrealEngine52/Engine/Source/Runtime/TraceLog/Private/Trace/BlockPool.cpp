// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include "Platform.h"
#include "Trace/Detail/Atomic.h"
#include "Trace/Detail/Writer.inl"

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
void*		Writer_MemoryAllocate(SIZE_T, uint32);
void		Writer_MemoryFree(void*, uint32);



////////////////////////////////////////////////////////////////////////////////
struct FPoolPage
{
	FPoolPage*	NextPage;
	uint32		AllocSize;
};

////////////////////////////////////////////////////////////////////////////////
struct FPoolBlockList
{
	FWriteBuffer*	Head;
	FWriteBuffer*	Tail;
};



////////////////////////////////////////////////////////////////////////////////
#define T_ALIGN alignas(PLATFORM_CACHE_LINE_SIZE)
static const uint32						GPoolBlockSize		= 4 << 10;
static const uint32						GPoolPageSize		= GPoolBlockSize << 4;
static const uint32						GPoolInitPageSize	= GPoolBlockSize << 6;
T_ALIGN static FWriteBuffer* volatile	GPoolFreeList;		// = nullptr;
T_ALIGN static UPTRINT volatile			GPoolFutex;			// = 0
T_ALIGN static FPoolPage* volatile		GPoolPageList;		// = nullptr;
static uint32							GPoolUsage;			// = 0;
#undef T_ALIGN

////////////////////////////////////////////////////////////////////////////////
static FPoolBlockList Writer_AllocateBlockList(uint32 PageSize)
{
	// The free list is empty so we have to populate it with some new blocks.
	uint8* PageBase = (uint8*)Writer_MemoryAllocate(PageSize, PLATFORM_CACHE_LINE_SIZE);
	GPoolUsage += PageSize;

	uint32 BufferSize = GPoolBlockSize;
	BufferSize -= sizeof(FWriteBuffer);
	BufferSize -= sizeof(uint32); // to preceed event data with a small header when sending.

	// Link subsequent blocks together
	uint8* FirstBlock = PageBase + GPoolBlockSize - sizeof(FWriteBuffer);
	uint8* Block = FirstBlock;
	for (int i = 1, n = PageSize / GPoolBlockSize; ; ++i)
	{
		auto* Buffer = (FWriteBuffer*)Block;
		Buffer->Size = uint16(BufferSize);
		if (i >= n)
		{
			break;
		}

		AtomicStoreRelaxed(&(Buffer->NextBuffer), (FWriteBuffer*)(Block + GPoolBlockSize));
		Block += GPoolBlockSize;
	}

	// Keep track of allocation base so we can free it on shutdown
	FWriteBuffer* NextBuffer = (FWriteBuffer*)FirstBlock;
	NextBuffer->Size -= sizeof(FPoolPage);
	FPoolPage* PageListNode = (FPoolPage*)PageBase;
	PageListNode->NextPage = GPoolPageList;
	PageListNode->AllocSize = PageSize;
	GPoolPageList = PageListNode;

	return { NextBuffer, (FWriteBuffer*)Block };
}

////////////////////////////////////////////////////////////////////////////////
FWriteBuffer* Writer_AllocateBlockFromPool()
{
	// Fetch a new buffer
	FWriteBuffer* Ret;
	while (true)
	{
		// First we'll try one from the free list
		FWriteBuffer* Owned = AtomicLoadRelaxed(&GPoolFreeList);
		if (Owned != nullptr)
		{
			FWriteBuffer* OwnedNext = AtomicLoadRelaxed(&(Owned->NextBuffer));

			if (!AtomicCompareExchangeAcquire(&GPoolFreeList, OwnedNext, Owned))
			{
				PlatformYield();
				continue;
			}
		}

		// If we didn't fetch the sentinal then we've taken a block we can use
		if (Owned != nullptr)
		{
			Ret = (FWriteBuffer*)Owned;
			break;
		}

		// The free list is empty. Map some more memory.
		UPTRINT Futex = AtomicLoadRelaxed(&GPoolFutex);
		if (Futex || !AtomicCompareExchangeAcquire(&GPoolFutex, Futex + 1, Futex))
		{
			// Someone else is mapping memory so we'll briefly yield and try the
			// free list again.
			ThreadSleep(0);
			continue;
		}

		FPoolBlockList BlockList = Writer_AllocateBlockList(GPoolPageSize);
		Ret = BlockList.Head;

		// And insert the block list into the freelist. 'Block' is now the last block
		for (auto* ListNode = BlockList.Tail;; PlatformYield())
		{
			FWriteBuffer* FreeListValue = AtomicLoadRelaxed(&GPoolFreeList);
			AtomicStoreRelaxed(&(ListNode->NextBuffer), FreeListValue);

			if (AtomicCompareExchangeRelease(&GPoolFreeList, Ret->NextBuffer, FreeListValue))
			{
				break;
			}
		}

		// Let other threads proceed. They should hopefully hit the free list
		for (;; Private::PlatformYield())
		{
			if (AtomicCompareExchangeRelease<UPTRINT>(&GPoolFutex, 0, 1))
			{
				break;
			}
		}

		break;
	}

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
void Writer_FreeBlockListToPool(FWriteBuffer* Head, FWriteBuffer* Tail)
{
	for (FWriteBuffer* ListNode = Tail;; PlatformYield())
	{
		FWriteBuffer* FreeListValue = AtomicLoadRelaxed(&GPoolFreeList);
		AtomicStoreRelaxed(&(ListNode->NextBuffer), FreeListValue);

		if (AtomicCompareExchangeRelease(&GPoolFreeList, Head, FreeListValue))
		{
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void Writer_InitializePool()
{
	static_assert(GPoolPageSize >= 0x10000, "Page growth must be >= 64KB");
	static_assert(GPoolInitPageSize >= 0x10000, "Initial page size must be >= 64KB");
}

////////////////////////////////////////////////////////////////////////////////
void Writer_ShutdownPool()
{
	// Claim ownership of the pool page list. There really should be no one
	// creating so we'll just read it an go instead of a CAS loop.
	for (auto* Page = AtomicLoadRelaxed(&GPoolPageList); Page != nullptr;)
	{
		FPoolPage* NextPage = Page->NextPage;
		uint32 PageSize = (NextPage == nullptr) ? GPoolBlockSize : GPoolPageSize;
		Writer_MemoryFree(Page, PageSize);
		Page = NextPage;
	}
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
