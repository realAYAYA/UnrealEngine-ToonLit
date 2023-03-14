// Copyright Epic Games, Inc. All Rights Reserved.

#include "TidPacketTransport.h"
#include "Algo/BinarySearch.h"
#include "HAL/UnrealMemory.h"

////////////////////////////////////////////////////////////////////////////////
namespace UE {
namespace Trace {
namespace Private {

TRACELOG_API int32 Decode(const void*, int32, void*, int32);

} // namespace Private
} // namespace Trace
} // namespace UE



namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
bool FTidPacketTransport::ReadPacket()
{
	using namespace UE::Trace::Private;

	const auto* PacketBase = GetPointer<FTidPacketBase>();
	if (PacketBase == nullptr)
	{
		return false;
	}

	if (GetPointer<uint8>(PacketBase->PacketSize) == nullptr)
	{
		return false;
	}

	FTransport::Advance(PacketBase->PacketSize);

	uint32 ThreadId = PacketBase->ThreadId & FTidPacketBase::ThreadIdMask;

	if (ThreadId == ETransportTid::Sync)
	{
		++Synced;
		return false;	// Do not read any more packets. Gives consumers a
						// chance to sample the world at each known sync point.
	}

	bool bIsPartial = !!(PacketBase->ThreadId & FTidPacketBase::PartialMarker);
	FThreadStream* Thread = FindOrAddThread(ThreadId, !bIsPartial);
	if (Thread == nullptr)
	{
		return true;
	}

	uint32 DataSize = PacketBase->PacketSize - sizeof(FTidPacketBase);
	if (PacketBase->ThreadId & FTidPacketBase::EncodedMarker)
	{
		const auto* Packet = (const FTidPacketEncoded*)PacketBase;
		uint16 DecodedSize = Packet->DecodedSize;
		uint8* Dest = Thread->Buffer.Append(DecodedSize);
		DataSize -= sizeof(DecodedSize);
		int32 ResultSize = UE::Trace::Private::Decode(Packet->Data, DataSize, Dest, DecodedSize);
		check(int32(DecodedSize) == ResultSize);
	}
	else
	{
		Thread->Buffer.Append((uint8*)(PacketBase + 1), DataSize);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
FTidPacketTransport::FThreadStream* FTidPacketTransport::FindOrAddThread(
	uint32	ThreadId,
	bool	bAddIfNotFound)
{
	uint32 ThreadCount = Threads.Num();
	for (uint32 i = 0; i < ThreadCount; ++i)
	{
		if (Threads[i].ThreadId == ThreadId)
		{
			return &(Threads[i]);
		}
	}
	
	if (!bAddIfNotFound)
	{
		return nullptr;
	}

	FThreadStream Thread;
	Thread.ThreadId = ThreadId;
	Threads.Add(Thread);
	return &(Threads[ThreadCount]);
}

////////////////////////////////////////////////////////////////////////////////
void FTidPacketTransport::Update()
{
	while (ReadPacket());

	Threads.RemoveAll([] (const FThreadStream& Thread)
	{
		return (Thread.ThreadId <= ETransportTid::Importants) ? false : Thread.Buffer.IsEmpty();
	});
}

////////////////////////////////////////////////////////////////////////////////
uint32 FTidPacketTransport::GetThreadCount() const
{
	return uint32(Threads.Num());
}

////////////////////////////////////////////////////////////////////////////////
FStreamReader* FTidPacketTransport::GetThreadStream(uint32 Index)
{
	return &(Threads[Index].Buffer);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FTidPacketTransport::GetThreadId(uint32 Index) const
{
	return Threads[Index].ThreadId;
}

} // namespace Trace
} // namespace UE
