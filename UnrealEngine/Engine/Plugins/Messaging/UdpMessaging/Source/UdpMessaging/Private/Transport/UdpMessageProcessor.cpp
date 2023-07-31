// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpMessageProcessor.h"
#include "Algo/AllOf.h"
#include "Algo/Transform.h"
#include "HAL/IConsoleManager.h"
#include "INetworkMessagingExtension.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/AssertionMacros.h"
#include "UdpMessagingPrivate.h"

#include "Common/UdpSocketSender.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "IMessageAttachment.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Sockets.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/Class.h"

#include "UdpMessagingTracing.h"
#include "Shared/UdpMessagingSettings.h"
#include "Transport/UdpMessageBeacon.h"
#include "Transport/UdpMessageSegmenter.h"
#include "Transport/UdpReassembledMessage.h"
#include "Transport/UdpSerializedMessage.h"
#include "Transport/UdpSerializeMessageTask.h"


/* FUdpMessageHelloSender static initialization
 *****************************************************************************/

const int32 FUdpMessageProcessor::DeadHelloIntervals = 5;

TAutoConsoleVariable<int32> CVarFakeSocketError(
	TEXT("MessageBus.UDP.InduceSocketError"),
	0,
	TEXT("This CVar can be used to induce a socket failure on outbound communication.\n")
	TEXT("Any non zero value will force the output socket connection to fail if the IP address matches\n")
	TEXT("one of the values in MessageBus.UDP.ConnectionsToError. The list can be cleared by invoking\n")
	TEXT("MessageBus.UDP.ClearDenyList."),
	ECVF_Default
);

TAutoConsoleVariable<FString> CVarConnectionsToError(
	TEXT("MessageBus.UDP.ConnectionsToError"),
	TEXT(""),
	TEXT("Connections to error out on when MessageBus.UDP.InduceSocketError is enabled.\n")
	TEXT("This can be a comma separated list in the form IPAddr2:port,IPAddr3:port"),
	ECVF_Default);

namespace UE::Private::MessageProcessor
{

bool ShouldErrorOnConnection(const FIPv4Endpoint& InEndpoint)
{
	if (CVarFakeSocketError.GetValueOnAnyThread() == 0)
	{
		return false;
	}

	const FString ConnectionsToErrorString = CVarConnectionsToError.GetValueOnAnyThread();
	TArray<FString> EndpointStrings;
	ConnectionsToErrorString.ParseIntoArray(EndpointStrings, TEXT(","));
	for (const FString& EndpointString : EndpointStrings)
	{
		FIPv4Endpoint Endpoint;
		if (FIPv4Endpoint::Parse(EndpointString, Endpoint))
		{
			if (Endpoint.Address == InEndpoint.Address &&
				(Endpoint.Port == 0 || Endpoint.Port == InEndpoint.Port))
			{
				return true;
			}
		}
	}
	return false;
}

FOnOutboundTransferDataUpdated& OnSegmenterUpdated()
{
	static FOnOutboundTransferDataUpdated OnTransferUpdated;
	return OnTransferUpdated;
}

FOnInboundTransferDataUpdated& OnReassemblerUpdated()
{
	static FOnInboundTransferDataUpdated OnTransferUpdated;
	return OnTransferUpdated;
}

uint16 GetMessageProcessorWorkQueueSize()
{
	return GetDefault<UUdpMessagingSettings>()->WorkQueueSize;
}

}
/* FUdpMessageProcessor structors
 *****************************************************************************/
FUdpMessageProcessor::FNodeInfo::FNodeInfo()
	: LastSegmentReceivedTime(FDateTime::MinValue())
	, NodeId()
	, ProtocolVersion(UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION)
	, WorkQueue(UE::Private::MessageProcessor::GetMessageProcessorWorkQueueSize())
{
	ComputeWindowSize(0,0);
}


