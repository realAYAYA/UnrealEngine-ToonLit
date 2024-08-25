// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/DirtyNetObjectTracker.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Tests/ReplicationSystem/ReplicationSystemTestFixture.h"
#include "Tests/ReplicationSystem/ReplicationSystemServerClientTestFixture.h"
#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectGroups.h"

namespace UE::Net::Private
{

/** Test that validates behavior when creating replicated sub objects inside PreUpdate/PreReplication */
UE_NET_TEST_FIXTURE(FReplicationSystemServerClientTestFixture, CreateObjectsInsidePreUpdateTest)
{
	// Add client
	FReplicationSystemTestClient* Client = CreateClient();

	// Spawn object on server that is polled late in order to test ForceNetUpdate
	UObjectReplicationBridge::FCreateNetRefHandleParams Params;
	const uint32 PollPeriod = 100;
	const float PollFrequency = Server->ConvertPollPeriodIntoFrequency(PollPeriod);
	Params.PollFrequency = PollFrequency;
	Params.bCanReceive = true;
	Params.bUseClassConfigDynamicFilter = true;
	Params.bNeedsPreUpdate = true;
	UTestReplicatedIrisObject* ServerObject = Server->CreateObject(Params);
	UTestReplicatedIrisObject* ServerSubObject(nullptr);

	// Send and deliver packet
	Server->UpdateAndSend({ Client });

	// Object should have been created on the client
	UTestReplicatedIrisObject* ClientObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerObject->NetRefHandle));
	UE_NET_ASSERT_NE(ClientObject, nullptr);

	// Add a PreUpdate where we create a new subobject
	auto PreUpdateObject = [&](FNetRefHandle NetHandle, UObject* ReplicatedObject, const UReplicationBridge* ReplicationBridge)
	{
		if (ServerObject == ReplicatedObject)
		{
			if (ServerSubObject == nullptr)
			{
				// Dirty a property
				ServerObject->IntA = 0xBB;

				// Create a subobject
				ServerSubObject = Server->CreateSubObject<UTestReplicatedIrisObject>(ServerObject->NetRefHandle);

				// Dirty this subobject
				ServerSubObject->IntA = 0xBB;

			}
		}
	};
	Server->GetReplicationBridge()->SetExternalPreUpdateFunctor(PreUpdateObject);

	Server->ReplicationSystem->ForceNetUpdate(ServerObject->NetRefHandle);
	Server->UpdateAndSend({ Client });
	
	// Was the property changed in PreUpdate replicated ?
	UE_NET_ASSERT_EQ(ClientObject->IntA, ServerObject->IntA);

	// And in the same Send was the subobject replicated ?
	UTestReplicatedIrisObject* ClientSubObject = Cast<UTestReplicatedIrisObject>(Client->GetReplicationBridge()->GetReplicatedObject(ServerSubObject->NetRefHandle));
	UE_NET_ASSERT_NE(ClientSubObject, nullptr);

	// The dirty property of the subobject should have replicated too.
	UE_NET_ASSERT_EQ(ClientSubObject->IntA, ClientSubObject->IntA);
}


}