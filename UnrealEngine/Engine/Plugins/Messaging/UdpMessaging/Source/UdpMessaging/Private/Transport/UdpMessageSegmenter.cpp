// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageSegmenter.h"
#include "HAL/Platform.h"
#include "UdpMessagingPrivate.h"
#include "Serialization/Archive.h"

#include "Transport/UdpSerializedMessage.h"


/* FUdpMessageSegmenter structors
 *****************************************************************************/

FUdpMessageSegmenter::~FUdpMessageSegmenter()
{
	if (MessageReader != nullptr)
	{
		delete MessageReader;
	}
}


/* FUdpMessageSegmenter interface
 *****************************************************************************/

int64 FUdpMessageSegmenter::GetMessageSize() const
{
	if (MessageReader == nullptr)
	{
		return 0;
	}

	return MessageReader->TotalSize();
}


bool FUdpMessageSegmenter::GetNextPendingSegment(TArray<uint8>& OutData, uint32& OutSegment) const
{
	if (MessageReader == nullptr)
	{
		return false;
	}

	for (TConstSetBitIterator<> It(PendingSendSegments); It; ++It)
	{
		OutSegment = It.GetIndex();

		uint64 SegmentOffset = static_cast<uint64>(OutSegment) * SegmentSize;
		uint64 ActualSegmentSize = MessageReader->TotalSize() - SegmentOffset;

		if (ActualSegmentSize > SegmentSize)
		{
			ActualSegmentSize = SegmentSize;
		}

		OutData.Reset(ActualSegmentSize);
		OutData.AddUninitialized(ActualSegmentSize);

		MessageReader->Seek(SegmentOffset);
		MessageReader->Serialize(OutData.GetData(), ActualSegmentSize);

		//FMemory::Memcpy(OutData.GetTypedData(), Message->GetTypedData() + SegmentOffset, ActualSegmentSize);

		return true;
	}

	return false;
}


bool FUdpMessageSegmenter::GetPendingSegment(uint32 InSegment, TArray<uint8>& OutData) const
{
	if (MessageReader == nullptr)
	{
		return false;
	}

	// Max segment number for protocol 12 is INT32_MAX, if increased, this will need changing
	if (InSegment < (uint32)PendingSendSegments.Num() && PendingSendSegments[InSegment])
	{
		uint64 SegmentOffset = static_cast<uint64>(InSegment) * SegmentSize;
		uint64 ActualSegmentSize = MessageReader->TotalSize() - SegmentOffset;

		if (ActualSegmentSize > SegmentSize)
		{
			ActualSegmentSize = SegmentSize;
		}

		OutData.Reset(ActualSegmentSize);
		OutData.AddUninitialized(ActualSegmentSize);

		MessageReader->Seek(SegmentOffset);
		MessageReader->Serialize(OutData.GetData(), ActualSegmentSize);

		return true;
	}

	return false;
}


void FUdpMessageSegmenter::Initialize()
{
	if (MessageReader != nullptr)
	{
		return;
	}

	if (SerializedMessage->GetState() == EUdpSerializedMessageState::Complete)
	{
		MessageReader = SerializedMessage->CreateReader();
		PendingSendSegmentsCount = (MessageReader->TotalSize() + SegmentSize - 1) / SegmentSize;
		PendingSendSegments.Init(true, PendingSendSegmentsCount);
		if (EnumHasAnyFlags(GetMessageFlags(), EMessageFlags::Reliable))
		{
			AcknowledgeSegments.Init(false, PendingSendSegmentsCount);
		}
		else
		{
			// Acks for unreliable messages are always zero
			AcknowledgeSegments.Init(false, 0);
		}		
		AcknowledgeSegmentsCount = 0;
	}
}


bool FUdpMessageSegmenter::IsMessageSerializationDone() const
{
	return SerializedMessage == nullptr || SerializedMessage->GetState() != EUdpSerializedMessageState::Incomplete;
}


bool FUdpMessageSegmenter::IsInvalid() const
{
	return (SerializedMessage->GetState() == EUdpSerializedMessageState::Invalid);
}


uint8 FUdpMessageSegmenter::GetProtocolVersion() const
{
	return SerializedMessage->GetProtocolVersion();
}


EMessageFlags FUdpMessageSegmenter::GetMessageFlags() const
{
	return SerializedMessage->GetFlags();
}

void FUdpMessageSegmenter::MarkAsSent(uint32 SegmentId)
{
	if (SegmentId < (uint32)PendingSendSegments.Num() && PendingSendSegments[SegmentId])
	{
		--PendingSendSegmentsCount;
		PendingSendSegments[SegmentId] = false;
		UE_LOG(LogUdpMessaging, Verbose, TEXT("Marking segment %d of %d as sent (%d outstanding)"), SegmentId+1, PendingSendSegments.Num(), PendingSendSegmentsCount);
	}
}

