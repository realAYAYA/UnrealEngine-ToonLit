// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationConnections.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/ReplicationSystem/ReplicationReader.h"
#include "Iris/PacketControl/PacketNotification.h"
#include "Net/Core/Trace/NetTrace.h"

namespace UE::Net
{

// FDataStreamTestUtil implementation
FDataStreamTestUtil::FDataStreamTestUtil()
: DataStreamDefinitions(nullptr)
, CurrentDataStreamDefinitions(nullptr)
{
}

void FDataStreamTestUtil::SetUp()
{
	StoreDataStreamDefinitions();
}

void FDataStreamTestUtil::TearDown()
{
	RestoreDataStreamDefinitions();
}

void FDataStreamTestUtil::StoreDataStreamDefinitions()
{
	DataStreamDefinitions = static_cast<UMyDataStreamDefinitions*>(GetMutableDefault<UDataStreamDefinitions>());
	check(DataStreamDefinitions != nullptr);
	CurrentDataStreamDefinitions = &DataStreamDefinitions->ReadWriteDataStreamDefinitions();
	PointerToFixupComplete = &DataStreamDefinitions->ReadWriteFixupComplete();

	PreviousDataStreamDefinitions.Empty();
	Swap(*CurrentDataStreamDefinitions, PreviousDataStreamDefinitions);
	*PointerToFixupComplete = false;
}

void FDataStreamTestUtil::RestoreDataStreamDefinitions()
{
	Swap(*CurrentDataStreamDefinitions, PreviousDataStreamDefinitions);
	*PointerToFixupComplete = false;
}

void FDataStreamTestUtil::AddDataStreamDefinition(const TCHAR* StreamName, const TCHAR* ClassPath, bool bValid)
{
	FDataStreamDefinition Definition = {};

	Definition.DataStreamName = FName(StreamName);
	Definition.ClassName = bValid ? FName(ClassPath): FName();
	Definition.Class = nullptr;
	Definition.DefaultSendStatus = EDataStreamSendStatus::Send;
	Definition.bAutoCreate = false;

	CurrentDataStreamDefinitions->Add(Definition);
}

// FReplicationSystemTestNode implementation
FReplicationSystemTestNode::FReplicationSystemTestNode(bool bIsServer, const TCHAR* Name)
{
	ReplicationBridge = NewObject<UReplicatedTestObjectBridge>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(ReplicationBridge));

	UReplicationSystem::FReplicationSystemParams Params;
	Params.ReplicationBridge = ReplicationBridge;
	Params.bIsServer = bIsServer;
	Params.bAllowObjectReplication = bIsServer;

	auto IrisLogVerbosity = UE_GET_LOG_VERBOSITY(LogIris);
	LogIris.SetVerbosity(ELogVerbosity::Error);
	ReplicationSystem = FReplicationSystemFactory::CreateReplicationSystem(Params);	
	if (!bIsServer)
	{
		ReplicationBridge->SetCreatedObjectsOnNode(&CreatedObjects);
	}
	LogIris.SetVerbosity(IrisLogVerbosity);
	check(ReplicationBridge != nullptr);

	UE_NET_TRACE_UPDATE_INSTANCE(GetNetTraceId(), bIsServer, Name);
}

FReplicationSystemTestNode::~FReplicationSystemTestNode()
{
	const uint32 NetTraceId = ReplicationSystem->GetId();
	FReplicationSystemFactory::DestroyReplicationSystem(ReplicationSystem);
	CreatedObjects.Empty();

	// End NetTrace session for this instance
	UE_NET_TRACE_END_SESSION(NetTraceId);
}

