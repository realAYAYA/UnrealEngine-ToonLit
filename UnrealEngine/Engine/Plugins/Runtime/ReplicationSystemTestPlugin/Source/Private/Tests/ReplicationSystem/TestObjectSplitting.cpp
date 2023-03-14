// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetObjectBlobHandler.h"
#include "NetBlob/NetBlobTestFixture.h"
#include "NetBlob/MockNetBlob.h"
#include "NetBlob/MockNetObjectAttachment.h"

namespace UE::Net::Private
{

class FSplitObjectTestFixture : public FNetBlobTestFixture
{
	typedef FNetBlobTestFixture Super;

public:
	enum : uint32
	{
		HugeObjectPayloadByteCount = 16384,
		HugeObjectMaxNetTickCountToArrive = 32U,
	};

public:
	FSplitObjectTestFixture()
	{
	}

protected:
	virtual void SetUp() override
	{
		AddNetBlobHandlerDefinitions();
		Super::SetUp();
		RegisterNetBlobHandlers(Server);
	}

	virtual void TearDown() override
	{
		Super::TearDown();
	}

	void RegisterNetBlobHandlers(FReplicationSystemTestNode* Node)
	{
		UReplicationSystem* RepSys = Node->GetReplicationSystem();
		const bool bIsServer = RepSys->IsServer();

		FNetBlobHandlerManager* NetBlobHandlerManager = &RepSys->GetReplicationSystemInternal()->GetNetBlobHandlerManager();

		{
			UMockNetObjectAttachmentHandler* BlobHandler = NewObject<UMockNetObjectAttachmentHandler>();
			const bool bMockNetObjectAttachmentHandlerWasRegistered = RegisterNetBlobHandler(RepSys, BlobHandler);
			check(bMockNetObjectAttachmentHandlerWasRegistered);

			if (bIsServer)
			{
				ServerMockNetObjectAttachmentHandler = TStrongObjectPtr<UMockNetObjectAttachmentHandler>(BlobHandler);
			}
			else
			{
				ClientMockNetObjectAttachmentHandler = TStrongObjectPtr<UMockNetObjectAttachmentHandler>(BlobHandler);
			}
		}
	}

	void SetObjectPayloadByteCount(UTestReplicatedIrisObject* Object, uint32 ByteCount)
	{
		UTestReplicatedIrisDynamicStatePropertyComponent* Component = Object->DynamicStateComponents[0].Get();
		Component->IntArray.SetNumZeroed(ByteCount/4U);
	}

	UTestReplicatedIrisObject* CreateObject(FReplicationSystemTestNode* Node)
	{
		UTestReplicatedIrisObject::FComponents Components;
		Components.DynamicStateComponentCount = 1;
		UTestReplicatedIrisObject* Object = Node->CreateObject(Components);

		return Object;
	}

	UTestReplicatedIrisObject* CreateSubObject(FReplicationSystemTestNode* Node, FNetHandle Parent)
	{
		UTestReplicatedIrisObject::FComponents Components;
		Components.DynamicStateComponentCount = 1;
		UTestReplicatedIrisObject* Object = Node->CreateSubObject(Parent, Components);

		return Object;
	}

	UTestReplicatedIrisObject* CreateHugeObject(FReplicationSystemTestNode* Node)
	{
		UTestReplicatedIrisObject* Object = CreateObject(Node);
		SetObjectPayloadByteCount(Object, HugeObjectPayloadByteCount);
		return Object;
	}

private:
	void AddNetBlobHandlerDefinitions()
	{
		AddMockNetBlobHandlerDefinition();
		const FNetBlobHandlerDefinition NetBlobHandlerDefinitions[] = 
		{
			{TEXT("MockNetObjectAttachmentHandler"),},
			// The proper partial attachment and net object blob handlers are needed for splitting huge objects and attachments.
			{TEXT("PartialNetObjectAttachmentHandler"),}, 
			{TEXT("NetObjectBlobHandler"),}, 
		};
		Super::AddNetBlobHandlerDefinitions(NetBlobHandlerDefinitions, UE_ARRAY_COUNT(NetBlobHandlerDefinitions));
	}


protected:
	TStrongObjectPtr<UMockNetObjectAttachmentHandler> ServerMockNetObjectAttachmentHandler;
	TStrongObjectPtr<UMockNetObjectAttachmentHandler> ClientMockNetObjectAttachmentHandler;
};

// Test that huge object state can be replicated on creation.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitHugeObjectOnCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && ClientObject == nullptr; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
		ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	}
	UE_NET_ASSERT_NE(ClientObject, nullptr);
}

