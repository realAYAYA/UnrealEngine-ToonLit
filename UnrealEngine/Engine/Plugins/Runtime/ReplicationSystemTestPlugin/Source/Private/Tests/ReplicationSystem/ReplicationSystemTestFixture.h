// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "ReplicatedTestObject.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ReplicationProtocolManager.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

namespace UE::Net
{

/** Simple fixture that spins up a ReplicationSystem and manages creation of UTestReplicatedIrisObjects */
class FReplicationSystemTestFixture : public FNetworkAutomationTestSuiteFixture
{
public:
	FReplicationSystemTestFixture()
	: FNetworkAutomationTestSuiteFixture()
	, ReplicationBridge(nullptr)
	{
	}

protected:
	virtual void SetUp() override
	{
		ReplicationBridge = NewObject<UReplicatedTestObjectBridge>();
		CreatedObjects.Add(TStrongObjectPtr<UObject>(ReplicationBridge));

		UReplicationSystem::FReplicationSystemParams Params;
		Params.ReplicationBridge = ReplicationBridge;
		Params.bIsServer = true;
		Params.bAllowObjectReplication = true;

		// In a testing environment without configs the creation of the ReplicationSystem can be quite spammy
		auto IrisLogVerbosity = UE_GET_LOG_VERBOSITY(LogIris);
		LogIris.SetVerbosity(ELogVerbosity::Error);
		ReplicationSystem = FReplicationSystemFactory::CreateReplicationSystem(Params);
		LogIris.SetVerbosity(IrisLogVerbosity);

		UE_NET_ASSERT_NE(ReplicationBridge, nullptr);
	}

	virtual void TearDown() override
	{
		FReplicationSystemFactory::DestroyReplicationSystem(ReplicationSystem);
		CreatedObjects.Empty();
	}

	// Creates a test object without components
	UTestReplicatedIrisObject* CreateObject()
	{
		UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
		CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
		return CreatedObject;
	}

	void DestroyObject(UObject* Object)
	{
		CreatedObjects.Remove(TStrongObjectPtr<UObject>(Object));
		Object->MarkAsGarbage();
	}

	// Creates an object of a specific type. Only ReplicatedTestObject derived classes are supported.
	template<typename T>
	T* CreateObject()
	{
		T* CreatedObject = NewObject<T>();
		if (Cast<UReplicatedTestObject>(CreatedObject))
		{
			CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));
			return CreatedObject;
		}

		return nullptr;
	}

	// Creates a test object without the specified number of property and native Iris components
	UTestReplicatedIrisObject* CreateObject(uint32 NumPropertyComponents, uint32 NumIrisComponents)
	{
		UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
		CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

		CreatedObject->AddComponents(NumPropertyComponents, NumIrisComponents);
	
		return CreatedObject;
	}

	// Creates a test object without the specified number of property, native Iris and dynamic state components
	UTestReplicatedIrisObject* CreateObjectWithDynamicState(uint32 NumPropertyComponents, uint32 NumIrisComponents, uint32 NumDynamicStateComponents)
	{
		UTestReplicatedIrisObject* CreatedObject = NewObject<UTestReplicatedIrisObject>();
		CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

		CreatedObject->AddComponents(NumPropertyComponents, NumIrisComponents);
		CreatedObject->AddDynamicStateComponents(NumDynamicStateComponents);
	
		return CreatedObject;
	}

	UReplicationSystem* ReplicationSystem;
	UReplicatedTestObjectBridge* ReplicationBridge;
	Private::FReplicationProtocolManager ReplicationProtocolManager;
	
	TArray<TStrongObjectPtr<UObject>> CreatedObjects;
};

}