FUdpMessageProcessor::FUdpMessageProcessor(FSocket& InSocket, const FGuid& InNodeId, const FIPv4Endpoint& InMulticastEndpoint)
	: Beacon(nullptr)
	, LocalNodeId(InNodeId)
	, LastSentMessage(-1)
	, MulticastEndpoint(InMulticastEndpoint)
	, Socket(&InSocket)
	, SocketSender(nullptr)
	, bStopping(false)
	, bIsInitialized(false)
	, MessageFormat(GetDefault<UUdpMessagingSettings>()->MessageFormat) // NOTE: When the message format changes (in the Udp Messaging settings panel), the service is restarted and the processor recreated.
{
	Init();

	WorkEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(), [](FEvent* EventToDelete)
	{
		FPlatformProcess::ReturnSynchEventToPool(EventToDelete);
	});

	CurrentTime = FDateTime::UtcNow();
	Thread = FRunnableThread::Create(this, TEXT("FUdpMessageProcessor"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}


FUdpMessageProcessor::~FUdpMessageProcessor()
{
	// shut down worker thread if it is still running
	Thread->Kill();

	delete Thread;
	Thread = nullptr;

	delete Beacon;
	Beacon = nullptr;

	delete SocketSender;
	SocketSender = nullptr;

	// remove all transport nodes
	if (NodeLostDelegate.IsBound())
	{
		for (auto& KnownNodePair : KnownNodes)
		{
			NodeLostDelegate.Execute(KnownNodePair.Key);
		}
	}

	KnownNodes.Empty();
}


void FUdpMessageProcessor::AddStaticEndpoint(const FIPv4Endpoint& InEndpoint)
{
	if (Beacon)
	{
		Beacon->AddStaticEndpoint(InEndpoint);
	}
}


void FUdpMessageProcessor::RemoveStaticEndpoint(const FIPv4Endpoint& InEndpoint)
{
	if (Beacon)
	{
		Beacon->RemoveStaticEndpoint(InEndpoint);
	}
}

TArray<FIPv4Endpoint> FUdpMessageProcessor::GetKnownEndpoints() const
{
	TArray<FIPv4Endpoint> Endpoints;
	for (const auto& NodePair : KnownNodes)
	{
		Endpoints.Add(NodePair.Value.Endpoint);
	}
	return Endpoints;
}

/* FUdpMessageProcessor interface
 *****************************************************************************/

TMap<uint8, TArray<FGuid>> FUdpMessageProcessor::GetRecipientsPerProtocolVersion(const TArray<FGuid>& Recipients)
{
	TMap<uint8, TArray<FGuid>> NodesPerVersion;
	{
		FScopeLock NodeVersionsLock(&NodeVersionCS);

		// No recipients means a publish, so broadcast to all known nodes (static nodes are in known nodes.)
		// We used to broadcast on the multicast endpoint, but the discovery of nodes should have found available nodes using multicast already
		if (Recipients.Num() == 0)
		{
			for (auto& NodePair : NodeVersions)
			{
				NodesPerVersion.FindOrAdd(NodePair.Value).Add(NodePair.Key);
			}
		}
		else
		{
			for (const FGuid& Recipient : Recipients)
			{
				uint8* Version = NodeVersions.Find(Recipient);
				if (Version)
				{
					NodesPerVersion.FindOrAdd(*Version).Add(Recipient);
				}
			}
		}
	}
	return NodesPerVersion;
}

bool FUdpMessageProcessor::EnqueueInboundSegment(const TSharedPtr<FArrayReader, ESPMode::ThreadSafe>& Data, const FIPv4Endpoint& InSender)
{
	if (bStopping)
	{
		return false;
	}

	if (!InboundSegments.Enqueue(FInboundSegment(Data, InSender)))
	{
		return false;
	}

	WorkEvent->Trigger();

	return true;
}

bool FUdpMessageProcessor::EnqueueOutboundMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, const TArray<FGuid>& Recipients)
{
	if (bStopping)
	{
		return false;
	}

	TMap<uint8, TArray<FGuid>> RecipientPerVersions = GetRecipientsPerProtocolVersion(Recipients);
	for (const auto& RecipientVersion : RecipientPerVersions)
	{
		// Create a message to serialize using that protocol version
		TSharedRef<FUdpSerializedMessage, ESPMode::ThreadSafe> SerializedMessage = MakeShared<FUdpSerializedMessage, ESPMode::ThreadSafe>(MessageFormat, RecipientVersion.Key, MessageContext->GetFlags());

		// Kick off the serialization task
		TGraphTask<FUdpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(MessageContext, SerializedMessage, WorkEvent);

		// Enqueue the message
		if (!OutboundMessages.Enqueue(FOutboundMessage(SerializedMessage, RecipientVersion.Value, MessageContext->GetFlags())))
		{
			return false;
		}
	}

	return true;
}

FMessageTransportStatistics FUdpMessageProcessor::GetStats(FGuid Node) const
{
	FScopeLock NodeVersionLock(&StatisticsCS);
	if (FMessageTransportStatistics const* Stats = NodeStats.Find(Node))
	{
		return *Stats;
	}
	return {};
}

void FUdpMessageProcessor::SendSegmenterStatsToListeners(int32 MessageId, FGuid NodeId, const TSharedPtr<FUdpMessageSegmenter>& Segmenter)
{
	FOnOutboundTransferDataUpdated& SegmenterUpdatedDelegate = UE::Private::MessageProcessor::OnSegmenterUpdated();
	if(!SegmenterUpdatedDelegate.IsBound())
	{
		return;
	}

	if (!Segmenter->IsInitialized() || Segmenter->IsInvalid())
	{
		return;
	}
	uint32 SegmentCount = Segmenter->GetSegmentCount();
	SegmenterUpdatedDelegate.Broadcast(
		{
			NodeId,
			MessageId,
			// Convert segment data into bytes.
			UDP_MESSAGING_SEGMENT_SIZE * SegmentCount,
			UDP_MESSAGING_SEGMENT_SIZE * (SegmentCount - Segmenter->GetPendingSendSegmentsCount()),
			UDP_MESSAGING_SEGMENT_SIZE * (Segmenter->GetAcknowledgedSegmentsCount())
		});
}

void FUdpMessageProcessor::UpdateNetworkStatistics()
{
	FScopeLock NodeVersionLock(&StatisticsCS);
	NodeStats.Reset();
	for (const auto& NodePair : KnownNodes)
	{
		NodeStats.Add(NodePair.Key, NodePair.Value.Statistics);
	}
}

/* FRunnable interface
 *****************************************************************************/

FSingleThreadRunnable* FUdpMessageProcessor::GetSingleThreadInterface()
{
	return this;
}


bool FUdpMessageProcessor::Init()
{
	if (!bIsInitialized)
	{
		Beacon = new FUdpMessageBeacon(Socket, LocalNodeId, MulticastEndpoint);
		SocketSender = new FUdpSocketSender(Socket, TEXT("FUdpMessageProcessor.Sender"));

		// Current protocol version 16
		SupportedProtocolVersions.Add(UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION);
		// Support Protocol version 10, 11, 12, 13, 14, 15
		SupportedProtocolVersions.Add(15);
		SupportedProtocolVersions.Add(14);
		SupportedProtocolVersions.Add(13);
		SupportedProtocolVersions.Add(12);
		SupportedProtocolVersions.Add(11);
		SupportedProtocolVersions.Add(10);
		bIsInitialized = true;
	}
	return bIsInitialized;
}


