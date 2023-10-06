// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/ReplicationSystem/MultiReplicationSystemsTestFixture.h"
#include "Net/Core/NetHandle/NetHandleManager.h"

namespace UE::Net::Private
{

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, CanCreateMultipleReplicationSystems)
{
	CreateSomeServers();

	TArrayView<FReplicationSystemTestServer*> ServersView = GetAllServers();
	UE_NET_ASSERT_EQ(unsigned(ServersView.Num()), unsigned(DefaultServerCount));

	const unsigned ExpectedReplicationSystemCount = DefaultServerCount;
	unsigned ValidReplicationSystemCount = 0U;
	for (FReplicationSystemTestServer* Server : ServersView)
	{
		if (UReplicationSystem* ReplicationSystem = Server->GetReplicationSystem())
		{
			++ValidReplicationSystemCount;
		}
	}

	UE_NET_ASSERT_EQ(ValidReplicationSystemCount, ExpectedReplicationSystemCount);
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, CanReplicateObjectOnMultipleReplicationSystems)
{
	CreateSomeServers();

	UTestReplicatedIrisObject* Object = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(Object);

	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge();
		FNetRefHandle RefHandle = ReplicationBridge->GetReplicatedRefHandle(Object);
		UE_NET_ASSERT_TRUE(RefHandle.IsValid());
	}
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ReplicatedObjectIsAssignedGlobalNetHandle)
{
	CreateSomeServers();

	UTestReplicatedIrisObject* Object = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(Object);

	FNetHandle NetHandle = FNetHandleManager::GetNetHandle(Object);
	UE_NET_ASSERT_TRUE(NetHandle.IsValid());
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ReplicatedObjectLosesGlobalNetHandleAfterEndReplication)
{
	CreateSomeServers();

	UTestReplicatedIrisObject* Object = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(Object);
	EndReplication(Object);

	// The object should no longer be associated with a NetHandle when ending replication on all systems.
	FNetHandle NetHandle = FNetHandleManager::GetNetHandle(Object);
	UE_NET_ASSERT_FALSE(NetHandle.IsValid());
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ReplicatedObjectKeepsGlobalNetHandleAfterEndReplicationOnSingleSystem)
{
	CreateSomeServers();

	UTestReplicatedIrisObject* Object = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(Object);

	FNetHandle NetHandlePriorToEndReplication = FNetHandleManager::GetNetHandle(Object);

	// End replication on a single system
	FReplicationSystemTestServer* Server = GetAllServers()[0];
	UReplicatedTestObjectBridge* ReplicationBridge = Server->GetReplicationBridge();
	UE_NET_ASSERT_NE(ReplicationBridge, nullptr);
	ReplicationBridge->EndReplication(Object);

	// Make sure replication was ended on the system.
	FNetRefHandle RefHandle = ReplicationBridge->GetReplicatedRefHandle(Object);
	UE_NET_ASSERT_FALSE(RefHandle.IsValid());

	// Validate there's still a global NetHandle assigned.
	FNetHandle NetHandleAfterSingleEndReplication = FNetHandleManager::GetNetHandle(Object);
	UE_NET_ASSERT_EQ(NetHandlePriorToEndReplication, NetHandleAfterSingleEndReplication);
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ObjectIsReplicatedToAllClientsOnAllSystems)
{
	CreateSomeServers();

	// Create clients for all systems
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		constexpr unsigned ClientCountPerServer = 3U;
		for (unsigned ClientIt = 0, ClientEndIt = ClientCountPerServer; ClientIt != ClientEndIt; ++ClientIt)
		{
			CreateClientForServer(Server);
		}
	}

	UTestReplicatedIrisObject* ServerObject = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(ServerObject);

	FullSendAndDeliverUpdate();

	// Verify the object was created on all clients
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		FNetRefHandle RefHandleOnServer = Server->GetReplicationBridge()->GetReplicatedRefHandle(ServerObject);
		for (FReplicationSystemTestClient* Client : GetClients(Server))
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(RefHandleOnServer), nullptr);
		}
	}
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ReplicatedObjectIsDestroyedOnAllClientsAfterEndReplication)
{
	CreateSomeServers();

	// Create clients for all systems
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		constexpr unsigned ClientCountPerServer = 3;
		for (unsigned ClientIt = 0, ClientEndIt = ClientCountPerServer; ClientIt != ClientEndIt; ++ClientIt)
		{
			CreateClientForServer(Server);
		}
	}

	UTestReplicatedIrisObject* ServerObject = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(ServerObject);

	TArray<FNetRefHandle> ServerRefHandles;
	ServerRefHandles.Reserve(GetAllServers().Num());
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		FNetRefHandle RefHandle = Server->GetReplicationBridge()->GetReplicatedRefHandle(ServerObject);
		ServerRefHandles.Add(RefHandle);
	}

	FullSendAndDeliverUpdate();

	EndReplication(ServerObject);

	FullSendAndDeliverUpdate();

	// Verify the object was destroyed on all clients
	unsigned ServerIt = 0;
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		FNetRefHandle RefHandleOnServer = ServerRefHandles[ServerIt++];
		for (FReplicationSystemTestClient* Client : GetClients(Server))
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(RefHandleOnServer), nullptr);
		}
	}
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ReplicatedObjectCanStopReplicatingOnSingleSystem)
{
	CreateSomeServers();

	// Create clients for all systems
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		CreateClientForServer(Server);
	}

	UTestReplicatedIrisObject* ServerObject = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(ServerObject);

	FullSendAndDeliverUpdate();

	// End replication on single system
	constexpr SIZE_T SystemIndexToEndReplicationOn = 0;
	FReplicationSystemTestServer* ServerToEndReplicationOn = GetAllServers()[SystemIndexToEndReplicationOn];
	FNetRefHandle RefHandleOnServerToEndReplicationOn;
	{
		UReplicatedTestObjectBridge* ReplicationBridge = ServerToEndReplicationOn->GetReplicationBridge();
		RefHandleOnServerToEndReplicationOn = ReplicationBridge->GetReplicatedRefHandle(ServerObject);
		ReplicationBridge->EndReplication(ServerObject);
	}

	FullSendAndDeliverUpdate();

	// Verify object was destroyed on client connected to system where replication was ended
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		if (Server == ServerToEndReplicationOn)
		{
			for (FReplicationSystemTestClient* Client : GetClients(ServerToEndReplicationOn))
			{
				UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(RefHandleOnServerToEndReplicationOn), nullptr);
			}
		}
		else
		{
			FNetRefHandle RefHandleOnServer = Server->GetReplicationBridge()->GetReplicatedRefHandle(ServerObject);
			for (FReplicationSystemTestClient* Client : GetClients(Server))
			{
				UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(RefHandleOnServer), nullptr);
			}
		}
	}
}

