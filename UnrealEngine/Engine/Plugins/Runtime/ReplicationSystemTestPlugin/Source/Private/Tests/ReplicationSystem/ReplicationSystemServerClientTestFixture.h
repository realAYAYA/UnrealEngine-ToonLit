// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "ReplicatedTestObject.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "HAL/PlatformMath.h"

#include "Iris/DataStream/DataStreamDefinitions.h"
#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/ReplicationSystem/NetTokenDataStream.h"
#include "Iris/ReplicationSystem/ReplicationDataStream.h"

#include "Net/Core/Misc/ResizableCircularQueue.h"

namespace UE::Net
{

// Utility class to use DataStreams in test code
class FDataStreamTestUtil
{
public:
	FDataStreamTestUtil();

	void SetUp();
	void TearDown();

	class UMyDataStreamDefinitions : public UDataStreamDefinitions
	{
	public:
		void FixupDefinitions() { return UDataStreamDefinitions::FixupDefinitions(); }
	};

	void StoreDataStreamDefinitions();
	void RestoreDataStreamDefinitions();
	void AddDataStreamDefinition(const TCHAR* StreamName, const TCHAR* ClassPath, bool bValid = true);
	void FixupDefinitions() { DataStreamDefinitions->FixupDefinitions(); }

protected:
	UMyDataStreamDefinitions* DataStreamDefinitions;
	TArray<FDataStreamDefinition>* CurrentDataStreamDefinitions;
	TArray<FDataStreamDefinition> PreviousDataStreamDefinitions;
	bool* PointerToFixupComplete;
};

// Implements everything we need to drive the replication system to test the system
class FReplicationSystemTestNode
{
public:
	uint32 MaxSendPacketSize = 2048U;

	struct FPacketData
	{
		enum { MaxPacketSize = 2048U };
		
		alignas(16) uint8 PacketBuffer[MaxPacketSize];
		uint32 BitCount;
	};

	struct FConnectionInfo
	{
		UDataStreamManager* DataStreamManager;
		UReplicationDataStream* ReplicationDataStream;
		UNetTokenDataStream* NetTokenDataStream;
		FNetTokenStoreState* RemoteNetTokenStoreState;
		TResizableCircularQueue<const FDataStreamRecord*> WriteRecords;
		TResizableCircularQueue<FPacketData> WrittenPackets;
		uint32 ConnectionId;
	};

public:
	explicit FReplicationSystemTestNode(bool bIsServer);
	~FReplicationSystemTestNode();

	template<typename T>
	T* CreateObject()
	{
		T* CreatedObject = NewObject<T>();
		if (Cast<UReplicatedTestObject>(CreatedObject))
		{
			CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

			// Add it to the bridge for replication
			ReplicationBridge->BeginReplication(CreatedObject);
	
			return CreatedObject;
		}
		return nullptr;
	}

	template<typename T>
	T* CreateSubObject(FNetHandle OwnerHandle, FNetHandle InsertRelativeToSubObjectHandle = FNetHandle(), UReplicationBridge::ESubObjectInsertionOrder InsertionOrder = UReplicationBridge::ESubObjectInsertionOrder::None)
	{
		T* CreatedObject = NewObject<T>();
		if (Cast<UReplicatedTestObject>(CreatedObject))
		{
			CreatedObjects.Add(TStrongObjectPtr<UObject>(CreatedObject));

			// Add it to the bridge for replication
			ReplicationBridge->BeginReplication(OwnerHandle, CreatedObject, InsertRelativeToSubObjectHandle, InsertionOrder);
	
			return CreatedObject;
		}
		return nullptr;
	}

	template <typename T>
	T* GetObjectAs(FNetHandle Handle)
	{
		return static_cast<T*>(ReplicationBridge->GetReplicatedObject(Handle));
	}

	UTestReplicatedIrisObject* CreateObject(uint32 NumComponents, uint32 NumIrisComponents);
	UTestReplicatedIrisObject* CreateSubObject(FNetHandle Owner, uint32 NumComponents, uint32 NumIrisComponents);
	UTestReplicatedIrisObject* CreateObject(const UTestReplicatedIrisObject::FComponents& Components);
	UTestReplicatedIrisObject* CreateSubObject(FNetHandle Owner, const UTestReplicatedIrisObject::FComponents& Components);
	UTestReplicatedIrisObject* CreateObjectWithDynamicState(uint32 NumComponents, uint32 NumIrisComponents, uint32 NumDynamicStateComponents);
	void DestroyObject(UReplicatedTestObject*);

	// Connection
	uint32 AddConnection();

	// System Update
	void PreSendUpdate();
	void PostSendUpdate();

	// Update methods for server connections
	bool SendUpdate(uint32 ConnectionId);
	bool SendUpdate() { return SendUpdate(1); }

	void DeliverTo(FReplicationSystemTestNode& Dest, uint32 LocalConnectionId, uint32 RemoteConnectionId, bool bDeliver);
	void RecvUpdate(uint32 ConnectionId, FNetSerializationContext& Context);
	void RecvUpdate(FNetSerializationContext& Context) { RecvUpdate(1, Context); }

	UReplicatedTestObjectBridge* GetReplicationBridge() { return ReplicationBridge; }
	UReplicationSystem* GetReplicationSystem() { return ReplicationSystem; }

	void SetMaxSendPacketSize(uint32 InMaxSendPacketSize) { MaxSendPacketSize = InMaxSendPacketSize; }

	FConnectionInfo& GetConnectionInfo(uint32 ConnectionId) { return Connections[ConnectionId - 1]; }

public:
	UReplicationSystem* ReplicationSystem;
	UReplicatedTestObjectBridge* ReplicationBridge;
	TArray<TStrongObjectPtr<UObject>> CreatedObjects;

private:
	TArray<FConnectionInfo> Connections;
};

class FReplicationSystemTestClient : public FReplicationSystemTestNode
{
public:
	explicit FReplicationSystemTestClient(uint32 ReplicationSystemId);
	uint32 ConnectionIdOnServer;
	uint32 LocalConnectionId;
};

class FReplicationSystemTestServer : public FReplicationSystemTestNode
{
public:
	explicit FReplicationSystemTestServer();

	// Send data and deliver to the client if bDeliver is true
	bool SendAndDeliverTo(FReplicationSystemTestClient* Client, bool bDeliver);

	// Send data, return true if data was written
	bool SendTo(FReplicationSystemTestClient* Client);

	// Explicitly set delivery status
	void DeliverTo(FReplicationSystemTestClient* Client, bool bDeliver);
};

class FReplicationSystemServerClientTestFixture : public FNetworkAutomationTestSuiteFixture
{
public:
	FReplicationSystemServerClientTestFixture() : FNetworkAutomationTestSuiteFixture() {}

protected:
	enum : bool
	{
		DoNotDeliverPacket = false,
		DeliverPacket = true,
	};

	virtual void SetUp() override;
	FReplicationSystemTestClient* CreateClient();
	virtual void TearDown() override;

	FDataStreamTestUtil DataStreamUtil;
	FReplicationSystemTestServer* Server;
	TArray<FReplicationSystemTestClient*> Clients;
};

}
