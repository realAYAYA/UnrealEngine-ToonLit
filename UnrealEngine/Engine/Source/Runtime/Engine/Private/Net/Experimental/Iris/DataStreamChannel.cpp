// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataStreamChannel.h"
#include "HAL/IConsoleManager.h"
#include "Templates/Casts.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataStreamChannel)

#if UE_WITH_IRIS
// IWYU pragma: begin_keep
#include "Engine/NetConnection.h"
#include "Iris/IrisConfig.h"
#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/PacketControl/PacketNotification.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Core/IrisMemoryTracker.h"
#include "Iris/Core/IrisLog.h"

#include "Net/Core/Trace/NetTrace.h"
#include "Net/Core/Connection/NetResult.h"
#include "Net/DataChannel.h"

#include "PacketHandler.h"
#include "ProfilingDebugging/CsvProfiler.h"
// IWYU pragma: end_keep
#endif // UE_WITH_IRIS

namespace UE::Net::Private
{
	static bool bIrisSaturateBandwidth = false;
	static FAutoConsoleVariableRef CVarIrisSaturateBandwidth(
		TEXT("net.Iris.SaturateBandwidth"),
		bIrisSaturateBandwidth,
		TEXT("Whether to saturate the bandwidth or not. Default is false."
		));

	static int32 IrisPacketSequenceSafetyMargin = 4;
	static FAutoConsoleVariableRef CVarIrisPacketSequenceSafetyMargin(
		TEXT("net.Iris.PacketSequenceSafetyMargin"),
		IrisPacketSequenceSafetyMargin,
		TEXT("How many packets to spare before considering packet sequence full. This allows a few non-DataStreamChannel packets to be sent without messing up packet acking."
		));
}

UDataStreamChannel::UDataStreamChannel(const FObjectInitializer& ObjectInitializer)
: UChannel(ObjectInitializer)
, WriteRecords(MaxPacketsInFlightCount)
, bIsReadyToHandshake(0U)
, bHandshakeSent(0U)
, bHandshakeComplete(0U)
{
	ChName = FName("DataStream");
}

void UDataStreamChannel::Init(UNetConnection* InConnection, int32 InChIndex, EChannelCreateFlags CreateFlags)
{
	Super::Init(InConnection, InChIndex, CreateFlags);

#if UE_WITH_IRIS
	{
		FDataStreamManagerInitParams InitParams;
		InitParams.PacketWindowSize = MaxPacketsInFlightCount;
		DataStreamManager = NewObject<UDataStreamManager>();
		DataStreamManager->Init(InitParams);
	}

	if (UReplicationSystem* ReplicationSystem = InConnection->Driver->GetReplicationSystem())
	{
		bIsReadyToHandshake = 1U;
		ReplicationSystem->InitDataStreams(InConnection->GetConnectionId(), DataStreamManager);
	}
#endif // UE_WITH_IRIS
}

bool UDataStreamChannel::CleanUp(const bool bForDestroy, EChannelCloseReason CloseReason)
{
#if UE_WITH_IRIS
	if (IsValid(DataStreamManager))
	{
		DataStreamManager->Deinit();
		DataStreamManager->MarkAsGarbage();
		DataStreamManager = nullptr;
	}

	WriteRecords.Reset();
#endif // UE_WITH_IRIS

	return Super::CleanUp(bForDestroy, CloseReason);
}

void UDataStreamChannel::ReceivedBunch(FInBunch& Bunch)
{
#if UE_WITH_IRIS
	using namespace UE::Net;

	IRIS_PROFILER_SCOPE(UDataStreamChannel_ReceivedBunch);

	// We are sending dummy bunches until we are open.
	if (!Bunch.GetNumBits())
	{
		return;
	}

	// We do not support partial bunches.
	if (Bunch.bPartial)
	{
		Bunch.SetAtEnd();
		Bunch.SetError();
		return;
	}

	FNetBitStreamReader BitReader;
	BitReader.InitBits(Bunch.GetData(), Bunch.GetNumBits());
	BitReader.Seek(Bunch.GetPosBits());

	FNetSerializationContext SerializationContext(&BitReader);
	SerializationContext.SetPacketId(Connection->InPacketId);

	// For packet stats
	SerializationContext.SetTraceCollector(Connection->GetInTraceCollector());

	DataStreamManager->ReadData(SerializationContext);

	// Set the bunch at the end
	Bunch.SetAtEnd();

	// If receiving was unsuccessful set bunch in error
	if (SerializationContext.HasErrorOrOverflow())
	{
		if (SerializationContext.GetErrorHandleContext().IsValid())
		{
			TNetResult<ENetCloseResult> NetResult(ENetCloseResult::IrisNetRefHandleError, FString::Printf(TEXT("IrisNetRefHandleError=%s"), *SerializationContext.GetErrorHandleContext().ToString()));
			AddToChainResultPtr(Bunch.ExtendedError, MoveTemp(NetResult));

			uint32 ErrorType = 0; //TBD
			uint64 RawHandleId = SerializationContext.GetErrorHandleContext().GetId();
			FNetControlMessage<NMT_IrisNetRefHandleError>::Send(Connection, ErrorType, RawHandleId);
		}
		Bunch.SetError();
	}

#endif // UE_WITH_IRIS
}

