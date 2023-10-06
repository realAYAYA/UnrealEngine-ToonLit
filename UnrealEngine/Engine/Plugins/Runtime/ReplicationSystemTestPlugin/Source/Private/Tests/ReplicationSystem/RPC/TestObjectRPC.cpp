// Copyright Epic Games, Inc. All Rights Reserved.

#include "RPCTestFixture.h"
#include "ReplicatedTestObjectWithRPC.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"

namespace UE::Net::Private
{

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestBasicObjectRPC)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();

		ServerObject->bIsServerObject = true;
		ServerObject->ReplicationSystem = Server->GetReplicationSystem();
		Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, 0x01);

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		UTestReplicatedObjectWithRPC* ClientObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
		
		// Verify that created server handle now also exists on client
		UE_NET_ASSERT_TRUE(ClientObject != nullptr);

		ClientObject->ReplicationSystem = Client->GetReplicationSystem();

		// Call an RPC server->client
		ServerObject->ClientRPC();

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ClientObject->bClientRPCCalled);

		// Call an RPC server->client
		const int32 IntParam = 0xBABA;
		ServerObject->ClientRPCWithParam(0xBABA);

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ClientObject->ClientRPCWithParamCalled == IntParam);

		// Call an RPC client->server
		ClientObject->ServerRPC();

		// Send and deliver client packet
		Client->PreSendUpdate();
		Client->SendUpdate();
		Client->DeliverTo(*Server, 0x01, 0x01, true);
		Client->PostSendUpdate();

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerObject->bServerRPCCalled);

		// Call an RPC client->server
		ClientObject->ServerRPCWithParam(IntParam);

		// Send and deliver client packet
		Client->PreSendUpdate();
		Client->SendUpdate();
		Client->DeliverTo(*Server, 0x01, 0x01, true);
		Client->PostSendUpdate();

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerObject->ServerRPCWithParamCalled == IntParam);
	}
}