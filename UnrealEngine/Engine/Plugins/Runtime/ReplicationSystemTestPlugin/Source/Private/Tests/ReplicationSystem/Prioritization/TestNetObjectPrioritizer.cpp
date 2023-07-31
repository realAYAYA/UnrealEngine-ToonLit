// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/NetHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/Prioritization/NetObjectPrioritizerDefinitions.h"
#include "Tests/ReplicationSystem/Prioritization/MockNetObjectPrioritizer.h"
#include "Tests/ReplicationSystem/Prioritization/TestPrioritizationObject.h"
#include "Tests/ReplicationSystem/ReplicationSystemTestFixture.h"
#include "Iris/ReplicationSystem/RepTag.h"

namespace UE::Net::Private
{

class FTestNetPrioritizerFixture : public FReplicationSystemTestFixture
{
public:
	FTestNetPrioritizerFixture() : MockNetObjectPrioritizer(nullptr) {}

private:
	virtual void SetUp() override
	{
		InitNetObjectPrioritizerDefinitions();
		FReplicationSystemTestFixture::SetUp();
		InitMockNetObjectPrioritizer();

		NetHandleManager = &ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();
	}

	virtual void TearDown() override
	{
		FReplicationSystemTestFixture::TearDown();
		RestoreNetObjectPrioritizerDefinitions();

		NetHandleManager = nullptr;
	}

	void InitNetObjectPrioritizerDefinitions()
	{
		const UClass* NetObjectPrioritizerDefinitionsClass = UNetObjectPrioritizerDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectPrioritizerDefinitionsClass->FindPropertyByName(TEXT("NetObjectPrioritizerDefinitions"));
		check(DefinitionsProperty != nullptr);

		// Save NetObjectPrioritizerDefinitions CDO state.
		UNetObjectPrioritizerDefinitions* PrioritizerDefinitions = GetMutableDefault<UNetObjectPrioritizerDefinitions>();
		DefinitionsProperty->CopyCompleteValue(&OriginalPrioritizerDefinitions, (void*)(UPTRINT(PrioritizerDefinitions) + DefinitionsProperty->GetOffset_ForInternal()));

		// Modify definitions to only include our mock prioritizer. Ugly... 
		TArray<FNetObjectPrioritizerDefinition> NewPrioritizerDefinitions;
		FNetObjectPrioritizerDefinition& MockDefinition = NewPrioritizerDefinitions.Emplace_GetRef();
		MockDefinition.PrioritizerName = TEXT("MockPrioritizer");
		MockDefinition.ClassName = TEXT("/Script/ReplicationSystemTestPlugin.MockNetObjectPrioritizer");
		MockDefinition.ConfigClassName = TEXT("/Script/ReplicationSystemTestPlugin.MockNetObjectPrioritizerConfig");
		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(PrioritizerDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &NewPrioritizerDefinitions);
	}

	void RestoreNetObjectPrioritizerDefinitions()
	{
		// Restore NetObjectPrioritizerDefinitions CDO state from the saved state.
		const UClass* NetObjectPrioritizerDefinitionsClass = UNetObjectPrioritizerDefinitions::StaticClass();
		const FProperty* DefinitionsProperty = NetObjectPrioritizerDefinitionsClass->FindPropertyByName(TEXT("NetObjectPrioritizerDefinitions"));
		UNetObjectPrioritizerDefinitions* PrioritizerDefinitions = GetMutableDefault<UNetObjectPrioritizerDefinitions>();
		DefinitionsProperty->CopyCompleteValue((void*)(UPTRINT(PrioritizerDefinitions) + DefinitionsProperty->GetOffset_ForInternal()), &OriginalPrioritizerDefinitions);
		OriginalPrioritizerDefinitions.Empty();

		MockPrioritizerHandle = InvalidNetObjectPrioritizerHandle;
		MockNetObjectPrioritizer = nullptr;
	}

