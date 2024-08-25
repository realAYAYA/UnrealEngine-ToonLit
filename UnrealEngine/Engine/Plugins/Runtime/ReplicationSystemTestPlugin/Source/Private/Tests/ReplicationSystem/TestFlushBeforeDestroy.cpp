// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "NetBlob/PartialNetBlobTestFixture.h"
#include "NetBlob/MockNetBlob.h"

namespace UE::Net::Private
{

class FTestFlushBeforeDestroyFixture : public FPartialNetBlobTestFixture
{
};

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentFlushedBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Destroy object, on server
	Server->DestroyObject(ServerObject);

	// Deliver a packet, this should flush the object and deliver the attachment
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the attachment has been received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Deliver a packet. Should destroy the object on the client unless that was done
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentFlushedWithDataInflightBeforeDestroy)
{

	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Setup a situation where we have reliable data in flight when the object is destroyed
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Destroy object, on server
	Server->DestroyObject(ServerObject);

	// Drop the data and notify server
	Server->DeliverTo(Client, false);

	// Deliver a packet, this should flush the object and deliver the attachment
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the attachment has been received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Deliver a packet. Should destroy the object on the client unless that was done
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

// This test exercises what was a bad case where we was posting RPC:s to a not yet confirmed objects which also was marked for destroy
// This put the replication system in a state where it wrote data that the client could not process.
// Currently we will just drop the data if the initial create packet is lost as we cannot yet send creation info for
// destroyed objects.
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentFlushedWithPendingCreationLostBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;

	// Setup a situation where we have creation info in flight when the object is destroyed

	// Send creation info
	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("WaitOnCreateConfirmation"));
	Server->PostSendUpdate();

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Destroy object, on server
	Server->DestroyObject(ServerObject);

	// Previously this would issue a flush and send the attachment data even though creation was not yet confirmed leading to a client disconnect..
	Server->PreSendUpdate();
	const bool bDataWasSent = Server->SendTo(Client, TEXT("State should still be WaitOnCreateConfirmation"));
	Server->PostSendUpdate();

	// We do not expect any data to be in this packet.
	UE_NET_ASSERT_FALSE(bDataWasSent);

	// Drop the data and notify server
	Server->DeliverTo(Client, false);

	// Deliver data
	if (bDataWasSent)
	{
		// Caused bitstream error on client.
		Server->DeliverTo(Client, true);
	}

	// Update to drive the last transition which we expect to be
	Server->UpdateAndSend({Client});

	// Verify that the attachment has not received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 0U);

	// Verify that object does not exist
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentFlushedWithPendingCreationInflightBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;

	// Setup a situation where we have creation info in flight when the object is destroyed

	// Send creation info
	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("WaitOnCreateConfirmation"));
	Server->PostSendUpdate();

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Destroy object, on server
	Server->DestroyObject(ServerObject);

	// Previously this would issue a flush and send data before creation is confirmed.
	Server->PreSendUpdate();
	const bool bDataWasSentInError = Server->SendTo(Client, TEXT("State should still be WaitOnCreateConfirmation"));
	Server->PostSendUpdate();
	
	// We do not expect any data to be in this packet.
	UE_NET_ASSERT_FALSE(bDataWasSentInError);

	// Deliver the packet with CreationInfo
	Server->DeliverTo(Client, true);

	// Deliver data if we sent data.
	if (bDataWasSentInError)
	{
		// Caused bitstream error on client.
		Server->DeliverTo(Client, true);
	}

	// Expected to write the attachment
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket, TEXT("WaitOnFlush"));
	Server->PostSendUpdate();

	// Verify that the attachment has not received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Expected to destroy the object
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket, TEXT("Destroy"));
	Server->PostSendUpdate();

	// Verify that object does not exist
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentSubObjectFlushedBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ObjectHandle, 0, 0);
	FNetRefHandle SubObjectHandle = ServerSubObject->NetRefHandle;
	
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Destroy sub object, on server
	Server->DestroyObject(ServerSubObject);

	// Deliver a packet, this should flush the object and deliver the attachment
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the attachment has been received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Deliver a packet. Should destroy the object on the client unless that was done
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestReliableAttachmentSubObjectFlushedBeforeDestroyIfOwnerIsDestroyed)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ObjectHandle, 0, 0);
	FNetRefHandle SubObjectHandle = ServerSubObject->NetRefHandle;
	
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Destroy object which should flush subobject and then destroy both subobject and object
	Server->DestroyObject(ServerObject);

	// Deliver a packet, this should flush the object and deliver the attachment
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that the attachment has been received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Deliver a packet. Should destroy the object on the client unless that was done
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that both object and subobject are destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle) == nullptr);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestStateFlushedBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle), nullptr);

	// Modify state
	ServerObject->IntA = 3;

	// Destroy object with flush flag which should flush the state before destroying the object
	Server->DestroyObject(ServerObject, EEndReplicationFlags::Destroy | EEndReplicationFlags::Flush);

	// Deliver a packet, this should flush the object and deliver the last state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle));

	// Verify that object is created
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify that we got the expected state
	UE_NET_ASSERT_EQ(ClientObject->IntA, 3);
	
	// Deliver a packet.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestStateInFlightFlushedBeforeDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle), nullptr);

	// Modify state
	ServerObject->IntA = 3;

	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Modify state
	ServerObject->IntB = 4;

	// Destroy object with flush flag which should flush the state before destroying the object
	Server->DestroyObject(ServerObject, EEndReplicationFlags::Destroy | EEndReplicationFlags::Flush);

	// Drop the data we had in flight and notify server
	Server->DeliverTo(Client, false);

	// Deliver a packet, this should flush the object and deliver the complete last state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle));

	// Verify that object is created
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify that we got the expected state
	UE_NET_ASSERT_EQ(ClientObject->IntA, 3);
	UE_NET_ASSERT_EQ(ClientObject->IntB, 4);

	// Deliver a packet. Should destroy the object on the client unless that was done
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that object is destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestSubObjectStateFlushedBeforeOwnerDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ObjectHandle, 0, 0);
	FNetRefHandle SubObjectHandle = ServerSubObject->NetRefHandle;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that objects is created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle), nullptr);

	// Modify state
	ServerSubObject->IntA = 3;

	// Destroy object with flush flag which should flush the state including before destroying the object
	Server->DestroyObject(ServerObject, EEndReplicationFlags::Destroy | EEndReplicationFlags::Flush);

	// Deliver a packet, this should flush the object and deliver the last state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle));
	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle));

	// Verify that objects is created
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);

	// Verify that we got the expected state
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, 3);
	
	// Deliver a packet.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that both objects are destroyed
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle) == nullptr);
}

UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestSubObjectStateFlushedBeforeSubObjectDestroy)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);
	FNetRefHandle ObjectHandle = ServerObject->NetRefHandle;
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ObjectHandle, 0, 0);
	FNetRefHandle SubObjectHandle = ServerSubObject->NetRefHandle;

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that objects is created
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle), nullptr);

	// Modify state on SubObject
	ServerSubObject->IntA = 3;

	// Destroy object with flush flag which should flush the state including before destroying the object
	Server->DestroyObject(ServerSubObject, EEndReplicationFlags::Destroy | EEndReplicationFlags::Flush);

	// Deliver a packet, this should flush the object and deliver the last state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle));
	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle));

	// Verify that objects are created
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);

	// Verify that we got the expected state
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, 3);
	
	// Deliver a packet.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify subobject is destroyed now that last state was confirmed flushed while the main object still is around
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ObjectHandle) == nullptr);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectHandle) == nullptr);
}

// Test TearOff for existing confirmed object
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestTearOffNewObjectWithReliableAttachment)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object got created
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);

	// Verify that ClientObject got final state and that the attachement was received
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject now has been teared off
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) == nullptr);
}

// Test TearOff for existing confirmed object
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestTearOffExistingObjectWithReliableAttachment)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject got final state and that the attachement was received
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);

	// Verify that ClientObject still is around (from a network perspective)
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) != nullptr);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject now has been teared off
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
}

