// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"

namespace UE::Net::Private
{

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySingleObject)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0,0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handle now also exists on client
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);

	// Destroy the spawned object on server
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDroppedDestroyed)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0,0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handle now also exists on client
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);

	// Destroy the spawned object on server
	Server->DestroyObject(ServerObject);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDestroyWhilePendingCreate)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0,0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);

	Server->PostSendUpdate();

	// Destroy
	Server->DestroyObject(ServerObject);

	// Verify that created server handle does not exists on client
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDestroyWhileWaitingOnCreate)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0,0);

	// Send packet with create
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Destroy while we are waiting for confirmation
	Server->DestroyObject(ServerObject);

	// Send packet with destroy
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Drop and report packet with create as lost
	Server->DeliverTo(Client, false);

	// deliver packet with destroy
	Server->DeliverTo(Client, true);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDestroyWithDataInFlight)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);
	Server->GetReplicationSystem()->SetStaticPriority(ServerObject->NetHandle, 1.f);

	// Send packet with create
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Modify some data to mark object dirty
	ServerObject->IntA = 13;

	// Trigger send 
	Server->PreSendUpdate();
	UE_NET_ASSERT_TRUE(Server->SendTo(Client));
	Server->PostSendUpdate();

	// Destroy while we are waiting for confirmation
	Server->DestroyObject(ServerObject);

	// Send packet with destroy
	Server->PreSendUpdate();
	UE_NET_ASSERT_TRUE(Server->SendTo(Client));
	Server->PostSendUpdate();

	// Drop and report packet with create as lost
	Server->DeliverTo(Client, false);

	// deliver packet with destroy
	Server->DeliverTo(Client, true);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObject)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle) != nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroyMultipleSubObjects)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Spawn second object on server as a subobject
	ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that second subobject replicated properly to server
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Destroy the spawned object on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectAndDestroyOwner)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Destroy owner after spawned subobject on server
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectAndDestroyOwnerWithDataInFlight)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Destroy owner after we spawned subobject on server
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectWithLostData)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectPendingCreateConfirmation)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	uint32 NumUnAcknowledgedPackets = 0;
	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// We have no data to send but we want to tick ReplicationSystem to capture state change
	Server->PreSendUpdate();
	UE_NET_ASSERT_FALSE(Server->SendTo(Client));
	Server->PostSendUpdate();

	Server->DeliverTo(Client, false);
	// As we did not send any data we do not have anything to deliver
	//Server->DeliverTo(Client, true);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestSubObjectDefaultReplicationOrder)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn some subobjects
	UReplicatedSubObjectOrderObject* ServerSubObject0 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle);
	UReplicatedSubObjectOrderObject* ServerSubObject1 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle);
	UReplicatedSubObjectOrderObject* ServerSubObject2 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle);

	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientSubObject0 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject0->NetHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject1 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject2 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetHandle));

	UE_NET_ASSERT_NE(ClientSubObject0, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject1, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject2, nullptr);

	// Verify that they have replicated in expected order
	UE_NET_ASSERT_EQ(ClientSubObject0->LastRepOrderCounter, 1U);
	UE_NET_ASSERT_GT(ClientSubObject1->LastRepOrderCounter, ClientSubObject0->LastRepOrderCounter);
	UE_NET_ASSERT_GT(ClientSubObject2->LastRepOrderCounter, ClientSubObject1->LastRepOrderCounter);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestSubObjectSpecfiedReplicationOrder)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn some subobjects
	UReplicatedSubObjectOrderObject* ServerSubObject0 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle);
	// Specify Subobject1 to replicate with SubObject0, which means that it will replicate before Subobjet0 is replicated
	UReplicatedSubObjectOrderObject* ServerSubObject1 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle, ServerSubObject0->NetHandle, UReplicationBridge::ESubObjectInsertionOrder::ReplicateWith);
	// Specify SubObect 2 to replicate with no specific order (it will be added to the owner and thus replicate last)
	UReplicatedSubObjectOrderObject* ServerSubObject2 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetHandle, ServerSubObject1->NetHandle, UReplicationBridge::ESubObjectInsertionOrder::None);

	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientSubObject0 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject0->NetHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject1 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject2 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetHandle));

	UE_NET_ASSERT_NE(ClientSubObject0, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject1, nullptr);
	UE_NET_ASSERT_NE(ClientSubObject2, nullptr);

	// Verify that they have replicated in expected order setup earlier
	UE_NET_ASSERT_EQ(ClientSubObject1->LastRepOrderCounter, 1U);
	UE_NET_ASSERT_EQ(ClientSubObject0->LastRepOrderCounter, 2U);
	UE_NET_ASSERT_EQ(ClientSubObject2->LastRepOrderCounter, 3U);
}

