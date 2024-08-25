// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineLogs.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Templates/IsSigned.h"
#include "Net/Core/Misc/ResizableCircularQueue.h"
#include "Util/SequenceNumber.h"
#include "Util/SequenceHistory.h"

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 0
#else
#	define UE_NET_ENABLE_PACKET_NOTIFY_LOG 1
#endif 

#if UE_NET_ENABLE_PACKET_NOTIFY_LOG
#	define UE_LOG_PACKET_NOTIFY(Format, ...)  UE_LOG(LogNetTraffic, Verbose, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_PACKET_NOTIFY(...)
#endif

#define UE_LOG_PACKET_NOTIFY_WARNING(Format, ...)  UE_LOG(LogNetTraffic, Warning, Format, ##__VA_ARGS__)

struct FBitWriter;
struct FBitReader;

namespace UE::Net::Private
{
	struct FNetPacketNotifyTestUtil;
}

/** 
	FNetPacketNotify - Drives delivery of sequence numbers, acknowledgments and notifications of delivery sequence numbers
*/
class FNetPacketNotify
{
public:
	enum { SequenceNumberBits = 14 };
	enum { MaxSequenceHistoryLength = 256 };

	typedef TSequenceNumber<SequenceNumberBits, uint16> SequenceNumberT;
	typedef TSequenceHistory<MaxSequenceHistoryLength> SequenceHistoryT;

	struct FNotificationHeader
	{
		SequenceHistoryT History;
		SIZE_T HistoryWordCount;
		SequenceNumberT Seq;
		SequenceNumberT AckedSeq;
	};

	/** Constructor */
	FNetPacketNotify();

	/** Init notification with expected initial sequence numbers */
	void Init(SequenceNumberT InitialInSeq, SequenceNumberT InitialOutSeq);

	/** Mark Seq as received and update current InSeq, missing sequence numbers will be marked as lost */
	void AckSeq(SequenceNumberT Seq) { AckSeq(Seq, true); }

	/** Explicitly mark Seq as not received and update current InSeq, additional missing sequence numbers will be marked as lost */
	void NakSeq(SequenceNumberT Seq) { AckSeq(Seq, false); }

	/** Increment outgoing seq number and commit data*/
	SequenceNumberT CommitAndIncrementOutSeq();

	/** Write NotificationHeader, and update outgoing ack record 
		if bRefresh is true we will attempt to refresh a previously written header if the resulting size will be the same as the already written header.
		returns true if data was written, and false if no data was written which might be the case if we try to rewrite an existing header but the required size differs.
	*/
	bool WriteHeader(FBitWriter& Writer, bool bRefresh = false);
	
	/** Read header from stream */
	bool ReadHeader(FNotificationHeader& Data, FBitReader& Reader) const;

	/**
	 * Gets the delta between the present sequence, and the sequence inside the specified header - if the delta is positive
	 */
	SequenceNumberT::DifferenceT GetSequenceDelta(const FNotificationHeader& NotificationData)
	{
		if (NotificationData.Seq > InSeq && NotificationData.AckedSeq >= OutAckSeq && OutSeq > NotificationData.AckedSeq)
		{
			return SequenceNumberT::Diff(NotificationData.Seq, InSeq);
		}
		else
		{
			return 0;
		}
	}

	/**
	 * Update state of PacketNotification based on received header and invoke packet notifications for received acks.
	 *
	 * @param NotificationData			The header to update from
	 * @param InFunc					A function in the format (void)(FNetPacketNotify::SequenceNumberT AckedSequence, bool bDelivered) to handle packet notifications.
	 * @return							The > 0 delta of the incoming seq if within half the seq number space. 0 if the received seq is outside current window ,or the ack seq received is invalid.
	*/
	template<class Functor>
	SequenceNumberT::DifferenceT Update(const FNotificationHeader& NotificationData, Functor&& InFunc);

	/** Get the current SequenceHistory */
	const SequenceHistoryT& GetInSeqHistory() const { return InSeqHistory; }

	/** Get the last received in sequence number */
	SequenceNumberT GetInSeq() const { return InSeq; }

