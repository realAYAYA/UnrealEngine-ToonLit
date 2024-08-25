// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/NetPacketNotify.h"
#include "CoreGlobals.h"
#include "Serialization/BitReader.h"
#include "Net/Util/SequenceHistory.h"
#include "Net/Util/SequenceNumber.h"

FNetPacketNotify::FNetPacketNotify()
	: AckRecord(64)
	, WrittenHistoryWordCount(0)
{
}

FNetPacketNotify::SequenceNumberT::DifferenceT FNetPacketNotify::GetCurrentSequenceHistoryLength() const
{
	if (InAckSeq >= InAckSeqAck)
	{
		return FMath::Min(SequenceNumberT::Diff(InAckSeq, InAckSeqAck), (SequenceNumberT::DifferenceT)SequenceHistoryT::Size);
	}
	else
	{
		// Worst case send full history
		return (SequenceNumberT::DifferenceT)SequenceHistoryT::Size;
	}
}

bool FNetPacketNotify::WillSequenceFitInSequenceHistory(SequenceNumberT Seq) const
{
	if (Seq >= InAckSeqAck)
	{
		return (SIZE_T)SequenceNumberT::Diff(Seq, InAckSeqAck) <= SequenceHistoryT::Size;
	}

	return false;
}

bool FNetPacketNotify::GetHasUnacknowledgedAcks() const
{
	for (SequenceNumberT::DifferenceT It = 0, EndIt = GetCurrentSequenceHistoryLength(); It < EndIt; ++It)
	{
		if (InSeqHistory.IsDelivered(It))
		{
			return true;
		}
	}
	return false;
}

FNetPacketNotify::SequenceNumberT FNetPacketNotify::UpdateInAckSeqAck(SequenceNumberT::DifferenceT AckCount, SequenceNumberT AckedSeq)
{
	if ((SIZE_T)AckCount <= AckRecord.Count())
	{
		if (AckCount > 1)
		{
			AckRecord.PopNoCheck(AckCount - 1);
		}

		FSentAckData AckData = AckRecord.PeekNoCheck();
		AckRecord.PopNoCheck();

		// verify that we have a matching sequence number
		if (AckData.OutSeq == AckedSeq)
		{
			UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::UpdateInAckSeqAck - InAckSeqAck: %u"), AckData.InAckSeq.Get());
			return AckData.InAckSeq;
		}
	}

	// Pessimistic view, should never occur but we do want to know about it if it would
	ensureMsgf(false, TEXT("FNetPacketNotify::UpdateInAckSeqAck - Failed to find matching AckRecord for %u"), AckedSeq.Get());
	
	return SequenceNumberT(AckedSeq.Get() - MaxSequenceHistoryLength);
}

void FNetPacketNotify::Init(SequenceNumberT InitialInSeq, SequenceNumberT InitialOutSeq)
{
	InSeqHistory.Reset();
	InSeq = InitialInSeq;
	InAckSeq = InitialInSeq;
	InAckSeqAck = InitialInSeq;
	OutSeq = InitialOutSeq;
	OutAckSeq = SequenceNumberT(InitialOutSeq.Get() - 1);
	WaitingForFlushSeqAck = OutAckSeq;
}

void FNetPacketNotify::SetWaitForSequenceHistoryFlush()
{
	UE_LOG_PACKET_NOTIFY_WARNING(TEXT("FNetPacketNotify::SetWaitForSequenceHistoryFlush - Wait for ack of next OutSeq: %u"), OutSeq.Get());
	WaitingForFlushSeqAck = OutSeq;
}

void FNetPacketNotify::AckSeq(SequenceNumberT AckedSeq, bool IsAck)
{
	check( AckedSeq == InSeq);

	while (AckedSeq > InAckSeq)
	{
		++InAckSeq;

		const bool bReportAcked = InAckSeq == AckedSeq ? IsAck : false;

		UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::AckSeq - AckedSeq: %u, IsAck %u, AckHistorySize: %d"), InAckSeq.Get(), bReportAcked ? 1u : 0u, GetCurrentSequenceHistoryLength());

		InSeqHistory.AddDeliveryStatus(bReportAcked);		
	}
}

FNetPacketNotify::SequenceNumberT::DifferenceT FNetPacketNotify::InternalUpdate(const FNotificationHeader& NotificationData, SequenceNumberT::DifferenceT InSeqDelta)
{
	// We must check if we will overflow our outgoing ack-window, if we do and it contains processed data we must initiate a re-sync of ack-sequence history.
	// This is done by ignoring any new packets until we are in sync again. 
	// This would typically only occur in situations where we would have had huge packet loss or spikes on the receiving end.
	if (!IsWaitingForSequenceHistoryFlush() && !WillSequenceFitInSequenceHistory(NotificationData.Seq))
	{
		if (GetHasUnacknowledgedAcks())
		{
			SetWaitForSequenceHistoryFlush();
		}
		else
		{
			// We can reset if we have no previous acks and can then safely synthesize nacks on the receiving end
			const SequenceNumberT NewInAckSeqAck(NotificationData.Seq.Get() - 1);
			UE_LOG_PACKET_NOTIFY_WARNING(TEXT("FNetPacketNotify::Reset SequenceHistory - New InSeqDelta: %u Old: %u"), NewInAckSeqAck.Get(), InAckSeqAck.Get());
			InAckSeqAck = NewInAckSeqAck;
		}
	}

	if (!IsWaitingForSequenceHistoryFlush())
	{
		// Just accept the incoming sequence, under normal circumstances NetConnection explicitly handles the acks.
		InSeq = NotificationData.Seq;

		return InSeqDelta;
	}
	else
	{
		// Until we have flushed the history we treat incoming packets as lost while still advancing ack window as far as we can.
		SequenceNumberT NewInSeqToAck(NotificationData.Seq);

		// Still waiting on flush, but we can fill up the history
		if (!WillSequenceFitInSequenceHistory(NotificationData.Seq) && GetHasUnacknowledgedAcks())
		{
			// Mark everything we can as lost up until the end of the sequence history 
			NewInSeqToAck = SequenceNumberT(InAckSeqAck.Get() + (MaxSequenceHistoryLength - GetCurrentSequenceHistoryLength()));
		}

		if (NewInSeqToAck >= InSeq)
		{
			const SequenceNumberT::DifferenceT AdjustedSequenceDelta = SequenceNumberT::Diff(NewInSeqToAck, InSeq);

			InSeq = NewInSeqToAck;

			// Nack driven from here
			AckSeq(NewInSeqToAck, false);

			UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::Update - Waiting for sequence history flush - Rejected: %u Accepted: InSeq: %u Adjusted delta %d"), NotificationData.Seq.Get(), InSeq.Get(), AdjustedSequenceDelta);
			
			return AdjustedSequenceDelta;
		}
		else
		{
			UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::Update - Waiting for sequence history flush - Rejected: %u Accepted: InSeq: %u"), NotificationData.Seq.Get(), InSeq.Get());
			return 0;
		}
	}
};

