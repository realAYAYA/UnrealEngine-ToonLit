// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemServerClientTestFixture.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Iris/Core/IrisLog.h"

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
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);

	// Destroy the spawned object on server
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);
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
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);

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
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);
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
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);

	// Send and drop packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);
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
	Server->GetReplicationSystem()->SetStaticPriority(ServerObject->NetRefHandle, 1.f);

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
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle) != nullptr);
	UE_NET_ASSERT_TRUE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle) != nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_FALSE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle) != nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroyMultipleSubObjects)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

	// Spawn second object on server as a subobject
	ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that second subobject replicated properly to server
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

	// Destroy the spawned object on server
	Server->DestroyObject(ServerSubObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that object now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectAndDestroyOwner)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

	// Destroy the spawned subobject on server
	Server->DestroyObject(ServerSubObject);

	// Destroy owner after spawned subobject on server
	Server->DestroyObject(ServerObject);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that subobject now is destroyed on client as well
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectAndDestroyOwnerWithDataInFlight)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

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
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectWithLostData)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that created server handles now also exists on client
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);

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
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, ReplicateAndDestroySubObjectPendingCreateConfirmation)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn second object on server as a subobject
	UReplicatedTestObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

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
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle), nullptr);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestSubObjectDefaultReplicationOrder)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UReplicatedTestObject* ServerObject = Server->CreateObject(0, 0);

	// Spawn some subobjects
	UReplicatedSubObjectOrderObject* ServerSubObject0 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle);
	UReplicatedSubObjectOrderObject* ServerSubObject1 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle);
	UReplicatedSubObjectOrderObject* ServerSubObject2 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle);

	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientSubObject0 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject0->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject1 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject2 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetRefHandle));

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
	UReplicatedSubObjectOrderObject* ServerSubObject0 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle);
	// Specify Subobject1 to replicate with SubObject0, which means that it will replicate before Subobjet0 is replicated
	UReplicatedSubObjectOrderObject* ServerSubObject1 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle, ServerSubObject0->NetRefHandle, UReplicationBridge::ESubObjectInsertionOrder::ReplicateWith);
	// Specify SubObect 2 to replicate with no specific order (it will be added to the owner and thus replicate last)
	UReplicatedSubObjectOrderObject* ServerSubObject2 = Server->CreateSubObject<UReplicatedSubObjectOrderObject>(ServerObject->NetRefHandle, ServerSubObject1->NetRefHandle, UReplicationBridge::ESubObjectInsertionOrder::None);

	UReplicatedSubObjectOrderObject::RepOrderCounter = 0U;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that objects have replicated
	UReplicatedSubObjectOrderObject* ClientSubObject0 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject0->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject1 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject1->NetRefHandle));
	UReplicatedSubObjectOrderObject* ClientSubObject2 = Cast<UReplicatedSubObjectOrderObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject2->NetRefHandle));

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
	ReplicationSystem->AddToGroup(Group, ServerObject->NetRefHandle);

	ReplicationSystem->AddExclusionFilterGroup(Group);
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

	// Expect client to create object
	Server->DeliverTo(Client, true);
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Send packet
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Allow group to trigger state to ensure that we restart replication
	ReplicationSystem->SetGroupFilterStatus(Group, ENetFilterStatus::Allow);

	// Expect client to destroy object
	Server->DeliverTo(Client, true);
	UE_NET_ASSERT_EQ(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);

	// Trigger replication
	++ServerObject->IntA;

	// Send packet
	// WaitOnDestroyConfirmation -> WaitOnCreateConfirmation
	Server->UpdateAndSend({ Client });

	// Verify that the object got created again
	UE_NET_ASSERT_NE(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle), nullptr);
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
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);
	}

	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetRefHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject1->IntA, ClientObject->IntA);

	}

	// Mark the object as a net temporary
	ReplicationSystem->SetIsNetTemporary(ServerObject->NetRefHandle);

	// Modify the value
	ServerObject->IntA = 2;
	ServerObject1->IntA = 2;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that client has not received the data for changed temporary
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_NE(ServerObject->IntA, ClientObject->IntA);
	}

	// Verify that client has received the data for normal object
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetRefHandle));

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
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client2->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

		UE_NET_ASSERT_TRUE(ClientObject != nullptr);
		UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObject->IntA);
	}

	// Verify that client has received the data for normal object
	{
		UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client2->GetReplicationBridge()->GetReplicatedObject(ServerObject1->NetRefHandle));

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

	// Verify that ClientObject is torn-off and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
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

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);

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
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObjectThatWillBeTornOff, nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// TearOff the object
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and do not deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DoNotDeliverPacket);
	Server->PostSendUpdate();

	// The ClientObject should still be found using the NetRefHandle
	UE_NET_ASSERT_NE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)), nullptr);

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, DeliverPacket);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off
	UE_NET_ASSERT_EQ(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)), nullptr);
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

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);

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
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetRefHandle);

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

	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);

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
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is TornOFf and that the final state was applied
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
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
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

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
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
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
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// TearOff the object
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetRefHandle);

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
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
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
	UTestReplicatedIrisObject* ClientObjectThatWillBeTornOff = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));

	UE_NET_ASSERT_TRUE(ClientObjectThatWillBeTornOff != nullptr);
	UE_NET_ASSERT_EQ(ServerObject->IntA, ClientObjectThatWillBeTornOff->IntA);

	// Modify the value
	ServerObject->IntA = 2;

	// TearOff the object
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetRefHandle);

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
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
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

	// TearOff the object using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied to subObject 
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
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
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
}