uint32 FUdpMessageProcessor::Run()
{
	while (!bStopping)
	{
		WorkEvent->Wait(CalculateWaitTime());

		do
		{
			FDateTime LastTime = CurrentTime;
			CurrentTime = FDateTime::UtcNow();
			DeltaTime = CurrentTime - LastTime;

			ConsumeInboundSegments();
			ConsumeOutboundMessages();
			UpdateKnownNodes();
			UpdateNetworkStatistics();
		} while ((!InboundSegments.IsEmpty() || MoreToSend()) && !bStopping);
	}

	return 0;
}


void FUdpMessageProcessor::Stop()
{
	bStopping = true;
	WorkEvent->Trigger();
}


void FUdpMessageProcessor::WaitAsyncTaskCompletion()
{
	// Stop to prevent any new work from being queued.
	Stop();

	// Wait for the processor thread, so we can access KnownNodes safely
	Thread->WaitForCompletion();

	// Check if processor has in-flight serialization task(s).
	auto HasIncompleteSerializationTasks = [this]()
	{
		for (const TPair<FGuid, FNodeInfo>& GuidNodeInfoPair : KnownNodes)
		{
			for (const TPair<int32, TSharedPtr<FUdpMessageSegmenter>>& SegmenterPair: GuidNodeInfoPair.Value.Segmenters)
			{
				if (!SegmenterPair.Value->IsMessageSerializationDone())
				{
					return true;
				}
			}
		}

		return false;
	};

	// Ensures the task graph doesn't contain any pending/running serialization tasks after the processor exit. If the engine is shutting down, the serialization (UStruct) might
	// not be available anymore when the task is run (The task graph shuts down after the UStruct stuff).
	while (HasIncompleteSerializationTasks())
	{
		FPlatformProcess::Sleep(0); // Yield.
	}
}

/* FSingleThreadRunnable interface
*****************************************************************************/

void FUdpMessageProcessor::Tick()
{
	CurrentTime = FDateTime::UtcNow();

	ConsumeInboundSegments();
	ConsumeOutboundMessages();
	UpdateKnownNodes();
	UpdateNetworkStatistics();
}

/* FUdpMessageProcessor implementation
 *****************************************************************************/

void FUdpMessageProcessor::AcknowledgeReceipt(int32 MessageId, const FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FHeader Header;
	{
		Header.ProtocolVersion = NodeInfo.ProtocolVersion;
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.SegmentType = EUdpMessageSegments::Acknowledge;
	}

	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	{
		AcknowledgeChunk.MessageId = MessageId;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		AcknowledgeChunk.Serialize(Writer, NodeInfo.ProtocolVersion);
	}

	int32 OutSent;
	Socket->SendTo(Writer.GetData(), Writer.Num(), OutSent, *NodeInfo.Endpoint.ToInternetAddr());
	UE_LOG(LogUdpMessaging, Verbose, TEXT("Sending EUdpMessageSegments::Acknowledge for msg %d from %s"), MessageId, *NodeInfo.NodeId.ToString());
}


FTimespan FUdpMessageProcessor::CalculateWaitTime() const
{
	return FTimespan::FromMilliseconds(10);
}


FTimespan ComputeMaxWorkTimespan(const int32 DeadHelloIntervals, FUdpMessageBeacon* Beacon)
{
	return 0.25 * DeadHelloIntervals * Beacon->GetBeaconInterval();
}

void FUdpMessageProcessor::ConsumeInboundSegments()
{
	SCOPED_MESSAGING_TRACE(FUdpMessageProcessor_ConsumeInboundSegments);
	FInboundSegment Segment;

	FTimespan MaxWorkTimespan = ComputeMaxWorkTimespan(DeadHelloIntervals, Beacon);
	while (InboundSegments.Dequeue(Segment))
	{
		// quick hack for TTP# 247103
		if (!Segment.Data.IsValid())
		{
			continue;
		}

		FUdpMessageSegment::FHeader Header;
		*Segment.Data << Header;

		if (FilterSegment(Header, Segment.Sender))
		{
			FNodeInfo& NodeInfo = KnownNodes.FindOrAdd(Header.SenderNodeId);

			if (!NodeInfo.NodeId.IsValid())
			{
				NodeInfo.NodeId = Header.SenderNodeId;
				NodeInfo.ProtocolVersion = Header.ProtocolVersion;
				NodeDiscoveredDelegate.ExecuteIfBound(NodeInfo.NodeId);
			}

			NodeInfo.ProtocolVersion = Header.ProtocolVersion;
			NodeInfo.Endpoint = Segment.Sender;
			NodeInfo.LastSegmentReceivedTime = CurrentTime;

			switch (Header.SegmentType)
			{
			case EUdpMessageSegments::Abort:
				ProcessAbortSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Acknowledge:
				ProcessAcknowledgeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::AcknowledgeSegments:
				ProcessAcknowledgeSegmentsSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Bye:
				ProcessByeSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Data:
				ProcessDataSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Hello:
				ProcessHelloSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Ping:
				ProcessPingSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Pong:
				ProcessPongSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Retransmit:
				ProcessRetransmitSegment(Segment, NodeInfo);
				break;

			case EUdpMessageSegments::Timeout:
				ProcessTimeoutSegment(Segment, NodeInfo);
				break;

			default:
				ProcessUnknownSegment(Segment, NodeInfo, (uint8)Header.SegmentType);
			}
		}

		if ((CurrentTime + MaxWorkTimespan) <= FDateTime::UtcNow())
		{
			break;
		}
	}
}