void UDataStreamChannel::SendOpenBunch()
{
#if UE_WITH_IRIS

	// We send this only once
	if (!bHandshakeSent && NumOutRec == 0)
	{
		// Send dummy data to open the channel
		constexpr int64 MaxBunchBits = 8;
		FOutBunch OutBunch(MaxBunchBits);
		OutBunch.ChName = this->ChName;
		OutBunch.ChIndex = this->ChIndex;
		OutBunch.Channel = this;
		OutBunch.Next = nullptr;
		// Unreliable bunches will be dropped on the receiving side unless the channel is open.
		OutBunch.bReliable = true;

		constexpr bool bAllowMerging = false;
		const FPacketIdRange PacketIds = SendBunch(&OutBunch, bAllowMerging);

		if (PacketIds.First != INDEX_NONE)
		{
			bHandshakeSent = 1U;
		}
	}

#endif // UE_WITH_IRIS
}

void UDataStreamChannel::PostTickDispatch()
{
#if UE_WITH_IRIS
	using namespace UE::Net;

	if (!Connection->Driver->IsUsingIrisReplication() || !bHandshakeComplete)
	{
		return;
	}

	if (IsPacketWindowFull() || !Connection->HasReceivedClientPacket() || (Connection->Handler != nullptr && !Connection->Handler->IsFullyInitialized()))
	{
		return;
	}

	// We probably want separate bandwidth management for iris as we are not pre-filling sendbuffer before call to NetReady.
	if (!IsNetReady(UE::Net::Private::bIrisSaturateBandwidth))
	{
		return;
	}

#if UE_NET_IRIS_CSV_STATS
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UDataStreamChannel_PostTickDispatch_Write);
#endif

	IRIS_PROFILER_SCOPE(UDataStreamChannel_PostTickDispatch);
	LLM_SCOPE_BYTAG(Iris);

	WriteData(EDataStreamWriteMode::PostTickDispatch);

#endif
}

void UDataStreamChannel::Tick()
{
#if UE_WITH_IRIS
	using namespace UE::Net;

	if (!Connection->Driver->IsUsingIrisReplication() || !bIsReadyToHandshake)
	{
		return;
	}

	if (IsPacketWindowFull() || !Connection->HasReceivedClientPacket() || (Connection->Handler != nullptr && !Connection->Handler->IsFullyInitialized()))
	{
		return;
	}

	// We probably want separate bandwidth management for iris as we are not pre-filling sendbuffer before call to NetReady.
	if (!IsNetReady(UE::Net::Private::bIrisSaturateBandwidth))
	{
		UE_CLOG(bHandshakeComplete, LogIris, Log, TEXT("Disallowed to write first packet in batch, with Iris this is not good!"))
		return;
	}

	// Wait for channel to open
	if (!bHandshakeComplete)
	{
		SendOpenBunch();
		return;
	}

#if UE_NET_IRIS_CSV_STATS
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UDataStreamChannel_Tick_Write);
#endif

	IRIS_PROFILER_SCOPE(UDataStreamChannel_Tick);
	LLM_SCOPE_BYTAG(Iris);

	WriteData(EDataStreamWriteMode::Full);

#endif
}