// Test dropped creation of subobject dirties owner
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestDroppedCreationForSubobjectDirtiesOwner)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientObject != nullptr);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Send and do not deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject now is created as expected
	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle));
	UE_NET_ASSERT_TRUE(ClientSubObject != nullptr);
}

// Test replicated destroy for not created object
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestReplicatedDestroyForNotCreatedObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Update and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Destroy object
	Server->ReplicationBridge->EndReplication(ServerObject);

	// Update and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Drop first packet containing creation info for object
	Server->DeliverTo(Client, false);

	// Deliver second packet that should contain destroy
	Server->DeliverTo(Client, true);

	// Verify that the object does not exist on client
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) == nullptr);
}


// Test replicated SubObjectDestroy for not created subobject
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestReplicatedSubObjectDestroyForNotCreatedObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Replicate object
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObjectWithNoReplicatedMembers* ServerSubObject = Server->CreateSubObject<UTestReplicatedIrisObjectWithNoReplicatedMembers>(ServerObject->NetRefHandle);

	// Update and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Destroy subobject
	Server->ReplicationBridge->EndReplication(ServerSubObject);

	// Update and delay delivery
	Server->PreSendUpdate();
	Server->SendTo(Client);
	Server->PostSendUpdate();

	// Drop first packet containing creation info for subobject
	Server->DeliverTo(Client, false);

	// Deliver second packet that should contain replicated subobject destroy
	Server->DeliverTo(Client, true);

	// Verify that the object still exists on client
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle)) != nullptr);

	// Verify that the subobject does not exist on client
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObjectWithNoReplicatedMembers>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
}

// Test tear-off object in PendingCreate state to ensure that tear-off logic works as expected
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffObjectWithNoFragmentsDoesNotTriggerCheckIfPendingCreateWhenDestroyed)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObjectWithNoReplicatedMembers* ServerObject = Server->CreateObject<UTestReplicatedIrisObjectWithNoReplicatedMembers>();

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Tear-off using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerObject, EEndReplicationFlags::TearOff);

	// Trigger the next update but avoid sending any data so that we keep the object in the PendingCreation state while we flush the Handles PendingTearOff Array which occurs in PostSendUpdate
	Server->PreSendUpdate();
	Server->PostSendUpdate();
}