uint32 FReplicationSystemTestNode::GetNetTraceId() const
{ 
	return ReplicationSystem ? ReplicationSystem->GetId() : ~0U;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateObject(const UObjectReplicationBridge::FCreateNetRefHandleParams& Params)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(CreatedObject, Params);

	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateObject(uint32 NumComponents, uint32 NumIrisComponents)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
	CreatedObject->AddComponents(NumComponents, NumIrisComponents);

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(CreatedObject);
	
	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateObject(const UTestReplicatedIrisObject::FComponents& Components)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObject->AddComponents(Components);

	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(CreatedObject);
	
	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateSubObject(FNetRefHandle Owner, const UTestReplicatedIrisObject::FComponents& Components)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
	CreatedObject->AddComponents(Components);

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(Owner, CreatedObject);
	
	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateSubObject(FNetRefHandle Owner, uint32 NumComponents, uint32 NumIrisComponents)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
	CreatedObject->AddComponents(NumComponents, NumIrisComponents);

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(Owner, CreatedObject);
	
	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateObjectWithDynamicState(uint32 NumComponents, uint32 NumIrisComponents, uint32 NumDynamicStateComponents)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

	CreatedObject->AddComponents(NumComponents, NumIrisComponents);
	CreatedObject->AddDynamicStateComponents(NumDynamicStateComponents);
	
	return CreatedObject;
}

void FReplicationSystemTestNode::DestroyObject(UReplicatedTestObject* Object, EEndReplicationFlags EndReplicationFlags)
{
	// Destroy handle
	check(Object && Object->NetRefHandle.IsValid());

	// End replication for the handle
	ReplicationBridge->EndReplication(Object, EndReplicationFlags);

	// Release ref
	CreatedObjects.Remove(TStrongObjectPtr<UObject>(Object));

	// Mark as garbage
	Object->MarkAsGarbage();
}

uint32 FReplicationSystemTestNode::AddConnection()
{
	check(ReplicationSystem);

	FConnectionInfo Connection;
	Connection.ConnectionId = Connections.Num() + 1U;

	// Create DataStreams
	FDataStreamManagerInitParams InitParams;
	InitParams.PacketWindowSize = 256;
	UDataStreamManager* DataStreamManager = NewObject<UDataStreamManager>();
	DataStreamManager->Init(InitParams);
	Connection.DataStreamManager = DataStreamManager;
	CreatedObjects.Add(TStrongObjectPtr<UObject>(Connection.DataStreamManager));

	// Streams created based on config
	Connection.DataStreamManager->CreateStream("NetToken");
	Connection.NetTokenDataStream = StaticCast<UNetTokenDataStream*>(Connection.DataStreamManager->GetStream("NetToken"));
	Connection.DataStreamManager->CreateStream("Replication");
	Connection.ReplicationDataStream = StaticCast<UReplicationDataStream*>(Connection.DataStreamManager->GetStream("Replication"));

	UE_NET_TRACE_CONNECTION_CREATED(GetNetTraceId(), Connection.ConnectionId);
	UE_NET_TRACE_CONNECTION_STATE_UPDATED(GetNetTraceId(), Connection.ConnectionId, static_cast<uint8>(3));
	
	// Add a connection
	ReplicationSystem->AddConnection(Connection.ConnectionId);

	// Initialize Streams
	ReplicationSystem->InitDataStreams(Connection.ConnectionId, Connection.DataStreamManager);
	ReplicationSystem->SetReplicationEnabledForConnection(Connection.ConnectionId, true);

	// Store RemoteNetTokenStoreState
	Connection.RemoteNetTokenStoreState = &ReplicationSystem->GetReplicationSystemInternal()->GetConnections().GetRemoteNetTokenStoreState(Connection.ConnectionId);

	// Add view
	FReplicationView View;
	View.Views.AddDefaulted();

	ReplicationSystem->SetReplicationView(Connection.ConnectionId, View);

	Connections.Add(Connection);

	return Connection.ConnectionId;
}

void FReplicationSystemTestNode::PreSendUpdate()
{
	ReplicationSystem->PreSendUpdate(1.f);
}

