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
		Client->UpdateAndSend(Server);

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerObject->bServerRPCCalled);

		// Call an RPC client->server
		ClientObject->ServerRPCWithParam(IntParam);

		// Send and deliver client packet
		Client->UpdateAndSend(Server);

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerObject->ServerRPCWithParamCalled == IntParam);
	}

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestMultiCastSendImmediateRPC)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();

		ServerObject->bIsServerObject = true;
		ServerObject->ReplicationSystem = Server->GetReplicationSystem();
		Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, 0x01);

		// Setup NetMulticast_MultiCastRPCSendImmediateCallOrder to be sent immediately
		ServerObject->ReplicationSystem->SetRPCSendPolicyFlags(UTestReplicatedObjectWithRPC::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UTestReplicatedObjectWithRPC, NetMulticast_MultiCastRPCSendImmediate)), ENetObjectAttachmentSendPolicyFlags::SendImmediate);

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		UTestReplicatedObjectWithRPC* ClientObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
		
		// Verify that created server handle now also exists on client
		UE_NET_ASSERT_TRUE(ClientObject != nullptr);

		ClientObject->ReplicationSystem = Client->GetReplicationSystem();

		// Send multicast RPCs Server->Client
		// This is a normal multicast rpc, it will be scheduled with replication of object
		ServerObject->NetMulticast_MultiCastRPC();

		// This is a send immediate multicast rpc, it should be scheduled using the OOB replication channel so it should be received before the normally flagged rpc.
		ServerObject->NetMulticast_MultiCastRPCSendImmediate();

		// Send and deliver packet, simulating a send from PostTickDispatch
		Server->PreSendUpdate(UReplicationSystem::FSendUpdateParams {.SendPass = UE::Net::EReplicationSystemSendPass::PostTickDispatch});
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		// Verify RPC reception, at this point we expect only the RPC flagged as ENetObjectAttachmentSendPolicyFlags::SendImmediate to have been received
		UE_NET_ASSERT_EQ(ClientObject->NetMulticast_MultiCastRPCCallOrder, 0);
		UE_NET_ASSERT_EQ(ClientObject->NetMulticast_MultiCastRPCSendImmediateCallOrder, 1);

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();
		
		// Verify RPC reception, at this point we expect the other RPC to have been received as well.
		UE_NET_ASSERT_EQ(ClientObject->NetMulticast_MultiCastRPCSendImmediateCallOrder, 1);
		UE_NET_ASSERT_EQ(ClientObject->NetMulticast_MultiCastRPCCallOrder, 2);
	}

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestSubObjectRPC)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerRootObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();
		ServerRootObject->Init(Server->GetReplicationSystem());

		const FNetRefHandle ServerRootObjectHandle = ServerRootObject->NetRefHandle;
		Server->ReplicationSystem->SetOwningNetConnection(ServerRootObjectHandle, 0x01);

		UTestReplicatedObjectWithRPC* ServerSubObject = Server->CreateSubObject<UTestReplicatedObjectWithRPC>(ServerRootObjectHandle);
		ServerSubObject->Init(Server->GetReplicationSystem());
		ServerSubObject->SetRootObject(ServerRootObject);

		const FNetRefHandle ServerSubObjectHandle = ServerSubObject->NetRefHandle;

		// Send and deliver packet
		Server->PreSendUpdate();
		Server->SendAndDeliverTo(Client, true);
		Server->PostSendUpdate();

		UTestReplicatedObjectWithRPC* ClientRootObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerRootObjectHandle));

		// Verify that the root object exists on the client
		UE_NET_ASSERT_TRUE(ClientRootObject != nullptr);

		UTestReplicatedObjectWithRPC* ClientSubObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObjectHandle));

		// Verify that the subobject also exists on the client
		UE_NET_ASSERT_TRUE(ClientSubObject != nullptr);

		ClientRootObject->Init(Client->GetReplicationSystem());
		ClientSubObject->Init(Client->GetReplicationSystem());
		ClientSubObject->SetRootObject(ClientRootObject);

		// Call an RPC server->client on the subobject
		ServerSubObject->ClientRPC();

		// Send and deliver packet
		Server->UpdateAndSend({ Client });

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ClientSubObject->bClientRPCCalled);

		// Call an RPC server->client on the subobject
		const int32 IntParam = 0xBABA;
		ServerSubObject->ClientRPCWithParam(0xBABA);

		// Send and deliver packet
		Server->UpdateAndSend({ Client });

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ClientSubObject->ClientRPCWithParamCalled == IntParam);

		// Call an RPC client->server
		ClientSubObject->ServerRPC();

		// Send and deliver client packet
		Client->UpdateAndSend(Server);

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerSubObject->bServerRPCCalled);

		// Call an RPC client->server
		ClientSubObject->ServerRPCWithParam(IntParam);

		// Send and deliver client packet
		Client->UpdateAndSend(Server);

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerSubObject->ServerRPCWithParamCalled == IntParam);
	}

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestUnreliableRPCIsOrderedWithReliableRPCToClient)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();

		ServerObject->bIsServerObject = true;
		ServerObject->ReplicationSystem = Server->GetReplicationSystem();
		Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		UTestReplicatedObjectWithRPC* ClientObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
		
		// Verify that created server handle now also exists on client
		UE_NET_ASSERT_NE(ClientObject, nullptr);

		// Call reliable + unreliable + reliable RPCs
		ServerObject->ClientRPC();
		ServerObject->ClientUnreliableRPC();
		ServerObject->ClientRPCWithParam(1);

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ClientObject->bClientRPCCalled);
		UE_NET_ASSERT_TRUE(ClientObject->bClientUnreliableRPCCalled);
		UE_NET_ASSERT_NE(ClientObject->ClientRPCWithParamCalled, 0);

		// Verify RPC call order
		UE_NET_ASSERT_LT(ClientObject->ClientRPCCallOrder, ClientObject->ClientUnreliableRPCCallOrder);
		UE_NET_ASSERT_LT(ClientObject->ClientUnreliableRPCCallOrder, ClientObject->ClientRPCWithParamCallOrder);
	}

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestUnreliableRPCIsOrderedWithReliableRPCToServer)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();

		ServerObject->bIsServerObject = true;
		ServerObject->ReplicationSystem = Server->GetReplicationSystem();
		Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		UTestReplicatedObjectWithRPC* ClientObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
		
		// Verify that created server handle now also exists on client
		UE_NET_ASSERT_NE(ClientObject, nullptr);

		ClientObject->ReplicationSystem = Client->GetReplicationSystem();

		// Call reliable + unreliable + reliable RPCs
		ClientObject->ServerRPC();
		ClientObject->ServerUnreliableRPC();
		ClientObject->ServerRPCWithParam(1);

		// Send and deliver packet
		Client->UpdateAndSend(Server);

		// Verify RPC reception
		UE_NET_ASSERT_TRUE(ServerObject->bServerRPCCalled);
		UE_NET_ASSERT_TRUE(ServerObject->bServerUnreliableRPCCalled);
		UE_NET_ASSERT_NE(ServerObject->ServerRPCWithParamCalled, 0);

		// Verify RPC call order
		UE_NET_ASSERT_LT(ServerObject->ServerRPCCallOrder, ServerObject->ServerUnreliableRPCCallOrder);
		UE_NET_ASSERT_LT(ServerObject->ServerUnreliableRPCCallOrder, ServerObject->ServerRPCWithParamCallOrder);
	}

	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestUnreliableRPCIsNotResentAfterPacketLoss)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();

		ServerObject->bIsServerObject = true;
		ServerObject->ReplicationSystem = Server->GetReplicationSystem();
		Server->ReplicationSystem->SetOwningNetConnection(ServerObject->NetRefHandle, Client->ConnectionIdOnServer);

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		UTestReplicatedObjectWithRPC* ClientObject = Cast<UTestReplicatedObjectWithRPC>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
		
		// Verify that created server handle now also exists on client
		UE_NET_ASSERT_NE(ClientObject, nullptr);

		// Call reliable + unreliable + reliable RPCs
		ServerObject->ClientRPC();
		ServerObject->ClientUnreliableRPC();
		ServerObject->ClientRPCWithParam(1);

		// Send and do not deliver packet
		Server->UpdateAndSend({Client}, DoNotDeliverPacket);

		// Verify no RPCs were received
		UE_NET_ASSERT_FALSE(ClientObject->bClientRPCCalled);
		UE_NET_ASSERT_FALSE(ClientObject->bClientUnreliableRPCCalled);
		UE_NET_ASSERT_EQ(ClientObject->ClientRPCWithParamCalled, 0);

		// Send and deliver packet
		Server->UpdateAndSend({Client}, DeliverPacket);

		// Verify unreliable RPC was dropped
		UE_NET_ASSERT_TRUE(ClientObject->bClientRPCCalled);
		UE_NET_ASSERT_FALSE(ClientObject->bClientUnreliableRPCCalled);
		UE_NET_ASSERT_NE(ClientObject->ClientRPCWithParamCalled, 0);

		// Verify RPC call order
		UE_NET_ASSERT_LT(ClientObject->ClientRPCCallOrder, ClientObject->ClientRPCWithParamCallOrder);
	}

	// This test is specifically written to exercise a path where we would cause bitstream errors by
	// posting rpc on an object only temporarily in scope, which would be assigned an internal index.
	// This same internal index would then be reused and would inherit previously posted attachments/rpcs possibly for a different type.
	// If rpc index would be valid for the new object and it did not have the same signature it would result in a bitstream error.
	UE_NET_TEST_FIXTURE(FRPCTestFixture, TestShortLivedSubObjectReliableRPC)
	{
		// Add a client
		FReplicationSystemTestClient* Client = CreateClient();

		// Spawn object on server
		UTestReplicatedObjectWithRPC* ServerRootObject = Server->CreateObject<UTestReplicatedObjectWithRPC>();
		ServerRootObject->Init(Server->GetReplicationSystem());

		const FNetRefHandle ServerRootObjectHandle = ServerRootObject->NetRefHandle;

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		// Create subobject
		UTestReplicatedObjectWithRPC* ServerSubObject = Server->CreateSubObject<UTestReplicatedObjectWithRPC>(ServerRootObjectHandle);
		ServerSubObject->Init(Server->GetReplicationSystem());
		ServerSubObject->SetRootObject(ServerRootObject);

		const FNetRefHandle ServerSubObjectHandle = ServerSubObject->NetRefHandle;

		// Call rpc
		ServerSubObject->ClientRPC();

		// Destroy subobject
		Server->DestroyObject(ServerSubObject, EEndReplicationFlags::Destroy);

		// Send and deliver packet
		Server->UpdateAndSend({Client});

		// Spawn object on server
		UTestReplicatedObjectWithSingleRPC* ServerRootObject2 = Server->CreateObject<UTestReplicatedObjectWithSingleRPC>();
		ServerRootObject2->Init(Server->GetReplicationSystem());
		ServerRootObject2->NetMulticast_ReliableMultiCastRPC(3);

		// Send and deliver packet
		Server->UpdateAndSend({Client});
	}

}