// Test tear-off subobject in PendingCreate state to ensure that tear-off logic works as expected
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffSubObjectWithNoFragmentsDoesNotTriggerCheckIfPendingCreateWhenDestroyed)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObjectWithNoReplicatedMembers* ServerSubObject = Server->CreateSubObject<UTestReplicatedIrisObjectWithNoReplicatedMembers>(ServerObject->NetRefHandle);

	// Update and drop
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, false);
	Server->PostSendUpdate();

	// Tear-off using immediate tear-off
	Server->ReplicationBridge->EndReplication(ServerSubObject, EEndReplicationFlags::TearOff);

	// Trigger the next update but avoid sending any data so that we keep the sub-object in the PendingCreation state while we flush the Handles PendingTearOff Array which occurs in PostSendUpdate
	Server->PreSendUpdate();
	Server->PostSendUpdate();
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

	// TearOff the object using immediate tear-off
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetRefHandle);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that ClientObject is torn-off and that the final state was applied to subObject 
	UE_NET_ASSERT_EQ(ServerSubObject->IntA, ClientSubObjectThatWillBeTornOff->IntA);
	UE_NET_ASSERT_TRUE(Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle)) == nullptr);
}

// Test TearOff and destroy of SubObjects that are still pending create/tearoff
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestTearOffNextUpdateExistingObjectWithSubObjectPendingCreation)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;
	FNetRefHandleManager* NetRefHandleManager = &ReplicationSystem->GetReplicationSystemInternal()->GetNetRefHandleManager();

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(0,0);

	// Spawn second object on server as a subobject
	UTestReplicatedIrisObject* ServerSubObject = Server->CreateSubObject(ServerObject->NetRefHandle, 0, 0);

	// Trigger replication
	ServerObject->IntA = 1;
	ServerSubObject->IntA = 1;

	const FInternalNetRefIndex ServerObjectInternalIndex = NetRefHandleManager->GetInternalIndex(ServerObject->NetRefHandle);
	const FInternalNetRefIndex SubObjectObjectInternalIndex = NetRefHandleManager->GetInternalIndex(ServerSubObject->NetRefHandle);

	// Trigger presend without send to add the objects to scope
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(uint16(1), NetRefHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(1), NetRefHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));

	// TearOff the object this will also tear-off subobject
	Server->ReplicationSystem->TearOffNextUpdate(ServerObject->NetRefHandle);

	// Update logic, object should be removed from scope but still exist as pending create in
	Server->PreSendUpdate();
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(uint16(1), NetRefHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(1), NetRefHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));

	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Destroy the object
	Server->DestroyObject(ServerObject);

	// Verify that we no longer have any references to the object
	UE_NET_ASSERT_EQ(uint16(0), NetRefHandleManager->GetNetObjectRefCount(ServerObjectInternalIndex));
	UE_NET_ASSERT_EQ(uint16(0), NetRefHandleManager->GetNetObjectRefCount(SubObjectObjectInternalIndex));
}

// Test that we can replicate an object with no replicated properties
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestReplicatedObjectWithNoReplicatedProperties)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server
	UTestReplicatedIrisObjectWithNoReplicatedMembers* ServerObject = Server->CreateObject<UTestReplicatedIrisObjectWithNoReplicatedMembers>();
	const FNetRefHandle ServerHandle = ServerObject->NetRefHandle;

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

	// Spawn second object on server that later will be added as a dependent object
	UObjectReplicationBridge::FCreateNetRefHandleParams Params = Server->GetReplicationBridge()->DefaultCreateNetRefHandleParams;
	Params.PollFrequency = Server->ConvertPollPeriodIntoFrequency(1U);
	UTestReplicatedIrisObject* ServerObjectPolledEveryOtherFrame = Server->CreateObject(Params);

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Store Pointer to objects and verify state after initial replication
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UTestReplicatedIrisObject* ClientObjectPolledEveryOtherFrame = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectPolledEveryOtherFrame->NetRefHandle));

	UE_NET_ASSERT_NE(ClientObjectPolledEveryOtherFrame, nullptr);
	UE_NET_ASSERT_NE(ClientObject, nullptr);
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	UE_NET_ASSERT_EQ(ClientObjectPolledEveryOtherFrame->IntA, ServerObjectPolledEveryOtherFrame->IntA);

	// After two value updates it's expected that the polling occurs exactly one time for the object with poll frame period 1 (meaning every other frame).
	bool SlowPollObjectHasBeenEqual = false;
	bool SlowPollObjectHasBeenInequal = false;

	// Update values
	ServerObject->IntA += 1;
	ServerObjectPolledEveryOtherFrame->IntA += 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	SlowPollObjectHasBeenEqual |= (ClientObjectPolledEveryOtherFrame->IntA == ServerObjectPolledEveryOtherFrame->IntA);
	SlowPollObjectHasBeenInequal |= (ClientObjectPolledEveryOtherFrame->IntA != ServerObjectPolledEveryOtherFrame->IntA);

	// Update values
	ServerObject->IntA += 1;
	ServerObjectPolledEveryOtherFrame->IntA += 1;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify that both objects now are in sync
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);
	SlowPollObjectHasBeenEqual |= (ClientObjectPolledEveryOtherFrame->IntA == ServerObjectPolledEveryOtherFrame->IntA);
	SlowPollObjectHasBeenInequal |= (ClientObjectPolledEveryOtherFrame->IntA != ServerObjectPolledEveryOtherFrame->IntA);

	UE_NET_ASSERT_TRUE(SlowPollObjectHasBeenEqual);
	UE_NET_ASSERT_TRUE(SlowPollObjectHasBeenInequal);
}

