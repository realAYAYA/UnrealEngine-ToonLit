// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilterDefinitions.h"
#include "Tests/ReplicationSystem/Filtering/MockNetObjectFilter.h"
#include "Tests/ReplicationSystem/Filtering/TestFilteringObject.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"

namespace UE::Net::Private
{

class FTestFilteringFixture : public FReplicationSystemServerClientTestFixture
{
protected:
	virtual void SetUp() override
	{
		InitNetObjectFilterDefinitions();
		FReplicationSystemServerClientTestFixture::SetUp();
		InitMockNetObjectFilter();
	}

	virtual void TearDown() override
	{
		FReplicationSystemServerClientTestFixture::TearDown();
		RestoreFilterDefinitions();
	}

	virtual FName GetMockFilterName() const { return FName("MockFilter"); }
	virtual FName GetMockFilterClassName() const { return FName("/Script/ReplicationSystemTestPlugin.MockNetObjectFilter"); }

private:
	void InitNetObjectFilterDefinitions()
	{
		const UClass* NetObjectFilterDefinitionsClass = UNetObjectFilterDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectFilterDefinitionsClass->FindPropertyByName("NetObjectFilterDefinitions");
		check(DefinitionsProperty != nullptr);

		// Save CDO state.
		UNetObjectFilterDefinitions* FilterDefinitions = GetMutableDefault<UNetObjectFilterDefinitions>();
		DefinitionsProperty->CopyCompleteValue(&OriginalFilterDefinitions, (void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()));
		
		// Modify definitions to only include our filters. Ugly... 
		TArray<FNetObjectFilterDefinition> NewFilterDefinitions;
		{
			FNetObjectFilterDefinition& MockDefinition = NewFilterDefinitions.Emplace_GetRef();
			MockDefinition.FilterName = GetMockFilterName();
			MockDefinition.ClassName = GetMockFilterClassName();
			MockDefinition.ConfigClassName = "/Script/ReplicationSystemTestPlugin.MockNetObjectFilterConfig";
		}

		{
			FNetObjectFilterDefinition& MockDefinition = NewFilterDefinitions.Emplace_GetRef();
			MockDefinition.FilterName = "MockFilterWithFragments";
			MockDefinition.ClassName = "/Script/ReplicationSystemTestPlugin.MockNetObjectFilterUsingFragmentData";
			MockDefinition.ConfigClassName = "/Script/ReplicationSystemTestPlugin.MockNetObjectFilterConfig";
		}

		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &NewFilterDefinitions);
	}

	void RestoreFilterDefinitions()
	{
		// Restore CDO state from the saved state.
		const UClass* NetObjectFilterDefinitionsClass = UNetObjectFilterDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectFilterDefinitionsClass->FindPropertyByName("NetObjectFilterDefinitions");
		UNetObjectFilterDefinitions* FilterDefinitions = GetMutableDefault<UNetObjectFilterDefinitions>();
		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(FilterDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &OriginalFilterDefinitions);
		OriginalFilterDefinitions.Empty();

		MockFilterHandle = InvalidNetObjectFilterHandle;
		MockNetObjectFilter = nullptr;

		MockFilterWithFragmentsHandle = InvalidNetObjectFilterHandle;
		MockNetObjectFilterWithFragments = nullptr;
	}

	void InitMockNetObjectFilter()
	{
		MockNetObjectFilter = CastChecked<UMockNetObjectFilter>(Server->GetReplicationSystem()->GetFilter(GetMockFilterName()));
		MockFilterHandle = Server->GetReplicationSystem()->GetFilterHandle(GetMockFilterName());

		MockNetObjectFilterWithFragments = ExactCast<UMockNetObjectFilterUsingFragmentData>(Server->GetReplicationSystem()->GetFilter("MockFilterWithFragments"));
		MockFilterWithFragmentsHandle = Server->GetReplicationSystem()->GetFilterHandle("MockFilterWithFragments");
	}

protected:
	UMockNetObjectFilter* MockNetObjectFilter;
	FNetObjectFilterHandle MockFilterHandle;

	UMockNetObjectFilterUsingFragmentData* MockNetObjectFilterWithFragments;
	FNetObjectFilterHandle MockFilterWithFragmentsHandle;

private:
	TArray<FNetObjectFilterDefinition> OriginalFilterDefinitions;
};


