// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/DateTime.h"
#include "UdpMessagingPrivate.h"
#include "Math/NumericLimits.h"

// IMessageContext forward declaration
enum class EMessageFlags : uint32;

/**
 * Implements a reassembled message.
 */
class FUdpReassembledMessage
{
public:

	/** Default constructor. */
	FUdpReassembledMessage() = default;

	/**
	 * Creates and initializes a new inbound message info.
	 *
	 * @param ProtocolVersion The protocol version the message is serialized in.
	 * @param MessageFlags The message flags of the reassembled message.
	 * @param MessageSize The total size of the message in bytes.
	 * @param SegmentCount The total number of segments that need to be received for this message.
	 * @param InSequence The message sequence number.
	 * @param InSender The IPv4 endpoint of the sender.
	 */
	FUdpReassembledMessage(uint8 InProtocolVersion, EMessageFlags InFlags, int64 MessageSize, uint32 SegmentCount, uint64 InSequence, const FIPv4Endpoint& InSender)
		: ProtocolVersion(InProtocolVersion)
		, MessageFlags(InFlags)
		, PendingSegmentsCount(SegmentCount)
		, ReceivedBytes(0)
		, bIsDelivered(false)
		, RetransmitRequestsCount(0)
		, Sender(InSender)
		, Sequence(InSequence)
	{
		// Containers only use int32 for indices, so we can't actually allow sizes greater than that
		// without causing overflow issues.
		if (MessageSize > static_cast<int64>(TNumericLimits<int32>::Max()) || MessageSize <= 0)
		{
			UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpReassembledMessage: Invalid MessageSize, marking message malformed. Message=%s, MessageSize=%lli"),
				*Describe(), MessageSize);
			bIsMalformed = true;
		}
		if (SegmentCount > static_cast<uint32>(TNumericLimits<int32>::Max()) || SegmentCount == 0)
		{
			UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpReassembledMessage: Invalid SegmentCount, marking message malformed. Message=%s, SegmentCount=%u"),
				*Describe(), SegmentCount);
			bIsMalformed = true;
		}

		// It's completely possible that MessageSize and SegmentCount are still incorrect due to corruption.
		// However, we have no way to detect that at this point, so we'll assume they're fine and validate
		// as we receive segments later.
		if (!IsMalformed())
		{
			PendingSegments = TBitArray<>(true, SegmentCount);
			Data.AddUninitialized(MessageSize);
		}
	}

	~FUdpReassembledMessage() = default;