// Test that huge object state can be replicated after an object has been created.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitHugeObjectAfterCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	SetObjectPayloadByteCount(ServerObject, HugeObjectPayloadByteCount);

	// Clear function call status so we can easily verify we get the huge payload.
	UTestReplicatedIrisDynamicStatePropertyComponent* Component = ClientObject->DynamicStateComponents[0].Get();
	Component->CallCounts = {};

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && Component->CallCounts.IntArrayRepNotifyCounter == 0; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
	}
	UE_NET_ASSERT_GT(Component->CallCounts.IntArrayRepNotifyCounter, 0U);
}

// Test that object with huge subobjects can be replicated on creation.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitObjectWithHugeSubObjectsOnCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);
	constexpr uint32 SubObjectCount = 3;
	UTestReplicatedIrisObject* ServerSubObjects[SubObjectCount];
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		ServerSubObjects[SubObjectIt] = CreateSubObject(Server, ServerObject->NetHandle);
		SetObjectPayloadByteCount(ServerSubObjects[SubObjectIt], 4096U);
	}

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && ClientObject == nullptr; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
		ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	}
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify the subobjects made it through as well.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectIt]->NetHandle));
		UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	}
}

// Test that object with lots of subobjects with attachments can be sent on creation.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitObjectWithSubObjectsWithHugeAttachmentsOnCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	constexpr uint32 SubObjectCount = 16;
	constexpr uint32 SubObjectPayloadByteCount = 128U;
	constexpr uint32 SubObjectAttachmentPayloadByteCount = 128U;

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);
	UTestReplicatedIrisObject* ServerSubObjects[SubObjectCount];

	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = CreateSubObject(Server, ServerObject->NetHandle);
		ServerSubObjects[SubObjectIt] = ServerSubObject;
		SetObjectPayloadByteCount(ServerSubObject, SubObjectPayloadByteCount);

		TRefCountPtr<FNetObjectAttachment> Attachment;
		// Alternate between reliable and unreliable attachments
		if ((SubObjectIt & 1U) != 0)
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}
		else
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateUnreliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}

		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && ClientObject == nullptr; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
		ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	}
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify the subobjects made it through.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectIt]->NetHandle));
		UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	}

	// Wait for attachments
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
	}

	// Verify the attachments made it through.
	UMockNetObjectAttachmentHandler::FCallCounts AttachmentCallCounts = ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts();
	UE_NET_ASSERT_EQ(AttachmentCallCounts.OnNetBlobReceived, SubObjectCount);
}

// Test that object with lots of subobjects with attachments can be sent after creation.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitObjectWithSubObjectsWithHugeAttachmentsAfterCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	constexpr uint32 SubObjectCount = 16;
	constexpr uint32 SubObjectPayloadByteCount = 128U;
	constexpr uint32 SubObjectAttachmentPayloadByteCount = 128U;

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);
	UTestReplicatedIrisObject* ServerSubObjects[SubObjectCount];

	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = CreateSubObject(Server, ServerObject->NetHandle);
		ServerSubObjects[SubObjectIt] = ServerSubObject;
	}

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify the subobjects made it through.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectIt]->NetHandle));
		UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	}

	// Now create huge payload and attachments for each subobject.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = ServerSubObjects[SubObjectIt];
		SetObjectPayloadByteCount(ServerSubObject, SubObjectPayloadByteCount);

		TRefCountPtr<FNetObjectAttachment> Attachment;
		// Alternate between reliable and unreliable attachments
		if ((SubObjectIt & 1U) != 0)
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}
		else
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateUnreliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}

		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	bool bHasReceivedHugeState = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasReceivedHugeState; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Assume that if one subobject has received its huge state then all of them have
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectCount - 1U]->NetHandle));
		if (ClientSubObject->DynamicStateComponents[0]->IntArray.Num() > 0)
		{
			bHasReceivedHugeState = true;
		}
	}
	UE_NET_ASSERT_TRUE(bHasReceivedHugeState);

	// Verify the attachments made it through.
	UMockNetObjectAttachmentHandler::FCallCounts AttachmentCallCounts = ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts();
	UE_NET_ASSERT_EQ(AttachmentCallCounts.OnNetBlobReceived, SubObjectCount);
}

