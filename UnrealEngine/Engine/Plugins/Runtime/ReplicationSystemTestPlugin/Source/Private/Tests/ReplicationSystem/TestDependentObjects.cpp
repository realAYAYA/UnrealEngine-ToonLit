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

	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

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
	ReplicationSystem->AddToGroup(FilterGroup, ServerObject->NetRefHandle);

	// Setup dependent object to only replicate with ServerObject
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(0, 0);
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should not be created on client
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientDependentObject, nullptr);

	// Enable 
	ReplicationSystem->SetGroupFilterStatus(FilterGroup, ENetFilterStatus::Allow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should now exist on client
	ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
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
	ReplicationSystem->AddToGroup(FilterGroup, ServerObject->NetRefHandle);

	// Setup dependent object to only replicate with ServerObject
	UTestReplicatedIrisObject* ServerDependentObject0 = Server->CreateObject(0, 0);
	UTestReplicatedIrisObject* ServerDependentObject1 = Server->CreateObject(0, 0);
	
	ReplicationSystem->SetStaticPriority(ServerDependentObject0->NetRefHandle, 0.f);
	ReplicationSystem->SetStaticPriority(ServerDependentObject1->NetRefHandle, 0.f);
	
	// Setup dependency chain
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject0->NetRefHandle);
	Bridge->AddDependentObject(ServerDependentObject0->NetRefHandle, ServerDependentObject1->NetRefHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Objects should not be created on client
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject0->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject1->NetRefHandle));

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
	ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	ClientDependentObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject0->NetRefHandle));
	ClientDependentObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject1->NetRefHandle));
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
	UObjectReplicationBridge::FCreateNetRefHandleParams Params = Server->GetReplicationBridge()->DefaultCreateNetRefHandleParams;
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(255U);
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(Params);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
	
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Trigger replication

	constexpr uint32 MaxIterationCount = 256;
	uint32 It = 0;
	for (const uint32 EndIt = MaxIterationCount; It < EndIt; ++It)
	{
		ServerObject->IntA += 1;
		ServerDependentObject->IntA += 1;

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);

		// Verify that only the server object has been updated
		if (ClientDependentObject->IntA != ServerDependentObject->IntA)
		{
			break;
		}
	}
	// At some point the object is expected not to be polled
	UE_NET_ASSERT_LT(It, MaxIterationCount);

	// Add dependency
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Change a value on owner
	ServerObject->IntA += 1;

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
	UObjectReplicationBridge::FCreateNetRefHandleParams Params = Server->GetReplicationBridge()->DefaultCreateNetRefHandleParams;

	// Setup different poll frequencies for the objects
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(10U);
	UTestReplicatedIrisObject* ServerObject0 = Server->CreateObject(Params);

	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(20U);
	UTestReplicatedIrisObject* ServerObject1 = Server->CreateObject(Params);

	// Spawn second object on server that later will be added as a dependent object
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(40U);
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(Params);



	// Add dependent object to both server objects
	Server->ReplicationBridge->AddDependentObject(ServerObject0->NetRefHandle, ServerDependentObject->NetRefHandle);
	Server->ReplicationBridge->AddDependentObject(ServerObject1->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Send and deliver packet, All objects are polled and replicated
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify state after initial replication
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientObject0 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject0->NetRefHandle));
	UTestReplicatedIrisObject* ClientObject1 = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetRefHandle));

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
	UObjectReplicationBridge::FCreateNetRefHandleParams Params = Server->GetReplicationBridge()->DefaultCreateNetRefHandleParams;
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(14U);
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(Params);

	// Spawn second object add it as a dependency and bump poll period
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(30U);
	UTestReplicatedIrisObject* ServerDependentObject = Server->CreateObject(Params);
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientDependentObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

	// Modify data until none of the objects are replicated, due to the set polling frequencies.
	constexpr uint32 MaxIterationCount = 32;
	uint32 It = 0;
	for (const uint32 EndIt = MaxIterationCount; It < EndIt; ++It)
	{
		ServerObject->IntA += 1;
		ServerDependentObject->IntA += 1;

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		// Verify that nothing replicated
		if (ClientObject->IntA != ServerObject->IntA && ClientDependentObject->IntA != ServerDependentObject->IntA)
		{
			break;
		}
	}
	// If we hit this assert then it's likely the poll frequency limiter is broken.
	UE_NET_ASSERT_LT(It, MaxIterationCount);

	// Mark dependent dirty
	ReplicationSystem->ForceNetUpdate(ServerDependentObject->NetRefHandle);

	// Send and deliver packet, we expect dependent object to have replicated 
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UE_NET_EXPECT_NE(ClientObject->IntA, ServerObject->IntA);
	// Verify that dependent object has replicated
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);

	// Modify data
	ServerObject->IntA += 2;
	ServerDependentObject->IntA += 2;

	// Mark parent dirty
	ReplicationSystem->ForceNetUpdate(ServerObject->NetRefHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that both objects have replicated
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientDependentObject->IntA, ServerDependentObject->IntA);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectScheduledAfterParent)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Spawn dependent object
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Set prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_GT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);

	// Modify both parent and dependent
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that they have replicated in expected order 
	UE_NET_ASSERT_GT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectScheduledBeforeParent)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Spawn dependent object
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParent);

	// Set prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_LT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);

	// Modify both parent and dependent
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that they have replicated in expected order
	UE_NET_ASSERT_LT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDependentObjectScheduledBeforeParentIfInitialState)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Spawn dependent object
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParentIfInitialState);

	// Set prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_LT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);

	// Modify both parent and dependent
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that they have replicated in expected order
	UE_NET_ASSERT_GT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestNestedDependentObjectScheduledBeforeParent)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server, abusing some assumptions about scheduling order based on assigned internal indices to try out ordering of dependent objects
	UReplicatedSubObjectOrderObject* ServerNestedDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Setup dependent object and nested dependent object to both replicate before the parent
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParent);
	Bridge->AddDependentObject(ServerDependentObject->NetRefHandle, ServerNestedDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParent);

	// Set static prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);
	ReplicationSystem->SetStaticPriority(ServerNestedDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientNestedDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_NE(ClientNestedDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_LT(ClientNestedDependentObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);
	UE_NET_ASSERT_LT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);

	// Modify both parent and dependent
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;
	ServerNestedDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that they have replicated in expected order 
	UE_NET_ASSERT_LT(ClientNestedDependentObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);
	UE_NET_ASSERT_LT(ClientDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestNestedDependentObjectScheduledBeforeParents)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server, abusing some assumptions about scheduling order based on assigned internal indices to try out ordering of dependent objects
	UReplicatedSubObjectOrderObject* ServerNestedDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Setup dependent object with a nested dependent object that is replicating before its parent
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);
	Bridge->AddDependentObject(ServerDependentObject->NetRefHandle, ServerNestedDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParent);

	// Set static prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);
	ReplicationSystem->SetStaticPriority(ServerNestedDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientNestedDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);
	UE_NET_ASSERT_NE(ClientNestedDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_LT(ClientNestedDependentObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);
	UE_NET_ASSERT_LT(ClientObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);

	// Modify both parent and dependent
	ServerObject->IntA = 1;
	ServerDependentObject->IntA = 1;
	ServerNestedDependentObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that they have replicated in expected order 
	UE_NET_ASSERT_LT(ClientNestedDependentObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);
	UE_NET_ASSERT_LT(ClientObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestLateAddedNestedDependentObjectScheduledBeforeParents)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	UReplicatedTestObjectBridge* Bridge = Server->GetReplicationBridge();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server, abusing some assumptions about scheduling order based on assigned internal indices to try out ordering of dependent objects
	UReplicatedSubObjectOrderObject* ServerDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	UReplicatedSubObjectOrderObject* ServerObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();

	// Setup dependent object with a nested dependent object that is replicating before its parent
	Bridge->AddDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);

	// Set static prio to only replicate with parent
	ReplicationSystem->SetStaticPriority(ServerDependentObject->NetRefHandle, 0.f);

	// Reset RepOrderCounter
	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientDependentObject, nullptr);

	// Verify that they have replicated in expected order for initial objects
	UE_NET_ASSERT_LT(ClientObject->LastRepOrderCounter, ClientDependentObject->LastRepOrderCounter);

	// Create new dependent object
	UReplicatedSubObjectOrderObject* ServerNestedDependentObject = Server->CreateObject<UReplicatedSubObjectOrderObject>();
	Bridge->AddDependentObject(ServerDependentObject->NetRefHandle, ServerNestedDependentObject->NetRefHandle, EDependentObjectSchedulingHint::ScheduleBeforeParent);
	ReplicationSystem->SetStaticPriority(ServerNestedDependentObject->NetRefHandle, 0.f);

	// Modify parent to trigger replication of new dependent object
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that new dependent object has been created
	UReplicatedSubObjectOrderObject* ClientNestedDependentObject = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle));
	UE_NET_ASSERT_NE(ClientNestedDependentObject, nullptr);

	// Verify that they have replicated in expected order 
	UE_NET_ASSERT_LT(ClientNestedDependentObject->LastRepOrderCounter, ClientObject->LastRepOrderCounter);
}

}