bool FUdpMessageProcessor::ConsumeOneOutboundMessage(const FOutboundMessage& OutboundMessage)
{
	const auto FindNodeWithLog = [this](const FGuid &Id)
	{
		FNodeInfo *NodeInfo = KnownNodes.Find(Id);
		if (NodeInfo == nullptr)
		{
			UE_LOG(LogUdpMessaging, Verbose, TEXT("No recipient NodeInfo found for %s"), *Id.ToString());
		}
		return NodeInfo;
	};

	TArray<FNodeInfo*> Recipients;
	Algo::TransformIf(OutboundMessage.RecipientIds, Recipients,
					  [FindNodeWithLog](const FGuid &Id) {return FindNodeWithLog(Id);},
					  [this](const FGuid &Id) {return KnownNodes.Find(Id);} );

	const bool bCanConsume = Algo::AllOf(Recipients, [this](FNodeInfo *Node)
		{
			return Node->CanCommitToWorkQueue(CurrentTime);
		});

	const bool bIsReliable = EnumHasAnyFlags(OutboundMessage.MessageFlags, EMessageFlags::Reliable);
	if (bCanConsume)
	{
		++LastSentMessage;
		if (LastSentMessage < 0)
		{
			// Prevent negative message ids
			//
			LastSentMessage = 0;
		}
		for (FNodeInfo *RecipientNodeInfo : Recipients)
		{
			UE_LOG(LogUdpMessaging, Verbose, TEXT("Passing %d byte message to be segement-sent to %s with id %s"),
				   OutboundMessage.SerializedMessage->TotalSize(),
				   *RecipientNodeInfo->Endpoint.ToString(),
				   *RecipientNodeInfo->NodeId.ToString());

			if (!bIsReliable && !RecipientNodeInfo->CanSendSegments())
			{
				// Discard unreliable messages that cannot be sent.
				continue;
			}
			TSharedPtr<FUdpMessageSegmenter> Segmenter = MakeShared<FUdpMessageSegmenter>(OutboundMessage.SerializedMessage.ToSharedRef(), UDP_MESSAGING_SEGMENT_SIZE);
			RecipientNodeInfo->Segmenters.Add(
				LastSentMessage, Segmenter);

			RecipientNodeInfo->WorkQueue.Enqueue(LastSentMessage);
			SendSegmenterStatsToListeners(LastSentMessage, RecipientNodeInfo->NodeId, Segmenter);
		}
	}

	// Return true that it was consumed or not reliable. Unreliable messages can be discarded.
	return bCanConsume || !bIsReliable;
}

void FUdpMessageProcessor::ConsumeOutboundMessages()
{
	SCOPED_MESSAGING_TRACE(FUdpMessageProcessor_ConsumeOutputMessages);
	FOutboundMessage OutboundMessage;

	if (DeferredOutboundMessage)
	{
		if (ConsumeOneOutboundMessage(DeferredOutboundMessage.GetValue()))
		{
			DeferredOutboundMessage.Reset();
		}
		else
		{
			return ;
		}
	}

	while (OutboundMessages.Dequeue(OutboundMessage))
	{
		if (!ConsumeOneOutboundMessage(OutboundMessage))
		{
			DeferredOutboundMessage = OutboundMessage;
			return ;
		}
	}
}

bool FUdpMessageProcessor::FilterSegment(const FUdpMessageSegment::FHeader& Header, const FIPv4Endpoint& Sender)
{
	// filter locally generated segments
	if (Header.SenderNodeId == LocalNodeId)
	{
		return false;
	}

	if (!CanAcceptEndpointDelegate.Execute(Header.SenderNodeId, Sender))
	{
		return false;
	}

	// filter unsupported protocol versions
	if (!SupportedProtocolVersions.Contains(Header.ProtocolVersion))
	{
		return false;
	}

	return true;
}


void FUdpMessageProcessor::ProcessAbortSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAbortChunk AbortChunk;
	AbortChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	NodeInfo.Segmenters.Remove(AbortChunk.MessageId);
}


void FUdpMessageProcessor::ProcessAcknowledgeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FAcknowledgeChunk AcknowledgeChunk;
	AcknowledgeChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter>* FoundSegmenter = NodeInfo.Segmenters.Find(AcknowledgeChunk.MessageId);
	if (FoundSegmenter)
	{
		TSharedPtr<FUdpMessageSegmenter>& Segmenter = *FoundSegmenter;
		if (EnumHasAnyFlags(Segmenter->GetMessageFlags(), EMessageFlags::Reliable))
		{
			NodeInfo.MarkComplete(AcknowledgeChunk.MessageId, CurrentTime);
		}
		SendSegmenterStatsToListeners(AcknowledgeChunk.MessageId, NodeInfo.NodeId, Segmenter);
	}

	NodeInfo.Segmenters.Remove(AcknowledgeChunk.MessageId);

	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received Acknowledge for %d from %s"), AcknowledgeChunk.MessageId , *NodeInfo.NodeId.ToString());
}


void FUdpMessageProcessor::ProcessAcknowledgeSegmentsSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo) // TODO: Rename function
{
	FUdpMessageSegment::FAcknowledgeSegmentsChunk AcknowledgeChunk;
	AcknowledgeChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received AcknowledgeSegments for %d from %s"), AcknowledgeChunk.MessageId, *NodeInfo.NodeId.ToString());

	NodeInfo.MarkAcks(AcknowledgeChunk.MessageId, AcknowledgeChunk.Segments, CurrentTime);
}