void UDataStreamChannel::WriteData(UE::Net::EDataStreamWriteMode WriteMode)
{
#if UE_WITH_IRIS
	using namespace UE::Net;

	// Limit the amount of bits to minimum of a bunch and our buffer. NetBitStreamWriter requires the number of bytes to be a multiple of 4.
	const uint32 MaxBitCount = uint32(Connection->GetMaxSingleBunchSizeBits());
	const uint32 MaxBytes = FPlatformMath::Min((MaxBitCount/32U)*4U, (uint32)sizeof(BitStreamBuffer));
	const int64 MaxBunchBits = MaxBytes*8;

	// Try to determine if we have headroom to write more than a single packet if needed.
	UDataStream::FBeginWriteParameters BeginWriteParams;
	BeginWriteParams.WriteMode = WriteMode;

	if (WriteMode == EDataStreamWriteMode::Full)
	{
		int32 CurrentQueuedBits = Connection->QueuedBits + Connection->SendBuffer.GetNumBits();
		if (CurrentQueuedBits < 0)
		{
			const int32 NumBunchesThatMightBeWritten = (-CurrentQueuedBits) / MaxBunchBits;

			// Indicate that DataStream can request more data
			BeginWriteParams.bCanWriteMoreData = NumBunchesThatMightBeWritten > 1;
		}
	}

	// Currently we want to use a full bunch so we flush if we have to
	bool bNeedsPreSendFlush = Connection->SendBuffer.GetNumBits() > MAX_PACKET_HEADER_BITS;

	auto WriteDataFunction = [this, &bNeedsPreSendFlush, &BeginWriteParams, MaxBunchBits, MaxBytes]()
	{
		if (bNeedsPreSendFlush)
		{
			IRIS_PROFILER_SCOPE(UDataStreamChannel_PreSendBunchAndFlushNet);
			Connection->FlushNet();
			bNeedsPreSendFlush = false;
		}

		// Make sure that packet header is written first to ensure that trace data is updated correctly
		if (Connection->SendBuffer.GetNumBits() == 0U)
		{
			Connection->WriteBitsToSendBuffer(nullptr, 0);
		}

		FNetBitStreamWriter BitWriter;
		BitWriter.InitBytes(BitStreamBuffer, MaxBytes);

		FNetSerializationContext SerializationContext(&BitWriter);
		SerializationContext.SetPacketId(Connection->OutPacketId);

#if UE_NET_TRACE_ENABLED	
		// For Iris we can use the connection trace collector as long as we make sure that the packet is prepared 
		FNetTraceCollector* Collector = Connection->GetOutTraceCollector();
		SerializationContext.SetTraceCollector(Collector);
		UE_NET_TRACE_BEGIN_BUNCH(Collector);
#endif

		const FDataStreamRecord* Record = nullptr;
		UDataStream::EWriteResult WriteResult = DataStreamManager->WriteData(SerializationContext, Record);
		if (WriteResult == UDataStream::EWriteResult::NoData || SerializationContext.HasError())
		{
			IRIS_PROFILER_SCOPE(UDataStreamChannel_NoDataSent);
			// Do not report the bunch
			UE_NET_TRACE_DISCARD_BUNCH(Collector);

			if (SerializationContext.HasError())
			{
				FString ErrorMsg = NSLOCTEXT("NetworkErrors", "DataStreamChannelWriteData", "DataStreamChannel failed to write data.").ToString();
				Connection->SendCloseReason(ENetCloseResult::HostClosedConnection);
				FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
				Connection->FlushNet(true);
				Connection->Close(ENetCloseResult::HostClosedConnection);
			}
			
			return UDataStream::EWriteResult::NoData;
		}

		// Flush bitstream
		BitWriter.CommitWrites();

		IRIS_PROFILER_SCOPE(UDataStreamChannel_SendBunchAndFlushNet);

		FOutBunch OutBunch(MaxBunchBits);
#if UE_NET_TRACE_ENABLED
		SetTraceCollector(OutBunch, Collector);
#endif
		OutBunch.ChName = this->ChName;
		OutBunch.ChIndex = this->ChIndex;
		OutBunch.Channel = this;
		OutBunch.Next = nullptr;
		// Unreliable bunches will be dropped on the receiving side unless the channel is open.
		OutBunch.bReliable = !OpenAcked;
		OutBunch.SerializeBits(BitStreamBuffer, BitWriter.GetPosBits());

		constexpr bool bAllowMerging = false;
		const FPacketIdRange PacketIds = SendBunch(&OutBunch, bAllowMerging);

#if UE_NET_TRACE_ENABLED
		// Since we steal the connection collector, we need to clear it out before the bunch goes out of scope
		SetTraceCollector(OutBunch, nullptr);
#endif

		if (PacketIds.First == INDEX_NONE)
		{
			check(PacketIds.First != INDEX_NONE);
			// Something went wrong.
			return UDataStream::EWriteResult::NoData;
		}

		FDataStreamChannelRecord ChannelRecord;
		ChannelRecord.Record = Record;
		ChannelRecord.PacketId = PacketIds.First;

		WriteRecords.Enqueue(ChannelRecord);

		// If we are allowed to write more data, we need to flush
		bNeedsPreSendFlush = true;

		return WriteResult;
	};

	// Begin the write, if we have nothing todo, just return
	if (DataStreamManager->BeginWrite(BeginWriteParams) == UDataStream::EWriteResult::NoData)
	{
		return;
	}

	do 
	{
		// Write data until we are not allowed to write more
	}
	while ((WriteDataFunction() == UDataStream::EWriteResult::HasMoreData) && IsNetReady(UE::Net::Private::bIrisSaturateBandwidth) && (!IsPacketWindowFull()));

	// call end write to cleanup data initialized in BeginWrite	
	DataStreamManager->EndWrite();

	// If we did write data and the current WriteMode is PostTickDispatch we flush the packet here.
	if (WriteMode == EDataStreamWriteMode::PostTickDispatch && bNeedsPreSendFlush)
	{
		IRIS_PROFILER_SCOPE(UDataStreamChannel_FlushNet);
		Connection->FlushNet();	
	}

#endif // UE_WITH_IRIS
}