// Test that object can have consecutive huge states.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, HugeObjectStateCanBeSentBackToBack)
{
	FReplicationSystemTestClient* Client = CreateClient();
	RegisterNetBlobHandlers(Client);

	constexpr uint32 SubObjectCount = 16;
	constexpr uint32 SubObjectPayloadByteCount = 128U;
	constexpr uint32 SubObjectAttachmentPayloadByteCount = 128U;

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);
	UTestReplicatedIrisObject* ServerSubObjects[SubObjectCount];

	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = CreateSubObject(Server, ServerObject->NetHandle);
		ServerSubObjects[SubObjectIt] = ServerSubObject;
	}

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Verify the subobjects made it through.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectIt]->NetHandle));
		UE_NET_ASSERT_NE(ClientSubObject, nullptr);
	}

	// Now create huge payload and attachments for each subobject.
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = ServerSubObjects[SubObjectIt];
		SetObjectPayloadByteCount(ServerSubObject, SubObjectPayloadByteCount);

		TRefCountPtr<FNetObjectAttachment> Attachment;
		// Alternate between reliable and unreliable attachments
		if ((SubObjectIt & 1U) != 0)
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateReliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}
		else
		{
			Attachment = ServerMockNetObjectAttachmentHandler->CreateUnreliableNetObjectAttachment(SubObjectPayloadByteCount*8U);
		}

		FNetObjectReference AttachmentTarget = FObjectReferenceCache::MakeNetObjectReference(ServerSubObject->NetHandle);
		Server->GetReplicationSystem()->QueueNetObjectAttachment(Client->ConnectionIdOnServer, AttachmentTarget, Attachment);
	}

	bool bHasReceivedHugeState = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasReceivedHugeState; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Assume that if one subobject has received its huge state then all of them have
		const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjects[SubObjectCount - 1U]->NetHandle));
		if (ClientSubObject->DynamicStateComponents[0]->IntArray.Num() > 0)
		{
			bHasReceivedHugeState = true;
		}
	}
	UE_NET_ASSERT_TRUE(bHasReceivedHugeState);

	// Verify the attachments made it through.
	UMockNetObjectAttachmentHandler::FCallCounts AttachmentCallCounts = ClientMockNetObjectAttachmentHandler->GetFunctionCallCounts();
	UE_NET_ASSERT_EQ(AttachmentCallCounts.OnNetBlobReceived, SubObjectCount);
}

// Test that we can send one huge object after another.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitObjectCanBeSentBackToBack)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// Send and deliver packet. This will initiate huge object transfer.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);

	const int OriginalArrayCount = ServerObject->DynamicStateComponents[0]->IntArray.Num();

	// Modify the payload which will cause the same object to require a huge object transfer again.
	ServerObject->DynamicStateComponents[0]->IntArray.Add(1);

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && ClientObject == nullptr; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
		ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	}
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->DynamicStateComponents[0]->IntArray.Num(), OriginalArrayCount);

	bool bHasReceivedSecondHugeState = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasReceivedSecondHugeState; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		if (ClientObject->DynamicStateComponents[0]->IntArray.Num() == OriginalArrayCount + 1)
		{
			bHasReceivedSecondHugeState = true;
		}
	}
	UE_NET_ASSERT_TRUE(bHasReceivedSecondHugeState);
}

