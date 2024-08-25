// Copyright Epic Games, Inc. All Rights Reserved.

#include "AllClientsSelectionModel.h"

#include "Replication/Client/ReplicationClientManager.h"

namespace UE::MultiUserClient
{
	FAllClientsSelectionModel::FAllClientsSelectionModel(FReplicationClientManager& InClientManager)
		: ClientManager(InClientManager)
	{
		ClientManager.OnRemoteClientsChanged().AddRaw(this, &FAllClientsSelectionModel::OnRemoteClientsChanged);
	}

	FAllClientsSelectionModel::~FAllClientsSelectionModel()
	{
		ClientManager.OnRemoteClientsChanged().RemoveAll(this);
	}

	void FAllClientsSelectionModel::ForEachSelectedClient(TFunctionRef<EBreakBehavior(FReplicationClient&)> ProcessClient) const
	{
		ClientManager.ForEachClient(ProcessClient);
	}

	void FAllClientsSelectionModel::OnRemoteClientsChanged()
	{
		OnSelectionChangedDelegate.Broadcast();
	}
}