void FUdpMessageSegmenter::MarkAsSent(const TArray<uint32>& Segments)
{
	for (uint32 Segment : Segments)
	{
		MarkAsSent(Segment);
	}
}

void FUdpMessageSegmenter::MarkAsAcknowledged(uint32 Segment)
{
	// Mark this segment as acknowledged. There's a chance segments could be acknowledged
	// twice so we need to check state
	if (Segment < (uint32)AcknowledgeSegments.Num() && !AcknowledgeSegments[Segment])
	{
		++AcknowledgeSegmentsCount;
		AcknowledgeSegments[Segment] = true;
		UE_LOG(LogUdpMessaging, Verbose, TEXT("Marked segment %d of %d as acknowledged (%d outstanding)"), Segment + 1, AcknowledgeSegments.Num(), AcknowledgeSegments.Num() - AcknowledgeSegmentsCount);
	}

	// We may have queued a segment to be resent, if so there's now no need to resend it
	if (Segment < (uint32)PendingSendSegments.Num() && PendingSendSegments[Segment])
	{
		--PendingSendSegmentsCount;
		PendingSendSegments[Segment] = false;
		UE_LOG(LogUdpMessaging, Verbose, TEXT("Received acknowledgment for segment %d that was queued or requeued for transmission. Will skip send"), Segment + 1);
	}
}

void FUdpMessageSegmenter::MarkAsAcknowledged(const TArray<uint32>& Segments)
{
	if (ensure(EnumHasAnyFlags(GetMessageFlags(), EMessageFlags::Reliable)))
	{
		for (const auto& Segment : Segments)
		{
			MarkAsAcknowledged(Segment);
		}
	}
}

void FUdpMessageSegmenter::MarkForRetransmission(uint32 SegmentId)
{
	if (SegmentId < (uint32)PendingSendSegments.Num() && !PendingSendSegments[SegmentId])
	{
		UE_LOG(LogUdpMessaging, Verbose, TEXT("Marking segment %d of %d for retransmission"), SegmentId+1, PendingSendSegments.Num());

		++PendingSendSegmentsCount;
		PendingSendSegments[SegmentId] = true;
		// Note - we don't need to clear acknowledgments. If any segment is in transit and acknowledged after this
		// call we'll do that and if possible stop the pending send, and if not we don't need to wait for the ack
	}
}

void FUdpMessageSegmenter::MarkForRetransmission(const TArray<uint16>& Segments)
{
	for (uint16 Segment : Segments)
	{
		MarkForRetransmission(Segment);
	}
}

/**
 * Marks the entire message for retransmission.
 */
void FUdpMessageSegmenter::MarkForRetransmission()
{
	UE_LOG(LogUdpMessaging, Verbose, TEXT("Marking all %d segments for retransmission"), PendingSendSegments.Num());

	// mark all segments to be resent and clear any pending state
	PendingSendSegments.Init(true, PendingSendSegments.Num());
	PendingSendSegmentsCount = PendingSendSegments.Num();
	
	// Note - we don't need to clear acknowledgments. If any segment is in transit and acknowledged after this 
	// call we'll do that and if possible stop the pending send, and if not we don't need to wait for the ack
}


const FTimespan FUdpMessageSegmenter::SendInterval = FTimespan::FromMilliseconds(100);
const uint16    MaxNumResends = 16;
bool FUdpMessageSegmenter::NeedSending(const FDateTime& CurrentTime)
{
	// still have outstanding segments
	if (PendingSendSegmentsCount > 0)
	{
		return true;
	}

	if (AreAcknowledgementsComplete() == false && SentNumber > MaxNumResends)
	{
		UE_LOG(LogUdpMessaging, Warning, TEXT("Gave up sending with %d outstanding acks."), AcknowledgeSegments.Num() - AcknowledgeSegmentsCount);
		SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
		return false;
	}

	if (AreAcknowledgementsComplete() == false
		&& LastSentTime + SendInterval <= CurrentTime)
	{
		// We have gone through a period of time where packets or acks may have been lost,
		// so resend any segments that have yet to be acknowledged
		for (TBitArray<>::FConstIterator BIt(AcknowledgeSegments); BIt; ++BIt)
		{
			const int32 Index = BIt.GetIndex();
			if (BIt.GetValue() == false && !PendingSendSegments[Index])
			{
				PendingSendSegments[Index] = true;
				++PendingSendSegmentsCount;
			}
		}
		LastSentTime = CurrentTime;
		++SentNumber;
		UE_LOG(LogUdpMessaging, Warning, TEXT("Waiting for ack too long. Re-sending %d segments."), AcknowledgeSegments.Num() - AcknowledgeSegmentsCount);
		return true;
	}

	return false;
}

void FUdpMessageSegmenter::UpdateSentTime(const FDateTime& CurrentTime)
{
	LastSentTime = CurrentTime;
}