// Test that broken objects can be skipped by client
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestClientCanSkipBrokenObject)
{
	UReplicationSystem* ReplicationSystem = Server->ReplicationSystem;

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	UTestReplicatedIrisObject* ServerObjectA = Server->CreateObject(0,0);
	UTestReplicatedIrisObject* ServerObjectB = Server->CreateObject(0,0);

	{
		// Setup client to fail to create next remote object
		ServerObjectA->bForceFailToInstantiateOnRemote = true;

		// Suppress ensure that will occur due to failing to instantiate the object
		UReplicatedTestObjectBridge::FSupressCreateInstanceFailedEnsureScope SuppressEnsureScope(*Client->GetReplicationBridge());

		// Disable error logging as we know we will fail.
		auto IrisLogVerbosity = UE_GET_LOG_VERBOSITY(LogIris);
		LogIris.SetVerbosity(ELogVerbosity::NoLogging);

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		// Restore LogVerbosity
		LogIris.SetVerbosity(IrisLogVerbosity);
	}

	// We expect replication of ObjectA to have failed
	{
		UTestReplicatedIrisObject* ClientObjectA = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle));
		UE_NET_ASSERT_TRUE(ClientObjectA == nullptr);
	}

	// ObjectB should have been replicated ok
	{
		UTestReplicatedIrisObject* ClientObjectB = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectB->NetRefHandle));
		UE_NET_ASSERT_TRUE(ClientObjectB != nullptr);
	}

	// Modify both objects to make them replicate again
	++ServerObjectA->IntA;
	++ServerObjectB->IntA;

	// Send and deliver packet to verify that client ignores the broken object
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// We expect replication of ObjectA to have failed
	{
		UTestReplicatedIrisObject* ClientObjectA = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle));
		UE_NET_ASSERT_TRUE(ClientObjectA == nullptr);
	}

	// Filter out ObjectA to tell the client that the object has gone out of scope
	ReplicationSystem->AddToGroup(ReplicationSystem->GetNotReplicatedNetObjectGroup(), ServerObjectA->NetRefHandle);

	// Send and deliver packet, the client should now remove the broken object from the list of broken objects
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Enable replication of ObjectA again to try to replicate it to server now that it should succeed
	ReplicationSystem->RemoveFromGroup(ReplicationSystem->GetNotReplicatedNetObjectGroup(), ServerObjectA->NetRefHandle);

	// Set ObjectA to be able instantiate on client again
	ServerObjectA->bForceFailToInstantiateOnRemote = false;

	// Client should now be able to instantiate the object
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// We expect replication of ObjectA to have succeeded this time
	{
		UTestReplicatedIrisObject* ClientObjectA = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle));
		UE_NET_ASSERT_TRUE(ClientObjectA == nullptr);
	}
}


