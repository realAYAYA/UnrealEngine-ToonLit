// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include "Trace/Detail/Protocol.h"
#include "Trace/Detail/Transport.h"
#include "Trace/Trace.h"

#include <memory.h>

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
uint32	GetEncodeMaxSize(uint32);
int32	Encode(const void*, int32, void*, int32);
void*	Writer_MemoryAllocate(SIZE_T, uint32);
void	Writer_MemoryFree(void*, uint32);
void	Writer_SendDataRaw(const void*, uint32);
void	Writer_SendData(uint32, uint8* __restrict, uint32);

////////////////////////////////////////////////////////////////////////////////
struct alignas(16) FCacheBuffer
{
	union
	{
		FCacheBuffer*	Next;
		FCacheBuffer**	TailNext;
	};
	uint32				Size;
	uint32				Remaining;
	uint32				_Unused[3];
	uint32				Underflow; // For packet header
	uint8				Data[];
};



////////////////////////////////////////////////////////////////////////////////
static const uint32		GCacheBufferSize	= 64 << 10;
static const uint32		GCacheCollectorSize	= 1 << 10;
static FCacheBuffer*	GCacheCollector;	// = nullptr;
static FCacheBuffer*	GCacheActiveBuffer;	// = nullptr;
static FCacheBuffer*	GCacheHeadBuffer;	// = nullptr;
extern FStatistics		GTraceStatistics;

////////////////////////////////////////////////////////////////////////////////
static FCacheBuffer* Writer_CacheCreateBuffer(uint32 Size)
{
#if TRACE_PRIVATE_STATISTICS
	GTraceStatistics.CacheAllocated += Size;
#endif

	void* Block = Writer_MemoryAllocate(sizeof(FCacheBuffer) + Size, alignof(FCacheBuffer));
	auto* Buffer = (FCacheBuffer*)Block;
	Buffer->Size = Size;
	Buffer->Remaining = Buffer->Size;
	Buffer->Next = nullptr;
	return Buffer;
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_CacheCommit(const FCacheBuffer* Collector)
{
	// Check there's enough size to compress Data into.
	uint32 InputSize = uint32(Collector->Size - Collector->Remaining);
	uint32 EncodeMaxSize = GetEncodeMaxSize(InputSize);
	if (EncodeMaxSize + sizeof(FTidPacketEncoded) > GCacheActiveBuffer->Remaining)
	{
#if TRACE_PRIVATE_STATISTICS
		GTraceStatistics.CacheWaste += GCacheActiveBuffer->Remaining;
#endif

		// Retire active buffer
		*(GCacheActiveBuffer->TailNext) = GCacheActiveBuffer;
		GCacheActiveBuffer->TailNext = nullptr;

		// Fire up a new active buffer
		FCacheBuffer* NewBuffer = Writer_CacheCreateBuffer(GCacheBufferSize);
		NewBuffer->TailNext = &(GCacheActiveBuffer->Next);
		GCacheActiveBuffer = NewBuffer;
	}

	uint32 Used = GCacheActiveBuffer->Size - GCacheActiveBuffer->Remaining;
	auto* Packet = (FTidPacketEncoded*)(GCacheActiveBuffer->Data + Used);
	uint32 OutputSize = Encode(Collector->Data, InputSize, Packet->Data, EncodeMaxSize);

	Packet->PacketSize = uint16(OutputSize + sizeof(FTidPacketEncoded));
	Packet->ThreadId = FTidPacketBase::EncodedMarker | uint16(ETransportTid::Importants);
	Packet->DecodedSize = uint16(InputSize);

	Used = sizeof(FTidPacketEncoded) + OutputSize;
	GCacheActiveBuffer->Remaining -= Used;

#if TRACE_PRIVATE_STATISTICS
	GTraceStatistics.CacheUsed += Used;
#endif
}

////////////////////////////////////////////////////////////////////////////////
void Writer_CacheData(uint8* Data, uint32 Size)
{
	Writer_SendData(ETransportTid::Importants, Data, Size);

	if (GCacheCollector == nullptr)
	{
		return;
	}

	while (true)
	{
		uint32 StepSize = (Size < GCacheCollector->Remaining) ? Size : GCacheCollector->Remaining;

		uint32 Used = GCacheCollector->Size - GCacheCollector->Remaining;
		memcpy(GCacheCollector->Data + Used, Data, StepSize);

		GCacheCollector->Remaining -= StepSize;
		if (GCacheCollector->Remaining == 0)
		{
			Writer_CacheCommit(GCacheCollector);
			GCacheCollector->Remaining = GCacheCollector->Size;
		}

		Size -= StepSize;
		if (Size == 0)
		{
			break;
		}

		Data += StepSize;
	}
}

////////////////////////////////////////////////////////////////////////////////
void Writer_CacheOnConnect()
{
	if (GCacheCollector == nullptr)
	{
		return;
	}

	for (FCacheBuffer* Buffer = GCacheHeadBuffer; Buffer != nullptr; Buffer = Buffer->Next)
	{
		uint32 Used = Buffer->Size - Buffer->Remaining;
		Writer_SendDataRaw(Buffer->Data, Used);
	}

	if (uint32 Used = GCacheActiveBuffer->Size - GCacheActiveBuffer->Remaining)
	{
		Writer_SendDataRaw(GCacheActiveBuffer->Data, Used);
	}

	if (uint32 Used = GCacheCollector->Size - GCacheCollector->Remaining)
	{
		Writer_SendData(ETransportTid::Importants, GCacheCollector->Data, Used);
	}
}

////////////////////////////////////////////////////////////////////////////////
void Writer_InitializeCache()
{
	GCacheCollector = Writer_CacheCreateBuffer(GCacheCollectorSize);

	GCacheActiveBuffer = Writer_CacheCreateBuffer(GCacheBufferSize);
	GCacheActiveBuffer->TailNext = &GCacheHeadBuffer;

	static_assert(ETransport::Active == ETransport::TidPacketSync, "The important cache is transport aware");
}

////////////////////////////////////////////////////////////////////////////////
void Writer_ShutdownCache()
{
	for (FCacheBuffer* Buffer = GCacheHeadBuffer; Buffer != nullptr;)
	{
		FCacheBuffer* Next = Buffer->Next;
		Writer_MemoryFree(Buffer, GCacheBufferSize);
		Buffer = Next;
	}

	Writer_MemoryFree(GCacheActiveBuffer, GCacheBufferSize);
	Writer_MemoryFree(GCacheCollector, GCacheCollectorSize);
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
