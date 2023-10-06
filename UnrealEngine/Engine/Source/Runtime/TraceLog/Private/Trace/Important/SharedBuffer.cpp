// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Detail/Important/SharedBuffer.h"

#if UE_TRACE_ENABLED

#include "CoreTypes.h"
#include "Trace/Detail/Atomic.h"
#include "Trace/Detail/Important/ImportantLogScope.inl"

#ifndef PLATFORM_TRACE_WRITER_BUFFER_SIZE
#define PLATFORM_TRACE_WRITER_BUFFER_SIZE				( 1*1024 )
#endif // PLATFORM_TRACE_WRITER_BUFFER_SIZE

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
void*	Writer_MemoryAllocate(SIZE_T, uint32);
void	Writer_MemoryFree(void*, uint32);
void	Writer_CacheData(uint8*, uint32);

////////////////////////////////////////////////////////////////////////////////
static FSharedBuffer	GNullSharedBuffer	= { 0, FSharedBuffer::RefInit };
FSharedBuffer* volatile GSharedBuffer		= &GNullSharedBuffer;
static FSharedBuffer*	GTailBuffer;		// = nullptr
static uint32			GTailPreSent;		// = 0
static const uint32		GBlockSize			= PLATFORM_TRACE_WRITER_BUFFER_SIZE;		// Block size must be a power of two!
extern FStatistics		GTraceStatistics;

////////////////////////////////////////////////////////////////////////////////
static FSharedBuffer* Writer_CreateSharedBuffer(uint32 SizeHint=0)
{
	const uint32 OverheadSize = sizeof(FSharedBuffer) + sizeof(uint32);

	uint32 BlockSize = GBlockSize;
	if (SizeHint + OverheadSize > GBlockSize)
	{
		BlockSize += SizeHint + OverheadSize - GBlockSize;
		BlockSize += GBlockSize - 1;
		BlockSize &= ~(GBlockSize - 1);
	}

	void* Block = Writer_MemoryAllocate(BlockSize, alignof(FSharedBuffer));
	auto* Buffer = (FSharedBuffer*)(UPTRINT(Block) + BlockSize) - 1;

	Buffer->Size = uint32(UPTRINT(Buffer) - UPTRINT(Block));
	Buffer->Size -= sizeof(uint32); // to preceed event data with a small header when sending.
	Buffer->Cursor = (Buffer->Size << FSharedBuffer::CursorShift) | FSharedBuffer::RefInit;
	Buffer->Next = nullptr;
	Buffer->Final = 0;

	return Buffer;
}

////////////////////////////////////////////////////////////////////////////////
FNextSharedBuffer Writer_NextSharedBuffer(FSharedBuffer* Buffer, int32 RegionStart, int32 NegSizeAndRef)
{
	// Lock free allocation of the next buffer
	FSharedBuffer* NextBuffer;
	while (true)
	{
		bool bBufferOwner = (RegionStart >= 0);
		if (LIKELY(bBufferOwner))
		{
			// Allocate the next buffer ourselves
			uint32 Size = -NegSizeAndRef >> FSharedBuffer::CursorShift;
			NextBuffer = Writer_CreateSharedBuffer(Size);
			Buffer->Next = NextBuffer;
			Buffer->Final = RegionStart >> FSharedBuffer::CursorShift;
			AtomicStoreRelease(&GSharedBuffer, NextBuffer);
		}
		else
		{
			// Another thread is already allocating the next buffer, wait for that to complete
			for (;; PlatformYield())
			{
				NextBuffer = AtomicLoadAcquire(&GSharedBuffer);
				if (NextBuffer != Buffer)
				{
					break;
				}
			}
		}

		AtomicAddRelease(&(Buffer->Cursor), int32(FSharedBuffer::RefBit));

		// Try and allocate some space in the next buffer.  The next buffer may have insufficient
		// space if another thread used some of the memory we allocated, or if it was a buffer
		// allocated by another thread that's too small to fit our allocation size.
		RegionStart = AtomicAddRelaxed(&(NextBuffer->Cursor), NegSizeAndRef);
		if (LIKELY(RegionStart + NegSizeAndRef >= 0))
		{
			break;
		}

		Buffer = NextBuffer;
	}

	return { NextBuffer, RegionStart };
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_RetireSharedBufferImpl()
{
	// Send any unsent data.
	uint8* Data = (uint8*)GTailBuffer - GTailBuffer->Size + GTailPreSent;
	if (auto SendSize = UPTRINT(GTailBuffer) - UPTRINT(Data) - GTailBuffer->Final)
	{
#if TRACE_PRIVATE_STATISTICS
		GTraceStatistics.BytesTraced += SendSize;
#endif

		Writer_CacheData(Data, uint32(SendSize));
	}

	FSharedBuffer* Temp = GTailBuffer->Next;
	void* Block = (uint8*)GTailBuffer - GTailBuffer->Size - sizeof(uint32);
	Writer_MemoryFree(Block, GBlockSize);
	GTailBuffer = Temp;
	GTailPreSent = 0;
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_RetireSharedBuffer()
{
	// Spin until the buffer's no longer being used.
	for (;; PlatformYield())
	{
		int32 TailCursor = AtomicLoadAcquire(&(GTailBuffer->Cursor));
		if (LIKELY(((TailCursor + 1) & FSharedBuffer::RefInit) == 0))
		{
			break;
		}
	}

	Writer_RetireSharedBufferImpl();
}

////////////////////////////////////////////////////////////////////////////////
void Writer_UpdateSharedBuffers()
{
	FSharedBuffer* HeadBuffer = AtomicLoadAcquire(&GSharedBuffer);
	while (true)
	{
		if (GTailBuffer != HeadBuffer)
		{
			Writer_RetireSharedBuffer();
			continue;
		}

		int32 Cursor = AtomicLoadAcquire(&(HeadBuffer->Cursor));
		if ((Cursor + 1) & FSharedBuffer::RefInit)
		{
			continue;
		}

		Cursor = Cursor >> FSharedBuffer::CursorShift;
		if (Cursor < 0)
		{
			Writer_RetireSharedBufferImpl();
			break;
		}

		uint32 PreSentBias = HeadBuffer->Size - GTailPreSent;
		if (uint32 Sendable = PreSentBias - Cursor)
		{
			uint8* Data = (uint8*)(UPTRINT(HeadBuffer) - PreSentBias);
			Writer_CacheData(Data, Sendable);
			GTailPreSent += Sendable;
		}

		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
void Writer_InitializeSharedBuffers()
{
	FSharedBuffer* Buffer = Writer_CreateSharedBuffer();

	GTailBuffer = Buffer;
	GTailPreSent = 0;

	AtomicStoreRelease(&GSharedBuffer, Buffer);
}

////////////////////////////////////////////////////////////////////////////////
void Writer_ShutdownSharedBuffers()
{
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
