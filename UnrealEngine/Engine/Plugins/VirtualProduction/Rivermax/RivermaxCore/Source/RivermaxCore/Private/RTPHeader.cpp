// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTPHeader.h"

#include "RivermaxLog.h"

namespace UE::RivermaxCore::Private
{
	uint16 FRawRTPHeader::GetSrd1RowNumber() const
	{
		return ((SRD1RowNumberHigh << 8) | SRD1RowNumberLow);
	}

	void FRawRTPHeader::SetSrd1RowNumber(uint16 RowNumber)
	{
		SRD1RowNumberHigh = (RowNumber >> 8) & 0xFF;
		SRD1RowNumberLow = RowNumber & 0xFF;
	}

	uint16 FRawRTPHeader::GetSrd1Offset() const
	{
		return ((SRD1OffsetHigh << 8) | SRD1OffsetLow);
	}

	void FRawRTPHeader::SetSrd1Offset(uint16 Offset)
	{
		SRD1OffsetHigh = (Offset >> 8) & 0xFF;
		SRD1OffsetLow = Offset & 0xFF;
	}

	uint16 FRawRTPHeader::GetSrd2RowNumber() const
	{
		return ((SRD2RowNumberHigh << 8) | SRD2RowNumberLow);
	}

	void FRawRTPHeader::SetSrd2RowNumber(uint16 RowNumber)
	{
		SRD2RowNumberHigh = (RowNumber >> 8) & 0xFF;
		SRD2RowNumberLow = RowNumber & 0xFF;
	}

	uint16 FRawRTPHeader::GetSrd2Offset() const
	{
		return ((SRD2OffsetHigh << 8) | SRD2OffsetLow);
	}

	void FRawRTPHeader::SetSrd2Offset(uint16 Offset)
	{
		SRD2OffsetHigh = (Offset >> 8) & 0xFF;
		SRD2OffsetLow = Offset & 0xFF;
	}

	const uint8* GetRTPHeaderPointer(const uint8* InHeader)
	{
		check(InHeader);

		static constexpr uint32 ETH_TYPE_802_1Q = 0x8100;          /* 802.1Q VLAN Extended Header  */
		static constexpr uint32 RTP_HEADER_SIZE = 12;
		uint16* ETHProto = (uint16_t*)(InHeader + RTP_HEADER_SIZE);
		if (ETH_TYPE_802_1Q == ByteSwap(*ETHProto))
		{
			InHeader += 46; // 802 + 802.1Q + IP + UDP
		}
		else
		{
			InHeader += 42; // 802 + IP + UDP
		}
		return InHeader;
	}

	FRTPHeader::FRTPHeader(const FRawRTPHeader& RawHeader)
	{
		Timestamp = 0;

		if (RawHeader.Version != 2)
		{
			return;
		}

		// Pretty sure some data needs to be swapped but can't validate that until we have other hardware generating data
		SequencerNumber = (ByteSwap((uint16)RawHeader.ExtendedSequenceNumber) << 16) | ByteSwap((uint16)RawHeader.SequenceNumber);
		Timestamp = ByteSwap(RawHeader.Timestamp);
		bIsMarkerBit = RawHeader.MarkerBit;

		SyncSouceId = RawHeader.SynchronizationSource;

		SRD1.Length = ByteSwap((uint16)RawHeader.SRD1Length);
		SRD1.DataOffset = RawHeader.GetSrd1Offset();
		SRD1.RowNumber = RawHeader.GetSrd1RowNumber();
		SRD1.bIsFieldOne = RawHeader.FieldIdentification1;
		SRD1.bHasContinuation = RawHeader.ContinuationBit1;

		if (SRD1.bHasContinuation)
		{
			SRD2.Length = ByteSwap((uint16)RawHeader.SRD2Length);
			SRD2.DataOffset = RawHeader.GetSrd2Offset();
			SRD2.RowNumber = RawHeader.GetSrd2RowNumber();
			SRD2.bIsFieldOne = RawHeader.FieldIdentification2;
			SRD2.bHasContinuation = RawHeader.ContinuationBit2;

			if (SRD2.bHasContinuation == true)
			{
				UE_LOG(LogRivermax, Verbose, TEXT("Received SRD with more than 2 SRD which isn't supported."));
			}
		}
	}

	uint16 FRTPHeader::GetTotalPayloadSize() const
	{
		uint16 PayloadSize = SRD1.Length;
		if (SRD1.bHasContinuation)
		{
			PayloadSize += SRD2.Length;
		}

		return PayloadSize;
	}

	uint16 FRTPHeader::GetLastPayloadSize() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.Length;
		}

		return SRD1.Length;
	}

	uint16 FRTPHeader::GetLastRowOffset() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.DataOffset;
		}

		return SRD1.DataOffset;
	}

	uint16 FRTPHeader::GetLastRowNumber() const
	{
		if (SRD1.bHasContinuation)
		{
			return SRD2.RowNumber;
		}

		return SRD1.RowNumber;
	}
}