void FUdpMessageProcessor::ProcessByeSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid() && (RemoteNodeId == NodeInfo.NodeId))
	{
		RemoveKnownNode(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessDataSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FDataChunk DataChunk;
	DataChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	if (Segment.Data->IsError())
	{
		UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpMessageProcessor::ProcessDataSegment: Failed to serialize DataChunk. Sender=%s"),
			*(Segment.Sender.ToString()));
		return;
	}

	TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage = NodeInfo.ReassembledMessages.FindOrAdd(DataChunk.MessageId);

	// Reassemble message
	if (!ReassembledMessage.IsValid())
	{
		ReassembledMessage = MakeShared<FUdpReassembledMessage, ESPMode::ThreadSafe>(NodeInfo.ProtocolVersion, DataChunk.MessageFlags, DataChunk.MessageSize, DataChunk.TotalSegments, DataChunk.Sequence, Segment.Sender);

		if (ReassembledMessage->IsMalformed())
		{
			// Go ahead and throw away the message.
			// The sender should see the NAK and resend, so we'll attempt to recreate it later.
			UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpMessageProcessor::ProcessDataSegment: Ignoring malformed Message %s"), *(ReassembledMessage->Describe()));
			NodeInfo.ReassembledMessages.Remove(DataChunk.MessageId);
			ReassembledMessage.Reset();
			return;
		}
	}

	NodeInfo.Statistics.TotalBytesReceived += DataChunk.Data.Num();
	NodeInfo.Statistics.PacketsReceived++;
	ReassembledMessage->Reassemble(DataChunk.SegmentNumber, DataChunk.SegmentOffset, DataChunk.Data, CurrentTime);
	FOnInboundTransferDataUpdated& ReassemblerUpdated = UE::Private::MessageProcessor::OnReassemblerUpdated();
	if(ReassemblerUpdated.IsBound())
	{
		ReassemblerUpdated.Broadcast(
			{NodeInfo.NodeId,
			 DataChunk.MessageId,
			 UDP_MESSAGING_SEGMENT_SIZE * ReassembledMessage->GetPendingSegmentsCount(),
			 ReassembledMessage->GetReceivedBytes()}
			);
	}

	// Deliver or re-sequence message
	if (!ReassembledMessage->IsComplete() || ReassembledMessage->IsDelivered())
	{
		return;
	}

	UE_LOG(LogUdpMessaging, Verbose, TEXT("FUdpMessageProcessor::ProcessDataSegment: Reassembled %d bytes message %s with %d for %s (%s)"),
		ReassembledMessage->GetData().Num(),
		*ReassembledMessage->Describe(),
		DataChunk.MessageId,
		*NodeInfo.NodeId.ToString(),
		*NodeInfo.Endpoint.ToString());

	AcknowledgeReceipt(DataChunk.MessageId, NodeInfo);
	DeliverMessage(ReassembledMessage, NodeInfo);
}


void FUdpMessageProcessor::ProcessHelloSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
}

void FUdpMessageProcessor::ProcessPingSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;
	uint8 NodeProtocolVersion;
	*Segment.Data << NodeProtocolVersion;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}

	// The protocol version we are going to use to communicate to this node is the smallest between its version and our own
	uint8 ProtocolVersion = FMath::Min<uint8>(NodeProtocolVersion, UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION);

	// if that protocol isn't in our supported protocols we do not reply to the pong and remove this node since we don't support its version
	if (!SupportedProtocolVersions.Contains(ProtocolVersion))
	{
		RemoveKnownNode(NodeInfo.NodeId);
		return;
	}

	// Set this node protocol to our agreed protocol
	NodeInfo.ProtocolVersion = ProtocolVersion;

	// Send the pong
	FUdpMessageSegment::FHeader Header;
	{
		// Reply to the ping using the agreed protocol
		Header.ProtocolVersion = ProtocolVersion;
		Header.RecipientNodeId = NodeInfo.NodeId;
		Header.SenderNodeId = LocalNodeId;
		Header.SegmentType = EUdpMessageSegments::Pong;
	}

	FArrayWriter Writer;
	{
		Writer << Header;
		Writer << LocalNodeId;
	}

	int32 OutSent;
	Socket->SendTo(Writer.GetData(), Writer.Num(), OutSent, *NodeInfo.Endpoint.ToInternetAddr());
}


void FUdpMessageProcessor::ProcessPongSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FGuid RemoteNodeId;
	*Segment.Data << RemoteNodeId;

	if (RemoteNodeId.IsValid())
	{
		NodeInfo.ResetIfRestarted(RemoteNodeId);
	}
}


void FUdpMessageProcessor::ProcessRetransmitSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FRetransmitChunk RetransmitChunk;
	RetransmitChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(RetransmitChunk.MessageId);

	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received retransmit for %d from %s"), RetransmitChunk.MessageId, *NodeInfo.NodeId.ToString());

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission(RetransmitChunk.Segments);
	}
	else
	{
		UE_LOG(LogUdpMessaging, Verbose, TEXT("No such segmenter for message %d"), RetransmitChunk.MessageId);
	}
}


void FUdpMessageProcessor::ProcessTimeoutSegment(FInboundSegment& Segment, FNodeInfo& NodeInfo)
{
	FUdpMessageSegment::FTimeoutChunk TimeoutChunk;
	TimeoutChunk.Serialize(*Segment.Data, NodeInfo.ProtocolVersion);

	TSharedPtr<FUdpMessageSegmenter> Segmenter = NodeInfo.Segmenters.FindRef(TimeoutChunk.MessageId);

	if (Segmenter.IsValid())
	{
		Segmenter->MarkForRetransmission();
	}
}


