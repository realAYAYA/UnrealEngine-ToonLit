// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/NetPacketNotify.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"

FNetPacketNotify::FNetPacketNotify()
	: AckRecord(64)
	, WrittenHistoryWordCount(0)
{
}

SIZE_T FNetPacketNotify::GetCurrentSequenceHistoryLength() const
{
	if (InAckSeq >= InAckSeqAck)
	{
		return (SIZE_T)SequenceNumberT::Diff(InAckSeq, InAckSeqAck);
	}
	else
	{
		// Worst case send full history
		return SequenceHistoryT::Size;
	}
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
}

void FNetPacketNotify::AckSeq(SequenceNumberT AckedSeq, bool IsAck)
{
	check( AckedSeq == InSeq);

	while (AckedSeq > InAckSeq)
	{
		++InAckSeq;

		const bool bReportAcked = InAckSeq == AckedSeq ? IsAck : false;

		UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::AckSeq - AckedSeq: %u, IsAck %u"), InAckSeq.Get(), bReportAcked ? 1u : 0u);

		InSeqHistory.AddDeliveryStatus(bReportAcked);		
	}
}

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