class FTestFilteringWithConditionFixture : public FTestFilteringFixture
{
protected:

	virtual FName GetMockFilterName() const { return FName("MockFilterWithCondition"); }
	virtual FName GetMockFilterClassName() const { return FName("/Script/ReplicationSystemTestPlugin.MockNetObjectFilterWithCondition"); }
};

// Owner filtering tests
UE_NET_TEST_FIXTURE(FTestFilteringFixture, OwnerFilterPreventsObjectFromReplicatingToNonOwner)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply owner filter
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not have been created on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObject);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, OwnerFilterAllowsObjectToReplicateToOwner)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);
	Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should have been created on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Destroy object on server and client
	Server->DestroyObject(ServerObject);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, OwnerFilterReplicatesOnlyToOwningConnection)
{
	// Add clients
	FReplicationSystemTestClient* ClientArray[] = {CreateClient(), CreateClient(), CreateClient()};
	constexpr SIZE_T LastClientIndex = UE_ARRAY_COUNT(ClientArray) - 1;

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);
	Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, ClientArray[LastClientIndex]->ConnectionIdOnServer);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should only have been created on the last client
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		const SIZE_T ClientIndex = &Client - &ClientArray[0];
		if (ClientIndex == LastClientIndex)
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
		else
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
	}

	Server->DestroyObject(ServerObject);
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanChangeOwningConnection)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Turn on owner filter
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As object is now filtered it should be deleted on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Set the client as owner
	Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The object should have been created again
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Finally, remove the owning connection
	Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The client is no longer owning the object so it should be deleted
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	Server->DestroyObject(ServerObject);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanChangeOwnerFilter)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Turn on owner filter
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As object is now filtered it should be deleted on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Remove the owner filter
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, InvalidNetObjectFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The object should have been created again
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Destroy the object
	Server->DestroyObject(ServerObject);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, LateAddedSubObjectGetsOwnerPropagated)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	TArray<UReplicatedTestObject*> ServerObjects;
	{
		constexpr SIZE_T ObjectCount = 64;
		ServerObjects.Reserve(ObjectCount);
		for (SIZE_T It = 0, EndIt = ObjectCount; It != EndIt; ++It)
		{
			UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
			ServerObjects.Add(ServerObject);
			Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);
		}
	}

	// Net update
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Create subobject to arbitrary object
	UReplicatedTestObject* ArbitraryServerObject = ServerObjects[3];
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ArbitraryServerObject->NetRefHandle, 1, 1);

	// Net update
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Verify subobject owner is as expected
	UE_NET_ASSERT_EQ(Server->ReplicationSystem->GetOwningNetConnection(ServerSubObject->NetRefHandle), Client->ConnectionIdOnServer);
}