void FUdpMessageProcessor::ProcessUnknownSegment(FInboundSegment& Segment, FNodeInfo& EndpointInfo, uint8 SegmentType)
{
	UE_LOG(LogUdpMessaging, Verbose, TEXT("Received unknown segment type '%i' from %s"), SegmentType, *Segment.Sender.ToText().ToString());
}


void FUdpMessageProcessor::DeliverMessage(const TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage, FNodeInfo& NodeInfo)
{
	// Do not deliver message while saving or garbage collecting since those deliveries will fail anyway...
	if (GIsSavingPackage || IsGarbageCollecting())
	{
		UE_LOG(LogUdpMessaging, Verbose, TEXT("Skipping delivery of %s"), *ReassembledMessage->Describe());
		return;
	}

	if (NodeInfo.NodeId.IsValid())
	{
		MessageReassembledDelegate.ExecuteIfBound(*ReassembledMessage, nullptr, NodeInfo.NodeId);
	}

	// Mark the message delivered but do not remove it from the list yet, this is to prevent the double delivery of reliable message
	ReassembledMessage->MarkDelivered();
}


void FUdpMessageProcessor::RemoveKnownNode(const FGuid& NodeId)
{
	NodeLostDelegate.ExecuteIfBound(NodeId);
	KnownNodes.Remove(NodeId);
}

int32 GetMaxSendRate()
{
	const uint32 OneGbitPerSecondInBytes = 125000000;
	return static_cast< uint32 > ( GetDefault<UUdpMessagingSettings>()->MaxSendRate * OneGbitPerSecondInBytes );
}

bool HasTimedOut(const FDateTime& ReceivedTime, const FDateTime& CurrentTime)
{
	const int32 TimeoutInSeconds = 5;
	FTimespan TimeoutInterval = TimeoutInSeconds * FTimespan::FromMilliseconds(1000);
	return (ReceivedTime + TimeoutInterval) < CurrentTime;
}

void FUdpMessageProcessor::RemoveDeadNodes()
{
	// Remove dead nodes
	FTimespan DeadHelloTimespan = DeadHelloIntervals * Beacon->GetBeaconInterval();
	for (auto It = KnownNodes.CreateIterator(); It; ++It)
	{
		FGuid& NodeId = It->Key;
		FNodeInfo& NodeInfo = It->Value;

		if ((NodeId.IsValid()) && HasTimedOut(NodeInfo.LastSegmentReceivedTime,CurrentTime))
		{
			UE_LOG(LogUdpMessaging, Verbose, TEXT("FUdpMessageProcessor::UpdateKnownNodes: Removing Node %s (%s)"), *NodeInfo.NodeId.ToString(), *NodeInfo.Endpoint.ToString());
			NodeLostDelegate.ExecuteIfBound(NodeId);
			It.RemoveCurrent();
		}
	}
}

bool FUdpMessageProcessor::MoreToSend()
{
	for (auto& KnownNodePair : KnownNodes)
	{
		if (KnownNodePair.Value.CanSendSegments() && KnownNodePair.Value.HasSegmenterThatCanSend(CurrentTime))
		{
			return true;
		}
	}
	return false;
}

void FUdpMessageProcessor::HandleSocketError(const FNodeInfo& NodeInfo) const
{
	UE_LOG(LogUdpMessaging, Error,
		   TEXT("Socket error detected when communicating with %s. Banning communication to that endpoint."), *NodeInfo.Endpoint.ToString());
	ErrorSendingToEndpointDelegate.Execute(NodeInfo.NodeId, NodeInfo.Endpoint);
}

void FUdpMessageProcessor::UpdateKnownNodes()
{
	SCOPED_MESSAGING_TRACE(FUdpMessageProcessor_UpdateKnownNodes);

	RemoveDeadNodes();

	UpdateNodesPerVersion();
	Beacon->SetEndpointCount(KnownNodes.Num() + 1);

	bool bSuccess = true;
	for (auto& KnownNodePair : KnownNodes)
	{
		int32 NodeByteSent = UpdateSegmenters(KnownNodePair.Value);
		// if NodByteSent is negative, there is a socket error, continuing is useless
		bSuccess = NodeByteSent >= 0;
		if (!bSuccess || UE::Private::MessageProcessor::ShouldErrorOnConnection(KnownNodePair.Value.Endpoint))
		{
			bSuccess = false; // To ensure we trigger the delegate on a forced error.
			HandleSocketError(KnownNodePair.Value);
			break;
		}

		bSuccess = UpdateReassemblers(KnownNodePair.Value);
		// if there is a socket error, continuing is useless
		if (!bSuccess)
		{
			HandleSocketError(KnownNodePair.Value);
			break;
		}

		KnownNodePair.Value.RemoveOldUnreliables(CurrentTime);
	}

	// if we had socket error, fire up the error delegate
	if (!bSuccess || Beacon->HasSocketError())
	{
		bStopping = true;
		ErrorDelegate.ExecuteIfBound();
	}
}

TSharedPtr<FUdpMessageSegmenter>* FUdpMessageProcessor::GetInitializedSegmenter(FNodeInfo& NodeInfo, int32 MessageId)
{
	TSharedPtr<FUdpMessageSegmenter>* FoundSegmenter = NodeInfo.Segmenters.Find(MessageId);
	if (!FoundSegmenter)
	{
		// It's possible that a segmenter was fully ack and removed from the segmenter list.
		return nullptr;
	}

	TSharedPtr<FUdpMessageSegmenter>& Segmenter = *FoundSegmenter;
	if (!Segmenter->IsInitialized())
	{
		Segmenter->Initialize();
	}

	if (Segmenter->IsInvalid())
	{
		UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpMessageProcessor::GetInitializedSegments segmenter is invalid. Removing from send queue."));
		NodeInfo.Segmenters.Remove(MessageId);
		return nullptr;
	}

	return  FoundSegmenter;
}

