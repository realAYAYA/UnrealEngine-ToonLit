// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/RemoveIf.h"
#include "CoreTypes.h"
#include "Common/UdpSocketReceiver.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/Runnable.h"
#include "IMessageContext.h"
#include "IMessageTransport.h"

#include "INetworkMessagingExtension.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/SingleThreadRunnable.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "UdpMessageSegmenter.h"
#include "UdpMessagingPrivate.h"
#include "Shared/UdpMessageSegment.h"
#include "Transport/UdpCircularQueue.h"

class FArrayReader;
class FEvent;
class FRunnableThread;
class FSocket;
class FUdpMessageBeacon;
class FUdpMessageSegmenter;
class FUdpReassembledMessage;
class FUdpSerializedMessage;
class FUdpSocketSender;
class IMessageAttachment;
enum class EUdpMessageFormat : uint8;

/**
 * Running statistics as known by the UdpMessageProcessor. This will be per endpoint.
 */
struct FUdpMessageTransportStatistics
{
	uint64 BytesSent    = 0;
	uint64 SegmentsLost = 0;
	uint64 AcksReceived = 0;
	uint64 SegmentsSent = 0;
	uint64 SegmentsReceived = 0;

	int32 SegmentsInFlight = 0;
	uint32 WindowSize = 0;

	FTimespan AverageRTT{0};
	FIPv4Endpoint IpAddress;
};

/** Holds the current statistics for a given message segmenter. */
struct FUdpSegmenterStats
{
	FGuid  NodeId;
	int32  MessageId     = 0;
	uint32 SegmentsSent  = 0;
	uint32 SegmentsAcked = 0;
	uint32 TotalSegments = 0;
};

/** Information about what was sent over the wire.  */
struct FSentData
{
	int32 MessageId = 0;
	uint32 SegmentNumber = 0;

	uint64 SequenceNumber = 0;
	bool bIsReliable = false;

	FDateTime TimeSent;
};

/** Returned by the segment processor. It provides details on what was sent and how it was sent. */
struct FSentSegmentInfo
{
	uint64 SequenceNumber = 0;
	uint32 BytesSent = 0;

	bool bIsReliable = false;
	bool bRequiresRequeue = false;
	bool bSendSocketError = false;

	/** Converts FSentSegmentInfo into a FSentData struct. */
	FSentData AsSentData(int32 MessageId, uint32 SegmentNumber, const FDateTime& CurrentTime)
	{
		return {
			MessageId,
			SegmentNumber,
			SequenceNumber,
			bIsReliable,
			CurrentTime
		};
	}

	bool WasSent() const
	{
		return BytesSent > 0;
	}
};

namespace UE::Private::MessageProcessor
{
/**
 * Global delegate for handling segmenter (aka sent data) updates.
 */
FOnOutboundTransferDataUpdated &OnSegmenterUpdated();

/*
 * Global delegate for handling reassembler (aka received data) updates.
 */
FOnInboundTransferDataUpdated &OnReassemblerUpdated();

}

/**
 * Implements a message processor for UDP messages.
 */