class FTestNetTokensFixture : public FReplicationSystemServerClientTestFixture
{
public:
	FStringTokenStore* ServerStringTokenStore = nullptr;
	FStringTokenStore* ClientStringTokenStore = nullptr;
	FReplicationSystemTestClient* Client = nullptr;
	const FNetTokenStoreState* ClientRemoteNetTokenState;

	FNetToken CreateAndExportNetToken(const FString& TokenString)
	{
		FNetToken Token = ServerStringTokenStore->GetOrCreateToken(TokenString);
		Server->GetConnectionInfo(Client->ConnectionIdOnServer).NetTokenDataStream->AddNetTokenForExplicitExport(Token);

		return Token;
	}

	virtual void SetUp() override
	{
		FReplicationSystemServerClientTestFixture::SetUp();

		Client = CreateClient();

		ServerStringTokenStore = Server->GetReplicationSystem()->GetStringTokenStore();
		ClientStringTokenStore = Client->GetReplicationSystem()->GetStringTokenStore();
		ClientRemoteNetTokenState = Client->GetConnectionInfo(Client->LocalConnectionId).NetTokenDataStream->GetRemoteNetTokenStoreState();
	}
};



UE_NET_TEST_FIXTURE(FTestNetTokensFixture, NetToken)
{
	// Create token
	FString TokenStringA(TEXT("MyStringToken"));
	FNetToken StringTokenA = CreateAndExportNetToken(TokenStringA);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Verify that we cannot resolve the token on the client
	UE_NET_ASSERT_NE(TokenStringA, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that we can resolve the token on the client
	UE_NET_ASSERT_EQ(TokenStringA, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
}

UE_NET_TEST_FIXTURE(FTestNetTokensFixture, NetTokenResendWithFullPacket)
{
	// Create token
	FString TokenStringA(TEXT("MyStringToken"));
	FNetToken StringTokenA = CreateAndExportNetToken(TokenStringA);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Limit packet size
	Server->SetMaxSendPacketSize(128U);

	// Create a new token that will not fit in the packet and only fit the resend data
	FString TokenStringB(TEXT("MyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongString"));
	FNetToken StringTokenB = CreateAndExportNetToken(TokenStringB);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that we can resolve the token first token on the client even though second one should not fit
	UE_NET_ASSERT_EQ(TokenStringA, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_NE(TokenStringB, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));

	// Restore packet size and make sure that we get the second token through
	Server->SetMaxSendPacketSize(1024U);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(TokenStringB, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));
}

UE_NET_TEST_FIXTURE(FTestNetTokensFixture, NetTokenResendWithFullPacketAfterFirstResend)
{
	// Create token
	FString TestStringA(TEXT("MyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongString"));
	FNetToken StringTokenA = CreateAndExportNetToken(TestStringA);	

	// Send and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Create a new token that will not fit in the packet and only fit the resend data
	FString TestStringB(TEXT("MyOtherLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongStringMyLongString"));
	FNetToken StringTokenB = CreateAndExportNetToken(TestStringB);

	// Send and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	Server->DeliverTo(Client, false);
	Server->DeliverTo(Client, false);

	// Verify that tokens has not been received
	UE_NET_ASSERT_NE(TestStringA, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_NE(TestStringB, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));

	// Send and deliver packet which now should contain two entries in the resend queue
	Server->SetMaxSendPacketSize(1024);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that we can resolve the token
	UE_NET_ASSERT_EQ(TestStringA, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_EQ(TestStringB, FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));
}

UE_NET_TEST_FIXTURE(FTestNetTokensFixture, NetTokenSequenceTest)
{
	const FString TestStrings[] = {
		FString(TEXT("TokenA")),
		FString(TEXT("TokenB")),
		FString(TEXT("TokenC")),
		FString(TEXT("TokenD")),
		FString(TEXT("TokenE")),
		FString(TEXT("TokenF")),
	};

	const uint32 TokenCount = UE_ARRAY_COUNT(TestStrings);

	// Create token
	FNetToken StringTokenA = CreateAndExportNetToken(TestStrings[0]);
	FNetToken StringTokenB = CreateAndExportNetToken(TestStrings[1]);

	// Send packet
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Create token
	FNetToken StringTokenC = CreateAndExportNetToken(TestStrings[2]);

	// Create token
	FNetToken StringTokenD = CreateAndExportNetToken(TestStrings[3]);

	// Send packet
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Drop packet 
	Server->DeliverTo(Client, false);

	// Deliver packet 
	Server->DeliverTo(Client, true);

	// Create local tokens
	ClientStringTokenStore->GetOrCreateToken(TEXT("LocalTokenA"));
	ClientStringTokenStore->GetOrCreateToken(TEXT("LocalTokenB"));

	// Send packet with resend data
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(TestStrings[0], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_EQ(TestStrings[1], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_EQ(TestStrings[2], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenC, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_EQ(TestStrings[3], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenD, *ClientRemoteNetTokenState)));
}

UE_NET_TEST_FIXTURE(FTestNetTokensFixture, NetTokenResendAndDataInSamePacketTest)
{
	const FString TestStrings[] = {
		FString(TEXT("TokenA")),
		FString(TEXT("TokenB")),
	};

	const uint32 TokenCount = UE_ARRAY_COUNT(TestStrings);


	// Create token
	FNetToken StringTokenA = CreateAndExportNetToken(TestStrings[0]);

	// Send packet
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// drop data
	Server->DeliverTo(Client, false);

	// Create token
	FNetToken StringTokenB = CreateAndExportNetToken(TestStrings[1]);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(TestStrings[0], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenA, *ClientRemoteNetTokenState)));
	UE_NET_ASSERT_EQ(TestStrings[1], FString(ClientStringTokenStore->ResolveRemoteToken(StringTokenB, *ClientRemoteNetTokenState)));
}


UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, AddRemoveFromConnectionScopeTest)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Add to group
	FNetObjectGroupHandle Group = ReplicationSystem->CreateGroup();
	ReplicationSystem->AddToGroup(Group, ServerObject->NetHandle);

	ReplicationSystem->AddGroupFilter(Group);
	ReplicationSystem->SetGroupFilterStatus(Group, ENetFilterStatus::Allow);

	// Start replicating object
	
	// Send packet
	// Expected state to be WaitOnCreateConfirmation
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Make sure we have data in flight
	++ServerObject->IntA;

	// Disallow group to trigger state change from PendingCreateConfirmation->PendingDestroy
	ReplicationSystem->SetGroupFilterStatus(Group, ENetFilterStatus::Disallow);	

	// Send packet
	// Expected state to be WaitOnDestroyConfirmation
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Allow group to trigger state to ensure that we restart replication since we have not actually created the object on the client
	ReplicationSystem->SetGroupFilterStatus(Group, ENetFilterStatus::Allow);

	// Expect client to create object
	Server->DeliverTo(Client, true);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);

	// Expect client to destroy object and server to move to Destroyed state
	Server->DeliverTo(Client, true);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) == nullptr);

	// Trigger replication
	++ServerObject->IntA;

	// Send packet
	// Invalid -> PendingCreate
	// PendingCreate->WaitOnCreateConfirmation
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that the object got created again
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestNetTemporary)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);
	UTestReplicatedIrisObject* ServerObject1 = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerObject1->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that client has received the data
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);
	}

	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject1->IntA, ClientObject->IntA);

	}

	// Mark the object as a net temporary
	ReplicationSystem->SetIsNetTemporary(ServerObject->NetHandle);

	// Modify the value
	ServerObject->IntA = 2;
	ServerObject1->IntA = 2;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that client has not received the data for changed temporary
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_NE(ServerObject->IntA, ClientObject->IntA);
	}

	// Verify that client has received the data for normal object
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject1->IntA, ClientObject->IntA);
	}

	// Test Late join
	// Add a client
	FReplicationSystemTestClient* Client2 = CreateClient();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client2, true);
	Server->PostSendUpdate();

	// We should now have the latest state for both objects
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client2->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);
	}

	// Verify that client has received the data for normal object
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client2->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject1->IntA, ClientObject->IntA);
	}
}

