// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientSelectionModel.h"

namespace UE::MultiUserClient
{
	class FReplicationClientManager;
	
	/** Exposes all clients and detects when clients disconnect. */
	class FAllClientsSelectionModel : public IClientSelectionModel
	{
	public:
		
		FAllClientsSelectionModel(FReplicationClientManager& InClientManager);
		virtual ~FAllClientsSelectionModel() override;

		//~ Begin IClientSelectionModel Interface
		virtual void ForEachSelectedClient(TFunctionRef<EBreakBehavior(FReplicationClient&)> ProcessClient) const override;
		virtual bool ContainsClient(const FGuid& ClientId) const override { return true; }
		virtual FOnSelectionChanged& OnSelectionChanged() override { return OnSelectionChangedDelegate; }
		//~ End IClientSelectionModel Interface

	private:

		/** Informs us when the list of clients changes */
		FReplicationClientManager& ClientManager;
		
		/** Called when the clients ForEachSelectedClient enumerates has changed. */
		FOnSelectionChanged OnSelectionChangedDelegate;
		
		void OnRemoteClientsChanged();
	};
}