public:

	/**
	 * Gets the message protocol version.
	 *
	 * @return The Message protocol version.
	 */
	uint8 GetProtocolVersion() const
	{
		return ProtocolVersion;
	}

	/**
	 * Gets the message flags
	 *
	 * @return The Message flags.
	 */
	EMessageFlags GetFlags() const
	{
		return MessageFlags;
	}

	/**
	 * Gets the message data.
	 *
	 * @return Message data.
	 */
	const TArray<uint8>& GetData() const
	{
		return Data;
	}

	/**
	 * Gets the time at which the last segment was received.
	 *
	 * @return Last receive time.
	 */
	FDateTime GetLastSegmentTime() const
	{
		return LastSegmentTime;
	}

	/**
	 * Gets the list of segments that haven't been received yet.
	 *
	 * @return List of pending segment numbers.
	 */
	TArray<uint16> GetPendingSegments() const
	{
		TArray<uint16> Result;

		if (PendingSegmentsCount > 0)
		{
			for (TConstSetBitIterator<> It(PendingSegments); It; ++It)
			{
				Result.Add(It.GetIndex());
			}
		}

		return Result;
	}


	/**
	 * Gets the total number of segments.
	 *
	 * @return Number of total segments.
	 */
	uint32 GetTotalSegmentsCount() const
	{
		return PendingSegments.Num();
	}

	/**
	 * Gets the number of segments that haven't been received yet.
	 *
	 * @return Number of pending segments.
	 */
	uint32 GetPendingSegmentsCount() const
	{
		return PendingSegmentsCount;
	}

	/**
	 * Gets the number of retransmit requests that were sent for this payload.
	 *
	 * @return Number of retransmit requests.
	 */
	int32 GetRetransmitRequestsCount() const
	{
		return RetransmitRequestsCount;
	}

	/**
	 * Gets the message's sequence number.
	 *
	 * @return Sequence number.
	 */
	uint64 GetSequence() const
	{
		return Sequence;
	}

	/**
	 * Checks whether this message is complete.
	 *
	 * @return true if the message is complete, false otherwise.
	 */
	bool IsComplete() const
	{
		return (PendingSegmentsCount == 0);
	}

	/**
	 * Checks whether this message has been initialized.
	 *
	 * @return true if the message is initialized, false otherwise.
	 */
	bool IsInitialized() const
	{
		return (Data.Num() > 0);
	}

	/**
	 * Checks whether this message has pending acknowledgments to send.
	 *
	 * @return true if the message has pending acknowledgments, false otherwise.
	 */
	bool HasPendingAcknowledgements() const
	{
		return PendingAcknowledgments.Num() > 0;
	}

	/**
	 * Get the list of pending Acknowledgments up to the size of one segment size and clear them
	 *
	 * @return The array of pending acknowledgments removed
	 */
	TArray<uint32> GetPendingAcknowledgments() 
	{
		TArray<uint32> Temp;
		const int32 MaxAckNum = (UDP_MESSAGING_SEGMENT_SIZE / sizeof(uint32));
		if (PendingAcknowledgments.Num() < MaxAckNum)
		{
			Swap(Temp, PendingAcknowledgments);
		}
		else
		{
			Temp.Append(PendingAcknowledgments.GetData(), MaxAckNum);
			PendingAcknowledgments.RemoveAt(0, MaxAckNum, false);
		}
		return Temp;
	}

	/**
	 * Get if the message has been delivered.
	 * @return true if the message has been marked delivered.
	 */
	bool IsDelivered() const
	{
		return bIsDelivered;
	}

	/**
	 * Mark the reasembled message as delivered.
	 */
	void MarkDelivered()
	{
		bIsDelivered = true;
	}


	/**
	 * Reassembles a segment into the specified message.
	 *
	 * @param SegmentNumber The number of the message segment.
	 * @param SegmentOffset The segment's offset within the message.
	 * @param SegmentData The segment data.
	 * @param CurrentTime The current time.
	 */
	void Reassemble(int32 SegmentNumber, int32 SegmentOffset, const TArray<uint8>& SegmentData, const FDateTime& CurrentTime)
	{
		if (IsMalformed() || !IsInitialized() || SegmentData.Num() == 0)
		{
			return;
		}
		else if (!PendingSegments.IsValidIndex(SegmentNumber))
		{
			UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpReassembledMessage::Reassemble: Invalid Segment Number, ignoring segment. Message=%s, Segment=%d"),
				*Describe(), SegmentNumber);
			return;
		}

		if (PendingSegments[SegmentNumber])
		{
			const int32 MaxAllowableOffset = Data.Num() - SegmentData.Num();
			if (SegmentOffset < 0 || SegmentOffset > MaxAllowableOffset)
			{
				UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpReassembledMessage::Reassemble: SegmentOffset is invalid or would overflow Data. Message=%s, Segment=%d, SegmentOffset=%d, SegmentSize=%d, DataSize=%d"),
					*Describe(), SegmentNumber, SegmentOffset, SegmentData.Num(), Data.Num());
				return;
			}

			FMemory::Memcpy(Data.GetData() + SegmentOffset, SegmentData.GetData(), SegmentData.Num());

			PendingSegments[SegmentNumber] = false;
			--PendingSegmentsCount;
			ReceivedBytes += SegmentData.Num();
		}

		LastSegmentTime = CurrentTime;

		// Send acknowledgement for unreliable messages
		if (EnumHasAnyFlags(GetFlags(), EMessageFlags::Reliable))
		{
			PendingAcknowledgments.AddUnique(SegmentNumber);
		}
	}

	/**
	 * Indicates whether or not this message was found to be malformed.
	 * Once we detect a message is malformed, we will not attempt to process any data for it.
	 */
	const bool IsMalformed() const
	{
		return bIsMalformed;
	}

	FString Describe() const
	{
		return FString::Printf(TEXT("Sender=%s, Sequence=%llu"), *Sender.ToString(), Sequence);
	}

	int32 GetReceivedBytes() const
	{
		return ReceivedBytes;
	}

private:

	/** Holds the message protocol version. */
	uint8 ProtocolVersion;

	/** */
	EMessageFlags MessageFlags;

	/** Holds the message data. */
	TArray<uint8> Data;

	/** Holds the time at which the last segment was received. */
	FDateTime LastSegmentTime;

	/** Holds an array of bits that indicate which segments still need to be received. */
	TBitArray<> PendingSegments;

	/** Holds the number of segments that haven't been received yet. */
	uint32 PendingSegmentsCount;

	/** Acknowledgment yet to be sent about segments we received */
	TArray<uint32> PendingAcknowledgments;

	/** Holds the number of bytes received so far. */
	int32 ReceivedBytes;

	/** Holds if the reassembled message has been delivered */
	bool bIsDelivered;

	/** Holds the number of retransmit requests that were sent since the last segment was received. */
	int32 RetransmitRequestsCount;

	/** Holds the sender. */
	FIPv4Endpoint Sender;

	/** Holds the message sequence. */
	uint64 Sequence;

	/** Indicates whether or not this message is malformed. */
	bool bIsMalformed = false;
};