class FUdpMessageProcessor
	: public FRunnable
	, private FSingleThreadRunnable
{
	/** Structure for known remote endpoints. */
	struct FNodeInfo
	{
		/** Holds the node's IP endpoint. */
		FIPv4Endpoint Endpoint;

		/** Holds the time at which the last Hello was received. */
		FDateTime LastSegmentReceivedTime;

		/** Holds the endpoint's node identifier. */
		FGuid NodeId;

		/** Holds the protocol version this node is communicating with */
		uint8 ProtocolVersion;

		/** Holds the collection of reassembled messages. */
		TMap<int32, TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>> ReassembledMessages;

		/** Holds the collection of message segmenters. */
		TMap<int32, TSharedPtr<FUdpMessageSegmenter>> Segmenters;

		/** Holds of queue of MessageIds to send. They are processed in round-robin fashion. */
		TUdpCircularQueue<int32> WorkQueue;

		/** Last time we issued a warning about work queues. */
		FDateTime LastWorkQueueFullMessage;

		/** The number of seconds we have been in a send full state. */
		double SecondsInFullQueueState = 0;

		/** A map from sequence id to information about what was sent. The size of this map is the size of our sending window. */
		TMap<uint64, FSentData> InflightSegments;

		/** Unique sequence id for every packet sent. */
		uint64 SequenceId = 0;

		/** Average time from when the packet was sent to ack received. */
		FTimespan AvgRoundTripTime = FTimespan::FromMilliseconds(20);

		/** Maximum number of segments we can have in flight. Start with a small value and let it grow based on performance. */
		uint64 WindowSize = 64;

		/** Various transport statistics for this endpoint */
		FMessageTransportStatistics Statistics;

		/** Default constructor. */
		FNodeInfo();

		/**
		 * Check the work queue buffer and note a warning if we are overcommitted.
		 */
		bool CanCommitToWorkQueue(const FDateTime& InCurrentTime)
		{
			const bool bWorkQueueFull = WorkQueue.IsFull();
			if (bWorkQueueFull)
			{
				const double ReportingIntervalInSeconds = 1;
				double Seconds = (InCurrentTime - LastWorkQueueFullMessage).GetTotalSeconds();
				if (Seconds > ReportingIntervalInSeconds)
				{
					SecondsInFullQueueState += Seconds;
					UE_LOG(LogUdpMessaging, Warning,
						   TEXT("Work queue for node %s is full. Send queue has been congested for %.3f seconds."),
						   *Endpoint.ToString(), SecondsInFullQueueState);
					LastWorkQueueFullMessage = InCurrentTime;
				}
			}
			else
			{
				LastWorkQueueFullMessage = InCurrentTime;
				SecondsInFullQueueState = 0;
			}
			return !bWorkQueueFull;
		}

		/**
		 * Remap a MessageId and SegmentId pair into a 64 bit number using Szudzik pairing calculation.
		 */
		uint64 Remap(uint32 MessageId, uint32 SegmentId)
		{
			// http://szudzik.com/ElegantPairing.pdf
			// a >= b ? a * a + a + b : a + b * b
			//
			if (MessageId >= SegmentId)
			{
				return MessageId * MessageId + MessageId + SegmentId;
			}
			else
			{
				return SegmentId * SegmentId + MessageId;
			}
		}

		/**
		 * Update the segmenter with the given acts. If all acks have been received then remove it from the list of segmenters.
		 */
		TSharedPtr<FUdpMessageSegmenter> MarkAcksOnSegmenter(int32 MessageId, const TArray<uint32>& Segments, const FDateTime& InCurrentTime)
		{
			TSharedPtr<FUdpMessageSegmenter> Segmenter = Segmenters.FindRef(MessageId);
			if (Segmenter.IsValid())
			{
				Segmenter->MarkAsAcknowledged(Segments);
				Segmenter->UpdateSentTime(InCurrentTime);
				if (Segmenter->IsSendingComplete() && Segmenter->AreAcknowledgementsComplete())
				{
					UE_LOG(LogUdpMessaging, Verbose, TEXT("Segmenter for %s is now complete. Removing"), *NodeId.ToString());
					Segmenters.Remove(MessageId);
				}
			}
			else
			{
				UE_LOG(LogUdpMessaging, Verbose, TEXT("No such segmenter for message %d"), MessageId);
			}
			return Segmenter;
		}

		/**
		 * Helper template to allow us to remove types of segments in flight.
		 */
		template <typename PredFunc>
		void RemoveInflightIf(PredFunc&& Pred)
		{
			for (TMap<uint64, FSentData>::TIterator ItRemove = InflightSegments.CreateIterator(); ItRemove; ++ItRemove)
			{
				if (Pred(ItRemove.Value()))
				{
					ItRemove.RemoveCurrent();
				}
			}
		}

		/**
		 * Update the calcuated round trip time based on the computed span of inflight packet.
		 */
		void CalculateNewRTT(const FTimespan& NewSpan)
		{
			const float Weight = 0.6f;
			Statistics.AverageRTT = Weight * Statistics.AverageRTT + (1-Weight) * NewSpan;
		}

		/**
		 * Remove any unreliables that are too old.  Too old is based on the average round trip time of a packet.
		 */
		void RemoveOldUnreliables(const FDateTime& InCurrentTime)
		{
			// Remove any unreliable packets that fall outside of our average RTT
			RemoveInflightIf([InCurrentTime, this](const FSentData& Data)
			{
				return ((InCurrentTime - Data.TimeSent) > Statistics.AverageRTT) && !Data.bIsReliable;
			});
		}

		/** 
		 * Compute the new window size for the connection.  We use a AIMD algorithm:
		 * https://en.wikipedia.org/wiki/Additive_increase/multiplicative_decrease
		 */
		void ComputeWindowSize(uint32 NumAcks, uint32 SegmentLoss)
		{
			const uint64 MinimumWindowSize = 64;
			const uint64 MaximumWindowSize = 2048;
			if (SegmentLoss == 0)
			{
				// If we did not get any packet loss increase our window size.
				WindowSize = FGenericPlatformMath::Min<uint64>(WindowSize+NumAcks, MaximumWindowSize);
			}
			else
			{
				// In the case of segment loss half our window size to a minimum value.
				WindowSize = FGenericPlatformMath::Max<uint64>(WindowSize/2, MinimumWindowSize);
			}
			Statistics.PacketsAcked += NumAcks;
			Statistics.WindowSize = WindowSize;
		}

		/**
		 * Remove all segments in the inflight buffer that match the given MessageId.
		 */
		uint32 RemoveAllInflightFromMessageId(int32 MessageId)
		{
			int32 BeforeRemove = InflightSegments.Num();
			RemoveInflightIf([MessageId](const FSentData& Data)
			{
				return Data.MessageId == MessageId;
			});
			return BeforeRemove - InflightSegments.Num();
		}

		/**
		 * Make a given MessageId as complete and recompute the desired window size based
		 * on any loss / acks received.
		 */
		void MarkComplete(int32 MessageId, const FDateTime& InCurrentTime)
		{
			uint32 AckSegments = RemoveAllInflightFromMessageId(MessageId);
			uint32 SegmentLoss = RemoveLostSegments(InCurrentTime);
			ComputeWindowSize(AckSegments, SegmentLoss);
		}

		/**
		 * For a given list of segments to acknowledge record how long it took to receive a response and update our
		 * average round trip time. We also use this opportunity to calculate any loss and update our window size.
		 */
		void MarkAcks(int32 MessageId, const TArray<uint32>& Segments, const FDateTime& InCurrentTime)
		{
			TSharedPtr<FUdpMessageSegmenter> Segmenter = MarkAcksOnSegmenter(MessageId, Segments, InCurrentTime);
			uint64 MaxSequenceId = 0;
			FTimespan MaxSpan(0);
			uint32 Acks = 0;
			for (uint32 SegmentId : Segments)
			{
				uint64 Id = Remap(MessageId,SegmentId);
				FSentData SentData;
				if (InflightSegments.RemoveAndCopyValue(Id,SentData))
				{
					MaxSpan = FGenericPlatformMath::Max<FTimespan>(InCurrentTime - SentData.TimeSent, MaxSpan);
					MaxSequenceId = FGenericPlatformMath::Max<uint64>(SentData.SequenceNumber,MaxSequenceId);
					++Acks;
				}
			}
			if (MaxSpan > 0)
			{
				CalculateNewRTT(MaxSpan);
			}

			uint32 SegmentLoss = RemoveLostSegments(InCurrentTime);
			ComputeWindowSize(Acks, SegmentLoss);
		}

		/**
		 * Mark a segment for retransmission because it was deemed lost.
		 */
		void MarkSegmenterSegmentLoss(const FSentData& Data)
		{
			TSharedPtr<FUdpMessageSegmenter> Segmenter = Segmenters.FindRef(Data.MessageId);
			if (Segmenter.IsValid())
			{
				Segmenter->MarkForRetransmission(Data.SegmentNumber);
			}
		}

		/**
		 * Iterate over all segments and discover any potential segments that may be lost.  A lost segment is determined
		 * using 2 times the average round trip time.  If an ack has not been received in that time frame then it is
		 * lost and we must resend it.
		 */
		uint32 RemoveLostSegments(const FDateTime& InCurrentTime)
		{
			uint32 SegmentsLost = 0;

			// We use 2 times the average round trip time to determine if a segment has been lost.
			RemoveInflightIf([&SegmentsLost, InCurrentTime, this](const FSentData& Data)
			{
				const bool bIsLost = ((InCurrentTime - Data.TimeSent) > 2*AvgRoundTripTime) && Data.bIsReliable;
				if (bIsLost)
				{
					MarkSegmenterSegmentLoss(Data);
					SegmentsLost++;
				}
				return bIsLost;
			});

			Statistics.TotalBytesLost += SegmentsLost * UDP_MESSAGING_SEGMENT_SIZE;
			Statistics.PacketsLost += SegmentsLost;
			return SegmentsLost;
		}

		/**
		 * Does this node have any segmenters that can send data for the current time frame.
		 */
		bool HasSegmenterThatCanSend(const FDateTime& InCurrentTime)
		{
			using FSegmenterTuple = TTuple<int32,TSharedPtr<FUdpMessageSegmenter>>;
			for (FSegmenterTuple& Segmenter : Segmenters)
			{
				if (Segmenter.Value->IsInitialized() && Segmenter.Value->NeedSending(InCurrentTime))
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Do we still have room in our outbound window to send data.
		 */
		bool CanSendSegments() const
		{
			return InflightSegments.Num() < WindowSize;
		}

		/** Resets the endpoint info. */
		void ResetIfRestarted(const FGuid& NewNodeId)
		{
			if (NewNodeId != NodeId)
			{
				ReassembledMessages.Reset();

				NodeId = NewNodeId;
			}
		}
	};


	/** Structure for inbound segments. */
	struct FInboundSegment
	{
		/** Holds the segment data. */
		TSharedPtr<FArrayReader, ESPMode::ThreadSafe> Data;

		/** Holds the sender's network endpoint. */
		FIPv4Endpoint Sender;

		/** Default constructor. */
		FInboundSegment() { }

		/** Creates and initializes a new instance. */
		FInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& InData, const FIPv4Endpoint& InSender)
			: Data(InData)
			, Sender(InSender)
		{ }
	};


	/** Structure for outbound messages. */
	struct FOutboundMessage
	{
		/** Holds the serialized message. */
		TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage;

		/** Holds the recipients. */
		TArray<FGuid> RecipientIds;

		EMessageFlags MessageFlags;

		/** Default constructor. */
		FOutboundMessage() { }

		/** Creates and initializes a new instance. */
		FOutboundMessage(TSharedPtr<FUdpSerializedMessage, ESPMode::ThreadSafe> InSerializedMessage, const TArray<FGuid>& InRecipientIds, EMessageFlags InFlags)
			: SerializedMessage(InSerializedMessage)
			, RecipientIds(InRecipientIds)
			, MessageFlags(InFlags)
		{ }
	};

	FUdpMessageSegment::FDataChunk GetDataChunk(FNodeInfo& NodeInfo, TSharedPtr<FUdpMessageSegmenter>& Segmenter, int32 MessageId);

	/** Returns an initialized segment ready for sending or ready to receive ack. */
	TSharedPtr<FUdpMessageSegmenter>* GetInitializedSegmenter(FNodeInfo& NodeInfo, int32 MessageId);

	/** Get a Sent Data structure for the outbound datachunk. */
	FSentData GetSentDataFromInfo(const FSentSegmentInfo &Info, const FUdpMessageSegment::FDataChunk& Chunk);

	/** Send one segment out for the given message id.  Send state is provided by the return value. */
	FSentSegmentInfo SendNextSegmentForMessageId(FNodeInfo& NodeInfo, FUdpMessageSegment::FHeader& Header, int32 MessageId);

	/** Remove any nodes we have not heard from in  while. */
	void RemoveDeadNodes();

	/** Update network statistics table that callers can use to gather info about running connections. */
	void UpdateNetworkStatistics();

	/** Send a segmenter stats to listeners  */
	void SendSegmenterStatsToListeners(int32 MessageId, FGuid NodeId, const TSharedPtr<FUdpMessageSegmenter>& Segmenter);

public:

	/**
	 * Creates and initializes a new message processor.
	 *
	 * @param InSocket The network socket used to transport messages.
	 * @param InNodeId The local node identifier (used to detect the unicast endpoint).
	 * @param InMulticastEndpoint The multicast group endpoint to transport messages to.
	 */
	FUdpMessageProcessor(FSocket& InSocket, const FGuid& InNodeId, const FIPv4Endpoint& InMulticastEndpoint);

	/** Virtual destructor. */
	virtual ~FUdpMessageProcessor();

	/**
	 * Add a static endpoint to the processor
	 * @param InEndpoint the endpoint to add
	 */
	void AddStaticEndpoint(const FIPv4Endpoint& InEndpoint);

	/**
	 * Remove a static endpoint from the processor
	 * @param InEndpoint the endpoint to remove
	 */
	void RemoveStaticEndpoint(const FIPv4Endpoint& InEndpoint);

	/**
	 * Get a list of Nodes Ids split by supported Protocol version
	 *
	 * @param Recipients The list of recipients Ids
	 * @return A map of protocol version -> list of node ids for that protocol
	 */
	TMap<uint8, TArray<FGuid>> GetRecipientsPerProtocolVersion(const TArray<FGuid>& Recipients);

	/**
	 * Queues up an inbound message segment.
	 *
	 * @param Data The segment data.
	 * @param Sender The sender's network endpoint.
	 * @return true if the segment was queued up, false otherwise.
	 */
	bool EnqueueInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& Sender);

	/**
	 * Queues up an outbound message.
	 *
	 * @param MessageContext The message to serialize and send.
	 * @param Recipients The recipients ids to send to.
	 * @return true if the message was queued up, false otherwise.
	 */
	bool EnqueueOutboundMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, const TArray<FGuid>& Recipients);

	/**
	 * Get the event used to signal the message processor that work is available.
	 *
	 * @return The event.
	 */
	const TSharedPtr<FEvent, ESPMode::ThreadSafe>& GetWorkEvent() const
	{
		return WorkEvent;
	}

	/**
	 * Waits for all serialization tasks fired by this processor to complete. Expected to be called when the application exit
	 * to prevent serialized (UStruct) object to being use after the UObject system is shutdown.
	 */
	void WaitAsyncTaskCompletion();

	/**
	 * Get the current running network statistics for the given node.
	 */
	FMessageTransportStatistics GetStats(FGuid Node) const;

