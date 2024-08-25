// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationSystem/ReplicationView.h"
#include "Iris/ReplicationSystem/NetTokenStoreState.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class UDataStreamManager;
namespace UE::Net::Private
{
	class FReplicationWriter;
	class FReplicationReader;
}

namespace UE::Net::Private
{

struct FReplicationConnection
{
	FWeakObjectPtr ReplicationDataStream;
	FReplicationWriter* ReplicationWriter = nullptr;
	FReplicationReader* ReplicationReader = nullptr;
	FObjectPtr UserData = nullptr;
};

class FReplicationConnections
{
public:
	explicit FReplicationConnections(uint32 MaxConnections = 128)
	: ValidConnections(MaxConnections)
	{
		Connections.SetNumZeroed(MaxConnections);
		ReplicationViews.SetNum(MaxConnections);
		RemoteNetTokenStoreStates.SetNum(MaxConnections);
	}

	void Deinit();

	const FReplicationConnection* GetConnection(uint32 ConnectionId) const
	{
		if (ValidConnections.GetBit(ConnectionId))
		{
			return &Connections[ConnectionId];
		}
		
		return nullptr;
	}

	FReplicationConnection* GetConnection(uint32 ConnectionId)
	{
		if (ValidConnections.GetBit(ConnectionId))
		{
			return &Connections[ConnectionId];
		}
		
		return nullptr;
	}

	bool IsValidConnection(uint32 ConnectionId) const
	{
		return ConnectionId < GetMaxConnectionCount() && ValidConnections.GetBit(ConnectionId);
	}

	void AddConnection(uint32 ConnectionId)
	{
		check(ValidConnections.GetBit(ConnectionId) == false);
		ValidConnections.SetBit(ConnectionId);
	}

	IRISCORE_API void RemoveConnection(uint32 ConnectionId);

	uint32 GetMaxConnectionCount() const { return ValidConnections.GetNumBits(); }

	const FNetBitArray& GetValidConnections() const { return ValidConnections; }

	void InitDataStreams(uint32 ReplicationSystemId, uint32 ConnectionId, UDataStreamManager* DataStreamManager);

	void SetReplicationView(uint32 ConnectionId, const FReplicationView& ViewInfo);
	const FReplicationView& GetReplicationView(uint32 ConnectionId) const { return ReplicationViews[ConnectionId]; }

	const FNetTokenStoreState& GetRemoteNetTokenStoreState(uint32 ConnectionId) const { return RemoteNetTokenStoreStates[ConnectionId]; }
	FNetTokenStoreState& GetRemoteNetTokenStoreState(uint32 ConnectionId) { return RemoteNetTokenStoreStates[ConnectionId]; }

private:
	void DestroyReplicationReaderAndWriter(uint32 ConnectionId);

private:
	TArray<FReplicationConnection> Connections;
	TArray<FReplicationView> ReplicationViews;
	TArray<FNetTokenStoreState> RemoteNetTokenStoreStates;
	FNetBitArray ValidConnections;
};

}
