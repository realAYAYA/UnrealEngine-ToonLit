// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestNetSerializerFixture.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"
#include "Tests/ReplicationSystem/ReplicatedTestObject.h"

namespace UE::Net::Private
{

static FTestMessage& PrintObjectNetSerializerConfig(FTestMessage& Message, const FNetSerializerConfig& InConfig)
{
	return Message;
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestObjectReference)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	UTestReplicatedIrisObjectWithObjectReference* ObjectD = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectC = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectB = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectA = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();

	ObjectA->RawObjectPtrRef = ObjectB;
	ObjectB->RawObjectPtrRef = ObjectC;

	// This will not work since we do not have any dependencies
	ObjectD->RawObjectPtrRef = ObjectA;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Check state, we cheat a bit and use the server nethandle for lookup
	// THIS will fail if we start using full handle compares
	auto ClientObjectA = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectA->NetHandle);
	auto ClientObjectB = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectB->NetHandle);
	auto ClientObjectC = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectC->NetHandle);
	auto ClientObjectD = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectD->NetHandle);
	
	UE_NET_ASSERT_TRUE(ClientObjectA != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectB != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectC != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectD != nullptr);

	// Verify that we managed to resolved the references
	UE_NET_ASSERT_EQ((UObject*)ClientObjectA->RawObjectPtrRef, (UObject*)ClientObjectB);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectB->RawObjectPtrRef, (UObject*)ClientObjectC);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectC->RawObjectPtrRef, (UObject*)nullptr);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectD->RawObjectPtrRef, (UObject*)ClientObjectA);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestCircularReference)
{
	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	UTestReplicatedIrisObjectWithObjectReference* ObjectD = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectC = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectB = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectA = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();

	ObjectA->RawObjectPtrRef = ObjectB;
	ObjectB->RawObjectPtrRef = ObjectC;
	ObjectC->RawObjectPtrRef = ObjectD;
	ObjectD->RawObjectPtrRef = ObjectA;

	// Send and deliver packet
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Check state, we cheat a bit and use the server nethandle for lookup
	// THIS will fail if we start using full handle compares
	auto ClientObjectA = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectA->NetHandle);
	auto ClientObjectB = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectB->NetHandle);
	auto ClientObjectC = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectC->NetHandle);
	auto ClientObjectD = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectD->NetHandle);
	
	UE_NET_ASSERT_TRUE(ClientObjectA != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectB != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectC != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectD != nullptr);

	// Verify that we managed to resolved the references
	UE_NET_ASSERT_EQ((UObject*)ClientObjectA->RawObjectPtrRef, (UObject*)ClientObjectB);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectB->RawObjectPtrRef, (UObject*)ClientObjectC);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectC->RawObjectPtrRef, (UObject*)ClientObjectD);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectD->RawObjectPtrRef, (UObject*)ClientObjectA);
}

UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, TestCircularReferenceWithLimitedThroughput)
{
	// Limit packet size
	Server->SetMaxSendPacketSize(128U);

	// Add a client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn objects on server
	UTestReplicatedIrisObjectWithObjectReference* ObjectD = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectC = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectB = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();
	UTestReplicatedIrisObjectWithObjectReference* ObjectA = Server->CreateObject<UTestReplicatedIrisObjectWithObjectReference>();

	ObjectA->RawObjectPtrRef = ObjectB;
	ObjectB->RawObjectPtrRef = ObjectC;
	ObjectC->RawObjectPtrRef = ObjectD;
	ObjectD->RawObjectPtrRef = ObjectA;

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Send and deliver packets
	Server->PreSendUpdate();
	Server->SendAndDeliverTo(Client, true);
	Server->PostSendUpdate();

	// Check state, we cheat a bit and use the server nethandle for lookup
	// THIS will fail if we start using full handle compares
	auto ClientObjectA = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectA->NetHandle);
	auto ClientObjectB = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectB->NetHandle);
	auto ClientObjectC = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectC->NetHandle);
	auto ClientObjectD = Client->GetObjectAs<UTestReplicatedIrisObjectWithObjectReference>(ObjectD->NetHandle);
	
	UE_NET_ASSERT_TRUE(ClientObjectA != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectB != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectC != nullptr);
	UE_NET_ASSERT_TRUE(ClientObjectD != nullptr);

	// Verify that we managed to resolved the references
	UE_NET_ASSERT_EQ((UObject*)ClientObjectA->RawObjectPtrRef, (UObject*)ClientObjectB);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectB->RawObjectPtrRef, (UObject*)ClientObjectC);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectC->RawObjectPtrRef, (UObject*)ClientObjectD);
	UE_NET_ASSERT_EQ((UObject*)ClientObjectD->RawObjectPtrRef, (UObject*)ClientObjectA);
}

}