bool FReplicationSystemTestNode::SendUpdate(uint32 ConnectionId, const TCHAR* Desc)
{
	FPacketData Packet;

	FNetBitStreamWriter Writer;
	Writer.InitBytes(Packet.PacketBuffer, FMath::Min<uint32>(FPacketData::MaxPacketSize, MaxSendPacketSize));

	FNetSerializationContext Context(&Writer);

	Context.SetTraceCollector(UE_NET_TRACE_CREATE_COLLECTOR(ENetTraceVerbosity::Trace));

	FConnectionInfo& Connection = GetConnectionInfo(ConnectionId);

	const FDataStreamRecord* Record = nullptr;

	const bool bResult = (Connection.DataStreamManager->BeginWrite() != UDataStream::EWriteResult::NoData) && (Connection.DataStreamManager->WriteData(Context, Record)  != UDataStream::EWriteResult::NoData);
	if (bResult)
	{
		Writer.CommitWrites();
		Packet.BitCount = Writer.GetPosBits();
		Packet.PacketId = PacketId++;
		if (Desc)
		{
			Packet.Desc = FString(Desc);
		}

		Connection.WriteRecords.Enqueue(Record);
		Connection.WrittenPackets.Enqueue(Packet);
	}

	Connection.DataStreamManager->EndWrite();

	if (bResult)
	{
		UE_NET_TRACE_FLUSH_COLLECTOR(Context.GetTraceCollector(), GetNetTraceId(), Connection.ConnectionId, ENetTracePacketType::Outgoing);
		UE_NET_TRACE_PACKET_SEND(GetNetTraceId(), Connection.ConnectionId, Packet.PacketId, Packet.BitCount);
		UE_LOG(LogIris, Log, TEXT("ReplicationSystemTestFixture: Conn: %u Send PacketId: %u %s"), ConnectionId, Packet.PacketId, *Packet.Desc);
	}

	UE_NET_TRACE_DESTROY_COLLECTOR(Context.GetTraceCollector());

	return bResult;
}

void FReplicationSystemTestNode::PostSendUpdate()
{
	ReplicationSystem->PostSendUpdate();
}

void FReplicationSystemTestNode::DeliverTo(FReplicationSystemTestNode& Dest, uint32 LocalConnectionId, uint32 RemoteConnectionId, bool bDeliver)
{
	FConnectionInfo& Connection = GetConnectionInfo(LocalConnectionId);
	const FPacketData& Packet = Connection.WrittenPackets.Peek();

	if (bDeliver)
	{
		FNetBitStreamReader Reader;
		Reader.InitBits(Packet.PacketBuffer, Packet.BitCount);
		FNetSerializationContext Context(&Reader);

		Context.SetTraceCollector(UE_NET_TRACE_CREATE_COLLECTOR(ENetTraceVerbosity::Trace));

		UE_LOG(LogIris, Log, TEXT("ReplicationSystemTestFixture: Conn: %u Deliver PacketId: %u %s"), LocalConnectionId, Packet.PacketId, *Packet.Desc);
		Dest.RecvUpdate(RemoteConnectionId, Context);

		UE_NET_TRACE_FLUSH_COLLECTOR(Context.GetTraceCollector(), Dest.GetNetTraceId(), RemoteConnectionId, ENetTracePacketType::Incoming);
		UE_NET_TRACE_DESTROY_COLLECTOR(Context.GetTraceCollector());
		UE_NET_TRACE_PACKET_RECV(Dest.GetNetTraceId(), RemoteConnectionId, Packet.PacketId, Packet.BitCount);
	}
	else
	{
		UE_LOG(LogIris, Log, TEXT("ReplicationSystemTestFixture: Conn: %u Dropped PacketId: %u %s"), LocalConnectionId, Packet.PacketId, *Packet.Desc);
		UE_NET_TRACE_PACKET_DROPPED(Dest.GetNetTraceId(), RemoteConnectionId, Packet.PacketId, ENetTracePacketType::Incoming);
		UE_NET_TRACE_PACKET_DROPPED(GetNetTraceId(), LocalConnectionId, Packet.PacketId, ENetTracePacketType::Outgoing);
	}

	// If this triggers an assert, ensure that SendTo() actually wrote any packets before.
	Connection.DataStreamManager->ProcessPacketDeliveryStatus(bDeliver ? EPacketDeliveryStatus::Delivered : EPacketDeliveryStatus::Lost, Connection.WriteRecords.Peek());
	Connection.WriteRecords.Pop();
	Connection.WrittenPackets.Pop();
}