// Test TearOff and SubObjects, SubObjects must apply state?
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestImmediateTearOffExistingObjectWithSubObjectWithReliableAttachment)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	UTestReplicatedIrisObject* ClientSubObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientSubObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Modify the value of subobject only
	ServerSubObject->IntA = 2;

	// Create attachment
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// TearOff the object using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject got final state and that the attachement was received
	UE_NET_ASSERT_EQ(ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts().OnNetBlobReceived, 1U);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
}

// Test to recreate a very specific bug where owner being torn-off has in flight rpc requiring a flush
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestImmediateTearOffWithSubObjectAndInFlightAttachmentsAndPacketLoss)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true, TEXT("Create Objects"));
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	UTestReplicatedIrisObject* ClientSubObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientSubObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Modify the value of object only
	ServerObject->IntA = 2;

	// Create attachment to force flush behavior by having a rpc in flight
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("State data + Attachment"));
	Server->PostSendUpdate();

	// Modify the value of object only
	++ServerObject->IntA ;

	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("State data"));
	Server->PostSendUpdate();

	// TearOff the object using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("Tear off"));
	Server->PostSendUpdate();

	// Deliver packet to drive PendingTearOff -> WaitOnFlush
	Server->DeliverTo(Client, true);

	// Notify that we dropped tear off data
	Server->DeliverTo(Client, false);

	// This earlier caused an unwanted state transition
	Server->PreSendUpdate();
	Server->SendTo(Client, TEXT("Packet after tearoff"));
	Server->PostSendUpdate();

	// Drop the packet containing the original tear-off
	Server->DeliverTo(Client, false);

	// Deliver a packet
	Server->DeliverTo(Client, true);

	// This should contain resend of lost state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true, TEXT("Resending tearoff"));
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
}

// Test to recreate a path where we cancel destroy for object pending flush
UE_NET_TEST_FIXTURE(FTestFlushBeforeDestroyFixture, TestCancelPendingDestroyWaitOnFlushDoesNotMissChanges)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client0 = CreateClient();
	FReplicationSystemTestClient* Client1 = CreateClient();

	RegisterNetBlobHandlers(Client0);
	RegisterNetBlobHandlers(Client1);

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client0, true, TEXT("Create Objects"));
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client0->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientObject != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);

	// Create attachment to force flush behavior by having a rpc in flight
	{
		constexpr uint32 PayloadBitCount = 24;
		const TRefCountPtr<FNetObjectAttachment>& Attachment = MockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(PayloadBitCount);
		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerObject->NetRefHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client0->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	Server->PreSendUpdate();
	Server->SendTo(Client0, TEXT("Attachment"));
	Server->PostSendUpdate();

	// Filter out object to cause a flush for Client0
	FNetObjectGroupHandle ExclusionGroupHandle = Server->ReplicationSystem->CreateGroup();
	Server->ReplicationSystem->AddToGroup(ExclusionGroupHandle, ServerObject->NetRefHandle);
	Server->ReplicationSystem->AddExclusionFilterGroup(ExclusionGroupHandle);

	Server->ReplicationSystem->SetGroupFilterStatus(ExclusionGroupHandle, Client0->ConnectionIdOnServer, ENetFilterStatus::Disallow);
	Server->ReplicationSystem->SetGroupFilterStatus(ExclusionGroupHandle, Client1->ConnectionIdOnServer, ENetFilterStatus::Allow);

	Server->PreSendUpdate();
	Server->SendTo(Client0, TEXT("Out of scope"));
	Server->PostSendUpdate();

	// Modify the value of object only
	++ServerObject->IntA ;

	// Trigger poll + propagate of state
	Server->PreSendUpdate();
	Server->PostSendUpdate();
	
	// Trigger WaitOnFlush -> Created
	Server->ReplicationSystem->SetGroupFilterStatus(ExclusionGroupHandle, Client0->ConnectionIdOnServer, ENetFilterStatus::Allow);

	// Drop some packets to stay in state
	Server->DeliverTo(Client0, false);
	Server->DeliverTo(Client0, false);

	// Do a normal update, should send state changed that occurred while we where in pending flush state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client0, DeliverPacket, TEXT("Expected state"));
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);
}

}