// Tests for TearOff

// Test TearOff for existing confirmed object
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffExistingObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

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

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);
}

// Test TearOff for new object
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffOnNewlyCreatedObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// We should not have any created objects
	const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Set state
	ServerObject->IntA = 1;

	// TearOff the object before first replication
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Client should have created a object
	UE_NET_ASSERT_EQ(NumObjectsCreatedOnClientBeforeReplication + 1, Client->CreatedObjects.Num());

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);

	// We should be able to get the object from the created objects array to validate the state
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->CreatedObjects[NumObjectsCreatedOnClientBeforeReplication].Get());

	// Verify that we replicated the expected state
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
}

// Test TearOff resend for existing confirmed object with no state changes
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffResendForExistingObjectWithoutDirtyState)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

	UE_NET_ASSERT_NE(ClientObjectThatWillBeTornOff, nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and do not deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DoNotDeliverPacket);
	Server->PostSendUpdate();

	// The ClientObject should still be found using the NetHandle
	UE_NET_ASSERT_NE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)), nullptr);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off
	UE_NET_ASSERT_EQ(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)), nullptr);
}

// Test TearOff for new object and resend (should not work or is this what we want?)
#if 0 // Until we either keep object around but out of scope, or cache creation info + deps
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffImmediateOnNewlyCreatedObjectResend)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// We should not have any created objects
	const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Set state
	ServerObject->IntA = 1;

	// TearOff the object before first replication
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Client should have created a object
	UE_NET_ASSERT_EQ(NumObjectsCreatedOnClientBeforeReplication + 1, Client->CreatedObjects.Num());

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);

	// We should be able to get the object from the created objects array to validate the state
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->CreatedObjects[NumObjectsCreatedOnClientBeforeReplication].Get());

	// Verify that we replicated the expected state
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
}
#endif


UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDefferedTearOffOnNewlyCreatedObjectResend)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// We should not have any created objects
	const int32 NumObjectsCreatedOnClientBeforeReplication = Client->CreatedObjects.Num();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Set state
	ServerObject->IntA = 1;

	// TearOff the object before first replication
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();
	
	// End replication and destroy object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::Destroy);

	// Client should have created a object
	UE_NET_ASSERT_EQ(NumObjectsCreatedOnClientBeforeReplication + 1, Client->CreatedObjects.Num());

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);

	// We should be able to get the object from the created objects array to validate the state
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->CreatedObjects[NumObjectsCreatedOnClientBeforeReplication].Get());

	// Verify that we replicated the expected state
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
}

// Test TearOff for existing not yet confirmed object
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffObjectPendingCreateConfirmation)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send packet to get put the object in flight
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Deliver Object (should now be created)
	Server->DeliverTo(Client, true);

	// Store Pointer to object and verify initial state
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is TornOFf and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);
}

// Test TearOff for existing object pending destroy (should do nothing)
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffExistingObjectPendingDestroy)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// Mark the object for destroy
	Server->ReplicationBridge->EndReplication(ServerObject);

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is not tornOff and that the final state was not applied as we issued tearoff after ending replication
	UE_NET_ASSERT_NE(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);
}

// Test TearOff resend 
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffResend)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// TearOff the object
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetHandle);

	// Send and deliver packet, in this case the packet containing 2 was lost, but, we did not know that when we 
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Object should now be torn-off, so it should not copy the latest state
	ServerObject->IntA = 3;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the expected final state was applied
	UE_NET_ASSERT_EQ(2, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);
}

// Test TearOff does not pickup statechanges after tear off
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTornedOffObjectDoesNotCopyStateChanges)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Trigger replication
	ServerObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to object 
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// TearOff the object
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetHandle);

	// Send and drop packet containing the value 2
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Object should now be torn-off, so it should not copy the latest state but instead resend the last copied state (2) along with the tear-off
	ServerObject->IntA = 3;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is TornOFf and that the expected final state was applied
	UE_NET_ASSERT_EQ(2, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle)) == nullptr);
}

