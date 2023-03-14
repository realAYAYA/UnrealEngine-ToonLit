// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectGroups.h"
#include "Tests/ReplicationSystem/ReplicatedTestObject.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"

namespace UE::Net::Private
{

class FTestGroupsFixture : public FReplicationSystemServerClientTestFixture
{
public:

	FReplicationFiltering* ServerFiltering = nullptr;
	FNetObjectGroups* ServerGroups = nullptr;
	FDirtyNetObjectTracker* ServerDirtyNetObjectTracker = nullptr;
	
protected:
	virtual void SetUp() override
	{
		FReplicationSystemServerClientTestFixture::SetUp();
		ServerFiltering = &Server->ReplicationSystem->GetReplicationSystemInternal()->GetFiltering();
		ServerGroups = &Server->ReplicationSystem->GetReplicationSystemInternal()->GetGroups();
		ServerDirtyNetObjectTracker = &Server->ReplicationSystem->GetReplicationSystemInternal()->GetDirtyNetObjectTracker();
	}

	void Filter()
	{
		Server->PreSendUpdate();
		Server->PostSendUpdate();
	}

	void VerifyGroupStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId)
	{
		FNetObjectGroup* Group = ServerGroups->GetGroup(GroupHandle);

		UE_NET_ASSERT_TRUE(Group != nullptr);

		ENetFilterStatus ReplicationStatus = ENetFilterStatus::Disallow;
		UE_NET_ASSERT_TRUE(ServerFiltering->GetGroupFilterStatus(GroupHandle, ConnectionId, ReplicationStatus));

		const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(ConnectionId);
		for (uint32 InternalIndex : MakeArrayView(Group->Members.GetData(), Group->Members.Num()))
		{
			UE_NET_ASSERT_TRUE(ObjectGroupFilter.GetBit(InternalIndex) == (ReplicationStatus == ENetFilterStatus::Disallow));
		}
	}

	void VerifyObjectFilterStatus(FNetHandle Handle, uint32 ConnectionId, bool bExpectedReplicationAllowed)
	{
		const uint32 InternalIndex = Server->GetReplicationSystem()->GetReplicationSystemInternal()->GetNetHandleManager().GetInternalIndex(Handle);
		UE_NET_ASSERT_NE((uint32)FNetHandleManager::InvalidInternalIndex, InternalIndex);

		// Verify expected filter status
		const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(ConnectionId);

		// Filter is subtractive, so if the bit is set we the object is filtered out
		UE_NET_ASSERT_TRUE(ObjectGroupFilter.GetBit(InternalIndex) == !bExpectedReplicationAllowed);		
	}
};

UE_NET_TEST_FIXTURE(FTestGroupsFixture, PublicGroupAPI)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Create
	{
		FNetObjectGroupHandle GroupHandle = ReplicationSystem->CreateGroup();

		UE_NET_ASSERT_FALSE(ReplicationSystem->IsValidGroup(0U));
		UE_NET_ASSERT_TRUE(ReplicationSystem->IsValidGroup(GroupHandle));

		// Destroy
		ReplicationSystem->DestroyGroup(GroupHandle);
		UE_NET_ASSERT_FALSE(ReplicationSystem->IsValidGroup(GroupHandle));
	}

	// Add/Remove
	{
		// Spawn object on server
		UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

		FNetObjectGroupHandle GroupHandle = ReplicationSystem->CreateGroup();

		UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle, ServerObject->NetHandle));

		ReplicationSystem->AddToGroup(GroupHandle, ServerObject->NetHandle);

		UE_NET_ASSERT_TRUE(ReplicationSystem->IsInGroup(GroupHandle, ServerObject->NetHandle));

		ReplicationSystem->RemoveFromGroup(GroupHandle, ServerObject->NetHandle);

		UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle, ServerObject->NetHandle));

		ReplicationSystem->DestroyGroup(GroupHandle);

		UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle, ServerObject->NetHandle));
	}
}

UE_NET_TEST_FIXTURE(FTestGroupsFixture, PublicGroupAPIMemberOfMultipleGroups)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Create
	FNetObjectGroupHandle GroupHandle0 = ReplicationSystem->CreateGroup();
	FNetObjectGroupHandle GroupHandle1 = ReplicationSystem->CreateGroup();

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle0, ServerObject->NetHandle));
	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle1, ServerObject->NetHandle));

	ReplicationSystem->AddToGroup(GroupHandle0, ServerObject->NetHandle);

	UE_NET_ASSERT_TRUE(ReplicationSystem->IsInGroup(GroupHandle0, ServerObject->NetHandle));
	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle1, ServerObject->NetHandle));

	ReplicationSystem->AddToGroup(GroupHandle1, ServerObject->NetHandle);

	UE_NET_ASSERT_TRUE(ReplicationSystem->IsInGroup(GroupHandle0, ServerObject->NetHandle));
	UE_NET_ASSERT_TRUE(ReplicationSystem->IsInGroup(GroupHandle1, ServerObject->NetHandle));

	ReplicationSystem->RemoveFromGroup(GroupHandle0, ServerObject->NetHandle);

	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle0, ServerObject->NetHandle));
	UE_NET_ASSERT_TRUE(ReplicationSystem->IsInGroup(GroupHandle1, ServerObject->NetHandle));

	ReplicationSystem->RemoveFromGroup(GroupHandle1, ServerObject->NetHandle);

	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle0, ServerObject->NetHandle));
	UE_NET_ASSERT_FALSE(ReplicationSystem->IsInGroup(GroupHandle1, ServerObject->NetHandle));
}

