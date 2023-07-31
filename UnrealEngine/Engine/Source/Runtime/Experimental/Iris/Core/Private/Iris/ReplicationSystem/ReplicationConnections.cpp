// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationConnections.h"
#include "Iris/DataStream/DataStreamManager.h"
#include "Iris/ReplicationSystem/NetTokenDataStream.h"
#include "Iris/ReplicationSystem/ReplicationDataStream.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"
#include "Iris/ReplicationSystem/ReplicationReader.h"

namespace UE::Net::Private
{

void FReplicationConnections::Deinit()
{
	ValidConnections.ForAllSetBits([this](uint32 ConnectionId) { RemoveConnection(ConnectionId); });
}

void FReplicationConnections::InitDataStreams(uint32 ReplicationSystemId, uint32 ConnectionId, UDataStreamManager* DataStreamManager)
{
	FReplicationConnection* Connection = GetConnection(ConnectionId);
	if (Connection == nullptr)
	{
		return;
	}

	// Init NetTokenDataStream if we should have one
	UNetTokenDataStream* NetTokenDataStream = StaticCast<UNetTokenDataStream*>(DataStreamManager->GetStream(FName("NetToken")));
	if (NetTokenDataStream != nullptr)
	{
		UNetTokenDataStream::FInitParameters Params;
		Params.ConnectionId = ConnectionId;
		Params.ReplicationSystemId = ReplicationSystemId;
		Params.RemoteTokenStoreState = &RemoteNetTokenStoreStates[ConnectionId];
		Params.NetExports = &DataStreamManager->GetNetExports();

		NetTokenDataStream->Init(Params);
	}

	// Init ReplicationDataStream
	Connection->ReplicationWriter->SetNetExports(DataStreamManager->GetNetExports());
	Connection->ReplicationReader->SetRemoteNetTokenStoreState(&RemoteNetTokenStoreStates[ConnectionId]);

	UReplicationDataStream* ReplicationDataStream = StaticCast<UReplicationDataStream*>(DataStreamManager->GetStream(FName("Replication")));
	if (ReplicationDataStream != nullptr) 
	{
		checkf(NetTokenDataStream != nullptr, TEXT("%s"), TEXT("ReplicationDataStream requires a NetTokenDataStream"));
		ReplicationDataStream->SetReaderAndWriter(Connection->ReplicationReader, Connection->ReplicationWriter);
		Connection->ReplicationDataStream = ReplicationDataStream;
	}
}

void FReplicationConnections::SetReplicationView(uint32 ConnectionId, const FReplicationView& View)
{
	ReplicationViews[ConnectionId] = View;
}

void FReplicationConnections::RemoveConnection(uint32 ConnectionId)
{
	check(ValidConnections.GetBit(ConnectionId));
	SetReplicationView(ConnectionId, FReplicationView());
	RemoteNetTokenStoreStates[ConnectionId] = FNetTokenStoreState();
	DestroyReplicationReaderAndWriter(ConnectionId);

	Connections[ConnectionId] = FReplicationConnection();
	ValidConnections.ClearBit(ConnectionId);
}

void FReplicationConnections::DestroyReplicationReaderAndWriter(uint32 ConnectionId)
{
	FReplicationConnection* Connection = GetConnection(ConnectionId);
	UReplicationDataStream* ReplicationDataStream = CastChecked<UReplicationDataStream>(Connection->ReplicationDataStream.Get(), ECastCheckedType::NullAllowed);
	if (ReplicationDataStream)
	{
		ReplicationDataStream->SetReaderAndWriter(nullptr, nullptr);
	}

	delete Connection->ReplicationReader;
	Connection->ReplicationReader = nullptr;
	delete Connection->ReplicationWriter;
	Connection->ReplicationWriter = nullptr;
}

}