	void InitMockNetObjectPrioritizer()
	{
		MockNetObjectPrioritizer = ExactCast<UMockNetObjectPrioritizer>(ReplicationSystem->GetPrioritizer(TEXT("MockPrioritizer")));
		MockPrioritizerHandle = ReplicationSystem->GetPrioritizerHandle(FName("MockPrioritizer"));
	}

protected:
	UMockNetObjectPrioritizer* MockNetObjectPrioritizer = nullptr;
	FNetObjectPrioritizerHandle MockPrioritizerHandle = InvalidNetObjectPrioritizerHandle;
	FNetHandleManager* NetHandleManager = nullptr;
	static constexpr float FakeDeltaTime = 0.0334f;

private:
	TArray<FNetObjectPrioritizerDefinition> OriginalPrioritizerDefinitions;
};

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, CanGetPrioritizerHandle)
{
	UE_NET_ASSERT_NE(MockPrioritizerHandle, InvalidNetObjectPrioritizerHandle);
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizerInitWasCalled)
{
	const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
	UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.Init, 1U);
	UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.Init, 1U);
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizerAddObjectSucceedsAndRemoveObjectIsCalledWhenObjectIsDestroyed)
{
	UTestReplicatedIrisObject* TestObject = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject, nullptr);

	// Check AddObject is called. At this point RemoveObject should not be called.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		// We want the add call to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle = ReplicationBridge->BeginReplication(TestObject);
		ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

		// We don't know if prioritizer changes are batched, but we assume everything is setup correctly in PreSendUpdate at least.
		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 0U);

		ReplicationSystem->PostSendUpdate();
	}

	// Check RemoveObject was called. AddObject should not have been called more times at this point.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		ReplicationBridge->EndReplication(TestObject);
		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.RemoveObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.AddObject, 0U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizerAddObjectFailsAndRemoveObjectIsNotCalledWhenObjectIsDestroyed)
{
	UTestReplicatedIrisObject* TestObject = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject, nullptr);

	// Check AddObject is called and fails. At this point RemoveObject should not be called.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		// We want the add call to fail
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = false;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle = ReplicationBridge->BeginReplication(TestObject);
		ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

		// We don't know if prioritizer changes are batched, but we assume everything is setup correctly in PreSendUpdate at least.
		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.AddObject, 0U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 0U);

		ReplicationSystem->PostSendUpdate();
	}

	// Check RemoveObject is not called when object is destroyed.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		ReplicationBridge->EndReplication(TestObject);
		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 0U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, SwitchingPrioritizersCallsRemoveObjectOnPreviousPrioritizer)
{
	UTestReplicatedIrisObject* TestObject = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject, nullptr);

	// Check AddObject is called and succeeds.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		// We want the add call to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle = ReplicationBridge->BeginReplication(TestObject);
		ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

		// We don't know if prioritizer changes are batched, but we assume everything is setup correctly in PreSendUpdate at least.
		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.AddObject, 1U);

		ReplicationSystem->PostSendUpdate();
	}

	// Check RemoveObject is called when switching prioritizers.
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		ReplicationSystem->SetStaticPriority(TestObject->NetHandle, 1.0f);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.RemoveObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.RemoveObject, 1U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizeIsNotCalledWhenNoObjectsAreAddedToIt)
{
	UTestReplicatedIrisObject* TestObject = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject, nullptr);

	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		const FNetHandle NetHandle = ReplicationBridge->BeginReplication(TestObject);
		ReplicationSystem->SetStaticPriority(NetHandle, 1.0f);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.AddObject, 0U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.Prioritize, 0U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizeIsNotCalledWhenThereAreNoConnections)
{
	UTestReplicatedIrisObject* TestObject = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject, nullptr);

	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		// We want the add call to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle = ReplicationBridge->BeginReplication(TestObject);
		ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.AddObject, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.Prioritize, 0U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, PrioritizeIsCalledWhenThereAreConnections)
{
	// Setup a couple of connections with valid views
	{
		ReplicationSystem->AddConnection(13);
		ReplicationSystem->AddConnection(37);

		FReplicationView View;
		View.Views.AddDefaulted();
		ReplicationSystem->SetReplicationView(13, View);
		ReplicationSystem->SetReplicationView(37, View);
	}

	UTestReplicatedIrisObject* TestObject1 = CreateObject(0, 0);
	UTestReplicatedIrisObject* TestObject2 = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject1, nullptr);
	UE_NET_ASSERT_NE(TestObject2, nullptr);

	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		// We want the add calls to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle1 = ReplicationBridge->BeginReplication(TestObject1);
		ReplicationSystem->SetPrioritizer(NetHandle1, MockPrioritizerHandle);

		const FNetHandle NetHandle2 = ReplicationBridge->BeginReplication(TestObject2);
		ReplicationSystem->SetPrioritizer(NetHandle2, MockPrioritizerHandle);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();
		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.Prioritize, 2U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.Prioritize, 2U);

		ReplicationSystem->PostSendUpdate();
	}

	// Remove connections
	{
		ReplicationSystem->RemoveConnection(13);
		ReplicationSystem->RemoveConnection(37);
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, UpdateObjectsIsCalledForDirtyObject)
{
	UTestReplicatedIrisObject* TestObject1 = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject1, nullptr);

	// Add object to prioritizer
	{
		// We want the add calls to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle1 = ReplicationBridge->BeginReplication(TestObject1);
		ReplicationSystem->SetPrioritizer(NetHandle1, MockPrioritizerHandle);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);
		ReplicationSystem->PostSendUpdate();
	}

	// Dirty object and check that UpdateObjects is called
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		TestObject1->IntA ^= 1;

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();

		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.UpdateObjects, 1U);
		UE_NET_ASSERT_EQ(FunctionCallStatus.SuccessfulCallCounts.UpdateObjects, 1U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, UpdateObjectsIsNotCalledWhenNoObjectIsDirty)
{
	UTestReplicatedIrisObject* TestObject1 = CreateObject(0, 0);
	UE_NET_ASSERT_NE(TestObject1, nullptr);

	// Add object to prioritizer
	{
		// We want the add calls to succeed
		{
			UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
			CallSetup.AddObject.ReturnValue = true;
			MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
		}

		const FNetHandle NetHandle1 = ReplicationBridge->BeginReplication(TestObject1);
		ReplicationSystem->SetPrioritizer(NetHandle1, MockPrioritizerHandle);

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);
		ReplicationSystem->PostSendUpdate();
	}

	// Do not dirty object and check that UpdateObjects is not called
	{
		MockNetObjectPrioritizer->ResetFunctionCallStatus();

		ReplicationSystem->PreSendUpdate(FakeDeltaTime);

		const UMockNetObjectPrioritizer::FFunctionCallStatus& FunctionCallStatus = MockNetObjectPrioritizer->GetFunctionCallStatus();

		UE_NET_ASSERT_EQ(FunctionCallStatus.CallCounts.UpdateObjects, 0U);

		ReplicationSystem->PostSendUpdate();
	}
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, NativeIrisObjectGetsPriorityFromStart)
{
	UTestPrioritizationNativeIrisObject* Object = CreateObject<UTestPrioritizationNativeIrisObject>();

	{
		UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.ReturnValue = true;
		MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
	}

	constexpr float ObjectPriority = 1.3056640625E10f;
	Object->SetPriority(ObjectPriority);

	const FNetHandle NetHandle = ReplicationBridge->BeginReplication(Object);
	ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

	ReplicationSystem->PreSendUpdate(FakeDeltaTime);
	ReplicationSystem->PostSendUpdate();

	UE::Net::Private::FInternalNetHandle InternalIndex = NetHandleManager->GetInternalIndex(NetHandle);
	const float Priority = MockNetObjectPrioritizer->GetPriority(InternalIndex);
	UE_NET_ASSERT_EQ(Priority, ObjectPriority);
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, NativeIrisObjectGetsUpdatedPriority)
{
	UTestPrioritizationNativeIrisObject* Object = CreateObject<UTestPrioritizationNativeIrisObject>();

	{
		UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.ReturnValue = true;
		MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
	}

	Object->SetPriority(2.0f);

	const FNetHandle NetHandle = ReplicationBridge->BeginReplication(Object);
	ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

	// Update priority
	constexpr float UpdatedPriority = 4711;
	Object->SetPriority(UpdatedPriority);

	ReplicationSystem->PreSendUpdate(FakeDeltaTime);
	ReplicationSystem->PostSendUpdate();

	UE::Net::Private::FInternalNetHandle InternalIndex = NetHandleManager->GetInternalIndex(NetHandle);
	const float Priority = MockNetObjectPrioritizer->GetPriority(InternalIndex);
	UE_NET_ASSERT_EQ(Priority, UpdatedPriority);
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, ObjectGetsPriorityFromStart)
{
	UTestPrioritizationObject* Object = CreateObject<UTestPrioritizationObject>();

	{
		UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.ReturnValue = true;
		MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
	}

	constexpr float ObjectPriority = 1.3056640625E10f;
	Object->SetPriority(ObjectPriority);

	const FNetHandle NetHandle = ReplicationBridge->BeginReplication(Object);
	ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

	ReplicationSystem->PreSendUpdate(FakeDeltaTime);
	ReplicationSystem->PostSendUpdate();

	UE::Net::Private::FInternalNetHandle InternalIndex = NetHandleManager->GetInternalIndex(NetHandle);
	const float Priority = MockNetObjectPrioritizer->GetPriority(InternalIndex);
	UE_NET_ASSERT_EQ(Priority, ObjectPriority);
}

UE_NET_TEST_FIXTURE(FTestNetPrioritizerFixture, ObjectGetsUpdatedPriority)
{
	UTestPrioritizationObject* Object = CreateObject<UTestPrioritizationObject>();

	{
		UMockNetObjectPrioritizer::FFunctionCallSetup CallSetup;
		CallSetup.AddObject.ReturnValue = true;
		MockNetObjectPrioritizer->SetFunctionCallSetup(CallSetup);
	}

	Object->SetPriority(2.0f);

	const FNetHandle NetHandle = ReplicationBridge->BeginReplication(Object);
	ReplicationSystem->SetPrioritizer(NetHandle, MockPrioritizerHandle);

	// Update priority
	constexpr float UpdatedPriority = 4711;
	Object->SetPriority(UpdatedPriority);

	ReplicationSystem->PreSendUpdate(FakeDeltaTime);
	ReplicationSystem->PostSendUpdate();

	UE::Net::Private::FInternalNetHandle InternalIndex = NetHandleManager->GetInternalIndex(NetHandle);
	const float Priority = MockNetObjectPrioritizer->GetPriority(InternalIndex);
	UE_NET_ASSERT_EQ(Priority, UpdatedPriority);
}

}