bool UDataStreamChannel::CanStopTicking() const
{
	return false;
}

FString UDataStreamChannel::Describe()
{
	return FString(TEXT("DataStream: ")) + UChannel::Describe();
}

void UDataStreamChannel::ReceivedAck(int32 PacketId)
{
#if UE_WITH_IRIS

	if (!bHandshakeComplete)
	{
		if (bHandshakeSent)
		{
			bHandshakeComplete = 1U;
		}
		return;
	}

	const FDataStreamChannelRecord& ChannelRecord = WriteRecords.Peek();
	if (ensureMsgf((uint32)PacketId == ChannelRecord.PacketId, TEXT("PacketId %d != ChannelRecord.PacketId %d, WriteRecords.Num %d"), PacketId, ChannelRecord.PacketId, (int32)WriteRecords.Count()))
	{
		DataStreamManager->ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus::Delivered, static_cast<const FDataStreamRecord*>(ChannelRecord.Record));
		WriteRecords.Pop();
	}

#endif // UE_WITH_IRIS
}

void UDataStreamChannel::ReceivedNak(int32 PacketId)
{
#if UE_WITH_IRIS

	if (!bHandshakeComplete)
	{
		// Rely on super to resend our open request
		Super::ReceivedNak(PacketId);
		return;
	}

	const FDataStreamChannelRecord& ChannelRecord = WriteRecords.Peek();

	if (ensureMsgf((uint32)PacketId == ChannelRecord.PacketId, TEXT("PacketId %d != ChannelRecord.PacketId %d, WriteRecords.Num %d"), PacketId, ChannelRecord.PacketId, (int32)WriteRecords.Count()))
	{
		DataStreamManager->ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus::Lost, static_cast<const FDataStreamRecord*>(ChannelRecord.Record));
		WriteRecords.Pop();
	}

#endif // UE_WITH_IRIS
}

// Some DataStreams require perfect acking. If the ack sequence window is full we would get NAKs for packets thay may have been received.
bool UDataStreamChannel::IsPacketWindowFull() const
{
	const uint32 IrisPacketSequenceSafetyMarginUnsigned = static_cast<uint32>(FPlatformMath::Max(0, UE::Net::Private::IrisPacketSequenceSafetyMargin));
	if (Connection->IsPacketSequenceWindowFull(IrisPacketSequenceSafetyMarginUnsigned))
	{
		UE_LOG(LogIris, Verbose, TEXT("Packet window full."));
		return true;
	}

	return WriteRecords.Count() >= WriteRecords.AllocatedCapacity();
}

void UDataStreamChannel::AddReferencedObjects(UObject* Object, FReferenceCollector& Collector)
{
	UDataStreamChannel* Channel = CastChecked<UDataStreamChannel>(Object);
	if (Channel->DataStreamManager)
	{
		Collector.AddReferencedObject(Channel->DataStreamManager);
	}

	Super::AddReferencedObjects(Channel, Collector);
}

void UDataStreamChannel::AppendExportBunches(TArray<FOutBunch*>& OutExportBunches)
{
}

void UDataStreamChannel::AppendMustBeMappedGuids(FOutBunch* Bunch)
{
}