void FReplicationSystemTestNode::RecvUpdate(uint32 ConnectionId, FNetSerializationContext& Context)
{
	FConnectionInfo& Connection = GetConnectionInfo(ConnectionId);

	Connection.DataStreamManager->ReadData(Context);

	check(!Context.HasErrorOrOverflow());
	check(Context.GetBitStreamReader()->GetBitsLeft() == 0U);
}

uint32 FReplicationSystemTestNode::GetReplicationSystemId() const
{
	return ReplicationSystem ? ReplicationSystem->GetId() : uint32(~0U);
}

float FReplicationSystemTestNode::ConvertPollPeriodIntoFrequency(uint32 PollPeriod) const
{
	const float PollFrequency = ReplicationBridge->GetMaxTickRate() / (float)PollPeriod;
	return PollFrequency;
}

// FReplicationSystemTestClient implementation
FReplicationSystemTestClient::FReplicationSystemTestClient(const TCHAR* Name)
: FReplicationSystemTestNode(false, Name)
, ConnectionIdOnServer(~0U)
{
}

// FReplicationSystemTestServer implementation
FReplicationSystemTestServer::FReplicationSystemTestServer(const TCHAR* Name)
: FReplicationSystemTestNode(true, Name)
{
}

bool FReplicationSystemTestServer::SendAndDeliverTo(FReplicationSystemTestClient* Client, bool bDeliver, const TCHAR* Desc)
{
	if (SendUpdate(Client->ConnectionIdOnServer, Desc))
	{
		DeliverTo(Client, bDeliver);

		return true;
	}

	return false;
}

// Send data, return true if data was written
bool FReplicationSystemTestServer::SendTo(FReplicationSystemTestClient* Client, const TCHAR* Desc)
{
	return SendUpdate(Client->ConnectionIdOnServer, Desc);
}

// If bDeliver is true deliver data to client and report packet as delivered
// if bDeliver is false, do not deliver packet and report a dropped packet
void FReplicationSystemTestServer::DeliverTo(FReplicationSystemTestClient* Client, bool bDeliver)
{
	FReplicationSystemTestNode::DeliverTo(*Client, Client->ConnectionIdOnServer, Client->LocalConnectionId, bDeliver);
}

bool FReplicationSystemTestServer::UpdateAndSend(const TArray<FReplicationSystemTestClient*>& Clients, bool bDeliver /*= true*/)
{
	bool bSuccess = true;

	PreSendUpdate();

	for (FReplicationSystemTestClient* Client : Clients)
	{
		bSuccess &= SendAndDeliverTo(Client, bDeliver);
	}
	
	PostSendUpdate();

	return bSuccess;
}

// FReplicationSystemServerClientTestFixture implementation
void FReplicationSystemServerClientTestFixture::SetUp()
{
	FNetworkAutomationTestSuiteFixture::SetUp();

	// Fake what we normally get from config
	DataStreamUtil.SetUp();
	DataStreamUtil.AddDataStreamDefinition(TEXT("NetToken"), TEXT("/Script/IrisCore.NetTokenDataStream"));
	DataStreamUtil.AddDataStreamDefinition(TEXT("Replication"), TEXT("/Script/IrisCore.ReplicationDataStream"));
	DataStreamUtil.FixupDefinitions();

	Server = new FReplicationSystemTestServer(GetName());
}

void FReplicationSystemServerClientTestFixture::TearDown()
{
	delete Server;
	for (FReplicationSystemTestClient* Client : Clients)
	{
		delete Client;
	}
	Clients.Empty();
	DataStreamUtil.TearDown();

	FNetworkAutomationTestSuiteFixture::TearDown();
}

FReplicationSystemTestClient* FReplicationSystemServerClientTestFixture::CreateClient()
{
	FReplicationSystemTestClient* Client = new FReplicationSystemTestClient(GetName());
	Clients.Add(Client);

	// The client needs a connection
	Client->LocalConnectionId = Client->AddConnection();

	// Auto connect to server
	Client->ConnectionIdOnServer = Server->AddConnection();

	return Client;
}

}