	/** Get the last received sequence number that we have accepted, InAckSeq cannot be larger than InSeq */
	SequenceNumberT GetInAckSeq() const { return InAckSeq; }

	/** Get the current outgoing sequence number */
	SequenceNumberT GetOutSeq() const { return OutSeq; }

	/** Get the last outgoing sequence number acknowledged by remote */
	SequenceNumberT GetOutAckSeq() const { return OutAckSeq; }

	/** If we do have more unacknowledged sequence numbers in-flight than our maximum sendwindow we should not send more as the receiving end will not be able to detect if the sequence number has wrapped around */
	bool CanSend() const { SequenceNumberT NextOutSeq = OutSeq; ++NextOutSeq; return NextOutSeq >= OutAckSeq; }

	/**
	 * Return whether we can send packets without exhausting the packet sequence history window, as it could cause packets to be NAKed even when they've been received by the remote peer. 
	 * @param SafetyMargin A small number representing how many packets you would like to keep as a safety margin for heart beats or other important packets.
	 */
	bool IsSequenceWindowFull(uint32 SafetyMargin=0U) const;

	/** Get the current sequenceHistory length in bits, clamped to the maximum history length */
	SequenceNumberT::DifferenceT GetCurrentSequenceHistoryLength() const;

	/** Returns true if we are currently waiting for a flush of the sequence window */
	bool IsWaitingForSequenceHistoryFlush() const { return WaitingForFlushSeqAck > OutAckSeq; }

private:
	struct FSentAckData
	{
		SequenceNumberT OutSeq;	// Not needed... just to verify that things work as expected
		SequenceNumberT InAckSeq;
	};
	typedef TResizableCircularQueue<FSentAckData, TInlineAllocator<128>> AckRecordT;

	AckRecordT AckRecord;				// Track acked seq for each sent packet to track size of ack history
	SIZE_T WrittenHistoryWordCount;		// Bookkeeping to track if we can update data
	SequenceNumberT WrittenInAckSeq;	// When we call CommitAndIncrementOutSequence this will be committed along with the current outgoing sequence number for bookkeeping

	// Track incoming sequence data
	SequenceHistoryT InSeqHistory;		// BitBuffer containing a bitfield describing the history of received packets
	SequenceNumberT InSeq;				// Last sequence number received and accepted from remote
	SequenceNumberT InAckSeq;			// Last sequence number received from remote that we have acknowledged, this is needed since we support accepting a packet but explicitly not acknowledge it as received.
	SequenceNumberT InAckSeqAck;		// Last sequence number received from remote that we have acknowledged and also knows that the remote has received the ack, used to calculate how big our history must be
	SequenceNumberT WaitingForFlushSeqAck;

	// Track outgoing sequence data
	SequenceNumberT OutSeq;				// Outgoing sequence number
	SequenceNumberT OutAckSeq;			// Last sequence number that we know that the remote side have received.

private:

	SequenceNumberT UpdateInAckSeqAck(SequenceNumberT::DifferenceT AckCount, SequenceNumberT AckedSeq);
	SequenceNumberT::DifferenceT InternalUpdate(const FNotificationHeader& NotificationData, SequenceNumberT::DifferenceT InSeqDelta);

	// Returns true if sequence history contains any packets marked as received for which we have not yet received an ack
	bool GetHasUnacknowledgedAcks() const;

	// Returns true if we can acknowledge the Seq without overshooting the sequence history
	bool WillSequenceFitInSequenceHistory(SequenceNumberT Seq) const;

	// Initiates a wait for a flush of the sequence history
	void SetWaitForSequenceHistoryFlush();

	template<class Functor>
	inline void ProcessReceivedAcks(const FNotificationHeader& NotificationData, Functor&& InFunc);
	void AckSeq(SequenceNumberT AckedSeq, bool IsAck);

#if WITH_AUTOMATION_WORKER
	friend UE::Net::Private::FNetPacketNotifyTestUtil;
#endif
};

inline bool FNetPacketNotify::IsSequenceWindowFull(uint32 SafetyMargin) const
{
	const SequenceNumberT SequenceLength = OutSeq - OutAckSeq;
	return SequenceLength > MaxSequenceHistoryLength || (SafetyMargin >= MaxSequenceHistoryLength) || (SequenceLength.Get() > (MaxSequenceHistoryLength - SafetyMargin));
}