// Test that PropertyReplication properly handles partial states during Apply
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestPartialDequantize)
{
	// Enable cvars to exercise path that store previous state for OnReps to make sure we exercise path that accumulate dirty changes so that we have a complete state.
	IConsoleVariable* CVarUsePrevReceivedStateForOnReps = IConsoleManager::Get().FindConsoleVariable(TEXT("net.Iris.UsePrevReceivedStateForOnReps"));
	check(CVarUsePrevReceivedStateForOnReps != nullptr && CVarUsePrevReceivedStateForOnReps->IsVariableBool());
	const bool bUsePrevReceivedStateForOnReps = CVarUsePrevReceivedStateForOnReps->GetBool();
	CVarUsePrevReceivedStateForOnReps->Set(true, ECVF_SetByCode);

	// Make sure we allow partial dequantize
	IConsoleVariable* CVarForceFullDequantizeAndApply = IConsoleManager::Get().FindConsoleVariable(TEXT("net.iris.ForceFullDequantizeAndApply"));
	check(CVarForceFullDequantizeAndApply != nullptr && CVarForceFullDequantizeAndApply->IsVariableBool());
	const bool bForceFullDequantizeAndApply = CVarForceFullDequantizeAndApply->GetBool();
	CVarForceFullDequantizeAndApply->Set(false, ECVF_SetByCode);

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	UTestReplicatedObjectWithRepNotifies* ServerObjectA = Server->CreateObject<UTestReplicatedObjectWithRepNotifies>();
	
	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Verify assumptions
	// Object should exist on client and have default state
	UTestReplicatedObjectWithRepNotifies* ClientObjectA = Cast<UTestReplicatedObjectWithRepNotifies>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObjectA->NetRefHandle));
	UE_NET_ASSERT_NE(ClientObjectA, nullptr);

	UE_NET_ASSERT_EQ(ServerObjectA->IntA, ClientObjectA->IntA);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntAStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntB, ClientObjectA->IntB);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntBStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntC, ClientObjectA->IntC);

	// Modify only IntA
	ServerObjectA->IntA = 1;

	// Send and deliver packet
	Server->UpdateAndSend({Client});

	// Verify assumptions
	// Only IntA should have been modified
	UE_NET_ASSERT_EQ(ServerObjectA->IntA, ClientObjectA->IntA);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntAStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntB, ClientObjectA->IntB);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntBStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntC, ClientObjectA->IntC);

	// Modify only IntB
	ServerObjectA->IntB = 1;

	// Send and deliver packet
	Server->UpdateAndSend({Client});

	// Verify assumptions
	// Only IntA should have been modified
	UE_NET_ASSERT_EQ(ServerObjectA->IntA, ClientObjectA->IntA);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntAStoredInOnRep, -1);

	UE_NET_ASSERT_EQ(ServerObjectA->IntB, ClientObjectA->IntB);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntBStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntC, ClientObjectA->IntC);

	// Modify only IntA
	ServerObjectA->IntA = 2;

	// Send and deliver packet
	Server->UpdateAndSend({Client});

	// Verify assumptions
	// IntA should have been modified, and if everything works correctly PrevIntAStoredInOnRep should be 1
	UE_NET_ASSERT_EQ(ServerObjectA->IntA, ClientObjectA->IntA);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntAStoredInOnRep, 1);

	UE_NET_ASSERT_EQ(ServerObjectA->IntB, ClientObjectA->IntB);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntBStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntC, ClientObjectA->IntC);

	// Verify that we do not apply repnotifies if we do not receive data from server by modifying values on the client and verifying that they do not get overwritten
	ServerObjectA->IntB = 2;
	ClientObjectA->IntA = -1;
	ClientObjectA->PrevIntAStoredInOnRep = -1;

	// Send and deliver packet
	Server->UpdateAndSend({Client});

	// Verify assumptions, since we messed with IntA and PrevIntAStoredInOnRep locally they have the value we set but IntB should be updated according to replicated state
	UE_NET_ASSERT_NE(ServerObjectA->IntA, ClientObjectA->IntA);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntAStoredInOnRep, -1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntB, ClientObjectA->IntB);
	UE_NET_ASSERT_EQ(ClientObjectA->PrevIntBStoredInOnRep, 1);
	UE_NET_ASSERT_EQ(ServerObjectA->IntC, ClientObjectA->IntC);

	// Restore cvars
	CVarUsePrevReceivedStateForOnReps->Set(bUsePrevReceivedStateForOnReps, ECVF_SetByCode);
	CVarForceFullDequantizeAndApply->Set(bForceFullDequantizeAndApply, ECVF_SetByCode);
}



}
