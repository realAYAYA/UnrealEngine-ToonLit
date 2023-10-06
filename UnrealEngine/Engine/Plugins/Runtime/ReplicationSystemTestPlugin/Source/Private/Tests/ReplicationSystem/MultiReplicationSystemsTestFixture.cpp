// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/ReplicationSystem/MultiReplicationSystemsTestFixture.h"
#include "Containers/Array.h"
#include "Containers/Map.h"

namespace UE::Net
{

void FMultiReplicationSystemsTestFixture::SetUp()
{
	FNetworkAutomationTestSuiteFixture::SetUp();

	// Fake what we normally get from config
	DataStreamUtil.SetUp();
	DataStreamUtil.AddDataStreamDefinition(TEXT("NetToken"), TEXT("/Script/IrisCore.NetTokenDataStream"));
	DataStreamUtil.AddDataStreamDefinition(TEXT("Replication"), TEXT("/Script/IrisCore.ReplicationDataStream"));
	DataStreamUtil.FixupDefinitions();
}

void FMultiReplicationSystemsTestFixture::TearDown()
{
	for (const TMap<FReplicationSystemTestServer*, TArray<FReplicationSystemTestClient*>>::ElementType& ServerAndClient : ServerClients)
	{
		FReplicationSystemTestServer* Server = ServerAndClient.Key;
		delete Server;
		for (FReplicationSystemTestClient* Client : ServerAndClient.Value)
		{
			delete Client;
		}
	}

	Servers.Empty();
	ServerClients.Empty();

	DataStreamUtil.TearDown();

	FNetworkAutomationTestSuiteFixture::TearDown();
}

void FMultiReplicationSystemsTestFixture::CreateServers(unsigned ServerCount)
{
	for (unsigned It = 0, EndIt = ServerCount; It != EndIt; ++It)
	{
		CreateServer();
	}
}

void FMultiReplicationSystemsTestFixture::CreateSomeServers()
{
	CreateServers(DefaultServerCount);
}

TArrayView<FReplicationSystemTestServer*> FMultiReplicationSystemsTestFixture::GetAllServers()
{
	return MakeArrayView(Servers);
}

FReplicationSystemTestServer* FMultiReplicationSystemsTestFixture::CreateServer()
{
	FReplicationSystemTestServer* Server = new FReplicationSystemTestServer(GetName());
	Servers.Add(Server);
	ServerClients.Emplace(Server);

	return Server;
}

FReplicationSystemTestClient* FMultiReplicationSystemsTestFixture::CreateClientForServer(FReplicationSystemTestServer* Server)
{
	if (TMap<FReplicationSystemTestServer*, TArray<FReplicationSystemTestClient*>>::ValueType* Clients = ServerClients.Find(Server))
	{
		FReplicationSystemTestClient* Client = new FReplicationSystemTestClient(GetName()); 
		Clients->Add(Client);

		// The client needs a connection
		Client->LocalConnectionId = Client->AddConnection();

		// Auto connect to server
		Client->ConnectionIdOnServer = Server->AddConnection();

		return Client;
	}

	return nullptr;
}

TArrayView<FReplicationSystemTestClient*> FMultiReplicationSystemsTestFixture::GetClients(FReplicationSystemTestServer* Server)
{
	if (TMap<FReplicationSystemTestServer*, TArray<FReplicationSystemTestClient*>>::ValueType* Clients = ServerClients.Find(Server))
	{
		return MakeArrayView(*Clients);
	}

	return TArrayView<FReplicationSystemTestClient*>();
}

UTestReplicatedIrisObject* FMultiReplicationSystemsTestFixture::CreateObject(const UTestReplicatedIrisObject::FComponents& Components)
{
	UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
	CreatedObject->AddComponents(Components);

	CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
	return CreatedObject;
}

void FMultiReplicationSystemsTestFixture::BeginReplication(UTestReplicatedIrisObject* Object)
{
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		if (UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge())
		{
			ReplicationBridge->BeginReplication(Object);
		}
	}
}

void FMultiReplicationSystemsTestFixture::EndReplication(UTestReplicatedIrisObject* Object)
{
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		if (UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge())
		{
			constexpr EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::Destroy | EEndReplicationFlags::DestroyNetHandle | EEndReplicationFlags::ClearNetPushId;
			ReplicationBridge->EndReplication(Object, EndReplicationFlags);
		}
	}
}

void FMultiReplicationSystemsTestFixture::BeginReplication(FReplicationSystemTestServer* Server, UTestReplicatedIrisObject* Object)
{
	if (UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge())
	{
		ReplicationBridge->BeginReplication(Object);
	}
}

void FMultiReplicationSystemsTestFixture::EndReplication(FReplicationSystemTestServer* Server, UTestReplicatedIrisObject* Object)
{
	if (UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge())
	{
		ReplicationBridge->EndReplication(Object);
	}
}

void FMultiReplicationSystemsTestFixture::FullSendAndDeliverUpdate()
{
	for (FReplicationSystemTestServer* Server : Servers)
	{
		Server->PreSendUpdate();
	}

	for (TMap<FReplicationSystemTestServer*, TArray<FReplicationSystemTestClient*>>::ElementType& ServerWithClients : ServerClients)
	{
		FReplicationSystemTestServer* Server = ServerWithClients.Key;
		for (FReplicationSystemTestClient* Client : ServerWithClients.Value)
		{
			Server->SendAndDeliverTo(Client, DeliverPacket);
		}
	}

	for (FReplicationSystemTestServer* Server : Servers)
	{
		Server->PostSendUpdate();
	}
}

}