template<class Functor>
FNetPacketNotify::SequenceNumberT::DifferenceT FNetPacketNotify::Update(const FNotificationHeader& NotificationData, Functor&& InFunc)
{
	const SequenceNumberT::DifferenceT InSeqDelta = GetSequenceDelta(NotificationData);

	if (InSeqDelta > 0)
	{
		UE_LOG_PACKET_NOTIFY(TEXT("FNetPacketNotify::Update - Seq %u, InSeq %u"), NotificationData.Seq.Get(), InSeq.Get());
	
		ProcessReceivedAcks(NotificationData, InFunc);

		return InternalUpdate(NotificationData, InSeqDelta);
	}
	else
	{
		return 0;
	}
}

template<class Functor>
void FNetPacketNotify::ProcessReceivedAcks(const FNotificationHeader& NotificationData, Functor&& InFunc)
{
	if (NotificationData.AckedSeq > OutAckSeq)
	{
		SequenceNumberT::DifferenceT AckCount = SequenceNumberT::Diff(NotificationData.AckedSeq, OutAckSeq);

		UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks - AckedSeq: %u, OutAckSeq: %u AckCount: %u"), NotificationData.AckedSeq.Get(), OutAckSeq.Get(), AckCount);

		// Update InAckSeqAck used to track the needed number of bits to transmit our ack history
		// Note: As we might reset sequence history we need to check if we already have advanced the InAckSeqAck
		const SequenceNumberT NewInAckSeqAck = UpdateInAckSeqAck(AckCount, NotificationData.AckedSeq);
		if (NewInAckSeqAck > InAckSeqAck)
		{
			InAckSeqAck = NewInAckSeqAck;
		}
		
		// ExpectedAck = OutAckSeq + 1
		SequenceNumberT CurrentAck(OutAckSeq);
		++CurrentAck;

		// Make sure that we only look at the sequence history bit included in the notification data as the sequence history might have been reset, 
		// in which case we might not receive the max size history even though the ack-count is bigger than the history
		const SequenceNumberT::DifferenceT HistoryBits = NotificationData.HistoryWordCount * SequenceHistoryT::BitsPerWord;

		// Warn if the received sequence number is greater than our history buffer, since if that is the case we have to treat the data as lost
		// Note: This should normally not be a problem since we try to flush the sequence history before allowing an overshoot of the sequence history window on the sending side.
		// If this occurs with no hitches on server or client, there might be reason to investigate if too much data is being sent in which case the the size sequence history might have to be increased.
		if (AckCount > (SequenceNumberT::DifferenceT)(SequenceHistoryT::Size))
		{
			UE_LOG_PACKET_NOTIFY_WARNING(TEXT("FNetPacketNotify::ProcessReceivedAcks - Missed Acks: AckedSeq: %u, OutAckSeq: %u, FirstMissingSeq: %u Count: %u"), NotificationData.AckedSeq.Get(), OutAckSeq.Get(), CurrentAck.Get(), AckCount - (SequenceNumberT::DifferenceT)(SequenceHistoryT::Size));
		}

		// Everything not found in the history buffer is treated as lost
		while (AckCount > HistoryBits)
		{
			--AckCount;
			InFunc(CurrentAck, false);
			++CurrentAck;
		}

		// For sequence numbers contained in the history we lookup the delivery status from the history
		while (AckCount > 0)
		{
			--AckCount;
			UE_LOG_PACKET_NOTIFY(TEXT("Notification::ProcessReceivedAcks Seq: %u - IsAck: %u HistoryIndex: %u"), CurrentAck.Get(), NotificationData.History.IsDelivered(AckCount) ? 1u : 0u, AckCount);
			InFunc(CurrentAck, NotificationData.History.IsDelivered(AckCount));
			++CurrentAck;
		}
		OutAckSeq = NotificationData.AckedSeq;

		// Are we done waiting for an reset of the ack history?
		if (OutAckSeq > WaitingForFlushSeqAck)
		{
			WaitingForFlushSeqAck = OutAckSeq;
		}
	}
}