namespace 
{
	struct FPackedHeader
	{
		using SequenceNumberT = FNetPacketNotify::SequenceNumberT;

		static_assert(FNetPacketNotify::SequenceNumberBits <= 14, "SequenceNumbers must be smaller than 14 bits to fit history word count");

		enum { HistoryWordCountBits = 4 };
		enum { SeqMask				= (1 << FNetPacketNotify::SequenceNumberBits) - 1 };
		enum { HistoryWordCountMask	= (1 << HistoryWordCountBits) - 1 };
		enum { AckSeqShift			= HistoryWordCountBits };
		enum { SeqShift				= AckSeqShift + FNetPacketNotify::SequenceNumberBits };
		
		static uint32 Pack(SequenceNumberT Seq, SequenceNumberT AckedSeq, SIZE_T HistoryWordCount)
		{
			uint32 Packed = 0u;

			Packed |= Seq.Get() << SeqShift;
			Packed |= AckedSeq.Get() << AckSeqShift;
			Packed |= HistoryWordCount & HistoryWordCountMask;

			return Packed;
		}

		static SequenceNumberT GetSeq(uint32 Packed) { return SequenceNumberT(Packed >> SeqShift & SeqMask); }
		static SequenceNumberT GetAckedSeq(uint32 Packed) { return SequenceNumberT(Packed >> AckSeqShift & SeqMask); }
		static SIZE_T GetHistoryWordCount(uint32 Packed) { return (Packed & HistoryWordCountMask); }
	};
}

// These methods must always write and read the exact same number of bits, that is the reason for not using WriteInt/WrittedWrappedInt
bool FNetPacketNotify::WriteHeader(FBitWriter& Writer, bool bRefresh)
{
	// we always write at least 1 word
	SIZE_T CurrentHistoryWordCount = FMath::Clamp<SIZE_T>((GetCurrentSequenceHistoryLength() + SequenceHistoryT::BitsPerWord - 1u) / SequenceHistoryT::BitsPerWord, 1u, SequenceHistoryT::WordCount);

	// We can only do a refresh if we do not need more space for the history
	if (bRefresh && (CurrentHistoryWordCount > WrittenHistoryWordCount))
	{
		return false;
	}

	// How many words of ack data should we write? If this is a refresh we must write the same size as the original header
	WrittenHistoryWordCount = bRefresh ? WrittenHistoryWordCount : CurrentHistoryWordCount;
	// This is the last InAck we have acknowledged at this time
	WrittenInAckSeq = InAckSeq;

	SequenceNumberT::SequenceT Seq = OutSeq.Get();
	SequenceNumberT::SequenceT AckedSeq = InAckSeq.Get();

	// Pack data into a uint
	uint32 PackedHeader = FPackedHeader::Pack(Seq, AckedSeq, WrittenHistoryWordCount - 1);

	// Write packed header
	Writer << PackedHeader;

	// Write ack history
	InSeqHistory.Write(Writer, WrittenHistoryWordCount);

	UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::WriteHeader - Seq %u, AckedSeq %u bReFresh %u HistorySizeInWords %u"), Seq, AckedSeq, bRefresh ? 1u : 0u, WrittenHistoryWordCount);

	return true;
}

bool FNetPacketNotify::ReadHeader(FNotificationHeader& Data, FBitReader& Reader) const
{
	// Read packed header
	uint32 PackedHeader = 0;	
	Reader << PackedHeader;

	// unpack
	Data.Seq = FPackedHeader::GetSeq(PackedHeader);
	Data.AckedSeq = FPackedHeader::GetAckedSeq(PackedHeader);
	Data.HistoryWordCount = FPackedHeader::GetHistoryWordCount(PackedHeader) + 1;

	// Read ack history
	Data.History.Read(Reader, Data.HistoryWordCount);

	UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::ReadHeader - Seq %u, AckedSeq %u HistorySizeInWords %u"), Data.Seq.Get(), Data.AckedSeq.Get(), Data.HistoryWordCount);

	return Reader.IsError() == false;
}

FNetPacketNotify::SequenceNumberT FNetPacketNotify::CommitAndIncrementOutSeq()
{
	// we have not written a header...this is a fail.
	check(WrittenHistoryWordCount != 0);

	// Add entry to the ack-record so that we can update the InAckSeqAck when we received the ack for this OutSeq.
	AckRecord.Enqueue( {OutSeq, WrittenInAckSeq} );
	WrittenHistoryWordCount = 0u;
	
	return ++OutSeq;
}

