// Copyright Epic Games, Inc. All Rights Reserved.

#include "TidPacketTransport.h"
#include "Algo/BinarySearch.h"
#include "HAL/UnrealMemory.h"
#include "CoreGlobals.h"

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
FTidPacketTransport::EReadPacketResult FTidPacketTransport::ReadPacket()
{
	using namespace UE::Trace::Private;

	const auto* PacketBase = GetPointer<FTidPacketBase>();
	if (PacketBase == nullptr)
	{
		return EReadPacketResult::NeedMoreData;
	}

	if (GetPointer<uint8>(PacketBase->PacketSize) == nullptr)
	{
		return EReadPacketResult::NeedMoreData;
	}

	FTransport::Advance(PacketBase->PacketSize);

	uint32 ThreadId = PacketBase->ThreadId & FTidPacketBase::ThreadIdMask;

	if (ThreadId == ETransportTid::Sync)
	{
		++Synced;
		return EReadPacketResult::NeedMoreData;	// Do not read any more packets. Gives consumers a
													// chance to sample the world at each known sync point.
	}

	bool bIsPartial = !!(PacketBase->ThreadId & FTidPacketBase::PartialMarker);
	FThreadStream* Thread = FindOrAddThread(ThreadId, !bIsPartial);
	if (Thread == nullptr)
	{
		return EReadPacketResult::Continue;
	}

	uint32 DataSize = PacketBase->PacketSize - sizeof(FTidPacketBase);
	if (PacketBase->ThreadId & FTidPacketBase::EncodedMarker)
	{
		const auto* Packet = (const FTidPacketEncoded*)PacketBase;
		uint16 DecodedSize = Packet->DecodedSize;
		uint8* Dest = Thread->Buffer.Append(DecodedSize);
		DataSize -= sizeof(DecodedSize);
		int32 ResultSize = UE::Trace::Private::Decode(Packet->Data, DataSize, Dest, DecodedSize);
		if (int32(DecodedSize) != ResultSize)
		{
			UE_LOG(LogCore, Error, TEXT("Unable to decompress packet, expected %d bytes but decoded %d bytes."), DecodedSize, ResultSize);
			return EReadPacketResult::ReadError;
		}
	}
	else
	{
		Thread->Buffer.Append((uint8*)(PacketBase + 1), DataSize);
	}

	return EReadPacketResult::Continue;
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
FTidPacketTransport::ETransportResult FTidPacketTransport::Update()
{
	EReadPacketResult Result;
	do
	{
		Result = ReadPacket();
	} while (Result == EReadPacketResult::Continue);

	Threads.RemoveAll([] (const FThreadStream& Thread)
	{
		return (Thread.ThreadId <= ETransportTid::Importants) ? false : Thread.Buffer.IsEmpty();
	});

	return Result == EReadPacketResult::ReadError ? ETransportResult::Error : ETransportResult::Ok;
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