public:

	// @todo gmp: remove the need for this typedef
	typedef TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> IMessageAttachmentPtr;

	/**
	 * Returns a delegate that is executed when message data has been received.
	 *
	 * @return The delegate.
	 */
	DECLARE_DELEGATE_ThreeParams(FOnMessageReassembled, const FUdpReassembledMessage& /*ReassembledMessage*/, const IMessageAttachmentPtr& /*Attachment*/, const FGuid& /*NodeId*/)
	FOnMessageReassembled& OnMessageReassembled()
	{
		return MessageReassembledDelegate;
	}

	/**
	 * Returns a delegate that is executed when a remote node has been discovered.
	 *
	 * @return The delegate.
	 * @see OnNodeLost
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeDiscovered, const FGuid& /*NodeId*/)
	FOnNodeDiscovered& OnNodeDiscovered()
	{
		return NodeDiscoveredDelegate;
	}

	/**
	 * Returns a delegate that is executed when a remote node was closed or timed out.
	 *
	 * @return The delegate.
	 * @see OnNodeDiscovered
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeLost, const FGuid& /*NodeId*/)
	FOnNodeLost& OnNodeLost()
	{
		return NodeLostDelegate;
	}

	/**
	 * Returns a delegate that is executed when a socket error happened.
	 *
	 * @return The delegate.
	 * @note this delegate is broadcasted from the processor thread.
	 */
	DECLARE_DELEGATE(FOnError)
	FOnError& OnError()
	{
		return ErrorDelegate;
	}

	/** 
	 * Returns a delegate that is executed when a socket fails to communicate
	 * upon sending to a target endpoint.
	 * @return The delegate
	 * @note this delegate is broadcasted from the processor thread.
	 */
	DECLARE_DELEGATE_TwoParams(FOnErrorSendingToEndpoint, const FGuid& /*NodeId*/, const FIPv4Endpoint& /*SendersIpAddress*/)
	FOnErrorSendingToEndpoint& OnErrorSendingToEndpoint_UdpMessageProcessorThread()
	{
		return ErrorSendingToEndpointDelegate;
	}

	/**
	 * Returns a delegate that is executed when a socket fails to communicate
	 * upon sending to a target endpoint.
	 * @return The delegate
	 * @note this delegate is broadcasted from the processor thread.
	 */
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanAcceptEndpoint, const FGuid& /*NodeId*/, const FIPv4Endpoint& /*SendersIpAddress*/)
	FCanAcceptEndpoint& OnCanAcceptEndpoint_UdpMessageProcessorThread()
	{
		return CanAcceptEndpointDelegate;
	}

	TArray<FIPv4Endpoint> GetKnownEndpoints() const;