// Connection filtering tests
UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterPreventsObjectFromReplicatingToFilteredOutConnections)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply connection filter
	TBitArray<> NoConnections;
	const bool bFilterWasApplied = Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Allow);
	UE_NET_ASSERT_TRUE(bFilterWasApplied);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not have been created on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterAllowsObjectToReplicateToAllowedConnections)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply connection filter
	TBitArray<> AllowedConnections;
	AllowedConnections.Init(false, Client->ConnectionIdOnServer + 1);
	AllowedConnections[Client->ConnectionIdOnServer] = true;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, AllowedConnections, ENetFilterStatus::Allow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should have been created on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterAllowsObjectToReplicateToLateJoiningConnections)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply filtering that allows all connections
	TBitArray<> NoConnections;
	constexpr ENetFilterStatus ReplicationStatus = ENetFilterStatus::Disallow;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ReplicationStatus);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Late join clients
	FReplicationSystemTestClient* ClientArray[] = {CreateClient(), CreateClient(), CreateClient()};

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should have been created on all clients
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	}
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterAllowsObjectToReplicateAndDoesNotAffectExistingGroupFilteredObjects)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject0 = Server->CreateObject(0, 0);
	
	// Setup group filter
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject0->NetRefHandle);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Apply filtering that allows all connections
	TBitArray<> NoConnections;
	constexpr ENetFilterStatus ReplicationStatus = ENetFilterStatus::Disallow;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ReplicationStatus);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Validate status on clients
	// Object should not have been created on the clients 
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject0->NetRefHandle), nullptr);
		
	// Object should have been crated on all clients
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterPreventsObjectFromReplicatingToFilteredOutLateJoiningConnections)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply filtering that allows all connections
	TBitArray<> Connection1(false, 2);
	Connection1[1] = true;
	constexpr ENetFilterStatus ReplicationStatus = ENetFilterStatus::Allow;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, Connection1, ReplicationStatus);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Late join clients
	FReplicationSystemTestClient* ClientArray[] = {CreateClient(), CreateClient(), CreateClient()};

	// Since we must set up the filtering before the clients are created we need to make sure our assumptions are valid.
	UE_NET_ASSERT_EQ(ClientArray[0]->ConnectionIdOnServer, 1U);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should have been created on the client with connection ID 1.
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		const uint32 ConnectionId = Client->ConnectionIdOnServer;
		const bool bShouldHaveObject = ConnectionId == 1;
		if (bShouldHaveObject)
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
		else
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
	}
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ConnectionFilterReplicatesOnlyToAllowedConnection)
{
	// Add clients
	FReplicationSystemTestClient* ClientArray[] = {CreateClient(), CreateClient(), CreateClient()};
	constexpr SIZE_T LastClientIndex = UE_ARRAY_COUNT(ClientArray) - 1;

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply filtering that allows the last client to receive the object
	const uint32 ConnectionIdForLastClient = ClientArray[LastClientIndex]->ConnectionIdOnServer;
	TBitArray<> AllowedConnections;
	AllowedConnections.Init(false, ConnectionIdForLastClient + 1);
	AllowedConnections[ConnectionIdForLastClient] = true;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, AllowedConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should only have been created on the last client
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		const SIZE_T ClientIndex = &Client - &ClientArray[0];
		if (ClientIndex == LastClientIndex)
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
		else
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
	}
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanChangeConnectionFilter)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Apply connection filtering, not allowing any connection to receive the object
	TBitArray<> NoConnections;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the object is now filtered it should be deleted on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Set the client as an allowed connection
	TBitArray<> AllowedConnections;
	AllowedConnections.Init(false, Client->ConnectionIdOnServer + 1);
	AllowedConnections[Client->ConnectionIdOnServer] = true;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, AllowedConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The object should have been created again
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Finally, set the filter to not include any connections again.
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The client is no longer owning the object so it should be deleted
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanToggleConnectionFilter)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Apply connection filtering, not allowing any connection to receive the object
	TBitArray<> NoConnections;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As object is now filtered it should be deleted on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Remove the connection filter
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, InvalidNetObjectFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// The object should have been created again
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Destroy the object
	Server->DestroyObject(ServerObject);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();
}


// Keep last! Toggle between different kinds of filters
UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanToggleBetweenAllFilters)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Toggle between owner and connection filters
	{
		// Apply owner filtering
		Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);

		// Send and deliver packets
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Object should not exist on client
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

		// Apply connection filtering, not allowing any connection to receive the object
		TBitArray<> NoConnections;
		Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Allow);

		// Send and deliver packets
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Object should still not exist on client
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

		// Finally test going from connection filtering to owner filtering
		Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, ToOwnerFilterHandle);

		// Send and deliver packets
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Object should still not exist on client
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	}
}