// Group filtering tests
UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPI)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject0 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject1 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject2 = Server->CreateObject(0, 0);

	// Create group
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	UE_NET_ASSERT_NE(0U, (uint32)GroupHandle);	

	// Add Objects to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject0->NetHandle);
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject1->NetHandle);
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerObject2->NetHandle);
	
	UReplicatedTestObject* ServerObject3 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject4 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject5 = Server->CreateObject(0, 0);

	// Create another group
	FNetObjectGroupHandle GroupHandle2 = Server->ReplicationSystem->CreateGroup();
	UE_NET_ASSERT_NE(0U, (uint32)GroupHandle2);	

	// Add Objects to group
	Server->ReplicationSystem->AddToGroup(GroupHandle2, ServerObject3->NetHandle);
	Server->ReplicationSystem->AddToGroup(GroupHandle2, ServerObject4->NetHandle);
	Server->ReplicationSystem->AddToGroup(GroupHandle2, ServerObject5->NetHandle);
													 
	UReplicatedTestObject* ServerObject6 = Server->CreateObject(0, 0);

	// Create another group
	FNetObjectGroupHandle GroupHandle3 = Server->ReplicationSystem->CreateGroup();
	UE_NET_ASSERT_NE(0U, (uint32)GroupHandle3);	

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	// Add group to group filter
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle2);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle3);

	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle2, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle3, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);

	// Update filters
	Filter();

	VerifyGroupStatus(GroupHandle, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle2, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle3, Client->ConnectionIdOnServer);

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsAnyBitSet());

	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, Client->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Update filters
	Filter();

	VerifyGroupStatus(GroupHandle, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle2, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle3, Client->ConnectionIdOnServer);

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsAnyBitSet());

	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle2, Client->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Update filters
	Filter();

	VerifyGroupStatus(GroupHandle, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle2, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle3, Client->ConnectionIdOnServer);

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsNoBitSet());

	// Add Object to group 3 which is set, which should filter out the object
	Server->ReplicationSystem->AddToGroup(GroupHandle3, ServerObject6->NetHandle);

	// Update filters
	Filter();

	VerifyGroupStatus(GroupHandle, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle2, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle3, Client->ConnectionIdOnServer);

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsAnyBitSet());

	// Add Object to group 3 which is set, which should filter out the object
	Server->ReplicationSystem->RemoveFromGroup(GroupHandle3, ServerObject6->NetHandle);

	// Update filters
	Filter();

	VerifyGroupStatus(GroupHandle, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle2, Client->ConnectionIdOnServer);
	VerifyGroupStatus(GroupHandle3, Client->ConnectionIdOnServer);

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsNoBitSet());

	// Delete group
	Server->ReplicationSystem->DestroyGroup(GroupHandle);
	Server->ReplicationSystem->DestroyGroup(GroupHandle2);
	Server->ReplicationSystem->DestroyGroup(GroupHandle3);

	Filter();

	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsNoBitSet());
}

UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPIManyFilters)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject0 = Server->CreateObject(0, 0);

	// Create groups and add them as filters
	const uint32 GroupCount = 256;

	FNetObjectGroupHandle GroupHandles[GroupCount];

	for (uint32 It = 0; It < GroupCount; ++It)
	{
		FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
		Server->ReplicationSystem->AddGroupFilter(GroupHandle);
	
		GroupHandles[It] = GroupHandle;	
	}
}

UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPIObjectMemberOfMoreThanOneGroup)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject0 = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject0->NetHandle;

	// Create groups
	FNetObjectGroupHandle GroupHandle0 = Server->ReplicationSystem->CreateGroup();
	FNetObjectGroupHandle GroupHandle1 = Server->ReplicationSystem->CreateGroup();
	UE_NET_ASSERT_NE(0U, (uint32)GroupHandle0);	
	UE_NET_ASSERT_NE(0U, (uint32)GroupHandle1);	

	// Add Objects to groups
	Server->ReplicationSystem->AddToGroup(GroupHandle0, ServerHandle);
	Server->ReplicationSystem->AddToGroup(GroupHandle1, ServerHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	// Verify that object is not filtered out
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, true);

	// Add groups to group filter
	Server->ReplicationSystem->AddGroupFilter(GroupHandle0);
	Server->ReplicationSystem->AddGroupFilter(GroupHandle1);

	// Filter out objects in the group
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle0, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle1, Client->ConnectionIdOnServer, ENetFilterStatus::Disallow);

	// Update filters
	Filter();

	// We now expect the object to be filtered out
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, false);

	// Allow objects from group 0
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle0, Client->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Update filters
	Filter();

	// We still  expect the object to be filtered out since the object is a member of two groups
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, false);

	// Allow objects from group 1
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle1, Client->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Update filters
	Filter();

	// We now expect the object to no longer be filtered out
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, true);
}

