// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"


namespace UE::Net::Private
{

class FTestCancelPendingDestroyFixture : public FReplicationSystemServerClientTestFixture
{
};

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingDestroyDuringWaitOnCreateConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write packet
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver original packet
	Server->DeliverTo(Client, DeliverPacket);

	// Verify that the object now exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Modify a property on the object and make sure it's replicated as the object should now be confirmed created
	ServerObject->IntA ^= 1;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingDestroyDuringWaitOnCreateConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write packet
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on object creation
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Write and send packet and verify object is created
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingDestroyDuringCreated)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object now also exists on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Modify a property on the object and make sure it's replicated as the object should still be created
	ServerObject->IntA ^= 1;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingDestroyDuringWaitOnDestroyConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause object to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on the object destroy packet
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify that object still exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Modify a property on the object and make sure it's replicated as the object should still be created
	ServerObject->IntA ^= 1;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingDestroyDuringWaitOnDestroyConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause object to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy, also modify a property 
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	ServerObject->IntA ^= 1;
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver object destroy packet
	Server->DeliverTo(Client, DeliverPacket);

	// The destroy was delivered so the client object should not exist
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
}

// Tests for canceling a canceled pending destroy
UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingDestroyDuringWaitOnCreateConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write packet
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the object to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	Server->DeliverTo(Client, DeliverPacket);

	// Verify that the object now exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object was destroyed on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingDestroyDuringWaitOnCreateConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write packet
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the object to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on object creation
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify that the object doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object still doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingDestroyDuringCreated)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object now also exists on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the object to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object was destroyed on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingDestroyDuringWaitOnDestroyConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause object to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the object to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on the object destroy packet
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify that the object still exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object was destroyed on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingDestroyDuringWaitOnDestroyConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause object to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the object to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver destroy packet
	Server->DeliverTo(Client, DeliverPacket);

	// Verify that the object doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the object still doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}


// Subobject tests
UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingSubObjectDestroyDuringCreated)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object with subobject on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the subobject exists on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Filter out subobject to cause a SubObjectPendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove subobject from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Modify a property on the subobject and make sure it's replicated as the subobject should still exist
	ServerSubObject->IntA ^= 1;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, ServerSubObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingSubObjectDestroyDuringWaitOnDestroyConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause subobject to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove subobject from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on the subobject destroy packet
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify that object still exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Modify a property on the subobject and make sure it's replicated as the subobject should still exist
	ServerSubObject->IntA ^= 1;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, ServerSubObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelPendingSubObjectDestroyDuringWaitOnDestroyConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause subobject to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy, also modify a property 
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	ServerSubObject->IntA ^= 1;
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver subobject destroy packet
	Server->DeliverTo(Client, DeliverPacket);

	// The destroy was delivered so the client subobject should not exist
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Client subobject should have been created with the latest state
	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, ServerSubObject->IntA);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingSubObjectDestroyDuringCreated)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the subobject now also exists on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Filter out object to cause a SubObjectPendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the subobject to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the subobject was destroyed on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingSubObjectDestroyDuringWaitOnDestroyConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out object and write packet to cause subobject to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove object from filter to cause subobject to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the subobject to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on the subobject destroy packet
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify that the object still exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the subobject was destroyed on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingCancelPendingSubObjectDestroyDuringWaitOnDestroyConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out subobject and write packet to cause subobject to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove subobject from filter to cause subobject to end up in CancelPendingDestroy...
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// ... and cancel that thought! We want the subobject to be destroyed after all.
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver destroy packet
	Server->DeliverTo(Client, DeliverPacket);

	// Verify that the subobject doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the subobject still doesn't exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FTestCancelPendingDestroyFixture, TestCancelingSubObjectDestroyAfterParentIsMarkedForDestroy)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject1 = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());
	UTestReplicatedIrisObject* ServerSubObject2 = Server->CreateSubObject(ServerObject->NetHandle, UTestReplicatedIrisObject::FComponents());

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Filter out subobject and write packet to cause subobject to end up in WaitOnDestroyConfirmation
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject2->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Filter out parent and write packet
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->SendUpdate(Client->ConnectionIdOnServer);
	Server->PostSendUpdate();

	// Remove subobject from filter to cause subobject to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerSubObject2->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause subobject destroy packet to be lost
	Server->DeliverTo(Client, DoNotDeliverPacket);

	// Verify all objects and subobjects exist on the client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetHandle), nullptr);

	// Deliver owner destroy packet
	Server->DeliverTo(Client, DeliverPacket);

	// Verify that no objects exist on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetHandle), nullptr);

	// Write and send a packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that all objects are still gone on the client
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetHandle), nullptr);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetHandle), nullptr);
}

// See TestObjectSplitting.cpp for cancel pending destroy on huge object

}