// Owner filtering tests
UE_NET_TEST_FIXTURE(FTestFilteringFixture, GroupFilterPreventsObjectFromReplicating)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject->NetRefHandle);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Filter out objects in group
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not have been created on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, GroupFilterAllowsObjectToReplicate)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject->NetRefHandle);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Filter out objects in group
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not have been created on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Allow replication again
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, Client->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now have been created on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, GroupFilterRestoresConnectionConnectionFilter)
{
	// Add clients
	FReplicationSystemTestClient* ClientArray[] = {CreateClient(), CreateClient(), CreateClient()};
	constexpr SIZE_T LastClientIndex = UE_ARRAY_COUNT(ClientArray) - 1;

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Apply filtering that allows the last client to receive the object
	const uint32 ConnectionIdForLastClient = ClientArray[LastClientIndex]->ConnectionIdOnServer;
	TBitArray<> AllowedConnections;
	AllowedConnections.Init(false, ConnectionIdForLastClient + 1);
	AllowedConnections[ConnectionIdForLastClient] = true;
	Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, AllowedConnections, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should only have been created on the last client
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		const SIZE_T ClientIndex = &Client - &ClientArray[0];
		if (ClientIndex == LastClientIndex)
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
		else
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
	}

	// Create and set group filter for last client only
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject->NetRefHandle);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, ConnectionIdForLastClient, ENetFilterStatus::Disallow);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object Should now have been destroyed on the last client
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	}

	// Clear Group filter
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, ConnectionIdForLastClient, ENetFilterStatus::Allow);

	// Send and deliver packets
	Server->PreSendUpdate();
	for (FReplicationSystemTestClient* Client : ClientArray)
	{
		Server->SendAndDeliverTo(Client, DeliverPacket);
	}
	Server->PostSendUpdate();

	// Object should now be recreated on last client
	for (FReplicationSystemTestClient*& Client : ClientArray)
	{
		const SIZE_T ClientIndex = &Client - &ClientArray[0];
		if (ClientIndex == LastClientIndex)
		{
			UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
		else
		{
			UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
		}
	}
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanGetDynamicFilter)
{
	UE_NET_ASSERT_NE(MockNetObjectFilter, nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, CanGetDynamicFilterHandle)
{
	UE_NET_ASSERT_NE(MockFilterHandle, InvalidNetObjectFilterHandle);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DynamicFilterInitIsCalled)
{
	const UMockNetObjectFilter::FFunctionCallStatus& FunctionCallStatus = MockNetObjectFilter->GetFunctionCallStatus();
	UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.Init, 1U);
	UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.Init, 1U);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DynamicFilterAddObjectAndRemoveObjectIsCalledWhenObjectIsDeleted)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = false;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	Server->PreSendUpdate();
	// Filter needs to be set now.
	{
		const UMockNetObjectFilter::FFunctionCallStatus& CallStatus = MockNetObjectFilter->GetFunctionCallStatus();

		UE_NET_ASSERT_EQ(CallStatus.CallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(CallStatus.SuccessfulCallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(CallStatus.CallCounts.RemoveObject, 0U);

		MockNetObjectFilter->ResetFunctionCallStatus();
	}
	Server->PostSendUpdate();

	Server->DestroyObject(ServerObject);

	Server->PreSendUpdate();
	// Filter needs to be cleared now.
	{
		const UMockNetObjectFilter::FFunctionCallStatus& CallStatus = MockNetObjectFilter->GetFunctionCallStatus();

		UE_NET_ASSERT_EQ(CallStatus.CallCounts.RemoveObject, 1U);
		UE_NET_ASSERT_EQ(CallStatus.SuccessfulCallCounts.RemoveObject, 1U);

		MockNetObjectFilter->ResetFunctionCallStatus();
	}
	Server->PostSendUpdate();
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DynamicFilterCanAllowObjectToReplicate)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = false;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should now exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DynamicFilterCanDisallowObjectToReplicate)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not exist on client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, SwitchingFiltersCallsRemoveObjectOnPreviousFilter)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	// Make sure filter is set
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Check RemoveObject is called when switching filters.
	{
		MockNetObjectFilter->ResetFunctionCallStatus();

		const TBitArray<> NoConnections;
		Server->ReplicationSystem->SetConnectionFilter(ServerObject->NetRefHandle, NoConnections, ENetFilterStatus::Disallow);

		Server->PreSendUpdate();
		Server->PostSendUpdate();

		const UMockNetObjectFilter::FFunctionCallStatus& FunctionCallStatus = MockNetObjectFilter->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.RemoveObject, 1U);
	}
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, SubObjectsAreReplicatedWhenOwnerDynamicFilterAllows)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = false;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that both the object and subobject exist.
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, SubObjectsAreNotReplicatedWhenOwnerDynamicFilterDisallows)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that neither the object nor the subobject exist.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DependentObjectIsUnaffectedByDynamicFilter)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server and set filter on dependent object
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerFutureDependentObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerFutureDependentObject->NetRefHandle, MockFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We expect the object to exist and the future dependent object not to exist
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFutureDependentObject->NetRefHandle), nullptr);

	// Make dependent object and make sure it's now replicated.
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetRefHandle, ServerFutureDependentObject->NetRefHandle);
	UReplicatedTestObject* ServerDependentObject = ServerFutureDependentObject;

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We now expect the dependent object to exist
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle), nullptr);

	// Remove dependency and make sure the formerly dependent object is removed from the client
	Server->ReplicationBridge->RemoveDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);
	UReplicatedTestObject* ServerFormerDependentObject = ServerDependentObject;

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We expect the former dependent object not to exist
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFormerDependentObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, NestedDependentObjectIsFilteredAsParentsOrIndependent)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server and set filter on dependent objects
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerFutureDependentObject = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerFutureNestedDependentObject = Server->CreateObject(0, 0);

	Server->ReplicationSystem->SetFilter(ServerFutureDependentObject->NetRefHandle, MockFilterHandle);
	Server->ReplicationSystem->SetFilter(ServerFutureNestedDependentObject->NetRefHandle, MockFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We expect the object to exist and the future dependent and future dependent objects not to exist
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFutureDependentObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFutureNestedDependentObject->NetRefHandle), nullptr);

	// Make dependent objects and make sure they're now replicated.
	Server->ReplicationBridge->AddDependentObject(ServerObject->NetRefHandle, ServerFutureDependentObject->NetRefHandle);
	Server->ReplicationBridge->AddDependentObject(ServerFutureDependentObject->NetRefHandle, ServerFutureNestedDependentObject->NetRefHandle);

	UReplicatedTestObject* ServerDependentObject = ServerFutureDependentObject;
	UReplicatedTestObject* ServerNestedDependentObject = ServerFutureNestedDependentObject;

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We now expect the dependent objects to exist
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerDependentObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle), nullptr);

	// Remove dependency on root and make sure the formerly dependent object is removed from the client thanks to the filter.
	Server->ReplicationBridge->RemoveDependentObject(ServerObject->NetRefHandle, ServerDependentObject->NetRefHandle);
	UReplicatedTestObject* ServerFormerDependentObject = ServerDependentObject;

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// We expect the former dependent object to not to exist, thanks to the filter
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFormerDependentObject->NetRefHandle), nullptr);

	// As the former dependent object is filtered out it's ok for the nested dependent object to be filtered out.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle), nullptr);

	// Remove filter on the nested dependent object
	Server->ReplicationSystem->SetFilter(ServerNestedDependentObject->NetRefHandle, InvalidNetObjectFilterHandle);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that dependent object no longer is filtered out even though its parent is
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerFormerDependentObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerNestedDependentObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ObjectGetsFilterOutSettingFromStart)
{
	// Setup dynamic filter for the test. For this test we want the value of the object's filter property to rule.
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = false;
		MockNetObjectFilterWithFragments->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilterWithFragments->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Create object and set filter
	UTestFilteringObject* ServerObject = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterWithFragmentsHandle);

	// We want the object to be filtered out
	constexpr bool bFilterOut = true;
	ServerObject->SetFilterOut(bFilterOut);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that the object does not exist on the client.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, ObjectGetsUpdatedFilterOutSetting)
{
	// Setup dynamic filter for the test. For this test we want the value of the object's filter property to rule.
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = false;
		MockNetObjectFilterWithFragments->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilterWithFragments->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Create object and set filter
	UTestFilteringObject* ServerObject = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterWithFragmentsHandle);

	// We don't want the object to be filtered out
	ServerObject->SetFilterOut(false);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that the object exists on the client.
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Mark the object to be filtered out.
	ServerObject->SetFilterOut(true);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that the object does not exist on the client.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, MixPreAndPostFilters)
{
	// Setup dynamic filter for the test.
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;

		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();

		MockNetObjectFilterWithFragments->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilterWithFragments->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Create the objects and set filter
	UTestFilteringObject* ServerObjectPreFilter = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObjectPreFilter->NetRefHandle, MockFilterHandle);

	UTestFilteringObject* ServerObjectPostFilter = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObjectPostFilter->NetRefHandle, MockFilterWithFragmentsHandle);

	// Create a non-filtered object
	UTestFilteringObject* ServerObjectNoFilter = Server->CreateObject<UTestFilteringObject>();

	// We want the objects to be filtered out
	constexpr bool bFilterOut = true;
	ServerObjectPreFilter->SetFilterOut(bFilterOut);
	ServerObjectPostFilter->SetFilterOut(bFilterOut);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Check that the object does not exist on the client.
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectPreFilter->NetRefHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectPostFilter->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectNoFilter->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestFilteringFixture, DynamicFilteredOutSubObjectsAreResetWhenIndexIsReused)
{
	// Setup dynamic filter for the test
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;
		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server and set filter
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	Server->ReplicationSystem->SetFilter(ServerObject->NetRefHandle, MockFilterHandle);

	// Create and destroy subobject
	{
		UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);
		Server->DestroyObject(ServerSubObject);
	}

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should not exist on client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Create new object which should get the same internal index as the destroyed SubObject
	UReplicatedTestObject* ServerObject2 = Server->CreateObject(0, 0);

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Object should exist on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject2->NetRefHandle), nullptr);
}