// Test that a huge object can be deleted. Currently we assume the object must be created before deleted.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SplitObjectIsDeletedAfterBeingCreated)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);
	const FNetHandle ServerNetHandle = ServerObject->NetHandle;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// As the payload is huge we don't expect the whole payload to arrive the first frame
	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);

	Server->DestroyObject(ServerObject);

	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && ClientObject == nullptr; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();
		ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNetHandle));
	}
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// The object should be destroyed after the next net update.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerNetHandle));
	UE_NET_ASSERT_EQ(ClientObject, nullptr);
}

// Test that a subobject to a huge object can be deleted properly. Currently we assume the huge object payload must have been received before the subobject can be deleted.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, SubObjectToHugeObjectCanBeDeleted)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);
	constexpr uint32 SubObjectCount = 3;
	UTestReplicatedIrisObject* ServerSubObjects[SubObjectCount];
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		ServerSubObjects[SubObjectIt] = CreateSubObject(Server, ServerObject->NetHandle);
	}

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	const UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	const FNetHandle SubObjectNetHandle = ServerSubObjects[0]->NetHandle;
	const UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);

	// Make subobject payloads huge
	for (uint32 SubObjectIt=0; SubObjectIt != SubObjectCount; ++SubObjectIt)
	{
		UTestReplicatedIrisObject* ServerSubObject = ServerSubObjects[SubObjectIt];
		SetObjectPayloadByteCount(ServerSubObjects[SubObjectIt], 4096U);
	}

	// Initiate sending so that we have huge data in flight with the subobject we are going to destroy.
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	Server->DestroyObject(ServerSubObjects[0]);

	bool bHasReceivedHugeState = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasReceivedHugeState; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		// Assume that if one subobject has received its huge state then all of them have
		ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectNetHandle));
		UE_NET_ASSERT_NE(ClientSubObject, nullptr);
		if (ClientSubObject->DynamicStateComponents[0]->IntArray.Num() > 0)
		{
			bHasReceivedHugeState = true;
		}
	}
	UE_NET_ASSERT_TRUE(bHasReceivedHugeState);

	// Now the subobject can safely be destroyed
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(SubObjectNetHandle));
	UE_NET_ASSERT_EQ(ClientSubObject, nullptr);
}

// Test TearOff for new huge object
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TearOffOnCreation)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

	bool bHasHugeObjectBeenCreated = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasHugeObjectBeenCreated; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		if (Client->CreatedObjects.Num() > NumObjectsCreatedOnClientBeforeReplication)
		{
			bHasHugeObjectBeenCreated = true;
		}
	}
	UE_NET_ASSERT_EQ(Client->CreatedObjects.Num(), NumObjectsCreatedOnClientBeforeReplication + 1);

	// Verify that ClientObject is torn-off and that the final state was applied
	UTestReplicatedIrisObject* ClientObjectThatWasTornOff = Cast<UTestReplicatedIrisObject>(Client->CreatedObjects[NumObjectsCreatedOnClientBeforeReplication].Get());
	UE_NET_ASSERT_EQ(ClientObjectThatWasTornOff->DynamicStateComponents[0]->IntArray.Num(), ServerObject->DynamicStateComponents[0]->IntArray.Num());
	UE_NET_ASSERT_EQ(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)), nullptr);
}

// Test TearOff for existing confirmed object during huge object state send
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TearOffCreatedObjectWithHugePayload)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store client object while it can still be found using the server net handle.
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObjectThatWillBeTornOff, nullptr);

	// Set huge payload and TearOff the object
	SetObjectPayloadByteCount(ServerObject, HugeObjectPayloadByteCount);
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	bool bHasReceivedHugeState = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasReceivedHugeState; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		if (ClientObjectThatWillBeTornOff->DynamicStateComponents[0]->IntArray.Num() > 0)
		{
			bHasReceivedHugeState = true;
		}
	}

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)), nullptr);
	UE_NET_ASSERT_EQ(ClientObjectThatWillBeTornOff->DynamicStateComponents[0]->IntArray.Num(), ServerObject->DynamicStateComponents[0]->IntArray.Num());
}