FUdpMessageSegment::FDataChunk FUdpMessageProcessor::GetDataChunk(FNodeInfo& NodeInfo, TSharedPtr<FUdpMessageSegmenter>& Segmenter, int32 MessageId)
{
	FUdpMessageSegment::FDataChunk DataChunk;

	// Track the segments we sent as we'll update the segmenter to keep track
	TConstSetBitIterator<> BIt(Segmenter->GetPendingSendSegments());
	Segmenter->GetPendingSegment(BIt.GetIndex(), DataChunk.Data);
	DataChunk.SegmentNumber = BIt.GetIndex();

	DataChunk.MessageId = MessageId;
	DataChunk.MessageFlags = Segmenter->GetMessageFlags();
	DataChunk.MessageSize = Segmenter->GetMessageSize();
	DataChunk.SegmentOffset = UDP_MESSAGING_SEGMENT_SIZE * DataChunk.SegmentNumber;
	DataChunk.Sequence = 0; // This should be kept 0 for legacy support.
	DataChunk.TotalSegments = Segmenter->GetSegmentCount();
	return DataChunk;
}

FSentSegmentInfo FUdpMessageProcessor::SendNextSegmentForMessageId(FNodeInfo& NodeInfo, FUdpMessageSegment::FHeader& Header, int32 MessageId)
{
	TSharedPtr<FUdpMessageSegmenter>* FoundSegmenter = GetInitializedSegmenter(NodeInfo, MessageId);
	if (!FoundSegmenter)
	{
		// It's possible that a segmenter was fully ack and removed from the segmenter list.
		return {};
	}

	TSharedPtr<FUdpMessageSegmenter>& Segmenter = *FoundSegmenter;

	FSentSegmentInfo SentInfo;
	if (Segmenter->IsInitialized() && Segmenter->NeedSending(CurrentTime))
	{
		FUdpMessageSegment::FDataChunk DataChunk = GetDataChunk(NodeInfo, Segmenter, MessageId);

		// validate with are sending message in the proper protocol version
		check(Header.ProtocolVersion == Segmenter->GetProtocolVersion());

		TSharedRef<FArrayWriter, ESPMode::ThreadSafe> Writer = MakeShared<FArrayWriter, ESPMode::ThreadSafe>();
		{
			*Writer << Header;
			DataChunk.Serialize(*Writer, Header.ProtocolVersion);
		}

		UE_LOG(LogUdpMessaging, Verbose, TEXT("FUdpMessageProcessor::UpdateSegmenters: Sending msg %d as segment %d/%d of %d bytes to %s"),
			   DataChunk.MessageId,
			   DataChunk.SegmentNumber + 1,
			   DataChunk.TotalSegments,
			   Segmenter->GetMessageSize(),
			   *NodeInfo.NodeId.ToString());

		if (!SocketSender->Send(Writer, NodeInfo.Endpoint))
		{
			SentInfo.bSendSocketError = true;
			return MoveTemp(SentInfo);
		}

		Segmenter->MarkAsSent(DataChunk.SegmentNumber);

		NodeInfo.Statistics.PacketsSent++;
		SentInfo.BytesSent += Writer->Num();
		SentInfo.SequenceNumber = ++NodeInfo.SequenceId;
		SentInfo.bIsReliable = EnumHasAnyFlags(Segmenter->GetMessageFlags(), EMessageFlags::Reliable);
		SentInfo.bRequiresRequeue = !Segmenter->IsSendingComplete() || SentInfo.bIsReliable;

		Segmenter->UpdateSentTime(CurrentTime);

		NodeInfo.InflightSegments.Add(
			NodeInfo.Remap(MessageId,DataChunk.SegmentNumber),
			SentInfo.AsSentData(MessageId,DataChunk.SegmentNumber,CurrentTime));

		SendSegmenterStatsToListeners(MessageId, NodeInfo.NodeId, Segmenter);

		if (Segmenter->IsSendingComplete() && !SentInfo.bIsReliable)
		{
			// Not reliably sent so we don't need to wait for acks.
			UE_LOG(LogUdpMessaging, VeryVerbose, TEXT("FUdpMessageProcessor::UpdateSegmenters: Finished with message segmenter for %s"), *NodeInfo.NodeId.ToString());
			NodeInfo.Segmenters.Remove(MessageId);
		}
	}
	else
	{
		SentInfo.bRequiresRequeue = true;
	}

	NodeInfo.Statistics.TotalBytesSent += SentInfo.BytesSent;
	NodeInfo.Statistics.IPv4AsString = NodeInfo.Endpoint.ToString();
	return MoveTemp(SentInfo);
}