UE_NET_TEST_FIXTURE(FTestFilteringWithConditionFixture, TestCulledDirtyActors)
{
	// Setup dynamic filter for the test.
	{
		UMockNetObjectFilter::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.bReturnValue = true;
		CallSetup.Filter.bFilterOutByDefault = true;

		MockNetObjectFilter->SetFunctionCallSetup(CallSetup);
		MockNetObjectFilter->ResetFunctionCallStatus();
	}

	const uint32 RepSystemID = Server->GetReplicationSystemId();

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Create multiple filtered objects
	UTestFilteringObject* ServerObjectA = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObjectA->NetRefHandle, MockFilterHandle);

	UTestFilteringObject* ServerObjectB = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObjectB->NetRefHandle, MockFilterHandle);

	UTestFilteringObject* ServerObjectC = Server->CreateObject<UTestFilteringObject>();
	Server->ReplicationSystem->SetFilter(ServerObjectC->NetRefHandle, MockFilterHandle);

	// Create a non-filtered object
	UTestFilteringObject* ServerObjectNoFilter = Server->CreateObject<UTestFilteringObject>();

	// Filter them in
	{	
		constexpr bool bFilterIn = false;
		ServerObjectA->SetFilterOut(bFilterIn);
		ServerObjectB->SetFilterOut(bFilterIn);
		ServerObjectC->SetFilterOut(bFilterIn);

		// Send and deliver packets
		Server->UpdateAndSend({Client});

		// Check that the filtered object do exist on the client.
		UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle), nullptr);
		UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle), nullptr);
		UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectC->NetRefHandle), nullptr);
	}

	// Now filter them out
	{
		constexpr bool bFilterOut = true;
		ServerObjectA->SetFilterOut(bFilterOut);
		ServerObjectB->SetFilterOut(bFilterOut);
		ServerObjectC->SetFilterOut(bFilterOut);

		// Send and deliver packets
		Server->UpdateAndSend({ Client });

		// Check that the filtered object do not exist on the client.
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle), nullptr);
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle), nullptr);
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectC->NetRefHandle), nullptr);
	}

	// Mark objects dirty
	{
		ServerObjectA->ReplicatedCounter = 0x01;
		ServerObjectB->ReplicatedCounter = 0x01;
		ServerObjectC->ReplicatedCounter = 0x01;
		Server->GetReplicationSystem()->MarkDirty(ServerObjectA->NetRefHandle);
		Server->GetReplicationSystem()->MarkDirty(ServerObjectB->NetRefHandle);
		Server->GetReplicationSystem()->MarkDirty(ServerObjectC->NetRefHandle);

		// Send and deliver packets
		Server->UpdateAndSend({ Client });

		// Should still not exist on the client.
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle), nullptr);
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle), nullptr);
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectC->NetRefHandle), nullptr);
	}

	// Put one of them back in the scope
	{
		constexpr bool bFilterIn = false;
		ServerObjectA->SetFilterOut(bFilterIn);

		// Send and deliver packets
		Server->UpdateAndSend({ Client });

		// This one exists
		UTestFilteringObject* ClientObjectA = Cast<UTestFilteringObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle));
		UE_NET_ASSERT_NE(ClientObjectA, nullptr);
		UE_NET_ASSERT_TRUE(ClientObjectA->ReplicatedCounter == 0x01);

		// These don't
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle), nullptr);
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectC->NetRefHandle), nullptr);
	}

	// Add another one back in the scope
	{
		constexpr bool bFilterIn = false;
		ServerObjectB->SetFilterOut(bFilterIn);

		// Send and deliver packets
		Server->UpdateAndSend({ Client });

		// This one exists
		UTestFilteringObject* ClientObjectB = Cast<UTestFilteringObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle));
		UE_NET_ASSERT_NE(ClientObjectB, nullptr);
		UE_NET_ASSERT_TRUE(ClientObjectB->ReplicatedCounter == 0x01);

		// These don't
		UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectC->NetRefHandle), nullptr);
	}
}

} // end namespace UE::Net::Private