// Test TearOff and SubObjects, SubObjects must apply state?
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestImmediateTearOffExistingObjectWithSubObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	UTestReplicatedIrisObject* ClientSubObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientSubObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Modify the value of subobject only
	ServerSubObject->IntA = 2;

	// TearOff the object using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied to subObject 
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle)) == nullptr);
}

// Test TearOff and SubObjects, SubObjects must apply state?
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestImmediateTearOffExistingObjectWithSubObjectDroppedData)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	UTestReplicatedIrisObject* ClientSubObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientSubObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Modify the value of subobject only
	ServerSubObject->IntA = 2;

	// TearOff the object using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and do not deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied to subObject 
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle)) == nullptr);
}

// Test TearOff and SubObjects, SubObjects must apply state?
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffNextUpdateExistingObjectWithSubObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	UTestReplicatedIrisObject* ClientSubObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle));
	UE_NET_ASSERT_TRUE(ClientSubObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);

	// Modify the value of subobject only
	ServerSubObject->IntA = 2;

	// TearOff the object using immediate tear-off
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied to subObject 
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetHandle)) == nullptr);
}

// Test TearOff and destroy of SubObjects that are still pending create/tearoff
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffNextUpdateExistingObjectWithSubObjectPendingCreation)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	FNetHandleManager* NetHandleManager = &ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	const FInternalNetHandle ServerObjectInternalIndex = NetHandleManager->GetInternalIndex(ServerObject->NetHandle);
	const FInternalNetHandle SubObjectObjectInternalIndex = NetHandleManager->GetInternalIndex(ServerSubObject->NetHandle);

	// Trigger presend without send to add the objects to scope
	Server->PreSendUpdate();

	UE_NET_ASSERT_EQ(uint16(1), NetHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(1), NetHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));

	// TearOff the object this will also tear-off subobject
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetHandle);

	// Update logic, object should be removed from scope but still exist as pending create in
	Server->PreSendUpdate();

	UE_NET_ASSERT_EQ(uint16(1), NetHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(1), NetHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));

	// Destroy the object
	Server->DestroyObject(ServerObject);

	// Verify that we no longer have any references to the object
	UE_NET_ASSERT_EQ(uint16(0), NetHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(0), NetHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));
	
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();
}

// Test that we can replicate an object with no replicated properties
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestReplicatedObjectWithNoReplicatedProperties)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObjectWithNoReplicatedMembers* ServerObject = Server->CreateObject<UTestReplicatedIrisObjectWithNoReplicatedMembers>();
	const FNetHandle ServerHandle = ServerObject->NetHandle;

	UE_NET_ASSERT_TRUE(ServerHandle.IsValid());

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UTestReplicatedIrisObjectWithNoReplicatedMembers* ClientObject = Cast<UTestReplicatedIrisObjectWithNoReplicatedMembers>(Client->GetReplicationBridge()->GetReplicatedObject(ServerHandle));
	UE_NET_ASSERT_TRUE(ClientObject != nullptr);

	// Destroy object
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	ClientObject = Cast<UTestReplicatedIrisObjectWithNoReplicatedMembers>(Client->GetReplicationBridge()->GetReplicatedObject(ServerHandle));
	UE_NET_ASSERT_TRUE(ClientObject == nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestObjectPollFramePeriod)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server that later will be addedded as a dependent object
	UTestReplicatedIrisObject* ServerObjectPolledEveryOtherFrame = Server->CreateObject(0, 0);
	Server->ReplicationBridge->SetPollFramePeriod(ServerObjectPolledEveryOtherFrame, 1U);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetHandle));
	UTestReplicatedIrisObject* ClientObjectPolledEveryOtherFrame = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectPolledEveryOtherFrame->NetHandle));

	UE_NET_ASSERT_NE(ClientObjectPolledEveryOtherFrame, nullptr);
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientObjectPolledEveryOtherFrame->IntA, ServerObjectPolledEveryOtherFrame->IntA);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerObjectPolledEveryOtherFrame->IntA = 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that only the server object has been updated
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_NE(ClientObjectPolledEveryOtherFrame->IntA, ServerObjectPolledEveryOtherFrame->IntA);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that both objects now are in sync
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientObjectPolledEveryOtherFrame->IntA, ServerObjectPolledEveryOtherFrame->IntA);
}

}