public:

	//~ FRunnable interface

	virtual FSingleThreadRunnable* GetSingleThreadInterface() override;
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override { }

protected:

	/**
	 * Acknowledges receipt of a message.
	 *
	 * @param MessageId The identifier of the message to acknowledge.
	 * @param NodeInfo Details for the node to send the acknowledgment to.
	 * @todo gmp: batch multiple of these into a single message
	 */
	void AcknowledgeReceipt(int32 MessageId, const FNodeInfo& NodeInfo);

	/**
	 * Calculates the time span that the thread should wait for work.
	 *
	 * @return Wait time.
	 */
	FTimespan CalculateWaitTime() const;

	/** Consumes all inbound segments. */
	void ConsumeInboundSegments();

	/** Consumes all outbound messages. */
	void ConsumeOutboundMessages();

	/**
	 * Filters the specified message segment.
	 *
	 * @param Header The segment header.
	 * @param Data The segment data.
	 * @param Sender The segment sender.
	 * @return true if the segment passed the filter, false otherwise.
	 */
	bool FilterSegment(const FUdpMessageSegment::FHeader& Header, const FIPv4Endpoint& Sender);

	/**
	 * Processes an Abort segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAbortSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an Acknowledgement segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAcknowledgeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an AcknowledgmentSegments segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessAcknowledgeSegmentsSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Bye segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessByeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Ping segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessDataSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Hello segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessHelloSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Ping segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessPingSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Pong segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	*/
	void ProcessPongSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Retransmit segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessRetransmitSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes a Timeout segment.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 */
	void ProcessTimeoutSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo);

	/**
	 * Processes an unknown segment type.
	 *
	 * @param Segment The segment to process.
	 * @param NodeInfo Details for the node that sent the segment.
	 * @param SegmentType The segment type.
	 */
	void ProcessUnknownSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo, uint8 SegmentType);

	/**
	 * Deliver a reassembled message
	 *
	 * @param ReassembledMessage The message to deliver.
	 * @param NodeInfo Details of the node that sent the message.
	 */
	void DeliverMessage(const TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage, FNodeInfo& NodeInfo);

	/**
	 * Removes the specified node from the list of known remote endpoints.
	 *
	 * @param NodeId The identifier of the node to remove.
	 */
	void RemoveKnownNode(const FGuid& NodeId);


	/** Updates all known remote nodes. */
	void UpdateKnownNodes();

	/**
	 * Updates all segmenters of the specified node.
	 *
	 * @param NodeInfo Details for the node to update.
	 * @return The actual number of bytes written or -1 if error
	 */
	int32 UpdateSegmenters(FNodeInfo& NodeInfo);

	/**
	 * Updates all reassemblers of the specified node.
	 *
	 * @param NodeInfo Details for the node to update.
	 * @return true if the update was successful
	 */
	bool UpdateReassemblers(FNodeInfo& NodeInfo);

	/** Updates nodes per protocol version map */
	void UpdateNodesPerVersion();

