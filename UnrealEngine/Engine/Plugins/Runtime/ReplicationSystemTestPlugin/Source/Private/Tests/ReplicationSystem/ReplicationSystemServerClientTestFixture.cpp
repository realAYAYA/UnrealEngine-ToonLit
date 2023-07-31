// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationConnections.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/ReplicationSystem/ReplicationReader.h"
#include "Iris/PacketControl/PacketNotification.h"

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
FReplicationSystemTestNode::FReplicationSystemTestNode(bool bIsServer)
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

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateSubObject(FNetHandle Owner, const UTestReplicatedIrisObject::FComponents& Components)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
	CreatedObject->AddComponents(Components);

	// Add it to the bridge for replication
	ReplicationBridge->BeginReplication(Owner, CreatedObject);
	
	return CreatedObject;
}

UTestReplicatedIrisObject* FReplicationSystemTestNode::CreateSubObject(FNetHandle Owner, uint32 NumComponents, uint32 NumIrisComponents)
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

void FReplicationSystemTestNode::DestroyObject(UReplicatedTestObject* Object)
{
	// Destroy handle
	check(Object && Object->NetHandle.IsValid());

	// Destroy the handle
	ReplicationBridge->EndReplication(Object);

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

bool FReplicationSystemTestNode::SendUpdate(uint32 ConnectionId)
{
	FPacketData Packet;

	FNetBitStreamWriter Writer;
	Writer.InitBytes(Packet.PacketBuffer, FMath::Min<uint32>(FPacketData::MaxPacketSize, MaxSendPacketSize));

	FNetSerializationContext Context(&Writer);

	FConnectionInfo& Connection = GetConnectionInfo(ConnectionId);

	const FDataStreamRecord* Record = nullptr;

	const bool bResult = (Connection.DataStreamManager->BeginWrite() != UDataStream::EWriteResult::NoData) && (Connection.DataStreamManager->WriteData(Context, Record)  != UDataStream::EWriteResult::NoData);
	if (bResult)
	{
		Writer.CommitWrites();
		Packet.BitCount = Writer.GetPosBits();

		Connection.WriteRecords.Enqueue(Record);
		Connection.WrittenPackets.Enqueue(Packet);
	}

	Connection.DataStreamManager->EndWrite();

	return bResult;
}

void FReplicationSystemTestNode::PostSendUpdate()
{
	ReplicationSystem->PostSendUpdate();
}

void FReplicationSystemTestNode::DeliverTo(FReplicationSystemTestNode& Dest, uint32 LocalConnectionId, uint32 RemoteConnectionId, bool bDeliver)
{
	FConnectionInfo& Connection = GetConnectionInfo(LocalConnectionId);

	if (bDeliver)
	{
		const FPacketData& Packet = Connection.WrittenPackets.Peek();

		FNetBitStreamReader Reader;
		Reader.InitBits(Packet.PacketBuffer, Packet.BitCount);
		FNetSerializationContext Context(&Reader);

		Dest.RecvUpdate(RemoteConnectionId, Context);
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

FReplicationSystemTestNode::~FReplicationSystemTestNode()
{
	FReplicationSystemFactory::DestroyReplicationSystem(ReplicationSystem);
	CreatedObjects.Empty();
}

// FReplicationSystemTestClient implementation
FReplicationSystemTestClient::FReplicationSystemTestClient(uint32 ReplicationSystemId)
: FReplicationSystemTestNode(false)
, ConnectionIdOnServer(~0U)
{
}

// FReplicationSystemTestServer implementation
FReplicationSystemTestServer::FReplicationSystemTestServer()
: FReplicationSystemTestNode(true)
{
}

bool FReplicationSystemTestServer::SendAndDeliverTo(FReplicationSystemTestClient* Client, bool bDeliver)
{
	if (SendUpdate(Client->ConnectionIdOnServer))
	{
		DeliverTo(Client, bDeliver);

		return true;
	}

	return false;
}

// Send data, return true if data was written
bool FReplicationSystemTestServer::SendTo(FReplicationSystemTestClient* Client)
{
	return SendUpdate(Client->ConnectionIdOnServer);
}

// If bDeliver is true deliver data to client and report packet as delivered
// if bDeliver is false, do not deliver packet and report a dropped packet
void FReplicationSystemTestServer::DeliverTo(FReplicationSystemTestClient* Client, bool bDeliver)
{
	FReplicationSystemTestNode::DeliverTo(*Client, Client->ConnectionIdOnServer, Client->LocalConnectionId, bDeliver);
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

	Server = new FReplicationSystemTestServer;
}

FReplicationSystemTestClient* FReplicationSystemServerClientTestFixture::CreateClient()
{
	FReplicationSystemTestClient* Client = new FReplicationSystemTestClient(Clients.Num() + 1U);
	Clients.Add(Client);

	// The client needs a connection
	Client->LocalConnectionId = Client->AddConnection();

	// Auto connect to server
	Client->ConnectionIdOnServer = Server->AddConnection();

	return Client;
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

}