// Group filtering tests
UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPINotFilteredGroup)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject0 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject1 = Server->CreateObject(0, 0);
	UReplicatedTestObject* ServerObject2 = Server->CreateObject(0, 0);

	// Add Objects to NotFilteredGroup
	Server->ReplicationSystem->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject0->NetHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	// We expect the filter to filter out the object	
	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsAnyBitSet());

	// Remove object from NotReplicatedNetObjectGroup
	Server->ReplicationSystem->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject0->NetHandle);

	// Update filters
	Filter();

	// We now expect no filtering
	UE_NET_ASSERT_TRUE(ObjectGroupFilter.IsNoBitSet());
}

// Verify that group filtering on subobjects works as expected
UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPISubObjects)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject->NetHandle;

	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerHandle, 0, 0);	
	const FNetHandle ServerSubObjectHandle = ServerSubObject->NetHandle;
	
	// Create groups
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Add Object to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	// Verify that both object and subobject are filtered out
	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	const bool bExpectsReplicationToBeAllowed = false;
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
	VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
}

// Verify that group filtering on subobjects works as expected and that it is restored if subobject is removed same frame
UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPIRemovingSubObjectsRestoresGroupFilteredOutObjects)
{
	// Create groups
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject->NetHandle;

	// Add Object to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerHandle);


	// Create sub object
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerHandle, 0, 0);
	const FNetHandle ServerSubObjectHandle = ServerSubObject->NetHandle;

	// destroy subobject in the same frame
	Server->DestroyObject(ServerSubObject);

	// Update filters
	Filter();

	// Verify that both object and subobject are filtered out
	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	const bool bExpectsReplicationToBeAllowed = false;
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
}



UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPILateAddedSubObjectIsFilteredOut)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject->NetHandle;
	
	// Create groups
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddGroupFilter(GroupHandle);

	// Add Object to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	// Add subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerHandle, 0, 0);	
	const FNetHandle ServerSubObjectHandle = ServerSubObject->NetHandle;

	// Update filters
	Filter();

	// Verify that both object and subobject are filtered out
	const FNetBitArrayView ObjectGroupFilter = ServerFiltering->GetGroupFilteredOutObjects(Client->ConnectionIdOnServer);

	const bool bExpectsReplicationToBeAllowed = false;
	VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
	VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
}

UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPISubObjectIsFilteredOutIfOwnerIsFilterIsChanged)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject->NetHandle;

	// Add subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerHandle, 0, 0);	
	const FNetHandle ServerSubObjectHandle = ServerSubObject->NetHandle;
	
	// Create groups
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	FNetObjectGroupHandle SubObjectGroupHandle = Server->ReplicationSystem->CreateGroup();

	Server->ReplicationSystem->AddGroupFilter(GroupHandle);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, ENetFilterStatus::Allow);

	// Add Object to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	// Verify that we allow replication for both object and subobject
	{
		const bool bExpectsReplicationToBeAllowed = true;
		VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
		VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
	}

	// Disallow group filter
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, ENetFilterStatus::Disallow);

	// Update filters
	Filter();

	// Verify that both object and subobject are filtered out
	{
		const bool bExpectsReplicationToBeAllowed = false;
		VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
		VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
	}
}

UE_NET_TEST_FIXTURE(FTestGroupsFixture, GroupFilterAPISubObjectIsFilteredOutWithSeparateFilter)
{
	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	const FNetHandle ServerHandle = ServerObject->NetHandle;

	// Add subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerHandle, 0, 0);	
	const FNetHandle ServerSubObjectHandle = ServerSubObject->NetHandle;
	
	// Create groups
	FNetObjectGroupHandle GroupHandle = Server->ReplicationSystem->CreateGroup();
	FNetObjectGroupHandle SubObjectGroupHandle = Server->ReplicationSystem->CreateGroup();

	Server->ReplicationSystem->AddGroupFilter(GroupHandle);
	Server->ReplicationSystem->AddGroupFilter(SubObjectGroupHandle);
	Server->ReplicationSystem->SetGroupFilterStatus(GroupHandle, ENetFilterStatus::Allow);

	// Add Object to group
	Server->ReplicationSystem->AddToGroup(GroupHandle, ServerHandle);
	Server->ReplicationSystem->AddToGroup(SubObjectGroupHandle, ServerSubObjectHandle);

	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Update filters
	Filter();

	// Verify that we only allow replication for owner
	{
		VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, true);
		VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, false);
	}

	// Enable SubObjectGroupHandle
	Server->ReplicationSystem->SetGroupFilterStatus(SubObjectGroupHandle, ENetFilterStatus::Allow);

	// Update filters
	Filter();

	// Verify that both object and subobject are enabled
	{
		const bool bExpectsReplicationToBeAllowed = true;
		VerifyObjectFilterStatus(ServerHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
		VerifyObjectFilterStatus(ServerSubObjectHandle, Client->ConnectionIdOnServer, bExpectsReplicationToBeAllowed);
	}
}

}