protected:

	//~ FSingleThreadRunnable interface

	virtual void Tick() override;

private:

	/** Handles a communication error to a particular endpoint.*/
	void HandleSocketError(const FNodeInfo& NodeInfo) const;

	/** Checks all known nodes to see if any segmenters have NeedSending set to true. */
	bool MoreToSend();

	/** Consume an Outbound Message for processing. A result of true will be returned if it was added for processing. */
	bool ConsumeOneOutboundMessage(const FOutboundMessage& OutboundMessage);

	/** Holds the queue of outbound messages. */
	TQueue<FInboundSegment, EQueueMode::Mpsc> InboundSegments;

	/** Holds the queue of outbound messages. */
	TQueue<FOutboundMessage, EQueueMode::Mpsc> OutboundMessages;

	/** Holds the hello sender. */
	FUdpMessageBeacon* Beacon;

	/** Holds the current time. */
	FDateTime CurrentTime;

	/** Holds the delta time between two ticks. */
	FTimespan DeltaTime;

	/** Holds the protocol version that can be communicated in. */
	TArray<uint8> SupportedProtocolVersions;

	/** Mutex protecting access to the Statistics map. */
	mutable FCriticalSection StatisticsCS;

	/** Map that holds latest statistics for network transmission */
	TMap<FGuid, FMessageTransportStatistics> NodeStats;

	/** Mutex protecting access to the NodeVersions map. */
	mutable FCriticalSection NodeVersionCS;

	/** Holds the protocol version of each nodes separately for safe access (NodeId -> Protocol Version). */
	TMap<FGuid, uint8> NodeVersions;

	/** Holds the collection of known remote nodes. */
	TMap<FGuid, FNodeInfo> KnownNodes;

	/** Holds the local node identifier. */
	FGuid LocalNodeId;

	/** Holds the last sent message number. */
	int32 LastSentMessage;

	/** Holds the multicast endpoint. */
	FIPv4Endpoint MulticastEndpoint;

	/** Holds the network socket used to transport messages. */
	FSocket* Socket;

	/** Holds the socket sender. volatile pointer because used to validate thread shutdown. */
	FUdpSocketSender* volatile SocketSender;

	/** Holds a flag indicating that the thread is stopping. */
	bool bStopping;

	/** Holds a flag indicating if the processor is initialized. */
	bool bIsInitialized;

	/** Holds the thread object. */
	FRunnableThread* Thread;

	/** Holds an event signaling that inbound messages need to be processed. */
	TSharedPtr<FEvent, ESPMode::ThreadSafe> WorkEvent;

	/** Holds a delegate to be invoked when a message was received on the transport channel. */
	FOnMessageReassembled MessageReassembledDelegate;

	/** Holds a delegate to be invoked when a network node was discovered. */
	FOnNodeDiscovered NodeDiscoveredDelegate;

	/** Holds a delegate to be invoked when a network node was lost. */
	FOnNodeLost NodeLostDelegate;

	/** Holds a delegate to be invoked when a socket error happen. */
	FOnError ErrorDelegate;

	/** Holds a delegate to be invoked when a socket error occurs sending to a given endpoint. */
	FOnErrorSendingToEndpoint ErrorSendingToEndpointDelegate;

	/** Holds a delegate to be invoked when checking the validity of a given endpoint address. */
	FCanAcceptEndpoint CanAcceptEndpointDelegate;

	/** The configured message format (from UUdpMessagingSettings). */
	EUdpMessageFormat MessageFormat;

	/** If our round-robin work queues couldn't accept the last outbound message then store a deferred message for next round. */
	TOptional<FOutboundMessage> DeferredOutboundMessage;

	/** Defines the maximum number of Hello segments that can be dropped before a remote endpoint is considered dead. */
	static const int32 DeadHelloIntervals;

	/** Defines a timespan after which non fully reassembled messages that have stopped receiving segments are dropped. */
	static const FTimespan StaleReassemblyInterval;
};