UE_NET_TEST_FIXTURE(FMultiReplicationSystemsTestFixture, ModifiedObjectIsReplicatedToAllClientsOnAllSystems)
{
	CreateSomeServers();

	// Create clients for all systems
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		constexpr unsigned ClientCountPerServer = 1U;
		for (unsigned ClientIt = 0, ClientEndIt = ClientCountPerServer; ClientIt != ClientEndIt; ++ClientIt)
		{
			CreateClientForServer(Server);
		}
	}

	UTestReplicatedIrisObject* ServerObject = CreateObject(UTestReplicatedIrisObject::FComponents{});
	BeginReplication(ServerObject);

	FullSendAndDeliverUpdate();

	ServerObject->IntA ^= 4711;
	const int32 ExpectedIntAVAlue = ServerObject->IntA;

	FullSendAndDeliverUpdate();

	// Verify the object has the updated value on all clients
	for (FReplicationSystemTestServer* Server : GetAllServers())
	{
		FNetRefHandle RefHandleOnServer = Server->GetReplicationBridge()->GetReplicatedRefHandle(ServerObject);
		for (FReplicationSystemTestClient* Client : GetClients(Server))
		{
			UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(RefHandleOnServer));
			CA_ASSUME(ClientObject != nullptr);
			UE_NET_ASSERT_EQ(ClientObject->IntA, ExpectedIntAVAlue);
		}
	}
}

}