int32 FUdpMessageProcessor::UpdateSegmenters(FNodeInfo& NodeInfo)
{
	SCOPED_MESSAGING_TRACE(FUdpMessageProcessor_UpdateSegmenters);
	FUdpMessageSegment::FHeader Header
	{
		NodeInfo.ProtocolVersion,		// Header.ProtocolVersion - Send data segment using the node protocol version
		NodeInfo.NodeId,				// Header.RecipientNodeId
		LocalNodeId,					// Header.SenderNodeId
		EUdpMessageSegments::Data		// Header.SegmentType
	};


	int32 MessageId;
	int32 BytesSent = 0;
	TArray<int32,TInlineAllocator<64>> MessagesForRequeue;
	while (NodeInfo.CanSendSegments() && NodeInfo.WorkQueue.Dequeue(MessageId))
	{
		FSentSegmentInfo Info = SendNextSegmentForMessageId(NodeInfo, Header, MessageId);
		if (Info.bSendSocketError)
		{
			return -1;
		}

		if (Info.bRequiresRequeue)
		{
			MessagesForRequeue.Add(MessageId);
		}

		BytesSent += Info.BytesSent;
	}
	
	NodeInfo.Statistics.PacketsInFlight = NodeInfo.InflightSegments.Num();
	NodeInfo.Statistics.BytesInflight = NodeInfo.InflightSegments.Num() * UDP_MESSAGING_SEGMENT_SIZE;

	// Requeue for the next round.
	for (int32 RequeueMessageId : MessagesForRequeue)
	{
		NodeInfo.WorkQueue.Enqueue(RequeueMessageId);
	}
	return BytesSent;
}



const FTimespan FUdpMessageProcessor::StaleReassemblyInterval = FTimespan::FromSeconds(30);

bool FUdpMessageProcessor::UpdateReassemblers(FNodeInfo& NodeInfo)
{
	SCOPED_MESSAGING_TRACE(FUdpMessageProcessor_UpdateReassemblers);
	FUdpMessageSegment::FHeader Header
	{
		FMath::Max(NodeInfo.ProtocolVersion, (uint8)11),	// Header.ProtocolVersion, AcknowledgeSegments are version 11 and onward segment
		NodeInfo.NodeId,									// Header.RecipientNodeId
		LocalNodeId,										// Header.SenderNodeId
		EUdpMessageSegments::AcknowledgeSegments			// Header.SegmentType
	};

	const uint32 MaxSendRate = GetMaxSendRate();
	double DeltaSeconds = DeltaTime.GetTotalSeconds();
	const int32 MaxSendRateDelta = MaxSendRate * DeltaSeconds;
	int32 MaxNodeByteSend = (MaxSendRateDelta) / KnownNodes.Num();

	for (TMap<int32, TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>>::TIterator It(NodeInfo.ReassembledMessages); It; ++It)
	{
		TSharedPtr<FUdpReassembledMessage, ESPMode::ThreadSafe>& ReassembledMessage = It.Value();

		int BytesSent = 0;
		while (ReassembledMessage->HasPendingAcknowledgements() && BytesSent < MaxNodeByteSend)
		{
			TArray<uint32> PendingAcknowledgments = ReassembledMessage->GetPendingAcknowledgments();
			const int32 AckCount = PendingAcknowledgments.Num();

			FUdpMessageSegment::FAcknowledgeSegmentsChunk AcknowledgeChunk(It.Key()/*MessageId*/, MoveTemp(PendingAcknowledgments)/*Segments*/);
			TSharedRef<FArrayWriter, ESPMode::ThreadSafe> Writer = MakeShared<FArrayWriter, ESPMode::ThreadSafe>();
			{
				*Writer << Header;
				AcknowledgeChunk.Serialize(*Writer, Header.ProtocolVersion);
			}
			BytesSent += Writer->Num();

			UE_LOG(LogUdpMessaging, Verbose, TEXT("FUdpMessageProcessor::UpdateReassemblers: Sending EUdpMessageSegments::AcknowledgeSegments for %d segments for message %d from %s"), AckCount, It.Key(), *ReassembledMessage->Describe());

			if (!SocketSender->Send(Writer, NodeInfo.Endpoint))
			{
				UE_LOG(LogUdpMessaging, Error, TEXT("FUdpMessageProcessor::UpdateReassemblers: error sending EUdpMessageSegments::AcknowledgeSegments from %s"), *NodeInfo.NodeId.ToString());

				return false;
			}

			UE_LOG(LogUdpMessaging, Verbose, TEXT("FUdpMessageProcessor::UpdateReassemblers: sending acknowledgement for reliable msg %d from %s"),
				It.Key(),
				*NodeInfo.NodeId.ToString());
		}

		// Try to deliver completed message that couldn't be delivered the first time around
		if (ReassembledMessage->IsComplete() && !ReassembledMessage->IsDelivered())
		{
			DeliverMessage(ReassembledMessage, NodeInfo);
		}

		// Remove stale reassembled message if they aren't reliable or are marked delivered
		if (ReassembledMessage->GetLastSegmentTime() + StaleReassemblyInterval <= CurrentTime &&
			(!EnumHasAnyFlags(ReassembledMessage->GetFlags(), EMessageFlags::Reliable) || ReassembledMessage->IsDelivered()))
		{
			if (!ReassembledMessage->IsDelivered())
			{
				const int ReceivedSegments = ReassembledMessage->GetTotalSegmentsCount() - ReassembledMessage->GetPendingSegmentsCount();
				UE_LOG(LogUdpMessaging, Warning, TEXT("FUdpMessageProcessor::UpdateReassemblers Discarding %d/%d of stale message segements from %s"),
					ReceivedSegments,
					ReassembledMessage->GetTotalSegmentsCount(),
					*ReassembledMessage->Describe());
			}
			It.RemoveCurrent();
		}
	}
	return true;
}


void FUdpMessageProcessor::UpdateNodesPerVersion()
{
	FScopeLock NodeVersionLock(&NodeVersionCS);
	NodeVersions.Empty();
	for (auto& NodePair : KnownNodes)
	{
		NodeVersions.Add(NodePair.Key, NodePair.Value.ProtocolVersion);
	}
}
