// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"


namespace UE::RivermaxCore::Private
{
	//RTP Header used for 2110 following https://www.rfc-editor.org/rfc/rfc4175.html

	/* RTP Header -  12 bytes
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	| V |P|X|  CC   |M|     PT      |            SEQ                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           ssrc                                |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	SRD Header 8-14 bytes

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|    Extended Sequence Number   |           SRD Length          |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|F|     SRD Row Number          |C|         SRD Offset          |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

	/** Raw representation as it's built for the network */

/** @note When other platform than windows are supports, reverify support for pragma_pack and endianness */
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 1)
#endif
	struct FRawRTPHeader
	{
		uint32 ContributingSourceCount : 4;
		uint32 ExtensionBit : 1;
		uint32 PaddingBit : 1;
		uint32 Version : 2;
		uint32 PayloadType : 7;
		uint32 MarkerBit : 1;
		uint32 SequenceNumber : 16;
		uint32 Timestamp : 32;
		uint32 SynchronizationSource : 32;
		uint32 ExtendedSequenceNumber : 16;
		//SRD 1
		uint32 SRD1Length : 16;
		uint32 SRD1RowNumberHigh : 7;
		uint32 FieldIdentification1 : 1;
		uint32 SRD1RowNumberLow : 8;
		uint32 SRD1OffsetHigh : 7;
		uint32 ContinuationBit1 : 1;
		uint32 SRD1OffsetLow : 8;
		//SRD 2
		uint32 SRD2Length : 16;
		uint32 SRD2RowNumberHigh : 7;
		uint32 FieldIdentification2 : 1;
		uint32 SRD2RowNumberLow : 8;
		uint32 SRD2OffsetHigh : 7;
		uint32 ContinuationBit2 : 1;
		uint32 SRD2OffsetLow : 8;

		/** Returns SRD1 associated row number */
		uint16 GetSrd1RowNumber() const;

		/** Sets SRD1 associated row number */
		void SetSrd1RowNumber(uint16 RowNumber);

		/** Returns SRD1 pixel offset in its associated row */
		uint16 GetSrd1Offset() const;

		/** Sets SRD1 pixel offset in its associated row */
		void SetSrd1Offset(uint16 Offset);

		/** Sets SRD2 associated row number */
		uint16 GetSrd2RowNumber() const;

		/** Sets SRD1 associated row number */
		void SetSrd2RowNumber(uint16 RowNumber);

		/** Returns SRD1 pixel offset in its associated row */
		uint16 GetSrd2Offset() const;

		/** Sets SRD2 associated row number */
		void SetSrd2Offset(uint16 Offset);

		/** Size of RTP representation whether it has one or two SRDs */
		static constexpr uint32 OneSRDSize = 20;
		static constexpr uint32 TwoSRDSize = 26;
	};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

	struct FSRDHeader
	{
		/** Length of payload. Is a multiple of pgroup (see pixel formats) */
		uint16 Length = 0;

		/** False if progressive or first field of interlace. True if second field of interlace */
		bool bIsFieldOne = false;

		/** Video line number, starts at 0 */
		uint16 RowNumber = 0;

		/** Whether another SRD is following this one */
		bool bHasContinuation = false;

		/** Location of the first pixel in payload, in pixel */
		uint16 DataOffset = 0;
	};

	/** RTP header built from network representation not requiring any byte swapping */
	struct FRTPHeader
	{
		FRTPHeader() = default;
		FRTPHeader(const FRawRTPHeader& RawHeader);

		/** Returns the total payload of this RTP */
		uint16 GetTotalPayloadSize() const;
		
		/** Returns the payload size of the last SRD in this RTP */
		uint16 GetLastPayloadSize() const;

		/** Returns the row offset of the last SRD in this RTP */
		uint16 GetLastRowOffset() const;

		/** Returns the row number of the last SRD in this RTP */
		uint16 GetLastRowNumber() const;

		/** Sequence number including extension if present */
		uint32 SequencerNumber = 0;

		/** Timestamp of frame in the specified clock resolution. Video is typically 90kHz */
		uint32 Timestamp = 0;

		/** Identification of this stream */
		uint32 SyncSouceId = 0;

		/** Whether extensions (SRD headers) are present */
		bool bHasExtension = false;

		/** True if RTP packet is last of video stream */
		bool bIsMarkerBit = false;

		/** Only supports 2 SRD for now. Adjust if needed */
		FSRDHeader SRD1;
		FSRDHeader SRD2;
	};

	/** Returns RTPHeader pointer from a raw ethernet packet skipping 802, IP, UDP headers */
	const uint8* GetRTPHeaderPointer(const uint8* InHeader);
}


