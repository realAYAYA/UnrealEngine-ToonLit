// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"

namespace UE::Net::Private
{

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectDroppedDataIsRetransmitted)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server add as a dependent object
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);
	
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	Bridge->AddDependentObject(ServerObject->NetHandle, ServerDependentObject->NetHandle);
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));

	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_EQ(ServerDependentObject->IntA, ClientDependentObject->IntA);

	// Modify the value of dependent object only
	ServerDependentObject->IntA = 2;

	// Send and do not deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Verify that the final state was applied to dependent object 
	UE_NET_ASSERT_NE(ServerDependentObject->IntA, ClientDependentObject->IntA);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that the final state was applied to dependent object 
	UE_NET_ASSERT_EQ(ServerDependentObject->IntA, ClientDependentObject->IntA);
}

// Dependent objects
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectWithZeroPrioOnlyReplicatesWithParent)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);

	// Filter out Server object to start with
	FNetObjectGroupHandle FilterGroup = ReplicationSystem->CreateGroup();
	ReplicationSystem->AddGroupFilter(FilterGroup);
	ReplicationSystem->AddToGroup(FilterGroup, ServerObject->NetHandle);

	// Setup dependent object to only replicate with ServerObject
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetHandle, 0.f);
	Bridge->AddDependentObject(ServerObject->NetHandle, ServerDependentObject->NetHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should not be created on client
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientDependentObject, nullptr);

	// Enable 
	ReplicationSystem->SetGroupFilterStatus(FilterGroup, ENetFilterStatus::Allow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should now exist on client
	ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
}

// Chained dependent objects
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestChainedDependentObjectWithZeroPrio)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);

	// Filter out Server object to start with
	FNetObjectGroupHandle FilterGroup = ReplicationSystem->CreateGroup();
	ReplicationSystem->AddGroupFilter(FilterGroup);
	ReplicationSystem->AddToGroup(FilterGroup, ServerObject->NetHandle);

	// Setup dependent object to only replicate with ServerObject
	UTestReplicatedIrisObject* ServerDependentObject0 = Server->CreateObject(0, 0);
	UTestReplicatedIrisObject* ServerDependentObject1 = Server->CreateObject(0, 0);
	
	ReplicationSystem->SetStaticPriority(ServerDependentObject0->NetHandle, 0.f);
	ReplicationSystem->SetStaticPriority(ServerDependentObject1->NetHandle, 0.f);
	
	// Setup dependency chain
	Bridge->AddDependentObject(ServerObject->NetHandle, ServerDependentObject0->NetHandle);
	Bridge->AddDependentObject(ServerDependentObject0->NetHandle, ServerDependentObject1->NetHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should not be created on client
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject0->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject1->NetHandle));

	UE_NET_ASSERT_EQ(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientDependentObject0, nullptr);
	UE_NET_ASSERT_EQ(ClientDependentObject1, nullptr);

	// Enable the parent
	ReplicationSystem->SetGroupFilterStatus(FilterGroup, ENetFilterStatus::Allow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that dependent object now is created
	ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	ClientDependentObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject0->NetHandle));
	ClientDependentObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject1->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject0, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject1, nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectPollFrequency)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server that later will be added as a dependent object
	// With high PollFramePeriod so that it will not replicate in a while unless it is a dependent
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);
	Server->ReplicationBridge->SetPollFramePeriod(ServerDependentObject, 255U);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));
	
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that only the server object has been updated
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_NE(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Add dependency
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetHandle, ServerDependentObject->NetHandle);

	// Change a value on owner
	ServerObject->IntA = 2;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// We now expect both objects to be in sync
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectPollWithDirtyParent)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject0 = Server->CreateObject(0, 0);
	UTestReplicatedIrisObject* ServerObject1 = Server->CreateObject(0, 0);

	// Spawn second object on server that later will be added as a dependent object
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);

	// Setup different pollframeperiods for the objects
	Server->ReplicationBridge->SetPollFramePeriod(ServerObject0, 10);
	Server->ReplicationBridge->SetPollFramePeriod(ServerObject1, 20);
	Server->ReplicationBridge->SetPollFramePeriod(ServerDependentObject, 40U);

	// Add dependent object to both server objects
	Server->ReplicationBridge->AddDependentObject(ServerObject0->NetHandle, ServerDependentObject->NetHandle);
	Server->ReplicationBridge->AddDependentObject(ServerObject1->NetHandle, ServerDependentObject->NetHandle);

	// Send and deliver packet, All objects are polled and replicated
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify state after initial replication
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));
	UTestReplicatedIrisObject* ClientObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject0->NetHandle));
	UTestReplicatedIrisObject* ClientObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetHandle));

	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_NE(ClientObject0, nullptr);
	UE_NET_ASSERT_NE(ClientObject1, nullptr);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, 0);

	const uint32 TickCount = 256;
	int PrevRcvdValue = 0;
	uint32 RepCount0 = 0U;
	uint32 RepCount1 = 0U;
	uint32 RepCountDependent = 0U;

	for (uint32 CurrentFrame = 1U; CurrentFrame < TickCount; ++CurrentFrame)
	{
		ServerObject0->IntA = CurrentFrame;
		ServerObject1->IntA = CurrentFrame;
		ServerDependentObject->IntA = CurrentFrame;

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		const int RcvdValue = ClientDependentObject->IntA;
		if (RcvdValue != PrevRcvdValue)
		{
			if (RcvdValue == ClientObject0->IntA)
			{
				++RepCount0;
			}
			if (RcvdValue == ClientObject1->IntA)
			{
				++RepCount1;
			}
			++RepCountDependent;
		}
		PrevRcvdValue = RcvdValue;		
	}
	// We expect the dependent object to have replicated more often than the parents
	UE_NET_ASSERT_GE(RepCountDependent, RepCount0);
	UE_NET_ASSERT_GE(RepCountDependent, RepCount1);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectIsPolledIfParentIsMarkedDirty)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationBridge->SetPollFramePeriod(ServerObject, 4U);

	// Spawn second object add it as a dependency and bump poll period
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);
	Server->ReplicationBridge->SetPollFramePeriod(ServerDependentObject, 10U);
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetHandle, ServerDependentObject->NetHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetHandle));

	// Modify data
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;

	// Send and deliver packet, we expect nothing to replicate
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that nothing replicated
	UE_NET_ASSERT_NE(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_NE(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Mark dependent dirty
	ReplicationSystem->MarkDirty(ServerDependentObject->NetHandle);

	// Send and deliver packet, we expect dependent object to have replicated 
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that dependent object has replicated
	UE_NET_ASSERT_NE(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Modify data
	ServerObject->IntA = 2;
	ServerDependentObject->IntA = 2;

	// Mark parent dirty
	ReplicationSystem->MarkDirty(ServerObject->NetHandle);

	// Send and deliver packet, we expect both objects to have replicated
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that both objects have replicated
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);
}

}