// Test TearOff while huge object state is still sending.
UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TearOffWhileHugeObjectStateIsSending)
{
	FReplicationSystemTestClient* Client = CreateClient();

	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Tear off object before it has been created on the client.
	ServerObject->IntA ^= 1;
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

	bool bHasHugeObjectBeenCreated = false;
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive && !bHasHugeObjectBeenCreated; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, DeliverPacket);
		Server->PostSendUpdate();

		if (Client->CreatedObjects.Num() > NumObjectsCreatedOnClientBeforeReplication)
		{
			bHasHugeObjectBeenCreated = true;
		}
	}
	UE_NET_ASSERT_EQ(Client->CreatedObjects.Num(), NumObjectsCreatedOnClientBeforeReplication + 1);

	// Verify we have the previous state
	const UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_EQ(ClientObjectThatWillBeTornOff->IntA ^ 1, ServerObject->IntA);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)), nullptr);
	UE_NET_ASSERT_EQ(ClientObjectThatWillBeTornOff->IntA, ServerObject->IntA);
}

UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TestCancelPendingDestroyOfHugeObjectDuringWaitOnCreateConfirmationWithoutPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// Write packets
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendUpdate(Client->ConnectionIdOnServer);
		Server->PostSendUpdate();
	}

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Deliver object creation packets
	{
		SIZE_T PacketCount = 0;
		const auto& ConnectionInfo = Server->GetConnectionInfo(Client->ConnectionIdOnServer);
		PacketCount = ConnectionInfo.WrittenPackets.Count();
		for (SIZE_T PacketIt = 0; PacketIt != PacketCount; ++PacketIt)
		{
			Server->DeliverTo(Client, DeliverPacket);
		}
	}

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


UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TestCancelPendingDestroyOfHugeObjectDuringWaitOnCreateConfirmationWithPacketLoss)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = CreateHugeObject(Server);

	// Write packets
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendUpdate(Client->ConnectionIdOnServer);
		Server->PostSendUpdate();
	}

	// Filter out object to cause a PendingDestroy
	Server->GetReplicationSystem()->AddToGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Remove object from filter to cause object to end up in CancelPendingDestroy
	Server->GetReplicationSystem()->RemoveFromGroup(NotReplicatedNetObjectGroupHandle, ServerObject->NetHandle);
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	// Cause packet loss on object creation
	{
		SIZE_T PacketCount = 0;
		const auto& ConnectionInfo = Server->GetConnectionInfo(Client->ConnectionIdOnServer);
		PacketCount = ConnectionInfo.WrittenPackets.Count();
		for (SIZE_T PacketIt = 0; PacketIt != PacketCount; ++PacketIt)
		{
			Server->DeliverTo(Client, DoNotDeliverPacket);
		}
	}

	// Write and send packets and verify object is created
	{
		const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

		for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive; ++RetryIt)
		{
			Server->PreSendUpdate();
			Server->SendAndDeliverTo(Client, DeliverPacket);
			Server->PostSendUpdate();

			if (Client->CreatedObjects.Num() > NumObjectsCreatedOnClientBeforeReplication)
			{
				break;
			}
		}
	}

	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FSplitObjectTestFixture, TestCancelPendingDestroyDuringHugeObjectStateUpdate)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = CreateObject(Server);

	// Write and send packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Force huge object state
	SetObjectPayloadByteCount(ServerObject, HugeObjectPayloadByteCount);

	// Write packets
	for (uint32 RetryIt = 0; RetryIt != HugeObjectMaxNetTickCountToArrive; ++RetryIt)
	{
		Server->PreSendUpdate();
		Server->SendUpdate(Client->ConnectionIdOnServer);
		Server->PostSendUpdate();
	}

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

	// Deliver huge state
	{
		SIZE_T PacketCount = 0;
		const auto& ConnectionInfo = Server->GetConnectionInfo(Client->ConnectionIdOnServer);
		PacketCount = ConnectionInfo.WrittenPackets.Count();
		for (SIZE_T PacketIt = 0; PacketIt != PacketCount; ++PacketIt)
		{
			Server->DeliverTo(Client, DeliverPacket);
		}
	}

	// Deliver latest state
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
}

}
